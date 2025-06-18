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

#include "base/translations.h"

#include "base/logging.h"

#include <QCoreApplication>
#include <QDir>
#include <QLocale>
#include <QTranslator>

namespace base {

namespace {

const QString kTranslationsDir = QStringLiteral(":/tr/");

} // namespace

//--------------------------------------------------------------------------------------------------
Translations::Translations()
{
    LOG(INFO) << "Ctor";

    const QStringList qm_file_list =
        QDir(kTranslationsDir).entryList(QStringList("*.qm"), QDir::Files);

    for (const auto& qm_file : qm_file_list)
    {
        QString locale_name = qm_file.chopped(3); // Remove file extension (*.qm).

        if (locale_name.right(2).isUpper())
        {
            // xx_XX (language / country).
            locale_name = locale_name.right(5);
        }
        else
        {
            // xx (language only).
            locale_name = locale_name.right(2);
        }

        LOG(INFO) << "Added:" << qm_file << locale_name;

        if (locale_list_.contains(locale_name))
            locale_list_[locale_name].push_back(qm_file);
        else
            locale_list_.insert(locale_name, QStringList(qm_file));
    }
}

//--------------------------------------------------------------------------------------------------
Translations::~Translations()
{
    LOG(INFO) << "Dtor";
    removeTranslators();
}

//--------------------------------------------------------------------------------------------------
Translations::LocaleList Translations::localeList() const
{
    LocaleList list;

    auto add_locale = [&](const QString& locale_code)
    {
        QLocale locale(locale_code);
        QString name;

        if (locale_code.length() == 2)
        {
            name = QLocale::languageToString(locale.language());
        }
        else
        {
            name = QLocale::languageToString(locale.language())
                + " (" + QLocale::territoryToString(locale.territory()) + ")";
        }

        list.push_back(Locale(locale_code, name));
    };

    add_locale("en");

    for (auto it = locale_list_.constBegin(); it != locale_list_.constEnd(); ++it)
        add_locale(it.key());

    std::sort(list.begin(), list.end(), [](const Locale& a, const Locale& b)
    {
        return QString::compare(a.second, b.second, Qt::CaseInsensitive) < 0;
    });

    return list;
}

//--------------------------------------------------------------------------------------------------
bool Translations::contains(const QString& locale) const
{
    return locale_list_.contains(locale);
}

//--------------------------------------------------------------------------------------------------
void Translations::installTranslators(const QString& locale)
{
    removeTranslators();

    LOG(INFO) << "Install translators for:" << locale;

    auto file_list = locale_list_.constFind(locale);
    if (file_list == locale_list_.constEnd())
        return;

    for (const auto& file : file_list.value())
    {
        std::unique_ptr<QTranslator> translator = std::make_unique<QTranslator>();

        if (translator->load(file, kTranslationsDir))
        {
            if (QCoreApplication::installTranslator(translator.get()))
                translator_list_.push_back(translator.release());
        }
    }
}

//--------------------------------------------------------------------------------------------------
void Translations::removeTranslators()
{
    LOG(INFO) << "Cleanup translators";

    for (auto it = translator_list_.begin(), it_end = translator_list_.end(); it != it_end; ++it)
    {
        QCoreApplication::removeTranslator(*it);
        delete *it;
    }

    translator_list_.clear();
}

} // namespace base
