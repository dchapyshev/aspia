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

#ifndef CLIENT_UI_CLIENT_TAB_H
#define CLIENT_UI_CLIENT_TAB_H

#include <QAction>
#include <QList>
#include <QWidget>

class QStatusBar;
class QToolBar;

namespace client {

class ClientTab : public QWidget
{
    Q_OBJECT

public:
    enum class Type { COMPUTERS, SESSION };

    explicit ClientTab(Type type, QWidget* parent = nullptr);
    ~ClientTab() override;

    Type tabType() const;
    bool isClosable() const;

    virtual void onActivated(QToolBar* toolbar, QStatusBar* statusbar) = 0;
    virtual void onDeactivated(QToolBar* toolbar, QStatusBar* statusbar) = 0;

    virtual bool hasSearchField() const;
    virtual void onSearchTextChanged(const QString& text);

signals:
    void sig_titleChanged(const QString& title);

protected:
    void addToolbarAction(QToolBar* toolbar, QAction* action, QAction* before);
    void removeAllToolbarActions(QToolBar* toolbar);

    QList<QAction*> toolbar_actions_;

private:
    Type type_;

    Q_DISABLE_COPY_MOVE(ClientTab)
};

} // namespace client

#endif // CLIENT_UI_CLIENT_TAB_H
