//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "host/ui/security_log_dialog.h"

#include <QApplication>
#include <QDataStream>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHeaderView>
#include <QPainter>
#include <QRegularExpression>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTextStream>
#include <QUrl>

#include "base/logging.h"
#include "base/security_log.h"
#include "host/ui/user_settings.h"
#include "ui_security_log_dialog.h"

namespace {

enum Column
{
    COLUMN_TIME    = 0,
    COLUMN_EVENT   = 1,
    COLUMN_DETAILS = 2,
    COLUMN_COUNT   = 3,
};

//--------------------------------------------------------------------------------------------------
SecurityEvent parseEventName(const QString& name)
{
    if (name == "CONNECT")    return SEC_CONNECT;
    if (name == "DISCONNECT") return SEC_DISCONNECT;
    if (name == "ERROR")      return SEC_ERROR;
    if (name == "WARNING")    return SEC_WARNING;
    if (name == "INFO")       return SEC_INFO;
    return -1;
}

//--------------------------------------------------------------------------------------------------
QBrush eventColor(SecurityEvent event)
{
    switch (event)
    {
        case SEC_ERROR:   return QBrush(Qt::red);
        case SEC_WARNING: return QBrush(Qt::darkYellow);
        default:          return QBrush();
    }
}

//--------------------------------------------------------------------------------------------------
class FilterProxyModel final : public QSortFilterProxyModel
{
public:
    explicit FilterProxyModel(QObject* parent = nullptr)
        : QSortFilterProxyModel(parent)
    {
        // Nothing.
    }

    void setEventMask(quint32 mask)
    {
        if (event_mask_ == mask)
            return;
        beginFilterChange();
        event_mask_ = mask;
        endFilterChange();
    }

    void setSearchText(const QString& text)
    {
        QString trimmed = text.trimmed();
        if (search_text_ == trimmed)
            return;
        beginFilterChange();
        search_text_ = trimmed;
        endFilterChange();
    }

protected:
    // QSortFilterProxyModel implementation.
    bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const final
    {
        QAbstractItemModel* m = sourceModel();

        QModelIndex event_index = m->index(source_row, COLUMN_EVENT, source_parent);
        SecurityEvent event = static_cast<SecurityEvent>(event_index.data(Qt::UserRole).toInt());

        if (event >= 0 && !(event_mask_ & (1u << event)))
            return false;

        if (!search_text_.isEmpty())
        {
            for (int column = 0; column < COLUMN_COUNT; ++column)
            {
                QString text = m->index(source_row, column, source_parent).data().toString();
                if (text.contains(search_text_, Qt::CaseInsensitive))
                    return true;
            }
            return false;
        }

        return true;
    }

private:
    quint32 event_mask_ = 0xffffffffu;
    QString search_text_;

    Q_DISABLE_COPY_MOVE(FilterProxyModel)
};

//--------------------------------------------------------------------------------------------------
// Paints item selection as a flat rectangle and skips State_HasFocus, so the platform style does
// not draw its own selection accent (e.g. the thin vertical bar Windows 11 puts at the left edge
// of each selected cell). Item foreground color from the model (red for ERROR, etc.) is preserved.
class FlatSelectionDelegate final : public QStyledItemDelegate
{
public:
    explicit FlatSelectionDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent)
    {
        // Nothing.
    }

    // QStyledItemDelegate implementation.
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const final
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        const bool selected = opt.state & QStyle::State_Selected;

        opt.state &= ~QStyle::State_Selected;
        opt.state &= ~QStyle::State_HasFocus;

        if (selected)
        {
            painter->fillRect(option.rect, option.palette.highlight());

            // The platform style now sees this item as not-selected and would use the regular Text
            // color on top of our highlight fill. Force HighlightedText so the row stays readable
            // even when the model assigned a custom foreground (red/yellow) that does not contrast
            // well with the highlight background.
            opt.palette.setBrush(QPalette::Text, option.palette.highlightedText());
        }

        // Calling QStyledItemDelegate::paint here would internally re-run initStyleOption(), which
        // would re-pull the model's ForegroundRole and overwrite our Text override. Drive the style
        // directly instead.
        const QWidget* widget = option.widget;
        QStyle* style = widget ? widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);
    }

private:
    Q_DISABLE_COPY_MOVE(FlatSelectionDelegate)
};

//--------------------------------------------------------------------------------------------------
// Returns ISO-8601 week tag (e.g. "2026-W19") if file_path looks like security log file, otherwise
// the bare base name. Used as combobox label.
QString fileLabel(const QString& file_path)
{
    return QFileInfo(file_path).completeBaseName();
}

} // namespace

