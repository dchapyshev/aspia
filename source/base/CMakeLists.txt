#
# Aspia Project
# Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

list(APPEND SOURCE_BASE
    application.h
    applications_reader.h
    bitset.h
    compiler_specific.h
    cpuid_util.cc
    cpuid_util.h
    debug.cc
    debug.h
    edid.cc
    edid.h
    endian_util.cc
    endian_util.h
    gui_application.cc
    gui_application.h
    license_reader.h
    location.cc
    location.h
    logging.cc
    logging.h
    macros_magic.h
    meta_types.cc
    meta_types.h
    power_controller.h
    process_handle.cc
    process_handle.h
    scoped_clear_last_error.cc
    scoped_clear_last_error.h
    serialization.cc
    serialization.h
    service.h
    session_id.cc
    session_id.h
    smbios.h
    smbios_parser.cc
    smbios_parser.h
    smbios_reader.h
    stl_util.h
    sys_info.cc
    sys_info.h
    system_error.cc
    system_error.h
    translations.cc
    translations.h
    version_constants.cc
    version_constants.h
    waitable_event.cc
    waitable_event.h
    xml_settings.cc
    xml_settings.h)

if (WIN32)
    list(APPEND SOURCE_BASE
        applications_reader_win.cc
        license_reader_win.cc
        power_controller_win.cc
        smbios_reader_win.cc
        service_win.cc
        sys_info_win.cc)
endif()

if (LINUX)
    list(APPEND SOURCE_BASE
        applications_reader_linux.cc
        license_reader_linux.cc
        power_controller_linux.cc
        smbios_reader_linux.cc
        service_posix.cc
        sys_info_linux.cc)
endif()

if (APPLE)
    list(APPEND SOURCE_BASE
        applications_reader_mac.cc
        license_reader_mac.cc
        power_controller_mac.mm
        smbios_reader_mac.mm
        service_posix.cc
        sys_info_mac.mm)
endif()

list(APPEND SOURCE_BASE_TESTS
    bitset_unittest.cc
    scoped_clear_last_error_unittest.cc
    stl_util_unittest.cc
    tests_main.cc)

list(APPEND SOURCE_BASE_AUDIO
    audio/audio_capturer.cc
    audio/audio_capturer.h
    audio/audio_capturer_wrapper.cc
    audio/audio_capturer_wrapper.h
    audio/audio_output.cc
    audio/audio_output.h
    audio/audio_player.cc
    audio/audio_player.h
    audio/audio_silence_detector.cc
    audio/audio_silence_detector.h
    audio/audio_volume_filter.cc
    audio/audio_volume_filter.h)

if (WIN32)
    list(APPEND SOURCE_BASE_AUDIO
        audio/audio_capturer_win.cc
        audio/audio_capturer_win.h
        audio/audio_output_win.cc
        audio/audio_output_win.h
        audio/audio_volume_filter_win.cc
        audio/audio_volume_filter_win.h)

    list(APPEND SOURCE_BASE_AUDIO_WIN
        audio/win/audio_util_win.cc
        audio/win/audio_util_win.h
        audio/win/default_audio_device_change_detector.cc
        audio/win/default_audio_device_change_detector.h
        audio/win/scoped_mmcss_registration.cc
        audio/win/scoped_mmcss_registration.h)
endif()

if (LINUX)
    list(APPEND SOURCE_BASE_AUDIO
        audio/audio_capturer_linux.cc
        audio/audio_capturer_linux.h
        audio/audio_output_pulse.cc
        audio/audio_output_pulse.h)

    list(APPEND SOURCE_BASE_AUDIO_LINUX
        audio/linux/audio_pipe_reader.cc
        audio/linux/audio_pipe_reader.h
        audio/linux/late_binding_symbol_table.cc
        audio/linux/late_binding_symbol_table.h
        audio/linux/pulseaudio_symbol_table.cc
        audio/linux/pulseaudio_symbol_table.h)
endif()

if (APPLE)
    list(APPEND SOURCE_BASE_AUDIO
        audio/audio_output_mac.cc
        audio/audio_output_mac.h)
endif()

