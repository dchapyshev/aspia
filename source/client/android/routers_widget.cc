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

#include "client/android/routers_widget.h"

#include <QDateTime>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPainter>
#include <QSet>
#include <QTreeWidgetItem>

#include "base/crypto/data_cryptor.h"
#include "base/gui_application.h"
#include "base/net/tcp_channel.h"
#include "client/android/router_edit_dialog.h"
#include "client/android/router_status_dialog.h"
#include "client/android/two_factor_dialog.h"
#include "client/config.h"
#include "client/database.h"
#include "client/router.h"
#include "common/android/controls.h"
#include "common/android/icon_button.h"
#include "common/android/tree_widget.h"

namespace {

constexpr int kRowActionsWidth = 96;
constexpr double kPlaceholderTextOpacity = 0.6;

//--------------------------------------------------------------------------------------------------
QString statusIconPath(Router::Status status)
{
    switch (status)
    {
        case Router::Status::CONNECTING:
            return ":/img/router-connecting.svg";

        case Router::Status::ONLINE:
            return ":/img/router-online.svg";

        case Router::Status::OFFLINE:
        default:
            return ":/img/router-offline.svg";
    }
}

} // namespace

// Overlay shown over the empty list: the standard window surface with a centered hint.
class RoutersEmptyView final : public QWidget
{
public:
    explicit RoutersEmptyView(QWidget* parent)
        : QWidget(parent)
    {
        // Nothing.
    }

    void setText(const QString& text)
    {
        text_ = text;
        update();
    }

protected:
    // QWidget implementation.
    void paintEvent(QPaintEvent* /* event */) final
    {
        QPainter painter(this);

        painter.fillRect(rect(), palette().color(QPalette::Window));

        QColor text_color = palette().color(QPalette::WindowText);
        text_color.setAlphaF(kPlaceholderTextOpacity);
        painter.setPen(text_color);
        painter.setFont(Controls::scaledFont(font(), Controls::kFontScale));
        painter.drawText(rect(), Qt::AlignCenter, text_);
    }

private:
    QString text_;
};

//--------------------------------------------------------------------------------------------------
RoutersWidget::RoutersWidget(QWidget* parent)
    : QWidget(parent),
      list_(new TreeWidget(this)),
      placeholder_(new RoutersEmptyView(this)),
      add_button_(new IconButton(":/img/material/add_2.svg", this)),
      edit_button_(new IconButton(":/img/material/edit.svg", this)),
      edit_mode_(false)
{
    // A trailing column hosts the per-row edit and delete buttons, shown only in edit mode.
    list_->setRootIsDecorated(false);
    list_->setColumnCount(2);
    list_->header()->setStretchLastSection(false);
    list_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    list_->header()->setSectionResizeMode(1, QHeaderView::Fixed);
    list_->setColumnWidth(1, kRowActionsWidth);
    list_->setColumnHidden(1, true);

    edit_button_->setCheckable(true);

    // The action buttons live in the app bar; AppBar::setActions() reparents and shows the ones it
    // receives. Hidden by default so an excluded button does not linger in this widget.
    add_button_->hide();
    edit_button_->hide();

    // The placeholder overlaps the list, so it dims the empty surface and shows the hint on top.
    QGridLayout* layout = new QGridLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(list_, 0, 0);
    layout->addWidget(placeholder_, 0, 0);
    placeholder_->raise();
    placeholder_->hide();

    connect(add_button_, &IconButton::clicked, this, &RoutersWidget::onAddRouter);
    connect(edit_button_, &IconButton::clicked, this, &RoutersWidget::onToggleEditMode);
    connect(list_, &QTreeWidget::itemClicked, this, &RoutersWidget::onRouterActivated);

    retranslate();
    reload();
}

//--------------------------------------------------------------------------------------------------
RoutersWidget::~RoutersWidget() = default;

//--------------------------------------------------------------------------------------------------
QList<QWidget*> RoutersWidget::appBarActions() const
{
    // The edit action is meaningful only when there is at least one router to act on.
    if (list_->topLevelItemCount() == 0)
        return { add_button_ };

    return { add_button_, edit_button_ };
}

