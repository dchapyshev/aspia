//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/ui/sys_info/sys_info_widget_event_logs.h"

#include "base/logging.h"
#include "common/system_info_constants.h"

#include <QMenu>

namespace client {

namespace {

class EventItem final : public QTreeWidgetItem
{
public:
    explicit EventItem(qint64 time)
        : time_(time)
    {
        // Nothing
    }

    // QTreeWidgetItem implementation.
    bool operator<(const QTreeWidgetItem &other) const final
    {
        if (treeWidget()->sortColumn() == 0)
        {
            const EventItem* other_item = static_cast<const EventItem*>(&other);
            return time_ < other_item->time_;
        }

        return QTreeWidgetItem::operator<(other);
    }

private:
    qint64 time_;
};

} // namespace

//--------------------------------------------------------------------------------------------------
SysInfoWidgetEventLogs::SysInfoWidgetEventLogs(QWidget* parent)
    : SysInfoWidget(parent)
{
    ui.setupUi(this);
    ui.tree->setMouseTracking(true);

    // Hide description column.
    ui.tree->setColumnHidden(5, true);

    ui.combobox_type->addItem(tr("Application"),
        static_cast<quint32>(proto::system_info::EventLogs::Event::TYPE_APPLICATION));
    ui.combobox_type->addItem(tr("Security"),
        static_cast<quint32>(proto::system_info::EventLogs::Event::TYPE_SECURITY));
    ui.combobox_type->addItem(tr("System"),
        static_cast<quint32>(proto::system_info::EventLogs::Event::TYPE_SYSTEM));

    ui.combobox_type->setCurrentIndex(2);

    connect(ui.action_copy_row, &QAction::triggered, this, [this]()
    {
        copyRow(ui.tree->currentItem());
    });

    connect(ui.action_copy_value, &QAction::triggered, this, [this]()
    {
        copyColumn(ui.tree->currentItem(), current_column_);
    });

    connect(ui.tree, &QTreeWidget::customContextMenuRequested,
            this, &SysInfoWidgetEventLogs::onContextMenu);

    connect(ui.tree, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem* item, int /* column */)
    {
        copyRow(item);
    });

    connect(ui.tree, &QTreeWidget::itemEntered,
            this, [this](QTreeWidgetItem* /* item */, int column)
    {
        current_column_ = column;
    });

    connect(ui.tree, &QTreeWidget::currentItemChanged,
            this, [this](QTreeWidgetItem* current, QTreeWidgetItem* /* previous */)
    {
        QString description;

        if (current)
            description = current->text(5);
        ui.edit_description->setPlainText(description);

        bool enable = !description.isEmpty();

        ui.edit_description->setEnabled(enable);
        ui.label_description->setEnabled(enable);
    });

    connect(ui.button_next, &QPushButton::clicked, this, [this]()
    {
        int index = ui.combobox_page->currentIndex() + 1;
        int count = ui.combobox_page->count();

        if (index >= count)
            return;

        onPageActivated(index);
    });

    connect(ui.button_prev, &QPushButton::clicked, this, [this]()
    {
        int index = ui.combobox_page->currentIndex() - 1;
        if (index < 0)
            return;

        onPageActivated(index);
    });

    connect(ui.combobox_page, QOverload<int>::of(&QComboBox::activated),
            this, &SysInfoWidgetEventLogs::onPageActivated);

    connect(ui.combobox_type, QOverload<int>::of(&QComboBox::activated), this, [this](int index)
    {
        proto::system_info::EventLogs::Event::Type type =
            static_cast<proto::system_info::EventLogs::Event::Type>(
                ui.combobox_type->itemData(index).toInt());
        emit sig_systemInfoRequest(createRequest(type, 0));
    });
}

//--------------------------------------------------------------------------------------------------
SysInfoWidgetEventLogs::~SysInfoWidgetEventLogs() = default;

//--------------------------------------------------------------------------------------------------
std::string SysInfoWidgetEventLogs::category() const
{
    return common::kSystemInfo_EventLogs;
}

