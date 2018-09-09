#
# Aspia Project
# Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

include_directories(
    ${PROJECT_SOURCE_DIR}
    ${ASPIA_THIRD_PARTY_DIR}/googletest/include
    ${ASPIA_THIRD_PARTY_DIR}/libvpx/include
    ${ASPIA_THIRD_PARTY_DIR}/libyuv/include
    ${ASPIA_THIRD_PARTY_DIR}/zlib-ng/include
    ${ASPIA_THIRD_PARTY_DIR}/openssl/include
    ${ASPIA_THIRD_PARTY_DIR}/protobuf/include)

link_directories(
    ${ASPIA_THIRD_PARTY_DIR}/googletest/lib
    ${ASPIA_THIRD_PARTY_DIR}/libvpx/lib
    ${ASPIA_THIRD_PARTY_DIR}/libyuv/lib
    ${ASPIA_THIRD_PARTY_DIR}/zlib-ng/lib
    ${ASPIA_THIRD_PARTY_DIR}/openssl/lib
    ${ASPIA_THIRD_PARTY_DIR}/protobuf/lib
    ${ASPIA_THIRD_PARTY_DIR}/qt/lib
    ${ASPIA_THIRD_PARTY_DIR}/qt/plugins/platforms
    ${ASPIA_THIRD_PARTY_DIR}/qt/plugins/styles)

list(APPEND SOURCE_BASE
    ${PROJECT_SOURCE_DIR}/base/aligned_memory.cc
    ${PROJECT_SOURCE_DIR}/base/aligned_memory.h
    ${PROJECT_SOURCE_DIR}/base/bitset.h
    ${PROJECT_SOURCE_DIR}/base/clipboard.cc
    ${PROJECT_SOURCE_DIR}/base/clipboard.h
    ${PROJECT_SOURCE_DIR}/base/const_buffer.h
    ${PROJECT_SOURCE_DIR}/base/cpuid.cc
    ${PROJECT_SOURCE_DIR}/base/cpuid.h
    ${PROJECT_SOURCE_DIR}/base/guid.cc
    ${PROJECT_SOURCE_DIR}/base/guid.h
    ${PROJECT_SOURCE_DIR}/base/keycode_converter.cc
    ${PROJECT_SOURCE_DIR}/base/keycode_converter.h
    ${PROJECT_SOURCE_DIR}/base/locale_loader.cc
    ${PROJECT_SOURCE_DIR}/base/locale_loader.h
    ${PROJECT_SOURCE_DIR}/base/logging.cc
    ${PROJECT_SOURCE_DIR}/base/logging.h
    ${PROJECT_SOURCE_DIR}/base/macros_magic.h
    ${PROJECT_SOURCE_DIR}/base/message_serialization.h
    ${PROJECT_SOURCE_DIR}/base/service.h
    ${PROJECT_SOURCE_DIR}/base/service_controller.cc
    ${PROJECT_SOURCE_DIR}/base/service_controller.h
    ${PROJECT_SOURCE_DIR}/base/service_impl.h
    ${PROJECT_SOURCE_DIR}/base/service_impl_win.cc
    ${PROJECT_SOURCE_DIR}/base/string_printf.cc
    ${PROJECT_SOURCE_DIR}/base/string_printf.h
    ${PROJECT_SOURCE_DIR}/base/string_util.cc
    ${PROJECT_SOURCE_DIR}/base/string_util.h
    ${PROJECT_SOURCE_DIR}/base/string_util_constants.cc
    ${PROJECT_SOURCE_DIR}/base/string_util_constants.h
    ${PROJECT_SOURCE_DIR}/base/typed_buffer.h
    ${PROJECT_SOURCE_DIR}/base/unicode.cc
    ${PROJECT_SOURCE_DIR}/base/unicode.h)

list(APPEND SOURCE_BASE_UNIT_TESTS
    ${PROJECT_SOURCE_DIR}/base/aligned_memory_unittest.cc
    ${PROJECT_SOURCE_DIR}/base/guid_unittest.cc
    ${PROJECT_SOURCE_DIR}/base/string_printf_unittest.cc)

