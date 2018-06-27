#
# PROJECT:         Aspia
# FILE:            aspia_core.cmake
# LICENSE:         GNU General Public License 3
# PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
#

include_directories(
    ${PROJECT_SOURCE_DIR}
    ${ASPIA_THIRD_PARTY_DIR}/libvpx/include
    ${ASPIA_THIRD_PARTY_DIR}/libyuv/include
    ${ASPIA_THIRD_PARTY_DIR}/zlib-ng/include
    ${ASPIA_THIRD_PARTY_DIR}/protobuf/include
    ${ASPIA_THIRD_PARTY_DIR}/libsodium/include)

link_directories(
    ${ASPIA_THIRD_PARTY_DIR}/libvpx/lib
    ${ASPIA_THIRD_PARTY_DIR}/libyuv/lib
    ${ASPIA_THIRD_PARTY_DIR}/zlib-ng/lib
    ${ASPIA_THIRD_PARTY_DIR}/protobuf/lib
    ${ASPIA_THIRD_PARTY_DIR}/qt/lib
    ${ASPIA_THIRD_PARTY_DIR}/qt/plugins/platforms
    ${ASPIA_THIRD_PARTY_DIR}/qt/plugins/styles
    ${ASPIA_THIRD_PARTY_DIR}/libsodium/lib)

list(APPEND SOURCE_BASE
    ${PROJECT_SOURCE_DIR}/base/aligned_memory.cc
    ${PROJECT_SOURCE_DIR}/base/aligned_memory.h
    ${PROJECT_SOURCE_DIR}/base/clipboard.cc
    ${PROJECT_SOURCE_DIR}/base/clipboard.h
    ${PROJECT_SOURCE_DIR}/base/errno_logging.cc
    ${PROJECT_SOURCE_DIR}/base/errno_logging.h
    ${PROJECT_SOURCE_DIR}/base/file_logger.cc
    ${PROJECT_SOURCE_DIR}/base/file_logger.h
    ${PROJECT_SOURCE_DIR}/base/keycode_converter.cc
    ${PROJECT_SOURCE_DIR}/base/keycode_converter.h
    ${PROJECT_SOURCE_DIR}/base/locale_loader.cc
    ${PROJECT_SOURCE_DIR}/base/locale_loader.h
    ${PROJECT_SOURCE_DIR}/base/message_serialization.h
    ${PROJECT_SOURCE_DIR}/base/service.h
    ${PROJECT_SOURCE_DIR}/base/service_controller.cc
    ${PROJECT_SOURCE_DIR}/base/service_controller.h
    ${PROJECT_SOURCE_DIR}/base/service_impl.h
    ${PROJECT_SOURCE_DIR}/base/service_impl_win.cc
    ${PROJECT_SOURCE_DIR}/base/typed_buffer.h)

list(APPEND SOURCE_BASE_WIN
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
    ${PROJECT_SOURCE_DIR}/client/client_session.h
    ${PROJECT_SOURCE_DIR}/client/client_session_desktop_manage.cc
    ${PROJECT_SOURCE_DIR}/client/client_session_desktop_manage.h
    ${PROJECT_SOURCE_DIR}/client/client_session_desktop_view.cc
    ${PROJECT_SOURCE_DIR}/client/client_session_desktop_view.h
    ${PROJECT_SOURCE_DIR}/client/client_session_file_transfer.cc
    ${PROJECT_SOURCE_DIR}/client/client_session_file_transfer.h
    ${PROJECT_SOURCE_DIR}/client/client_user_authorizer.cc
    ${PROJECT_SOURCE_DIR}/client/client_user_authorizer.h
    ${PROJECT_SOURCE_DIR}/client/computer_factory.cc
    ${PROJECT_SOURCE_DIR}/client/computer_factory.h
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
    ${PROJECT_SOURCE_DIR}/client/ui/file_item.cc
    ${PROJECT_SOURCE_DIR}/client/ui/file_item.h
    ${PROJECT_SOURCE_DIR}/client/ui/file_item_delegate.h
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
    ${PROJECT_SOURCE_DIR}/crypto/data_encryptor.cc
    ${PROJECT_SOURCE_DIR}/crypto/data_encryptor.h
    ${PROJECT_SOURCE_DIR}/crypto/encryptor.cc
    ${PROJECT_SOURCE_DIR}/crypto/encryptor.h
	${PROJECT_SOURCE_DIR}/crypto/random.cc
    ${PROJECT_SOURCE_DIR}/crypto/random.h
	${PROJECT_SOURCE_DIR}/crypto/secure_memory.cc
    ${PROJECT_SOURCE_DIR}/crypto/secure_memory.h)