//--------------------------------------------------------------------------------------------------
void RoutersWidget::reload()
{
    list_->clear();

    // The router fields are decrypted with the master-password-derived key, so there is nothing to
    // show until the cryptor is unlocked.
    if (!DataCryptor::instance().isValid())
        return;

    syncSessions();

    for (const RouterConfig& router : Database::instance().routerList())
    {
        const qint64 router_id = router.routerId();
        const Router* session = sessions_.value(router_id);
        const Router::Status status = session ? session->status() : Router::Status::OFFLINE;

        QTreeWidgetItem* item = new QTreeWidgetItem(list_, {router.displayLabel()});
        item->setIcon(0, GuiApplication::svgIcon(statusIconPath(status)));
        item->setData(0, Qt::UserRole, router_id);
    }

    placeholder_->setVisible(list_->topLevelItemCount() == 0);

    updateRowActions();
    emit appBarActionsChanged();
}

//--------------------------------------------------------------------------------------------------
void RoutersWidget::resetEditMode()
{
    if (!edit_mode_)
        return;

    edit_mode_ = false;
    edit_button_->setChecked(false);
    list_->setColumnHidden(1, true);
    updateRowActions();
}

//--------------------------------------------------------------------------------------------------
void RoutersWidget::retranslate()
{
    placeholder_->setText(tr("No routers added"));
}

//--------------------------------------------------------------------------------------------------
void RoutersWidget::onAddRouter()
{
    RouterEditDialog dialog(-1, this);
    if (dialog.exec() == QDialog::Accepted)
        reload();
}

//--------------------------------------------------------------------------------------------------
void RoutersWidget::onToggleEditMode()
{
    edit_mode_ = edit_button_->isChecked();
    list_->setColumnHidden(1, !edit_mode_);
    updateRowActions();
}

//--------------------------------------------------------------------------------------------------
void RoutersWidget::onRouterActivated(QTreeWidgetItem* item, int /* column */)
{
    // In edit mode the row hosts the edit and delete buttons; tapping the row itself does nothing.
    if (edit_mode_ || !item)
        return;

    const qint64 router_id = item->data(0, Qt::UserRole).toLongLong();

    RouterStatusDialog dialog(item->text(0), router_id, events_.value(router_id), this);
    connect(this, &RoutersWidget::sig_routerEvent, &dialog, &RouterStatusDialog::onRouterEvent);
    connect(&dialog, &RouterStatusDialog::sig_clearEvents, this, [this](qint64 id)
    {
        auto it = events_.find(id);
        if (it != events_.end())
            it->clear();
    });
    dialog.exec();
}

//--------------------------------------------------------------------------------------------------
void RoutersWidget::updateRowActions()
{
    // With no rows there is nothing to edit, so edit mode is left to keep it from silently
    // persisting and decorating the next added router.
    if (list_->topLevelItemCount() == 0 && edit_mode_)
    {
        edit_mode_ = false;
        edit_button_->setChecked(false);
        list_->setColumnHidden(1, true);
    }

    for (int i = 0; i < list_->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* item = list_->topLevelItem(i);
        if (edit_mode_)
            list_->setItemWidget(item, 1, createRowActions(item->data(0, Qt::UserRole).toLongLong()));
        else
            list_->setItemWidget(item, 1, nullptr);
    }
}

//--------------------------------------------------------------------------------------------------
QWidget* RoutersWidget::createRowActions(qint64 router_id)
{
    QWidget* actions = new QWidget();

    IconButton* edit_button = new IconButton(":/img/material/edit.svg", actions);
    IconButton* delete_button = new IconButton(":/img/material/delete.svg", actions);

    QHBoxLayout* layout = new QHBoxLayout(actions);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(edit_button);
    layout->addWidget(delete_button);

    connect(edit_button, &IconButton::clicked, this, [this, router_id]() { editRouter(router_id); });
    connect(delete_button, &IconButton::clicked, this, [this, router_id]() { removeRouter(router_id); });

    return actions;
}

//--------------------------------------------------------------------------------------------------
void RoutersWidget::editRouter(qint64 router_id)
{
    RouterEditDialog dialog(router_id, this);
    if (dialog.exec() == QDialog::Accepted)
        reload();
}

//--------------------------------------------------------------------------------------------------
void RoutersWidget::removeRouter(qint64 router_id)
{
    if (Database::instance().removeRouter(router_id))
        reload();
}

