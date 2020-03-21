//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/host_server.h"

#include "base/logging.h"
#include "base/task_runner.h"
#include "base/files/base_paths.h"
#include "base/files/file_path_watcher.h"
#include "host/client_session.h"
#include "net/firewall_manager.h"
#include "net/network_channel.h"

namespace host {

namespace {

const wchar_t kFirewallRuleName[] = L"Aspia Host Service";
const wchar_t kFirewallRuleDecription[] = L"Allow incoming TCP connections";

} // namespace

Server::Server(std::shared_ptr<base::TaskRunner> task_runner)
    : task_runner_(std::move(task_runner))
{
    DCHECK(task_runner_);
}

Server::~Server()
{
    LOG(LS_INFO) << "Stopping the server...";

    settings_watcher_.reset();
    authenticator_manager_.reset();
    user_session_manager_.reset();
    network_server_.reset();

    deleteFirewallRules();

    LOG(LS_INFO) << "Server is stopped";
}

void Server::start()
{
    if (network_server_)
    {
        DLOG(LS_WARNING) << "An attempt was start an already running server";
        return;
    }

    LOG(LS_INFO) << "Starting the host server";

    settings_watcher_ = std::make_unique<base::FilePathWatcher>(task_runner_);
    settings_watcher_->watch(settings_.filePath(), false,
        [this](const std::filesystem::path& path, bool error)
    {
        LOG(LS_INFO) << "Configuration file change detected";

        if (!error)
        {
            DCHECK_EQ(path, settings_.filePath());

            // Synchronize the parameters from the file.
            settings_.sync();

            // Reload user lists.
            reloadUserList();
        }
    });

    authenticator_manager_ = std::make_unique<AuthenticatorManager>(task_runner_, this);

    user_session_manager_ = std::make_unique<UserSessionManager>(task_runner_);
    user_session_manager_->start(this);

    reloadUserList();
    addFirewallRules();

    network_server_ = std::make_unique<net::Server>();
    network_server_->start(settings_.tcpPort(), this);

    LOG(LS_INFO) << "Host server is started successfully";
}

void Server::setSessionEvent(base::win::SessionStatus status, base::SessionId session_id)
{
    if (user_session_manager_)
        user_session_manager_->setSessionEvent(status, session_id);
}

void Server::onNewConnection(std::unique_ptr<net::Channel> channel)
{
    if (authenticator_manager_)
        authenticator_manager_->addNewChannel(std::move(channel));
}

void Server::onNewSession(std::unique_ptr<ClientSession> session)
{
    if (user_session_manager_)
        user_session_manager_->addNewSession(std::move(session));
}

void Server::onUserListChanged()
{
    reloadUserList();
}

void Server::addFirewallRules()
{
    std::filesystem::path file_path;
    if (!base::BasePaths::currentExecFile(&file_path))
        return;

    net::FirewallManager firewall(file_path);
    if (!firewall.isValid())
        return;

    if (!firewall.addTcpRule(kFirewallRuleName, kFirewallRuleDecription, settings_.tcpPort()))
        return;

    LOG(LS_INFO) << "Rule is added to the firewall";
}

void Server::deleteFirewallRules()
{
    std::filesystem::path file_path;
    if (!base::BasePaths::currentExecFile(&file_path))
        return;

    net::FirewallManager firewall(file_path);
    if (!firewall.isValid())
        return;

    firewall.deleteRuleByName(kFirewallRuleName);
}

void Server::reloadUserList()
{
    // Read the list of regular users.
    std::shared_ptr<UserList> user_list = std::make_shared<UserList>(settings_.userList());

    // Add a list of one-time users to the list of regular users.
    user_list->merge(user_session_manager_->userList());

    // Updating the list of users.
    authenticator_manager_->setUserList(user_list);
}

} // namespace host
