//
// PROJECT:         Aspia
// FILE:            base/program_enumerator.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__PROGRAM_ENUMERATOR_H
#define _ASPIA_BASE__PROGRAM_ENUMERATOR_H

#include "base/registry.h"

namespace aspia {

class ProgramEnumerator
{
public:
    ProgramEnumerator();

    bool IsAtEnd() const;
    void Advance();

    std::string GetName() const;
    std::string GetVersion() const;
    std::string GetPublisher() const;
    std::string GetInstallDate() const;
    std::string GetInstallLocation() const;

private:
    enum class Type
    {
        UNKNOWN          = 0,
        PROGRAMM         = 1,
        UPDATE           = 2,
        SYSTEM_COMPONENT = 3
    };

    Type GetType() const;
    std::string GetRegistryValue(const WCHAR* name) const;

    RegistryKeyIterator key_iterator_;
    int count_;

    DISALLOW_COPY_AND_ASSIGN(ProgramEnumerator);
};

} // namespace aspia

#endif // _ASPIA_BASE__PROGRAM_ENUMERATOR_H