list(APPEND SOURCE_BASE_CODEC
    codec/audio_bus.cc
    codec/audio_bus.h
    codec/audio_decoder.cc
    codec/audio_decoder.h
    codec/audio_decoder_opus.cc
    codec/audio_decoder_opus.h
    codec/audio_encoder.h
    codec/audio_encoder_opus.cc
    codec/audio_encoder_opus.h
    codec/audio_sample_types.h
    codec/cursor_decoder.cc
    codec/cursor_decoder.h
    codec/cursor_encoder.cc
    codec/cursor_encoder.h
    codec/multi_channel_resampler.cc
    codec/multi_channel_resampler.h
    codec/pixel_translator.cc
    codec/pixel_translator.h
    codec/scale_reducer.cc
    codec/scale_reducer.h
    codec/scoped_vpx_codec.cc
    codec/scoped_vpx_codec.h
    codec/scoped_zstd_stream.cc
    codec/scoped_zstd_stream.h
    codec/sinc_resampler.cc
    codec/sinc_resampler.h
    codec/vector_math.cc
    codec/vector_math.h
    codec/video_decoder.cc
    codec/video_decoder.h
    codec/video_decoder_vpx.cc
    codec/video_decoder_vpx.h
    codec/video_decoder_zstd.cc
    codec/video_decoder_zstd.h
    codec/video_encoder.cc
    codec/video_encoder.h
    codec/video_encoder_vpx.cc
    codec/video_encoder_vpx.h
    codec/video_encoder_zstd.cc
    codec/video_encoder_zstd.h
    codec/webm_file_muxer.cc
    codec/webm_file_muxer.h
    codec/webm_file_writer.cc
    codec/webm_file_writer.h
    codec/webm_video_encoder.cc
    codec/webm_video_encoder.h
    codec/zstd_compress.cc
    codec/zstd_compress.h)

list(APPEND SOURCE_BASE_CRYPTO
    crypto/big_num.cc
    crypto/big_num.h
    crypto/data_cryptor.h
    crypto/data_cryptor_chacha20_poly1305.cc
    crypto/data_cryptor_chacha20_poly1305.h
    crypto/data_cryptor_fake.cc
    crypto/data_cryptor_fake.h
    crypto/generic_hash.cc
    crypto/generic_hash.h
    crypto/key_pair.cc
    crypto/key_pair.h
    crypto/large_number_increment.cc
    crypto/large_number_increment.h
    crypto/message_decryptor.h
    crypto/message_decryptor_fake.cc
    crypto/message_decryptor_fake.h
    crypto/message_decryptor_openssl.cc
    crypto/message_decryptor_openssl.h
    crypto/message_encryptor.h
    crypto/message_encryptor_fake.cc
    crypto/message_encryptor_fake.h
    crypto/message_encryptor_openssl.cc
    crypto/message_encryptor_openssl.h
    crypto/openssl_util.cc
    crypto/openssl_util.h
    crypto/os_crypt.h
    crypto/password_generator.cc
    crypto/password_generator.h
    crypto/password_hash.cc
    crypto/password_hash.h
    crypto/random.cc
    crypto/random.h
    crypto/scoped_crypto_initializer.cc
    crypto/scoped_crypto_initializer.h
    crypto/secure_memory.cc
    crypto/secure_memory.h
    crypto/srp_constants.cc
    crypto/srp_constants.h
    crypto/srp_math.cc
    crypto/srp_math.h)

if (WIN32)
    list(APPEND SOURCE_BASE_CRYPTO crypto/os_crypt_win.cc)
endif()

if (APPLE)
    list(APPEND SOURCE_BASE_CRYPTO crypto/os_crypt_mac.mm)
endif()

if (LINUX)
    list(APPEND SOURCE_BASE_CRYPTO crypto/os_crypt_linux.cc)
endif()

list(APPEND SOURCE_BASE_CRYPTO_TESTS
    crypto/big_num_unittest.cc
    crypto/cryptor_unittest.cc
    crypto/data_cryptor_unittest.cc
    crypto/generic_hash_unittest.cc
    crypto/key_pair_unittest.cc
    crypto/large_number_increment_unittest.cc
    crypto/password_generator_unittest.cc
    crypto/password_hash_unittest.cc
    crypto/srp_math_unittest.cc)

