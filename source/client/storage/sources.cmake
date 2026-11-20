#
# Aspia Project
# Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.
#

collect_sources(SOURCE_CLIENT_STORAGE
    auto_backup.cc
    auto_backup.h
    backup.cc
    backup.h
    credential_file.cc
    credential_file.h
    credential_store.cc
    credential_store.h
    database.cc
    database.h
    database_local_hosts.cc
    database_migration.cc
    database_migration.h
    database_router.cc
    database_settings.cc
    database_store.h
    instance_settings.cc
    instance_settings.h
    local_host_store.cc
    local_host_store.h
    master_password.cc
    master_password.h
    profile_store.cc
    profile_store.h
    router_host_store.cc
    router_host_store.h
    sync_store.cc
    sync_store.h
    system_settings.cc
    system_settings.h
    user_settings.cc
    user_settings.h)