list(APPEND SOURCE_BASE_WIN
    ${PROJECT_SOURCE_DIR}/base/win/registry.cc
    ${PROJECT_SOURCE_DIR}/base/win/registry.h
    ${PROJECT_SOURCE_DIR}/base/win/scoped_com_initializer.h
    ${PROJECT_SOURCE_DIR}/base/win/scoped_gdi_object.h
    ${PROJECT_SOURCE_DIR}/base/win/scoped_hdc.h
    ${PROJECT_SOURCE_DIR}/base/win/scoped_local.h
    ${PROJECT_SOURCE_DIR}/base/win/scoped_object.h
    ${PROJECT_SOURCE_DIR}/base/win/scoped_user_object.h
    ${PROJECT_SOURCE_DIR}/base/win/scoped_wts_memory.h
    ${PROJECT_SOURCE_DIR}/base/win/security_helpers.cc
    ${PROJECT_SOURCE_DIR}/base/win/security_helpers.h)

list(APPEND SOURCE_CLIENT
    ${PROJECT_SOURCE_DIR}/client/client.cc
    ${PROJECT_SOURCE_DIR}/client/client.h
    ${PROJECT_SOURCE_DIR}/client/client.qrc
    ${PROJECT_SOURCE_DIR}/client/client_connections.cc
    ${PROJECT_SOURCE_DIR}/client/client_connections.h
    ${PROJECT_SOURCE_DIR}/client/client_session.h
    ${PROJECT_SOURCE_DIR}/client/client_session_desktop_manage.cc
    ${PROJECT_SOURCE_DIR}/client/client_session_desktop_manage.h
    ${PROJECT_SOURCE_DIR}/client/client_session_desktop_view.cc
    ${PROJECT_SOURCE_DIR}/client/client_session_desktop_view.h
    ${PROJECT_SOURCE_DIR}/client/client_session_file_transfer.cc
    ${PROJECT_SOURCE_DIR}/client/client_session_file_transfer.h
    ${PROJECT_SOURCE_DIR}/client/client_session_system_info.cc
    ${PROJECT_SOURCE_DIR}/client/client_session_system_info.h
    ${PROJECT_SOURCE_DIR}/client/config_factory.cc
    ${PROJECT_SOURCE_DIR}/client/config_factory.h
    ${PROJECT_SOURCE_DIR}/client/connect_data.cc
    ${PROJECT_SOURCE_DIR}/client/connect_data.h
    ${PROJECT_SOURCE_DIR}/client/file_remove_queue_builder.cc
    ${PROJECT_SOURCE_DIR}/client/file_remove_queue_builder.h
    ${PROJECT_SOURCE_DIR}/client/file_remove_task.cc
    ${PROJECT_SOURCE_DIR}/client/file_remove_task.h
    ${PROJECT_SOURCE_DIR}/client/file_remover.cc
    ${PROJECT_SOURCE_DIR}/client/file_remover.h
    ${PROJECT_SOURCE_DIR}/client/file_status.cc
    ${PROJECT_SOURCE_DIR}/client/file_status.h
    ${PROJECT_SOURCE_DIR}/client/file_transfer.cc
    ${PROJECT_SOURCE_DIR}/client/file_transfer.h
    ${PROJECT_SOURCE_DIR}/client/file_transfer_queue_builder.cc
    ${PROJECT_SOURCE_DIR}/client/file_transfer_queue_builder.h
    ${PROJECT_SOURCE_DIR}/client/file_transfer_task.cc
    ${PROJECT_SOURCE_DIR}/client/file_transfer_task.h)