//--------------------------------------------------------------------------------------------------
SecurityLogDialog::SecurityLogDialog(QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::SecurityLogDialog>()),
      log_directory_(securityLogDirectory())
{
    LOG(INFO) << "Ctor";

    ui->setupUi(this);

    model_ = new QStandardItemModel(0, COLUMN_COUNT, this);
    model_->setHeaderData(COLUMN_TIME,    Qt::Horizontal, tr("Time"));
    model_->setHeaderData(COLUMN_EVENT,   Qt::Horizontal, tr("Event"));
    model_->setHeaderData(COLUMN_DETAILS, Qt::Horizontal, tr("Details"));

    proxy_ = new FilterProxyModel(this);
    proxy_->setSourceModel(model_);

    ui->table->setModel(proxy_);
    ui->table->setItemDelegate(new FlatSelectionDelegate(ui->table));
    ui->table->sortByColumn(COLUMN_TIME, Qt::AscendingOrder);
    ui->table->horizontalHeader()->resizeSection(COLUMN_TIME, 180);
    ui->table->horizontalHeader()->resizeSection(COLUMN_EVENT, 100);

    ui->label_directory->setText(log_directory_);

    connect(ui->combo_file, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &SecurityLogDialog::onFileSelected);
    connect(ui->button_refresh, &QToolButton::clicked, this, &SecurityLogDialog::onRefresh);
    connect(ui->button_open_directory, &QToolButton::clicked,
            this, &SecurityLogDialog::onOpenDirectory);

    connect(ui->edit_search, &QLineEdit::textChanged, this, &SecurityLogDialog::onFilterChanged);
    connect(ui->check_connect,    &QCheckBox::toggled, this, &SecurityLogDialog::onFilterChanged);
    connect(ui->check_disconnect, &QCheckBox::toggled, this, &SecurityLogDialog::onFilterChanged);
    connect(ui->check_error,      &QCheckBox::toggled, this, &SecurityLogDialog::onFilterChanged);
    connect(ui->check_warning,    &QCheckBox::toggled, this, &SecurityLogDialog::onFilterChanged);
    connect(ui->check_info,       &QCheckBox::toggled, this, &SecurityLogDialog::onFilterChanged);

    connect(model_, &QAbstractItemModel::rowsInserted, this, &SecurityLogDialog::updateStatusBar);
    connect(model_, &QAbstractItemModel::modelReset,   this, &SecurityLogDialog::updateStatusBar);
    connect(proxy_, &QAbstractItemModel::rowsInserted, this, &SecurityLogDialog::updateStatusBar);
    connect(proxy_, &QAbstractItemModel::rowsRemoved,  this, &SecurityLogDialog::updateStatusBar);
    connect(proxy_, &QAbstractItemModel::modelReset,   this, &SecurityLogDialog::updateStatusBar);

    onFilterChanged();
    reloadFileList();
    updateStatusBar();

    QByteArray state = UserSettings().securityLogDialogState();
    if (!state.isEmpty())
    {
        QByteArray geometry;
        QByteArray header_state;

        QDataStream stream(state);
        stream >> geometry >> header_state;

        if (stream.status() == QDataStream::Ok)
        {
            restoreGeometry(geometry);
            ui->table->horizontalHeader()->restoreState(header_state);
        }
    }
}

//--------------------------------------------------------------------------------------------------
SecurityLogDialog::~SecurityLogDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void SecurityLogDialog::closeEvent(QCloseEvent* event)
{
    QByteArray state;
    QDataStream stream(&state, QIODevice::WriteOnly);
    stream << saveGeometry() << ui->table->horizontalHeader()->saveState();

    UserSettings().setSecurityLogDialogState(state);

    QDialog::closeEvent(event);
}

//--------------------------------------------------------------------------------------------------
void SecurityLogDialog::onFileSelected(int index)
{
    if (index < 0)
        return;

    QString file_path = ui->combo_file->itemData(index).toString();
    if (file_path.isEmpty() || file_path == current_file_path_)
        return;

    switchToFile(file_path);
}

//--------------------------------------------------------------------------------------------------
void SecurityLogDialog::onRefresh()
{
    LOG(INFO) << "[ACTION] Refresh";
    reloadFileList();
    if (!current_file_path_.isEmpty())
        switchToFile(current_file_path_);
}

//--------------------------------------------------------------------------------------------------
void SecurityLogDialog::onOpenDirectory()
{
    LOG(INFO) << "[ACTION] Open directory:" << log_directory_;
    QDesktopServices::openUrl(QUrl::fromLocalFile(log_directory_));
}

