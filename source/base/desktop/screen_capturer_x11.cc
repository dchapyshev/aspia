//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/desktop/screen_capturer_x11.h"

#include "base/logging.h"
#include "base/desktop/mouse_cursor.h"
#include "base/desktop/shared_memory_frame.h"
#include "base/desktop/x11/x_error_trap.h"

#include <dlfcn.h>

namespace base {

//--------------------------------------------------------------------------------------------------
ScreenCapturerX11::ScreenCapturerX11(QObject* parent)
    : ScreenCapturer(ScreenCapturer::Type::LINUX_X11, parent)
{
    LOG(INFO) << "Ctor";
    helper_.setLogGridSize(4);
}

//--------------------------------------------------------------------------------------------------
ScreenCapturerX11::~ScreenCapturerX11()
{
    LOG(INFO) << "Dtor";

    display_->removeEventHandler(ConfigureNotify, this);

    if (use_damage_)
        display_->removeEventHandler(damage_event_base_ + XDamageNotify, this);

    if (use_randr_)
        display_->removeEventHandler(randr_event_base_ + RRScreenChangeNotify, this);

    deinitXlib();
}

//--------------------------------------------------------------------------------------------------
// static
ScreenCapturerX11* ScreenCapturerX11::create(QObject* parent)
{
    std::unique_ptr<ScreenCapturerX11> instance = std::make_unique<ScreenCapturerX11>(parent);
    if (!instance->init())
    {
        LOG(ERROR) << "Unable to initialize X11 screen capturer";
        return nullptr;
    }

    return instance.release();
}

//--------------------------------------------------------------------------------------------------
int ScreenCapturerX11::screenCount()
{
    return num_monitors_;
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerX11::screenList(ScreenList* screens)
{
    DCHECK(screens->screens.size() == 0);

    if (!use_randr_)
    {
        LOG(INFO) << "No screen list";
        screens->screens.push_back({});
        return true;
    }

    // Ensure that |monitors_| is updated with changes that may have happened
    // between calls to GetSourceList().
    display_->processPendingXEvents();

    for (int i = 0; i < num_monitors_; ++i)
    {
        XRRMonitorInfo& m = monitors_[i];

        char* monitor_title = XGetAtomName(display(), m.name);
        QPoint position(m.x, m.y);
        QSize resolution(m.width, m.height);
        QPoint dpi(96, 96);
        bool primary = m.primary != 0;

        QString title;
        if (monitor_title)
            title = QString::fromLocal8Bit(monitor_title);
        else
            title = "Unknown monitor";

        // Note name is an X11 Atom used to id the monitor.
        screens->screens.emplace_back(
            static_cast<ScreenId>(m.name), title, position, resolution, dpi, primary);
        XFree(monitor_title);
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerX11::selectScreen(ScreenId screen_id)
{
    // Prevent the reuse of any frame buffers allocated for a previously selected source. This is
    // required to stop crashes, or old data from appearing in a captured frame, when the new source
    // is sized differently then the source that was selected at the time a reused frame buffer was
    // created.
    queue_.reset();

    if (!use_randr_ || screen_id == kFullDesktopScreenId)
    {
        selected_monitor_name_ = kFullDesktopScreenId;
        selected_monitor_rect_ = QRect(QPoint(0, 0), x_server_pixel_buffer_.windowSize());
        return true;
    }

    for (int i = 0; i < num_monitors_; ++i)
    {
        if (screen_id == static_cast<ScreenId>(monitors_[i].name))
        {
            LOG(INFO) << "XRandR selected source:" << screen_id;

            XRRMonitorInfo& m = monitors_[i];
            selected_monitor_name_ = m.name;
            selected_monitor_rect_ = QRect(QPoint(m.x, m.y), QSize(m.width, m.height));

            return true;
        }
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
ScreenCapturer::ScreenId ScreenCapturerX11::currentScreen() const
{
    return selected_monitor_name_;
}

//--------------------------------------------------------------------------------------------------
const Frame* ScreenCapturerX11::captureFrame(Error* error)
{
    // Process XEvents for XDamage and cursor shape tracking.
    display_->processPendingXEvents();

    // processPendingXEvents() may call screenConfigurationChanged() which reinitializes
    // |m_xServerPixelBuffer|. Check if the pixel buffer is still in a good shape.
    if (!x_server_pixel_buffer_.isInitialized())
    {
        // We failed to initialize pixel buffer.
        LOG(ERROR) << "Pixel buffer is not initialized";
        *error = Error::PERMANENT;
        return nullptr;
    }

    if (!queue_.currentFrame())
    {
        std::unique_ptr<Frame> frame = SharedMemoryFrame::create(
            selected_monitor_rect_.size(), PixelFormat::ARGB(), sharedMemoryFactory());
        if (!frame)
        {
            LOG(ERROR) << "Unable to create frame";
            return nullptr;
        }

        // We set the top-left of the frame so the mouse cursor will be composited
        // properly, and our frame buffer will not be overrun while blitting.
        frame->setTopLeft(selected_monitor_rect_.topLeft());
        queue_.replaceCurrentFrame(std::move(frame));
    }

    Frame* result = captureFrameImpl();
    if (!result)
    {
        LOG(ERROR) << "Temporarily failed to capture screen";
        *error = Error::PERMANENT;
        return nullptr;
    }

    QRegion* updated_region = result->updatedRegion();
    last_invalid_region_ = *updated_region;
    updated_region->translate(-selected_monitor_rect_.left(), -selected_monitor_rect_.top());

    *error = Error::SUCCEEDED;
    return result;
}

//--------------------------------------------------------------------------------------------------
const MouseCursor* ScreenCapturerX11::captureCursor()
{
    if (!has_xfixes_)
        return nullptr;

    XFixesCursorImage* x_image = nullptr;
    {
        XErrorTrap error_trap(display());
        x_image = XFixesGetCursorImage(display());
        if (!x_image || error_trap.lastErrorAndDisable() != 0)
        {
            LOG(ERROR) << "XFixesGetCursorImage failed";
            return nullptr;
        }
    }

    QSize size(x_image->width, x_image->height);
    QPoint hotspot(std::min(x_image->xhot, x_image->width),
                   std::min(x_image->yhot, x_image->height));

    if (size.width() <= 0 || size.height() <= 0)
    {
        LOG(ERROR) << "Invalid cursor size:" << size;
        return nullptr;
    }

    size_t image_size = x_image->width * x_image->height;

    QByteArray image_data;
    image_data.resize(image_size * MouseCursor::kBytesPerPixel);

    unsigned long* src = x_image->pixels;
    uint32_t* dst = reinterpret_cast<uint32_t*>(image_data.data());
    uint32_t* dst_end = dst + image_size;

    while (dst < dst_end)
        *dst++ = static_cast<uint32_t>(*src++);

    XFree(x_image);

    mouse_cursor_ = std::make_unique<MouseCursor>(std::move(image_data), size, hotspot);
    return mouse_cursor_.get();
}

//--------------------------------------------------------------------------------------------------
QPoint ScreenCapturerX11::cursorPosition()
{
    Window root_window;
    Window child_window;
    int root_x;
    int root_y;
    int win_x;
    int win_y;
    unsigned int mask;

    XErrorTrap errorTrap(display_->display());
    X11_Bool result = XQueryPointer(display_->display(),
                                    root_window_,
                                    &root_window,
                                    &child_window,
                                    &root_x, &root_y,
                                    &win_x, &win_y,
                                    &mask);
    if (!result || errorTrap.lastErrorAndDisable() != 0)
    {
        LOG(ERROR) << "XQueryPointer failed";
        return QPoint(0, 0);
    }

    return QPoint(root_x, root_y) - selected_monitor_rect_.topLeft();
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerX11::reset()
{
    queue_.reset();
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerX11::init()
{
    display_ = SharedXDisplay::createDefault();
    if (!display_)
    {
        LOG(ERROR) << "SharedXDisplay::createDefault failed";
        return false;
    }

    atom_cache_ = std::make_unique<XAtomCache>(display());

    root_window_ = RootWindow(display(), DefaultScreen(display()));
    if (root_window_ == BadValue)
    {
        LOG(ERROR) << "Unable to get the root window";
        deinitXlib();
        return false;
    }

    gc_ = XCreateGC(display(), root_window_, 0, nullptr);
    if (gc_ == nullptr)
    {
        LOG(ERROR) << "Unable to get graphics context";
        deinitXlib();
        return false;
    }

    display_->addEventHandler(ConfigureNotify, this);

    // Check for XFixes extension. This is required for cursor shape notifications, and for our use
    // of XDamage.
    if (XFixesQueryExtension(display(), &xfixes_event_base_, &xfixes_error_base_))
    {
        LOG(INFO) << "X server supports XFixes";
        has_xfixes_ = true;
    }
    else
    {
        LOG(INFO) << "X server does not support XFixes";
    }

    // Register for changes to the dimensions of the root window.
    XSelectInput(display(), root_window_, StructureNotifyMask);

    if (has_xfixes_)
    {
        // Register for changes to the cursor shape.
        XFixesSelectCursorInput(display(), root_window_, XFixesDisplayCursorNotifyMask);
        display_->addEventHandler(xfixes_event_base_ + XFixesCursorNotify, this);
        captureCursor();
    }

    if (!x_server_pixel_buffer_.init(atom_cache_.get(), DefaultRootWindow(display())))
    {
        LOG(ERROR) << "Failed to initialize pixel buffer";
        return false;
    }

    initXDamage();
    initXrandr();

    // Default source set here so that selected_monitor_rect_ is sized correctly.
    if (!selectScreen(kFullDesktopScreenId))
    {
        LOG(ERROR) << "Unable select screen";
    }

    LOG(INFO) << "X11 screen capturer is initialized!";
    return true;
}

//--------------------------------------------------------------------------------------------------
bool ScreenCapturerX11::handleXEvent(const XEvent& event)
{
    if (use_damage_ && (event.type == damage_event_base_ + XDamageNotify))
    {
        const XDamageNotifyEvent* damage_event = reinterpret_cast<const XDamageNotifyEvent*>(&event);
        if (damage_event->damage != damage_handle_)
            return false;

        DCHECK(damage_event->level == XDamageReportNonEmpty);
        return true;
    }
    else if (use_randr_ && event.type == randr_event_base_ + RRScreenChangeNotify)
    {
        XRRUpdateConfiguration(const_cast<XEvent*>(&event));
        updateMonitors();
        LOG(INFO) << "XRandR screen change event received";
        return true;
    }
    else if (event.type == ConfigureNotify)
    {
        screenConfigurationChanged();
        return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerX11::initXDamage()
{
    // Our use of XDamage requires XFixes.
    if (!has_xfixes_)
        return;

    // Check for XDamage extension.
    if (!XDamageQueryExtension(display(), &damage_event_base_, &damage_error_base_))
    {
        LOG(INFO) << "X server does not support XDamage";
        return;
    }

    // TODO(lambroslambrou): Disable DAMAGE in situations where it is known to fail, such as when
    // Desktop Effects are enabled, with graphics drivers (nVidia, ATI) that fail to report DAMAGE
    // notifications properly.

    // Request notifications every time the screen becomes damaged.
    damage_handle_ = XDamageCreate(display(), root_window_, XDamageReportNonEmpty);
    if (!damage_handle_)
    {
        LOG(ERROR) << "Unable to initialize XDamage";
        return;
    }

    // Create an XFixes server-side region to collate damage into.
    damage_region_ = XFixesCreateRegion(display(), 0, 0);
    if (!damage_region_)
    {
        XDamageDestroy(display(), damage_handle_);
        LOG(ERROR) << "Unable to create XFixes region";
        return;
    }

    display_->addEventHandler(damage_event_base_ + XDamageNotify, this);

    use_damage_ = true;
    LOG(INFO) << "Using XDamage extension";
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerX11::initXrandr()
{
    int major_version = 0;
    int minor_version = 0;
    int error_base_ignored = 0;

    if (XRRQueryExtension(display(), &randr_event_base_, &error_base_ignored) &&
        XRRQueryVersion(display(), &major_version, &minor_version))
    {
        if (major_version > 1 || (major_version == 1 && minor_version >= 5))
        {
            // Dynamically link XRRGetMonitors and XRRFreeMonitors as a workaround to avoid a
            // dependency issue with Debian 8.
            get_monitors_ = reinterpret_cast<get_monitors_func>(
                dlsym(RTLD_DEFAULT, "XRRGetMonitors"));
            free_monitors_ = reinterpret_cast<free_monitors_func>(
                dlsym(RTLD_DEFAULT, "XRRFreeMonitors"));

            if (get_monitors_ && free_monitors_)
            {
                use_randr_ = true;
                LOG(INFO) << "Using XRandR extension v" << major_version << '.' << minor_version;
                monitors_ = get_monitors_(display(), root_window_, true, &num_monitors_);

                // Register for screen change notifications
                XRRSelectInput(display(), root_window_, RRScreenChangeNotifyMask);
                display_->addEventHandler(randr_event_base_ + RRScreenChangeNotify, this);
            }
            else
            {
                LOG(ERROR) << "Unable to link XRandR monitor functions";
            }
        }
        else
        {
            LOG(ERROR) << "XRandR entension is older than v1.5";
        }
    }
    else
    {
        LOG(ERROR) << "X server does not support XRandR";
    }
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerX11::updateMonitors()
{
    if (monitors_)
    {
        free_monitors_(monitors_);
        monitors_ = nullptr;
    }

    monitors_ = get_monitors_(display(), root_window_, true, &num_monitors_);

    if (selected_monitor_name_)
    {
        if (selected_monitor_name_ == static_cast<Atom>(kFullDesktopScreenId))
        {
            selected_monitor_rect_ = QRect(QPoint(0, 0), x_server_pixel_buffer_.windowSize());
            return;
        }

        for (int i = 0; i < num_monitors_; ++i)
        {
            XRRMonitorInfo& m = monitors_[i];
            if (selected_monitor_name_ == m.name)
            {
                LOG(INFO) << "XRandR monitor" << m.name << "rect updated";
                selected_monitor_rect_ = QRect(QPoint(m.x, m.y), QSize(m.width, m.height));
                return;
            }
        }

        // The selected monitor is not connected anymore
        LOG(INFO) << "XRandR selected monitor" << selected_monitor_name_ << "lost";
        selected_monitor_rect_ = QRect(QPoint(0, 0), QSize(0, 0));
    }
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerX11::screenConfigurationChanged()
{
    // Make sure the frame buffers will be reallocated.
    queue_.reset();

    helper_.clearInvalidRegion();
    if (!x_server_pixel_buffer_.init(atom_cache_.get(), DefaultRootWindow(display())))
    {
        LOG(ERROR) << "Failed to initialize pixel buffer after screen configuration change";
    }

    if (!use_randr_)
        selected_monitor_rect_ = QRect(QPoint(0, 0), x_server_pixel_buffer_.windowSize());
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerX11::synchronizeFrame()
{
    // Synchronize the current buffer with the previous one since we do not capture the entire
    // desktop. Note that encoder may be reading from the previous buffer at this time so thread
    // access complaints are false positives.

    // TODO(hclam): We can reduce the amount of copying here by subtracting |capturer_helper_|s
    // region from |last_invalid_region_|. http://crbug.com/92354
    DCHECK(queue_.previousFrame());

    Frame* current = queue_.currentFrame();
    Frame* last = queue_.previousFrame();
    DCHECK(current != last);

    for (const auto& rect : std::as_const(last_invalid_region_))
    {
        if (selected_monitor_rect_.contains(rect))
        {
            QRect r = rect;
            r.translate(-selected_monitor_rect_.left(), -selected_monitor_rect_.top());
            current->copyPixelsFrom(*last, r.topLeft(), r);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void ScreenCapturerX11::deinitXlib()
{
    if (monitors_)
    {
        free_monitors_(monitors_);
        monitors_ = nullptr;
    }

    if (gc_)
    {
        XFreeGC(display(), gc_);
        gc_ = nullptr;
    }

    x_server_pixel_buffer_.release();

    if (display())
    {
        if (damage_handle_)
        {
            XDamageDestroy(display(), damage_handle_);
            damage_handle_ = 0;
        }

        if (damage_region_)
        {
            XFixesDestroyRegion(display(), damage_region_);
            damage_region_ = 0;
        }
    }
}

//--------------------------------------------------------------------------------------------------
Frame* ScreenCapturerX11::captureFrameImpl()
{
    Frame* frame = queue_.currentFrame();

    // Pass the screen size to the helper, so it can clip the invalid region if it expands that
    // region to a grid.
    helper_.setSizeMostRecent(x_server_pixel_buffer_.windowSize());

    // In the DAMAGE case, ensure the frame is up-to-date with the previous frame if any. If there
    // isn't a previous frame, that means a screen-resolution change occurred, and |invalid_rects|
    // will be updated to include the whole screen.
    if (use_damage_ && queue_.previousFrame())
        synchronizeFrame();

    QRegion* updated_region = frame->updatedRegion();

    // Clear updated region.
    *updated_region = QRegion();

    x_server_pixel_buffer_.synchronize();
    if (use_damage_ && queue_.previousFrame())
    {
        // Atomically fetch and clear the damage region.
        XDamageSubtract(display(), damage_handle_, X11_None, damage_region_);
        int rectsNum = 0;
        XRectangle bounds;
        XRectangle* rects = XFixesFetchRegionAndBounds(display(), damage_region_,
                                                       &rectsNum, &bounds);
        for (int i = 0; i < rectsNum; ++i)
        {
            *updated_region +=
                QRect(QPoint(rects[i].x, rects[i].y), QSize(rects[i].width, rects[i].height));
        }
        XFree(rects);
        helper_.invalidateRegion(*updated_region);

        // Capture the damaged portions of the desktop.
        helper_.takeInvalidRegion(updated_region);
        *updated_region = updated_region->intersected(selected_monitor_rect_);

        for (const auto& rect : std::as_const(*updated_region))
        {
            if (!x_server_pixel_buffer_.captureRect(rect, frame))
                return nullptr;
        }
    }
    else
    {
        // Doing full-screen polling, or this is the first capture after a screen-resolution change.
        //In either case, need a full-screen capture.
        if (!x_server_pixel_buffer_.captureRect(selected_monitor_rect_, frame))
            return nullptr;

        *updated_region = QRegion(selected_monitor_rect_);
    }

    return frame;
}

} // namespace base
