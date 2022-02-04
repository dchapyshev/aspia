//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "build/build_config.h"
#include "qt_base/qt_logging.h"

#include <QHideEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QScreen>
#include <QTimer>
#include <QTranslator>
#include <QToolButton>

namespace host {

namespace {

class SessionTreeItem : public QTreeWidgetItem
{
public:
    SessionTreeItem(const UserSessionAgent::Client& client)
        : id_(client.id)
    {
        switch (client.session_type)
        {
            case proto::SESSION_TYPE_DESKTOP_MANAGE:
                setIcon(0, QIcon(QStringLiteral(":/img/monitor-keyboard.png")));
                break;

            case proto::SESSION_TYPE_DESKTOP_VIEW:
                setIcon(0, QIcon(QStringLiteral(":/img/monitor.png")));
                break;

            case proto::SESSION_TYPE_FILE_TRANSFER:
                setIcon(0, QIcon(QStringLiteral(":/img/folder-stand.png")));
                break;

            case proto::SESSION_TYPE_SYSTEM_INFO:
                setIcon(0, QIcon(QStringLiteral(":/img/computer_info.png")));
                break;

            case proto::SESSION_TYPE_TEXT_CHAT:
                setIcon(0, QIcon(QStringLiteral(":/img/text-chat.png")));
                break;

            default:
                LOG(LS_FATAL) << "Unexpected session type: " << client.session_type;
                return;
        }

        setText(0, QString::fromStdString(client.computer_name));
    }

    uint32_t id() const { return id_; }

private:
    const uint32_t id_;
    DISALLOW_COPY_AND_ASSIGN(SessionTreeItem);
};

QToolButton* createSessionButton(QWidget* parent, const QString& icon, const QString& tooltip)
{
    static const QString kStyle = QStringLiteral("\
        QToolButton {\
            border:1px solid #FAFAFA;\
            border-radius:2px;\
            background:transparent;\
            padding:1px;\
        }\
        QToolButton:hover {\
            border:1px solid #AAAAAA;\
            background:#EBEBEB;\
        }\
        QToolButton:pressed {\
            border:1px solid #AAAAAA;\
            background:#D7D7D7;\
        }");

    QToolButton* button = new QToolButton(parent);

    button->setIcon(QIcon(icon));
    button->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    button->setFixedWidth(20);
    button->setFixedHeight(20);
    button->setStyleSheet(kStyle);
    button->setToolTip(tooltip);

    return button;
}

} // namespace

NotifierWindow::NotifierWindow(QWidget* parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint)
{
    LOG(LS_INFO) << "Ctor";
    ui.setupUi(this);

    ui.label_title->installEventFilter(this);
    ui.label_connections->installEventFilter(this);

    connect(ui.button_show_hide, &QPushButton::clicked, this, &NotifierWindow::onShowHidePressed);
    connect(ui.button_voice_chat, &QToolButton::clicked, this, &NotifierWindow::onVoiceChat);
    connect(ui.button_text_chat, &QToolButton::clicked, this, &NotifierWindow::onTextChat);
    connect(ui.button_lock_mouse, &QToolButton::clicked, this, &NotifierWindow::onLockMouse);
    connect(ui.button_lock_keyboard, &QToolButton::clicked, this, &NotifierWindow::onLockKeyboard);
    connect(ui.button_pause, &QToolButton::clicked, this, &NotifierWindow::onPause);
    connect(ui.button_stop, &QToolButton::clicked, this, &NotifierWindow::onStop);

#if 1
    ui.button_text_chat->setVisible(false);
    ui.button_voice_chat->setVisible(false);
#endif

    setAttribute(Qt::WA_TranslucentBackground);

    connect(QApplication::primaryScreen(), &QScreen::availableGeometryChanged,
            this, &NotifierWindow::updateWindowPosition);

    QTimer* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this]
    {
        setWindowFlag(Qt::WindowStaysOnTopHint);

        if (!isVisible())
        {
            show();
            activateWindow();
        }
    });
    timer->start(std::chrono::seconds(3));

    QTimer::singleShot(std::chrono::milliseconds(15), this, &NotifierWindow::updateWindowPosition);
}

