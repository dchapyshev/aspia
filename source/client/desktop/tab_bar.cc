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

#include "client/desktop/tab_bar.h"

#include <QApplication>
#include <QIcon>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QProxyStyle>
#include <QRegion>
#include <QStyle>
#include <QStyleHints>
#include <QStyleOption>
#include <QTimer>
#include <QToolButton>

namespace {

// Vertical distance the cursor must travel beyond the tab bar bounds while dragging a tab to
// trigger detachment. Horizontal movement keeps the tab in the bar (reorder).
constexpr int kDetachThresholdPx = 30;

// Drop-target pulsing animation parameters.
constexpr MilliSeconds kPulseTick{ 33 };
constexpr MilliSeconds kPulsePeriod{ 800 };
constexpr int kPulseMaxAlpha = 220;
constexpr int kPulseFrameWidthPx = 2;

// The Windows 11 style of Qt paints the current tab like the rest of them, so everything around it
// is shaded instead. Every other style tells the tabs apart by itself.
constexpr int kTabBarShadeLight = 20;
constexpr int kTabBarShadeDark = 40;

// The style keeps the tab it draws inside the rect it is given, so the cutout follows the drawing
// and not the rect.
constexpr int kTabMarginPx = 2;

#if defined(Q_OS_MACOS)
// Air between the icon, the text and the buttons of a tab.
constexpr int kTabElementSpacingPx = 4;
#endif // defined(Q_OS_MACOS)

//--------------------------------------------------------------------------------------------------
bool isWindows11Style(const QStyle* style)
{
    const QProxyStyle* proxy = qobject_cast<const QProxyStyle*>(style);
    const QStyle* base_style = proxy ? proxy->baseStyle() : style;

    return base_style && base_style->objectName().compare("windows11", Qt::CaseInsensitive) == 0;
}

#if defined(Q_OS_MACOS)
//--------------------------------------------------------------------------------------------------
bool isHorizontalTab(QTabBar::Shape shape)
{
    return shape == QTabBar::RoundedNorth || shape == QTabBar::RoundedSouth ||
           shape == QTabBar::TriangularNorth || shape == QTabBar::TriangularSouth;
}

// Lays a tab out the way the styles of the other platforms do. The macOS style keeps the text of a
// tab in the middle of it and puts the icon aside, so a gap opens between them, and it asks for a
// tab wider than the label it draws.
class TabStyle final : public QProxyStyle
{
public:
    explicit TabStyle(QObject* parent)
    {
        setParent(parent);
    }

    int pixelMetric(
        PixelMetric metric, const QStyleOption* option, const QWidget* widget) const final
    {
        // The style below is the system one, and the icons of the window are of the size the
        // application style gives.
        if (metric == QStyle::PM_TabBarIconSize || metric == QStyle::PM_SmallIconSize)
            return QApplication::style()->pixelMetric(metric, option, widget);

        return QProxyStyle::pixelMetric(metric, option, widget);
    }

    QSize sizeFromContents(ContentsType type, const QStyleOption* option, const QSize& size,
                           const QWidget* widget) const final
    {
        QSize result = QProxyStyle::sizeFromContents(type, option, size, widget);
        const QStyleOptionTab* tab = qstyleoption_cast<const QStyleOptionTab*>(option);

        // The tab bar asks for a size that already holds the horizontal padding of a tab and the
        // style adds it a second time.
        if (type == QStyle::CT_TabBarTab && tab && isHorizontalTab(tab->shape))
            result.rwidth() -= pixelMetric(QStyle::PM_TabBarTabHSpace, option, widget);

        return result;
    }

    QRect subElementRect(SubElement element, const QStyleOption* option,
                         const QWidget* widget) const final
    {
        const QStyleOptionTab* tab = qstyleoption_cast<const QStyleOptionTab*>(option);

        if (element == QStyle::SE_TabBarTabText && tab && isHorizontalTab(tab->shape))
        {
            QRect icon_rect;
            QRect text_rect;

            labelRects(tab, widget, &icon_rect, &text_rect);
            return text_rect;
        }

        return QProxyStyle::subElementRect(element, option, widget);
    }