//--------------------------------------------------------------------------------------------------
void SecurityLogDialog::onFilterChanged()
{
    quint32 mask = 0;
    if (ui->check_connect->isChecked())    mask |= (1u << SEC_CONNECT);
    if (ui->check_disconnect->isChecked()) mask |= (1u << SEC_DISCONNECT);
    if (ui->check_error->isChecked())      mask |= (1u << SEC_ERROR);
    if (ui->check_warning->isChecked())    mask |= (1u << SEC_WARNING);
    if (ui->check_info->isChecked())       mask |= (1u << SEC_INFO);

    static_cast<FilterProxyModel*>(proxy_)->setEventMask(mask);
    static_cast<FilterProxyModel*>(proxy_)->setSearchText(ui->edit_search->text());

    updateStatusBar();
}

//--------------------------------------------------------------------------------------------------
void SecurityLogDialog::reloadFileList()
{
    QDir dir(log_directory_);
    QFileInfoList files = dir.entryInfoList(
        QStringList() << "*.log", QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot,
        QDir::Name | QDir::Reversed);

    QString previous_selection = current_file_path_;

    QSignalBlocker blocker(ui->combo_file);
    ui->combo_file->clear();

    for (const QFileInfo& info : std::as_const(files))
        ui->combo_file->addItem(fileLabel(info.filePath()), info.filePath());

    if (ui->combo_file->count() == 0)
    {
        switchToFile(QString());
        return;
    }

    int index = ui->combo_file->findData(previous_selection);
    if (index < 0)
        index = 0;

    ui->combo_file->setCurrentIndex(index);

    QString file_path = ui->combo_file->itemData(index).toString();
    if (file_path != current_file_path_)
        switchToFile(file_path);
}

//--------------------------------------------------------------------------------------------------
void SecurityLogDialog::switchToFile(const QString& file_path)
{
    model_->setRowCount(0);
    current_file_path_ = file_path;
    last_pos_ = 0;

    if (current_file_path_.isEmpty())
    {
        updateStatusBar();
        return;
    }

    tailCurrentFile();
}

//--------------------------------------------------------------------------------------------------
void SecurityLogDialog::tailCurrentFile()
{
    if (current_file_path_.isEmpty())
        return;

    QFile file(current_file_path_);
    if (!file.open(QFile::ReadOnly | QFile::Text))
    {
        LOG(WARNING) << "Failed to open security log file:" << current_file_path_
                     << "error:" << file.errorString();
        return;
    }

    qint64 size = file.size();
    if (size < last_pos_)
    {
        // The file shrank (manual edit/replace). Reload from scratch.
        model_->setRowCount(0);
        last_pos_ = 0;
    }

    if (!file.seek(last_pos_))
    {
        LOG(WARNING) << "Failed to seek security log file:" << current_file_path_;
        return;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);

    while (!stream.atEnd())
    {
        QString line = stream.readLine();
        if (line.isEmpty())
            continue;
        appendLine(line);
    }

    last_pos_ = file.pos();
}

//--------------------------------------------------------------------------------------------------
void SecurityLogDialog::appendLine(const QString& line)
{
    static const QRegularExpression re(R"RE(^"([^"]+)"\s+([A-Z]+)\s+(.*)$)RE");

    QRegularExpressionMatch match = re.match(line);
    if (!match.hasMatch())
    {
        LOG(WARNING) << "Unparseable security log line:" << line;
        return;
    }

    QDateTime timestamp = QDateTime::fromString(match.captured(1), Qt::ISODateWithMs);
    if (!timestamp.isValid())
    {
        LOG(WARNING) << "Invalid timestamp in security log line:" << line;
        return;
    }
    timestamp.setTimeZone(QTimeZone::UTC);

    SecurityEvent event = parseEventName(match.captured(2));
    QString details = match.captured(3);

    QStandardItem* time_item = new QStandardItem(timestamp.toLocalTime().toString("yyyy-MM-dd HH:mm:ss.zzz"));
    time_item->setData(timestamp, Qt::UserRole);
    time_item->setToolTip(timestamp.toString(Qt::ISODateWithMs));

    QStandardItem* event_item = new QStandardItem(match.captured(2));
    event_item->setData(event, Qt::UserRole);

    QStandardItem* details_item = new QStandardItem(details);

    QBrush brush = eventColor(event);
    if (brush.style() != Qt::NoBrush)
    {
        time_item->setForeground(brush);
        event_item->setForeground(brush);
        details_item->setForeground(brush);
    }

    model_->appendRow({ time_item, event_item, details_item });
}

//--------------------------------------------------------------------------------------------------
void SecurityLogDialog::updateStatusBar()
{
    int total = model_->rowCount();
    int shown = proxy_->rowCount();
    ui->label_count->setText(tr("%1 / %2").arg(shown).arg(total));
}
