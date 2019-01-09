//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ASPIA_CLIENT__UI__SESSION_WINDOW_H
#define ASPIA_CLIENT__UI__SESSION_WINDOW_H

#include <QWidget>

#include "base/macros_magic.h"
#include "client/connect_data.h"

namespace aspia {

class ClientSession;
class StatusDialog;

class SessionWindow : public QWidget
{
    Q_OBJECT

public:
    virtual ~SessionWindow();

    static SessionWindow* create(const ConnectData& connect_data, QWidget* parent = nullptr);
    static bool connectTo(const ConnectData* connect_data, QWidget* parent = nullptr);

    void startSession();

protected:
    explicit SessionWindow(QWidget* parent);

    virtual void sessionStarted();
    virtual void sessionFinished();

    template <class ClassType, class ... Args>
    void createSession(Args ... args)
    {
        session_ = new ClassType(args..., this);
    }

    ClientSession* currentSession() { return session_; }

private:
    StatusDialog* status_dialog_;
    ClientSession* session_;

    DISALLOW_COPY_AND_ASSIGN(SessionWindow);
};

} // namespace aspia

#endif // ASPIA_CLIENT__UI__SESSION_WINDOW_H