list(APPEND SOURCE_CLIENT_UI
    ${PROJECT_SOURCE_DIR}/client/ui/address_bar.cc
    ${PROJECT_SOURCE_DIR}/client/ui/address_bar.h
    ${PROJECT_SOURCE_DIR}/client/ui/address_bar_model.cc
    ${PROJECT_SOURCE_DIR}/client/ui/address_bar_model.h
    ${PROJECT_SOURCE_DIR}/client/ui/authorization_dialog.cc
    ${PROJECT_SOURCE_DIR}/client/ui/authorization_dialog.h
    ${PROJECT_SOURCE_DIR}/client/ui/authorization_dialog.ui
    ${PROJECT_SOURCE_DIR}/client/ui/category_group_tree_item.cc
    ${PROJECT_SOURCE_DIR}/client/ui/category_group_tree_item.h
    ${PROJECT_SOURCE_DIR}/client/ui/category_tree_item.cc
    ${PROJECT_SOURCE_DIR}/client/ui/category_tree_item.h
    ${PROJECT_SOURCE_DIR}/client/ui/client_dialog.cc
    ${PROJECT_SOURCE_DIR}/client/ui/client_dialog.h
    ${PROJECT_SOURCE_DIR}/client/ui/client_dialog.ui
    ${PROJECT_SOURCE_DIR}/client/ui/desktop_config_dialog.cc
    ${PROJECT_SOURCE_DIR}/client/ui/desktop_config_dialog.h
    ${PROJECT_SOURCE_DIR}/client/ui/desktop_config_dialog.ui
    ${PROJECT_SOURCE_DIR}/client/ui/file_name_validator.cc
    ${PROJECT_SOURCE_DIR}/client/ui/file_name_validator.h
    ${PROJECT_SOURCE_DIR}/client/ui/desktop_panel.cc
    ${PROJECT_SOURCE_DIR}/client/ui/desktop_panel.h
    ${PROJECT_SOURCE_DIR}/client/ui/desktop_panel.ui
    ${PROJECT_SOURCE_DIR}/client/ui/desktop_widget.cc
    ${PROJECT_SOURCE_DIR}/client/ui/desktop_widget.h
    ${PROJECT_SOURCE_DIR}/client/ui/desktop_window.cc
    ${PROJECT_SOURCE_DIR}/client/ui/desktop_window.h
    ${PROJECT_SOURCE_DIR}/client/ui/file_item_delegate.cc
    ${PROJECT_SOURCE_DIR}/client/ui/file_item_delegate.h
    ${PROJECT_SOURCE_DIR}/client/ui/file_list.cc
    ${PROJECT_SOURCE_DIR}/client/ui/file_list.h
    ${PROJECT_SOURCE_DIR}/client/ui/file_list_model.cc
    ${PROJECT_SOURCE_DIR}/client/ui/file_list_model.h
    ${PROJECT_SOURCE_DIR}/client/ui/file_manager_settings.cc
    ${PROJECT_SOURCE_DIR}/client/ui/file_manager_settings.h
    ${PROJECT_SOURCE_DIR}/client/ui/file_manager_window.cc
    ${PROJECT_SOURCE_DIR}/client/ui/file_manager_window.h
    ${PROJECT_SOURCE_DIR}/client/ui/file_manager_window.ui
    ${PROJECT_SOURCE_DIR}/client/ui/file_mime_data.cc
    ${PROJECT_SOURCE_DIR}/client/ui/file_mime_data.h
    ${PROJECT_SOURCE_DIR}/client/ui/file_path_validator.cc
    ${PROJECT_SOURCE_DIR}/client/ui/file_path_validator.h
    ${PROJECT_SOURCE_DIR}/client/ui/file_panel.cc
    ${PROJECT_SOURCE_DIR}/client/ui/file_panel.h
    ${PROJECT_SOURCE_DIR}/client/ui/file_panel.ui
    ${PROJECT_SOURCE_DIR}/client/ui/file_remove_dialog.cc
    ${PROJECT_SOURCE_DIR}/client/ui/file_remove_dialog.h
    ${PROJECT_SOURCE_DIR}/client/ui/file_remove_dialog.ui
    ${PROJECT_SOURCE_DIR}/client/ui/file_transfer_dialog.cc
    ${PROJECT_SOURCE_DIR}/client/ui/file_transfer_dialog.h
    ${PROJECT_SOURCE_DIR}/client/ui/file_transfer_dialog.ui
    ${PROJECT_SOURCE_DIR}/client/ui/key_sequence_dialog.cc
    ${PROJECT_SOURCE_DIR}/client/ui/key_sequence_dialog.h
    ${PROJECT_SOURCE_DIR}/client/ui/key_sequence_dialog.ui
    ${PROJECT_SOURCE_DIR}/client/ui/select_screen_action.h
    ${PROJECT_SOURCE_DIR}/client/ui/status_dialog.cc
    ${PROJECT_SOURCE_DIR}/client/ui/status_dialog.h
    ${PROJECT_SOURCE_DIR}/client/ui/status_dialog.ui
    ${PROJECT_SOURCE_DIR}/client/ui/system_info_window.cc
    ${PROJECT_SOURCE_DIR}/client/ui/system_info_window.h
    ${PROJECT_SOURCE_DIR}/client/ui/system_info_window.ui)