NotifierWindow::~NotifierWindow()
{
    LOG(LS_INFO) << "Dtor";
}

void NotifierWindow::onClientListChanged(const UserSessionAgent::ClientList& clients)
{
    if (!clients.empty())
    {
        ui.tree->clear();

        for (const auto& client : clients)
        {
            SessionTreeItem* tree_item = new SessionTreeItem(client);

            tree_item->setTextAlignment(0, Qt::AlignLeft | Qt::AlignVCenter);
            tree_item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
            tree_item->setFlags(Qt::ItemIsEnabled);

            ui.tree->addTopLevelItem(tree_item);

            QToolButton* stop_button =
                createSessionButton(ui.tree,
                                    QStringLiteral(":/img/control-stop.png"),
                                    tr("Disconnect"));
            connect(stop_button, &QToolButton::clicked, this, [=]()
            {
                emit killSession(tree_item->id());
            });

            ui.tree->setItemWidget(tree_item, 1, stop_button);
        }

        ui.tree->resizeColumnToContents(1);

        int first_column_width = ui.tree->header()->width() - ui.tree->columnWidth(1);
        ui.tree->setColumnWidth(0, first_column_width);
    }
    else
    {
        LOG(LS_INFO) << "Empty session list. Notifier closed";

        emit finished();
        closeNotifier();
    }
}

void NotifierWindow::onVoiceChat()
{
    is_voice_chat_ = !is_voice_chat_;
    emit voiceChat(is_voice_chat_);
}

void NotifierWindow::onTextChat()
{
    emit textChat();
}

void NotifierWindow::onLockMouse()
{
    is_mouse_locked_ = !is_mouse_locked_;

    QString icon;
    QString tooltip;

    if (is_mouse_locked_)
    {
        icon = QStringLiteral(":/img/mouse-lock.png");
        tooltip = tr("Unlock mouse");
    }
    else
    {
        icon = QStringLiteral(":/img/mouse-unlock.png");
        tooltip = tr("Lock mouse");
    }

    ui.button_lock_mouse->setIcon(QIcon(icon));
    ui.button_lock_mouse->setToolTip(tooltip);

    emit lockMouse(is_mouse_locked_);
}

void NotifierWindow::onLockKeyboard()
{
    is_keyboard_locked_ = !is_keyboard_locked_;

    QString icon;
    QString tooltip;

    if (is_keyboard_locked_)
    {
        icon = QStringLiteral(":/img/keyboard-lock.png");
        tooltip = tr("Unlock keyboard");
    }
    else
    {
        icon = QStringLiteral(":/img/keyboard.png");
        tooltip = tr("Lock keyboard");
    }

    ui.button_lock_keyboard->setIcon(QIcon(icon));
    ui.button_lock_keyboard->setToolTip(tooltip);

    emit lockKeyboard(is_keyboard_locked_);
}

void NotifierWindow::onPause()
{
    is_paused_ = !is_paused_;

    QString icon;
    QString tooltip;

    if (is_paused_)
    {
        icon = QStringLiteral(":/img/control-start.png");
        tooltip = tr("Resume");
    }
    else
    {
        icon = QStringLiteral(":/img/control-pause.png");
        tooltip = tr("Pause");
    }

    ui.button_pause->setIcon(QIcon(icon));
    ui.button_pause->setToolTip(tooltip);

    emit pause(is_paused_);
}

void NotifierWindow::onStop()
{
    for (int i = 0; i < ui.tree->topLevelItemCount(); ++i)
    {
        SessionTreeItem* item = static_cast<SessionTreeItem*>(ui.tree->topLevelItem(i));
        if (item)
        {
            LOG(LS_INFO) << "Disconnect session with ID: " << item->id();
            emit killSession(item->id());
        }
    }
}

void NotifierWindow::retranslateUi()
{
    ui.retranslateUi(this);
}

void NotifierWindow::closeNotifier()
{
    should_be_close_ = true;
    close();
}

