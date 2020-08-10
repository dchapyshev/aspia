#
# Aspia Project
# Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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
    base64.cc
    base64.h
    bitset.h
    command_line.cc
    command_line.h
    compiler_specific.h
    converter.h
    cpuid.cc
    cpuid.h
    crc32.cc
    crc32.h
    debug.cc
    debug.h
    endian.cc
    endian.h
    guid.cc
    guid.h
    location.cc
    location.h
    logging.cc
    logging.h
    macros_magic.h
    power_controller.h
    power_controller_win.cc
    process_handle.cc
    process_handle.h
    scoped_clear_last_error.cc
    scoped_clear_last_error.h
    session_id.cc
    session_id.h
    smbios.h
    smbios_parser.cc
    smbios_parser.h
    smbios_reader.h
    smbios_reader_win.cc
    stl_util.h
    sys_info.cc
    sys_info.h
    sys_info_win.cc
    system_error.cc
    system_error.h
    system_time.cc
    system_time.h
    task_runner.cc
    task_runner.h
    version.cc
    version.h
    waitable_event.cc
    waitable_event.h
    waitable_timer.cc
    waitable_timer.h
    xml_sax_writer.cc
    xml_sax_writer.h)

list(APPEND SOURCE_BASE_UNIT_TESTS
    base64_unittest.cc
    bitset_unittest.cc
    converter_unittest.cc
    crc32_unittest.cc
    guid_unittest.cc
    scoped_clear_last_error_unittest.cc
    stl_util_unittest.cc
    tests_main.cc
    version_unittest.cc)

list(APPEND SOURCE_BASE_CODEC
    codec/cursor_decoder.cc
    codec/cursor_decoder.h
    codec/cursor_encoder.cc
    codec/cursor_encoder.h
    codec/encoder_bitrate_filter.cc
    codec/encoder_bitrate_filter.h
    codec/pixel_translator.cc
    codec/pixel_translator.h
    codec/running_samples.cc
    codec/running_samples.h
    codec/scale_reducer.cc
    codec/scale_reducer.h
    codec/scoped_vpx_codec.cc
    codec/scoped_vpx_codec.h
    codec/scoped_zstd_stream.cc
    codec/scoped_zstd_stream.h
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
    codec/video_util.cc
    codec/video_util.h
    codec/weighted_samples.cc
    codec/weighted_samples.h)

list(APPEND SOURCE_BASE_CODEC_UNIT_TESTS
    codec/running_samples_unittest.cc
    codec/weighted_samples_unittest.cc)

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

list(APPEND SOURCE_BASE_CRYPTO_UNIT_TESTS
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
    desktop/cursor_capturer.h
    desktop/cursor_capturer_win.cc
    desktop/cursor_capturer_win.h
    desktop/diff_block_32bpp_avx2.cc
    desktop/diff_block_32bpp_avx2.h
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
    desktop/frame_dib.cc
    desktop/frame_dib.h
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
    desktop/region.cc
    desktop/region.h
    desktop/screen_capturer.cc
    desktop/screen_capturer.h
    desktop/screen_capturer_dxgi.cc
    desktop/screen_capturer_dxgi.h
    desktop/screen_capturer_gdi.cc
    desktop/screen_capturer_gdi.h
    desktop/screen_capturer_wrapper.cc
    desktop/screen_capturer_wrapper.h
    desktop/shared_frame.cc
    desktop/shared_frame.h
    desktop/shared_memory_frame.cc
    desktop/shared_memory_frame.h)

list(APPEND SOURCE_DESKTOP_UNIT_TESTS
    desktop/diff_block_32bpp_avx2_unittest.cc
    desktop/diff_block_32bpp_c_unittest.cc
    desktop/diff_block_32bpp_sse2_unittest.cc
    desktop/geometry_unittest.cc
    desktop/region_unittest.cc)

list(APPEND SOURCE_BASE_DESKTOP_WIN
    desktop/win/bitmap_info.h
    desktop/win/cursor.cc
    desktop/win/cursor.h
    desktop/win/d3d_device.cc
    desktop/win/d3d_device.h
    desktop/win/desktop_environment.cc
    desktop/win/desktop_environment.h
    desktop/win/display_configuration_monitor.cc
    desktop/win/display_configuration_monitor.h
    desktop/win/dxgi_adapter_duplicator.cc
    desktop/win/dxgi_adapter_duplicator.h
    desktop/win/dxgi_context.cc
    desktop/win/dxgi_context.h
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
    desktop/win/power_save_blocker.cc
    desktop/win/power_save_blocker.h
    desktop/win/screen_capture_utils.cc
    desktop/win/screen_capture_utils.h)