list(APPEND SOURCE_BASE_DESKTOP
    desktop/capture_scheduler.cc
    desktop/capture_scheduler.h
    desktop/desktop_environment.cc
    desktop/desktop_environment.h
    desktop/desktop_resizer.cc
    desktop/desktop_resizer.h
    desktop/diff_block_32bpp_c.cc
    desktop/diff_block_32bpp_c.h
    desktop/diff_block_32bpp_sse2.cc
    desktop/diff_block_32bpp_sse2.h
    desktop/differ.cc
    desktop/differ.h
    desktop/frame.cc
    desktop/frame.h
    desktop/frame_aligned.cc
    desktop/frame_aligned.h
    desktop/frame_qimage.cc
    desktop/frame_qimage.h
    desktop/frame_rotation.cc
    desktop/frame_rotation.h
    desktop/frame_simple.cc
    desktop/frame_simple.h
    desktop/geometry.cc
    desktop/geometry.h
    desktop/mouse_cursor.cc
    desktop/mouse_cursor.h
    desktop/pixel_format.cc
    desktop/pixel_format.h
    desktop/power_save_blocker.cc
    desktop/power_save_blocker.h
    desktop/region.cc
    desktop/region.h
    desktop/screen_capturer.cc
    desktop/screen_capturer.h
    desktop/screen_capturer_helper.cc
    desktop/screen_capturer_helper.h
    desktop/screen_capturer_wrapper.cc
    desktop/screen_capturer_wrapper.h
    desktop/shared_frame.cc
    desktop/shared_frame.h
    desktop/shared_memory_frame.cc
    desktop/shared_memory_frame.h)

if (WIN32)
    list(APPEND SOURCE_BASE_DESKTOP
        desktop/desktop_environment_win.cc
        desktop/desktop_environment_win.h
        desktop/desktop_resizer_win.cc
        desktop/desktop_resizer_win.h
        desktop/frame_dib.cc
        desktop/frame_dib.h
        desktop/screen_capturer_dxgi.cc
        desktop/screen_capturer_dxgi.h
        desktop/screen_capturer_gdi.cc
        desktop/screen_capturer_gdi.h
        desktop/screen_capturer_mirror.cc
        desktop/screen_capturer_mirror.h
        desktop/screen_capturer_win.cc
        desktop/screen_capturer_win.h)
endif()

if (LINUX)
    list(APPEND SOURCE_BASE_DESKTOP
        desktop/desktop_environment_linux.cc
        desktop/desktop_environment_linux.h
        desktop/desktop_resizer_x11.cc
        desktop/desktop_resizer_x11.h
        desktop/screen_capturer_x11.cc
        desktop/screen_capturer_x11.h)
endif()

if (APPLE)
    list(APPEND SOURCE_BASE_DESKTOP
        desktop/desktop_environment_mac.mm
        desktop/desktop_environment_mac.h
        desktop/screen_capturer_mac.mm
        desktop/screen_capturer_mac.h)
endif()

list(APPEND SOURCE_BASE_DESKTOP_TESTS
    desktop/diff_block_32bpp_c_unittest.cc
    desktop/diff_block_32bpp_sse2_unittest.cc
    desktop/frame_unittest.cc
    desktop/geometry_unittest.cc
    desktop/region_unittest.cc)

if (APPLE)
    list(APPEND SOURCE_BASE_DESKTOP_MAC
        desktop/mac/desktop_configuration_monitor.cc
        desktop/mac/desktop_configuration_monitor.h
        desktop/mac/desktop_configuration.mm
        desktop/mac/desktop_configuration.h
        desktop/mac/frame_cgimage.mm
        desktop/mac/frame_cgimage.h
        desktop/mac/frame_iosurface.mm
        desktop/mac/frame_iosurface.h
        desktop/mac/frame_provider.mm
        desktop/mac/frame_provider.h)
endif()

if (WIN32)
    list(APPEND SOURCE_BASE_DESKTOP_WIN
        desktop/win/bitmap_info.h
        desktop/win/cursor.cc
        desktop/win/cursor.h
        desktop/win/d3d_device.cc
        desktop/win/d3d_device.h
        desktop/win/dfmirage.h
        desktop/win/dfmirage_helper.cc
        desktop/win/dfmirage_helper.h
        desktop/win/display_configuration_monitor.cc
        desktop/win/display_configuration_monitor.h
        desktop/win/dxgi_adapter_duplicator.cc
        desktop/win/dxgi_adapter_duplicator.h
        desktop/win/dxgi_context.cc
        desktop/win/dxgi_context.h
        desktop/win/dxgi_cursor.cc
        desktop/win/dxgi_cursor.h
        desktop/win/dxgi_duplicator_controller.cc
        desktop/win/dxgi_duplicator_controller.h
        desktop/win/dxgi_frame.cc
        desktop/win/dxgi_frame.h
        desktop/win/dxgi_output_duplicator.cc
        desktop/win/dxgi_output_duplicator.h
        desktop/win/dxgi_texture.cc
        desktop/win/dxgi_texture.h
        desktop/win/dxgi_texture_mapping.cc
        desktop/win/dxgi_texture_mapping.h
        desktop/win/dxgi_texture_staging.cc
        desktop/win/dxgi_texture_staging.h
        desktop/win/mirror_helper.cc
        desktop/win/mirror_helper.h
        desktop/win/mv2.h
        desktop/win/mv2_helper.cc
        desktop/win/mv2_helper.h
        desktop/win/screen_capture_utils.cc
        desktop/win/screen_capture_utils.h
        desktop/win/swdevice_defines.h
        desktop/win/virtual_display.cc
        desktop/win/virtual_display.h)

    list(APPEND SOURCE_BASE_DESKTOP_WIN_TESTS
        desktop/win/cursor_unittest.cc
        desktop/win/cursor_unittest_resources.h
        desktop/win/cursor_unittest_resources.rc)
