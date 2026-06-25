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

#include "host/ui/notifier_window.h"

#include <QGuiApplication>
#include <QHideEvent>
#include <QLayout>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QTimer>
#include <QTranslator>
#include <QToolButton>
#include <QWindow>

#include "base/gui_application.h"
#include "base/logging.h"
#include "common/desktop/session_type.h"
#include "proto/peer.h"
#include "ui_notifier_window.h"

#if defined(Q_OS_WINDOWS)
#include <qt_windows.h>
#endif // defined(Q_OS_WINDOWS)

namespace {

const int kRightPadding = 10;
const int kBottonPadding = 10;
const int kBorderRadius = 4;

const Qt::WindowFlags kWindowFlags = Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint |
    Qt::CustomizeWindowHint | Qt::WindowDoesNotAcceptFocus;

class SessionTreeItem : public QTreeWidgetItem
{
public:
    SessionTreeItem(const UserSessionAgent::Client& client)
        : session_type_(client.session_type),
          client_id_(client.client_id),
          display_name_(client.display_name),
          computer_name_(client.computer_name)
    {
        setIcon(0, sessionIcon(session_type_));

        if (display_name_.isEmpty())
        {
            is_display_name_ = false;
            setText(0, computer_name_);
        }
        else
        {
            is_display_name_ = true;
            setText(0, display_name_);
        }
    }

    proto::peer::SessionType sessionType() const { return session_type_; }
    quint32 clientId() const { return client_id_; }

    void switchName()
    {
        if (display_name_.isEmpty())
            return;

        if (is_display_name_)
            setText(0, computer_name_);
        else
            setText(0, display_name_);

        is_display_name_ = !is_display_name_;
    }

private:
    const proto::peer::SessionType session_type_;
    const quint32 client_id_;
    const QString display_name_;
    const QString computer_name_;
    bool is_display_name_ = true;

    Q_DISABLE_COPY_MOVE(SessionTreeItem)
};

//--------------------------------------------------------------------------------------------------
QToolButton* createSessionButton(QWidget* parent, const QString& icon, const QString& tooltip)
{
    QToolButton* button = new QToolButton(parent);

    button->setIconSize(QSize(22, 22));
    button->setIcon(QIcon(icon));
    button->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    button->setFixedWidth(28);
    button->setFixedHeight(28);
    button->setToolTip(tooltip);

    return button;
}

} // namespace

//--------------------------------------------------------------------------------------------------
NotifierWindow::NotifierWindow(QWidget* parent)
    : QWidget(parent, kWindowFlags),
      ui(std::make_unique<Ui::NotifierWindow>())
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    setAttribute(Qt::WA_TranslucentBackground);

    ui->label_title->installEventFilter(this);

    ui->button_lock_keyboard->setCheckable(true);
    ui->button_lock_mouse->setCheckable(true);
    ui->button_pause->setCheckable(true);

    ui->tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->tree->setTextElideMode(Qt::ElideRight);

    connect(GuiApplication::instance(), &GuiApplication::sig_themeChanged,
            this, &NotifierWindow::onThemeChanged);

    connect(ui->button_hide, &QPushButton::clicked, this, &NotifierWindow::onHideNotifier);
    connect(ui->button_show, &QToolButton::clicked, this, &NotifierWindow::onShowNotifier);
    connect(ui->button_lock_mouse, &QToolButton::clicked, this, &NotifierWindow::onLockMouse);
    connect(ui->button_lock_keyboard, &QToolButton::clicked, this, &NotifierWindow::onLockKeyboard);
    connect(ui->button_pause, &QToolButton::clicked, this, &NotifierWindow::onPause);
    connect(ui->button_stop, &QToolButton::clicked, this, &NotifierWindow::onStop);

    connect(QApplication::primaryScreen(), &QScreen::availableGeometryChanged,
            this, [this]()
    {
        LOG(INFO) << "Screen geometry changed";
        // The taskbar does not move instantly.
        QTimer::singleShot(
            std::chrono::milliseconds(500), this, &NotifierWindow::onUpdateWindowPosition);
    });

    connect(ui->tree, &QTreeWidget::itemDoubleClicked,
            this, [](QTreeWidgetItem* item, int /* column */)
    {
        SessionTreeItem* session_item = dynamic_cast<SessionTreeItem*>(item);
        if (session_item)
            session_item->switchName();
    });

    QTimer* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this]
    {
        // 1. Re-assert window flags (topmost, customize, no-focus etc.).
        if ((windowFlags() & kWindowFlags) != kWindowFlags)
            setWindowFlags(kWindowFlags);

        // 2. Restore visibility / minimized state.
        if (!isVisible())
            show();
        if (windowState() & Qt::WindowMinimized)
            setWindowState(windowState() & ~Qt::WindowMinimized);

        // 3. Revert opacity tampering.
        if (windowOpacity() < 0.95)
            setWindowOpacity(1.0);

        // 4. Revert mouse passthrough / disabled tampering.
        if (testAttribute(Qt::WA_TransparentForMouseEvents))
            setAttribute(Qt::WA_TransparentForMouseEvents, false);
        if (!isEnabled())
            setEnabled(true);

        // 5. Snap back to the anchor if pushed off-screen, then raise above
        // any other top-most window created after us.
        if (!QGuiApplication::screenAt(mapToGlobal(rect().center())))
            onUpdateWindowPosition();
        raise();
    });
    timer->start(std::chrono::milliseconds(500));

    ui->show_panel->setVisible(false);
    onThemeChanged();

    QTimer::singleShot(std::chrono::milliseconds(15), this, &NotifierWindow::onUpdateWindowPosition);
}

