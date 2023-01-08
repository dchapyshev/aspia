//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef QT_BASE_LOCALE_LOADER_H
#define QT_BASE_LOCALE_LOADER_H

#include "base/macros_magic.h"

#include <QHash>
#include <QStringList>
#include <QVector>

class QTranslator;

namespace qt_base {

class LocaleLoader
{
public:
    LocaleLoader();
    ~LocaleLoader();

    using Locale = QPair<QString, QString>;
    using LocaleList = QVector<Locale>;

    LocaleList localeList() const;
    bool contains(const QString& locale) const;
    void installTranslators(const QString& locale);

private:
    void removeTranslators();

    QHash<QString, QStringList> locale_list_;
    QVector<QTranslator*> translator_list_;

    DISALLOW_COPY_AND_ASSIGN(LocaleLoader);
};

} // namespace qt_base

#endif // QT_BASE_LOCALE_LOADER_H
