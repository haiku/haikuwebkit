/*
 * Copyright (C) 2010-2025 Apple Inc. All rights reserved.
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
#include "NetworkProcessConnection.h"

#include "LibWebRTCNetwork.h"
#include "Logging.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "RTCDataChannelRemoteManager.h"
#include "StorageAreaMap.h"
#include "StorageAreaMapMessages.h"
#include "WebBroadcastChannelRegistry.h"
#include "WebBroadcastChannelRegistryMessages.h"
#include "WebCacheStorageProvider.h"
#include "WebCookieJar.h"
#include "WebFileSystemStorageConnection.h"
#include "WebFileSystemStorageConnectionMessages.h"
#include "WebFrame.h"
#include "WebIDBConnectionToServer.h"
#include "WebIDBConnectionToServerMessages.h"
#include "WebLoaderStrategy.h"
#include "WebPage.h"
#include "WebPageMessages.h"
#include "WebPaymentCoordinator.h"
#include "WebProcess.h"
#include "WebRTCMonitor.h"
#include "WebRTCMonitorMessages.h"
#include "WebRTCResolverMessages.h"
#include "WebResourceLoaderMessages.h"
#include "WebSWClientConnection.h"
#include "WebSWClientConnectionMessages.h"
#include "WebSWContextManagerConnection.h"
#include "WebSWContextManagerConnectionMessages.h"
#include "WebServiceWorkerProvider.h"
#include "WebSharedWorkerContextManagerConnection.h"
#include "WebSharedWorkerContextManagerConnectionMessages.h"
#include "WebSharedWorkerObjectConnection.h"
#include "WebSharedWorkerObjectConnectionMessages.h"
#include "WebSocketChannel.h"
#include "WebSocketChannelMessages.h"
#include "WebTransportSession.h"
#include "WebTransportSessionMessages.h"
#include <WebCore/BackgroundFetchRecordInformation.h>
#include <WebCore/CachedResource.h>
#include <WebCore/HTTPCookieAcceptPolicy.h>
#include <WebCore/InspectorInstrumentationWebKit.h>
#include <WebCore/MemoryCache.h>
#include <WebCore/MessagePort.h>
#include <WebCore/NavigationScheduler.h>
#include <WebCore/Page.h>
#include <WebCore/SharedBuffer.h>
#include <pal/SessionID.h>

#if ENABLE(APPLE_PAY_REMOTE_UI)
#include "WebPaymentCoordinatorMessages.h"
#endif

namespace WebKit {
using namespace WebCore;

NetworkProcessConnection::NetworkProcessConnection(IPC::Connection::Identifier&& connectionIdentifier, HTTPCookieAcceptPolicy cookieAcceptPolicy)
    : m_connection(IPC::Connection::createClientConnection(WTFMove(connectionIdentifier)))
    , m_cookieAcceptPolicy(cookieAcceptPolicy)
{
    m_connection->open(*this);

    if (WebRTCProvider::webRTCAvailable())
        WebProcess::singleton().protectedLibWebRTCNetwork()->setConnection(m_connection.copyRef());
}

NetworkProcessConnection::~NetworkProcessConnection()
{
    m_connection->invalidate();
}

bool NetworkProcessConnection::dispatchMessage(IPC::Connection& connection, IPC::Decoder& decoder)
{
    if (decoder.messageReceiverName() == Messages::WebResourceLoader::messageReceiverName()) {
        if (RefPtr webResourceLoader = WebProcess::singleton().protectedWebLoaderStrategy()->webResourceLoaderForIdentifier(AtomicObjectIdentifier<WebCore::ResourceLoaderIdentifierType>(decoder.destinationID())))
            webResourceLoader->didReceiveMessage(connection, decoder);
        return true;
    }
    if (decoder.messageReceiverName() == Messages::WebBroadcastChannelRegistry::messageReceiverName()) {
        WebProcess::singleton().broadcastChannelRegistry().didReceiveMessage(connection, decoder);
        return true;
    }
    if (decoder.messageReceiverName() == Messages::WebSocketChannel::messageReceiverName()) {
        WebProcess::singleton().webSocketChannelManager().didReceiveMessage(connection, decoder);
        return true;
    }
    if (decoder.messageReceiverName() == Messages::WebPage::messageReceiverName()) {
        if (RefPtr webPage = WebProcess::singleton().webPage(ObjectIdentifier<PageIdentifierType>(decoder.destinationID())))
            webPage->didReceiveMessage(connection, decoder);
        return true;
    }
    if (decoder.messageReceiverName() == Messages::StorageAreaMap::messageReceiverName()) {
        if (auto storageAreaMap = WebProcess::singleton().storageAreaMap(ObjectIdentifier<StorageAreaMapIdentifierType>(decoder.destinationID())))
            storageAreaMap->didReceiveMessage(connection, decoder);
        return true;
    }
    if (decoder.messageReceiverName() == Messages::WebFileSystemStorageConnection::messageReceiverName()) {
        WebProcess::singleton().protectedFileSystemStorageConnection()->didReceiveMessage(connection, decoder);
        return true;
    }
    if (decoder.messageReceiverName() == Messages::WebTransportSession::messageReceiverName() && WebProcess::singleton().isWebTransportEnabled()) {
        if (RefPtr webTransportSession = WebProcess::singleton().webTransportSession(WebTransportSessionIdentifier(decoder.destinationID())))
            webTransportSession->didReceiveMessage(connection, decoder);
        return true;
    }

#if USE(LIBWEBRTC)
    if (decoder.messageReceiverName() == Messages::WebRTCMonitor::messageReceiverName()) {
        Ref network = WebProcess::singleton().libWebRTCNetwork();
        if (network->isActive())
            network->protectedMonitor()->didReceiveMessage(connection, decoder);
        else
            RELEASE_LOG_ERROR(WebRTC, "Received WebRTCMonitor message while libWebRTCNetwork is not active");
        return true;
    }
    if (decoder.messageReceiverName() == Messages::WebRTCResolver::messageReceiverName()) {
        Ref network = WebProcess::singleton().libWebRTCNetwork();
        if (network->isActive())
            network->resolver(AtomicObjectIdentifier<LibWebRTCResolverIdentifierType>(decoder.destinationID()))->didReceiveMessage(connection, decoder);
        else
            RELEASE_LOG_ERROR(WebRTC, "Received WebRTCResolver message while libWebRTCNetwork is not active");
        return true;
    }
#endif

    if (decoder.messageReceiverName() == Messages::WebIDBConnectionToServer::messageReceiverName()) {
        if (RefPtr webIDBConnection = m_webIDBConnection)
            webIDBConnection->didReceiveMessage(connection, decoder);
        return true;
    }

    if (decoder.messageReceiverName() == Messages::WebSWClientConnection::messageReceiverName()) {
        protectedServiceWorkerConnection()->didReceiveMessage(connection, decoder);
        return true;
    }
    if (decoder.messageReceiverName() == Messages::WebSWContextManagerConnection::messageReceiverName()) {
        ASSERT(SWContextManager::singleton().connection());
        if (RefPtr contextManagerConnection = SWContextManager::singleton().connection())
            downcast<WebSWContextManagerConnection>(*contextManagerConnection).didReceiveMessage(connection, decoder);
        return true;
    }
    if (decoder.messageReceiverName() == Messages::WebSharedWorkerObjectConnection::messageReceiverName()) {
        protectedSharedWorkerConnection()->didReceiveMessage(connection, decoder);
        return true;
    }
    if (decoder.messageReceiverName() == Messages::WebSharedWorkerContextManagerConnection::messageReceiverName()) {
        ASSERT(SharedWorkerContextManager::singleton().connection());
        if (RefPtr contextManagerConnection = SharedWorkerContextManager::singleton().connection())
            downcast<WebSharedWorkerContextManagerConnection>(*contextManagerConnection).didReceiveMessage(connection, decoder);
        return true;
    }

#if ENABLE(APPLE_PAY_REMOTE_UI)
    if (decoder.messageReceiverName() == Messages::WebPaymentCoordinator::messageReceiverName()) {
        if (auto webPage = WebProcess::singleton().webPage(ObjectIdentifier<PageIdentifierType>(decoder.destinationID())))
            webPage->paymentCoordinator()->didReceiveMessage(connection, decoder);
        return true;
    }
#endif
    return false;
}

bool NetworkProcessConnection::dispatchSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& replyEncoder)
{
#if ENABLE(APPLE_PAY_REMOTE_UI)
    if (decoder.messageReceiverName() == Messages::WebPaymentCoordinator::messageReceiverName()) {
        if (auto webPage = WebProcess::singleton().webPage(ObjectIdentifier<PageIdentifierType>(decoder.destinationID())))
            return webPage->paymentCoordinator()->didReceiveSyncMessage(connection, decoder, replyEncoder);
        return false;
    }
#endif
    return false;
}

void NetworkProcessConnection::didClose(IPC::Connection&)
{
    // The NetworkProcess probably crashed.
    Ref<NetworkProcessConnection> protector(*this);
    WebProcess::singleton().networkProcessConnectionClosed(this);

    if (auto idbConnection = std::exchange(m_webIDBConnection, nullptr))
        idbConnection->connectionToServerLost();

    if (auto swConnection = std::exchange(m_swConnection, nullptr))
        swConnection->connectionToServerLost();
}

void NetworkProcessConnection::didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName, int32_t)
{
}

void NetworkProcessConnection::writeBlobsToTemporaryFilesForIndexedDB(const Vector<String>& blobURLs, CompletionHandler<void(Vector<String>&& filePaths)>&& completionHandler)
{
    m_connection->sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::WriteBlobsToTemporaryFilesForIndexedDB(blobURLs), WTFMove(completionHandler));
}

void NetworkProcessConnection::didFinishPingLoad(WebCore::ResourceLoaderIdentifier pingLoadIdentifier, ResourceError&& error, ResourceResponse&& response)
{
    WebProcess::singleton().protectedWebLoaderStrategy()->didFinishPingLoad(pingLoadIdentifier, WTFMove(error), WTFMove(response));
}

void NetworkProcessConnection::didFinishPreconnection(WebCore::ResourceLoaderIdentifier preconnectionIdentifier, ResourceError&& error)
{
    WebProcess::singleton().protectedWebLoaderStrategy()->didFinishPreconnection(preconnectionIdentifier, WTFMove(error));
}

void NetworkProcessConnection::setOnLineState(bool isOnLine)
{
    WebProcess::singleton().protectedWebLoaderStrategy()->setOnLineState(isOnLine);
}

bool NetworkProcessConnection::cookiesEnabled() const
{
    return m_cookieAcceptPolicy != HTTPCookieAcceptPolicy::Never;
}

void NetworkProcessConnection::cookieAcceptPolicyChanged(HTTPCookieAcceptPolicy newPolicy)
{
    if (m_cookieAcceptPolicy == newPolicy)
        return;

    m_cookieAcceptPolicy = newPolicy;
    WebProcess::singleton().cookieJar().clearCache();
}

#if HAVE(COOKIE_CHANGE_LISTENER_API)
void NetworkProcessConnection::cookiesAdded(const String& host, Vector<WebCore::Cookie>&& cookies)
{
    WebProcess::singleton().cookieJar().cookiesAdded(host, WTFMove(cookies));
}

void NetworkProcessConnection::cookiesDeleted(const String& host, Vector<WebCore::Cookie>&& cookies)
{
    WebProcess::singleton().cookieJar().cookiesDeleted(host, WTFMove(cookies));
}

void NetworkProcessConnection::allCookiesDeleted()
{
    WebProcess::singleton().cookieJar().allCookiesDeleted();
}
#endif

void NetworkProcessConnection::updateCachedCookiesEnabled()
{
    WebProcess::singleton().updateCachedCookiesEnabled();
}

#if ENABLE(SHAREABLE_RESOURCE)
void NetworkProcessConnection::didCacheResource(const ResourceRequest& request, ShareableResource::Handle&& handle)
{
    auto* resource = MemoryCache::singleton().resourceForRequest(request, WebProcess::singleton().sessionID());
    if (!resource)
        return;
    
    auto buffer = WTFMove(handle).tryWrapInSharedBuffer();
    if (!buffer) {
        LOG_ERROR("Unable to create FragmentedSharedBuffer from ShareableResource handle for resource url %s", request.url().string().utf8().data());
        return;
    }

    resource->tryReplaceEncodedData(*buffer);
}
#endif

WebIDBConnectionToServer& NetworkProcessConnection::idbConnectionToServer()
{
    if (!m_webIDBConnection)
        m_webIDBConnection = WebIDBConnectionToServer::create(WebProcess::singleton().sessionID());

    return *m_webIDBConnection;
}

WebSWClientConnection& NetworkProcessConnection::serviceWorkerConnection()
{
    if (!m_swConnection)
        m_swConnection = WebSWClientConnection::create();
    return *m_swConnection;
}

Ref<WebSWClientConnection> NetworkProcessConnection::protectedServiceWorkerConnection()
{
    return serviceWorkerConnection();
}

WebSharedWorkerObjectConnection& NetworkProcessConnection::sharedWorkerConnection()
{
    if (!m_sharedWorkerConnection)
        m_sharedWorkerConnection = WebSharedWorkerObjectConnection::create();
    return *m_sharedWorkerConnection;
}

Ref<WebSharedWorkerObjectConnection> NetworkProcessConnection::protectedSharedWorkerConnection()
{
    return sharedWorkerConnection();
}

void NetworkProcessConnection::messagesAvailableForPort(const WebCore::MessagePortIdentifier& messagePortIdentifier)
{
    WebProcess::singleton().messagesAvailableForPort(messagePortIdentifier);
}

void NetworkProcessConnection::broadcastConsoleMessage(MessageSource source, MessageLevel level, const String& message)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());

    Page::forEachPage([&] (auto& page) {
        if (RefPtr localTopDocument = page.localTopDocument())
            localTopDocument->addConsoleMessage(source, level, message);
    });
}

void NetworkProcessConnection::loadCancelledDownloadRedirectRequestInFrame(WebCore::ResourceRequest&& request, WebCore::FrameIdentifier frameID, WebCore::PageIdentifier pageID)
{
    if (RefPtr webPage = WebProcess::singleton().webPage(pageID); webPage && WebProcess::singleton().webFrame(frameID)) {
        LoadParameters loadParameters;
        loadParameters.frameIdentifier = frameID;
        loadParameters.request = request;
        webPage->loadRequest(WTFMove(loadParameters));
    } else
        RELEASE_LOG_ERROR(Process, "Trying to load Invalid page or frame for %s", request.url().string().utf8().data());
}

#if ENABLE(WEB_RTC)
void NetworkProcessConnection::connectToRTCDataChannelRemoteSource(WebCore::RTCDataChannelIdentifier localIdentifier, WebCore::RTCDataChannelIdentifier remoteIdentifier, CompletionHandler<void(std::optional<bool>)>&& callback)
{
    callback(RTCDataChannelRemoteManager::singleton().connectToRemoteSource(localIdentifier, remoteIdentifier));
}
#endif

} // namespace WebKit
