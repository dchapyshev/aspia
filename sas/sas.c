/*
* PROJECT:         Aspia Remote Desktop
* FILE:            sas/sas.c
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include <windows.h>
#include <wtsapi32.h>
#include <versionhelpers.h>
#include <process.h>
#include <strsafe.h>
#include "sas.h"

/*
 * Declarations
 */
#define UUID_LENGTH               (36 + 1)

#define TYPE_FORMAT_STRING_SIZE   164
#define PROC_FORMAT_STRING_SIZE   38

typedef struct _MIDL_TYPE_FORMAT_STRING
{
    SHORT Pad;
    UCHAR Format[TYPE_FORMAT_STRING_SIZE];
} MIDL_TYPE_FORMAT_STRING;

typedef struct _MIDL_PROC_FORMAT_STRING
{
    SHORT Pad;
    UCHAR Format[PROC_FORMAT_STRING_SIZE];
} MIDL_PROC_FORMAT_STRING;

typedef DWORD(WINAPI *PWMSGSENDMESSAGE)(DWORD dwSessionId, UINT uMsg, WPARAM wParam, LPARAM lParam);

/*
 * Declarations
 */
static const RPC_CLIENT_INTERFACE SecureDesktop_RpcClientInterface =
{
    sizeof(RPC_CLIENT_INTERFACE),
    { { 0x12E65DD8, 0x887F, 0x41EF, { 0x91, 0xBF, 0x8D, 0x81, 0x6C, 0x42, 0xC2, 0xE7 } }, { 1, 0 } },
    { { 0x8A885D04, 0x1CEB, 0x11C9, { 0x9F, 0xE8, 0x08, 0x00, 0x2B, 0x10, 0x48, 0x60 } }, { 2, 0 } },
    0,
    0,
    NULL,
    0,
    0,
    0
};

static RPC_BINDING_HANDLE MIDL_AutoBindHandle;

static const MIDL_PROC_FORMAT_STRING MIDL_ProcFormatString =
{
    0,
    {
        0, 0x48, 0, 0, 0, 0, 4, 0, 8, 0, 0x32, 0, 0, 0, 0, 0, 8, 0, 0x44,
        1, 8, 1, 0, 0, 0, 0, 0, 0, 0x70, 0, 4, 0, 8, 0, 0, 0, 0, 0
    }
};

static const MIDL_TYPE_FORMAT_STRING MIDL_TypeFormatString =
{
    0,
    {
        0x15, 3, 4, 0, 8, 0x5B, 0x1C, 0, 1, 0, 0x19, 0, 0x28, 0, 1, 0, 0x19, 0, 0x28,
        0, 1, 0, 2, 0x5B, 0x1A, 3, 0x34, 0, 0, 0, 0x12, 0, 0x0D, 0x36, 0x36, 0x36, 0x36,
        8, 6, 6, 0x0D, 8, 8, 8, 0x36, 8, 0x5C, 0x5B, 0x12, 0, 0xCE, 0xFF, 0x12, 8, 0x25,
        0x5C, 0x12, 8, 0x25, 0x5C, 0x12, 8, 0x25, 0x5C, 0x12, 0x20, 0xC4, 0xFF, 0x12, 8,
        0x25, 0x5C, 0x12, 0x20, 2, 0, 0x1B, 0, 1, 0, 0x29, 0, 0x54, 0, 0, 0, 2, 0x5B,
        0x11, 0x14, 2, 0, 0x12, 0x20, 2, 0, 0x1B, 0, 1, 0, 0x29, 0x54, 0x64, 0, 0, 0, 2,
        0x5B, 0x11, 0x0C, 8, 0x5C, 0x11, 8, 8, 0x5C, 0x11, 0x14, 2, 0, 0x12, 0x20, 2, 0,
        0x1B, 1, 2, 0, 0x29, 0x54, 0x1C, 0, 0, 0, 5, 0x5B, 0x11, 0x14, 2, 0, 0x12, 0x20,
        2, 0, 0x1B, 0, 1, 0, 0x29, 0x54, 0x24, 0, 0, 0, 2, 0x5B, 0, 0, 0, 0, 0, 0, 0, 0
    }
};