    void drawControl(ControlElement element, const QStyleOption* option, QPainter* painter,
                     const QWidget* widget) const final
    {
        const QStyleOptionTab* tab = qstyleoption_cast<const QStyleOptionTab*>(option);

        if (element != QStyle::CE_TabBarTabLabel || !tab || !isHorizontalTab(tab->shape))
        {
            QProxyStyle::drawControl(element, option, painter, widget);
            return;
        }

        QRect icon_rect;
        QRect text_rect;

        labelRects(tab, widget, &icon_rect, &text_rect);

        if (!tab->icon.isNull())
        {
            QIcon::Mode mode = (tab->state & QStyle::State_Enabled) ? QIcon::Normal : QIcon::Disabled;
            QIcon::State state = (tab->state & QStyle::State_Selected) ? QIcon::On : QIcon::Off;

            painter->drawPixmap(icon_rect.topLeft(), tab->icon.pixmap(
                icon_rect.size(), painter->device()->devicePixelRatio(), mode, state));
        }

        int alignment = Qt::AlignCenter | Qt::TextShowMnemonic;
        if (!styleHint(QStyle::SH_UnderlineShortcut, tab, widget))
            alignment |= Qt::TextHideMnemonic;

        QPalette::ColorRole role = widget ? widget->foregroundRole() : QPalette::WindowText;
        QPalette palette = tab->palette;

        // A tab of a document gets the light palette even in the dark theme, so the style colors
        // its label itself.
        if (tab->documentMode && QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark)
        {
            bool active = (tab->state & QStyle::State_Selected) &&
                (tab->state & QStyle::State_Active);

            palette.setColor(role, active ? Qt::white : Qt::gray);
        }

        drawItemText(painter, text_rect, alignment, palette,
                     tab->state & QStyle::State_Enabled, tab->text, role);
    }

private:
    // The icon at the left of the label and the text right next to it.
    void labelRects(const QStyleOptionTab* tab, const QWidget* widget, QRect* icon_rect,
                    QRect* text_rect) const
    {
        int padding = pixelMetric(QStyle::PM_TabBarTabHSpace, tab, widget) / 2;
        QRect rect = tab->rect.adjusted(padding, 0, -padding, 0);

        if (!tab->leftButtonSize.isEmpty())
            rect.setLeft(rect.left() + kTabElementSpacingPx + tab->leftButtonSize.width());

        if (!tab->rightButtonSize.isEmpty())
            rect.setRight(rect.right() - kTabElementSpacingPx - tab->rightButtonSize.width());

        *icon_rect = QRect();

        if (!tab->icon.isNull())
        {
            QSize icon_size = tab->iconSize;
            if (!icon_size.isValid())
            {
                int extent = pixelMetric(QStyle::PM_TabBarIconSize, tab, widget);
                icon_size = QSize(extent, extent);
            }

            *icon_rect = QRect(rect.left(), rect.center().y() - icon_size.height() / 2,
                               icon_size.width(), icon_size.height());
            rect.setLeft(rect.left() + icon_size.width() + kTabElementSpacingPx);
        }

        *text_rect = rect;
    }

    Q_DISABLE_COPY_MOVE(TabStyle)
};
#endif // defined(Q_OS_MACOS)

} // namespace

//--------------------------------------------------------------------------------------------------
TabBar::TabBar(QWidget* parent)
    : QTabBar(parent),
      pulse_timer_(new QTimer(this))
{
    pulse_timer_->setInterval(kPulseTick);
    connect(pulse_timer_, &QTimer::timeout, this, &TabBar::onPulseTick);

#if defined(Q_OS_MACOS)
    setStyle(new TabStyle(this));
#endif // defined(Q_OS_MACOS)
}

//--------------------------------------------------------------------------------------------------
TabBar::~TabBar() = default;

//--------------------------------------------------------------------------------------------------
void TabBar::setDropTarget(int index)
{
    if (drop_target_index_ == index)
        return;

    drop_target_index_ = index;

    if (index >= 0)
    {
        pulse_phase_ = MilliSeconds::zero();
        if (!pulse_timer_->isActive())
            pulse_timer_->start();
    }
    else
    {
        pulse_timer_->stop();
    }

    update();
}

//--------------------------------------------------------------------------------------------------
void TabBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        pressed_tab_index_ = tabAt(event->pos());

    QTabBar::mousePressEvent(event);
}

