//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/android/profile_combo_box.h"

#include "base/gui_application.h"
#include "base/logging.h"
#include "client/router/router_controller.h"
#include "client/storage/database.h"

//--------------------------------------------------------------------------------------------------
ProfileComboBox::ProfileComboBox(QWidget* parent)
    : ComboBox(parent)
{
    connect(&RouterController::instance(), &RouterController::sig_profilesChanged,
            this, &ProfileComboBox::onReload);
}

//--------------------------------------------------------------------------------------------------
ProfileComboBox::~ProfileComboBox() = default;

//--------------------------------------------------------------------------------------------------
qint64 ProfileComboBox::currentProfileId() const
{
    return currentIndex() >= 0 ? currentData().toLongLong() : 0;
}

//--------------------------------------------------------------------------------------------------
void ProfileComboBox::setCurrentProfileId(qint64 profile_id)
{
    const int index = findData(QVariant::fromValue(profile_id));
    setCurrentIndex(index >= 0 ? index : 0);
}

//--------------------------------------------------------------------------------------------------
bool ProfileComboBox::onReload()
{
    QList<ProfileConfig> profiles;
    const Database::ReadResult result = Database::instance().profileList(&profiles);
    if (result == Database::ReadResult::FAILED)
    {
        LOG(ERROR) << "Unable to read the list of profiles";
        return false;
    }

    if (result == Database::ReadResult::INCOMPLETE)
        LOG(ERROR) << "Unable to read some of the profiles";

    const qint64 profile_id = currentProfileId();
    const QIcon icon = GuiApplication::svgIcon(":/img/dashboard-layout.svg");

    clear();

    for (const ProfileConfig& profile : std::as_const(profiles))
    {
        addItem(icon, profile.isDefault() ? tr("Default") : profile.name(),
                QVariant::fromValue<qint64>(profile.isDefault() ? 0 : profile.id()));
    }

    setCurrentProfileId(profile_id);
    return true;
}
