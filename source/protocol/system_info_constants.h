//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/system_info_constants.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_PROTOCOL__SYSTEM_INFO_CONSTANTS_H
#define _ASPIA_PROTOCOL__SYSTEM_INFO_CONSTANTS_H

namespace aspia {

namespace system_info {

extern const size_t kGuidLength;

extern const char kSummary[];

namespace hardware {

namespace dmi {
extern const char kBIOS[];
extern const char kSystem[];
extern const char kMotherboard[];
extern const char kChassis[];
extern const char kCaches[];
extern const char kProcessors[];
extern const char kMemoryDevices[];
extern const char kSystemSlots[];
extern const char kPortConnectors[];
extern const char kOnboardDevices[];
extern const char kBuildinPointing[];
extern const char kPortableBattery[];
} // namespace dmi

extern const char kCPU[];

namespace storage {
extern const char kOpticalDrives[];
extern const char kATA[];
extern const char kSMART[];
} // namespace storage

namespace display {
extern const char kWindowsVideo[];
extern const char kMonitor[];
extern const char kOpenGL[];
} // namespace display

extern const char kPowerOptions[];
extern const char kPrinters[];

namespace windows_devices {
extern const char kAll[];
extern const char kUnknown[];
} // namespace windows_devices

} // namespace hardware

namespace software {

extern const char kPrograms[];
extern const char kUpdates[];
extern const char kServices[];
extern const char kDrivers[];
extern const char kProcesses[];
extern const char kLicenses[];

} // namespace software

namespace network {

extern const char kNetworkCards[];
extern const char kRASConnections[];
extern const char kOpenConnections[];
extern const char kSharedResources[];
extern const char kOpenFiles[];
extern const char kRoutes[];

} // namespace network

namespace operating_system {

extern const char kRegistrationInformation[];
extern const char kTaskScheduler[];

namespace users_and_groups {
extern const char kUsers[];
extern const char kUserGroups[];
extern const char kActiveSessions[];
} // namespace users_and_groups

extern const char kEnvironmentVariables[];

namespace event_logs {
extern const char kApplications[];
extern const char kSecurity[];
extern const char kSystem[];
} // namespace event_logs

} // namespace operating_system

} // namespace system_info

} // namespace aspia

#endif // _ASPIA_PROTOCOL__SYSTEM_INFO_CONSTANTS_H