list(APPEND SOURCE_CODEC
    ${PROJECT_SOURCE_DIR}/codec/compressor.h
    ${PROJECT_SOURCE_DIR}/codec/compressor_zlib.cc
    ${PROJECT_SOURCE_DIR}/codec/compressor_zlib.h
    ${PROJECT_SOURCE_DIR}/codec/cursor_decoder.cc
    ${PROJECT_SOURCE_DIR}/codec/cursor_decoder.h
    ${PROJECT_SOURCE_DIR}/codec/cursor_encoder.cc
    ${PROJECT_SOURCE_DIR}/codec/cursor_encoder.h
    ${PROJECT_SOURCE_DIR}/codec/decompressor.h
    ${PROJECT_SOURCE_DIR}/codec/decompressor_zlib.cc
    ${PROJECT_SOURCE_DIR}/codec/decompressor_zlib.h
    ${PROJECT_SOURCE_DIR}/codec/pixel_translator.cc
    ${PROJECT_SOURCE_DIR}/codec/pixel_translator.h
    ${PROJECT_SOURCE_DIR}/codec/scale_reducer.cc
    ${PROJECT_SOURCE_DIR}/codec/scale_reducer.h
    ${PROJECT_SOURCE_DIR}/codec/scoped_vpx_codec.cc
    ${PROJECT_SOURCE_DIR}/codec/scoped_vpx_codec.h
    ${PROJECT_SOURCE_DIR}/codec/video_decoder.cc
    ${PROJECT_SOURCE_DIR}/codec/video_decoder.h
    ${PROJECT_SOURCE_DIR}/codec/video_decoder_vpx.cc
    ${PROJECT_SOURCE_DIR}/codec/video_decoder_vpx.h
    ${PROJECT_SOURCE_DIR}/codec/video_decoder_zlib.cc
    ${PROJECT_SOURCE_DIR}/codec/video_decoder_zlib.h
    ${PROJECT_SOURCE_DIR}/codec/video_encoder.cc
    ${PROJECT_SOURCE_DIR}/codec/video_encoder.h
    ${PROJECT_SOURCE_DIR}/codec/video_encoder_vpx.cc
    ${PROJECT_SOURCE_DIR}/codec/video_encoder_vpx.h
    ${PROJECT_SOURCE_DIR}/codec/video_encoder_zlib.cc
    ${PROJECT_SOURCE_DIR}/codec/video_encoder_zlib.h
    ${PROJECT_SOURCE_DIR}/codec/video_util.cc
    ${PROJECT_SOURCE_DIR}/codec/video_util.h)

list(APPEND SOURCE_CONSOLE
    ${PROJECT_SOURCE_DIR}/console/about_dialog.cc
    ${PROJECT_SOURCE_DIR}/console/about_dialog.h
    ${PROJECT_SOURCE_DIR}/console/about_dialog.ui
    ${PROJECT_SOURCE_DIR}/console/address_book_dialog.cc
    ${PROJECT_SOURCE_DIR}/console/address_book_dialog.h
    ${PROJECT_SOURCE_DIR}/console/address_book_dialog.ui
    ${PROJECT_SOURCE_DIR}/console/address_book_tab.cc
    ${PROJECT_SOURCE_DIR}/console/address_book_tab.h
    ${PROJECT_SOURCE_DIR}/console/address_book_tab.ui
    ${PROJECT_SOURCE_DIR}/console/computer_dialog.cc
    ${PROJECT_SOURCE_DIR}/console/computer_dialog.h
    ${PROJECT_SOURCE_DIR}/console/computer_dialog.ui
    ${PROJECT_SOURCE_DIR}/console/computer_drag.h
    ${PROJECT_SOURCE_DIR}/console/computer_group_dialog.cc
    ${PROJECT_SOURCE_DIR}/console/computer_group_dialog.h
    ${PROJECT_SOURCE_DIR}/console/computer_group_dialog.ui
    ${PROJECT_SOURCE_DIR}/console/computer_group_drag.h
    ${PROJECT_SOURCE_DIR}/console/computer_group_item.cc
    ${PROJECT_SOURCE_DIR}/console/computer_group_item.h
    ${PROJECT_SOURCE_DIR}/console/computer_group_mime_data.h
    ${PROJECT_SOURCE_DIR}/console/computer_group_tree.cc
    ${PROJECT_SOURCE_DIR}/console/computer_group_tree.h
    ${PROJECT_SOURCE_DIR}/console/computer_item.cc
    ${PROJECT_SOURCE_DIR}/console/computer_item.h
    ${PROJECT_SOURCE_DIR}/console/computer_mime_data.h
    ${PROJECT_SOURCE_DIR}/console/computer_tree.cc
    ${PROJECT_SOURCE_DIR}/console/computer_tree.h
    ${PROJECT_SOURCE_DIR}/console/console.qrc
    ${PROJECT_SOURCE_DIR}/console/console_main.cc
    ${PROJECT_SOURCE_DIR}/console/console_main.h
    ${PROJECT_SOURCE_DIR}/console/console_settings.cc
    ${PROJECT_SOURCE_DIR}/console/console_settings.h
    ${PROJECT_SOURCE_DIR}/console/console_statusbar.cc
    ${PROJECT_SOURCE_DIR}/console/console_statusbar.h
    ${PROJECT_SOURCE_DIR}/console/console_tab.cc
    ${PROJECT_SOURCE_DIR}/console/console_tab.h
    ${PROJECT_SOURCE_DIR}/console/console_window.cc
    ${PROJECT_SOURCE_DIR}/console/console_window.h
    ${PROJECT_SOURCE_DIR}/console/console_window.ui
    ${PROJECT_SOURCE_DIR}/console/open_address_book_dialog.cc
    ${PROJECT_SOURCE_DIR}/console/open_address_book_dialog.h
    ${PROJECT_SOURCE_DIR}/console/open_address_book_dialog.ui)

