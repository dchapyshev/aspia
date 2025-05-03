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

#include "base/desktop/win/dxgi_output_duplicator.h"

#include "base/logging.h"
#include "base/desktop/mouse_cursor.h"
#include "base/desktop/win/dxgi_texture_mapping.h"
#include "base/desktop/win/dxgi_texture_staging.h"

#include <libyuv/convert_argb.h>

#include <algorithm>
#include <cstring>

#include <comdef.h>
#include <dxgi.h>
#include <dxgiformat.h>
#include <Windows.h>

namespace base {

using Microsoft::WRL::ComPtr;

namespace {

// Timeout for AcquireNextFrame() call.
// DxgiDuplicatorController leverages external components to do the capture scheduling. So here
// DxgiOutputDuplicator does not need to actively wait for a new frame.
const int kAcquireTimeoutMs = 0;

//--------------------------------------------------------------------------------------------------
Rect RECTToDesktopRect(const RECT& rect)
{
    return Rect::makeLTRB(rect.left, rect.top, rect.right, rect.bottom);
}

//--------------------------------------------------------------------------------------------------
Rotation dxgiRotationToRotation(DXGI_MODE_ROTATION rotation)
{
    switch (rotation)
    {
        case DXGI_MODE_ROTATION_IDENTITY:
        case DXGI_MODE_ROTATION_UNSPECIFIED:
            return Rotation::CLOCK_WISE_0;

        case DXGI_MODE_ROTATION_ROTATE90:
            return Rotation::CLOCK_WISE_90;

        case DXGI_MODE_ROTATION_ROTATE180:
            return Rotation::CLOCK_WISE_180;

        case DXGI_MODE_ROTATION_ROTATE270:
            return Rotation::CLOCK_WISE_270;
    }

    return Rotation::CLOCK_WISE_0;
}

}  // namespace

//--------------------------------------------------------------------------------------------------
DxgiOutputDuplicator::DxgiOutputDuplicator(const D3dDevice& device,
                                           const ComPtr<IDXGIOutput1>& output,
                                           const DXGI_OUTPUT_DESC& desc)
    : device_(device),
      output_(output),
      device_name_(QString::fromWCharArray(desc.DeviceName)),
      initial_desktop_rect_(RECTToDesktopRect(desc.DesktopCoordinates)),
      desktop_rect_(RECTToDesktopRect(desc.DesktopCoordinates))
{
    LOG(LS_INFO) << "Ctor";

    DCHECK(output_);
    DCHECK(!desktop_rect_.isEmpty());
    DCHECK_GT(desktop_rect_.width(), 0);
    DCHECK_GT(desktop_rect_.height(), 0);

    memset(&desc_, 0, sizeof(desc_));
}

//--------------------------------------------------------------------------------------------------
DxgiOutputDuplicator::DxgiOutputDuplicator(DxgiOutputDuplicator&& other) = default;

//--------------------------------------------------------------------------------------------------
DxgiOutputDuplicator::~DxgiOutputDuplicator()
{
    LOG(LS_INFO) << "Dtor";

    if (duplication_)
        duplication_->ReleaseFrame();

    texture_.reset();
}

//--------------------------------------------------------------------------------------------------
bool DxgiOutputDuplicator::initialize()
{
    if (duplicateOutput())
    {
        LOG(LS_INFO) << "Image in system memory: " << desc_.DesktopImageInSystemMemory;

        if (desc_.DesktopImageInSystemMemory)
            texture_.reset(new DxgiTextureMapping(duplication_.Get()));
        else
            texture_.reset(new DxgiTextureStaging(device_));

        return true;
    }
    else
    {
        duplication_.Reset();
        return false;
    }
}

//--------------------------------------------------------------------------------------------------
bool DxgiOutputDuplicator::duplicateOutput()
{
    DCHECK(!duplication_);

    static const int kRetryCount = 10;

    for (int i = 0;; ++i)
    {
        _com_error error =
            output_->DuplicateOutput(static_cast<IUnknown*>(device_.d3dDevice()),
                                     duplication_.GetAddressOf());
        if (error.Error() != S_OK || !duplication_)
        {
            // DuplicateOutput may temporarily return E_ACCESSDENIED.
            if (error.Error() == E_ACCESSDENIED && i < kRetryCount)
            {
                desktop_ = Desktop::inputDesktop();
                if (desktop_.isValid())
                    desktop_.setThreadDesktop();

                Sleep(100);
                continue;
            }

            LOG(LS_ERROR) << "Failed to duplicate output from IDXGIOutput1, error "
                          << error.ErrorMessage() << ", with code " << error.Error();
            return false;
        }
        else
        {
            break;
        }
    }

    memset(&desc_, 0, sizeof(desc_));
    duplication_->GetDesc(&desc_);

    if (desc_.ModeDesc.Format != DXGI_FORMAT_B8G8R8A8_UNORM)
    {
        if (desc_.ModeDesc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT)
        {
            LOG(LS_INFO) << "HDR display detected";
        }
        else
        {
            LOG(LS_ERROR) << "IDXGIDuplicateOutput does not use RGBA (8 bit) "
                             "format, which is required by downstream components, format is "
                          << desc_.ModeDesc.Format;
            return false;
        }
    }

    if (static_cast<int>(desc_.ModeDesc.Width) != desktop_rect_.width() ||
        static_cast<int>(desc_.ModeDesc.Height) != desktop_rect_.height())
    {
        LOG(LS_ERROR) << "IDXGIDuplicateOutput does not return a same size as its "
                         "IDXGIOutput1, size returned by IDXGIDuplicateOutput is "
                      << desc_.ModeDesc.Width << " x " << desc_.ModeDesc.Height
                      << ", size returned by IDXGIOutput1 is " << desktop_rect_.width()
                      << " x " << desktop_rect_.height();
        return false;
    }

    rotation_ = dxgiRotationToRotation(desc_.Rotation);
    unrotated_size_ = rotateSize(desktopSize(), reverseRotation(rotation_));

    return true;
}

//--------------------------------------------------------------------------------------------------
bool DxgiOutputDuplicator::releaseFrame()
{
    DCHECK(duplication_);

    _com_error error = duplication_->ReleaseFrame();
    if (error.Error() != S_OK)
    {
        LOG(LS_ERROR) << "Failed to release frame from IDXGIOutputDuplication, error "
                      << error.ErrorMessage() << ", code " << error.Error();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool DxgiOutputDuplicator::duplicate(
    Context* context, const Point& offset, SharedFrame* target, DxgiCursor* cursor)
{
    DCHECK(duplication_);
    DCHECK(texture_);
    DCHECK(target);
    DCHECK(cursor);

    if (!Rect::makeSize(target->size()).containsRect(translatedDesktopRect(offset)))
    {
        // target size is not large enough to cover current output region.
        return false;
    }

    DXGI_OUTDUPL_FRAME_INFO frame_info;
    memset(&frame_info, 0, sizeof(frame_info));

    ComPtr<IDXGIResource> resource;

    _com_error error = duplication_->AcquireNextFrame(kAcquireTimeoutMs,
                                                      &frame_info,
                                                      resource.GetAddressOf());
    if (error.Error() != S_OK && error.Error() != DXGI_ERROR_WAIT_TIMEOUT)
    {
        LOG(LS_ERROR) << "Failed to capture frame, error "
                      << error.ErrorMessage() << ", code " << error.Error();
        return false;
    }

    if (frame_info.PointerShapeBufferSize != 0)
    {
        DXGI_OUTDUPL_POINTER_SHAPE_INFO* shape_info = cursor->pointerShapeInfo();
        QByteArray* buffer = cursor->pointerShapeBuffer();

        if (buffer->capacity() < static_cast<QByteArray::size_type>(frame_info.PointerShapeBufferSize))
            buffer->reserve(static_cast<QByteArray::size_type>(frame_info.PointerShapeBufferSize));

        buffer->resize(static_cast<QByteArray::size_type>(frame_info.PointerShapeBufferSize));

        UINT buffer_required;
        _com_error hr = duplication_->GetFramePointerShape(frame_info.PointerShapeBufferSize,
                                                           buffer->data(),
                                                           &buffer_required,
                                                           shape_info);
        if (FAILED(hr.Error()))
        {
            LOG(LS_ERROR) << "Failed to capture cursor, error "
                          << error.ErrorMessage() << ", code " << error.Error();
            buffer->clear();
        }
    }

    if (frame_info.LastMouseUpdateTime.QuadPart != 0)
    {
        cursor->setVisible(frame_info.PointerPosition.Visible != FALSE);

        if (frame_info.PointerPosition.Visible)
        {
            DXGI_OUTDUPL_POINTER_SHAPE_INFO* shape_info = cursor->pointerShapeInfo();

            int pos_x = frame_info.PointerPosition.Position.x + shape_info->HotSpot.x + offset.x();
            int pos_y = frame_info.PointerPosition.Position.y + shape_info->HotSpot.y + offset.y();

            cursor->setNativePosition(
                Point(pos_x + initial_desktop_rect_.x(), pos_y + initial_desktop_rect_.y()));
            cursor->setPosition(Point(pos_x, pos_y));
        }
    }

    // We need to merge updated region with the one from context, but only spread updated region
    // from current frame. So keeps a copy of updated region from context here. The |updated_region|
    // always starts from (0, 0).
    Region updated_region;
    updated_region.swap(&context->updated_region);

    if (error.Error() == S_OK && frame_info.AccumulatedFrames > 0 && resource)
    {
        detectUpdatedRegion(frame_info, &context->updated_region);
        spreadContextChange(context);

        if (!texture_->copyFrom(frame_info, resource.Get()))
            return false;

        updated_region.addRegion(context->updated_region);

        // TODO(zijiehe): Figure out why clearing context->updated_region() here triggers screen
        // flickering?

        const Frame& source = texture_->asDesktopFrame();

        if (rotation_ != Rotation::CLOCK_WISE_0)
        {
            for (Region::Iterator it(updated_region); !it.isAtEnd(); it.advance())
            {
                // The |updated_region| returned by Windows is rotated, but the |source| frame is
                // not. So we need to rotate it reversely.
                const Rect source_rect =
                    rotateRect(it.rect(), desktopSize(), reverseRotation(rotation_));
                rotateDesktopFrame(source, source_rect, rotation_, offset, target);
            }
        }
        else
        {
            for (Region::Iterator it(updated_region); !it.isAtEnd(); it.advance())
            {
                // The Rect in |target|, starts from offset.
                Rect dest_rect = it.rect();
                dest_rect.translate(offset);

                libyuv::ARGBCopy(source.frameDataAtPos(it.rect().topLeft()), source.stride(),
                                 target->frameDataAtPos(dest_rect.topLeft()), target->stride(),
                                 dest_rect.width(), dest_rect.height());
            }
        }

        last_frame_ = target->share();
        last_frame_offset_ = offset;

        updated_region.translate(offset.x(), offset.y());
        target->updatedRegion()->addRegion(updated_region);
        ++num_frames_captured_;

        return texture_->release() && releaseFrame();
    }

    if (last_frame_)
    {
        // No change since last frame or AcquireNextFrame() timed out, we will export last frame to
        // the target.
        for (Region::Iterator it(updated_region); !it.isAtEnd(); it.advance())
        {
            // The Rect in |source|, starts from last_frame_offset_.
            Rect source_rect = it.rect();

            // The Rect in |target|, starts from offset.
            Rect target_rect = source_rect;

            source_rect.translate(last_frame_offset_);
            target_rect.translate(offset);

            libyuv::ARGBCopy(last_frame_->frameDataAtPos(source_rect.topLeft()), last_frame_->stride(),
                             target->frameDataAtPos(target_rect.topLeft()), target->stride(),
                             target_rect.width(), target_rect.height());
        }

        updated_region.translate(offset.x(), offset.y());
        target->updatedRegion()->addRegion(updated_region);
    }
    else
    {
        // If we were at the very first frame, and capturing failed, the
        // context->updated_region should be kept unchanged for next attempt.
        context->updated_region.swap(&updated_region);
    }

    // If AcquireNextFrame() failed with timeout error, we do not need to release the frame.
    return error.Error() == DXGI_ERROR_WAIT_TIMEOUT || releaseFrame();
}

//--------------------------------------------------------------------------------------------------
Rect DxgiOutputDuplicator::translatedDesktopRect(const Point& offset) const
{
    Rect result(Rect::makeSize(desktopSize()));
    result.translate(offset);
    return result;
}

//--------------------------------------------------------------------------------------------------
Rect DxgiOutputDuplicator::untranslatedDesktopRect() const
{
    return Rect::makeSize(desktopSize());
}

//--------------------------------------------------------------------------------------------------
void DxgiOutputDuplicator::detectUpdatedRegion(const DXGI_OUTDUPL_FRAME_INFO& frame_info,
                                               Region* updated_region)
{
    if (doDetectUpdatedRegion(frame_info, updated_region))
    {
        // Make sure even a region returned by Windows API is out of the scope of
        // desktop_rect_, we still won't export it to the target DesktopFrame.
        updated_region->intersectWith(untranslatedDesktopRect());
    }
    else
    {
        updated_region->setRect(untranslatedDesktopRect());
    }
}

//--------------------------------------------------------------------------------------------------
bool DxgiOutputDuplicator::doDetectUpdatedRegion(const DXGI_OUTDUPL_FRAME_INFO& frame_info,
                                                 Region* updated_region)
{
    DCHECK(updated_region);
    updated_region->clear();

    if (frame_info.TotalMetadataBufferSize == 0)
    {
        // This should not happen, since frame_info.AccumulatedFrames > 0.
        LOG(LS_ERROR) << "frame_info.AccumulatedFrames > 0, but TotalMetadataBufferSize == 0";
        return false;
    }

    if (metadata_.size() < frame_info.TotalMetadataBufferSize)
    {
        metadata_.clear(); // Avoid data copy
        metadata_.resize(frame_info.TotalMetadataBufferSize);
    }

    UINT buff_size = 0;
    DXGI_OUTDUPL_MOVE_RECT* move_rects = reinterpret_cast<DXGI_OUTDUPL_MOVE_RECT*>(metadata_.data());

    _com_error error = duplication_->GetFrameMoveRects(
        static_cast<UINT>(metadata_.size()), move_rects, &buff_size);
    if (error.Error() != S_OK)
    {
        LOG(LS_ERROR) << "Failed to get move rectangles, error "
                      << error.ErrorMessage() << ", code " << error.Error();
        return false;
    }

    size_t move_rects_count = buff_size / sizeof(DXGI_OUTDUPL_MOVE_RECT);

    RECT* dirty_rects = reinterpret_cast<RECT*>(metadata_.data() + buff_size);

    error = duplication_->GetFrameDirtyRects(
        static_cast<UINT>(metadata_.size()) - buff_size, dirty_rects, &buff_size);
    if (error.Error() != S_OK)
    {
        LOG(LS_ERROR) << "Failed to get dirty rectangles, error "
                      << error.ErrorMessage() << ", code " << error.Error();
        return false;
    }

    size_t dirty_rects_count = buff_size / sizeof(RECT);

    while (move_rects_count > 0)
    {
        // DirectX capturer API may randomly return unmoved move_rects, which should
        // be skipped to avoid unnecessary wasting of differing and encoding
        // resources.
        // By using testing application it2me_standalone_host_main, this check
        // reduces average capture time by 0.375% (4.07 -> 4.055), and average
        // encode time by 0.313% (8.042 -> 8.016) without other impacts.
        if (move_rects->SourcePoint.x != move_rects->DestinationRect.left ||
            move_rects->SourcePoint.y != move_rects->DestinationRect.top)
        {
            updated_region->addRect(
                rotateRect(Rect::makeXYWH(move_rects->SourcePoint.x,
                                          move_rects->SourcePoint.y,
                                          move_rects->DestinationRect.right -
                                          move_rects->DestinationRect.left,
                                          move_rects->DestinationRect.bottom -
                                          move_rects->DestinationRect.top),
                           unrotated_size_, rotation_));

            updated_region->addRect(
                rotateRect(Rect::makeLTRB(move_rects->DestinationRect.left,
                                          move_rects->DestinationRect.top,
                                          move_rects->DestinationRect.right,
                                          move_rects->DestinationRect.bottom),
                           unrotated_size_, rotation_));
        }
        else
        {
            LOG(LS_INFO) << "Unmoved move_rect detected, ["
                         << move_rects->DestinationRect.left << ", "
                         << move_rects->DestinationRect.top << "] - ["
                         << move_rects->DestinationRect.right << ", "
                         << move_rects->DestinationRect.bottom << "].";
        }

        ++move_rects;
        --move_rects_count;
    }

    while (dirty_rects_count > 0)
    {
        updated_region->addRect(rotateRect(
            Rect::makeLTRB(dirty_rects->left, dirty_rects->top,
                           dirty_rects->right, dirty_rects->bottom),
            unrotated_size_, rotation_));

        ++dirty_rects;
        --dirty_rects_count;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void DxgiOutputDuplicator::setup(Context* context)
{
    DCHECK(context->updated_region.isEmpty());

    // Always copy entire monitor during the first duplicate() function call.
    context->updated_region.addRect(untranslatedDesktopRect());
    DCHECK(std::find(contexts_.begin(), contexts_.end(), context) == contexts_.end());
    contexts_.push_back(context);
}

//--------------------------------------------------------------------------------------------------
void DxgiOutputDuplicator::unregister(const Context* const context)
{
    auto it = std::find(contexts_.begin(), contexts_.end(), context);
    DCHECK(it != contexts_.end());
    contexts_.erase(it);
}

//--------------------------------------------------------------------------------------------------
void DxgiOutputDuplicator::spreadContextChange(const Context* const source)
{
    for (Context* dest : contexts_)
    {
        DCHECK(dest);

        if (dest != source)
            dest->updated_region.addRegion(source->updated_region);
    }
}

//--------------------------------------------------------------------------------------------------
Size DxgiOutputDuplicator::desktopSize() const
{
    return desktop_rect_.size();
}

//--------------------------------------------------------------------------------------------------
qint64 DxgiOutputDuplicator::numFramesCaptured() const
{
#if !defined(NDEBUG)
    DCHECK_EQ(!!last_frame_, num_frames_captured_ > 0);
#endif
    return num_frames_captured_;
}

//--------------------------------------------------------------------------------------------------
void DxgiOutputDuplicator::translateRect(const Point& position)
{
    desktop_rect_.translate(position);
    DCHECK_GE(desktop_rect_.left(), 0);
    DCHECK_GE(desktop_rect_.top(), 0);
}

} // namespace base