//--------------------------------------------------------------------------------------------------
NotifierWindow::~NotifierWindow()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
QList<quint32> NotifierWindow::sessions(proto::peer::SessionType session_type)
{
    QList<quint32> result;

    for (int i = 0; i < ui->tree->topLevelItemCount(); ++i)
    {
        SessionTreeItem* item = static_cast<SessionTreeItem*>(ui->tree->topLevelItem(i));
        if (item->sessionType() == session_type)
            result.emplace_back(item->clientId());
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
void NotifierWindow::onClientListChanged(const UserSessionAgent::ClientList& clients)
{
    LOG(INFO) << "Client list changed:" << clients.size();

    if (!clients.empty())
    {
        ui->tree->clear();

        bool has_desktop = false;

        for (const auto& client : clients)
        {
            if (client.session_type == proto::peer::SESSION_TYPE_DESKTOP)
                has_desktop = true;

            SessionTreeItem* tree_item = new SessionTreeItem(client);

            tree_item->setTextAlignment(0, Qt::AlignLeft | Qt::AlignVCenter);
            tree_item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
            tree_item->setFlags(Qt::ItemIsEnabled);

            ui->tree->addTopLevelItem(tree_item);

            QToolButton* stop_button =
                createSessionButton(ui->tree, ":/img/stop.svg", tr("Disconnect"));
            quint32 tree_item_id = tree_item->clientId();

            connect(stop_button, &QToolButton::clicked, this, [this, tree_item_id]()
            {
                emit sig_killSession(tree_item_id);
            });

            ui->tree->setItemWidget(tree_item, 1, stop_button);
        }

        ui->button_lock_keyboard->setVisible(has_desktop);
        ui->button_lock_mouse->setVisible(has_desktop);
        ui->button_pause->setVisible(has_desktop);

        updateTreeColumns();
    }
    else
    {
        LOG(INFO) << "Empty session list. Notifier closed";

        emit sig_finished();
        closeNotifier();
    }
}

//--------------------------------------------------------------------------------------------------
void NotifierWindow::onLockMouse(bool value)
{
    LOG(INFO) << "[ACTION] Lock mouse:" << value;
    ui->button_lock_mouse->setToolTip(value ? tr("Unlock mouse") : tr("Lock mouse"));
    emit sig_lockMouse(value);
}

//--------------------------------------------------------------------------------------------------
void NotifierWindow::onLockKeyboard(bool value)
{
    LOG(INFO) << "[ACTION] Lock keyboard:" << value;
    ui->button_lock_keyboard->setToolTip(value ? tr("Unlock keyboard") : tr("Lock keyboard"));
    emit sig_lockKeyboard(value);
}

//--------------------------------------------------------------------------------------------------
void NotifierWindow::onPause(bool value)
{
    LOG(INFO) << "[ACTION] Pause:" << value;
    ui->button_pause->setToolTip(value ? tr("Resume") : tr("Pause"));
    emit sig_pause(value);
}

//--------------------------------------------------------------------------------------------------
void NotifierWindow::onStop()
{
    LOG(INFO) << "[ACTION] Stop";
    emit sig_killSession(0);
}

//--------------------------------------------------------------------------------------------------
void NotifierWindow::retranslateUi()
{
    ui->retranslateUi(this);
}

//--------------------------------------------------------------------------------------------------
void NotifierWindow::closeNotifier()
{
    should_be_close_ = true;
    close();
}

//--------------------------------------------------------------------------------------------------
bool NotifierWindow::eventFilter(QObject* object, QEvent* event)
{
    if (object == ui->label_title)
    {
        switch (event->type())
        {
            case QEvent::MouseButtonPress:
            {
                QMouseEvent* mouse_event = static_cast<QMouseEvent*>(event);
                if (mouse_event->button() == Qt::LeftButton)
                {
                    // Ask the compositor to perform an interactive move. On Wayland a client cannot
                    // reposition itself, so dragging the window by hand (move()) does nothing; this is
                    // the portable way and works on X11 as well.
                    if (QWindow* handle = windowHandle())
                        handle->startSystemMove();
                    return true;
                }
            }
            break;

            default:
                break;
        }
    }

    return QWidget::eventFilter(object, event);
}

//--------------------------------------------------------------------------------------------------
void NotifierWindow::hideEvent(QHideEvent* event)
{
    event->ignore();
}

//--------------------------------------------------------------------------------------------------
void NotifierWindow::closeEvent(QCloseEvent* event)
{
    if (!should_be_close_)
    {
        event->ignore();
        return;
    }

    QWidget::closeEvent(event);
}

//--------------------------------------------------------------------------------------------------
void NotifierWindow::moveEvent(QMoveEvent* event)
{
    LOG(INFO) << "Notifier moved to" << event->pos() << "from" << event->oldPos();
    QWidget::moveEvent(event);
}

//--------------------------------------------------------------------------------------------------
void NotifierWindow::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);

    QPalette palette = GuiApplication::palette();

    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(palette.brush(QPalette::Window));
    painter.setPen(QPen(palette.color(QPalette::Dark), 1));

    painter.drawRoundedRect(rect(), kBorderRadius + 2, kBorderRadius + 2);

    QPainterPath path;
    path.addRoundedRect(rect(), kBorderRadius, kBorderRadius);
    QRegion mask = QRegion(path.toFillPolygon().toPolygon());
    setMask(mask);
}