list(APPEND SOURCE_CRYPTO
    ${PROJECT_SOURCE_DIR}/crypto/big_num.cc
    ${PROJECT_SOURCE_DIR}/crypto/big_num.h
    ${PROJECT_SOURCE_DIR}/crypto/cryptor.h
    ${PROJECT_SOURCE_DIR}/crypto/cryptor_aes256_gcm.cc
    ${PROJECT_SOURCE_DIR}/crypto/cryptor_aes256_gcm.h
    ${PROJECT_SOURCE_DIR}/crypto/cryptor_chacha20_poly1305.cc
    ${PROJECT_SOURCE_DIR}/crypto/cryptor_chacha20_poly1305.h
    ${PROJECT_SOURCE_DIR}/crypto/data_cryptor.h
    ${PROJECT_SOURCE_DIR}/crypto/data_cryptor_chacha20_poly1305.cc
    ${PROJECT_SOURCE_DIR}/crypto/data_cryptor_chacha20_poly1305.h
    ${PROJECT_SOURCE_DIR}/crypto/generic_hash.cc
    ${PROJECT_SOURCE_DIR}/crypto/generic_hash.h
    ${PROJECT_SOURCE_DIR}/crypto/openssl_util.cc
    ${PROJECT_SOURCE_DIR}/crypto/openssl_util.h
    ${PROJECT_SOURCE_DIR}/crypto/password_hash.cc
    ${PROJECT_SOURCE_DIR}/crypto/password_hash.h
    ${PROJECT_SOURCE_DIR}/crypto/random.cc
    ${PROJECT_SOURCE_DIR}/crypto/random.h
    ${PROJECT_SOURCE_DIR}/crypto/scoped_crypto_initializer.cc
    ${PROJECT_SOURCE_DIR}/crypto/scoped_crypto_initializer.h
    ${PROJECT_SOURCE_DIR}/crypto/secure_memory.cc
    ${PROJECT_SOURCE_DIR}/crypto/secure_memory.h
    ${PROJECT_SOURCE_DIR}/crypto/srp_constants.cc
    ${PROJECT_SOURCE_DIR}/crypto/srp_constants.h
    ${PROJECT_SOURCE_DIR}/crypto/srp_math.cc
    ${PROJECT_SOURCE_DIR}/crypto/srp_math.h)

list(APPEND SOURCE_CRYPTO_UNIT_TESTS
    ${PROJECT_SOURCE_DIR}/crypto/srp_math_unittest.cc)

list(APPEND SOURCE_DESKTOP_CAPTURE
    ${PROJECT_SOURCE_DIR}/desktop_capture/capture_scheduler.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/capture_scheduler.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/cursor_capturer.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/cursor_capturer_win.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/cursor_capturer_win.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_frame.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_frame.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_frame_aligned.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_frame_aligned.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_frame_dib.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_frame_dib.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_frame_qimage.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_frame_qimage.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_geometry.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_geometry.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_region.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_region.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/diff_block_avx2.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/diff_block_avx2.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/diff_block_c.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/diff_block_c.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/diff_block_sse2.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/diff_block_sse2.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/diff_block_sse3.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/diff_block_sse3.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/differ.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/differ.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/mouse_cursor.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/mouse_cursor.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/mouse_cursor_cache.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/mouse_cursor_cache.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/pixel_format.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/pixel_format.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/screen_capture_frame_queue.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/screen_capturer.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/screen_capturer_gdi.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/screen_capturer_gdi.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/screen_settings_tracker.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/screen_settings_tracker.h)

list(APPEND SOURCE_DESKTOP_CAPTURE_UNIT_TESTS
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_geometry_unittest.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_region_unittest.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/diff_block_avx2_unittest.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/diff_block_c_unittest.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/diff_block_sse2_unittest.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/diff_block_sse3_unittest.cc)

