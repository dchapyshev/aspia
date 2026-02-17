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

#ifndef CLIENT_UI_DESKTOP_DESKTOP_SETTINGS_H
#define CLIENT_UI_DESKTOP_DESKTOP_SETTINGS_H

#include <QSettings>

namespace client {

class DesktopSettings
{
public:
    DesktopSettings();

    int scale() const;
    void setScale(int value);

    bool autoScrolling() const;
    void setAutoScrolling(bool enable);

    bool sendKeyCombinations() const;
    void setSendKeyCombinations(bool enable);

    QString recordingPath() const;
    void setRecordingPath(const QString& path);

    bool recordSessions() const;
    void setRecordSessions(bool enable);

    bool isToolBarPinned() const;
    void setToolBarPinned(bool enable);

    bool pauseVideoWhenMinimizing() const;
    void setPauseVideoWhenMinimizing(bool enable);

    bool pauseAudioWhenMinimizing() const;
    void setPauseAudioWhenMinimizing(bool enable);

    bool waitForHost() const;
    void setWaitForHost(bool enable);

private:
    QSettings settings_;
    Q_DISABLE_COPY(DesktopSettings)
};

} // namespace client

#endif // CLIENT_UI_DESKTOP_DESKTOP_SETTINGS_H
