//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/settings_dialog.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/settings_dialog.h"
#include "desktop_capture/pixel_format.h"
#include "codec/video_helpers.h"
#include "base/strings/string_util.h"

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

UiSettingsDialog::UiSettingsDialog(proto::SessionType session_type,
                                   const proto::DesktopSessionConfig& config) :
    session_type_(session_type),
    config_(config)
{
    // Nothing
}

void UiSettingsDialog::AddColorDepth(CComboBox& combobox, UINT string_id, int item_data)
{
    CString text;
    text.LoadStringW(string_id);

    int item_index = combobox.AddString(text);
    combobox.SetItemData(item_index, item_data);
}

void UiSettingsDialog::SelectItemWithData(CComboBox& combobox, int item_data)
{
    int count = combobox.GetCount();

    for (int i = 0; i < count; ++i)
    {
        LPARAM cur_item_data = combobox.GetItemData(i);

        if (cur_item_data == item_data)
        {
            combobox.SetCurSel(i);
            return;
        }
    }
}

void UiSettingsDialog::InitColorDepthList()
{
    CComboBox combo(GetDlgItem(IDC_COLOR_DEPTH_COMBO));

    AddColorDepth(combo, IDS_DM_32BIT, kARGB);
    AddColorDepth(combo, IDS_DM_24BIT, kRGB888);
    AddColorDepth(combo, IDS_DM_16BIT, kRGB565);
    AddColorDepth(combo, IDS_DM_15BIT, kRGB555);
    AddColorDepth(combo, IDS_DM_12BIT, kRGB444);
    AddColorDepth(combo, IDS_DM_8BIT,  kRGB332);
    AddColorDepth(combo, IDS_DM_6BIT,  kRGB222);
    AddColorDepth(combo, IDS_DM_3BIT,  kRGB111);

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

    SelectItemWithData(combo, curr_item);
}

void UiSettingsDialog::InitCodecList()
{
    CComboBox combo(GetDlgItem(IDC_CODEC_COMBO));

    int item_index = combo.AddString(L"VP9");
    combo.SetItemData(item_index, proto::VideoEncoding::VIDEO_ENCODING_VP9);

    item_index = combo.AddString(L"VP8");
    combo.SetItemData(item_index, proto::VideoEncoding::VIDEO_ENCODING_VP8);

    item_index = combo.AddString(L"ZLIB");
    combo.SetItemData(item_index, proto::VideoEncoding::VIDEO_ENCODING_ZLIB);

    SelectItemWithData(combo, config_.video_encoding());
}

void UiSettingsDialog::UpdateCompressionRatio(int compression_ratio)
{
    CString text;
    text.Format(IDS_DM_COMPRESSION_RATIO_FORMAT, compression_ratio);
    SetDlgItemTextW(IDC_COMPRESS_RATIO_TEXT, text);
}

LRESULT UiSettingsDialog::OnInitDialog(UINT message,
                                       WPARAM wparam,
                                       LPARAM lparam,
                                       BOOL& handled)
{
    InitCodecList();
    InitColorDepthList();
    UpdateCompressionRatio(config_.compress_ratio());
    OnCodecChanged();

    CTrackBarCtrl ratio(GetDlgItem(IDC_COMPRESS_RATIO_TRACKBAR));
    ratio.SetRange(kMinCompressRatio, kMaxCompressRatio);
    ratio.SetPos(config_.compress_ratio());

    CEdit(GetDlgItem(IDC_INTERVAL_EDIT)).SetLimitText(3);

    CUpDownCtrl updown(GetDlgItem(IDC_INTERVAL_UPDOWN));
    updown.SetRange(kMinUpdateInterval, kMaxUpdateInterval);
    updown.SetPos(config_.update_interval());

    if (session_type_ == proto::SessionType::SESSION_TYPE_DESKTOP_VIEW)
    {
        GetDlgItem(IDC_ENABLE_CLIPBOARD_CHECK).EnableWindow(FALSE);
        GetDlgItem(IDC_ENABLE_CURSOR_SHAPE_CHECK).EnableWindow(FALSE);
    }
    else
    {
        CheckDlgButton(IDC_ENABLE_CURSOR_SHAPE_CHECK,
            (config_.flags() & proto::DesktopSessionConfig::ENABLE_CURSOR_SHAPE) ?
                BST_CHECKED : BST_UNCHECKED);

        CheckDlgButton(IDC_ENABLE_CLIPBOARD_CHECK,
            (config_.flags() & proto::DesktopSessionConfig::ENABLE_CLIPBOARD) ?
                BST_CHECKED : BST_UNCHECKED);
    }

    return TRUE;
}

