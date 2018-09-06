//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ASPIA_CRYPTO__SRP_CONSTANTS_H_
#define ASPIA_CRYPTO__SRP_CONSTANTS_H_

#include "base/const_buffer.h"

namespace aspia {

struct SrpNg
{
    const ConstBuffer N;
    const ConstBuffer g;
};

extern const SrpNg kSrpNg_1024;
extern const SrpNg kSrpNg_1536;
extern const SrpNg kSrpNg_2048;
extern const SrpNg kSrpNg_3072;
extern const SrpNg kSrpNg_4096;
extern const SrpNg kSrpNg_6144;
extern const SrpNg kSrpNg_8192;

} // namespace aspia

#endif // ASPIA_CRYPTO__SRP_CONSTANTS_H_
