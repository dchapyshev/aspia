//
// PROJECT:         Aspia
// FILE:            base/locale_loader.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/locale_loader.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QTranslator>

namespace aspia {

LocaleLoader::LocaleLoader()
{
    QStringList qm_file_list =
        QDir(translationsDir()).entryList(QStringList() << QStringLiteral("*.qm"));

    QRegExp regexp(QStringLiteral("([a-zA-Z0-9-_]+)_([^.]*).qm"));

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
            qWarning() << "Translation file not loaded: " << qm_file;
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
    return QCoreApplication::applicationDirPath() + QStringLiteral("/translations/");
}

void LocaleLoader::removeTranslators()
{
    for (auto translator : translator_list_)
    {
        QCoreApplication::removeTranslator(translator);
        delete translator;
    }

    translator_list_.clear();
}

} // namespace aspia
