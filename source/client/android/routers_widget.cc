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

#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPainter>
#include <QTreeWidgetItem>

#include "base/crypto/data_cryptor.h"
#include "base/gui_application.h"
#include "client/android/router_edit_dialog.h"
#include "client/config.h"
#include "client/database.h"
#include "common/android/controls.h"
#include "common/android/icon_button.h"
#include "common/android/tree_widget.h"

namespace {

constexpr int kRowActionsWidth = 96;
constexpr double kPlaceholderTextOpacity = 0.6;

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
      add_button_(new IconButton(":/img/add.svg", this)),
      edit_button_(new IconButton(":/img/pencil-drawing.svg", this)),
      edit_mode_(false)
{
    // A trailing column hosts the per-row edit and delete buttons, shown only in edit mode.
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

    const QIcon icon = GuiApplication::svgIcon(":/img/stack.svg");

    for (const RouterConfig& router : Database::instance().routerList())
    {
        QTreeWidgetItem* item = new QTreeWidgetItem(list_, {router.displayLabel()});
        item->setIcon(0, icon);
        item->setData(0, Qt::UserRole, router.routerId());
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

    IconButton* edit_button = new IconButton(":/img/pencil-drawing.svg", actions);
    IconButton* delete_button = new IconButton(":/img/cancel.svg", actions);

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
