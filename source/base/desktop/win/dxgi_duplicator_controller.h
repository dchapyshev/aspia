//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#ifndef BASE_DESKTOP_WIN_DXGI_DUPLICATOR_CONTROLLER_H
#define BASE_DESKTOP_WIN_DXGI_DUPLICATOR_CONTROLLER_H

#include "base/desktop/win/d3d_device.h"
#include "base/desktop/win/display_configuration_monitor.h"
#include "base/desktop/win/dxgi_adapter_duplicator.h"
#include "base/desktop/win/dxgi_context.h"
#include "base/desktop/win/dxgi_frame.h"

#include <d3dcommon.h>

namespace base {

// A controller for all the objects we need to call Windows DirectX capture APIs It's a singleton
// because only one IDXGIOutputDuplication instance per monitor is allowed per application.
//
// Consumers should create a DxgiDuplicatorController::Context and keep it throughout their
// lifetime, and pass it when calling duplicate(). Consumers can also call isSupported() to
// determine whether the system supports DXGI duplicator or not. If a previous isSupported()
// function call returns true, but a later duplicate() returns false, this usually means the
// display mode is changing. Consumers should retry after a while. (Typically 50 milliseconds,
// but according to hardware performance, this time may vary.)
class DxgiDuplicatorController
{
public:
    DxgiDuplicatorController();
    ~DxgiDuplicatorController();

    using Context = DxgiFrameContext;
    using ErrorCode = DxgiAdapterDuplicator::ErrorCode;

    // A collection of D3d information we are interested on, which may impact capturer performance
    // or reliability.
    struct D3dInfo
    {
        // Each video adapter has its own D3D_FEATURE_LEVEL, so this structure contains the minimum
        // and maximium D3D_FEATURE_LEVELs current system supports.
        // Both fields can be 0, which is the default value to indicate no valid D3D_FEATURE_LEVEL
        // has been retrieved from underlying OS APIs.
        D3D_FEATURE_LEVEL min_feature_level;
        D3D_FEATURE_LEVEL max_feature_level;

        // TODO(zijiehe): Add more fields, such as manufacturer name, mode, driver version.
    };

    enum class Result
    {
        SUCCEEDED,
        UNSUPPORTED_SESSION,
        FRAME_PREPARE_FAILED,
        INITIALIZATION_FAILED,
        DUPLICATION_FAILED,
        INVALID_MONITOR_ID,
    };

    // Converts |result| into user-friendly string representation. The return value should not be
    // used to identify error types.
    static const char* resultName(Result result);

    // See ScreenCapturerWinDirectx::isCurrentSessionSupported().
    static bool isCurrentSessionSupported();

    // All the following public functions implicitly call initialize() function.

    // Detects whether the system supports DXGI based capturer.
    bool isSupported();

    // Returns a copy of D3dInfo composed by last Initialize() function call. This function always
    // copies the latest information into |info|. But once the function returns false, the
    // information in |info| may not accurate.
    bool retrieveD3dInfo(D3dInfo* info);

    // Captures current screen and writes into |frame|.
    // TODO(zijiehe): Windows cannot guarantee the frames returned by each IDXGIOutputDuplication
    // are synchronized. But we are using a totally different threading model than the way Windows
    // suggested, it's hard to synchronize them manually. We should find a way to do it.
    Result duplicate(DxgiFrame* frame, DxgiCursor* cursor);

    // Captures one monitor and writes into target. |monitor_id| should >= 0. If |monitor_id| is
    // greater than the total screen count of all the Duplicators, this function returns false.
    Result duplicateMonitor(DxgiFrame* frame, DxgiCursor* cursor, int monitor_id);

    // Returns the count of screens on the system. These screens can be retrieved by an integer in
    // the range of [0, screenCount()). If system does not support DXGI based capturer, this
    // function returns 0.
    int screenCount();

    // Returns the device names of all screens on the system. These screens can be retrieved by an
    // integer in the range of [0, output->size()). If system does not support DXGI based capturer,
    // this function returns false.
    bool deviceNames(QStringList* output);

private:
    // DxgiFrameContext calls private unregister(Context*) function in reset().
    friend void DxgiFrameContext::reset();

    // Does the real duplication work. Setting |monitor_id| < 0 to capture entire screen. This
    // function calls initialize(). And if the duplication failed, this function calls
    // deinitialize() to ensure the Dxgi components can be reinitialized next time.
    Result doDuplicate(DxgiFrame* frame, DxgiCursor* cursor, int monitor_id);

    // Unregisters Context from this instance and all DxgiAdapterDuplicator(s) it owns.
    void unregister(const Context* const context);

    // All functions below should be called should be after a successful initialize().

    // If current instance has not been initialized, executes doInitialize() function, and returns
    // initialize result. Otherwise directly returns true.
    // This function may calls deinitialize() if initialization failed.
    bool initialize();

    // Does the real initialization work, this function should only be called in initialize().
    bool doInitialize();

    // Clears all COM components referred by this instance. So next duplicate() call will
    // eventually initialize this instance again.
    void deinitialize();

    // A helper function to check whether a Context has been expired.
    bool contextExpired(const Context* const context) const;

    // Updates Context if needed.
    void setup(Context* context);

    // Captures all monitors.
    bool doDuplicateAll(Context* context, SharedFrame* target, DxgiCursor* cursor);

    // Captures one monitor.
    bool doDuplicateOne(Context* context, int monitor_id, SharedFrame* target, DxgiCursor* cursor);

    // The minimum numFramesCaptured() returned by |duplicators_|.
    qint64 numFramesCaptured() const;

    // Returns a Size to cover entire |desktop_rect_|.
    Size desktopSize() const;

    // Returns the size of one screen. |id| should be >= 0. If system does not support DXGI based
    // capturer, or |id| is greater than the total screen count of all the Duplicators, this
    // function returns an empty Rect.
    Rect screenRect(int id) const;

    int doScreenCount() const;

    // Returns the desktop size of the selected screen |monitor_id|. Setting |monitor_id| < 0 to
    // return the entire screen size.
    Size selectedDesktopSize(int monitor_id) const;

    // Retries doDuplicateAll() for several times until numFramesCaptured() is large enough.
    // Returns false if doDuplicateAll() returns false, or numFramesCaptured() has never reached
    // the requirement.
    // According to http://crbug.com/682112, dxgi capturer returns a black frame during first
    // several capture attempts.
    bool ensureFrameCaptured(Context* context, SharedFrame* target, DxgiCursor* cursor);

    // Moves |desktop_rect_| and all underlying |duplicators_|, putting top left corner of the
    // desktop at (0, 0). This is necessary because DXGI_OUTPUT_DESC may return negative
    // coordinates. Called from doInitialize() after all DxgiAdapterDuplicator and
    // DxgiOutputDuplicator instances are initialized.
    void translateRect();

    // A self-incremented integer to compare with the one in Context. It ensures a Context instance
    // is always initialized after DxgiDuplicatorController.
    int identity_ = 0;
    Rect desktop_rect_;
    std::vector<DxgiAdapterDuplicator> duplicators_;
    D3dInfo d3d_info_;
    DisplayConfigurationMonitor display_configuration_monitor_;
    // A number to indicate how many succeeded duplications have been performed.
    quint32 succeeded_duplications_ = 0;
};

} // namespace base

#endif // BASE_DESKTOP_WIN_DXGI_DUPLICATOR_CONTROLLER_H
