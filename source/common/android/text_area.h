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

#ifndef COMMON_ANDROID_TEXT_AREA_H
#define COMMON_ANDROID_TEXT_AREA_H

#include <QString>
#include <QWidget>

class QPlainTextEdit;
class QVariantAnimation;

// Multi-line text input.
class TextArea final : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QString label READ label WRITE setLabel)

public:
    explicit TextArea(QWidget* parent = nullptr);
    ~TextArea() final;

    void setLabel(const QString& label);
    const QString& label() const { return label_; }

    QString text() const;
    void setText(const QString& text);
    void clear();
    void setPlaceholderText(const QString& text);

    // QWidget implementation.
    QSize sizeHint() const final;
    QSize minimumSizeHint() const final;

protected:
    // QWidget implementation.
    void paintEvent(QPaintEvent* event) final;
    bool eventFilter(QObject* watched, QEvent* event) final;

private slots:
    void onTextChanged();

private:
    int labelOverflow() const;
    void updateFloatState();
    void animateFocus(bool focused);

    QPlainTextEdit* edit_ = nullptr;
    QVariantAnimation* float_animation_ = nullptr;
    QVariantAnimation* focus_animation_ = nullptr;
    double float_progress_ = 0.0;
    double focus_progress_ = 0.0;
    QString label_;

    Q_DISABLE_COPY_MOVE(TextArea)
};

#endif // COMMON_ANDROID_TEXT_AREA_H
