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

#ifndef CONSOLE_MRU_ACTION_H
#define CONSOLE_MRU_ACTION_H

#include <QAction>

namespace console {

class MruAction final : public QAction
{
    Q_OBJECT

public:
    MruAction(const QString& file, QObject* parent = nullptr);

    QString filePath() const { return file_path_; }

private:
    QString file_path_;
    Q_DISABLE_COPY(MruAction)
};

} // namespace console

#endif // CONSOLE_MRU_ACTION_H