static LPVOID WINAPI
MIDL_UserAllocate(size_t dwBytes)
{
    return malloc(dwBytes);
}

static VOID WINAPI
MIDL_UserFree(LPVOID lpMem)
{
    free(lpMem);
}

static const MIDL_STUB_DESC MIDL_StubDescriptor =
{
    (LPVOID)&SecureDesktop_RpcClientInterface,
    MIDL_UserAllocate,
    MIDL_UserFree,
    { &MIDL_AutoBindHandle },
    0,
    0,
    0,
    0,
    MIDL_TypeFormatString.Format,
    1, /* -error bounds_check flag */
    0x60001, /* Ndr library version */
    0,
    0x700022B, /* MIDL Version 7.0.555 */
    0,
    0,
    0, /* notify & notify_flag routine table */
    1, /* Flags */
    0, /* Reserved3 */
    0, /* Reserved4 */
    0  /* Reserved5 */
};

static RPC_BINDING_HANDLE
GetBindingHandle(const WCHAR *SessionUuid)
{
    RPC_BINDING_HANDLE BindingHandle = NULL;
    RPC_WSTR BindingString = NULL;
    RPC_STATUS Status;

    Status = RpcStringBindingComposeW((RPC_WSTR)SessionUuid,
                                      (RPC_WSTR)L"ncalrpc",
                                      NULL,
                                      NULL,
                                      NULL,
                                      &BindingString);
    if (Status == RPC_S_OK)
    {
        Status = RpcBindingFromStringBindingW(BindingString, &BindingHandle);
        if (Status == RPC_S_OK)
        {
            RPC_SECURITY_QOS_V3_W RpcSecurityQOS = { 0 };
            SID Sid = { 0 };

            /* Fill SID structure */
            Sid.Revision = 1;
            Sid.SubAuthorityCount = 1;
            Sid.IdentifierAuthority.Value[5] = 5;
            Sid.SubAuthority[0] = 18;

            /* Fill RPC_SECURITY_QOS_V3 structure */
            RpcSecurityQOS.Version           = RPC_C_SECURITY_QOS_VERSION_3;
            RpcSecurityQOS.ImpersonationType = RPC_C_IMP_LEVEL_IMPERSONATE;
            RpcSecurityQOS.Capabilities      = RPC_C_QOS_CAPABILITIES_MUTUAL_AUTH;
            RpcSecurityQOS.IdentityTracking  = RPC_C_QOS_IDENTITY_STATIC;
            RpcSecurityQOS.Sid               = &Sid;

            Status = RpcBindingSetAuthInfoExW(BindingHandle,
                                              NULL,
                                              RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
                                              RPC_C_AUTHN_WINNT, NULL,
                                              RPC_C_AUTHZ_NONE,
                                              (RPC_SECURITY_QOS*)&RpcSecurityQOS);
            if (Status != RPC_S_OK)
            {
                RpcBindingFree(&BindingHandle);
                BindingHandle = NULL;
            }
        }

        RpcStringFreeW(&BindingString);
    }

    return BindingHandle;
}

static VOID
SendSAS_AsUser(DWORD SessionId)
{
    RPC_BINDING_HANDLE Handle;
    WCHAR szSessionUuid[UUID_LENGTH];
    HRESULT Status;

    Status = StringCbPrintfW(szSessionUuid,
                             sizeof(szSessionUuid),
                             L"52EF130C-08FD-4388-86B3-6EDF%08X",
                             SessionId);
    if (SUCCEEDED(Status))
    {
        Handle = GetBindingHandle(szSessionUuid);
        if (Handle != NULL)
        {
            NdrClientCall2(&MIDL_StubDescriptor,
                           &MIDL_ProcFormatString.Format[0],
                           &Handle);

            RpcBindingFree(&Handle);
        }
    }
}

