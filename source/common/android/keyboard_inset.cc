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

#include "common/android/keyboard_inset.h"

#include <QApplication>
#include <QDialog>
#include <QEvent>
#include <QInputMethod>
#include <QScrollArea>
#include <QScrollBar>

namespace {

constexpr int kScrollMargin = 16;

} // namespace

//--------------------------------------------------------------------------------------------------
KeyboardInset::KeyboardInset(QWidget* window, QWidget* scope)
    : QObject(window),
      window_(window),
      scope_(scope)
{
    QInputMethod* input_method = QGuiApplication::inputMethod();
    connect(input_method, &QInputMethod::keyboardRectangleChanged, this, &KeyboardInset::onUpdate);
    connect(input_method, &QInputMethod::visibleChanged, this, &KeyboardInset::onUpdate);
    connect(qApp, &QApplication::focusChanged, this, &KeyboardInset::onUpdate);

    // A change of the safe area margins arrives as a resize of the window.
    window_->installEventFilter(this);
}

//--------------------------------------------------------------------------------------------------
KeyboardInset::~KeyboardInset() = default;

//--------------------------------------------------------------------------------------------------
bool KeyboardInset::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == window_ && event->type() == QEvent::Resize)
        onUpdate();

    return QObject::eventFilter(watched, event);
}

//--------------------------------------------------------------------------------------------------
void KeyboardInset::onUpdate()
{
    const QInputMethod* input_method = QGuiApplication::inputMethod();
    const QWidget* focus = QApplication::focusWidget();

    bool active = input_method->isVisible() && focus && scope_->isAncestorOf(focus);
    for (const QWidget* widget = focus; active && widget != scope_; widget = widget->parentWidget())
    {
        if (qobject_cast<const QDialog*>(widget))
            active = false;
    }

    int inset = 0;
    if (active)
    {
        // The keyboard rectangle is in physical pixels.
        const int keyboard_top =
            qRound(input_method->keyboardRectangle().top() / window_->devicePixelRatioF());
        const int window_bottom = window_->mapToGlobal(QPoint(0, window_->height())).y();
        inset = qMax(0, window_bottom - keyboard_top);
    }

    if (active != active_)
    {
        active_ = active;
        emit sig_activeChanged(active);
    }

    if (inset != inset_)
    {
        inset_ = inset;
        window_->setContentsMargins(0, 0, 0, inset);
    }

    // Deferred so it runs after the layout has settled around the new inset.
    if (active)
        QMetaObject::invokeMethod(this, &KeyboardInset::onScrollFocusIntoView, Qt::QueuedConnection);
}

//--------------------------------------------------------------------------------------------------
void KeyboardInset::onScrollFocusIntoView()
{
    QScrollArea* area = focusedScrollArea();
    if (!active_ || !area)
        return;

    // Scrolling by the exact overlap also pulls a field left too high back down to the keyboard.
    const QWidget* focus = QApplication::focusWidget();
    const QWidget* viewport = area->viewport();
    const int viewport_bottom = viewport->mapToGlobal(QPoint(0, viewport->height())).y();
    const int field_bottom = focus->mapToGlobal(QPoint(0, focus->height())).y();
    const int delta = field_bottom + kScrollMargin - viewport_bottom;
    if (delta == 0)
        return;

    QScrollBar* bar = area->verticalScrollBar();
    bar->setValue(bar->value() + delta);
}

//--------------------------------------------------------------------------------------------------
QScrollArea* KeyboardInset::focusedScrollArea() const
{
    QWidget* focus = QApplication::focusWidget();
    if (!focus)
        return nullptr;

    for (QWidget* widget = focus->parentWidget(); widget; widget = widget->parentWidget())
    {
        if (QScrollArea* area = qobject_cast<QScrollArea*>(widget))
            return scope_->isAncestorOf(area) ? area : nullptr;
    }

    return nullptr;
}
