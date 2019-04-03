//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

// Originally these classes are from Chromium.
// http://src.chromium.org/viewvc/chrome/trunk/src/base/memory/ref_counted.h?view=markup

//
// A smart pointer class for reference counted objects. Use this class instead of calling addRef()
// and release() manually on a reference counted object to avoid common memory leaks caused by
// forgetting to release() an object
// reference. Sample usage:
//
//   class MyFoo : public RefCounted<MyFoo>
//   {
//        ...
//   };
//
//   void some_function()
//   {
//       scoped_refptr<MyFoo> foo = new MyFoo();
//       foo->method(param);
//       // |foo| is released when this function returns
//   }
//
//   void some_other_function()
//   {
//       scoped_refptr<MyFoo> foo = new MyFoo();
//       ...
//       foo = nullptr; // explicitly releases |foo|
//       ...
//       if (foo)
//           foo->method(param);
//   }
//
// The above examples show how scoped_refptr<T> acts like a pointer to T. Given two
// scoped_refptr<T> classes, it is also possible to exchange references between the two objects,
// like so:
//
//   {
//     scoped_refptr<MyFoo> a = new MyFoo();
//     scoped_refptr<MyFoo> b;
//
//     b.swap(a);
//     // now, |b| references the MyFoo object, and |a| references null.
//   }
//
// To make both |a| and |b| in the above example reference the same MyFoo object, simply use the
// assignment operator:
//
//   {
//     scoped_refptr<MyFoo> a = new MyFoo();
//     scoped_refptr<MyFoo> b;
//
//     b = a;
//     // now, |a| and |b| each own a reference to the same MyFoo object.
//   }
//

#ifndef BASE__SCOPED_REFPTR_H
#define BASE__SCOPED_REFPTR_H

#include <memory>
#include <utility>

namespace base {

template <class T>
class scoped_refptr
{
public:
    using element_type = T;

    scoped_refptr()
        : ptr_(nullptr)
    {
        // Nothing
    }

    scoped_refptr(T* p)
        : ptr_(p)
    {  // NOLINT(runtime/explicit)
        if (ptr_)
            ptr_->addRef();
    }

    scoped_refptr(const scoped_refptr<T>& r)
        : ptr_(r.ptr_)
    {
        if (ptr_)
            ptr_->addRef();
    }

    template <typename U>
    scoped_refptr(const scoped_refptr<U>& r)
        : ptr_(r.get())
    {
        if (ptr_)
            ptr_->addRef();
    }

    // Move constructors.
    scoped_refptr(scoped_refptr<T>&& r)
        : ptr_(r.release())
    {
        // Nothing
    }

    template <typename U>
    scoped_refptr(scoped_refptr<U>&& r)
        : ptr_(r.release())
    {
        // Nothing
    }

    ~scoped_refptr()
    {
        if (ptr_)
            ptr_->release();
    }

    T* get() const { return ptr_; }
    operator T*() const { return ptr_; }
    T* operator->() const { return ptr_; }

    // Returns the (possibly null) raw pointer, and makes the scoped_refptr hold a
    // null pointer, all without touching the reference count of the underlying
    // pointed-to object. The object is still reference counted, and the caller of
    // release() is now the proud owner of one reference, so it is responsible for
    // calling Release() once on the object when no longer using it.
    T* release()
    {
        T* retVal = ptr_;
        ptr_ = nullptr;
        return retVal;
    }

    scoped_refptr<T>& operator=(T* p)
    {
        // addRef first so that self assignment should work
        if (p)
            p->addRef();
        if (ptr_)
            ptr_->release();
        ptr_ = p;
        return *this;
    }

    scoped_refptr<T>& operator=(const scoped_refptr<T>& r)
    {
        return *this = r.ptr_;
    }

    template <typename U>
    scoped_refptr<T>& operator=(const scoped_refptr<U>& r)
    {
        return *this = r.get();
    }

    scoped_refptr<T>& operator=(scoped_refptr<T>&& r)
    {
        scoped_refptr<T>(std::move(r)).swap(*this);
        return *this;
    }

    template <typename U>
    scoped_refptr<T>& operator=(scoped_refptr<U>&& r)
    {
        scoped_refptr<T>(std::move(r)).swap(*this);
        return *this;
    }

    void swap(T** pp)
    {
        T* p = ptr_;
        ptr_ = *pp;
        *pp = p;
    }

    void swap(scoped_refptr<T>& r) { swap(&r.ptr_); }

protected:
    T* ptr_;
};

} // namespace base

#endif // BASE__SCOPED_REFPTR_H
