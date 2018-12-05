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

#ifndef ASPIA_UPDATER__UPDATE_CHECKER_H_
#define ASPIA_UPDATER__UPDATE_CHECKER_H_

#include <QObject>

#include <memory>
#include <string>

#include "base/macros_magic.h"
#include "updater/update.h"

namespace aspia {

class UpdateChecker : public QObject
{
    Q_OBJECT

public:
    explicit UpdateChecker(QObject* parent = nullptr);
    ~UpdateChecker();

    void checkForUpdates(const std::string& url);

signals:
    void updateAvailable(const Update& update);

private:
    DISALLOW_COPY_AND_ASSIGN(UpdateChecker);
};

} // namespace aspia

#endif // ASPIA_UPDATER__UPDATE_CHECKER_H_
