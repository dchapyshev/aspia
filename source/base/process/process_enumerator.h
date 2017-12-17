//
// PROJECT:         Aspia
// FILE:            base/process/process_enumerator.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__PROCESS_PROCESS_ENUMERATOR_H
#define _ASPIA_BASE__PROCESS_PROCESS_ENUMERATOR_H

#include "base/scoped_object.h"

#include <tlhelp32.h>
#include <string>

namespace aspia {

class ProcessEnumerator
{
public:
    ProcessEnumerator();

    bool IsAtEnd() const;
    void Advance();

    std::string GetProcessName() const;
    std::string GetFilePath() const;
    std::string GetFileDescription() const;
    uint64_t GetUsedMemory() const;
    uint64_t GetUsedSwap() const;

private:
    ScopedHandle snapshot_;
    ScopedHandle current_process_;
    PROCESSENTRY32W process_entry_;

    DISALLOW_COPY_AND_ASSIGN(ProcessEnumerator);
};

} // namespace aspia

#endif // _ASPIA_BASE__PROCESS_PROCESS_ENUMERATOR_H