list(APPEND SOURCE_DESKTOP_CAPTURE
    ${PROJECT_SOURCE_DIR}/desktop_capture/capture_scheduler.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/capture_scheduler.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/capturer.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/capturer_gdi.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/capturer_gdi.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_frame.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_frame.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_frame_aligned.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_frame_aligned.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_frame_dib.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_frame_dib.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_frame_qimage.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/desktop_frame_qimage.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/diff_block_avx2.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/diff_block_avx2.h
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
    ${PROJECT_SOURCE_DIR}/desktop_capture/pixel_format.h)

list(APPEND SOURCE_DESKTOP_CAPTURE_WIN
    ${PROJECT_SOURCE_DIR}/desktop_capture/win/desktop.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/win/desktop.h
    ${PROJECT_SOURCE_DIR}/desktop_capture/win/scoped_thread_desktop.cc
    ${PROJECT_SOURCE_DIR}/desktop_capture/win/scoped_thread_desktop.h)

list(APPEND SOURCE_HOST
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
    ${PROJECT_SOURCE_DIR}/host/host_config_main.cc
    ${PROJECT_SOURCE_DIR}/host/host_config_main.h
    ${PROJECT_SOURCE_DIR}/host/host_notifier.cc
    ${PROJECT_SOURCE_DIR}/host/host_notifier.h
    ${PROJECT_SOURCE_DIR}/host/host_notifier_main.cc
    ${PROJECT_SOURCE_DIR}/host/host_notifier_main.h
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
    ${PROJECT_SOURCE_DIR}/host/host_settings.cc
    ${PROJECT_SOURCE_DIR}/host/host_settings.h
    ${PROJECT_SOURCE_DIR}/host/host_user_authorizer.cc
    ${PROJECT_SOURCE_DIR}/host/host_user_authorizer.h
    ${PROJECT_SOURCE_DIR}/host/input_injector.cc
    ${PROJECT_SOURCE_DIR}/host/input_injector.h
    ${PROJECT_SOURCE_DIR}/host/screen_updater.cc
    ${PROJECT_SOURCE_DIR}/host/screen_updater.h
    ${PROJECT_SOURCE_DIR}/host/user.cc
    ${PROJECT_SOURCE_DIR}/host/user.h)

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
    ${PROJECT_SOURCE_DIR}/network/network_server.cc
    ${PROJECT_SOURCE_DIR}/network/network_server.h)

list(APPEND SOURCE_PROTOCOL
    ${PROJECT_SOURCE_DIR}/protocol/address_book.pb.cc
    ${PROJECT_SOURCE_DIR}/protocol/address_book.pb.h
    ${PROJECT_SOURCE_DIR}/protocol/address_book.proto
    ${PROJECT_SOURCE_DIR}/protocol/authorization.pb.cc
    ${PROJECT_SOURCE_DIR}/protocol/authorization.pb.h
    ${PROJECT_SOURCE_DIR}/protocol/authorization.proto
    ${PROJECT_SOURCE_DIR}/protocol/desktop_session.pb.cc
    ${PROJECT_SOURCE_DIR}/protocol/desktop_session.pb.h
    ${PROJECT_SOURCE_DIR}/protocol/desktop_session.proto
    ${PROJECT_SOURCE_DIR}/protocol/file_transfer_session.pb.cc
    ${PROJECT_SOURCE_DIR}/protocol/file_transfer_session.pb.h
    ${PROJECT_SOURCE_DIR}/protocol/file_transfer_session.proto
    ${PROJECT_SOURCE_DIR}/protocol/key_exchange.pb.cc
    ${PROJECT_SOURCE_DIR}/protocol/key_exchange.pb.h
    ${PROJECT_SOURCE_DIR}/protocol/key_exchange.proto
    ${PROJECT_SOURCE_DIR}/protocol/notifier.pb.cc
    ${PROJECT_SOURCE_DIR}/protocol/notifier.pb.h
    ${PROJECT_SOURCE_DIR}/protocol/notifier.proto)

list(APPEND SOURCE_RESOURCES
    ${PROJECT_SOURCE_DIR}/resources/resources.qrc)

list(APPEND SOURCE
    ${PROJECT_SOURCE_DIR}/build_config.cc
    ${PROJECT_SOURCE_DIR}/build_config.h
    ${PROJECT_SOURCE_DIR}/core_export.h
    ${PROJECT_SOURCE_DIR}/version.h
    ${PROJECT_SOURCE_DIR}/aspia_core.rc)
