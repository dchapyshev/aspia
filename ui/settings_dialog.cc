//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/settings_dialog.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/settings_dialog.h"
#include "ui/resource.h"
#include "ui/base/module.h"
#include "desktop_capture/pixel_format.h"
#include "codec/video_helpers.h"
#include "base/string_util.h"

#include <windowsx.h>
#include <commctrl.h>

namespace aspia {

enum Codecs
{
    kVP9 = 0,
    kVP8 = 1,
    kZLIB = 2
};

enum PixelFormats
{
    kARGB = 0,
    kRGB888 = 1,
    kRGB565 = 2,
    kRGB555 = 3,
    kRGB444 = 4,
    kRGB332 = 5,
    kRGB222 = 6,
    kRGB111 = 7
};

static const int kMaxUpdateInterval = 100;
static const int kMinUpdateInterval = 15;
static const int kDefUpdateInterval = 30;

static const int kMaxCompressRatio = 9;
static const int kMinCompressRatio = 1;
static const int kDefCompressRatio = 6;

INT_PTR SettingsDialog::DoModal(HWND parent,
                                proto::SessionType session_type,
                                const proto::DesktopSessionConfig& config)
{
    session_type_ = session_type;
    config_.CopyFrom(config);
    return DoModal(parent);
}

INT_PTR SettingsDialog::DoModal(HWND parent)
{
    return Run(Module::Current(), parent, IDD_SETTINGS);
}

void SettingsDialog::InitColorDepthList()
{
    HWND combo = GetDlgItem(IDC_COLOR_DEPTH_COMBO);

    AddColorDepth(combo, kARGB,   IDS_32BIT);
    AddColorDepth(combo, kRGB888, IDS_24BIT);
    AddColorDepth(combo, kRGB565, IDS_16BIT);
    AddColorDepth(combo, kRGB555, IDS_15BIT);
    AddColorDepth(combo, kRGB444, IDS_12BIT);
    AddColorDepth(combo, kRGB332, IDS_8BIT);
    AddColorDepth(combo, kRGB222, IDS_6BIT);
    AddColorDepth(combo, kRGB111, IDS_3BIT);

    PixelFormat format(ConvertFromVideoPixelFormat(config_.pixel_format()));

    int curr_item = kRGB565;

    if (format.IsEqual(PixelFormat::ARGB()))
    {
        curr_item = kARGB;
    }
    else if (format.IsEqual(PixelFormat::RGB888()))
    {
        curr_item = kRGB888;
    }
    else if (format.IsEqual(PixelFormat::RGB565()))
    {
        curr_item = kRGB565;
    }
    else if (format.IsEqual(PixelFormat::RGB555()))
    {
        curr_item = kRGB555;
    }
    else if (format.IsEqual(PixelFormat::RGB444()))
    {
        curr_item = kRGB444;
    }
    else if (format.IsEqual(PixelFormat::RGB332()))
    {
        curr_item = kRGB332;
    }
    else if (format.IsEqual(PixelFormat::RGB222()))
    {
        curr_item = kRGB222;
    }
    else if (format.IsEqual(PixelFormat::RGB111()))
    {
        curr_item = kRGB111;
    }

    ComboBox_SetCurSel(combo, curr_item);
}

void SettingsDialog::InitCodecList()
{
    HWND combo = GetDlgItem(IDC_CODEC_COMBO);

    ComboBox_InsertString(combo, kVP9, L"VP9");
    ComboBox_InsertString(combo, kVP8, L"VP8");
    ComboBox_InsertString(combo, kZLIB, L"ZLIB");

    int curr = -1;

    switch (config_.video_encoding())
    {
        case proto::VideoEncoding::VIDEO_ENCODING_VP8:
            curr = kVP8;
            break;

        case proto::VideoEncoding::VIDEO_ENCODING_VP9:
            curr = kVP9;
            break;

        case proto::VideoEncoding::VIDEO_ENCODING_ZLIB:
            curr = kZLIB;
            break;
    }

    ComboBox_SetCurSel(combo, curr);
}

void SettingsDialog::UpdateCompressionRatio(int compression_ratio)
{
    std::wstring format = module().string(IDS_COMPRESSION_RATIO_FORMAT);

    SetDlgItemString(IDC_COMPRESS_RATIO_TEXT,
                     StringPrintfW(format.c_str(), compression_ratio));
}

void SettingsDialog::OnInitDialog()
{
    InitCodecList();
    InitColorDepthList();
    UpdateCompressionRatio(config_.compress_ratio());
    OnCodecChanged();

    HWND ratio = GetDlgItem(IDC_COMPRESS_RATIO_TRACKBAR);
    SendMessageW(ratio, TBM_SETRANGE, TRUE, MAKELPARAM(kMinCompressRatio, kMaxCompressRatio));
    SendMessageW(ratio, TBM_SETPOS, TRUE, config_.compress_ratio());

    SendMessageW(GetDlgItem(IDC_INTERVAL_EDIT), EM_SETLIMITTEXT, 3, 0);

    HWND updown = GetDlgItem(IDC_INTERVAL_UPDOWN);
    SendMessageW(updown, UDM_SETRANGE, 0, MAKELPARAM(kMaxUpdateInterval, kMinUpdateInterval));
    SendMessageW(updown, UDM_SETPOS, 0, MAKELPARAM(config_.update_interval(), 0));

    if (session_type_ == proto::SessionType::SESSION_TYPE_DESKTOP_VIEW)
    {
        EnableWindow(GetDlgItem(IDC_ENABLE_CLIPBOARD_CHECK), FALSE);
        EnableWindow(GetDlgItem(IDC_ENABLE_CURSOR_SHAPE_CHECK), FALSE);
    }
    else
    {
        CheckDlgButton(hwnd(), IDC_ENABLE_CURSOR_SHAPE_CHECK,
            (config_.flags() & proto::DesktopSessionConfig::ENABLE_CURSOR_SHAPE) ?
                BST_CHECKED : BST_UNCHECKED);

        CheckDlgButton(hwnd(), IDC_ENABLE_CLIPBOARD_CHECK,
            (config_.flags() & proto::DesktopSessionConfig::ENABLE_CLIPBOARD) ?
                BST_CHECKED : BST_UNCHECKED);
    }
}

void SettingsDialog::OnHScroll(HWND ctrl)
{
    if (GetWindowLongPtrW(ctrl, GWLP_ID) == IDC_COMPRESS_RATIO_TRACKBAR)
    {
        int compression_ratio =
            static_cast<int>(SendMessageW(GetDlgItem(IDC_COMPRESS_RATIO_TRACKBAR),
                                          TBM_GETPOS, 0, 0));

        UpdateCompressionRatio(compression_ratio);
    }
}

void SettingsDialog::OnCodecChanged()
{
    BOOL has_pixel_format =
        (ComboBox_GetCurSel(GetDlgItem(IDC_CODEC_COMBO)) == kZLIB);

    EnableWindow(GetDlgItem(IDC_COLOR_DEPTH_TEXT), has_pixel_format);
    EnableWindow(GetDlgItem(IDC_COLOR_DEPTH_COMBO), has_pixel_format);
    EnableWindow(GetDlgItem(IDC_COMPRESS_RATIO_TEXT), has_pixel_format);
    EnableWindow(GetDlgItem(IDC_COMPRESS_RATIO_TRACKBAR), has_pixel_format);
    EnableWindow(GetDlgItem(IDC_FAST_TEXT), has_pixel_format);
    EnableWindow(GetDlgItem(IDC_BEST_TEXT), has_pixel_format);
}

void SettingsDialog::OnCodecChanged(WORD notify_code)
{
    if (notify_code == CBN_SELCHANGE)
        OnCodecChanged();
}

void SettingsDialog::OnOkButton()
{
    int codec = ComboBox_GetCurSel(GetDlgItem(IDC_CODEC_COMBO));

    switch (codec)
    {
        case kVP8:
            config_.set_video_encoding(proto::VideoEncoding::VIDEO_ENCODING_VP8);
            break;

        case kVP9:
            config_.set_video_encoding(proto::VideoEncoding::VIDEO_ENCODING_VP9);
            break;

        case kZLIB:
            config_.set_video_encoding(proto::VideoEncoding::VIDEO_ENCODING_ZLIB);
            break;
    }

    if (codec == kZLIB)
    {
        PixelFormat format;

        switch (ComboBox_GetCurSel(GetDlgItem(IDC_COLOR_DEPTH_COMBO)))
        {
            case kARGB:
                format = PixelFormat::ARGB();
                break;

            case kRGB888:
                format = PixelFormat::RGB888();
                break;

            case kRGB565:
                format = PixelFormat::RGB565();
                break;

            case kRGB555:
                format = PixelFormat::RGB555();
                break;

            case kRGB444:
                format = PixelFormat::RGB444();
                break;

            case kRGB332:
                format = PixelFormat::RGB332();
                break;

            case kRGB222:
                format = PixelFormat::RGB222();
                break;

            case kRGB111:
                format = PixelFormat::RGB111();
                break;
        }

        ConvertToVideoPixelFormat(format, config_.mutable_pixel_format());

        int compress_ratio =
            static_cast<int>(SendMessageW(GetDlgItem(IDC_COMPRESS_RATIO_TRACKBAR), TBM_GETPOS, 0, 0));

        if (compress_ratio >= kMinCompressRatio && compress_ratio <= kMaxCompressRatio)
        {
            config_.set_compress_ratio(compress_ratio);
        }
    }

    uint32_t flags = 0;

    if (IsDlgButtonChecked(hwnd(), IDC_ENABLE_CURSOR_SHAPE_CHECK) == BST_CHECKED)
        flags |= proto::DesktopSessionConfig::ENABLE_CURSOR_SHAPE;

    if (IsDlgButtonChecked(hwnd(), IDC_ENABLE_CLIPBOARD_CHECK) == BST_CHECKED)
        flags |= proto::DesktopSessionConfig::ENABLE_CLIPBOARD;

    config_.set_flags(flags);

    DWORD ret = static_cast<DWORD>(SendMessageW(GetDlgItem(IDC_INTERVAL_UPDOWN),
                                                UDM_GETPOS, 0, 0));
    if (HIWORD(ret))
    {
        int interval = LOWORD(ret);

        if (interval >= kMinUpdateInterval && interval <= kMaxUpdateInterval)
        {
            config_.set_update_interval(interval);
        }
    }

    EndDialog(IDOK);
}

void SettingsDialog::AddColorDepth(HWND combobox, int index, UINT string_id)
{
    ComboBox_InsertString(combobox,
                          index,
                          module().string(string_id).c_str());
}

INT_PTR SettingsDialog::OnMessage(UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
        case WM_INITDIALOG:
            OnInitDialog();
            break;

        case WM_CLOSE:
            EndDialog(IDCANCEL);
            break;

        case WM_HSCROLL:
            OnHScroll(reinterpret_cast<HWND>(lparam));
            break;

        case WM_COMMAND:
        {
            switch (LOWORD(wparam))
            {
                case IDC_CODEC_COMBO:
                    OnCodecChanged(HIWORD(wparam));
                    break;

                case IDOK:
                    OnOkButton();
                    break;

                case IDCANCEL:
                    EndDialog(IDCANCEL);
                    break;
            }
        }
        break;
    }

    return 0;
}

} // namespace aspia