LRESULT UiSettingsDialog::OnClose(UINT message,
                                  WPARAM wparam,
                                  LPARAM lparam,
                                  BOOL& handled)
{
    EndDialog(IDCANCEL);
    return 0;
}

LRESULT UiSettingsDialog::OnHScroll(UINT message,
                                    WPARAM wparam,
                                    LPARAM lparam,
                                    BOOL& handled)
{
    CWindow control(reinterpret_cast<HWND>(lparam));

    if (control.GetWindowLongPtrW(GWLP_ID) == IDC_COMPRESS_RATIO_TRACKBAR)
    {
        CTrackBarCtrl ratio(control);
        UpdateCompressionRatio(ratio.GetPos());
    }

    return 0;
}

void UiSettingsDialog::OnCodecChanged()
{
    CComboBox codec_combo(GetDlgItem(IDC_CODEC_COMBO));

    BOOL has_pixel_format =
        (codec_combo.GetItemData(codec_combo.GetCurSel()) ==
            proto::VIDEO_ENCODING_ZLIB);

    GetDlgItem(IDC_COLOR_DEPTH_TEXT).EnableWindow(has_pixel_format);
    GetDlgItem(IDC_COLOR_DEPTH_COMBO).EnableWindow(has_pixel_format);
    GetDlgItem(IDC_COMPRESS_RATIO_TEXT).EnableWindow(has_pixel_format);
    GetDlgItem(IDC_COMPRESS_RATIO_TRACKBAR).EnableWindow(has_pixel_format);
    GetDlgItem(IDC_FAST_TEXT).EnableWindow(has_pixel_format);
    GetDlgItem(IDC_BEST_TEXT).EnableWindow(has_pixel_format);
}

LRESULT UiSettingsDialog::OnCodecListChanged(WORD notify_code,
                                             WORD control_id,
                                             HWND control,
                                             BOOL& handled)
{
    OnCodecChanged();
    return 0;
}

LRESULT UiSettingsDialog::OnOkButton(WORD notify_code,
                                     WORD control_id,
                                     HWND control,
                                     BOOL& handled)
{
    CComboBox codec_combo(GetDlgItem(IDC_CODEC_COMBO));

    proto::VideoEncoding encoding =
        static_cast<proto::VideoEncoding>(
            codec_combo.GetItemData(codec_combo.GetCurSel()));

    config_.set_video_encoding(encoding);

    if (encoding == proto::VIDEO_ENCODING_ZLIB)
    {
        CComboBox color_combo(GetDlgItem(IDC_COLOR_DEPTH_COMBO));
        PixelFormat format;

        switch (color_combo.GetItemData(color_combo.GetCurSel()))
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
            CTrackBarCtrl(GetDlgItem(IDC_COMPRESS_RATIO_TRACKBAR)).GetPos();

        if (compress_ratio >= kMinCompressRatio && compress_ratio <= kMaxCompressRatio)
        {
            config_.set_compress_ratio(compress_ratio);
        }
    }

    uint32_t flags = 0;

    if (IsDlgButtonChecked(IDC_ENABLE_CURSOR_SHAPE_CHECK) == BST_CHECKED)
        flags |= proto::DesktopSessionConfig::ENABLE_CURSOR_SHAPE;

    if (IsDlgButtonChecked(IDC_ENABLE_CLIPBOARD_CHECK) == BST_CHECKED)
        flags |= proto::DesktopSessionConfig::ENABLE_CLIPBOARD;

    config_.set_flags(flags);

    DWORD ret = CUpDownCtrl(GetDlgItem(IDC_INTERVAL_UPDOWN)).GetPos();
    if (HIWORD(ret) == 0)
    {
        int interval = LOWORD(ret);

        if (interval >= kMinUpdateInterval && interval <= kMaxUpdateInterval)
        {
            config_.set_update_interval(interval);
        }
    }

    EndDialog(IDOK);
    return 0;
}

LRESULT UiSettingsDialog::OnCancelButton(WORD notify_code,
                                         WORD control_id,
                                         HWND control,
                                         BOOL& handled)
{
    EndDialog(IDCANCEL);
    return 0;
}

} // namespace aspia
