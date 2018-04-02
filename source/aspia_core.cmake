
include_directories(
    ${PROJECT_SOURCE_DIR}
    ${ASPIA_THIRD_PARTY_DIR}/libvpx/include
    ${ASPIA_THIRD_PARTY_DIR}/libyuv/include
    ${ASPIA_THIRD_PARTY_DIR}/zlib-ng/include
    ${ASPIA_THIRD_PARTY_DIR}/protobuf/include
    ${ASPIA_THIRD_PARTY_DIR}/libsodium/include
    ${ASPIA_THIRD_PARTY_DIR}/asio/include
    ${ASPIA_THIRD_PARTY_DIR}/rapidjson/include
    ${ASPIA_THIRD_PARTY_DIR}/rapidxml/include)

link_directories(
    ${ASPIA_THIRD_PARTY_DIR}/libvpx/lib
    ${ASPIA_THIRD_PARTY_DIR}/libyuv/lib
    ${ASPIA_THIRD_PARTY_DIR}/zlib-ng/lib
    ${ASPIA_THIRD_PARTY_DIR}/protobuf/lib
    ${ASPIA_THIRD_PARTY_DIR}/qt/lib
    ${ASPIA_THIRD_PARTY_DIR}/qt/plugins/platforms
    ${ASPIA_THIRD_PARTY_DIR}/libsodium/lib)

list(APPEND SOURCE_BASE
    ${PROJECT_SOURCE_DIR}/base/aligned_memory.cc
    ${PROJECT_SOURCE_DIR}/base/aligned_memory.h
    ${PROJECT_SOURCE_DIR}/base/command_line.cc
    ${PROJECT_SOURCE_DIR}/base/command_line.h
    ${PROJECT_SOURCE_DIR}/base/file_logger.cc
    ${PROJECT_SOURCE_DIR}/base/file_logger.h
    ${PROJECT_SOURCE_DIR}/base/keycode_converter.cc
    ${PROJECT_SOURCE_DIR}/base/keycode_converter.h
    ${PROJECT_SOURCE_DIR}/base/message_serialization.h
    ${PROJECT_SOURCE_DIR}/base/message_window.cc
    ${PROJECT_SOURCE_DIR}/base/message_window.h
    ${PROJECT_SOURCE_DIR}/base/object_watcher.cc
    ${PROJECT_SOURCE_DIR}/base/object_watcher.h
    ${PROJECT_SOURCE_DIR}/base/scoped_native_library.h
    ${PROJECT_SOURCE_DIR}/base/system_error_code.cc
    ${PROJECT_SOURCE_DIR}/base/system_error_code.h
    ${PROJECT_SOURCE_DIR}/base/typed_buffer.h
    ${PROJECT_SOURCE_DIR}/base/waitable_timer.cc
    ${PROJECT_SOURCE_DIR}/base/waitable_timer.h)

list(APPEND SOURCE_BASE_MESSAGE_LOOP
    ${PROJECT_SOURCE_DIR}/base/message_loop/message_loop.cc
    ${PROJECT_SOURCE_DIR}/base/message_loop/message_loop.h
    ${PROJECT_SOURCE_DIR}/base/message_loop/message_loop_proxy.cc
    ${PROJECT_SOURCE_DIR}/base/message_loop/message_loop_proxy.h
    ${PROJECT_SOURCE_DIR}/base/message_loop/message_pump.h
    ${PROJECT_SOURCE_DIR}/base/message_loop/message_pump_default.cc
    ${PROJECT_SOURCE_DIR}/base/message_loop/message_pump_default.h
    ${PROJECT_SOURCE_DIR}/base/message_loop/message_pump_dispatcher.h
    ${PROJECT_SOURCE_DIR}/base/message_loop/message_pump_ui.cc
    ${PROJECT_SOURCE_DIR}/base/message_loop/message_pump_ui.h
    ${PROJECT_SOURCE_DIR}/base/message_loop/pending_task.cc
    ${PROJECT_SOURCE_DIR}/base/message_loop/pending_task.h)

list(APPEND SOURCE_BASE_PROCESS
    ${PROJECT_SOURCE_DIR}/base/process/process.cc
    ${PROJECT_SOURCE_DIR}/base/process/process.h
    ${PROJECT_SOURCE_DIR}/base/process/process_watcher.cc
    ${PROJECT_SOURCE_DIR}/base/process/process_watcher.h)

