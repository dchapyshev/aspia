//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/macros_magic.h"
#include "proto/desktop.pb.h"

#include <QObject>

namespace common {

class Clipboard : public QObject
{
    Q_OBJECT

public:
    Clipboard(QObject* parent = nullptr);
    ~Clipboard() = default;

public slots:
    // Receiving the incoming clipboard.
    void injectClipboardEvent(const proto::ClipboardEvent& event);

signals:
    void clipboardEvent(const proto::ClipboardEvent& event);

private:
    std::string last_mime_type_;
    std::string last_data_;

    DISALLOW_COPY_AND_ASSIGN(Clipboard);
};

} // namespace common

#endif // COMMON__CLIPBOARD_H
