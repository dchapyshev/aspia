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

#ifndef CLIENT_UI_HOSTS_CONTENT_WIDGET_H
#define CLIENT_UI_HOSTS_CONTENT_WIDGET_H

#include <QWidget>

class QStatusBar;

namespace client {

class ContentWidget : public QWidget
{
    Q_OBJECT

public:
    enum class Type { LOCAL_GROUP, ROUTER, ROUTER_GROUP, SEARCH };

    explicit ContentWidget(Type type, QWidget* parent = nullptr);
    ~ContentWidget() override;

    Type contentType() const;
    virtual QByteArray saveState()  = 0;
    virtual void restoreState(const QByteArray& state) = 0;
    virtual bool canReload() const { return false; }
    virtual void reload() {}
    virtual bool canSave() const { return false; }
    virtual void save() {}
    virtual void attach(QStatusBar* /* statusbar */) {}
    virtual void detach(QStatusBar* /* statusbar */) {}

private:
    Type type_;

    Q_DISABLE_COPY_MOVE(ContentWidget)
};

} // namespace client

#endif // CLIENT_UI_HOSTS_CONTENT_WIDGET_H
