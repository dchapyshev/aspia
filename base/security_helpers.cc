//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/security_helpers.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/security_helpers.h"
#include "base/scoped_object.h"
#include "base/scoped_local.h"
#include "base/strings/string_util.h"
#include "base/strings/unicode.h"
#include "base/logging.h"

#include <objidl.h>
#include <sddl.h>
#include <memory>

namespace aspia {

bool MakeScopedAbsoluteSd(const ScopedSd& relative_sd,
                          ScopedSd* absolute_sd,
                          ScopedAcl* dacl,
                          ScopedSid* group,
                          ScopedSid* owner,
                          ScopedAcl* sacl)
{
    // Get buffer sizes.
    DWORD absolute_sd_size = 0;
    DWORD dacl_size = 0;
    DWORD group_size = 0;
    DWORD owner_size = 0;
    DWORD sacl_size = 0;

    if (MakeAbsoluteSD(relative_sd.get(),
                       nullptr,
                       &absolute_sd_size,
                       nullptr,
                       &dacl_size,
                       nullptr,
                       &sacl_size,
                       nullptr,
                       &owner_size,
                       nullptr,
                       &group_size) ||
        GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    {
        LOG(ERROR) << "MakeAbsoluteSD() failed: " << GetLastSystemErrorString();
        return false;
    }

    // Allocate buffers.
    ScopedSd local_absolute_sd(absolute_sd_size);
    ScopedAcl local_dacl(dacl_size);
    ScopedSid local_group(group_size);
    ScopedSid local_owner(owner_size);
    ScopedAcl local_sacl(sacl_size);

    // Do the conversion.
    if (!MakeAbsoluteSD(relative_sd.get(),
                        local_absolute_sd.get(),
                        &absolute_sd_size,
                        reinterpret_cast<PACL>(local_dacl.get()),
                        &dacl_size,
                        local_sacl.get(),
                        &sacl_size,
                        local_owner.get(),
                        &owner_size,
                        local_group.get(),
                        &group_size))
    {
        LOG(ERROR) << "MakeAbsoluteSD() failed: " << GetLastSystemErrorString();
        return false;
    }

    absolute_sd->Swap(local_absolute_sd);
    dacl->Swap(local_dacl);
    group->Swap(local_group);
    owner->Swap(local_owner);
    sacl->Swap(local_sacl);

    return true;
}

bool InitializeComSecurity(const std::wstring& security_descriptor,
                           const std::wstring& mandatory_label,
                           bool activate_as_activator)
{
    std::wstring sddl = security_descriptor + mandatory_label;

    // Convert the SDDL description into a security descriptor in absolute format.
    ScopedSd relative_sd = ConvertSddlToSd(sddl);
    if (!relative_sd)
    {
        LOG(ERROR) << "Failed to create a security descriptor";
        return false;
    }

    ScopedSd absolute_sd;
    ScopedAcl dacl;
    ScopedSid group;
    ScopedSid owner;
    ScopedAcl sacl;

    if (!MakeScopedAbsoluteSd(relative_sd, &absolute_sd, &dacl, &group, &owner, &sacl))
    {
        LOG(ERROR) << "MakeScopedAbsoluteSd() failed";
        return false;
    }

    DWORD capabilities = EOAC_DYNAMIC_CLOAKING;
    if (!activate_as_activator)
        capabilities |= EOAC_DISABLE_AAA;

    // Apply the security descriptor and default security settings. See
    // InitializeComSecurity's declaration for details.
    HRESULT result = CoInitializeSecurity(absolute_sd.get(),
                                          -1,          // Let COM choose which authentication services to register.
                                          nullptr,     // See above.
                                          nullptr,     // Reserved, must be nullptr.
                                          RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
                                          RPC_C_IMP_LEVEL_IDENTIFY,
                                          nullptr,     // Default authentication information is not provided.
                                          capabilities,
                                          nullptr);    // Reserved, must be nullptr
    if (FAILED(result))
    {
        LOG(ERROR) << "CoInitializeSecurity() failed: " << result;
        return false;
    }

    return true;
}

bool GetUserSidString(std::wstring& user_sid)
{
    // Get the current token.
    ScopedHandle token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, token.Recieve()))
        return false;

    DWORD size = sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE;
    std::unique_ptr<BYTE[]> user_bytes(new BYTE[size]);

    TOKEN_USER* user = reinterpret_cast<TOKEN_USER*>(user_bytes.get());

    if (!GetTokenInformation(token, TokenUser, user, size, &size))
        return false;

    if (!user->User.Sid)
        return false;

    // Convert the data to a string.
    ScopedLocal<WCHAR*> sid_string;

    if (!ConvertSidToStringSidW(user->User.Sid, sid_string.Recieve()))
        return false;

    user_sid.assign(sid_string);

    return true;
}

ScopedSd ConvertSddlToSd(const std::wstring& sddl)
{
    ScopedLocal<PSECURITY_DESCRIPTOR> raw_sd;
    ULONG length = 0;

    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(),
                                                              SDDL_REVISION_1,
                                                              raw_sd.Recieve(),
                                                              &length))
    {
        LOG(ERROR) << "ConvertStringSecurityDescriptorToSecurityDescriptorW() failed: "
                   << GetLastSystemErrorString();
        return ScopedSd();
    }

    ScopedSd sd(length);
    memcpy(sd.get(), raw_sd, length);

    return sd;
}

} // namespace aspia
