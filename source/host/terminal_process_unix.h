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

#ifndef HOST_TERMINAL_PROCESS_UNIX_H
#define HOST_TERMINAL_PROCESS_UNIX_H

#include "host/terminal_process.h"

#include <QByteArray>
#include <QQueue>

#include <array>

#include <asio/posix/stream_descriptor.hpp>

// Pseudo-terminal process backed by forkpty(). The shell output is read asynchronously from the
// master descriptor on the agent's I/O context.
class TerminalProcessUnix final : public TerminalProcess
{
    Q_OBJECT

public:
    explicit TerminalProcessUnix(QObject* parent = nullptr);
    ~TerminalProcessUnix() final;

    // TerminalProcess implementation.
    bool start(int columns, int rows) final;
    void writeInput(const QByteArray& data) final;
    void resize(int columns, int rows) final;

private:
    struct IoState
    {
        bool alive = true;
        std::array<char, 4096> read_buffer;
        QQueue<QByteArray> write_queue;
    };

    void doRead();
    void doWrite();

    SharedPointer<IoState> io_ { new IoState() };
    asio::posix::stream_descriptor pty_;
    int child_pid_ = -1;

    Q_DISABLE_COPY_MOVE(TerminalProcessUnix)
};

#endif // HOST_TERMINAL_PROCESS_UNIX_H
