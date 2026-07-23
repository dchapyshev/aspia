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

#ifndef COMMON_ANDROID_BOTTOM_SHEET_H
#define COMMON_ANDROID_BOTTOM_SHEET_H

#include <QList>
#include <QPoint>
#include <QString>
#include <QWidget>

#include "base/time_types.h"

class QVariantAnimation;

// Modal action sheet anchored to the bottom edge over the whole window: an optional title and a list
// of actions. The actions are laid out as a vertical list in portrait orientation and as a wrapping
// row of compact buttons (icon over a caption) in landscape. A tap on the scrim outside the sheet
// dismisses it.
class BottomSheet final : public QWidget
{
    Q_OBJECT

public:
    explicit BottomSheet(QWidget* parent = nullptr);
    ~BottomSheet() final;

    void setTitle(const QString& title);

    // A |selected| item is drawn highlighted (accent color) to mark the current choice. When |tinted|
    // is false the icon is drawn in its own colors instead of being recolored to the theme.
    void addItem(const QString& text, const QString& icon_file_path = QString(), bool selected = false,
                 bool tinted = true);

    // Marks |index| as the selected item (and clears the rest); pass -1 to clear. Updates a sheet
    // that is already shown, e.g. when the current screen changes from another client.
    void setSelected(int index);

    // Covers the top-level window and shows the sheet over its content.
    void showSheet();

signals:
    void sig_triggered(int index);

    // Emitted after the drag handle is tapped several times in quick succession - a hidden gesture
    // used to reveal the statistics dialog without a dedicated button.
    void sig_secretGesture();

protected:
    // QWidget implementation.
    void paintEvent(QPaintEvent* event) final;
    void mouseMoveEvent(QMouseEvent* event) final;
    void mousePressEvent(QMouseEvent* event) final;
    void mouseReleaseEvent(QMouseEvent* event) final;

    // Keeps the sheet covering the host window when it is resized (e.g. an orientation change).
    bool eventFilter(QObject* watched, QEvent* event) final;

private:
    struct Item
    {
        QString icon_file_path;
        QString text;
        bool selected = false;
        bool tinted = true;
    };

    // Animates the vertical offset to |target_offset| (0 = fully shown), closing the sheet when the
    // animation finishes if |close_after| is set.
    void animateTo(int target_offset, bool close_after);

    // Slides the sheet down and out, then closes it.
    void dismiss();

    bool isPortrait() const;

    // Recomputes the bottom system-bar inset (navigation bar) so the sheet rests above it instead of
    // being clipped. Cached because it is queried over JNI; refreshed on show and on resize.
    void updateBottomInset();

    QRect sheetRect() const;

    // Tap target for the hidden gesture: the strip at the top of the sheet that holds the handle.
    QRect handleHitRect() const;
    int itemsTop() const;
    int columnCount() const;
    int rowCount() const;

    // Height of a landscape cell, sized to fit the icon and a caption wrapped to up to two lines.
    int landscapeCellHeight() const;

    // Width reserved on the leading side for the title shown inline with the buttons in landscape
    // (zero when there is no title); includes the gap before the first button.
    int landscapeTitleWidth() const;

    QRect itemRect(int index) const;
    int itemAt(const QPoint& pos) const;

    QString title_;
    QList<Item> items_;
    int active_ = -1;

    // Bottom system-bar inset, kept above the navigation bar. Refreshed on show and resize.
    int bottom_inset_ = 0;

    // Vertical offset of the sheet below its resting position, driven by the drag-to-dismiss gesture
    // and the show/hide animation (0 = fully shown).
    int offset_ = 0;
    int press_y_ = 0;
    bool pressed_ = false;
    bool dragging_ = false;
    bool close_on_finish_ = false;
    QVariantAnimation* offset_animation_ = nullptr;

    // Consecutive taps on the handle strip; reset when too slow (see handle tap constants in the .cc).
    int handle_taps_ = 0;
    TimePoint handle_tap_time_;

    Q_DISABLE_COPY_MOVE(BottomSheet)
};

#endif // COMMON_ANDROID_BOTTOM_SHEET_H
