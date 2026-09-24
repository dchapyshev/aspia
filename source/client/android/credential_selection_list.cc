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

#include "client/android/credential_selection_list.h"

#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>

#include "common/android/label.h"
#include "common/android/scroll_area.h"
#include "common/android/switch.h"

namespace {

constexpr int kHorizontalMargin = 16;
constexpr int kVerticalMargin = 10;
constexpr int kSpacing = 12;

// A tap anywhere on the item toggles its switch. A drag that scrolls the list releases the press
// outside the item, so it toggles nothing.
class Row final : public QWidget
{
public:
    Row(const CredentialSelectionList::Item& item, Switch* control)
        : control_(control)
    {
        QVBoxLayout* text_layout = new QVBoxLayout();
        text_layout->setContentsMargins(0, 0, 0, 0);
        text_layout->setSpacing(0);
        text_layout->addWidget(new Label(item.name, Label::Role::BODY));
        text_layout->addWidget(new Label(item.details, Label::Role::CAPTION));

        QHBoxLayout* layout = new QHBoxLayout(this);
        layout->setContentsMargins(kHorizontalMargin, kVerticalMargin, kHorizontalMargin, kVerticalMargin);
        layout->setSpacing(kSpacing);
        layout->addLayout(text_layout, 1);
        if (!item.status.isEmpty())
            layout->addWidget(new Label(item.status, Label::Role::CAPTION));
        layout->addWidget(control_);
    }

protected:
    void mousePressEvent(QMouseEvent* event) final
    {
        event->accept();
    }

    void mouseReleaseEvent(QMouseEvent* event) final
    {
        if (rect().contains(event->position().toPoint()))
            control_->toggle();
    }

private:
    Switch* control_;
};

} // namespace

//--------------------------------------------------------------------------------------------------
CredentialSelectionList::CredentialSelectionList(QWidget* parent)
    : QWidget(parent),
      list_layout_(new QVBoxLayout())
{
    list_layout_->setContentsMargins(0, 0, 0, 0);
    list_layout_->setSpacing(0);

    QWidget* content = new QWidget();
    QVBoxLayout* content_layout = new QVBoxLayout(content);
    content_layout->setContentsMargins(0, 0, 0, 0);
    content_layout->addLayout(list_layout_);
    content_layout->addStretch();

    ScrollArea* scroll = new ScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setWidget(content);

    // The bottom pixel is left for the separator.
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 1);
    layout->addWidget(scroll);
}

//--------------------------------------------------------------------------------------------------
CredentialSelectionList::~CredentialSelectionList() = default;

//--------------------------------------------------------------------------------------------------
void CredentialSelectionList::setItems(const QList<Item>& items)
{
    qDeleteAll(rows_);
    rows_.clear();
    switches_.clear();

    for (const Item& item : items)
    {
        Switch* control = new Switch();
        control->setChecked(item.checked);
        connect(control, &Switch::toggled, this, &CredentialSelectionList::sig_selectionChanged);

        Row* row = new Row(item, control);
        row->setEnabled(item.enabled);

        list_layout_->addWidget(row);
        rows_.append(row);
        switches_.append(control);
    }

    emit sig_selectionChanged();
}

//--------------------------------------------------------------------------------------------------
void CredentialSelectionList::toggleAll()
{
    bool all_checked = true;
    for (const Switch* control : std::as_const(switches_))
    {
        if (control->isEnabled() && !control->isChecked())
            all_checked = false;
    }

    for (Switch* control : std::as_const(switches_))
    {
        if (control->isEnabled())
            control->setChecked(!all_checked);
    }
}

//--------------------------------------------------------------------------------------------------
QList<int> CredentialSelectionList::checkedItems() const
{
    QList<int> indexes;

    for (int i = 0; i < switches_.size(); ++i)
    {
        if (switches_[i]->isChecked())
            indexes.append(i);
    }

    return indexes;
}

//--------------------------------------------------------------------------------------------------
void CredentialSelectionList::paintEvent(QPaintEvent* /* event */)
{
    QPainter painter(this);
    painter.setPen(palette().color(QPalette::Mid));
    painter.drawLine(0, height() - 1, width(), height() - 1);
}