endif()

if (LINUX)
    list(APPEND SOURCE_BASE_DESKTOP_X11
        desktop/x11/shared_x_display.cc
        desktop/x11/shared_x_display.h
        desktop/x11/x_atom_cache.cc
        desktop/x11/x_atom_cache.h
        desktop/x11/x_error_trap.cc
        desktop/x11/x_error_trap.h
        desktop/x11/x_server_pixel_buffer.cc
        desktop/x11/x_server_pixel_buffer.h)
endif()

list(APPEND SOURCE_BASE_FILES
    files/file_util.cc
    files/file_util.h)

if (UNIX)
    list(APPEND SOURCE_BASE_FILES
        files/file_descriptor_watcher_posix.cc
        files/file_descriptor_watcher_posix.h)
endif()

list(APPEND SOURCE_BASE_IPC
    ipc/ipc_channel.cc
    ipc/ipc_channel.h
    ipc/ipc_server.cc
    ipc/ipc_server.h
    ipc/shared_memory.cc
    ipc/shared_memory.h
    ipc/shared_memory_factory.cc
    ipc/shared_memory_factory.h
    ipc/shared_memory_factory_proxy.cc
    ipc/shared_memory_factory_proxy.h)

if (APPLE)
    list(APPEND SOURCE_BASE_MAC
        mac/app_nap_blocker.mm
        mac/app_nap_blocker.h
        mac/nsstring_conversions.mm
        mac/nsstring_conversions.h
        mac/scoped_cftyperef.h)
endif()

list(APPEND SOURCE_BASE_MEMORY
    memory/aligned_memory.cc
    memory/aligned_memory.h
    memory/local_memory.h
    memory/typed_buffer.h
    memory/local_memory_impl/bad_local_weak_ptr.h
    memory/local_memory_impl/checked_delete.h
    memory/local_memory_impl/enable_shared_from_this.h
    memory/local_memory_impl/local_shared_ptr.h
    memory/local_memory_impl/local_weak_ptr.h
    memory/local_memory_impl/make_local_shared.h
    memory/local_memory_impl/shared_count.h
    memory/local_memory_impl/sp_convertible.h
    memory/local_memory_impl/sp_counted_base_nt.h
    memory/local_memory_impl/sp_counted_impl.h)

list(APPEND SOURCE_BASE_MEMORY_TESTS
    memory/aligned_memory_unittest.cc)

list(APPEND SOURCE_BASE_NET
    net/adapter_enumerator.cc
    net/adapter_enumerator.h
    net/address.cc
    net/address.h
    net/curl_util.cc
    net/curl_util.h
    net/ip_util.cc
    net/ip_util.h
    net/network_channel.cc
    net/network_channel.h
    net/tcp_channel.cc
    net/tcp_channel.h
    net/tcp_server.cc
    net/tcp_server.h
    net/variable_size.cc
    net/variable_size.h
    net/write_task.h)

if (WIN32)
    list(APPEND SOURCE_BASE_NET
        net/connect_enumerator.cc
        net/connect_enumerator.h
        net/firewall_manager.cc
        net/firewall_manager.h
        net/open_files_enumerator.cc
        net/open_files_enumerator.h
        net/route_enumerator.cc
        net/route_enumerator.h)
endif()

list(APPEND SOURCE_BASE_NET_TESTS
    net/address_unittest.cc
    net/ip_util_unittest.cc)

list(APPEND SOURCE_BASE_PEER
    peer/authenticator.cc
    peer/authenticator.h
    peer/client_authenticator.cc
    peer/client_authenticator.h
    peer/host_id.cc
    peer/host_id.h
    peer/relay_peer.cc
    peer/relay_peer.h
    peer/relay_peer_manager.cc
    peer/relay_peer_manager.h
    peer/server_authenticator.cc
    peer/server_authenticator.h
    peer/server_authenticator_manager.cc
    peer/server_authenticator_manager.h
    peer/user.cc
    peer/user.h
    peer/user_list.cc
    peer/user_list.h
    peer/user_list_base.h)