//--------------------------------------------------------------------------------------------------
void TabBar::mouseMoveEvent(QMouseEvent* event)
{
    QTabBar::mouseMoveEvent(event);

    if (!(event->buttons() & Qt::LeftButton) || pressed_tab_index_ < 0)
        return;

    int y = event->position().toPoint().y();
    if (y >= -kDetachThresholdPx && y <= height() + kDetachThresholdPx)
        return;

    int detach_index = pressed_tab_index_;
    pressed_tab_index_ = -1;

    // Cancel QTabBar's internal drag state so the synthetic release leaves it in a clean state.
    QMouseEvent release(QEvent::MouseButtonRelease,
                        event->position(),
                        event->scenePosition(),
                        event->globalPosition(),
                        Qt::LeftButton,
                        Qt::NoButton,
                        event->modifiers());
    QTabBar::mouseReleaseEvent(&release);

    emit sig_tabDetachRequested(detach_index, event->globalPosition().toPoint());
}

//--------------------------------------------------------------------------------------------------
void TabBar::mouseReleaseEvent(QMouseEvent* event)
{
    pressed_tab_index_ = -1;
    QTabBar::mouseReleaseEvent(event);
}

//--------------------------------------------------------------------------------------------------
void TabBar::paintEvent(QPaintEvent* event)
{
    QTabBar::paintEvent(event);

    if (isWindows11Style(style()))
    {
        const bool is_dark = palette().color(QPalette::Window).lightness() < 128;
        const QColor shade(0, 0, 0, is_dark ? kTabBarShadeDark : kTabBarShadeLight);

        QRegion around(rect());
        const int current_index = currentIndex();
        if (current_index >= 0 && isTabVisible(current_index))
            around -= QRegion(tabRect(current_index)
                                  .adjusted(kTabMarginPx, kTabMarginPx, -kTabMarginPx, 0));

        QPainter painter(this);
        painter.setClipRegion(around);
        painter.fillRect(rect(), shade);
    }

    if (drop_target_index_ < 0 || drop_target_index_ >= count())
        return;

    if (!isTabVisible(drop_target_index_))
        return;

    QRect rect = tabRect(drop_target_index_);
    if (rect.isEmpty())
        return;

    // Triangle wave: 0 -> max -> 0 over kPulsePeriod.
    double t = static_cast<double>(pulse_phase_.count()) / kPulsePeriod.count();
    double wave = (t < 0.5) ? (t * 2.0) : ((1.0 - t) * 2.0);
    int alpha = static_cast<int>(wave * kPulseMaxAlpha);

    QColor color = palette().highlight().color();
    color.setAlpha(alpha);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(color, kPulseFrameWidthPx));

    int inset = kPulseFrameWidthPx / 2 + 1;
    painter.drawRect(rect.adjusted(inset, inset, -inset, -inset));
}

//--------------------------------------------------------------------------------------------------
void TabBar::tabInserted(int index)
{
    QTabBar::tabInserted(index);

    // Replace Qt's built-in close button.
    QToolButton* close_button = new QToolButton(this);
    close_button->setIcon(QIcon(":/img/close-window.svg"));
    close_button->setIconSize(QSize(16, 16));
    close_button->setFixedSize(QSize(16, 16));
    close_button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    close_button->setAutoRaise(true);
    close_button->setCursor(Qt::ArrowCursor);
    close_button->setFocusPolicy(Qt::NoFocus);
    close_button->setStyleSheet(
        "QToolButton { border: none; margin: 0px; padding: 0px; }"
        "QToolButton:hover { background-color: rgba(0, 0, 0, 40); border-radius: 2px; }"
        "QToolButton:pressed { background-color: rgba(0, 0, 0, 80); }");

    connect(close_button, &QToolButton::clicked, this, [this, close_button]()
    {
        for (int i = 0; i < count(); ++i)
        {
            if (tabButton(i, QTabBar::RightSide) == close_button ||
                tabButton(i, QTabBar::LeftSide) == close_button)
            {
                emit tabCloseRequested(i);
                break;
            }
        }
    });

    // The button belongs to the right on every platform. macOS asks for it on the left, so the one
    // Qt has already put there is dropped.
    setTabButton(index, QTabBar::LeftSide, nullptr);
    setTabButton(index, QTabBar::RightSide, close_button);
}

//--------------------------------------------------------------------------------------------------
void TabBar::onPulseTick()
{
    pulse_phase_ = (pulse_phase_ + kPulseTick) % kPulsePeriod;
    update();
}
