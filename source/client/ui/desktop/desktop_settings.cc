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

#include "client/ui/desktop/desktop_settings.h"

#include "base/xml_settings.h"

#include <QStandardPaths>

namespace client {

namespace {

const QString kScaleParam = QStringLiteral("Desktop/Scale");
const QString kAutoScrollingParam = QStringLiteral("Desktop/AutoScrolling");
const QString kSendKeyCombinationsParam = QStringLiteral("Desktop/SendKeyCombinations");
const QString kRecordingPathParam = QStringLiteral("Desktop/RecordingPath");
const QString kRecordSessionsParam = QStringLiteral("Desktop/RecordSessions");
const QString kToolBarPinnedParam = QStringLiteral("Desktop/ToolBarPinned");
const QString kPauseVideoParam = QStringLiteral("Desktop/PauseVideo");
const QString kPauseAudioParam = QStringLiteral("Desktop/PauseAudio");
const QString kWaitForHostParam = QStringLiteral("Desktop/WaitForHost");

} // namespace

//--------------------------------------------------------------------------------------------------
DesktopSettings::DesktopSettings()
    : settings_(base::XmlSettings::format(),
                QSettings::UserScope,
                QStringLiteral("aspia"),
                QStringLiteral("client"))
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
int DesktopSettings::scale() const
{
    int result = settings_.value(kScaleParam, -1).toInt();

    switch (result)
    {
        case 100:
        case 90:
        case 80:
        case 70:
        case 60:
        case 50:
        case -1:
            return result;

        default:
            return 100;
    }
}

//--------------------------------------------------------------------------------------------------
void DesktopSettings::setScale(int value)
{
    settings_.setValue(kScaleParam, value);
}

//--------------------------------------------------------------------------------------------------
bool DesktopSettings::autoScrolling() const
{
    return settings_.value(kAutoScrollingParam, true).toBool();
}

//--------------------------------------------------------------------------------------------------
void DesktopSettings::setAutoScrolling(bool enable)
{
    settings_.setValue(kAutoScrollingParam, enable);
}

//--------------------------------------------------------------------------------------------------
bool DesktopSettings::sendKeyCombinations() const
{
    return settings_.value(kSendKeyCombinationsParam, true).toBool();
}

//--------------------------------------------------------------------------------------------------
void DesktopSettings::setSendKeyCombinations(bool enable)
{
    settings_.setValue(kSendKeyCombinationsParam, enable);
}

//--------------------------------------------------------------------------------------------------
QString DesktopSettings::recordingPath() const
{
    QString default_path = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation) +
        QStringLiteral("/Aspia");
    return settings_.value(kRecordingPathParam, default_path).toString();
}

//--------------------------------------------------------------------------------------------------
void DesktopSettings::setRecordingPath(const QString& path)
{
    settings_.setValue(kRecordingPathParam, path);
}

//--------------------------------------------------------------------------------------------------
bool DesktopSettings::recordSessions() const
{
    return settings_.value(kRecordSessionsParam, false).toBool();
}

//--------------------------------------------------------------------------------------------------
void DesktopSettings::setRecordSessions(bool enable)
{
    settings_.setValue(kRecordSessionsParam, enable);
}

//--------------------------------------------------------------------------------------------------
bool DesktopSettings::isToolBarPinned() const
{
    return settings_.value(kToolBarPinnedParam, false).toBool();
}

//--------------------------------------------------------------------------------------------------
void DesktopSettings::setToolBarPinned(bool enable)
{
    settings_.setValue(kToolBarPinnedParam, enable);
}

//--------------------------------------------------------------------------------------------------
bool DesktopSettings::pauseVideoWhenMinimizing() const
{
    return settings_.value(kPauseVideoParam, true).toBool();
}

//--------------------------------------------------------------------------------------------------
void DesktopSettings::setPauseVideoWhenMinimizing(bool enable)
{
    settings_.setValue(kPauseVideoParam, enable);
}

//--------------------------------------------------------------------------------------------------
bool DesktopSettings::pauseAudioWhenMinimizing() const
{
    return settings_.value(kPauseAudioParam, true).toBool();
}

//--------------------------------------------------------------------------------------------------
void DesktopSettings::setPauseAudioWhenMinimizing(bool enable)
{
    settings_.setValue(kPauseAudioParam, enable);
}

//--------------------------------------------------------------------------------------------------
bool DesktopSettings::waitForHost() const
{
    return settings_.value(kWaitForHostParam, true).toBool();
}

//--------------------------------------------------------------------------------------------------
void DesktopSettings::setWaitForHost(bool enable)
{
    settings_.setValue(kWaitForHostParam, enable);
}

} // namespace client
