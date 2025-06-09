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

#ifndef COMMON_CLIPBOARD_H
#define COMMON_CLIPBOARD_H

#include "proto/desktop.h"

#include <QObject>

namespace common {

class Clipboard : public QObject
{
    Q_OBJECT

public:
    explicit Clipboard(QObject* parent);
    virtual ~Clipboard() = default;

public slots:
    void start();
    void injectClipboardEvent(const proto::desktop::ClipboardEvent& event);
    void clearClipboard();

signals:
    void sig_clipboardEvent(const proto::desktop::ClipboardEvent& event);

protected:
    virtual void init() = 0;
    virtual void setData(const QString& data) = 0;
    void onData(const QString& data);

private:
    QString last_data_;
};

} // namespace common

#endif // COMMON_CLIPBOARD_H
