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

collect_sources(SOURCE_ROUTER_STORAGE
    access_log.cc
    access_log.h
    database.cc
    database.h
    database_store.h
    device_token_store.cc
    device_token_store.h
    email_queue.cc
    email_queue.h
    group_store.cc
    group_store.h
    host_store.cc
    host_store.h
    sync_storage.cc
    sync_storage.h
    user_store.cc
    user_store.h
    workspace.cc
    workspace.h
    workspace_store.cc
    workspace_store.h)

collect_sources(SOURCE_ROUTER_STORAGE_TESTS
    access_log_unittest.cc
    database_unittest.cc
    device_token_store_unittest.cc
    email_queue_unittest.cc
    host_store_unittest.cc
    sync_storage_unittest.cc
    user_store_unittest.cc
    workspace_store_unittest.cc)
