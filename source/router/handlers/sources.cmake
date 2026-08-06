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

collect_sources(SOURCE_ROUTER_HANDLERS
    connection_request_handler.cc
    connection_request_handler.h
    group_request_handler.cc
    group_request_handler.h
    host_id_handler.cc
    host_id_handler.h
    host_request_handler.cc
    host_request_handler.h
    request_caller.h
    request_result.h
    two_factor_handler.cc
    two_factor_handler.h
    user_request_handler.cc
    user_request_handler.h
    workspace_request_handler.cc
    workspace_request_handler.h)

collect_sources(SOURCE_ROUTER_HANDLERS_TESTS
    connection_request_handler_unittest.cc
    group_request_handler_unittest.cc
    host_id_handler_unittest.cc
    host_request_handler_unittest.cc
    two_factor_handler_unittest.cc
    user_request_handler_unittest.cc
    workspace_request_handler_unittest.cc)
