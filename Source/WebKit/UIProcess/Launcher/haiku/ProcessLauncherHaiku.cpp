/*
 * Copyright (C) 2019, 2024 Haiku, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ProcessLauncher.h"

#include "IPCUtilities.h"
#include "ProcessExecutablePath.h"

#include <assert.h>
#include <Logging.h>
#include <Roster.h>
#include <String.h>
#include <spawn.h>
#include <unistd.h>

#include <sys/socket.h>

using namespace WebCore;

namespace WebKit {

void ProcessLauncher::launchProcess()
{
    IPC::SocketPair socketPair = IPC::createPlatformConnection(SOCK_DGRAM, 
        IPC::PlatformConnectionOptions::SetCloexecOnClient | IPC::PlatformConnectionOptions::SetCloexecOnServer);

    BString executablePath;
    switch (m_launchOptions.processType) {
    case ProcessLauncher::ProcessType::Web:
        executablePath = executablePathOfWebProcess();
        break;
    case ProcessLauncher::ProcessType::Network:
        executablePath = executablePathOfNetworkProcess();
        break;
    default:
        ASSERT_NOT_REACHED();
        return;
    }

    BString processIdentifier;
    processIdentifier.SetToFormat("%" PRIu64, m_launchOptions.processIdentifier.toUInt64());

    BString clientSocketFd;
    clientSocketFd.SetToFormat("%d", socketPair.client.value());

    char* argv[] = {
        executablePath.LockBuffer(-1),
        processIdentifier.LockBuffer(-1),
        clientSocketFd.LockBuffer(-1),
        nullptr
    };

    char* envp[] = {
        nullptr
    };

    posix_spawn_file_actions_t file_actions;
    posix_spawn_file_actions_init(&file_actions);
    posix_spawn_file_actions_adddup2(&file_actions, socketPair.client.value(), socketPair.client.value());
        // make client socket available to child process. This is necessary since
        // CLOEXEC is set on it.
    posix_spawn_file_actions_destroy(&file_actions);

    int status = posix_spawn(&m_processID, executablePath, &file_actions, NULL, argv, envp);

    if (status != 0)
        LOG(Process, "failed to start process %s, error %s", executablePath.String(), strerror(status));

    executablePath.UnlockBuffer();
    processIdentifier.UnlockBuffer();
    clientSocketFd.UnlockBuffer();

    RunLoop::mainSingleton().dispatch([protectedThis = Ref { *this }, this, serverIdentifier = WTFMove(socketPair.server)] mutable {
        protectedThis->didFinishLaunchingProcess(m_processID, IPC::Connection::Identifier { WTFMove(serverIdentifier) });
    });
}

void ProcessLauncher::terminateProcess()
{
    if (m_isLaunching) {
        invalidate();
        return;
    }

    if (!m_processID)
        return;

    kill(m_processID, SIGKILL);
    m_processID = 0;
}

void ProcessLauncher::platformInvalidate()
{
}

} // namespace WebKit

