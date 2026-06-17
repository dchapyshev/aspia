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

    // A |selected| item is drawn highlighted (accent color) to mark the current choice.
    void addItem(const QString& text, const QString& icon_file_path = QString(), bool selected = false);

    // Covers the top-level window and shows the sheet over its content.
    void showSheet();

signals:
    void sig_triggered(int index);

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
    };

    // Animates the vertical offset to |target_offset| (0 = fully shown), closing the sheet when the
    // animation finishes if |close_after| is set.
    void animateTo(int target_offset, bool close_after);

    // Slides the sheet down and out, then closes it.
    void dismiss();

    bool isPortrait() const;
    QRect sheetRect() const;
    int itemsTop() const;
    int columnCount() const;
    int rowCount() const;

    // Width reserved on the leading side for the title shown inline with the buttons in landscape
    // (zero when there is no title); includes the gap before the first button.
    int landscapeTitleWidth() const;

    QRect itemRect(int index) const;
    int itemAt(const QPoint& pos) const;

    QString title_;
    QList<Item> items_;
    int active_ = -1;

    // Vertical offset of the sheet below its resting position, driven by the drag-to-dismiss gesture
    // and the show/hide animation (0 = fully shown).
    int offset_ = 0;
    int press_y_ = 0;
    bool pressed_ = false;
    bool dragging_ = false;
    bool close_on_finish_ = false;
    QVariantAnimation* offset_animation_ = nullptr;

    Q_DISABLE_COPY_MOVE(BottomSheet)
};

#endif // COMMON_ANDROID_BOTTOM_SHEET_H