list(APPEND SOURCE_BASE_STRINGS
    ${PROJECT_SOURCE_DIR}/base/strings/string_printf.cc
    ${PROJECT_SOURCE_DIR}/base/strings/string_printf.h
    ${PROJECT_SOURCE_DIR}/base/strings/string_util.cc
    ${PROJECT_SOURCE_DIR}/base/strings/string_util.h
    ${PROJECT_SOURCE_DIR}/base/strings/string_util_constants.cc
    ${PROJECT_SOURCE_DIR}/base/strings/string_util_constants.h
    ${PROJECT_SOURCE_DIR}/base/strings/unicode.cc
    ${PROJECT_SOURCE_DIR}/base/strings/unicode.h)

list(APPEND SOURCE_BASE_THREADING
    ${PROJECT_SOURCE_DIR}/base/threading/simple_thread.cc
    ${PROJECT_SOURCE_DIR}/base/threading/simple_thread.h
    ${PROJECT_SOURCE_DIR}/base/threading/thread.cc
    ${PROJECT_SOURCE_DIR}/base/threading/thread.h)

list(APPEND SOURCE_BASE_WIN
    ${PROJECT_SOURCE_DIR}/base/win/scoped_clipboard.cc
    ${PROJECT_SOURCE_DIR}/base/win/scoped_clipboard.h
    ${PROJECT_SOURCE_DIR}/base/win/scoped_com_initializer.h
    ${PROJECT_SOURCE_DIR}/base/win/scoped_gdi_object.h
    ${PROJECT_SOURCE_DIR}/base/win/scoped_hdc.h
    ${PROJECT_SOURCE_DIR}/base/win/scoped_hglobal.h
    ${PROJECT_SOURCE_DIR}/base/win/scoped_local.h
    ${PROJECT_SOURCE_DIR}/base/win/scoped_object.h
    ${PROJECT_SOURCE_DIR}/base/win/scoped_privilege.cc
    ${PROJECT_SOURCE_DIR}/base/win/scoped_privilege.h
    ${PROJECT_SOURCE_DIR}/base/win/scoped_user_object.h
    ${PROJECT_SOURCE_DIR}/base/win/scoped_wts_memory.h
    ${PROJECT_SOURCE_DIR}/base/win/security_helpers.cc
    ${PROJECT_SOURCE_DIR}/base/win/security_helpers.h
    ${PROJECT_SOURCE_DIR}/base/win/service.cc
    ${PROJECT_SOURCE_DIR}/base/win/service.h
    ${PROJECT_SOURCE_DIR}/base/win/service_manager.cc
    ${PROJECT_SOURCE_DIR}/base/win/service_manager.h)

