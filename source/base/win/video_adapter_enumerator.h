//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_WIN_VIDEO_ADAPTER_ENUMERATOR_H
#define BASE_WIN_VIDEO_ADAPTER_ENUMERATOR_H

#include "base/win/device_enumerator.h"

namespace base {

class VideoAdapterEnumarator final : public DeviceEnumerator
{
public:
    VideoAdapterEnumarator();

    QString adapterString() const;
    QString biosString() const;
    QString chipString() const;
    QString dacType() const;
    quint64 memorySize() const;

private:
    Q_DISABLE_COPY(VideoAdapterEnumarator)
};

} // namespace base

#endif // BASE_WIN_VIDEO_ADAPTER_ENUMERATOR_H