list(APPEND SOURCE_DESKTOP_CAPTURE_WIN
    ${PROJECT_SOURCE_DIR}/desktop_capture/win/cursor.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/win/cursor.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/win/desktop.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/win/desktop.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/win/scoped_thread_desktop.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/win/scoped_thread_desktop.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/win/screen_capture_utils.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/win/screen_capture_utils.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/win/visual_effects_disabler.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/win/visual_effects_disabler.h)

list(APPEND SOURCE_HOST
    ${PROJECT_SOURCE_DIR}/host/desktop_config_tracker.cc
    ${PROJECT_SOURCE_DIR}/host/desktop_config_tracker.h
    ${PROJECT_SOURCE_DIR}/host/file_depacketizer.cc
    ${PROJECT_SOURCE_DIR}/host/file_depacketizer.h
    ${PROJECT_SOURCE_DIR}/host/file_packetizer.cc
    ${PROJECT_SOURCE_DIR}/host/file_packetizer.h
    ${PROJECT_SOURCE_DIR}/host/file_platform_util_win.cc
    ${PROJECT_SOURCE_DIR}/host/file_platform_util.h
    ${PROJECT_SOURCE_DIR}/host/file_request.cc
    ${PROJECT_SOURCE_DIR}/host/file_request.h
    ${PROJECT_SOURCE_DIR}/host/file_worker.cc
    ${PROJECT_SOURCE_DIR}/host/file_worker.h
    ${PROJECT_SOURCE_DIR}/host/host.qrc
    ${PROJECT_SOURCE_DIR}/host/host_config_main.cc
    ${PROJECT_SOURCE_DIR}/host/host_config_main.h
    ${PROJECT_SOURCE_DIR}/host/host_notifier.cc
    ${PROJECT_SOURCE_DIR}/host/host_notifier.h
    ${PROJECT_SOURCE_DIR}/host/host_server.cc
    ${PROJECT_SOURCE_DIR}/host/host_server.h
    ${PROJECT_SOURCE_DIR}/host/host_session.cc
    ${PROJECT_SOURCE_DIR}/host/host_session.h
    ${PROJECT_SOURCE_DIR}/host/host_session_desktop.cc
    ${PROJECT_SOURCE_DIR}/host/host_session_desktop.h
    ${PROJECT_SOURCE_DIR}/host/host_session_fake.cc
    ${PROJECT_SOURCE_DIR}/host/host_session_fake.h
    ${PROJECT_SOURCE_DIR}/host/host_session_fake_desktop.cc
    ${PROJECT_SOURCE_DIR}/host/host_session_fake_desktop.h
    ${PROJECT_SOURCE_DIR}/host/host_session_fake_file_transfer.cc
    ${PROJECT_SOURCE_DIR}/host/host_session_fake_file_transfer.h
    ${PROJECT_SOURCE_DIR}/host/host_session_file_transfer.cc
    ${PROJECT_SOURCE_DIR}/host/host_session_file_transfer.h
    ${PROJECT_SOURCE_DIR}/host/host_session_system_info.cc
    ${PROJECT_SOURCE_DIR}/host/host_session_system_info.h
    ${PROJECT_SOURCE_DIR}/host/host_settings.cc
    ${PROJECT_SOURCE_DIR}/host/host_settings.h
    ${PROJECT_SOURCE_DIR}/host/input_injector.cc
    ${PROJECT_SOURCE_DIR}/host/input_injector.h
    ${PROJECT_SOURCE_DIR}/host/screen_updater.cc
    ${PROJECT_SOURCE_DIR}/host/screen_updater.h
    ${PROJECT_SOURCE_DIR}/host/system_info_request.cc
    ${PROJECT_SOURCE_DIR}/host/system_info_request.h
    ${PROJECT_SOURCE_DIR}/host/user_util.cc
    ${PROJECT_SOURCE_DIR}/host/user_util.h)

list(APPEND SOURCE_HOST_UI
    ${PROJECT_SOURCE_DIR}/host/ui/host_config_dialog.cc
    ${PROJECT_SOURCE_DIR}/host/ui/host_config_dialog.h
    ${PROJECT_SOURCE_DIR}/host/ui/host_config_dialog.ui
    ${PROJECT_SOURCE_DIR}/host/ui/host_notifier_window.cc
    ${PROJECT_SOURCE_DIR}/host/ui/host_notifier_window.h
    ${PROJECT_SOURCE_DIR}/host/ui/host_notifier_window.ui
    ${PROJECT_SOURCE_DIR}/host/ui/user_dialog.cc
    ${PROJECT_SOURCE_DIR}/host/ui/user_dialog.h
    ${PROJECT_SOURCE_DIR}/host/ui/user_dialog.ui
    ${PROJECT_SOURCE_DIR}/host/ui/user_tree_item.cc
    ${PROJECT_SOURCE_DIR}/host/ui/user_tree_item.h)