list(APPEND SOURCE_CLIENT
    ${PROJECT_SOURCE_DIR}/client/client.cc
    ${PROJECT_SOURCE_DIR}/client/client.h
    ${PROJECT_SOURCE_DIR}/client/client_main.cc
    ${PROJECT_SOURCE_DIR}/client/client_main.h
    ${PROJECT_SOURCE_DIR}/client/client_session.h
    ${PROJECT_SOURCE_DIR}/client/client_session_desktop_manage.cc
    ${PROJECT_SOURCE_DIR}/client/client_session_desktop_manage.h
    ${PROJECT_SOURCE_DIR}/client/client_session_desktop_view.cc
    ${PROJECT_SOURCE_DIR}/client/client_session_desktop_view.h
    ${PROJECT_SOURCE_DIR}/client/client_session_file_transfer.cc
    ${PROJECT_SOURCE_DIR}/client/client_session_file_transfer.h
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
    ${PROJECT_SOURCE_DIR}/client/ui/authorization_dialog.cc
    ${PROJECT_SOURCE_DIR}/client/ui/authorization_dialog.h
    ${PROJECT_SOURCE_DIR}/client/ui/authorization_dialog.ui
    ${PROJECT_SOURCE_DIR}/client/ui/client_dialog.cc
    ${PROJECT_SOURCE_DIR}/client/ui/client_dialog.h
    ${PROJECT_SOURCE_DIR}/client/ui/client_dialog.ui
    ${PROJECT_SOURCE_DIR}/client/ui/desktop_config_dialog.cc
    ${PROJECT_SOURCE_DIR}/client/ui/desktop_config_dialog.h
    ${PROJECT_SOURCE_DIR}/client/ui/desktop_config_dialog.ui
    ${PROJECT_SOURCE_DIR}/client/ui/desktop_panel.cc
    ${PROJECT_SOURCE_DIR}/client/ui/desktop_panel.h
    ${PROJECT_SOURCE_DIR}/client/ui/desktop_panel.ui
    ${PROJECT_SOURCE_DIR}/client/ui/desktop_widget.cc
    ${PROJECT_SOURCE_DIR}/client/ui/desktop_widget.h
    ${PROJECT_SOURCE_DIR}/client/ui/desktop_window.cc
    ${PROJECT_SOURCE_DIR}/client/ui/desktop_window.h
    ${PROJECT_SOURCE_DIR}/client/ui/desktop_window.ui
    ${PROJECT_SOURCE_DIR}/client/ui/file_item.cc
    ${PROJECT_SOURCE_DIR}/client/ui/file_item.h
    ${PROJECT_SOURCE_DIR}/client/ui/file_item_drag.cc
    ${PROJECT_SOURCE_DIR}/client/ui/file_item_drag.h
    ${PROJECT_SOURCE_DIR}/client/ui/file_item_mime_data.cc
    ${PROJECT_SOURCE_DIR}/client/ui/file_item_mime_data.h
    ${PROJECT_SOURCE_DIR}/client/ui/file_manager_window.cc
    ${PROJECT_SOURCE_DIR}/client/ui/file_manager_window.h
    ${PROJECT_SOURCE_DIR}/client/ui/file_manager_window.ui
    ${PROJECT_SOURCE_DIR}/client/ui/file_panel.cc
    ${PROJECT_SOURCE_DIR}/client/ui/file_panel.h
    ${PROJECT_SOURCE_DIR}/client/ui/file_panel.ui
    ${PROJECT_SOURCE_DIR}/client/ui/file_remove_dialog.cc
    ${PROJECT_SOURCE_DIR}/client/ui/file_remove_dialog.h
    ${PROJECT_SOURCE_DIR}/client/ui/file_remove_dialog.ui
    ${PROJECT_SOURCE_DIR}/client/ui/file_transfer_dialog.cc
    ${PROJECT_SOURCE_DIR}/client/ui/file_transfer_dialog.h
    ${PROJECT_SOURCE_DIR}/client/ui/file_transfer_dialog.ui
    ${PROJECT_SOURCE_DIR}/client/ui/file_tree_widget.cc
    ${PROJECT_SOURCE_DIR}/client/ui/file_tree_widget.h
    ${PROJECT_SOURCE_DIR}/client/ui/key_sequence_dialog.cc
    ${PROJECT_SOURCE_DIR}/client/ui/key_sequence_dialog.h
    ${PROJECT_SOURCE_DIR}/client/ui/key_sequence_dialog.ui
    ${PROJECT_SOURCE_DIR}/client/ui/status_dialog.cc
    ${PROJECT_SOURCE_DIR}/client/ui/status_dialog.h
    ${PROJECT_SOURCE_DIR}/client/ui/status_dialog.ui)

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
    ${PROJECT_SOURCE_DIR}/codec/scoped_vpx_codec.cc
    ${PROJECT_SOURCE_DIR}/codec/scoped_vpx_codec.h
    ${PROJECT_SOURCE_DIR}/codec/video_decoder.cc
    ${PROJECT_SOURCE_DIR}/codec/video_decoder.h
    ${PROJECT_SOURCE_DIR}/codec/video_decoder_vpx.cc
    ${PROJECT_SOURCE_DIR}/codec/video_decoder_vpx.h
    ${PROJECT_SOURCE_DIR}/codec/video_decoder_zlib.cc
    ${PROJECT_SOURCE_DIR}/codec/video_decoder_zlib.h
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
    ${PROJECT_SOURCE_DIR}/console/address_book.cc
    ${PROJECT_SOURCE_DIR}/console/address_book.h
    ${PROJECT_SOURCE_DIR}/console/address_book_dialog.cc
    ${PROJECT_SOURCE_DIR}/console/address_book_dialog.h
    ${PROJECT_SOURCE_DIR}/console/address_book_dialog.ui
    ${PROJECT_SOURCE_DIR}/console/computer.cc
    ${PROJECT_SOURCE_DIR}/console/computer.h
    ${PROJECT_SOURCE_DIR}/console/computer_dialog.cc
    ${PROJECT_SOURCE_DIR}/console/computer_dialog.h
    ${PROJECT_SOURCE_DIR}/console/computer_dialog.ui
    ${PROJECT_SOURCE_DIR}/console/computer_group.cc
    ${PROJECT_SOURCE_DIR}/console/computer_group.h
    ${PROJECT_SOURCE_DIR}/console/computer_group_dialog.cc
    ${PROJECT_SOURCE_DIR}/console/computer_group_dialog.h
    ${PROJECT_SOURCE_DIR}/console/computer_group_dialog.ui
    ${PROJECT_SOURCE_DIR}/console/computer_group_drag.h
    ${PROJECT_SOURCE_DIR}/console/computer_group_mime_data.h
    ${PROJECT_SOURCE_DIR}/console/computer_group_tree.cc
    ${PROJECT_SOURCE_DIR}/console/computer_group_tree.h
    ${PROJECT_SOURCE_DIR}/console/computer_tree.cc
    ${PROJECT_SOURCE_DIR}/console/computer_tree.h
    ${PROJECT_SOURCE_DIR}/console/console_main.cc
    ${PROJECT_SOURCE_DIR}/console/console_main.h
    ${PROJECT_SOURCE_DIR}/console/console_window.cc
    ${PROJECT_SOURCE_DIR}/console/console_window.h
    ${PROJECT_SOURCE_DIR}/console/console_window.ui
    ${PROJECT_SOURCE_DIR}/console/open_address_book_dialog.cc
    ${PROJECT_SOURCE_DIR}/console/open_address_book_dialog.h
    ${PROJECT_SOURCE_DIR}/console/open_address_book_dialog.ui)