list(APPEND SOURCE_BASE_DESKTOP_WIN_UNIT_TESTS
    desktop/win/cursor_unittest.cc
    desktop/win/cursor_unittest_resources.h
    desktop/win/cursor_unittest_resources.rc)

list(APPEND SOURCE_BASE_FILES
    files/base_paths.cc
    files/base_paths.h
    files/file_path_watcher.cc
    files/file_path_watcher.h
    files/file_path_watcher_win.cc
    files/file_util.cc
    files/file_util.h
    files/scoped_temp_file.cc
    files/scoped_temp_file.h)

list(APPEND SOURCE_BASE_IPC
    ipc/ipc_channel.cc
    ipc/ipc_channel.h
    ipc/ipc_channel_proxy.cc
    ipc/ipc_channel_proxy.h
    ipc/ipc_server.cc
    ipc/ipc_server.h
    ipc/shared_memory.cc
    ipc/shared_memory.h
    ipc/shared_memory_factory.cc
    ipc/shared_memory_factory.h
    ipc/shared_memory_factory_proxy.cc
    ipc/shared_memory_factory_proxy.h)

list(APPEND SOURCE_BASE_MEMORY
    memory/aligned_memory.cc
    memory/aligned_memory.h
    memory/byte_array.cc
    memory/byte_array.h
    memory/scalable_queue.h
    memory/typed_buffer.h)

list(APPEND SOURCE_BASE_MEMORY_UNIT_TESTS
    memory/aligned_memory_unittest.cc
    memory/byte_array_unittest.cc)

list(APPEND SOURCE_BASE_MESSAGE_LOOP
    message_loop/message_loop.cc
    message_loop/message_loop.h
    message_loop/message_loop_task_runner.cc
    message_loop/message_loop_task_runner.h
    message_loop/message_pump.h
    message_loop/message_pump_asio.cc
    message_loop/message_pump_asio.h
    message_loop/message_pump_default.cc
    message_loop/message_pump_default.h
    message_loop/message_pump_dispatcher.h
    message_loop/message_pump_win.cc
    message_loop/message_pump_win.h
    message_loop/pending_task.cc
    message_loop/pending_task.h)

list(APPEND SOURCE_BASE_NET
    net/adapter_enumerator.cc
    net/adapter_enumerator.h
    net/address.cc
    net/address.h
    net/firewall_manager.cc
    net/firewall_manager.h
    net/ip_util.cc
    net/ip_util.h
    net/network_channel.cc
    net/network_channel.h
    net/network_channel_proxy.cc
    net/network_channel_proxy.h
    net/network_server.cc
    net/network_server.h
    net/variable_size.cc
    net/variable_size.h)

list(APPEND SOURCE_BASE_NET_UNIT_TESTS
    net/address_unittest.cc)

list(APPEND SOURCE_BASE_SETTINGS
    settings/json_settings.cc
    settings/json_settings.h
    settings/settings.cc
    settings/settings.h
    settings/xml_settings.cc
    settings/xml_settings.h)

list(APPEND SOURCE_BASE_SETTINGS_UNIT_TESTS
    settings/json_settings_unittest.cc
    settings/xml_settings_unittest.cc)

list(APPEND SOURCE_BASE_STRINGS
    strings/strcat.cc
    strings/strcat.h
    strings/string_number_conversions.cc
    strings/string_number_conversions.h
    strings/string_printf.cc
    strings/string_printf.h
    strings/string_split.cc
    strings/string_split.h
    strings/string_util.cc
    strings/string_util.h
    strings/string_util_constants.cc
    strings/string_util_constants.h
    strings/string_util_win.cc
    strings/unicode.cc
    strings/unicode.h)

list(APPEND SOURCE_BASE_STRINGS_UNIT_TESTS
    strings/string_number_conversions_unittest.cc
    strings/string_printf_unittest.cc
    strings/string_split_unittest.cc)

list(APPEND SOURCE_BASE_THREADING
    threading/simple_thread.cc
    threading/simple_thread.h
    threading/thread.cc
    threading/thread.h
    threading/thread_checker.cc
    threading/thread_checker.h)

