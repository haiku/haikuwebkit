/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(WEB_AUTHN)

#include "APIObject.h"
#include <WebCore/AuthenticatorAssertionResponse.h>

namespace API {

class Data;

class WebAuthenticationAssertionResponse final : public ObjectImpl<Object::Type::WebAuthenticationAssertionResponse> {
public:
    static Ref<WebAuthenticationAssertionResponse> create(Ref<WebCore::AuthenticatorAssertionResponse>&&);
    ~WebAuthenticationAssertionResponse();

    const WTF::String& name() const { return m_response->name(); }
    const WTF::String& displayName() const { return m_response->displayName(); }
    RefPtr<Data> userHandle() const;
    bool synchronizable() const { return m_response->synchronizable(); }
    const WTF::String& group() const { return m_response->group(); }
    RefPtr<Data> credentialID() const;
    const WTF::String& accessGroup() const { return m_response->accessGroup(); }

    void setLAContext(LAContext *context) { m_response->setLAContext(context); }

    WebCore::AuthenticatorAssertionResponse& response() { return m_response.get(); }
    Ref<WebCore::AuthenticatorAssertionResponse> protectedResponse() { return m_response; }

private:
    WebAuthenticationAssertionResponse(Ref<WebCore::AuthenticatorAssertionResponse>&&);

    Ref<WebCore::AuthenticatorAssertionResponse> m_response;
};

} // namespace API

SPECIALIZE_TYPE_TRAITS_API_OBJECT(WebAuthenticationAssertionResponse);

#endif // ENABLE(WEB_AUTHN)
