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

#ifndef HOST_DESKTOP_ENVIRONMENT_LINUX_H
#define HOST_DESKTOP_ENVIRONMENT_LINUX_H

#include "host/desktop_environment.h"

#include <QString>

#include <sys/types.h>

class DesktopEnvironmentLinux final : public DesktopEnvironment
{
public:
    explicit DesktopEnvironmentLinux(QObject* parent = nullptr);
    ~DesktopEnvironmentLinux() final;

protected:
    void disableWallpaper() final;
    void disableEffects() final;
    void revertAll() final;

private:
    enum class Desktop { OTHER, GNOME, KDE };

    // Fills the target of the active graphical session (uid, group, home, user name, desktop).
    // Returns false if there is no usable active session, in which case nothing must be changed.
    bool resolveTarget();

    // Detects the desktop by the shell process (gnome-shell / plasmashell) owned by |uid|.
    static Desktop detectDesktop(uid_t uid);

    // Runs a command as the session user on the user's session bus. Optionally captures stdout.
    // Returns true only if the command exited successfully.
    bool runAsUser(const QString& program, const QStringList& arguments, QString* output);

    // The base class issues disable/revert back-to-back when the client changes several settings at
    // once; applying each immediately makes the desktop flicker. Instead the requests only record the
    // desired state and applyDesired() reconciles it once per event-loop turn (and synchronously on
    // teardown).
    void scheduleApply();
    void applyDesired();

    Desktop desktop_ = Desktop::OTHER;
    uid_t uid_ = 0;
    gid_t gid_ = 0;
    QString user_name_;
    QString home_dir_;

    bool apply_scheduled_ = false;
    bool desired_wallpaper_disabled_ = false;
    bool desired_effects_disabled_ = false;
    bool applied_wallpaper_disabled_ = false;
    bool applied_effects_disabled_ = false;

    // Originals captured on the disable transition, so revert is always possible.
    QString saved_picture_options_;
    QString saved_animations_;

    Q_DISABLE_COPY(DesktopEnvironmentLinux)
};

#endif // HOST_DESKTOP_ENVIRONMENT_LINUX_H
