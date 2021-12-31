//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef COMMON__CLIPBOARD_H
#define COMMON__CLIPBOARD_H

#include "proto/desktop.pb.h"

#include <memory>

namespace common {

class Clipboard
{
public:
    virtual ~Clipboard() = default;

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onClipboardEvent(const proto::ClipboardEvent& event) = 0;
    };

    void start(Delegate* delegate);

    // Receiving the incoming clipboard.
    void injectClipboardEvent(const proto::ClipboardEvent& event);
    void clearClipboard();

protected:
    virtual void init() = 0;
    virtual void setData(const std::string& data) = 0;
    void onData(const std::string& data);

private:
    Delegate* delegate_ = nullptr;
    std::string last_data_;
};

} // namespace common

#endif // COMMON__CLIPBOARD_H
