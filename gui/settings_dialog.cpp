//
// PROJECT:         Aspia Remote Desktop
// FILE:            gui/settings_dialog.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "gui/settings_dialog.h"

namespace aspia {

void SettingsDialog::InitColorDepthList()
{
    CComboBox combo(GetDlgItem(IDC_COLOR_DEPTH_COMBO));

    AddColorDepth(combo, kARGB,   IDS_32BIT);
    AddColorDepth(combo, kRGB888, IDS_24BIT);
    AddColorDepth(combo, kRGB565, IDS_16BIT);
    AddColorDepth(combo, kRGB555, IDS_15BIT);
    AddColorDepth(combo, kRGB444, IDS_12BIT);
    AddColorDepth(combo, kRGB332, IDS_8BIT);
    AddColorDepth(combo, kRGB222, IDS_6BIT);
    AddColorDepth(combo, kRGB111, IDS_3BIT);

    const PixelFormat &format = config_.Format();

    if (format.IsEqualTo(PixelFormat::ARGB()))
    {
        combo.SetCurSel(kARGB);
    }
    else if (format.IsEqualTo(PixelFormat::RGB888()))
    {
        combo.SetCurSel(kRGB888);
    }
    else if (format.IsEqualTo(PixelFormat::RGB565()))
    {
        combo.SetCurSel(kRGB565);
    }
    else if (format.IsEqualTo(PixelFormat::RGB555()))
    {
        combo.SetCurSel(kRGB555);
    }
    else if (format.IsEqualTo(PixelFormat::RGB444()))
    {
        combo.SetCurSel(kRGB444);
    }
    else if (format.IsEqualTo(PixelFormat::RGB332()))
    {
        combo.SetCurSel(kRGB332);
    }
    else if (format.IsEqualTo(PixelFormat::RGB222()))
    {
        combo.SetCurSel(kRGB222);
    }
    else if (format.IsEqualTo(PixelFormat::RGB111()))
    {
        combo.SetCurSel(kRGB111);
    }
    else
    {
        combo.SetCurSel(kRGB565);
    }
}

void SettingsDialog::InitCodecList()
{
    CComboBox combo(GetDlgItem(IDC_CODEC_COMBO));

    combo.InsertString(kVP9, L"VP9");
    combo.InsertString(kVP8, L"VP8");
    combo.InsertString(kZLIB, L"ZLIB");

    switch (config_.Encoding())
    {
        case proto::VideoEncoding::VIDEO_ENCODING_VP8:
            combo.SetCurSel(kVP8);
            break;

        case proto::VideoEncoding::VIDEO_ENCODING_VP9:
            combo.SetCurSel(kVP9);
            break;

        case proto::VideoEncoding::VIDEO_ENCODING_ZLIB:
            combo.SetCurSel(kZLIB);
            break;
    }
}

BOOL SettingsDialog::OnInitDialog(CWindow focus_window, LPARAM lParam)
{
    config_ = *reinterpret_cast<ScreenConfig*>(lParam);

    CenterWindow();

    InitCodecList();
    InitColorDepthList();

    DoCodecChanged();

    CString ratio_title;
    ratio_title.Format(IDS_COMPRESSION_RATIO_FORMAT, config_.CompressRatio());

    SetDlgItemTextW(IDC_COMPRESS_RATIO_TEXT, ratio_title);

    CTrackBarCtrl ratio(GetDlgItem(IDC_COMPRESS_RATIO_TRACKBAR));

    ratio.SetRange(kMinCompressRatio, kMaxCompressRatio);
    ratio.SetPos(config_.CompressRatio());

    CEdit interval(GetDlgItem(IDC_INTERVAL_EDIT));
    interval.SetLimitText(3);

    CUpDownCtrl updown(GetDlgItem(IDC_INTERVAL_UPDOWN));
    updown.SetRange(kMinUpdateInterval, kMaxUpdateInterval);
    updown.SetPos(config_.UpdateInterval());

    CheckDlgButton(IDC_DESKTOP_EFFECTS_CHECK,
                   config_.DisableDesktopEffects() ? BST_CHECKED : BST_UNCHECKED);

    CheckDlgButton(IDC_REMOTE_CURSOR_CHECK,
                   config_.ShowRemoteCursor() ? BST_CHECKED : BST_UNCHECKED);

    CheckDlgButton(IDC_AUTOSEND_CLIPBOARD_CHECK,
                   config_.AutoSendClipboard() ? BST_CHECKED : BST_UNCHECKED);

    return FALSE;
}

void SettingsDialog::OnClose()
{
    EndDialog(0);
}

void SettingsDialog::OnHScroll(UINT code, UINT pos, HWND ctrl)
{
    switch (CWindow(ctrl).GetWindowLongPtrW(GWLP_ID))
    {
        case IDC_COMPRESS_RATIO_TRACKBAR:
        {
            CTrackBarCtrl trackbar(ctrl);

            CString ratio;
            ratio.Format(IDS_COMPRESSION_RATIO_FORMAT, trackbar.GetPos());

            SetDlgItemTextW(IDC_COMPRESS_RATIO_TEXT, ratio);
        }
        break;
    }
}

void SettingsDialog::DoCodecChanged()
{
    BOOL has_pixel_format = (CComboBox(GetDlgItem(IDC_CODEC_COMBO)).GetCurSel() == kZLIB);

    GetDlgItem(IDC_COLOR_DEPTH_TEXT).EnableWindow(has_pixel_format);
    GetDlgItem(IDC_COLOR_DEPTH_COMBO).EnableWindow(has_pixel_format);
    GetDlgItem(IDC_COMPRESS_RATIO_TEXT).EnableWindow(has_pixel_format);
    GetDlgItem(IDC_COMPRESS_RATIO_TRACKBAR).EnableWindow(has_pixel_format);
    GetDlgItem(IDC_FAST_TEXT).EnableWindow(has_pixel_format);
    GetDlgItem(IDC_BEST_TEXT).EnableWindow(has_pixel_format);
}

LRESULT SettingsDialog::OnCodecChanged(WORD notify_code, WORD id, HWND ctrl, BOOL &handled)
{
    if (notify_code == CBN_SELCHANGE)
    {
        DoCodecChanged();
    }

    return 0;
}

LRESULT SettingsDialog::OnOkButton(WORD notify_code, WORD id, HWND ctrl, BOOL &handled)
{
    int codec = CComboBox(GetDlgItem(IDC_CODEC_COMBO)).GetCurSel();

    switch (codec)
    {
        case kVP8:  config_.SetEncoding(proto::VideoEncoding::VIDEO_ENCODING_VP8);  break;
        case kVP9:  config_.SetEncoding(proto::VideoEncoding::VIDEO_ENCODING_VP9);  break;
        case kZLIB: config_.SetEncoding(proto::VideoEncoding::VIDEO_ENCODING_ZLIB); break;
    }

    if (codec == kZLIB)
    {
        switch (CComboBox(GetDlgItem(IDC_COLOR_DEPTH_COMBO)).GetCurSel())
        {
            case kARGB:   config_.SetFormat(PixelFormat::ARGB());   break;
            case kRGB888: config_.SetFormat(PixelFormat::RGB888()); break;
            case kRGB565: config_.SetFormat(PixelFormat::RGB565()); break;
            case kRGB555: config_.SetFormat(PixelFormat::RGB555()); break;
            case kRGB444: config_.SetFormat(PixelFormat::RGB444()); break;
            case kRGB332: config_.SetFormat(PixelFormat::RGB332()); break;
            case kRGB222: config_.SetFormat(PixelFormat::RGB222()); break;
            case kRGB111: config_.SetFormat(PixelFormat::RGB111()); break;
        }

        int32_t compress_ratio = CTrackBarCtrl(GetDlgItem(IDC_COMPRESS_RATIO_TRACKBAR)).GetPos();
        if (compress_ratio >= kMinCompressRatio && compress_ratio <= kMaxCompressRatio)
        {
            config_.SetCompressRatio(compress_ratio);
        }
    }

    bool value = IsDlgButtonChecked(IDC_DESKTOP_EFFECTS_CHECK) == BST_CHECKED;
    config_.SetDisableDesktopEffects(value);

    value = IsDlgButtonChecked(IDC_REMOTE_CURSOR_CHECK) == BST_CHECKED;
    config_.SetShowRemoteCursor(value);

    value = IsDlgButtonChecked(IDC_AUTOSEND_CLIPBOARD_CHECK) == BST_CHECKED;
    config_.SetAutoSendClipboard(value);

    int32_t interval = CUpDownCtrl(GetDlgItem(IDC_INTERVAL_UPDOWN)).GetPos();
    if (interval >= kMinUpdateInterval && interval <= kMaxUpdateInterval)
    {
        config_.SetUpdateInterval(interval);
    }

    PostMessageW(WM_CLOSE);
    return 0;
}

LRESULT SettingsDialog::OnCancelButton(WORD notify_code, WORD id, HWND ctrl, BOOL &handled)
{
    PostMessageW(WM_CLOSE);
    return 0;
}

void SettingsDialog::AddColorDepth(CComboBox &combobox, int index, UINT string_id)
{
    CString title;
    title.LoadStringW(string_id);

    combobox.InsertString(index, title);
}

} // namespace aspia