list(APPEND SOURCE_HOST_WIN
    ${PROJECT_SOURCE_DIR}/host/win/file_enumerator.cc
    ${PROJECT_SOURCE_DIR}/host/win/file_enumerator.h
    ${PROJECT_SOURCE_DIR}/host/win/host.cc
    ${PROJECT_SOURCE_DIR}/host/win/host.h
    ${PROJECT_SOURCE_DIR}/host/win/host_main.cc
    ${PROJECT_SOURCE_DIR}/host/win/host_main.h
    ${PROJECT_SOURCE_DIR}/host/win/host_process.cc
    ${PROJECT_SOURCE_DIR}/host/win/host_process.h
    ${PROJECT_SOURCE_DIR}/host/win/host_process_impl.cc
    ${PROJECT_SOURCE_DIR}/host/win/host_process_impl.h
    ${PROJECT_SOURCE_DIR}/host/win/host_service.cc
    ${PROJECT_SOURCE_DIR}/host/win/host_service.h
    ${PROJECT_SOURCE_DIR}/host/win/host_service_constants.cc
    ${PROJECT_SOURCE_DIR}/host/win/host_service_constants.h
    ${PROJECT_SOURCE_DIR}/host/win/host_service_main.cc
    ${PROJECT_SOURCE_DIR}/host/win/host_service_main.h)

list(APPEND SOURCE_IPC
    ${PROJECT_SOURCE_DIR}/ipc/ipc_channel.cc
    ${PROJECT_SOURCE_DIR}/ipc/ipc_channel.h
    ${PROJECT_SOURCE_DIR}/ipc/ipc_server.cc
    ${PROJECT_SOURCE_DIR}/ipc/ipc_server.h)

list(APPEND SOURCE_NETWORK
    ${PROJECT_SOURCE_DIR}/network/firewall_manager.cc
    ${PROJECT_SOURCE_DIR}/network/firewall_manager.h
    ${PROJECT_SOURCE_DIR}/network/network_channel.cc
    ${PROJECT_SOURCE_DIR}/network/network_channel.h
    ${PROJECT_SOURCE_DIR}/network/network_channel_client.cc
    ${PROJECT_SOURCE_DIR}/network/network_channel_client.h
    ${PROJECT_SOURCE_DIR}/network/network_channel_host.cc
    ${PROJECT_SOURCE_DIR}/network/network_channel_host.h
    ${PROJECT_SOURCE_DIR}/network/network_server.cc
    ${PROJECT_SOURCE_DIR}/network/network_server.h
    ${PROJECT_SOURCE_DIR}/network/srp_client_context.cc
    ${PROJECT_SOURCE_DIR}/network/srp_client_context.h
    ${PROJECT_SOURCE_DIR}/network/srp_host_context.cc
    ${PROJECT_SOURCE_DIR}/network/srp_host_context.h)

list(APPEND SOURCE_PROTOCOL
    ${PROJECT_SOURCE_DIR}/protocol/address_book.proto
    ${PROJECT_SOURCE_DIR}/protocol/desktop_session.proto
    ${PROJECT_SOURCE_DIR}/protocol/file_transfer_session.proto
    ${PROJECT_SOURCE_DIR}/protocol/key_exchange.proto
    ${PROJECT_SOURCE_DIR}/protocol/notifier.proto
    ${PROJECT_SOURCE_DIR}/protocol/session_type.proto
    ${PROJECT_SOURCE_DIR}/protocol/srp_user.proto
    ${PROJECT_SOURCE_DIR}/protocol/system_info_session.proto)

list(APPEND SOURCE_SYSTEM_INFO
    ${PROJECT_SOURCE_DIR}/system_info/category.cc
    ${PROJECT_SOURCE_DIR}/system_info/category.h)

list(APPEND SOURCE_SYSTEM_INFO_PARSER
    ${PROJECT_SOURCE_DIR}/system_info/parser/dmi_parser.cc
    ${PROJECT_SOURCE_DIR}/system_info/parser/dmi_parser.h
    ${PROJECT_SOURCE_DIR}/system_info/parser/parser.h)

