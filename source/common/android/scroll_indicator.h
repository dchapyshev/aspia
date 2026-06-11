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

#ifndef COMMON_ANDROID_SCROLL_INDICATOR_H
#define COMMON_ANDROID_SCROLL_INDICATOR_H

#include <QWidget>

class QAbstractScrollArea;
class QTimer;
class QVariantAnimation;

// Thin overlay scroll position indicator for touch scroll areas: a handle along the trailing edge
// that appears while scrolling and fades out afterwards. It does not reserve layout space and is
// transparent to input. Attaches itself to the scroll area passed to the constructor.
class ScrollIndicator final : public QWidget
{
    Q_OBJECT

public:
    // |margin| insets the indicator from the trailing, top and bottom edges; pass the corner
    // radius for a rounded scroll area so it does not overlap the corners.
    explicit ScrollIndicator(QAbstractScrollArea* area, int margin = 2);
    ~ScrollIndicator() final;

protected:
    // QWidget implementation.
    bool eventFilter(QObject* watched, QEvent* event) final;
    void paintEvent(QPaintEvent* event) final;

private slots:
    void onScrolled();

private:
    void updatePlacement();

    QAbstractScrollArea* area_;
    QVariantAnimation* fade_;
    QTimer* hide_timer_;
    int margin_;
    double opacity_;

    Q_DISABLE_COPY_MOVE(ScrollIndicator)
};

#endif // COMMON_ANDROID_SCROLL_INDICATOR_H