bool NotifierWindow::eventFilter(QObject* object, QEvent* event)
{
    if (object == ui.label_title || object == ui.label_connections)
    {
        switch (event->type())
        {
            case QEvent::MouseButtonPress:
            {
                QMouseEvent* mouse_event = static_cast<QMouseEvent*>(event);
                if (mouse_event->button() == Qt::LeftButton)
                {
                    start_pos_ = mouse_event->pos();
                    return true;
                }
            }
            break;

            case QEvent::MouseMove:
            {
                QMouseEvent* mouse_event = static_cast<QMouseEvent*>(event);
                if (mouse_event->buttons() & Qt::LeftButton && start_pos_.x() >= 0)
                {
                    QPoint diff = mouse_event->pos() - start_pos_;
                    move(pos() + diff);
                    return true;
                }
            }
            break;

            case QEvent::MouseButtonRelease:
            {
                start_pos_ = QPoint(-1, -1);
            }
            break;

            default:
                break;
        }
    }

    return QWidget::eventFilter(object, event);
}

void NotifierWindow::hideEvent(QHideEvent* event)
{
    event->ignore();
}

void NotifierWindow::closeEvent(QCloseEvent* event)
{
    if (!should_be_close_)
    {
        event->ignore();
        return;
    }

    QWidget::closeEvent(event);
}

void NotifierWindow::onShowHidePressed()
{
    if (ui.content->isVisible())
        hideNotifier();
    else
        showNotifier();
}

void NotifierWindow::updateWindowPosition()
{
    showNotifier();

    QRect available_rect = currentAvailableRect();
    QSize window_size = frameSize();

    int x = available_rect.x() + (available_rect.width() - window_size.width());
    int y = available_rect.y() + (available_rect.height() - window_size.height());

    LOG(LS_INFO) << "Available geometry for primary screen: " << available_rect;
    LOG(LS_INFO) << "Notifier window size: " << window_size;
    LOG(LS_INFO) << "Notifier window moved to: " << x << "x" << y;

    move(x, y);
}

void NotifierWindow::showNotifier()
{
    LOG(LS_INFO) << "showNotifier called";

    if (ui.content->isHidden() && ui.title->isHidden())
    {
        ui.content->show();
        ui.title->show();

        QPoint window_pos = window_rect_.topLeft();
        QSize window_size = window_rect_.size();

        LOG(LS_INFO) << "Notifier window size: " << window_size;
        LOG(LS_INFO) << "Notifier window moved to: " << window_pos;

        move(window_pos);
        setFixedSize(window_size);

        ui.button_show_hide->setIcon(QIcon(QStringLiteral(":/img/arrow-left-gray.png")));
    }
    else
    {
        LOG(LS_INFO) << "Notifier already visibled";
    }
}

void NotifierWindow::hideNotifier()
{
    LOG(LS_INFO) << "hideNotifier called";

    QRect screen_rect = currentAvailableRect();
    QSize content_size = ui.content->frameSize();
    window_rect_ = frameGeometry();

    ui.content->hide();
    ui.title->hide();

    QPoint window_pos(screen_rect.x() + screen_rect.width() - ui.button_show_hide->width(), pos().y());
    QSize window_size(window_rect_.width() - content_size.width(), window_rect_.height());

    LOG(LS_INFO) << "Notifier window size: " << window_size;
    LOG(LS_INFO) << "Notifier window moved to: " << window_pos;

    move(window_pos);
    setFixedSize(window_size);

    ui.button_show_hide->setIcon(QIcon(QStringLiteral(":/img/arrow-right-gray.png")));
}

QRect NotifierWindow::currentAvailableRect()
{
#if defined(OS_WIN)
    RECT work_area;
    if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_area, 0))
    {
        LOG(LS_ERROR) << "SystemParametersInfoW failed";
        return QRect();
    }

    return QRect(QPoint(work_area.left, work_area.top),
                 QSize(work_area.right - work_area.left, work_area.bottom - work_area.top));
#else
    QScreen* primary_screen = QApplication::primaryScreen();
    if (!primary_screen)
    {
        LOG(LS_ERROR) << "Primary screen not available";
        return QRect();
    }

    return primary_screen->availableGeometry();
#endif
}

} // namespace host
