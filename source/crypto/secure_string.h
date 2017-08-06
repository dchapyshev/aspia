//
// PROJECT:         Aspia Remote Desktop
// FILE:            crypto/secure_string.h
// LICENSE:         Mozilla Public License Version 2.0
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

template<class StringType>
void SecureClearString(StringType& str)
{
    size_t memory_size = str.size();

    if (memory_size)
    {
        sodium_memzero(const_cast<StringType::value_type*>(str.data()), memory_size);
        str.clear();
    }
}

template <class StringType>
class SecureString : public StringType
{
public:
    SecureString() = default;

    explicit SecureString(StringType&& other) noexcept
      : StringType(std::move(other))
    {
        // Nothing
    }

    virtual ~SecureString()
    {
        SecureClearString(*this);
    }

private:
    DISALLOW_COPY_AND_ASSIGN(SecureString);
};

} // namespace aspia

#endif // _ASPIA_CRYPTO__SECURE_STRING_H
