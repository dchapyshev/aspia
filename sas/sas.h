/*
* PROJECT:         Aspia Remote Desktop
* FILE:            sas/sas.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_SAS_H
#define _ASPIA_SAS_H

#ifdef __cplusplus
namespace sas {
extern "C" {
#endif

#define DEFAULT_SESSION_ID   0xFFFFFFFF

VOID WINAPI SendSAS_XP(VOID);
VOID WINAPI SendSAS_Vista(BOOL AsUser, DWORD SessionId);
VOID WINAPI SendSAS(VOID);

#ifdef __cplusplus
} // extern "C"
} // namespace sas
#endif

#endif // _ASPIA_SAS_H
