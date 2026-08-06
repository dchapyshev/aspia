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

#include "router/workers/client_worker.h"

#include <gtest/gtest.h>

namespace {

// The live sessions of a router: two ordinary ones and the administrator console that sends the
// commands (the last id).
const std::vector<qint64> kSessions = { 1, 2, 7 };
constexpr qint64 kAdminSession = 7;
constexpr qint64 kAllSessions = -1;

} // namespace

//--------------------------------------------------------------------------------------------------
// "Disconnect everybody" must leave the console that asked for it alone: the administrator would
// otherwise drop its own connection and have to pass the two-factor stage again to see the result
// of its own command. Every other command of the admin channel that acts on sessions follows the
// same rule.
TEST(ClientWorkerTest, DisconnectAllSpareTheRequestingSession)
{
    const std::vector<qint64> targets =
        ClientWorker::sessionsToStop(kSessions, kAllSessions, kAdminSession);

    EXPECT_EQ(targets, std::vector<qint64>({ 1, 2 }));
}

//--------------------------------------------------------------------------------------------------
// Nothing else is connected: the command has nothing to do, which is not a failure.
TEST(ClientWorkerTest, DisconnectAllWithNobodyElseSelectsNothing)
{
    const std::vector<qint64> targets =
        ClientWorker::sessionsToStop({ kAdminSession }, kAllSessions, kAdminSession);

    EXPECT_TRUE(targets.empty());
}

//--------------------------------------------------------------------------------------------------
TEST(ClientWorkerTest, DisconnectOneTargetsExactlyThatSession)
{
    const std::vector<qint64> targets = ClientWorker::sessionsToStop(kSessions, 2, kAdminSession);

    EXPECT_EQ(targets, std::vector<qint64>({ 2 }));
}

//--------------------------------------------------------------------------------------------------
// Picking your own session in the list is an explicit decision, so it is carried out.
TEST(ClientWorkerTest, OwnSessionCanBePickedExplicitly)
{
    const std::vector<qint64> targets =
        ClientWorker::sessionsToStop(kSessions, kAdminSession, kAdminSession);

    EXPECT_EQ(targets, std::vector<qint64>({ kAdminSession }));
}

//--------------------------------------------------------------------------------------------------
// The session ended between the list and the command: nothing is selected, and the caller answers
// with an invalid entry id instead of silently reporting success.
TEST(ClientWorkerTest, UnknownSessionSelectsNothing)
{
    EXPECT_TRUE(ClientWorker::sessionsToStop(kSessions, 12345, kAdminSession).empty());
    EXPECT_TRUE(ClientWorker::sessionsToStop({}, 1, kAdminSession).empty());
}
