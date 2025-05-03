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

#ifndef BASE_TRANSLATIONS_H
#define BASE_TRANSLATIONS_H

#include "base/macros_magic.h"

#include <utility>

#include <QHash>
#include <QStringList>
#include <QVector>

class QTranslator;

namespace base {

class Translations
{
public:
    Translations();
    ~Translations();

    using Locale = std::pair<QString, QString>;
    using LocaleList = QVector<Locale>;

    LocaleList localeList() const;
    bool contains(const QString& locale) const;
    void installTranslators(const QString& locale);

private:
    void removeTranslators();

    QHash<QString, QStringList> locale_list_;
    QVector<QTranslator*> translator_list_;

    DISALLOW_COPY_AND_ASSIGN(Translations);
};

} // namespace base

#endif // BASE_TRANSLATIONS_H
