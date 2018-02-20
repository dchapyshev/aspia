//
// PROJECT:         Aspia
// FILE:            host/host_switches.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "host/host_switches.h"

namespace aspia {

const wchar_t kSessionTypeSwitch[] = L"session-type";
const wchar_t kChannelIdSwitch[] = L"channel-id";
const wchar_t kServiceIdSwitch[] = L"service-id";
const wchar_t kSessionIdSwitch[] = L"session-id";

const wchar_t kSessionTypeDesktop[] = L"desktop";
const wchar_t kSessionTypeFileTransfer[] = L"file-transfer";
const wchar_t kSessionTypeSystemInfo[] = L"system-info";

} // namespace aspia
