/*
 * Copyright (C) 2009, 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebSocketProvider.h"

#include "NetworkProcessConnection.h"
#include "WebProcess.h"
#include "WebSocketChannelManager.h"
#include "WebTransportSession.h"
#include <WebCore/DocumentInlines.h>
#include <WebCore/WebTransportSessionClient.h>
#include <WebCore/WorkerGlobalScope.h>

namespace WebKit {
using namespace WebCore;

RefPtr<ThreadableWebSocketChannel> WebSocketProvider::createWebSocketChannel(Document& document, WebSocketChannelClient& client)
{
    return WebKit::WebSocketChannel::create(m_webPageProxyID, document, client);
}

Ref<WebCore::WebTransportSessionPromise> WebSocketProvider::initializeWebTransportSession(ScriptExecutionContext& context, WebTransportSessionClient& client, const URL& url)
{
    if (RefPtr scope = dynamicDowncast<WorkerGlobalScope>(context)) {
        ASSERT(!RunLoop::isMain());
        WebCore::WebTransportSessionPromise::Producer producer;
        Ref<WebCore::WebTransportSessionPromise> promise = producer.promise();

        RunLoop::protectedMain()->dispatch([
            contextID = context.identifier(),
            producer = WTFMove(producer),
            webPageProxyID = m_webPageProxyID,
            origin = crossThreadCopy(scope->clientOrigin()),
            client = ThreadSafeWeakPtr { client },
            url = crossThreadCopy(url)
        ] mutable {
            WebKit::WebTransportSession::initialize(WebProcess::singleton().ensureNetworkProcessConnection().connection(), WTFMove(client), url, webPageProxyID, origin)->whenSettled(RunLoop::protectedMain(), [producer = WTFMove(producer)] (auto&& result) mutable {
                if (!result)
                    producer.reject();
                else
                    producer.resolve(WTFMove(*result));
            });
        });
        return promise;
    }

    Ref document = downcast<Document>(context);
    ASSERT(RunLoop::isMain());
    return WebKit::WebTransportSession::initialize(WebProcess::singleton().ensureNetworkProcessConnection().connection(), client, url, m_webPageProxyID, document->clientOrigin());
}

} // namespace WebKit
