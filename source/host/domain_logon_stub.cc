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

#include "host/domain_logon.h"

#include "base/logging.h"

//--------------------------------------------------------------------------------------------------
DomainLogonResult domainLogon(const QString& /* domain */,
                              const QString& /* username */,
                              const SecureString& /* password */,
                              const QString& /* peer_address */,
                              const QString& /* pam_service */)
{
    LOG(ERROR) << "Domain logon is not supported on this platform";

    DomainLogonResult result;
    result.status = DomainAuthenticator::Status::UNSUPPORTED;
    return result;
}

//--------------------------------------------------------------------------------------------------
bool resolveDomainAccount(const QString& /* text */, DomainAccountInfo* /* info */, QString* error)
{
    if (error)
        *error = QStringLiteral("Not supported on this platform");
    return false;
}

//--------------------------------------------------------------------------------------------------
bool resolveLegacyDomainRule(const QString& text, DomainAccountInfo* info, QString* error)
{
    return resolveDomainAccount(text, info, error);
}

//--------------------------------------------------------------------------------------------------
bool isUsablePamService(const QString& /* name */, QString* /* error */)
{
    // PAM is not used on this platform.
    return true;
}