if (UNIX)
    list(APPEND SOURCE_BASE_POSIX
        posix/eintr_wrapper.h)
endif()

list(APPEND SOURCE_BASE_THREADING
    threading/asio_event_dispatcher.cc
    threading/asio_event_dispatcher.h
    threading/simple_thread.cc
    threading/simple_thread.h
    threading/thread.cc
    threading/thread.h
    threading/thread_checker.cc
    threading/thread_checker.h)

if (WIN32)
    list(APPEND SOURCE_BASE_WIN
        win/battery_enumerator.cc
        win/battery_enumerator.h
        win/desktop.cc
        win/desktop.h
        win/device.cc
        win/device.h
        win/device_enumerator.cc
        win/device_enumerator.h
        win/drive_enumerator.cc
        win/drive_enumerator.h
        win/event_enumerator.cc
        win/event_enumerator.h
        win/file_version_info.cc
        win/file_version_info.h
        win/message_window.cc
        win/message_window.h
        win/mini_dump_writer.cc
        win/mini_dump_writer.h
        win/monitor_enumerator.cc
        win/monitor_enumerator.h
        win/net_share_enumerator.cc
        win/net_share_enumerator.h
        win/power_info.cc
        win/power_info.h
        win/printer_enumerator.cc
        win/printer_enumerator.h
        win/process_util.cc
        win/process_util.h
        win/registry.cc
        win/registry.h
        win/resource_util.cc
        win/resource_util.h
        win/safe_mode_util.cc
        win/safe_mode_util.h
        win/scoped_clipboard.cc
        win/scoped_clipboard.h
        win/scoped_co_mem.h
        win/scoped_com_initializer.cc
        win/scoped_com_initializer.h
        win/scoped_device_info.h
        win/scoped_gdi_object.h
        win/scoped_hdc.h
        win/scoped_hglobal.h
        win/scoped_impersonator.cc
        win/scoped_impersonator.h
        win/scoped_local.h
        win/scoped_object.h
        win/scoped_select_object.h
        win/scoped_thread_desktop.cc
        win/scoped_thread_desktop.h
        win/scoped_user_object.h
        win/scoped_wts_memory.h
        win/security_helpers.cc
        win/security_helpers.h
        win/service_controller.cc
        win/service_controller.h
        win/service_enumerator.cc
        win/service_enumerator.h
        win/session_enumerator.cc
        win/session_enumerator.h
        win/session_info.cc
        win/session_info.h
        win/session_status.cc
        win/session_status.h
        win/session_watcher.cc
        win/session_watcher.h
        win/user_enumerator.cc
        win/user_enumerator.h
        win/user_group_enumerator.cc
        win/user_group_enumerator.h
        win/video_adapter_enumerator.cc
        win/video_adapter_enumerator.h
        win/window_station.cc
        win/window_station.h
        win/windows_version.cc
        win/windows_version.h)

    list(APPEND SOURCE_BASE_WIN_TESTS
        win/registry_unittest.cc
        win/scoped_object_unittest.cc)
endif()

if (LINUX)
    list(APPEND SOURCE_BASE_X11
        x11/x_server_clipboard.cc
        x11/x_server_clipboard.h)
endif()

source_group("" FILES ${SOURCE_BASE} ${SOURCE_BASE_TESTS})
source_group(audio FILES ${SOURCE_BASE_AUDIO})
source_group(codec FILES ${SOURCE_BASE_CODEC})
source_group(crypto FILES ${SOURCE_BASE_CRYPTO} ${SOURCE_BASE_CRYPTO_TESTS})
source_group(desktop FILES ${SOURCE_BASE_DESKTOP} ${SOURCE_BASE_DESKTOP_TESTS})
source_group(files FILES ${SOURCE_BASE_FILES})
source_group(ipc FILES ${SOURCE_BASE_IPC})
source_group(memory FILES ${SOURCE_BASE_MEMORY} ${SOURCE_BASE_MEMORY_TESTS})
source_group(net FILES ${SOURCE_BASE_NET} ${SOURCE_BASE_NET_TESTS})
source_group(peer FILES ${SOURCE_BASE_PEER})
source_group(threading FILES ${SOURCE_BASE_THREADING})

