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

#ifndef COMMON_UI_LANGUAGE_ACTION_H
#define COMMON_UI_LANGUAGE_ACTION_H

#include <QAction>

#include "base/macros_magic.h"

namespace common {

class LanguageAction final : public QAction
{
public:
    LanguageAction(const QString& locale_code,
                   const QString& locale_name,
                   QObject* parent = nullptr);
    ~LanguageAction() final = default;

    const QString& locale() const { return locale_code_; }

private:
    QString locale_code_;

    DISALLOW_COPY_AND_ASSIGN(LanguageAction);
};

} // namespace common

#endif // COMMON_UI_LANGUAGE_ACTION_H