//--------------------------------------------------------------------------------------------------
proto::system_info::SystemInfoRequest SysInfoWidgetEventLogs::request() const
{
    proto::system_info::EventLogs::Event::Type type =
        static_cast<proto::system_info::EventLogs::Event::Type>(
            ui.combobox_type->currentData().toInt());
    return createRequest(type, start_record_);
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetEventLogs::setSystemInfo(const proto::system_info::SystemInfo& system_info)
{
    ui.tree->clear();
    ui.combobox_type->setEnabled(true);

    if (!system_info.has_event_logs())
    {
        ui.tree->setEnabled(false);
        ui.button_next->setEnabled(false);
        ui.button_prev->setEnabled(false);
        ui.combobox_page->setEnabled(false);
        return;
    }

    const proto::system_info::EventLogs& event_logs = system_info.event_logs();

    total_records_ = event_logs.total_records();
    if (current_type_ != event_logs.type())
    {
        current_type_ = event_logs.type();
        start_record_ = 0;
    }

    ui.combobox_page->clear();
    ui.combobox_page->setEnabled(true);

    quint32 page_count = std::max(total_records_ / kRecordsPerPage, 1U);
    for (quint32 i = 1; i <= page_count; ++i)
        ui.combobox_page->addItem(tr("Page %1/%2").arg(i).arg(page_count));

    int current_page = std::max(0, static_cast<int>(start_record_ / kRecordsPerPage));

    ui.button_prev->setEnabled(current_page > 0);
    ui.button_next->setEnabled(current_page < ui.combobox_page->count() - 1);

    LOG(LS_INFO) << "Events count: " << event_logs.event_size();
    LOG(LS_INFO) << "Current page: " << current_page << " (total: " << total_records_
                    << " start: " << start_record_ << ")";

    ui.combobox_page->setCurrentIndex(current_page);

    QIcon error_icon(":/img/error.png");
    QIcon warning_icon(":/img/warning.png");
    QIcon info_icon(":/img/information.png");

    for (int i = 0; i < event_logs.event_size(); ++i)
    {
        const proto::system_info::EventLogs::Event& event = event_logs.event(i);

        EventItem* item = new EventItem(event.time());
        item->setText(0, timeToString(event.time()));
        item->setText(1, levelToString(event.level()));
        item->setText(2, QString::fromStdString(event.category()));
        item->setText(3, QString::number(event.event_id()));
        item->setText(4, QString::fromStdString(event.source()));
        item->setText(5, QString::fromStdString(event.description()));

        switch (event.level())
        {
            case proto::system_info::EventLogs::Event::LEVEL_ERROR:
                item->setIcon(0, error_icon);
                break;

            case proto::system_info::EventLogs::Event::LEVEL_WARNING:
            case proto::system_info::EventLogs::Event::LEVEL_AUDIT_FAILURE:
                item->setIcon(0, warning_icon);
                break;

            default:
                item->setIcon(0, info_icon);
                break;
        }

        ui.tree->addTopLevelItem(item);
    }

    ui.tree->setColumnWidth(0, 150);
    ui.tree->setColumnWidth(1, 120);
    ui.tree->setColumnWidth(2, 80);
    ui.tree->setColumnWidth(3, 80);
    ui.tree->setColumnWidth(4, 140);

    if (ui.tree->topLevelItemCount() > 0)
        ui.tree->setCurrentItem(ui.tree->topLevelItem(0));
}

//--------------------------------------------------------------------------------------------------
QTreeWidget* SysInfoWidgetEventLogs::treeWidget()
{
    return ui.tree;
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetEventLogs::onContextMenu(const QPoint& point)
{
    QTreeWidgetItem* current_item = ui.tree->itemAt(point);
    if (!current_item)
        return;

    ui.tree->setCurrentItem(current_item);

    QMenu menu;
    menu.addAction(ui.action_copy_row);
    menu.addAction(ui.action_copy_value);

    menu.exec(ui.tree->viewport()->mapToGlobal(point));
}

//--------------------------------------------------------------------------------------------------
void SysInfoWidgetEventLogs::onPageActivated(int index)
{
    start_record_ = static_cast<quint32>(index) * kRecordsPerPage;
    LOG(LS_INFO) << "Page activated: " << index << " (start: " << start_record_ << ")";
    emit sig_systemInfoRequest(request());
}

//--------------------------------------------------------------------------------------------------
proto::system_info::SystemInfoRequest SysInfoWidgetEventLogs::createRequest(
    proto::system_info::EventLogs::Event::Type type, quint32 start) const
{
    ui.button_next->setEnabled(false);
    ui.button_prev->setEnabled(false);
    ui.combobox_page->setEnabled(false);
    ui.combobox_type->setEnabled(false);

    proto::system_info::SystemInfoRequest system_info_request;
    system_info_request.set_category(category());

    proto::system_info::EventLogsData* data = system_info_request.mutable_event_logs_data();
    data->set_type(type);
    data->set_record_start(start);
    data->set_record_count(kRecordsPerPage);

    return system_info_request;
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfoWidgetEventLogs::levelToString(proto::system_info::EventLogs::Event::Level value)
{
    switch (value)
    {
        case proto::system_info::EventLogs::Event::LEVEL_INFORMATION:
            return tr("Information");

        case proto::system_info::EventLogs::Event::LEVEL_WARNING:
            return tr("Warning");

        case proto::system_info::EventLogs::Event::LEVEL_ERROR:
            return tr("Error");

        case proto::system_info::EventLogs::Event::LEVEL_AUDIT_SUCCESS:
            return tr("Audit Success");

        case proto::system_info::EventLogs::Event::LEVEL_AUDIT_FAILURE:
            return tr("Audit Failure");

        default:
            return tr("Unknown");
    }
}

} // namespace client
