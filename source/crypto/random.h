//
// PROJECT:         Aspia
// FILE:            crypto/random.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CRYPTO__RANDOM_H
#define _ASPIA_CRYPTO__RANDOM_H

#include <string>

namespace aspia {

std::string CreateRandomBuffer(size_t size);

uint32_t CreateRandomNumber();

} // namespace aspia

#endif // _ASPIA_CRYPTO__RANDOM_H