static VOID
SendSAS_AsSystem(DWORD SessionId)
{
    HMODULE hModule;

    hModule = LoadLibraryW(L"wmsgapi.dll");
    if (hModule != NULL)
    {
        PWMSGSENDMESSAGE pWmsgSendMessage;

        pWmsgSendMessage = (PWMSGSENDMESSAGE)GetProcAddress(hModule, "WmsgSendMessage");
        if (pWmsgSendMessage != NULL)
        {
            BOOL AsUser = FALSE;

            pWmsgSendMessage(SessionId, 0x208, 0, (LPARAM)&AsUser);
        }

        FreeLibrary(hModule);
    }
}

static BOOL
IsExistingSession(DWORD SessionId)
{
    PWTS_SESSION_INFOW SessionInfo;
    DWORD SessionCount;
    BOOL Found = FALSE;

    if (WTSEnumerateSessionsW(WTS_CURRENT_SERVER_HANDLE,
                              0,
                              1,
                              &SessionInfo,
                              &SessionCount) != FALSE)
    {
        DWORD i;

        for (i = 0; i < SessionCount; i++)
        {
            if (SessionId == SessionInfo[i].SessionId)
            {
                Found = TRUE;
                break;
            }
        }

        WTSFreeMemory(SessionInfo);
    }

    return Found;
}

static BOOL
EnableSoftwareSASGeneration(VOID)
{
    DWORD Disposition;
    HKEY hPoliciesKey;
    BOOL Result = FALSE;

    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE,
                        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies",
                        0,
                        REG_NONE,
                        REG_OPTION_NON_VOLATILE,
                        KEY_READ,
                        NULL,
                        &hPoliciesKey,
                        &Disposition) == ERROR_SUCCESS)
    {
        HKEY hSystemKey;

        if (RegOpenKeyExW(hPoliciesKey,
                          L"System",
                          0,
                          KEY_WRITE | KEY_READ,
                          &hSystemKey) == ERROR_SUCCESS)
        {
            DWORD Value = 3;

            if (RegSetValueExW(hSystemKey,
                               L"SoftwareSASGeneration",
                               0,
                               REG_DWORD,
                               (LPBYTE)&Value,
                               sizeof(Value)) == ERROR_SUCCESS)
            {
                Result = TRUE;
            }

            RegCloseKey(hSystemKey);
        }

        RegCloseKey(hPoliciesKey);
    }

    return Result;
}

static BOOL
IsSoftwareSASGenerationEnabled(VOID)
{
    BOOL Result = FALSE;
    HKEY hKey;

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
                      0,
                      KEY_READ,
                      &hKey) == ERROR_SUCCESS)
    {
        DWORD dwType = REG_DWORD;
        DWORD dwSize = sizeof(DWORD);
        DWORD dwValue = 0;

        if (RegQueryValueExW(hKey,
                             L"SoftwareSASGeneration",
                             NULL,
                             &dwType,
                             (LPBYTE)&dwValue,
                             &dwSize) == ERROR_SUCCESS)
        {
            if (dwValue == 3)
                Result = TRUE;
        }

        RegCloseKey(hKey);
    }

    return Result;
}

static BOOL
ImpersonateAsUser(DWORD SessionId)
{
    HANDLE hUserToken;
    BOOL Result = FALSE;

    if (WTSQueryUserToken(SessionId, &hUserToken) != FALSE)
    {
        HANDLE hThread;

        hThread = OpenThread(THREAD_IMPERSONATE | THREAD_QUERY_INFORMATION |
                                 THREAD_SET_THREAD_TOKEN,
                             TRUE,
                             GetCurrentThreadId());
        if (hThread != NULL)
        {
            HANDLE hDuplicateUserToken = NULL;

            if (DuplicateTokenEx(hUserToken,
                                 TOKEN_ALL_ACCESS,
                                 NULL,
                                 SecurityImpersonation,
                                 TokenImpersonation,
                                 &hDuplicateUserToken) != FALSE)
            {
                if (SetTokenInformation(hDuplicateUserToken,
                                        TokenSessionId,
                                        &SessionId,
                                        sizeof(SessionId)) != FALSE)
                {
                    if (ImpersonateLoggedOnUser(hDuplicateUserToken) != FALSE)
                    {
                        Result = TRUE;
                    }
                }

                CloseHandle(hDuplicateUserToken);
            }

            CloseHandle(hThread);
        }

        CloseHandle(hUserToken);
    }

    return Result;
}

