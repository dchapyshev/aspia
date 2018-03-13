//
// PROJECT:         Aspia
// FILE:            base/byte_order.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__BYTE_ORDER_H
#define _ASPIA_BASE__BYTE_ORDER_H

#include <cstdint>

namespace aspia {

uint16_t ByteSwap(uint16_t value);
uint32_t ByteSwap(uint32_t value);
uint64_t ByteSwap(uint64_t value);

void ChangeByteOrder(uint8_t* data, size_t data_size);
void ChangeByteOrder(char* data, size_t data_size);

} // namespace aspia

#endif // _ASPIA_BASE__BYTE_ORDER_H