//--------------------------------------------------------------------------------------------------
void NotifierWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::WindowStateChange)
    {
        if (windowState() & Qt::WindowMinimized)
        {
            setWindowState(windowState() & ~Qt::WindowMinimized);
            event->accept();
            return;
        }
    }
    else if (event->type() == QEvent::LanguageChange)
    {
        retranslateUi();
    }

    QWidget::changeEvent(event);
}

//--------------------------------------------------------------------------------------------------
void NotifierWindow::onUpdateWindowPosition()
{
    onShowNotifier();

    // When the GUI starts at login the window can be laid out before the session's screen geometry and
    // DPI are final (the first run here even reports an oversized screen and off-screen position). Re-
    // activate the layout and refit the tree columns so the content is corrected once this runs again
    // after the screen settles (it is re-invoked on availableGeometryChanged).
    if (QLayout* window_layout = layout())
        window_layout->activate();
    updateTreeColumns();

    QRect available_rect = currentAvailableRect();
    QSize window_size = frameSize();

    int x = available_rect.x() + (available_rect.width() - window_size.width()) - kRightPadding;
    int y = available_rect.y() + (available_rect.height() - window_size.height()) - kBottonPadding;

    LOG(INFO) << "Notifier window size:" << window_size;
    LOG(INFO) << "Notifier window moved to:" << x << "x" << y;

    move(x, y);
}

