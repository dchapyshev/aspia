//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/win/security_helpers.h"

#include <comdef.h>
#include <ObjIdl.h>
#include <sddl.h>

#include "base/logging.h"
#include "base/win/scoped_local.h"
#include "base/win/scoped_object.h"

namespace base {

namespace {

//--------------------------------------------------------------------------------------------------
bool makeScopedAbsoluteSd(const ScopedSd& relative_sd,
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
        PLOG(ERROR) << "MakeAbsoluteSD failed";
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
        PLOG(ERROR) << "MakeAbsoluteSD failed";
        return false;
    }

    absolute_sd->swap(local_absolute_sd);
    dacl->swap(local_dacl);
    group->swap(local_group);
    owner->swap(local_owner);
    sacl->swap(local_sacl);

    return true;
}

} // namespace

//--------------------------------------------------------------------------------------------------
bool initializeComSecurity(const wchar_t* security_descriptor,
                           const wchar_t* mandatory_label,
                           bool activate_as_activator)
{
    QString sddl;

    sddl.append(QString::fromWCharArray(security_descriptor));
    sddl.append(QString::fromWCharArray(mandatory_label));

    // Convert the SDDL description into a security descriptor in absolute format.
    ScopedSd relative_sd = convertSddlToSd(sddl);
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

    if (!makeScopedAbsoluteSd(relative_sd, &absolute_sd, &dacl,
                              &group, &owner, &sacl))
    {
        LOG(ERROR) << "MakeScopedAbsoluteSd failed";
        return false;
    }

    DWORD capabilities = EOAC_DYNAMIC_CLOAKING;
    if (!activate_as_activator)
        capabilities |= EOAC_DISABLE_AAA;

    // Apply the security descriptor and default security settings. See
    // InitializeComSecurity's declaration for details.
    _com_error result = CoInitializeSecurity(
        absolute_sd.get(),
        -1,        // Let COM choose which authentication services to register.
        nullptr,   // See above.
        nullptr,   // Reserved, must be nullptr.
        RPC_C_AUTHN_LEVEL_PKT_PRIVACY,
        RPC_C_IMP_LEVEL_IDENTIFY,
        nullptr,   // Default authentication information is not provided.
        capabilities,
        nullptr);  // Reserved, must be nullptr
    if (FAILED(result.Error()))
    {
        LOG(ERROR) << "CoInitializeSecurity failed:" << result;
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
ScopedSd convertSddlToSd(const QString& sddl)
{
    ScopedLocal<PSECURITY_DESCRIPTOR> raw_sd;
    ULONG length = 0;

    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(qUtf16Printable(sddl), SDDL_REVISION_1,
        raw_sd.recieve(), &length))
    {
        PLOG(ERROR) << "ConvertStringSecurityDescriptorToSecurityDescriptorW failed";
        return ScopedSd();
    }

    ScopedSd sd(length);
    memcpy(sd.get(), raw_sd, length);

    return sd;
}

//--------------------------------------------------------------------------------------------------
bool userSidString(QString* user_sid)
{
    // Get the current token.
    ScopedHandle token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, token.recieve()))
    {
        PLOG(ERROR) << "OpenProcessToken failed";
        return false;
    }

    DWORD size = sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE;
    std::unique_ptr<BYTE[]> user_bytes = std::make_unique<BYTE[]>(size);

    TOKEN_USER* user = reinterpret_cast<TOKEN_USER*>(user_bytes.get());

    if (!GetTokenInformation(token, TokenUser, user, size, &size))
    {
        PLOG(ERROR) << "GetTokenInformation failed";
        return false;
    }

    if (!user->User.Sid)
        return false;

    // Convert the data to a string.
    ScopedLocal<wchar_t*> sid_string;

    if (!ConvertSidToStringSidW(user->User.Sid, sid_string.recieve()))
    {
        PLOG(ERROR) << "ConvertSidToStringSidW failed";
        return false;
    }

    *user_sid = QString::fromWCharArray(sid_string);
    return true;
}

} // namespace base::win