list(APPEND SOURCE_BASE_WIN
    win/desktop.cc
    win/desktop.h
    win/drive_enumerator.cc
    win/drive_enumerator.h
    win/file_version_info.cc
    win/file_version_info.h
    win/message_window.cc
    win/message_window.h
    win/object_watcher.cc
    win/object_watcher.h
    win/printer_enumerator.cc
    win/printer_enumerator.h
    win/process.cc
    win/process.h
    win/process_util.cc
    win/process_util.h
    win/registry.cc
    win/registry.h
    win/resource_util.cc
    win/resource_util.h
    win/scoped_clipboard.cc
    win/scoped_clipboard.h
    win/scoped_com_initializer.cc
    win/scoped_com_initializer.h
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
    win/service.cc
    win/service.h
    win/service_controller.cc
    win/service_controller.h
    win/session_enumerator.cc
    win/session_enumerator.h
    win/session_info.cc
    win/session_info.h
    win/session_status.cc
    win/session_status.h
    win/session_watcher.cc
    win/session_watcher.h
    win/windows_version.cc
    win/windows_version.h)

list(APPEND SOURCE_BASE_WIN_UNIT_TESTS
    win/registry_unittest.cc
    win/scoped_object_unittest.cc)

source_group("" FILES ${SOURCE_BASE} ${SOURCE_BASE_UNIT_TESTS})
source_group(codec FILES ${SOURCE_BASE_CODEC} ${SOURCE_BASE_CODEC_UNIT_TESTS})
source_group(crypto FILES ${SOURCE_BASE_CRYPTO} ${SOURCE_BASE_CRYPTO_UNIT_TESTS})
source_group(desktop FILES ${SOURCE_BASE_DESKTOP} ${SOURCE_BASE_DESKTOP_UNIT_TESTS})
source_group(desktop\\win FILES ${SOURCE_BASE_DESKTOP_WIN} ${SOURCE_BASE_DESKTOP_WIN_UNIT_TESTS})
source_group(files FILES ${SOURCE_BASE_FILES})
source_group(ipc FILES ${SOURCE_BASE_IPC})
source_group(memory FILES ${SOURCE_BASE_MEMORY} ${SOURCE_BASE_MEMORY_UNIT_TESTS})
source_group(message_loop FILES ${SOURCE_BASE_MESSAGE_LOOP})
source_group(net FILES ${SOURCE_BASE_NET} ${SOURCE_BASE_NET_UNIT_TESTS})
source_group(settings FILES ${SOURCE_BASE_SETTINGS} ${SOURCE_BASE_SETTINGS_UNIT_TESTS})
source_group(strings FILES ${SOURCE_BASE_STRINGS} ${SOURCE_BASE_STRINGS_UNIT_TESTS})
source_group(threading FILES ${SOURCE_BASE_THREADING})
source_group(win FILES ${SOURCE_BASE_WIN} ${SOURCE_BASE_WIN_UNIT_TESTS})

add_library(aspia_base STATIC
    ${SOURCE_BASE}
    ${SOURCE_BASE_CODEC}
    ${SOURCE_BASE_CRYPTO}
    ${SOURCE_BASE_DESKTOP}
    ${SOURCE_BASE_DESKTOP_WIN}
    ${SOURCE_BASE_FILES}
    ${SOURCE_BASE_IPC}
    ${SOURCE_BASE_MEMORY}
    ${SOURCE_BASE_MESSAGE_LOOP}
    ${SOURCE_BASE_NET}
    ${SOURCE_BASE_SETTINGS}
    ${SOURCE_BASE_STRINGS}
    ${SOURCE_BASE_THREADING}
    ${SOURCE_BASE_WIN})
target_link_libraries(aspia_base ${THIRD_PARTY_LIBS})

# If the build of unit tests is enabled.
if (BUILD_UNIT_TESTS)
    add_executable(aspia_base_tests
        ${SOURCE_BASE_UNIT_TESTS}
        ${SOURCE_BASE_CODEC_UNIT_TESTS}
        ${SOURCE_BASE_CRYPTO_UNIT_TESTS}
        ${SOURCE_BASE_DESKTOP_UNIT_TESTS}
        ${SOURCE_BASE_DESKTOP_WIN_UNIT_TESTS}
        ${SOURCE_BASE_MEMORY_UNIT_TESTS}
        ${SOURCE_BASE_NET_UNIT_TESTS}
        ${SOURCE_BASE_SETTINGS_UNIT_TESTS}
        ${SOURCE_BASE_STRINGS_UNIT_TESTS}
        ${SOURCE_BASE_WIN_UNIT_TESTS})
    target_link_libraries(aspia_base_tests
        aspia_base
        optimized gtest
        debug gtestd
        ${WINDOWS_LIBS}
        ${THIRD_PARTY_LIBS})

    add_test(NAME aspia_base_tests COMMAND aspia_base_tests)
endif()

