/*
 * Copyright (C) 2019 Haiku, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS''
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
#include "NetworkProcessMain.h"

#include "AuxiliaryProcessMain.h"
#include "NetworkProcess.h"

namespace WebKit {

class NetworkProcessMainHaiku: public AuxiliaryProcessMainBaseNoSingleton<NetworkProcess> {
public:
    void platformFinalize() override
    {
        /*process().destroySession(PAL::SessionID::defaultSessionID());*/
    }
};

class NetworkProcessApp : public BApplication {
public:
    NetworkProcessApp()
        : BApplication("application/x-vnd-HaikuWebKit-NetworkProcess") {}

    void ArgvReceived(int argc, char** argv)
    {
        AuxiliaryProcessMain<NetworkProcessMainHaiku>(argc, argv);
    }
};

int NetworkProcessMain(int argc, char** argv)
{
    // Instead of calling AuxiliaryProcessMain directly as the other ports do,
    // we need to wrap it in a BApplication. RunLoop currently requires
    // WebKit's code to be running from a BApplication.
    NetworkProcessApp* app = new NetworkProcessApp();
    app->Run();
    delete app;
    return 0;
}

} // namespace WebKit
