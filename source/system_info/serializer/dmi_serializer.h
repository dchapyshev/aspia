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

#ifndef ASPIA_SYSTEM_INFO__SERIALIZER__DMI_SERIALIZER_H_
#define ASPIA_SYSTEM_INFO__SERIALIZER__DMI_SERIALIZER_H_

#include "base/macros_magic.h"
#include "system_info/serializer/serializer.h"

namespace aspia {

class DmiSerializer : public Serializer
{
    Q_OBJECT

public:
    static Serializer* create(QObject* parent, const QString& uuid);

public slots:
    void start() override;

protected:
    DmiSerializer(QObject* parent, const QString& uuid);

private:
    DISALLOW_COPY_AND_ASSIGN(DmiSerializer);
};

} // namespace aspia

#endif // ASPIA_SYSTEM_INFO__SERIALIZER__DMI_SERIALIZER_H_
