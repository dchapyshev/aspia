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

#include "qt_base/locale_loader.h"

#include <QCoreApplication>
#include <QDir>
#include "QLocale"
#include <QTranslator>

namespace qt_base {

namespace {

const QString kTranslationsDir = QLatin1String(":/tr/");

} // namespace

LocaleLoader::LocaleLoader()
{
    QStringList qm_file_list =
        QDir(kTranslationsDir).entryList(QStringList() << QLatin1String("*.qm"));

    QRegExp regexp(QLatin1String("([a-zA-Z0-9-_]+)_([^.]*).qm"));

    for (const auto& qm_file : qm_file_list)
    {
        if (regexp.exactMatch(qm_file))
        {
            const QString locale_name = regexp.cap(2);

            if (locale_list_.contains(locale_name))
                locale_list_[locale_name].push_back(qm_file);
            else
                locale_list_.insert(locale_name, QStringList() << qm_file);
        }
    }
}

LocaleLoader::~LocaleLoader()
{
    removeTranslators();
}

LocaleLoader::LocaleList LocaleLoader::localeList() const
{
    LocaleList list;

    auto add_locale = [&](const QString& locale_code)
    {
        list.push_back(
            Locale(locale_code, QLocale::languageToString(QLocale(locale_code).language())));
    };

    add_locale(QLatin1String("en"));

    for (auto it = locale_list_.constBegin(); it != locale_list_.constEnd(); ++it)
        add_locale(it.key());

    std::sort(list.begin(), list.end(), [](const Locale& a, const Locale& b)
    {
        return QString::compare(a.second, b.second, Qt::CaseInsensitive) < 0;
    });

    return list;
}

bool LocaleLoader::contains(const QString& locale) const
{
    return locale_list_.contains(locale);
}

void LocaleLoader::installTranslators(const QString& locale)
{
    removeTranslators();

    auto file_list = locale_list_.constFind(locale);
    if (file_list == locale_list_.constEnd())
        return;

    for (const auto& file : file_list.value())
    {
        QScopedPointer<QTranslator> translator(new QTranslator());

        if (translator->load(file, kTranslationsDir))
        {
            if (QCoreApplication::installTranslator(translator.get()))
                translator_list_.push_back(translator.take());
        }
    }
}

void LocaleLoader::removeTranslators()
{
    for (const auto& translator : translator_list_)
    {
        QCoreApplication::removeTranslator(translator);
        delete translator;
    }

    translator_list_.clear();
}

} // namespace qt_base