if (WIN32)
    source_group(audio\\win FILES ${SOURCE_BASE_AUDIO_WIN})
    source_group(desktop\\win FILES ${SOURCE_BASE_DESKTOP_WIN} ${SOURCE_BASE_DESKTOP_WIN_TESTS})
    source_group(posix FILES ${SOURCE_BASE_POSIX})
    source_group(win FILES ${SOURCE_BASE_WIN} ${SOURCE_BASE_WIN_TESTS})
endif()

if (LINUX)
    source_group(audio\\linux FILES ${SOURCE_BASE_AUDIO_LINUX})
    source_group(desktop\\x11 FILES ${SOURCE_BASE_DESKTOP_X11})
    source_group(posix FILES ${SOURCE_BASE_POSIX})
    source_group(x11 FILES ${SOURCE_BASE_X11})
endif()

if (APPLE)
    source_group(mac FILES ${SOURCE_BASE_MAC})
    source_group(desktop\\mac FILES ${SOURCE_BASE_DESKTOP_MAC})
endif()

add_library(aspia_base STATIC
    ${SOURCE_BASE}
    ${SOURCE_BASE_AUDIO}
    ${SOURCE_BASE_AUDIO_LINUX}
    ${SOURCE_BASE_AUDIO_WIN}
    ${SOURCE_BASE_CODEC}
    ${SOURCE_BASE_CRYPTO}
    ${SOURCE_BASE_DESKTOP}
    ${SOURCE_BASE_DESKTOP_MAC}
    ${SOURCE_BASE_DESKTOP_WIN}
    ${SOURCE_BASE_DESKTOP_X11}
    ${SOURCE_BASE_FILES}
    ${SOURCE_BASE_IPC}
    ${SOURCE_BASE_MAC}
    ${SOURCE_BASE_MEMORY}
    ${SOURCE_BASE_NET}
    ${SOURCE_BASE_PEER}
    ${SOURCE_BASE_POSIX}
    ${SOURCE_BASE_THREADING}
    ${SOURCE_BASE_WIN}
    ${SOURCE_BASE_X11})

set_property(TARGET aspia_base PROPERTY AUTOMOC ON)
set_property(TARGET aspia_base PROPERTY AUTOUIC ON)
set_property(TARGET aspia_base PROPERTY AUTORCC ON)
qt5_import_plugins(aspia_base
    INCLUDE ""
    EXCLUDE ""
    INCLUDE_BY_TYPE ""
    EXCLUDE_BY_TYPE imageformats)

if (LINUX)
    set(BASE_PLATFORM_LIBS ${XFIXES_LIB} stdc++fs xdg_user_dirs)
endif()

target_link_libraries(aspia_base PRIVATE aspia_proto
    ${THIRD_PARTY_LIBS} ${BASE_PLATFORM_LIBS} ${QT_COMMON_LIBS} ${QT_PLATFORM_LIBS})

if (WIN32)
    set(BASE_TESTS_PLATFORM_LIBS crypt32 dbghelp iphlpapi ws2_32)
endif()

if (LINUX)
    set(BASE_TESTS_PLATFORM_LIBS stdc++fs)
endif()

if (APPLE)
    set(BASE_TESTS_PLATFORM_LIBS ${FOUNDATION_LIB})
endif()

add_executable(aspia_base_tests
    ${SOURCE_BASE_TESTS}
    ${SOURCE_BASE_CRYPTO_TESTS}
    ${SOURCE_BASE_DESKTOP_TESTS}
    ${SOURCE_BASE_DESKTOP_WIN_TESTS}
    ${SOURCE_BASE_MEMORY_TESTS}
    ${SOURCE_BASE_NET_TESTS}
    ${SOURCE_BASE_WIN_TESTS})
target_link_libraries(aspia_base_tests PRIVATE
    aspia_base
    aspia_proto
    GTest::gtest
    ${BASE_TESTS_PLATFORM_LIBS}
    ${THIRD_PARTY_LIBS}
    ${QT_COMMON_LIBS}
    ${QT_PLATFORM_LIBS})
set_property(TARGET aspia_base_tests PROPERTY AUTOMOC ON)
set_property(TARGET aspia_base_tests PROPERTY AUTOUIC ON)
set_property(TARGET aspia_base_tests PROPERTY AUTORCC ON)
qt5_import_plugins(aspia_base_tests
    INCLUDE ""
    EXCLUDE ""
    INCLUDE_BY_TYPE ""
    EXCLUDE_BY_TYPE imageformats)

add_test(NAME aspia_base_tests COMMAND aspia_base_tests)
