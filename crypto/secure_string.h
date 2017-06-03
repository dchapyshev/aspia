//
// PROJECT:         Aspia Remote Desktop
// FILE:            crypto/secure_string.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CRYPTO__SECURE_STRING_H
#define _ASPIA_CRYPTO__SECURE_STRING_H

#include "base/macros.h"

extern "C" {
#define SODIUM_STATIC
#include <sodium.h>
} // extern "C"

namespace aspia {

template<class T>
void ClearStringContent(T& str)
{
    size_t memory_size = str.size();

    if (memory_size)
        sodium_memzero(const_cast<T::value_type*>(str.data()), memory_size);
}

template <class T>
class SecureString : public T
{
public:
    SecureString() { }

    SecureString(T&& other) :
        T(std::move(other))
    {
        // Nothing
    }

    virtual ~SecureString()
    {
        ClearStringContent(*this);
    }

private:
    DISALLOW_COPY_AND_ASSIGN(SecureString);
};

} // namespace aspia

#endif // _ASPIA_CRYPTO__SECURE_STRING_H