//--------------------------------------------------------------------------------------------------
void RoutersWidget::syncSessions()
{
    const QList<RouterConfig> configs = Database::instance().routerList();

    QSet<qint64> present;
    for (const RouterConfig& config : std::as_const(configs))
        present.insert(config.routerId());

    // Drop sessions whose router was removed.
    const QList<qint64> existing = sessions_.keys();
    for (qint64 router_id : existing)
    {
        if (!present.contains(router_id))
        {
            Router* session = sessions_.take(router_id);
            session->disconnectFromRouter();
            session->deleteLater();
            events_.remove(router_id);
        }
    }

    // Connect new routers, refresh the configuration of the ones already connected.
    for (const RouterConfig& config : std::as_const(configs))
    {
        if (!config.isValid())
            continue;

        if (Router* session = sessions_.value(config.routerId()))
            session->updateConfig(config);
        else
            createRouterSession(config);
    }
}

//--------------------------------------------------------------------------------------------------
void RoutersWidget::createRouterSession(const RouterConfig& config)
{
    Router* router = new Router(config, this);
    sessions_.insert(config.routerId(), router);
    events_.insert(config.routerId(), {});

    connect(router, &Router::sig_statusChanged, this, [this](qint64 id, Router::Status status)
    {
        if (QTreeWidgetItem* item = itemForRouter(id))
            item->setIcon(0, GuiApplication::svgIcon(statusIconPath(status)));

        const Router* session = sessions_.value(id);
        const QString address = session ? session->config().address() : QString();

        switch (status)
        {
            case Router::Status::CONNECTING:
                addRouterEvent(id, RouterEvent::Severity::INFO,
                               tr("Connecting to router %1...").arg(address));
                break;

            case Router::Status::ONLINE:
                addRouterEvent(id, RouterEvent::Severity::INFO,
                               tr("Connection to router %1 established.").arg(address));
                break;

            case Router::Status::OFFLINE:
                addRouterEvent(id, RouterEvent::Severity::WARNING,
                               tr("Disconnected from router %1.").arg(address));
                break;
        }
    });

    connect(router, &Router::sig_errorOccurred, this,
            [this](qint64 id, TcpChannel::ErrorCode error_code)
    {
        RouterEvent::Severity severity = RouterEvent::Severity::WARNING;
        if (error_code == TcpChannel::ErrorCode::CRYPTO_ERROR ||
            error_code == TcpChannel::ErrorCode::ACCESS_DENIED)
        {
            severity = RouterEvent::Severity::CRITICAL;
        }

        addRouterEvent(id, severity,
                       tr("Network error: %1.").arg(TcpChannel::errorToString(error_code)));
    });

    connect(router, &Router::sig_twoFactorCodeRequired, this, [this](qint64 id)
    {
        requestTwoFactorCode(id, QString());
    });

    connect(router, &Router::sig_twoFactorEnrollment, this,
            [this](qint64 id, const QString& otpauth_uri)
    {
        requestTwoFactorCode(id, otpauth_uri);
    });

    connect(router, &Router::sig_passwordChangeRequired, this, [this](qint64 id)
    {
        addRouterEvent(id, RouterEvent::Severity::CRITICAL,
                       tr("The router requires a password change, which is not supported here yet."));
    });

    router->connectToRouter();
}

//--------------------------------------------------------------------------------------------------
void RoutersWidget::requestTwoFactorCode(qint64 router_id, const QString& otpauth_uri)
{
    Router* session = sessions_.value(router_id);
    if (!session)
        return;

    TwoFactorDialog dialog(otpauth_uri, this);
    if (dialog.exec() == QDialog::Accepted)
        session->submitTwoFactorCode(dialog.code());
    else
        session->disconnectFromRouter();
}

//--------------------------------------------------------------------------------------------------
void RoutersWidget::addRouterEvent(qint64 router_id, RouterEvent::Severity severity,
                                   const QString& text)
{
    RouterEvent event;
    event.time = QDateTime::currentDateTime();
    event.severity = severity;
    event.text = text;

    auto it = events_.find(router_id);
    if (it != events_.end())
        it->append(event);

    emit sig_routerEvent(router_id, event);
}

//--------------------------------------------------------------------------------------------------
QTreeWidgetItem* RoutersWidget::itemForRouter(qint64 router_id) const
{
    for (int i = 0; i < list_->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* item = list_->topLevelItem(i);
        if (item->data(0, Qt::UserRole).toLongLong() == router_id)
            return item;
    }

    return nullptr;
}