//--------------------------------------------------------------------------------------------------
void NotifierWindow::onShowNotifier()
{
    LOG(INFO) << "[ACTION] showNotifier called";

    if (ui->content->isHidden() && ui->title->isHidden())
    {
        ui->content->show();
        ui->title->show();
        ui->show_panel->hide();

        QPoint window_pos = window_rect_.topLeft();
        QSize window_size = window_rect_.size();

        LOG(INFO) << "Notifier window size:" << window_size;
        LOG(INFO) << "Notifier window moved to:" << window_pos;

        move(window_pos);
        setFixedSize(window_size);
    }
    else
    {
        LOG(INFO) << "Notifier already visibled";
    }
}

//--------------------------------------------------------------------------------------------------
void NotifierWindow::onHideNotifier()
{
    LOG(INFO) << "[ACTION] hideNotifier called";

    window_rect_ = frameGeometry();

    ui->content->hide();
    ui->title->hide();
    ui->show_panel->show();

    setFixedSize(minimumSizeHint());

    QTimer::singleShot(0, this, [this]()
    {
        QRect screen_rect = currentAvailableRect();
        QPoint window_pos(screen_rect.x() + screen_rect.width() - width() - kRightPadding, pos().y());

        LOG(INFO) << "Notifier window moved to:" << window_pos;
        move(window_pos);
    });
}

//--------------------------------------------------------------------------------------------------
void NotifierWindow::onThemeChanged()
{
    QString window_color = GuiApplication::palette().color(QPalette::Window).name(QColor::HexRgb);

    ui->tree->setStyleSheet(QString("QTreeWidget {"
                                       "background-color: %1;"
                                   "}"
                                   "QTreeWidget::item {"
                                       "border: none;"
                                   "}").arg(window_color));

    ui->content->setStyleSheet(QString("#content {"
                                          "background-color: %1;"
                                          "padding: 30px;"
                                      "}").arg(window_color));

    ui->toolbar->setStyleSheet(QString("#toolbar {"
                                          "background-color: %1;"
                                      "}"
                                      "QToolButton {"
                                          "padding: 3px;"
                                      "}").arg(window_color));

    ui->title->setStyleSheet(QString("#title { background-color: %1; }").arg(window_color));

    ui->label_title->setText(
        QString("<html><head/><body><p><span style=\"font-weight:700;\">%1</span></p></body></html>")
            .arg(tr("Aspia Host")));
    ui->label_title->setStyleSheet("padding: 3px;");
}

//--------------------------------------------------------------------------------------------------
void NotifierWindow::updateTreeColumns()
{
    if (ui->content->isHidden() || ui->tree->topLevelItemCount() == 0)
        return;

    // The disconnect button lives in column 1; size it to its contents and give the remaining width to
    // the session name in column 0.
    ui->tree->resizeColumnToContents(1);
    ui->tree->setColumnWidth(0, ui->tree->header()->width() - ui->tree->columnWidth(1));
}

//--------------------------------------------------------------------------------------------------
QRect NotifierWindow::currentAvailableRect()
{
#if defined(Q_OS_WINDOWS)
    RECT work_area;
    memset(&work_area, 0, sizeof(work_area));

    if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0))
    {
        LOG(ERROR) << "SystemParametersInfoW failed";
        return QRect();
    }

    QScreen* primary_screen = QApplication::primaryScreen();
    if (!primary_screen)
    {
        LOG(ERROR) << "Primary screen not available";
        return QRect();
    }

    qreal device_pixel_ratio = primary_screen->devicePixelRatio();
    QRect rect(QPoint(work_area.left, work_area.top) / device_pixel_ratio,
               QSize(work_area.right - work_area.left, work_area.bottom - work_area.top) /
                   device_pixel_ratio);

    LOG(INFO) << "Available rect:" << rect << "pixel ratio:" << device_pixel_ratio;
    return rect;
#else
    QScreen* primary_screen = QApplication::primaryScreen();
    if (!primary_screen)
    {
        LOG(ERROR) << "Primary screen not available";
        return QRect();
    }

    return primary_screen->availableGeometry();
#endif
}
