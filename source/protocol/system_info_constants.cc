//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/system_info_constants.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "protocol/system_info_constants.h"

namespace aspia {

namespace system_info {

const size_t kGuidLength = 36;

const char kSummary[]                 = "8A66762A-0B23-47CB-B66E-A371B3B7111A";

namespace hardware {

namespace dmi {
const char kBIOS[]                    = "B0B73D57-2CDC-4814-9AE0-C7AF7DDDD60E";
const char kSystem[]                  = "F599BBA4-AEBB-4583-A15E-9848F4C98601";
const char kMotherboard[]             = "8143642D-3248-40F5-8FCF-629C581FFF01";
const char kChassis[]                 = "81D9E51F-4A86-49FC-A37F-232D6A62EC45";
const char kCaches[]                  = "BA9258E7-0046-4A77-A97B-0407453706A3";
const char kProcessors[]              = "84D8B0C3-37A4-4825-A523-40B62E0CADC3";
const char kMemoryDevices[]           = "9C591459-A83F-4F48-883D-927765C072B0";
const char kSystemSlots[]             = "7A4F71C6-557F-48A5-AC94-E430F69154F1";
const char kPortConnectors[]          = "FF4CE0FE-261F-46EF-852F-42420E68CFD2";
const char kOnboardDevices[]          = "6C62195C-5E5F-41BA-B6AD-99041594DAC6";
const char kBuildinPointing[]         = "6883684B-3CEC-451B-A2E3-34C16348BA1B";
const char kPortableBattery[]         = "0CA213B5-12EE-4828-A399-BA65244E65FD";
} // namespace dmi

const char kCPU[]                     = "31D1312E-85A9-419A-91B4-BA81129B3CCC";

namespace storage {
const char kOpticalDrives[]           = "68E028FE-3DA6-4BAF-9E18-CDB828372860";
const char kATA[]                     = "79D80586-D264-46E6-8718-09E267730B78";
const char kSMART[]                   = "7B1F2ED7-7A2E-4F5C-A70B-A56AB5B8CE00";
} // namespace storage

namespace display {
const char kWindowsVideo[]            = "09E9069D-C394-4CD7-8252-E5CF83B7674C";
const char kMonitor[]                 = "281100E4-88ED-4AE2-BC4A-3A37282BBAB5";
const char kOpenGL[]                  = "05E4437C-A0CD-41CB-8B50-9A627E13CB97";
} // namespace display

const char kPowerOptions[]            = "42E04A9E-36F7-42A1-BCDA-F3ED70112DFF";
const char kPrinters[]                = "ACBDCE39-CE38-4A79-9626-8C8BA2E3A26A";

namespace windows_devices {
const char kAll[]                     = "22C4F1A6-67F2-4445-B807-9D39E1A80636";
const char kUnknown[]                 = "5BE9FAA9-5F94-4420-8650-B649F35A1DA0";
} // namespace windows_devices

} // namespace hardware

namespace software {

const char kPrograms[]                = "606C70BE-0C6C-4CB6-90E6-D374760FC5EE";
const char kUpdates[]                 = "3E160E27-BE2E-45DB-8292-C3786C9533AB";
const char kServices[]                = "BE3143AB-67C3-4EFE-97F5-FA0C84F338C3";
const char kDrivers[]                 = "8278DA10-227F-4484-9D5D-9A66C294CA82";
const char kProcesses[]               = "14BB101B-EE61-49E6-B5B9-874C4DBEA03C";
const char kLicenses[]                = "6BD88575-9D23-44BC-8A49-64D94CC3EE48";

} // namespace software

namespace network {

const char kNetworkCards[]            = "98D665E9-0F78-4054-BDF3-A51E950A8618";
const char kRASConnections[]          = "E0A43CFD-3A97-4577-B3FB-3B542C0729F7";
const char kOpenConnections[]         = "1A9CBCBD-5623-4CEC-B58C-BD7BD8FAE622";
const char kSharedResources[]         = "9219D538-E1B8-453C-9298-61D5B80C4130";
const char kOpenFiles[]               = "EAE638B9-CCF6-442C-84A1-B0901A64DA3D";
const char kRoutes[]                  = "84184CEB-E232-4CA7-BCAC-E156F1E6DDCB";

} // namespace network

namespace operating_system {

const char kRegistrationInformation[] = "2DDA7127-6ECF-49E1-9C6A-549AEF4B9E87";
const char kTaskScheduler[]           = "1B27C27F-847E-47CC-92DF-6B8F5CB4827A";

namespace users_and_groups {
const char kUsers[]                   = "838AD22A-82BB-47F2-9001-1CD9714ED298";
const char kUserGroups[]              = "B560FDED-5E88-4116-98A5-12462C07AC90";
const char kActiveSessions[]          = "8702E4A1-C9A2-4BA3-BBDE-CFCB6937D2C8";
} // namespace users_and_groups

const char kEnvironmentVariables[]    = "AAB8670A-3C90-4F75-A907-512ACBAD1BE6";

namespace event_logs {
const char kApplications[]            = "0DD03A20-D1AF-4D1F-938F-956EE9003EE9";
const char kSecurity[]                = "7E0220A8-AC51-4C9E-8834-F0F805D40977";
const char kSystem[]                  = "8421A38A-4757-4298-A5CB-9493C7726515";
} // namespace event_logs

} // namespace operating_system

} // namespace system_info

} // namespace aspia
