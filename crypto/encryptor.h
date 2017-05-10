//
// PROJECT:         Aspia Remote Desktop
// FILE:            crypto/encryptor.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CRYPTO__ENCRYPTOR_H
#define _ASPIA_CRYPTO__ENCRYPTOR_H

#include "base/io_buffer.h"

namespace aspia {

class Encryptor
{
public:
    virtual ~Encryptor() = default;

    virtual IOBuffer Encrypt(const IOBuffer& source_buffer) = 0;
};

} // namespace aspia

#endif // _ASPIA_CRYPTO__ENCRYPTOR_H
