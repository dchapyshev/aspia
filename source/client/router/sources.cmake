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

collect_sources(SOURCE_CLIENT_ROUTER
    router_2fa.cc
    router_2fa.h
    router_cache.cc
    router_cache.h
    router_controller.cc
    router_controller.h
    router_hosts_cleaner.cc
    router_hosts_cleaner.h
    router_rpc.h
    router_session.cc
    router_session.h
    router_types.h)
