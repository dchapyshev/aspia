//
// PROJECT:         Aspia Remote Desktop
// FILE:            crypto/decryptor.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CRYPTO__DECRYPTOR_H
#define _ASPIA_CRYPTO__DECRYPTOR_H

#include "base/io_buffer.h"

#include <memory>

namespace aspia {

class Decryptor
{
public:
    Decryptor() {}
    virtual ~Decryptor() = default;

    virtual std::unique_ptr<IOBuffer> Decrypt(const IOBuffer* source_buffer) = 0;
};

} // namespace aspia

#endif // _ASPIA_CRYPTO__DECRYPTOR_H