list(APPEND SOURCE_CRYPTO
    ${PROJECT_SOURCE_DIR}/crypto/random.cc
    ${PROJECT_SOURCE_DIR}/crypto/random.h
    ${PROJECT_SOURCE_DIR}/crypto/string_encryptor.cc
    ${PROJECT_SOURCE_DIR}/crypto/string_encryptor.h)

list(APPEND SOURCE_DESKTOP_CAPTURE
    ${PROJECT_SOURCE_DIR}/desktop_capture/capture_scheduler.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/capture_scheduler.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/capturer.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/capturer_gdi.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/capturer_gdi.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_frame.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_frame.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_frame_aligned.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_frame_aligned.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_frame_dib.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_frame_dib.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_frame_qimage.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_frame_qimage.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/diff_block_sse2.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/diff_block_sse2.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/differ.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/differ.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/mouse_cursor.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/mouse_cursor.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/mouse_cursor_cache.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/mouse_cursor_cache.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/pixel_format.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/pixel_format.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/scoped_thread_desktop.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/scoped_thread_desktop.h)

list(APPEND SOURCE_HOST
    ${PROJECT_SOURCE_DIR}/host/clipboard.cc
    ${PROJECT_SOURCE_DIR}/host/clipboard.h
    ${PROJECT_SOURCE_DIR}/host/clipboard_thread.cc
    ${PROJECT_SOURCE_DIR}/host/clipboard_thread.h
    ${PROJECT_SOURCE_DIR}/host/console_session_watcher.cc
    ${PROJECT_SOURCE_DIR}/host/console_session_watcher.h
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
    ${PROJECT_SOURCE_DIR}/host/host.cc
    ${PROJECT_SOURCE_DIR}/host/host.h
    ${PROJECT_SOURCE_DIR}/host/host_config_main.cc
    ${PROJECT_SOURCE_DIR}/host/host_config_main.h
    ${PROJECT_SOURCE_DIR}/host/host_main.cc
    ${PROJECT_SOURCE_DIR}/host/host_main.h
    ${PROJECT_SOURCE_DIR}/host/host_pool.cc
    ${PROJECT_SOURCE_DIR}/host/host_pool.h
    ${PROJECT_SOURCE_DIR}/host/host_process_connector.cc
    ${PROJECT_SOURCE_DIR}/host/host_process_connector.h
    ${PROJECT_SOURCE_DIR}/host/host_service.cc
    ${PROJECT_SOURCE_DIR}/host/host_service.h
    ${PROJECT_SOURCE_DIR}/host/host_service_main.cc
    ${PROJECT_SOURCE_DIR}/host/host_service_main.h
    ${PROJECT_SOURCE_DIR}/host/host_session.cc
    ${PROJECT_SOURCE_DIR}/host/host_session.h
    ${PROJECT_SOURCE_DIR}/host/host_session_desktop.cc
    ${PROJECT_SOURCE_DIR}/host/host_session_desktop.h
    ${PROJECT_SOURCE_DIR}/host/host_session_file_transfer.cc
    ${PROJECT_SOURCE_DIR}/host/host_session_file_transfer.h
    ${PROJECT_SOURCE_DIR}/host/host_session_launcher.cc
    ${PROJECT_SOURCE_DIR}/host/host_session_launcher.h
    ${PROJECT_SOURCE_DIR}/host/host_switches.cc
    ${PROJECT_SOURCE_DIR}/host/host_switches.h
    ${PROJECT_SOURCE_DIR}/host/input_injector.cc
    ${PROJECT_SOURCE_DIR}/host/input_injector.h
    ${PROJECT_SOURCE_DIR}/host/screen_updater.cc
    ${PROJECT_SOURCE_DIR}/host/screen_updater.h
    ${PROJECT_SOURCE_DIR}/host/user.cc
    ${PROJECT_SOURCE_DIR}/host/user.h
    ${PROJECT_SOURCE_DIR}/host/user_list.cc
    ${PROJECT_SOURCE_DIR}/host/user_list.h)

