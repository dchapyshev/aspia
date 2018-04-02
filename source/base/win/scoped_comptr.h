//
// PROJECT:         Aspia
// FILE:            base/win/scoped_comptr.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__WIN__SCOPED_COMPTR_H
#define _ASPIA_BASE__WIN__SCOPED_COMPTR_H

#include <unknwn.h>

#include "base/scoped_refptr.h"

namespace aspia {

namespace details {

template <typename T>
class ScopedComPtrRef;

} // details

template <class Interface>
class ScopedComPtr
{
public:
    using InterfaceType = Interface;

    // Utility template to prevent users of ScopedComPtr from calling AddRef
    // and/or Release() without going through the ScopedComPtr class.
    class BlockIUnknownMethods : public Interface
    {
    private:
        STDMETHOD(QueryInterface)(REFIID iid, void** object) = 0;
        STDMETHOD_(ULONG, AddRef)() = 0;
        STDMETHOD_(ULONG, Release)() = 0;
    };

    ScopedComPtr() {}

    ScopedComPtr(std::nullptr_t) : ptr_(nullptr) {}

    explicit ScopedComPtr(Interface* p) : ptr_(p)
    {
        if (ptr_)
            ptr_->AddRef();
    }

    ScopedComPtr(const ScopedComPtr<Interface>& p) : ptr_(p.Get())
    {
        if (ptr_)
            ptr_->AddRef();
    }

    ~ScopedComPtr()
    {
        // We don't want the smart pointer class to be bigger than the pointer
        // it wraps.
        static_assert(sizeof(ScopedComPtr<Interface>) == sizeof(Interface*),
                      "ScopedComPtrSize");
        Reset();
    }

    Interface* Get() const { return ptr_; }

    explicit operator bool() const { return ptr_ != nullptr; }

    // Explicit Release() of the held object.  Useful for reuse of the
    // ScopedComPtr instance.
    // Note that this function equates to IUnknown::Release and should not
    // be confused with e.g. unique_ptr::release().
    unsigned long Reset()
    {
        unsigned long ref = 0;
        Interface* temp = ptr_;
        if (temp)
        {
            ptr_ = nullptr;
            ref = temp->Release();
        }
        return ref;
    }

    // Sets the internal pointer to NULL and returns the held object without
    // releasing the reference.
    Interface* Detach()
    {
        Interface* p = ptr_;
        ptr_ = nullptr;
        return p;
    }

    // Accepts an interface pointer that has already been addref-ed.
    void Attach(Interface* p)
    {
        DCHECK(!ptr_);
        ptr_ = p;
    }

    // Retrieves the pointer address.
    // Used to receive object pointers as out arguments (and take ownership).
    // The function DCHECKs on the current value being NULL.
    // Usage: Foo(p.GetAddressOf());
    Interface** GetAddressOf()
    {
        Q_ASSERT(!ptr_);
        return &ptr_;
    }

    template <class Query>
    HRESULT CopyTo(Query** p)
    {
        Q_ASSERT(p);
        Q_ASSERT(ptr_);
        // IUnknown already has a template version of QueryInterface
        // so the iid parameter is implicit here. The only thing this
        // function adds are the DCHECKs.
        return ptr_->QueryInterface(IID_PPV_ARGS(p));
    }

    // QI for times when the IID is not associated with the type.
    HRESULT CopyTo(const IID& iid, void** obj)
    {
        Q_ASSERT(obj);
        Q_ASSERT(ptr_);
        return ptr_->QueryInterface(iid, obj);
    }

    // Provides direct access to the interface.
    // Here we use a well known trick to make sure we block access to
    // IUnknown methods so that something bad like this doesn't happen:
    //    ScopedComPtr<IUnknown> p(Foo());
    //    p->Release();
    //    ... later the destructor runs, which will Release() again.
    // and to get the benefit of the DCHECKs we add to QueryInterface.
    // There's still a way to call these methods if you absolutely must
    // by statically casting the ScopedComPtr instance to the wrapped interface
    // and then making the call... but generally that shouldn't be necessary.
    BlockIUnknownMethods* operator->() const
    {
        Q_ASSERT(ptr_);
        return reinterpret_cast<BlockIUnknownMethods*>(ptr_);
    }

    ScopedComPtr<Interface>& operator=(std::nullptr_t)
    {
        Reset();
        return *this;
    }

    ScopedComPtr<Interface>& operator=(Interface* rhs)
    {
        // AddRef first so that self assignment should work
        if (rhs)
            rhs->AddRef();
        Interface* old_ptr = ptr_;
        ptr_ = rhs;
        if (old_ptr)
            old_ptr->Release();
        return *this;
    }

    ScopedComPtr<Interface>& operator=(const ScopedComPtr<Interface>& rhs)
    {
        return *this = rhs.ptr_;
    }

    Interface& operator*() const
    {
        Q_ASSERT(ptr_);
        return *ptr_;
    }

    bool operator==(const ScopedComPtr<Interface>& rhs) const
    {
        return ptr_ == rhs.Get();
    }

    template <typename U>
    bool operator==(const ScopedComPtr<U>& rhs) const
    {
        return ptr_ == rhs.Get();
    }

    template <typename U>
    bool operator==(const U* rhs) const
    {
        return ptr_ == rhs;
    }

    bool operator!=(const ScopedComPtr<Interface>& rhs) const
    {
        return ptr_ != rhs.Get();
    }

    template <typename U>
    bool operator!=(const ScopedComPtr<U>& rhs) const
    {
        return ptr_ != rhs.Get();
    }

    template <typename U>
    bool operator!=(const U* rhs) const
    {
        return ptr_ != rhs;
    }

    details::ScopedComPtrRef<ScopedComPtr<Interface>> operator&()
    {
        return details::ScopedComPtrRef<ScopedComPtr<Interface>>(this);
    }

    void Swap(ScopedComPtr<Interface>& r)
    {
        Interface* tmp = ptr_;
        ptr_ = r.ptr_;
        r.ptr_ = tmp;
    }

private:
    Interface* ptr_ = nullptr;
};

}  // namespace aspia

#endif // _ASPIA_BASE__WIN__SCOPED_COMPTR_H
