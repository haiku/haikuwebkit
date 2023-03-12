/*
 * Copyright (C) 2014 Igalia S.L.
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
#include "AuxiliaryProcessMain.h"

#include <WebCore/ProcessIdentifier.h>
#include <stdlib.h>

namespace WebKit {

AuxiliaryProcessMainCommon::AuxiliaryProcessMainCommon()
{
#if ENABLE(BREAKPAD)
    installBreakpadExceptionHandler();
#endif
}

bool AuxiliaryProcessMainCommon::parseCommandLine(int argc, char** argv)
{
    if (argc < 3) {
        return false;
    }

    m_parameters.processIdentifier = ObjectIdentifier<WebCore::ProcessIdentifierType>(atoll(argv[1]));

    const char* ptr = argv[2];

    for (size_t i = 0; i < sizeof(BMessenger); i++) {
        uint8_t tmp2;
        uint8_t tmp = ptr[0];
        if (tmp >= '0' && tmp <= '9')
            tmp -= '0';
        else if (tmp >= 'a' && tmp <= 'f') {
            tmp -= 'a';
            tmp += 10;
        }

        tmp2 = tmp << 4;
        ptr++;

        tmp = ptr[0];
        if (tmp >= '0' && tmp <= '9')
            tmp -= '0';
        else if (tmp >= 'a' && tmp <= 'f') {
            tmp -= 'a';
            tmp += 10;
        }

        tmp2 |= tmp;
        ptr++;

        uint8_t* target = (uint8_t*)(&m_parameters.connectionIdentifier.handle);
        target[i] = tmp2;
    }

    return true;
}

void AuxiliaryProcess::platformInitialize(const AuxiliaryProcessInitializationParameters&)
{
    notImplemented();
}

} // namespace WebKit
