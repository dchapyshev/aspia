//
// PROJECT:         Aspia
// FILE:            base/locale_loader.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__LOCALE_LOADER_H
#define _ASPIA_BASE__LOCALE_LOADER_H

#include <QHash>
#include <QStringList>

class QTranslator;

namespace aspia {

class LocaleLoader
{
public:
    LocaleLoader();
    ~LocaleLoader();

    QStringList localeList() const;
    QStringList sortedLocaleList() const;
    QStringList fileList(const QString& locale_name) const;
    bool contains(const QString& locale_name) const;
    void installTranslators(const QString& locale_name);

    static QString translationsDir();

private:
    void removeTranslators();

    QHash<QString, QStringList> locale_list_;
    QList<QTranslator*> translator_list_;

    Q_DISABLE_COPY(LocaleLoader)
};

} // namespace aspia

#endif // _ASPIA_BASE__LOCALE_LOADER_H
