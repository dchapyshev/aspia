//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/service.h"

#include "base/logging.h"
#include "base/crypto/scoped_crypto_initializer.h"
#include "base/message_loop/message_loop_task_runner.h"

namespace base {

Service::Service(std::u16string_view name, MessageLoop::Type type)
    : type_(type),
      name_(name)
{
    LOG(LS_INFO) << "Ctor";
}

Service::~Service()
{
    LOG(LS_INFO) << "Dtor";
}

void Service::exec()
{
    LOG(LS_INFO) << "Begin";

    std::unique_ptr<ScopedCryptoInitializer> crypto_initializer =
        std::make_unique<ScopedCryptoInitializer>();
    CHECK(crypto_initializer->isSucceeded());

    message_loop_ = std::make_unique<MessageLoop>(type_);
    task_runner_ = message_loop_->taskRunner();

    message_loop_->run();
    message_loop_.reset();

    LOG(LS_INFO) << "End";
}

} // namespace base