list(APPEND SOURCE_HOST_UI
    ${PROJECT_SOURCE_DIR}/host/ui/user_dialog.cc
    ${PROJECT_SOURCE_DIR}/host/ui/user_dialog.h
    ${PROJECT_SOURCE_DIR}/host/ui/user_dialog.ui
    ${PROJECT_SOURCE_DIR}/host/ui/user_list_dialog.cc
    ${PROJECT_SOURCE_DIR}/host/ui/user_list_dialog.h
    ${PROJECT_SOURCE_DIR}/host/ui/user_list_dialog.ui
    ${PROJECT_SOURCE_DIR}/host/ui/user_tree_item.cc
    ${PROJECT_SOURCE_DIR}/host/ui/user_tree_item.h)

list(APPEND SOURCE_IPC
    ${PROJECT_SOURCE_DIR}/ipc/pipe_channel.cc
    ${PROJECT_SOURCE_DIR}/ipc/pipe_channel.h
    ${PROJECT_SOURCE_DIR}/ipc/pipe_channel_proxy.cc
    ${PROJECT_SOURCE_DIR}/ipc/pipe_channel_proxy.h)

list(APPEND SOURCE_NETWORK
    ${PROJECT_SOURCE_DIR}/network/channel.cc
    ${PROJECT_SOURCE_DIR}/network/channel.h
    ${PROJECT_SOURCE_DIR}/network/firewall_manager.cc
    ${PROJECT_SOURCE_DIR}/network/firewall_manager.h
    ${PROJECT_SOURCE_DIR}/network/network_channel.cc
    ${PROJECT_SOURCE_DIR}/network/network_channel.h
    ${PROJECT_SOURCE_DIR}/network/network_channel_proxy.cc
    ${PROJECT_SOURCE_DIR}/network/network_channel_proxy.h
    ${PROJECT_SOURCE_DIR}/network/network_channel_tcp.cc
    ${PROJECT_SOURCE_DIR}/network/network_channel_tcp.h
    ${PROJECT_SOURCE_DIR}/network/network_server_tcp.cc
    ${PROJECT_SOURCE_DIR}/network/network_server_tcp.h)

list(APPEND SOURCE_PROTOCOL
    ${PROJECT_SOURCE_DIR}/protocol/address_book.pb.cc
    ${PROJECT_SOURCE_DIR}/protocol/address_book.pb.h
    ${PROJECT_SOURCE_DIR}/protocol/address_book.proto
    ${PROJECT_SOURCE_DIR}/protocol/authorization.pb.cc
    ${PROJECT_SOURCE_DIR}/protocol/authorization.pb.h
    ${PROJECT_SOURCE_DIR}/protocol/authorization.proto
    ${PROJECT_SOURCE_DIR}/protocol/computer.pb.cc
    ${PROJECT_SOURCE_DIR}/protocol/computer.pb.h
    ${PROJECT_SOURCE_DIR}/protocol/computer.proto
    ${PROJECT_SOURCE_DIR}/protocol/desktop_session.pb.cc
    ${PROJECT_SOURCE_DIR}/protocol/desktop_session.pb.h
    ${PROJECT_SOURCE_DIR}/protocol/desktop_session.proto
    ${PROJECT_SOURCE_DIR}/protocol/file_transfer_session.pb.cc
    ${PROJECT_SOURCE_DIR}/protocol/file_transfer_session.pb.h
    ${PROJECT_SOURCE_DIR}/protocol/file_transfer_session.proto)

list(APPEND SOURCE_RESOURCES
    ${PROJECT_SOURCE_DIR}/resources/resources.qrc)

list(APPEND SOURCE
    ${PROJECT_SOURCE_DIR}/build_config.cc
    ${PROJECT_SOURCE_DIR}/build_config.h
    ${PROJECT_SOURCE_DIR}/export.h
    ${PROJECT_SOURCE_DIR}/version.h
    ${PROJECT_SOURCE_DIR}/aspia_core.rc)
