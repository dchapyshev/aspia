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

#include "common/locale_loader.h"

#include <QCoreApplication>
#include <QDir>
#include "QLocale"
#include <QTranslator>
#include <QString>

#include "base/logging.h"

namespace aspia {

LocaleLoader::LocaleLoader()
{
    QStringList qm_file_list =
        QDir(translationsDir()).entryList(QStringList() << QLatin1String("*.qm"));

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

QStringList LocaleLoader::localeList() const
{
    QStringList list;

    QHashIterator<QString, QStringList> iter(locale_list_);
    while (iter.hasNext())
    {
        iter.next();
        list.push_back(iter.key());
    }

    const QString english_locale = QStringLiteral("en");
    if (!locale_list_.contains(english_locale))
        list.push_back(english_locale);

    return list;
}

QStringList LocaleLoader::sortedLocaleList() const
{
    QStringList list = localeList();

    std::sort(list.begin(), list.end(), [](const QString& a, const QString& b)
    {
        return QString::compare(QLocale::languageToString(QLocale(a).language()),
                                QLocale::languageToString(QLocale(b).language()),
                                Qt::CaseInsensitive) < 0;
    });

    return list;
}

QStringList LocaleLoader::fileList(const QString& locale_name) const
{
    if (!contains(locale_name))
        return QStringList();

    return locale_list_[locale_name];
}

bool LocaleLoader::contains(const QString& locale_name) const
{
    return locale_list_.contains(locale_name);
}

void LocaleLoader::installTranslators(const QString& locale_name)
{
    removeTranslators();

    const QString translations_dir = translationsDir();

    for (const auto& qm_file : fileList(locale_name))
    {
        QTranslator* translator = new QTranslator();

        if (!translator->load(qm_file, translations_dir))
        {
            LOG(LS_WARNING) << "Translation file not loaded: " << qm_file.toStdString();
            delete translator;
        }
        else
        {
            QCoreApplication::installTranslator(translator);
            translator_list_.push_back(translator);
        }
    }
}

// static
QString LocaleLoader::translationsDir()
{
    return QLatin1String(":/tr/");
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

} // namespace aspia
