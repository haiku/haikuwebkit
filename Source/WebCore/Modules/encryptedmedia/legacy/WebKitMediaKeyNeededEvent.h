/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)

#include "Event.h"
#include "WebKitMediaKeyError.h"

namespace WebCore {

class WebKitMediaKeyNeededEvent final : public Event {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(WebKitMediaKeyNeededEvent);
public:
    virtual ~WebKitMediaKeyNeededEvent();

    static Ref<WebKitMediaKeyNeededEvent> create(const AtomString& type, Uint8Array* initData)
    {
        return adoptRef(*new WebKitMediaKeyNeededEvent(type, initData));
    }

    struct Init : EventInit {
        RefPtr<Uint8Array> initData;
    };

    static Ref<WebKitMediaKeyNeededEvent> create(const AtomString& type, const Init& initializer, IsTrusted isTrusted = IsTrusted::No)
    {
        return adoptRef(*new WebKitMediaKeyNeededEvent(type, initializer, isTrusted));
    }

    Uint8Array* initData() const { return m_initData.get(); }

private:
    WebKitMediaKeyNeededEvent(const AtomString& type, Uint8Array* initData);
    WebKitMediaKeyNeededEvent(const AtomString& type, const Init&, IsTrusted);

    RefPtr<Uint8Array> m_initData;
};

} // namespace WebCore

#endif // ENABLE(LEGACY_ENCRYPTED_MEDIA)
