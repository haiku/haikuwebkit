/*
 * Copyright (C) 2006-2025 Apple Inc. All rights reserved.
 *           (C) 2007 Graham Dennis (graham.dennis@gmail.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ResourceLoader.h"

#include "ApplicationCacheHost.h"
#include "AuthenticationChallenge.h"
#include "ContentRuleListResults.h"
#include "DNS.h"
#include "DataURLDecoder.h"
#include "DiagnosticLoggingClient.h"
#include "DiagnosticLoggingKeys.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "DocumentLoader.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameLoader.h"
#include "HTMLFrameOwnerElement.h"
#include "InspectorInstrumentation.h"
#include "LoaderStrategy.h"
#include "LocalFrame.h"
#include "LocalFrameLoaderClient.h"
#include "Logging.h"
#include "NetworkingContext.h"
#include "OriginAccessPatterns.h"
#include "Page.h"
#include "PageConsoleClient.h"
#include "PlatformStrategies.h"
#include "ProgressTracker.h"
#include "Quirks.h"
#include "ResourceError.h"
#include "ResourceHandle.h"
#include "ResourceMonitor.h"
#include "SecurityOrigin.h"
#include "SubresourceLoader.h"
#include <wtf/CompletionHandler.h>
#include <wtf/Ref.h>
#include <wtf/text/MakeString.h>

#if ENABLE(CONTENT_EXTENSIONS)
#include "UserContentController.h"
#endif

#if USE(QUICK_LOOK)
#include "LegacyPreviewLoader.h"
#include "PreviewConverter.h"
#endif

#if PLATFORM(COCOA)
#include "BundleResourceLoader.h"
#endif

#undef RESOURCELOADER_RELEASE_LOG
#define PAGE_ID (this->frame() && this->frame()->pageID() ? this->frame()->pageID()->toUInt64() : 0)
#define FRAME_ID (this->frame() ? this->frame()->frameID().toUInt64() : 0)
#define RESOURCELOADER_RELEASE_LOG(fmt, ...) RELEASE_LOG(Network, "%p - [pageID=%" PRIu64 ", frameID=%" PRIu64 ", frameLoader=%p, resourceID=%" PRIu64 "] ResourceLoader::" fmt, this, PAGE_ID, FRAME_ID, this->frameLoader(), identifier() ? identifier()->toUInt64() : 0, ##__VA_ARGS__)
#define RESOURCELOADER_RELEASE_LOG_FORWARDABLE(fmt, ...) RELEASE_LOG_FORWARDABLE(Network, fmt, PAGE_ID, FRAME_ID, identifier() ? identifier()->toUInt64() : 0, ##__VA_ARGS__)

namespace WebCore {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(ResourceLoader);

ResourceLoader::ResourceLoader(LocalFrame& frame, ResourceLoaderOptions options)
    : m_frame { &frame }
    , m_documentLoader { frame.loader().activeDocumentLoader() }
    , m_defersLoading { options.defersLoadingPolicy == DefersLoadingPolicy::AllowDefersLoading && frame.page() && frame.page()->defersLoading() }
    , m_options { options }
{
}

ResourceLoader::~ResourceLoader()
{
    ASSERT(m_reachedTerminalState);
}

void ResourceLoader::finishNetworkLoad()
{
    platformStrategies()->loaderStrategy()->remove(this);

    if (RefPtr handle = m_handle) {
        ASSERT(handle->client() == this);
        handle->clearClient();
        m_handle = nullptr;
    }
}

void ResourceLoader::releaseResources()
{
    ASSERT(!m_reachedTerminalState);
    
    // It's possible that when we release the handle, it will be
    // deallocated and release the last reference to this object.
    // We need to retain to avoid accessing the object after it
    // has been deallocated and also to avoid reentering this method.
    Ref protectedThis { *this };

    m_frame = nullptr;
    m_documentLoader = nullptr;
    
    // We need to set reachedTerminalState to true before we release
    // the resources to prevent a double dealloc of WebView <rdar://problem/4372628>
    m_reachedTerminalState = true;

    finishNetworkLoad();

    m_identifier = std::nullopt;

    m_resourceData.reset();
    m_deferredRequest = ResourceRequest();
}

void ResourceLoader::init(ResourceRequest&& clientRequest, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!m_documentLoader || !m_documentLoader->frame()) {
        if (!m_documentLoader)
            RESOURCELOADER_RELEASE_LOG("init: Cancelling because there is no document loader.");
        else
            RESOURCELOADER_RELEASE_LOG("init: Cancelling because the document loader has no frame.");
        cancel();
        return completionHandler(false);
    }

    ASSERT(!m_handle);
    ASSERT(m_request.isNull());
    ASSERT(m_deferredRequest.isNull());
    ASSERT(!m_documentLoader->isSubstituteLoadPending(this));
    
    m_loadTiming.markStartTime();

    RefPtr frame = m_frame.get();
    if (!frame)
        return completionHandler(false);
    m_defersLoading = m_options.defersLoadingPolicy == DefersLoadingPolicy::AllowDefersLoading && frame->page()->defersLoading();

    if (m_options.securityCheck == SecurityCheckPolicy::DoSecurityCheck && !frame->document()->protectedSecurityOrigin()->canDisplay(clientRequest.url(), OriginAccessPatternsForWebProcess::singleton())) {
        RESOURCELOADER_RELEASE_LOG("init: Cancelling load because it violates security policy.");
        FrameLoader::reportLocalLoadFailed(frame.get(), clientRequest.url().string());
        releaseResources();
        return completionHandler(false);
    }

    if (!portAllowed(clientRequest.url())) {
        RESOURCELOADER_RELEASE_LOG("init: Cancelling load to a blocked port.");
        FrameLoader::reportBlockedLoadFailed(*frame, clientRequest.url());
        releaseResources();
        return completionHandler(false);
    }

    if (isIPAddressDisallowed(clientRequest.url())) {
        RESOURCELOADER_RELEASE_LOG("init: Cancelling load to disallowed IP address.");
        FrameLoader::reportBlockedLoadFailed(*frame, clientRequest.url());
        releaseResources();
        return completionHandler(false);
    }

    // The various plug-in implementations call directly to ResourceLoader::load() instead of piping requests
    // through FrameLoader. As a result, they miss the FrameLoader::updateRequestAndAddExtraFields() step which sets
    // up the 1st party for cookies URL and Same-Site info. Until plug-in implementations can be reigned in
    // to pipe through that method, we need to make sure there is always both a 1st party for cookies set and
    // Same-Site info. See <https://bugs.webkit.org/show_bug.cgi?id=26391>.
    if (clientRequest.firstPartyForCookies().isNull()) {
        if (RefPtr document = frame->document())
            clientRequest.setFirstPartyForCookies(document->firstPartyForCookies());
    }
    FrameLoader::addSameSiteInfoToRequestIfNeeded(clientRequest, frame->protectedDocument().get());

    willSendRequestInternal(WTFMove(clientRequest), ResourceResponse(), [this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)](ResourceRequest&& request) mutable {

#if PLATFORM(IOS_FAMILY)
        // If this ResourceLoader was stopped as a result of willSendRequest, bail out.
        if (m_reachedTerminalState) {
            RESOURCELOADER_RELEASE_LOG("init: Cancelling load because it was stopped as a result of willSendRequest.");
            return completionHandler(false);
        }
#endif

        if (request.isNull()) {
            RESOURCELOADER_RELEASE_LOG("init: Cancelling load because the request is null.");
            cancel();
            return completionHandler(false);
        }

        m_request = WTFMove(request);
        m_originalRequest = m_request;
        completionHandler(true);
    });
}

void ResourceLoader::deliverResponseAndData(ResourceResponse&& response, RefPtr<FragmentedSharedBuffer>&& buffer)
{
    didReceiveResponse(WTFMove(response), [this, protectedThis = Ref { *this }, buffer = WTFMove(buffer)]() mutable {
        if (reachedTerminalState())
            return;

        if (buffer) {
            unsigned size = buffer->size();
            didReceiveBuffer(buffer.releaseNonNull(), size, DataPayloadWholeResource);
            if (reachedTerminalState())
                return;
        }

        NetworkLoadMetrics emptyMetrics;
        didFinishLoading(emptyMetrics);
    });
}

void ResourceLoader::start()
{
    ASSERT(!m_handle);
    ASSERT(!m_request.isNull());
    ASSERT(m_deferredRequest.isNull());
    ASSERT(frameLoader());

#if ENABLE(WEB_ARCHIVE) || ENABLE(MHTML)
    if (RefPtr documentLoader = m_documentLoader) {
        if (documentLoader->scheduleArchiveLoad(*this, m_request))
            return;
    }
#endif

    if (RefPtr documentLoader = m_documentLoader) {
        if (documentLoader->applicationCacheHost().maybeLoadResource(*this, m_request, m_request.url()))
            return;
    }

    if (m_defersLoading) {
        m_deferredRequest = m_request;
        return;
    }

    if (m_reachedTerminalState)
        return;

    if (m_request.url().protocolIsData()) {
        loadDataURL();
        return;
    }

#if PLATFORM(COCOA)
    if (isPDFJSResourceLoad()) {
        BundleResourceLoader::loadResourceFromBundle(*this, "pdfjs/"_s);
        return;
    }
#endif

#if USE(SOUP)
    if (m_request.url().protocolIs("resource"_s) || isPDFJSResourceLoad()) {
        loadGResource();
        return;
    }
#endif

    RefPtr subresourceLoader = dynamicDowncast<SubresourceLoader>(*this);
    RefPtr sourceOrigin = subresourceLoader ? subresourceLoader->origin() : nullptr;
    RefPtr frameLoader = this->frameLoader();
    if (!frameLoader)
        return;

    if (!sourceOrigin) {
        RefPtr document = frameLoader->frame().document();
        sourceOrigin =  document ? &document->securityOrigin() : nullptr;
    }

    bool isMainFrameNavigation = frame() && frame()->isMainFrame() && options().mode == FetchOptions::Mode::Navigate;

    m_handle = ResourceHandle::create(frameLoader->protectedNetworkingContext().get(), m_request, this, m_defersLoading, m_options.sniffContent == ContentSniffingPolicy::SniffContent, m_options.contentEncodingSniffingPolicy, WTFMove(sourceOrigin), isMainFrameNavigation);
}

void ResourceLoader::setDefersLoading(bool defers)
{
    if (m_options.defersLoadingPolicy == DefersLoadingPolicy::DisallowDefersLoading)
        return;

    m_defersLoading = defers;
    if (RefPtr handle = m_handle)
        handle->setDefersLoading(defers);

    platformStrategies()->loaderStrategy()->setDefersLoading(*this, defers);
}

FrameLoader* ResourceLoader::frameLoader() const
{
    RefPtr frame = m_frame.get();
    if (!frame)
        return nullptr;
    return &frame->loader();
}

RefPtr<DocumentLoader> ResourceLoader::protectedDocumentLoader() const
{
    return m_documentLoader;
}

void ResourceLoader::loadDataURL()
{
    auto url = m_request.url();
    ASSERT(url.protocolIsData());

    auto shouldValidatePadding = DataURLDecoder::ShouldValidatePadding::Yes;
    RefPtr frame = m_frame.get();
    if (RefPtr document = frame ? frame->document() : nullptr) {
        if (document->quirks().shouldDisableDataURLPaddingValidation())
            shouldValidatePadding = DataURLDecoder::ShouldValidatePadding::No;
    }

    DataURLDecoder::ScheduleContext scheduleContext;
#if USE(COCOA_EVENT_LOOP)
    if (RefPtr page = frame ? frame->page() : nullptr)
        scheduleContext.scheduledPairs = *page->scheduledRunLoopPairs();
#endif
    DataURLDecoder::decode(url, scheduleContext, shouldValidatePadding, [this, protectedThis = Ref { *this }, url](auto decodeResult) mutable {
        if (this->reachedTerminalState())
            return;
        if (!decodeResult) {
            RESOURCELOADER_RELEASE_LOG("loadDataURL: decoding of data failed");
            protectedThis->didFail(ResourceError(errorDomainWebKitInternal, 0, url, "Data URL decoding failed"_s));
            return;
        }
        if (this->wasCancelled()) {
            RESOURCELOADER_RELEASE_LOG("loadDataURL: Load was cancelled");
            return;
        }

        auto dataSize = decodeResult->data.size();
        ResourceResponse dataResponse = ResourceResponse::dataURLResponse(url, decodeResult.value());
        this->didReceiveResponse(WTFMove(dataResponse), [this, protectedThis = WTFMove(protectedThis), dataSize, data = SharedBuffer::create(WTFMove(decodeResult->data))]() {
            if (!this->reachedTerminalState() && dataSize && m_request.httpMethod() != "HEAD"_s)
                this->didReceiveBuffer(data, dataSize, DataPayloadWholeResource);

            if (!this->reachedTerminalState()) {
                NetworkLoadMetrics emptyMetrics;
                this->didFinishLoading(emptyMetrics);
            }
        });
    });
}

void ResourceLoader::setDataBufferingPolicy(DataBufferingPolicy dataBufferingPolicy)
{
    m_options.dataBufferingPolicy = dataBufferingPolicy;

    // Reset any already buffered data
    if (dataBufferingPolicy == DataBufferingPolicy::DoNotBufferData)
        m_resourceData.reset();
}

void ResourceLoader::willSwitchToSubstituteResource()
{
    ASSERT(m_documentLoader && !m_documentLoader->isSubstituteLoadPending(this));
    platformStrategies()->loaderStrategy()->remove(this);
    if (RefPtr handle = m_handle)
        handle->cancel();
}

void ResourceLoader::addBuffer(const FragmentedSharedBuffer& buffer, DataPayloadType dataPayloadType)
{
    if (m_options.dataBufferingPolicy == DataBufferingPolicy::DoNotBufferData)
        return;

    if (dataPayloadType == DataPayloadWholeResource)
        m_resourceData.reset();

    m_resourceData.append(buffer);
}

const FragmentedSharedBuffer* ResourceLoader::resourceData() const
{
    return m_resourceData.get().get();
}

RefPtr<const FragmentedSharedBuffer> ResourceLoader::protectedResourceData() const
{
    return resourceData();
}

void ResourceLoader::clearResourceData()
{
    if (m_resourceData)
        m_resourceData.empty();
}

bool ResourceLoader::isSubresourceLoader() const
{
    return false;
}

RefPtr<FrameLoader> ResourceLoader::protectedFrameLoader() const
{
    return frameLoader();
}

void ResourceLoader::willSendRequestInternal(ResourceRequest&& request, const ResourceResponse& redirectResponse, CompletionHandler<void(ResourceRequest&&)>&& completionHandler)
{
    // Protect this in this delegate method since the additional processing can do
    // anything including possibly derefing this; one example of this is Radar 3266216.
    Ref protectedThis { *this };

    ASSERT(!m_reachedTerminalState);
#if ENABLE(CONTENT_EXTENSIONS)
    ASSERT(!m_resourceType.isEmpty());
#endif

    // We need a resource identifier for all requests, even if FrameLoader is never going to see it (such as with CORS preflight requests).
    bool createdResourceIdentifier = false;
    if (!m_identifier) {
        m_identifier = ResourceLoaderIdentifier::generate();
        createdResourceIdentifier = true;
    }

    RefPtr frameLoader = this->frameLoader();
#if ENABLE(CONTENT_EXTENSIONS)
    if (!redirectResponse.isNull() && frameLoader) {
        RefPtr page = frameLoader->frame().page();
        RefPtr documentLoader = m_documentLoader;
        if (page && documentLoader) {
            auto results = page->protectedUserContentProvider()->processContentRuleListsForLoad(*page, request.url(), m_resourceType, *documentLoader, redirectResponse.url());
            ContentExtensions::applyResultsToRequest(WTFMove(results), page.get(), request);
            if (results.shouldBlock()) {
                RESOURCELOADER_RELEASE_LOG("willSendRequestInternal: resource load canceled because of content blocker");
                didFail(blockedByContentBlockerError());
                completionHandler({ });
                return;
            }
        }
    }
#endif

    if (request.isNull()) {
        RESOURCELOADER_RELEASE_LOG("willSendRequestInternal: resource load canceled because of empty request");
        didFail(cannotShowURLError());
        completionHandler({ });
        return;
    }

    if (frameLoader && frameLoader->frame().isMainFrame() && cachedResource() && cachedResource()->type() == CachedResource::Type::MainResource && !redirectResponse.isNull()) {
        if (request.wasSchemeOptimisticallyUpgraded() && request.url() == redirectResponse.url()) {
            RESOURCELOADER_RELEASE_LOG("willSendRequestInternal: resource load canceled because of entering same-URL redirect loop");
            cancel(httpsUpgradeRedirectLoopError());
            completionHandler({ });
            return;
        }
    }

    if (m_options.sendLoadCallbacks == SendCallbackPolicy::SendCallbacks) {
        if (createdResourceIdentifier && frameLoader)
            frameLoader->notifier().assignIdentifierToInitialRequest(*m_identifier, options().mode == FetchOptions::Mode::Navigate ? IsMainResourceLoad::Yes : IsMainResourceLoad::No, protectedDocumentLoader().get(), request);

#if PLATFORM(IOS_FAMILY)
        // If this ResourceLoader was stopped as a result of assignIdentifierToInitialRequest, bail out
        if (m_reachedTerminalState) {
            RESOURCELOADER_RELEASE_LOG("willSendRequestInternal: resource load reached terminal state after calling assignIdentifierToInitialRequest()");
            completionHandler(WTFMove(request));
            return;
        }
#endif

        if (frameLoader)
            frameLoader->notifier().willSendRequest(*this, *m_identifier, request, redirectResponse);
    } else if (RefPtr frame = m_frame.get())
        InspectorInstrumentation::willSendRequest(frame.get(), *m_identifier, frame->loader().protectedDocumentLoader().get(), request, redirectResponse, protectedCachedResource().get(), this);

#if USE(QUICK_LOOK)
    if (m_documentLoader) {
        if (RefPtr previewConverter = m_documentLoader->previewConverter())
            request = previewConverter->safeRequest(request);
    }
#endif

    bool isRedirect = !redirectResponse.isNull();
    if (isRedirect) {
        RESOURCELOADER_RELEASE_LOG("willSendRequestInternal: Processing cross-origin redirect");
        platformStrategies()->loaderStrategy()->crossOriginRedirectReceived(this, request.url());
        if (frameLoader)
            frameLoader->protectedClient()->didLoadFromRegistrableDomain(RegistrableDomain(request.url()));
    }
    m_request = request;

    if (isRedirect) {
        auto& redirectURL = request.url();
        if (m_documentLoader && !m_documentLoader->isCommitted() && frameLoader)
            frameLoader->protectedClient()->dispatchDidReceiveServerRedirectForProvisionalLoad();

        if (redirectURL.protocolIsData()) {
            // Handle data URL decoding locally.
            RESOURCELOADER_RELEASE_LOG("willSendRequestInternal: Redirected to a data URL. Processing locally");
            finishNetworkLoad();
            loadDataURL();
        }
    }

    RESOURCELOADER_RELEASE_LOG_FORWARDABLE(RESOURCELOADER_WILLSENDREQUESTINTERNAL);
    completionHandler(WTFMove(request));
}

void ResourceLoader::willSendRequest(ResourceRequest&& request, const ResourceResponse& redirectResponse, CompletionHandler<void(ResourceRequest&&)>&& completionHandler)
{
    willSendRequestInternal(WTFMove(request), redirectResponse, WTFMove(completionHandler));
}

void ResourceLoader::didSendData(unsigned long long, unsigned long long)
{
}

static void logResourceResponseSource(LocalFrame* frame, ResourceResponse::Source source)
{
    if (!frame || !frame->page())
        return;

    String sourceKey;
    switch (source) {
    case ResourceResponse::Source::Network:
        sourceKey = DiagnosticLoggingKeys::networkKey();
        break;
    case ResourceResponse::Source::DiskCache:
        sourceKey = DiagnosticLoggingKeys::diskCacheKey();
        break;
    case ResourceResponse::Source::DiskCacheAfterValidation:
        sourceKey = DiagnosticLoggingKeys::diskCacheAfterValidationKey();
        break;
    case ResourceResponse::Source::ServiceWorker:
        sourceKey = DiagnosticLoggingKeys::serviceWorkerKey();
        break;
    case ResourceResponse::Source::MemoryCache:
        sourceKey = DiagnosticLoggingKeys::memoryCacheKey();
        break;
    case ResourceResponse::Source::MemoryCacheAfterValidation:
        sourceKey = DiagnosticLoggingKeys::memoryCacheAfterValidationKey();
        break;
    case ResourceResponse::Source::DOMCache:
    case ResourceResponse::Source::ApplicationCache:
    case ResourceResponse::Source::InspectorOverride:
    case ResourceResponse::Source::Unknown:
        return;
    }

    frame->protectedPage()->diagnosticLoggingClient().logDiagnosticMessage(DiagnosticLoggingKeys::resourceResponseSourceKey(), sourceKey, ShouldSample::Yes);
}

bool ResourceLoader::shouldAllowResourceToAskForCredentials() const
{
    if (m_canCrossOriginRequestsAskUserForCredentials)
        return true;
    RefPtr frame = m_frame.get();
    if (!frame)
        return false;
    RefPtr topFrame = dynamicDowncast<LocalFrame>(frame->tree().top());
    if (!topFrame)
        return false;
    RefPtr topDocument = topFrame->document();
    if (!topDocument)
        return false;
    RefPtr securityOrigin = static_cast<SecurityContext*>(topDocument.get())->securityOrigin();
    if (!securityOrigin)
        return false;
    return securityOrigin->canRequest(m_request.url(), OriginAccessPatternsForWebProcess::singleton());
}

void ResourceLoader::didBlockAuthenticationChallenge()
{
    m_wasAuthenticationChallengeBlocked = true;
    if (m_options.clientCredentialPolicy == ClientCredentialPolicy::CannotAskClientForCredentials)
        return;
    RefPtr frame = m_frame.get();
    if (frame && !shouldAllowResourceToAskForCredentials())
        frame->protectedDocument()->addConsoleMessage(MessageSource::Security, MessageLevel::Error, makeString("Blocked "_s, m_request.url().stringCenterEllipsizedToLength(), " from asking for credentials because it is a cross-origin request."_s));
}

void ResourceLoader::didReceiveResponse(ResourceResponse&& r, CompletionHandler<void()>&& policyCompletionHandler)
{
    ASSERT(!m_reachedTerminalState);
    CompletionHandlerCallingScope completionHandlerCaller(WTFMove(policyCompletionHandler));

    // Protect this in this delegate method since the additional processing can do
    // anything including possibly derefing this; one example of this is Radar 3266216.
    Ref protectedThis { *this };

    RefPtr frame = m_frame.get();
    if (r.usedLegacyTLS() && frame) {
        if (RefPtr document = frame->document()) {
            if (!document->usedLegacyTLS()) {
                if (RefPtr page = document->page()) {
                    RESOURCELOADER_RELEASE_LOG("usedLegacyTLS:");
                    page->console().addMessage(MessageSource::Network, MessageLevel::Warning, makeString("Loaded resource from "_s, r.url().host(), " using TLS 1.0 or 1.1, which are deprecated protocols that will be removed. Please use TLS 1.2 or newer instead."_s), 0, document.get());
                }
                document->setUsedLegacyTLS(true);
            }
        }
    }

    if (r.wasPrivateRelayed() && frame) {
        if (RefPtr document = frame->document()) {
            if (!document->wasPrivateRelayed())
                document->setWasPrivateRelayed(true);
        }
    }

    logResourceResponseSource(frame.get(), r.source());

    m_response = WTFMove(r);

    RefPtr frameLoader = this->frameLoader();
    if (frameLoader && m_options.sendLoadCallbacks == SendCallbackPolicy::SendCallbacks && m_identifier)
        frameLoader->notifier().didReceiveResponse(*this, *m_identifier, m_response);
}

void ResourceLoader::didReceiveData(const SharedBuffer& buffer, long long encodedDataLength, DataPayloadType dataPayloadType)
{
    // The following assertions are not quite valid here, since a subclass
    // might override didReceiveData in a way that invalidates them. This
    // happens with the steps listed in 3266216
    // ASSERT(con == connection);
    // ASSERT(!m_reachedTerminalState);

    didReceiveBuffer(buffer, encodedDataLength, dataPayloadType);
}

void ResourceLoader::didReceiveBuffer(const FragmentedSharedBuffer& buffer, long long encodedDataLength, DataPayloadType dataPayloadType)
{
    // Protect this in this delegate method since the additional processing can do
    // anything including possibly derefing this; one example of this is Radar 3266216.
    Ref protectedThis { *this };

    addBuffer(buffer, dataPayloadType);

    // FIXME: If we get a resource with more than 2B bytes, this code won't do the right thing.
    // However, with today's computers and networking speeds, this won't happen in practice.
    // Could be an issue with a giant local file.
    RefPtr frame = m_frame.get();
    RefPtr frameLoader = this->frameLoader();
    if (m_options.sendLoadCallbacks == SendCallbackPolicy::SendCallbacks && frame && frameLoader && m_identifier)
        frameLoader->notifier().didReceiveData(*this, *m_identifier, buffer.makeContiguous(), static_cast<int>(encodedDataLength));
}

void ResourceLoader::didFinishLoading(const NetworkLoadMetrics& networkLoadMetrics)
{
    RESOURCELOADER_RELEASE_LOG("didFinishLoading:");

    didFinishLoadingOnePart(networkLoadMetrics);

    // If the load has been cancelled by a delegate in response to didFinishLoad(), do not release
    // the resources a second time, they have been released by cancel.
    if (wasCancelled())
        return;
    releaseResources();
}

void ResourceLoader::didFinishLoadingOnePart(const NetworkLoadMetrics& networkLoadMetrics)
{
    // If load has been cancelled after finishing (which could happen with a
    // JavaScript that changes the window location), do nothing.
    if (wasCancelled()) {
        RESOURCELOADER_RELEASE_LOG("didFinishLoadingOnePart: Load was cancelled after finishing.");
        return;
    }
    ASSERT(!m_reachedTerminalState);

    if (m_notifiedLoadComplete)
        return;
    m_notifiedLoadComplete = true;
    RefPtr frameLoader = this->frameLoader();
    if (frameLoader && m_options.sendLoadCallbacks == SendCallbackPolicy::SendCallbacks && m_identifier)
        frameLoader->notifier().didFinishLoad(*this, *m_identifier, networkLoadMetrics);
}

void ResourceLoader::didFail(const ResourceError& error)
{
    RESOURCELOADER_RELEASE_LOG("didFail:");

    if (wasCancelled())
        return;
    ASSERT(!m_reachedTerminalState);

    // Protect this in this delegate method since the additional processing can do
    // anything including possibly derefing this; one example of this is Radar 3266216.
    Ref protectedThis { *this };

    cleanupForError(error);
    releaseResources();
}

void ResourceLoader::cleanupForError(const ResourceError& error)
{
    if (m_notifiedLoadComplete)
        return;
    m_notifiedLoadComplete = true;
    if (m_options.sendLoadCallbacks == SendCallbackPolicy::SendCallbacks && m_identifier) {
        if (RefPtr frameLoader = this->frameLoader())
            frameLoader->notifier().didFailToLoad(*this, *m_identifier, error);
    }
}

void ResourceLoader::cancel()
{
    cancel(ResourceError());
}

void ResourceLoader::cancel(const ResourceError& error, LoadWillContinueInAnotherProcess loadWillContinueInAnotherProcess)
{
    // If the load has already completed - succeeded, failed, or previously cancelled - do nothing.
    if (m_reachedTerminalState)
        return;
       
    ResourceError nonNullError = error.isNull() ? cancelledError() : error;
    
    // willCancel() and didFailToLoad() both call out to clients that might do 
    // something causing the last reference to this object to go away.
    Ref protectedThis { *this };
    
    // If we re-enter cancel() from inside willCancel(), we want to pick up from where we left 
    // off without re-running willCancel()
    if (m_cancellationStatus == NotCancelled) {
        m_cancellationStatus = CalledWillCancel;
        
        willCancel(nonNullError);
    }

    // If we re-enter cancel() from inside didFailToLoad(), we want to pick up from where we 
    // left off without redoing any of this work.
    if (m_cancellationStatus == CalledWillCancel) {
        m_cancellationStatus = Cancelled;

        if (RefPtr handle = m_handle)
            handle->clearAuthentication();

        if (RefPtr documentLoader = m_documentLoader)
            documentLoader->cancelPendingSubstituteLoad(this);

        if (RefPtr handle = m_handle) {
            handle->cancel();
            m_handle = nullptr;
        }
        cleanupForError(nonNullError);
    }

    // If cancel() completed from within the call to willCancel() or didFailToLoad(),
    // we don't want to redo didCancel() or releasesResources().
    if (m_reachedTerminalState)
        return;

    didCancel(loadWillContinueInAnotherProcess);

    if (m_cancellationStatus == FinishedCancel)
        return;
    m_cancellationStatus = FinishedCancel;

    releaseResources();
}

ResourceError ResourceLoader::cancelledError()
{
    auto error = platformStrategies()->loaderStrategy()->cancelledError(m_request);
    error.setType(ResourceError::Type::Cancellation);
    return error;
}

ResourceError ResourceLoader::blockedError()
{
    return platformStrategies()->loaderStrategy()->blockedError(m_request);
}

ResourceError ResourceLoader::blockedByContentBlockerError()
{
    return platformStrategies()->loaderStrategy()->blockedByContentBlockerError(m_request);
}

ResourceError ResourceLoader::cannotShowURLError()
{
    return platformStrategies()->loaderStrategy()->cannotShowURLError(m_request);
}

ResourceError ResourceLoader::httpsUpgradeRedirectLoopError()
{
    return platformStrategies()->loaderStrategy()->httpsUpgradeRedirectLoopError(m_request);
}

void ResourceLoader::willSendRequestAsync(ResourceHandle* handle, ResourceRequest&& request, ResourceResponse&& redirectResponse, CompletionHandler<void(ResourceRequest&&)>&& completionHandler)
{
    RefPtr protectedHandle { handle };
    if (protectedDocumentLoader()->applicationCacheHost().maybeLoadFallbackForRedirect(this, request, redirectResponse)) {
        RESOURCELOADER_RELEASE_LOG("willSendRequestAsync: exiting early because maybeLoadFallbackForRedirect returned false");
        completionHandler(WTFMove(request));
        return;
    }
    willSendRequestInternal(WTFMove(request), redirectResponse, WTFMove(completionHandler));
}

void ResourceLoader::didSendData(ResourceHandle*, unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    didSendData(bytesSent, totalBytesToBeSent);
}

void ResourceLoader::didReceiveResponseAsync(ResourceHandle*, ResourceResponse&& response, CompletionHandler<void()>&& completionHandler)
{
    if (protectedDocumentLoader()->applicationCacheHost().maybeLoadFallbackForResponse(this, response)) {
        completionHandler();
        return;
    }
    didReceiveResponse(WTFMove(response), WTFMove(completionHandler));
}

void ResourceLoader::didReceiveData(ResourceHandle*, const SharedBuffer& buffer, int encodedDataLength)
{
    didReceiveData(buffer, encodedDataLength, DataPayloadBytes);
}

void ResourceLoader::didReceiveBuffer(ResourceHandle*, const FragmentedSharedBuffer& buffer, int encodedDataLength)
{
    didReceiveBuffer(buffer, encodedDataLength, DataPayloadBytes);
}

void ResourceLoader::didFinishLoading(ResourceHandle*, const NetworkLoadMetrics& metrics)
{
    didFinishLoading(metrics);
}

void ResourceLoader::didFail(ResourceHandle*, const ResourceError& error)
{
    if (protectedDocumentLoader()->applicationCacheHost().maybeLoadFallbackForError(this, error))
        return;
    didFail(error);
}

void ResourceLoader::wasBlocked(ResourceHandle*)
{
    RESOURCELOADER_RELEASE_LOG("wasBlocked: resource load canceled because of content blocker");
    didFail(blockedError());
}

void ResourceLoader::cannotShowURL(ResourceHandle*)
{
    RESOURCELOADER_RELEASE_LOG("wasBlocked: resource load canceled because of invalid URL");
    didFail(cannotShowURLError());
}

bool ResourceLoader::shouldUseCredentialStorage()
{
    if (m_options.storedCredentialsPolicy != StoredCredentialsPolicy::Use)
        return false;

    RefPtr frame = m_frame.get();
    if (RefPtr page = frame ? frame->page() : nullptr) {
        if (!page->canUseCredentialStorage())
            return false;
    }

    Ref protectedThis { *this };
    RefPtr frameLoader = this->frameLoader();
    return frameLoader && frameLoader->protectedClient()->shouldUseCredentialStorage(protectedDocumentLoader().get(), *identifier());
}

bool ResourceLoader::isAllowedToAskUserForCredentials() const
{
    if (m_options.clientCredentialPolicy == ClientCredentialPolicy::CannotAskClientForCredentials)
        return false;
    if (!shouldAllowResourceToAskForCredentials())
        return false;
    RefPtr frame = m_frame.get();
    return m_options.credentials == FetchOptions::Credentials::Include || (m_options.credentials == FetchOptions::Credentials::SameOrigin && frame && frame->document()->protectedSecurityOrigin()->canRequest(originalRequest().url(), OriginAccessPatternsForWebProcess::singleton()));
}

bool ResourceLoader::shouldIncludeCertificateInfo() const
{
    if (m_options.certificateInfoPolicy == CertificateInfoPolicy::IncludeCertificateInfo)
        return true;
    if (InspectorInstrumentation::hasFrontends()) [[unlikely]]
        return true;
    return false;
}

void ResourceLoader::didReceiveAuthenticationChallenge(ResourceHandle* handle, const AuthenticationChallenge& challenge)
{
    ASSERT_UNUSED(handle, handle == m_handle);
    ASSERT(m_handle->hasAuthenticationChallenge());

    // Protect this in this delegate method since the additional processing can do
    // anything including possibly derefing this; one example of this is Radar 3266216.
    Ref protectedThis { *this };

    if (m_options.storedCredentialsPolicy == StoredCredentialsPolicy::Use) {
        if (isAllowedToAskUserForCredentials() && m_identifier) {
            if (RefPtr frameLoader = this->frameLoader())
                frameLoader->notifier().didReceiveAuthenticationChallenge(*m_identifier, documentLoader(), challenge);
            return;
        }
        didBlockAuthenticationChallenge();
    }
    challenge.authenticationClient()->receivedRequestToContinueWithoutCredential(challenge);
    ASSERT(!m_handle || !m_handle->hasAuthenticationChallenge());
}

#if USE(PROTECTION_SPACE_AUTH_CALLBACK)
void ResourceLoader::canAuthenticateAgainstProtectionSpaceAsync(ResourceHandle*, const ProtectionSpace& protectionSpace, CompletionHandler<void(bool)>&& completionHandler)
{
    completionHandler(canAuthenticateAgainstProtectionSpace(protectionSpace));
}

bool ResourceLoader::canAuthenticateAgainstProtectionSpace(const ProtectionSpace& protectionSpace)
{
    Ref protectedThis { *this };
    RefPtr frameLoader = this->frameLoader();
    return frameLoader && frameLoader->client().canAuthenticateAgainstProtectionSpace(protectedDocumentLoader().get(), *identifier(), protectionSpace);
}

#endif

#if PLATFORM(HAIKU) && !USE(CURL)

bool ResourceLoader::didReceiveInvalidCertificate(ResourceHandle* handle,
    const BCertificate& certificate, const char* message)
{
    CertificateInfo info(certificate);
    return frameLoader()->notifier().didReceiveInvalidCertificate(this, info, message);
}

#endif

#if PLATFORM(IOS_FAMILY)

RetainPtr<CFDictionaryRef> ResourceLoader::connectionProperties(ResourceHandle*)
{
    RefPtr frameLoader = this->frameLoader();
    if (!frameLoader)
        return nullptr;
    return frameLoader->connectionProperties(this);
}

#endif

void ResourceLoader::receivedCancellation(const AuthenticationChallenge&)
{
    cancel();
}

#if PLATFORM(COCOA)

void ResourceLoader::schedule(SchedulePair& pair)
{
    if (RefPtr handle = m_handle)
        handle->schedule(pair);
}

void ResourceLoader::unschedule(SchedulePair& pair)
{
    if (RefPtr handle = m_handle)
        handle->unschedule(pair);
}

#endif

#if USE(QUICK_LOOK)
bool ResourceLoader::isQuickLookResource() const
{
    return !!m_previewLoader;
}
#endif

bool ResourceLoader::isPDFJSResourceLoad() const
{
#if ENABLE(PDFJS)
    if (!m_request.url().protocolIs("webkit-pdfjs-viewer"_s))
        return false;

    RefPtr frame = m_frame.get();
    RefPtr document = frame && frame->ownerElement() ? &frame->ownerElement()->document() : nullptr;
    return document ? document->isPDFDocument() : false;
#else
    return false;
#endif
}

RefPtr<LocalFrame> ResourceLoader::protectedFrame() const
{
    return m_frame.get();
}

LocalFrame* ResourceLoader::frame() const
{
    return m_frame.get();
}

#if ENABLE(CONTENT_EXTENSIONS)
ResourceMonitor* ResourceLoader::resourceMonitorIfExists()
{
    RefPtr frame = m_frame.get();
    if (RefPtr document = frame ? frame->document() : nullptr)
        return document->resourceMonitorIfExists();
    return nullptr;
}
#endif

} // namespace WebCore

#undef RESOURCELOADER_RELEASE_LOG
#undef PAGE_ID
#undef FRAME_ID
