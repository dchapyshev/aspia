//
// PROJECT:         Aspia
// FILE:            crypto/secure_memory.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CRYPTO__SECURE_MEMORY_H
#define _ASPIA_CRYPTO__SECURE_MEMORY_H

#include <QString>

namespace aspia {

void secureMemZero(void* data, size_t data_size);
void secureMemZero(std::string* str);
void secureMemZero(QString* str);
void secureMemZero(QByteArray* bytes);

} // namespace aspia

#endif // _ASPIA_CRYPTO__SECURE_MEMORY_H
