/*
 *  Copyright (C) 2019 Haiku, Inc.
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

#include "ProcessExecutablePath.h"

#include <Roster.h>
#include <StackOrHeapArray.h>
#include <String.h>
#include <unistd.h>

#include <assert.h>
#include <unistd.h>

using namespace WebCore;

namespace WebKit {

// TODO we could have a single instance of this instead of creating and deleting one everytime.
// But it would need a way to either locate the ProcessLauncher from the processIdentifier,
// or to store the ProcessLauncher pointer into the message sent to the other process and back
// into the reply
//
// Another option would be to have the BHandler already created by the connectionIdentifier
// and then ownership transferred to the corresponding ConnectionHaiku when it is created, instead
// of creating yet another BHandler there.
class ProcessLauncherHandler: public BHandler
{
    public:
        ProcessLauncherHandler(ProcessLauncher* launcher)
            : BHandler("process launcher")
            , m_launcher(launcher)
        {
        }

        void MessageReceived(BMessage* message)
        {
            switch(message->what)
            {
                case 'inig':
                    GlobalMessage(message);
                    break;
                default:
                    BHandler::MessageReceived(message);
                    break;
            }
        }

    private:

        void GlobalMessage(BMessage* message)
        {
            WTF::ProcessID processID = message->FindInt64("processID");
            IPC::Connection::Identifier connectionIdentifier;
            message->FindMessenger("messenger", &connectionIdentifier.handle);
            connectionIdentifier.m_isCreatedFromMessage = true;
            m_launcher->didFinishLaunchingProcess(processID, connectionIdentifier);

            // Our job is done!
            // FIXME or is it? maybe keep this around until the processLauncher is destroyed?
            delete this;
        }

        ProcessLauncher* m_launcher;
};

static BHandler* GetProcessLauncherHandler(ProcessLauncher* launcher)
{
    BHandler* handle = nullptr;
    handle = new ProcessLauncherHandler(launcher);
    BLooper* looper = BLooper::LooperForThread(find_thread(NULL));
    looper->AddHandler(handle);

    return handle;
}

static status_t GetRefForPath(BString path, entry_ref* pathRef)
{
    BEntry pathEntry(path);
    if(!pathEntry.Exists())
        return B_BAD_VALUE;

    status_t result = pathEntry.GetRef(pathRef);
    if(result != B_OK)
        return result;

    return B_OK;
}

void ProcessLauncher::launchProcess()
{
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

    BString processIdentifierString;
    processIdentifierString.SetToFormat("%" PRIu64, m_launchOptions.processIdentifier.toUInt64());

    BHandler* connectionHandler = GetProcessLauncherHandler(this);
    BMessenger messenger(connectionHandler);
    BString connectionIdentifierString;

    for (size_t i = 0; i < sizeof(BMessenger); i++) {
        char formatted[3];
        uint8 byte = ((uint8*)&messenger)[i];
        sprintf(formatted, "%02x", byte);
        connectionIdentifierString.Append(formatted);
    }

    int argc = 2;
    const char* argv[] = {
        processIdentifierString.String(),
        connectionIdentifierString.String(),
        nullptr
    };

    entry_ref executableRef;
    if(GetRefForPath(executablePath, &executableRef) != B_OK)
    {
        return;
    }

    team_id child_id;
    be_roster->Launch(&executableRef, argc, argv, &child_id);

    // When the process is launched, it will reply by sending the 'inig' message with the
    // connectionIdentifier that we can use to communicate with it. This triggers the call to
    // didFinishLaunchingProcess which starts establishing the connection
}

void ProcessLauncher::terminateProcess()
{
    if (m_isLaunching) {
        invalidate();
        return;
    }

    if (!m_processIdentifier)
        return;

    kill(m_processIdentifier, SIGKILL);
    m_processIdentifier = 0;
}

void ProcessLauncher::platformInvalidate()
{
}

} // namespace WebKit