list(APPEND SOURCE_SYSTEM_INFO_PROTOCOL
    ${PROJECT_SOURCE_DIR}/system_info/protocol/dmi.proto)

list(APPEND SOURCE_SYSTEM_INFO_SERIALIZER
    ${PROJECT_SOURCE_DIR}/system_info/serializer/dmi_impl.cc
    ${PROJECT_SOURCE_DIR}/system_info/serializer/dmi_impl.h
    ${PROJECT_SOURCE_DIR}/system_info/serializer/dmi_serializer.cc
    ${PROJECT_SOURCE_DIR}/system_info/serializer/dmi_serializer.h
    ${PROJECT_SOURCE_DIR}/system_info/serializer/serializer.cc
    ${PROJECT_SOURCE_DIR}/system_info/serializer/serializer.h)

list(APPEND SOURCE_SYSTEM_INFO_UI
    ${PROJECT_SOURCE_DIR}/system_info/ui/dmi_form.cc
    ${PROJECT_SOURCE_DIR}/system_info/ui/dmi_form.h
    ${PROJECT_SOURCE_DIR}/system_info/ui/dmi_form.ui
    ${PROJECT_SOURCE_DIR}/system_info/ui/form.h)

list(APPEND SOURCE
    ${PROJECT_SOURCE_DIR}/build_config.cc
    ${PROJECT_SOURCE_DIR}/build_config.h
    ${PROJECT_SOURCE_DIR}/core_export.h
    ${PROJECT_SOURCE_DIR}/version.h
    ${PROJECT_SOURCE_DIR}/aspia_core.rc)

list(APPEND ALL_SOURCES
    ${SOURCE_BASE}
    ${SOURCE_BASE_WIN}
    ${SOURCE_CLIENT}
    ${SOURCE_CLIENT_UI}
    ${SOURCE_CODEC}
    ${SOURCE_CONSOLE}
    ${SOURCE_CRYPTO}
    ${SOURCE_DESKTOP_CAPTURE}
    ${SOURCE_DESKTOP_CAPTURE_WIN}
    ${SOURCE_HOST}
    ${SOURCE_HOST_UI}
    ${SOURCE_HOST_WIN}
    ${SOURCE_IPC}
    ${SOURCE_NETWORK}
    ${SOURCE_PROTOCOL}
    ${SOURCE_SYSTEM_INFO}
    ${SOURCE_SYSTEM_INFO_PARSER}
    ${SOURCE_SYSTEM_INFO_PROTOCOL}
    ${SOURCE_SYSTEM_INFO_SERIALIZER}
    ${SOURCE_SYSTEM_INFO_UI}
    ${SOURCE})

list(APPEND ALL_SOURCES_WITH_UNIT_TESTS
    ${SOURCE_BASE_UNIT_TESTS}
    ${SOURCE_CRYPTO_UNIT_TESTS}
    ${SOURCE_DESKTOP_CAPTURE_UNIT_TESTS}
    ${ALL_SOURCES})

set(THIRD_PARTY_LIBS
    Qt5::Core
    Qt5::Gui
    Qt5::Network
    Qt5::Widgets
    Qt5::WinMain
    Qt5::WinExtras
    Qt5::Xml
    debug Qt5AccessibilitySupportd
    debug Qt5EventDispatcherSupportd
    debug Qt5FontDatabaseSupportd
    debug Qt5ThemeSupportd
    debug Qt5WindowsUIAutomationSupportd
    debug libprotobuf-lited
    debug libvpxd
    debug libyuvd
    debug qtfreetyped
    debug qtharfbuzzd
    debug qtlibpngd
    debug qtpcre2d
    debug qwindowsd
    debug qwindowsvistastyled
    debug zlib-ngd
    optimized Qt5AccessibilitySupport
    optimized Qt5EventDispatcherSupport
    optimized Qt5FontDatabaseSupport
    optimized Qt5ThemeSupport
    optimized Qt5WindowsUIAutomationSupport
    optimized libprotobuf-lite
    optimized libvpx
    optimized libyuv
    optimized qtfreetype
    optimized qtharfbuzz
    optimized qtlibpng
    optimized qtpcre2
    optimized qwindows
    optimized qwindowsvistastyle
    optimized zlib-ng
    crypt32
    dwmapi
    imm32
    iphlpapi
    libcrypto
    mpr
    netapi32
    sas
    shlwapi
    userenv
    uxtheme
    version
    winmm
    ws2_32
    wtsapi32)
