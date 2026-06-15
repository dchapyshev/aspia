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

#ifndef COMMON_ANDROID_EXPANDABLE_PANEL_H
#define COMMON_ANDROID_EXPANDABLE_PANEL_H

#include <QPoint>
#include <QWidget>

class QHBoxLayout;
class QTimer;
class QVBoxLayout;
class QVariantAnimation;

// A touch list item with a fixed-height header bar and a panel below it that slides open and shut.
class ExpandablePanel : public QWidget
{
    Q_OBJECT

public:
    explicit ExpandablePanel(QWidget* parent = nullptr);
    ~ExpandablePanel() override;

    // The header bar laid out horizontally; add the header widgets here.
    QHBoxLayout* headerLayout() const { return header_layout_; }

    // The collapsible area below the header; add the content widgets here. Call contentChanged()
    // after adding or removing content while expanded so the panel resizes to fit.
    QVBoxLayout* contentLayout() const { return content_layout_; }

    bool isExpanded() const { return expanded_; }
    void setExpanded(bool expanded);

    // Resizes an open panel to fit its current content. Has no effect while collapsed or animating.
    void contentChanged();

    // QWidget implementation.
    QSize sizeHint() const final;

signals:
    void sig_headerClicked();
    void sig_headerLongPressed();

protected:
    // QWidget implementation.
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) final;
    void mousePressEvent(QMouseEvent* event) final;
    void mouseMoveEvent(QMouseEvent* event) final;
    void mouseReleaseEvent(QMouseEvent* event) final;

private:
    int contentHeight() const;

    QWidget* header_ = nullptr;
    QWidget* content_ = nullptr;
    QHBoxLayout* header_layout_ = nullptr;
    QVBoxLayout* content_layout_ = nullptr;
    QVariantAnimation* animation_ = nullptr;
    QTimer* long_press_timer_ = nullptr;
    QPoint press_pos_;
    bool expanded_ = false;
    bool long_pressed_ = false;

    Q_DISABLE_COPY_MOVE(ExpandablePanel)
};

#endif // COMMON_ANDROID_EXPANDABLE_PANEL_H
