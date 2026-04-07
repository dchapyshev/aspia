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
#include <QPair>
#include <QWidget>

class QStatusBar;

namespace client {

class ClientTab : public QWidget
{
    Q_OBJECT

public:
    enum class Type { COMPUTERS, SESSION };

    enum class ActionGroup
    {
        EDIT,
        SESSION_TYPE
    };

    explicit ClientTab(Type type, QWidget* parent = nullptr);
    ~ClientTab() override;

    Type tabType() const;
    bool isClosable() const;

    virtual void onActivated(QStatusBar* statusbar) = 0;
    virtual void onDeactivated(QStatusBar* statusbar) = 0;

    virtual bool hasSearchField() const;
    virtual void onSearchTextChanged(const QString& text);

    using ActionGroupEntry = QPair<ActionGroup, QList<QAction*>>;
    const QList<ActionGroupEntry>& actionGroups() const;

signals:
    void sig_titleChanged(const QString& title);

protected:
    void addActions(ActionGroup group, const QList<QAction*>& actions);

private:
    Type type_;
    QList<ActionGroupEntry> action_groups_;

    Q_DISABLE_COPY_MOVE(ClientTab)
};

} // namespace client

#endif // CLIENT_UI_CLIENT_TAB_H
