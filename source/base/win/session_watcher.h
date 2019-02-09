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

#ifndef BASE__WIN__SESSION_WATCHER_H
#define BASE__WIN__SESSION_WATCHER_H

#include <QWidget>

#include "base/macros_magic.h"

namespace base::win {

class SessionWatcher : public QWidget
{
    Q_OBJECT

public:
    explicit SessionWatcher(QWidget* parent = nullptr);
    ~SessionWatcher() = default;

    void start();
    void stop();

signals:
    void sessionEvent(uint32_t status_code, uint32_t session_id);

protected:
    // QWidget implementation.
    bool nativeEvent(const QByteArray& event_type, void* message, long* result) override;

private:
    DISALLOW_COPY_AND_ASSIGN(SessionWatcher);
};

} // namespace base::win

#endif // BASE__WIN__SESSION_WATCHER_H
