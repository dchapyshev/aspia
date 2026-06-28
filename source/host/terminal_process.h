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

#ifndef HOST_TERMINAL_PROCESS_H
#define HOST_TERMINAL_PROCESS_H

#include <QObject>

#include "base/shared_pointer.h"

// Pseudo-terminal process: spawns the operating system shell behind a pseudo-terminal and relays its
// byte stream. The shell output is read asynchronously on the agent's I/O context (no dedicated
// thread) and delivered through sig_output; keyboard input and resize requests are passed in via
// writeInput()/resize(). Use create() to obtain the platform-specific implementation.
class TerminalProcess : public QObject
{
    Q_OBJECT

public:
    ~TerminalProcess() override = default;

    // Creates a platform-specific TerminalProcess instance owned by |parent|.
    static TerminalProcess* create(QObject* parent = nullptr);

    virtual bool start(int columns, int rows) = 0;
    virtual void writeInput(const QByteArray& data) = 0;
    virtual void resize(int columns, int rows) = 0;

signals:
    void sig_output(const QByteArray& data);
    void sig_finished();

protected:
    explicit TerminalProcess(QObject* parent = nullptr);

    // Guards asynchronous completion handlers: set to false before the implementation releases its
    // resources, so that an already-queued handler does not touch a destroyed object.
    SharedPointer<bool> alive_guard_ { new bool(true) };

private:
    Q_DISABLE_COPY_MOVE(TerminalProcess)
};

#endif // HOST_TERMINAL_PROCESS_H