VOID WINAPI
SendSAS_XP(VOID)
{
    HDESK hDesktop;

    hDesktop = OpenDesktopW(L"Winlogon",
                            0,
                            TRUE,
                            DESKTOP_CREATEMENU | DESKTOP_CREATEWINDOW |
                               DESKTOP_ENUMERATE | DESKTOP_HOOKCONTROL |
                               DESKTOP_WRITEOBJECTS | DESKTOP_READOBJECTS |
                               DESKTOP_SWITCHDESKTOP | GENERIC_WRITE);

    if (hDesktop != NULL)
    {
        if (SetThreadDesktop(hDesktop) != FALSE)
        {
            HWND hWindow;

            hWindow = FindWindowW(L"SAS window class", L"SAS window");
            if (hWindow == NULL)
                hWindow = HWND_BROADCAST;

            PostMessageW(hWindow,
                         WM_HOTKEY,
                         0,
                         MAKELONG(MOD_ALT | MOD_CONTROL, VK_DELETE));
        }

        CloseDesktop(hDesktop);
    }
}

VOID WINAPI
SendSAS_Vista(BOOL AsUser, DWORD SessionId)
{
    if (SessionId == DEFAULT_SESSION_ID)
    {
        if (ProcessIdToSessionId(GetCurrentProcessId(), &SessionId) == FALSE)
            return;

        if (SessionId == DEFAULT_SESSION_ID)
            return;
    }
    else
    {
        if (SessionId == 0 || IsExistingSession(SessionId) == FALSE)
            return;
    }

    if (AsUser != FALSE)
        SendSAS_AsUser(SessionId);
    else
        SendSAS_AsSystem(SessionId);
}

static UINT WINAPI
SendSAS_Thread(LPVOID lpParam)
{
    /* Переменная для блокировки одновременного выполнения операции */
    static BOOL Busy = FALSE;

    UNREFERENCED_PARAMETER(lpParam);

    /* Если команда уже выполняется */
    if (Busy != FALSE)
    {
        /* Завершаем поток и выходим */
        _endthreadex(0);
        return 0;
    }

    /* Ставим флаг, что команда выполняется */
    Busy = TRUE;

    /* Если версия ОС равна Vista или выше */
    if (IsWindowsVistaOrGreater() != FALSE)
    {
        DWORD SessionId;

        /* Если программное генерирование команды отключено */
        if (IsSoftwareSASGenerationEnabled() == FALSE)
        {
            /* Включаем его */
            EnableSoftwareSASGeneration();
        }

        /* Получаем ID текущей консольной сессии */
        SessionId = WTSGetActiveConsoleSessionId();
        if (SessionId != 0xFFFFFFFF)
        {
            /* Переводим поток в пространство пользователя */
            if (ImpersonateAsUser(SessionId) != FALSE)
            {
                /* Выполняем команду */
                SendSAS_Vista(FALSE, DEFAULT_SESSION_ID);

                /* Возвращаем поток в предыдущее состояние */
                RevertToSelf();
            }
        }
    }
    else
    {
        /* Выполняем команду для XP, 2003 */
        SendSAS_XP();
    }

    /* Ставим флаг завершения операции, завершаем поток, выходим */
    Busy = FALSE;
    _endthreadex(0);
    return 0;
}

VOID WINAPI
SendSAS(VOID)
{
    /* Запускаем поток для выполнения команды */
    _beginthreadex(NULL, 0, SendSAS_Thread, NULL, 0, NULL);
}
