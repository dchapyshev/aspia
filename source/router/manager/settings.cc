//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "router/manager/settings.h"

#include "qt_base/qt_xml_settings.h"

#include <QLocale>

namespace router {

Settings::Settings()
    : settings_(qt_base::QtXmlSettings::format(),
                QSettings::UserScope,
                QLatin1String("aspia"),
                QLatin1String("router_manager"))
{
    // Nothing
}

Settings::~Settings() = default;

QString Settings::locale() const
{
    return settings_.value(QLatin1String("Locale"), QLocale::system().bcp47Name()).toString();
}

void Settings::setLocale(const QString& locale)
{
    settings_.setValue(QLatin1String("Locale"), locale);
}

MruCache Settings::mru(int max_size) const
{
    MruCache mru(max_size);

    int count = settings_.beginReadArray(QLatin1String("MRU"));
    for (int i = 0; i < count; ++i)
    {
        settings_.setArrayIndex(i);

        MruCache::Entry entry;
        entry.address = settings_.value(QLatin1String("Address")).toString();
        entry.port = settings_.value(QLatin1String("Port")).toUInt();
        entry.username = settings_.value(QLatin1String("UserName")).toString();
        entry.key_path = settings_.value(QLatin1String("KeyPath")).toString();

        mru.put(std::move(entry));
    }
    settings_.endArray();

    if (mru.isEmpty())
    {
        MruCache::Entry entry;
        entry.address = QLatin1String("localhost");
        entry.port = 8061;
        entry.username = QLatin1String("admin");

        mru.put(std::move(entry));
    }

    return mru;
}

void Settings::setMru(const MruCache& mru)
{
    settings_.remove(QLatin1String("MRU"));

    settings_.beginWriteArray(QLatin1String("MRU"));
    for (int i = 0; i < mru.size(); ++i)
    {
        settings_.setArrayIndex(i);

        const MruCache::Entry& entry = mru.at(i);

        settings_.setValue(QLatin1String("Address"), entry.address);
        settings_.setValue(QLatin1String("Port"), entry.port);
        settings_.setValue(QLatin1String("UserName"), entry.username);
        settings_.setValue(QLatin1String("KeyPath"), entry.key_path);
    }
    settings_.endArray();
}

} // namespace router
