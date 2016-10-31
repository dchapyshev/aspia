/*
* PROJECT:         Aspia Remote Desktop
* FILE:            desktop_capture/desktop_region_win.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "desktop_capture/desktop_region_win.h"

DesktopRegion::DesktopRegion()
{
    region_ = CreateRectRgn(0, 0, 0, 0);
}

DesktopRegion::DesktopRegion(const DesktopRect &rect)
{
    region_ = CreateRectRgn(rect.left(), rect.top(), rect.right(), rect.bottom());
}

DesktopRegion::DesktopRegion(const DesktopRegion &region)
{
    region_ = CreateRectRgn(0, 0, 0, 0);
    CombineRgn(region_, region.region_, region.region_, RGN_COPY);
}

DesktopRegion::~DesktopRegion()
{
    DeleteObject(region_);
}

void DesktopRegion::CopyFrom(const DesktopRegion &region)
{
    Clear();
    CombineRgn(region_, region.region_, region.region_, RGN_COPY);
}

void DesktopRegion::Clear()
{
    SetRectRgn(region_, 0, 0, 0, 0);
}

bool DesktopRegion::is_empty() const
{
    RECT rc;
    return (GetRgnBox(region_, &rc) == NULLREGION);
}

bool DesktopRegion::Equals(const DesktopRegion &region) const
{
    return EqualRgn(region_, region.region_) ? true : false;
}

void DesktopRegion::AddRegion(const DesktopRegion &region)
{
    CombineRgn(region_, region_, region.region_, RGN_OR);
}

void DesktopRegion::AddRect(const DesktopRect &rect)
{
    HRGN tmp = CreateRectRgn(rect.left(), rect.top(), rect.right(), rect.bottom());
    CombineRgn(region_, region_, tmp, RGN_OR);
    DeleteObject(tmp);
}

DesktopRegion& DesktopRegion::operator=(const DesktopRegion &other)
{
    CopyFrom(other);
    return *this;
}

bool DesktopRegion::operator==(const DesktopRegion &other)
{
    return Equals(other);
}

bool DesktopRegion::operator!=(const DesktopRegion &other)
{
    return !Equals(other);
}

DesktopRegion::Iterator::Iterator(const DesktopRegion &target) :
    data_(nullptr),
    index_(0),
    count_(0)
{
    DWORD size = ::GetRegionData(target.region_, 0, nullptr);
    if (size)
    {
        data_ = reinterpret_cast<PRGNDATA>(malloc(size));
        if (data_)
        {
            if (::GetRegionData(target.region_, size, data_) == size)
            {
                count_ = data_->rdh.nCount;
            }
        }
    }
}

DesktopRegion::Iterator::~Iterator()
{
    free(data_);
}

bool DesktopRegion::Iterator::IsAtEnd() const
{
    return index_ >= count_;
}

void DesktopRegion::Iterator::Advance()
{
    ++index_;
}

DesktopRect DesktopRegion::Iterator::rect()
{
    RECT *r = &(reinterpret_cast<PRECT>(data_->Buffer))[index_];

    return DesktopRect::MakeLTRB(r->left, r->top, r->right, r->bottom);
}
