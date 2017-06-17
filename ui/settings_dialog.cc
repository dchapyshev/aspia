//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/settings_dialog.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/settings_dialog.h"
#include "ui/resource.h"
#include "ui/base/module.h"
#include "ui/base/combobox.h"
#include "ui/base/updown.h"
#include "ui/base/trackbar.h"
#include "desktop_capture/pixel_format.h"
#include "codec/video_helpers.h"
#include "base/string_util.h"

namespace aspia {

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

INT_PTR UiSettingsDialog::DoModal(HWND parent,
                                  proto::SessionType session_type,
                                  const proto::DesktopSessionConfig& config)
{
    session_type_ = session_type;
    config_.CopyFrom(config);
    return DoModal(parent);
}

INT_PTR UiSettingsDialog::DoModal(HWND parent)
{
    return Run(UiModule::Current(), parent, IDD_SETTINGS);
}

void UiSettingsDialog::InitColorDepthList()
{
    HWND combo = GetDlgItem(IDC_COLOR_DEPTH_COMBO);

    ComboBox_AddItem(combo, module().string(IDS_32BIT), kARGB);
    ComboBox_AddItem(combo, module().string(IDS_24BIT), kRGB888);
    ComboBox_AddItem(combo, module().string(IDS_16BIT), kRGB565);
    ComboBox_AddItem(combo, module().string(IDS_15BIT), kRGB555);
    ComboBox_AddItem(combo, module().string(IDS_12BIT), kRGB444);
    ComboBox_AddItem(combo, module().string(IDS_8BIT),  kRGB332);
    ComboBox_AddItem(combo, module().string(IDS_6BIT),  kRGB222);
    ComboBox_AddItem(combo, module().string(IDS_3BIT),  kRGB111);

    PixelFormat format(ConvertFromVideoPixelFormat(config_.pixel_format()));

    PixelFormats curr_item = kRGB565;

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

    ComboBox_SelItemWithData(combo, curr_item);
}

void UiSettingsDialog::InitCodecList()
{
    HWND combo = GetDlgItem(IDC_CODEC_COMBO);

    ComboBox_AddItem(combo, L"VP9", proto::VideoEncoding::VIDEO_ENCODING_VP9);
    ComboBox_AddItem(combo, L"VP8", proto::VideoEncoding::VIDEO_ENCODING_VP8);
    ComboBox_AddItem(combo, L"ZLIB", proto::VideoEncoding::VIDEO_ENCODING_ZLIB);

    ComboBox_SelItemWithData(combo, config_.video_encoding());
}

void UiSettingsDialog::UpdateCompressionRatio(int compression_ratio)
{
    std::wstring format = module().string(IDS_COMPRESSION_RATIO_FORMAT);

    SetDlgItemString(IDC_COMPRESS_RATIO_TEXT,
                     StringPrintfW(format.c_str(), compression_ratio));
}

void UiSettingsDialog::OnInitDialog()
{
    InitCodecList();
    InitColorDepthList();
    UpdateCompressionRatio(config_.compress_ratio());
    OnCodecChanged();

    HWND ratio = GetDlgItem(IDC_COMPRESS_RATIO_TRACKBAR);
    TrackBar_SetRange(ratio, kMinCompressRatio, kMaxCompressRatio);
    TrackBar_SetPos(ratio, config_.compress_ratio());

    SendMessageW(GetDlgItem(IDC_INTERVAL_EDIT), EM_SETLIMITTEXT, 3, 0);

    HWND updown = GetDlgItem(IDC_INTERVAL_UPDOWN);
    UpDown_SetRange(updown, kMinUpdateInterval, kMaxUpdateInterval);
    UpDown_SetPos(updown, config_.update_interval());

    if (session_type_ == proto::SessionType::SESSION_TYPE_DESKTOP_VIEW)
    {
        EnableDlgItem(IDC_ENABLE_CLIPBOARD_CHECK, FALSE);
        EnableDlgItem(IDC_ENABLE_CURSOR_SHAPE_CHECK, FALSE);
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

void UiSettingsDialog::OnHScroll(HWND ctrl)
{
    if (GetWindowLongPtrW(ctrl, GWLP_ID) == IDC_COMPRESS_RATIO_TRACKBAR)
    {
        int compression_ratio =
            TrackBar_GetPos(GetDlgItem(IDC_COMPRESS_RATIO_TRACKBAR));

        UpdateCompressionRatio(compression_ratio);
    }
}

void UiSettingsDialog::OnCodecChanged()
{
    bool has_pixel_format =
        (ComboBox_CurItemData(GetDlgItem(IDC_CODEC_COMBO)) ==
            proto::VIDEO_ENCODING_ZLIB);

    EnableDlgItem(IDC_COLOR_DEPTH_TEXT, has_pixel_format);
    EnableDlgItem(IDC_COLOR_DEPTH_COMBO, has_pixel_format);
    EnableDlgItem(IDC_COMPRESS_RATIO_TEXT, has_pixel_format);
    EnableDlgItem(IDC_COMPRESS_RATIO_TRACKBAR, has_pixel_format);
    EnableDlgItem(IDC_FAST_TEXT, has_pixel_format);
    EnableDlgItem(IDC_BEST_TEXT, has_pixel_format);
}

void UiSettingsDialog::OnCodecChanged(WORD notify_code)
{
    if (notify_code == CBN_SELCHANGE)
        OnCodecChanged();
}

void UiSettingsDialog::OnOkButton()
{
    proto::VideoEncoding encoding =
        static_cast<proto::VideoEncoding>(
            ComboBox_CurItemData(GetDlgItem(IDC_CODEC_COMBO)));

    config_.set_video_encoding(encoding);

    if (encoding == proto::VIDEO_ENCODING_ZLIB)
    {
        PixelFormat format;

        switch (ComboBox_CurItemData(GetDlgItem(IDC_COLOR_DEPTH_COMBO)))
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
            TrackBar_GetPos(GetDlgItem(IDC_COMPRESS_RATIO_TRACKBAR));

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

    DWORD ret = UpDown_GetPos(GetDlgItem(IDC_INTERVAL_UPDOWN));
    if (HIWORD(ret) == 0)
    {
        int interval = LOWORD(ret);

        if (interval >= kMinUpdateInterval && interval <= kMaxUpdateInterval)
        {
            config_.set_update_interval(interval);
        }
    }

    EndDialog(IDOK);
}

INT_PTR UiSettingsDialog::OnMessage(UINT msg, WPARAM wparam, LPARAM lparam)
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
