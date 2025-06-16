//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_NET_CONNECT_ENUMERATOR_H
#define BASE_NET_CONNECT_ENUMERATOR_H

#include <QString>

#include "base/win/scoped_object.h"

namespace base {

class ConnectEnumerator
{
public:
    enum class Mode { TCP, UDP };

    explicit ConnectEnumerator(Mode mode);
    ~ConnectEnumerator();

    bool isAtEnd() const;
    void advance();

    QString protocol() const;
    QString processName() const;
    QString localAddress() const;
    QString remoteAddress() const;
    quint16 localPort() const;
    quint16 remotePort() const;
    QString state() const;

private:
    const Mode mode_;
    base::ScopedHandle snapshot_;
    QByteArray table_buffer_;

    quint32 num_entries_ = 0;
    quint32 pos_ = 0;

    Q_DISABLE_COPY(ConnectEnumerator)
};

} // namespace base

#endif // BASE_NET_CONNECT_ENUMERATOR_H
