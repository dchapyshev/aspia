//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef COMMON__UI__LANGUAGE_ACTION_H
#define COMMON__UI__LANGUAGE_ACTION_H

#include "base/macros_magic.h"

#include <QAction>

namespace common {

class LanguageAction : public QAction
{
public:
    LanguageAction(const QString& locale_code,
                   const QString& locale_name,
                   QObject* parent = nullptr);
    ~LanguageAction() override = default;

    const QString& locale() const { return locale_code_; }

private:
    QString locale_code_;

    DISALLOW_COPY_AND_ASSIGN(LanguageAction);
};

} // namespace common

#endif // COMMON__UI__LANGUAGE_ACTION_H
