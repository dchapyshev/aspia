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

#ifndef COMMON_ANDROID_APP_BAR_H
#define COMMON_ANDROID_APP_BAR_H

#include <QList>
#include <QWidget>

class QLineEdit;

// Top app bar for touch screens: a title with an optional leading back button and trailing action
// buttons. Sits at the top of a screen and drives screen-level navigation. In search mode the title
// is replaced by an inline text field.
class AppBar final : public QWidget
{
    Q_OBJECT

public:
    explicit AppBar(QWidget* parent = nullptr);
    ~AppBar() final;

    void setTitle(const QString& title);
    const QString& title() const { return title_; }

    void setBackVisible(bool visible);
    bool isBackVisible() const { return back_visible_; }

    // Places |actions| as trailing buttons; pass an empty list to clear them. The widgets are
    // reparented into the bar.
    void setActions(const QList<QWidget*>& actions);

    // Replaces the title with an inline, focused search field; the field is cleared on entry and its
    // text is reported through sig_searchTextChanged(). The caller drives the back button and actions.
    void setSearchMode(bool enabled);
    bool isSearchMode() const { return search_mode_; }

    // QWidget implementation.
    QSize sizeHint() const final;

signals:
    void sig_backClicked();
    void sig_searchTextChanged(const QString& text);

protected:
    // QWidget implementation.
    void paintEvent(QPaintEvent* event) final;
    void mousePressEvent(QMouseEvent* event) final;
    void resizeEvent(QResizeEvent* event) final;

private:
    void relayoutActions();
    void relayoutSearchField();
    int actionsWidth() const;

    QString title_;
    bool back_visible_;
    bool search_mode_ = false;
    QList<QWidget*> actions_;
    QLineEdit* search_field_ = nullptr;

    Q_DISABLE_COPY_MOVE(AppBar)
};

#endif // COMMON_ANDROID_APP_BAR_H
