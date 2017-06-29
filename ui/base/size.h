//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/size.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__BASE__SIZE_H
#define _ASPIA_UI__BASE__SIZE_H

namespace aspia {

class UiSize
{
public:
    UiSize()
    {
        memset(&size_, 0, sizeof(size_));
    }

    UiSize(const UiSize& other)
    {
        CopyFrom(other);
    }

    explicit UiSize(const SIZE& other)
    {
        CopyFrom(other);
    }

    explicit UiSize(int width, int height)
    {
        Set(width, height);
    }

    explicit UiSize(LPARAM lparam)
    {
        Set(LOWORD(lparam), HIWORD(lparam));
    }

    SIZE* Pointer()
    {
        return &size_;
    }

    const SIZE* ConstPointer()
    {
        return &size_;
    }

    void CopyFrom(const SIZE& other)
    {
        size_ = other;
    }

    void CopyFrom(const UiSize& other)
    {
        CopyFrom(other.size_);
    }

    void Set(int width, int height)
    {
        size_.cx = width;
        size_.cy = height;
    }

    int Width() const
    {
        return size_.cx;
    }

    int Height() const
    {
        return size_.cy;
    }

    bool IsEmpty() const
    {
        return (Width() <= 0 || Height() <= 0);
    }

private:
    SIZE size_;
};

} // namespace aspia

#endif // _ASPIA_UI__BASE__SIZE_H
