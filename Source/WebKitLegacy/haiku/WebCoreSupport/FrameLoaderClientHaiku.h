/*
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com> All rights reserved.
 * Copyright (C) 2009 Maxime Simon <simon.maxime@gmail.com> All rights reserved.
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
 *
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FrameLoaderClientHaiku_h
#define FrameLoaderClientHaiku_h

#include <WebCore/FrameLoader.h>
#include <WebCore/LocalFrameLoaderClient.h>
#include <WebCore/ResourceError.h>

#include "wtf/URL.h"
#include <Messenger.h>
#include <unicode/uidna.h>
#include <wtf/Forward.h>

class BWebFrame;
class BWebPage;

namespace WebCore {

class AuthenticationChallenge;
class DocumentLoader;
class Element;
class FormState;
class FrameView;
class HistoryItem;
class NavigationAction;
class ResourceLoader;
class ResourceResponse;

struct LoadErrorResetToken;

class FrameLoaderClientHaiku : public LocalFrameLoaderClient {
 public:
    explicit FrameLoaderClientHaiku(FrameLoader& loader, BWebPage*);
    ~FrameLoaderClientHaiku();

    void setFrame(BWebFrame* frame) {m_webFrame = frame;}
    BWebFrame* webFrame() { return m_webFrame; }
    void setDispatchTarget(const BMessenger& messenger);
    BWebPage* page() const;

    bool hasWebView() const override;

    void makeRepresentation(DocumentLoader*) override;
    void forceLayoutForNonHTML() override;

    void setCopiesOnScroll() override;

    void detachedFromParent2() override;
    void detachedFromParent3() override;

    void assignIdentifierToInitialRequest(ResourceLoaderIdentifier identifier, WebCore::IsMainResourceLoad, DocumentLoader*, const ResourceRequest&) override;

    void dispatchWillSendRequest(DocumentLoader*, ResourceLoaderIdentifier, ResourceRequest&,
                                         const ResourceResponse&) override;
    bool shouldUseCredentialStorage(DocumentLoader*, ResourceLoaderIdentifier identifier) override;
    void dispatchDidReceiveAuthenticationChallenge(DocumentLoader*,
        ResourceLoaderIdentifier identifier, const AuthenticationChallenge&) override;

    bool dispatchDidReceiveInvalidCertificate(DocumentLoader*,
        const CertificateInfo& certificate, const char* message) override;

    void dispatchDidCommitLoad(std::optional<WebCore::HasInsecureContent>,
        std::optional<WebCore::UsedLegacyTLS>, std::optional<WasPrivateRelayed>) override;
    void dispatchDidReceiveResponse(DocumentLoader*, ResourceLoaderIdentifier,
        const ResourceResponse&) override;
    void dispatchDidReceiveContentLength(DocumentLoader*, ResourceLoaderIdentifier, int) override;
    void dispatchDidFinishLoading(DocumentLoader*, WebCore::IsMainResourceLoad, ResourceLoaderIdentifier) override;
    void dispatchDidFailLoading(DocumentLoader*, WebCore::IsMainResourceLoad, ResourceLoaderIdentifier,
                                        const ResourceError&) override;

    void dispatchDidDispatchOnloadEvents() override;
    void dispatchDidReceiveServerRedirectForProvisionalLoad() override;
    void dispatchDidCancelClientRedirect() override;
    void dispatchWillPerformClientRedirect(const URL&, double interval, WallTime fireDate, LockBackForwardList) override;
    void dispatchDidChangeLocationWithinPage() override;
    void dispatchDidPushStateWithinPage() override;
    void dispatchDidReplaceStateWithinPage() override;
    void dispatchDidPopStateWithinPage() override;
    void dispatchWillClose() override;
    void dispatchDidReceiveIcon() override;
    void dispatchDidStartProvisionalLoad() override;
    void dispatchDidReceiveTitle(const StringWithDirection&) override;
    void dispatchDidFailProvisionalLoad(const ResourceError&, WillContinueLoading, WillInternallyHandleFailure) override;
    void dispatchDidFailLoad(const ResourceError&) override;
    void dispatchDidFinishDocumentLoad() override;
    void dispatchDidFinishLoad() override;

    LocalFrame* dispatchCreatePage(const NavigationAction&, NewFrameOpenerPolicy) override;
    void dispatchShow() override;

    void dispatchDecidePolicyForResponse(const ResourceResponse&,
		const ResourceRequest&, const String& downloadAttribute, FramePolicyFunction&&) override;
    void dispatchDecidePolicyForNewWindowAction(const NavigationAction&,
        const ResourceRequest&, FormState*, const String& formName,
		std::optional<WebCore::HitTestResult>&&, FramePolicyFunction&&) override;
    void dispatchDecidePolicyForNavigationAction(const NavigationAction&,
        const ResourceRequest&, const WebCore::ResourceResponse&, FormState*,
		const String& clientRedirectSourceForHistory, std::optional<NavigationIdentifier> navigationID, std::optional<HitTestResult>&&, bool hasOpener, IsPerformingHTTPFallback, SandboxFlags, PolicyDecisionMode, FramePolicyFunction&&) override;
    void cancelPolicyCheck() override;
    void updateSandboxFlags(WebCore::SandboxFlags) override {}

    void dispatchUnableToImplementPolicy(const ResourceError&) override;

    void dispatchWillSendSubmitEvent(WTF::Ref<FormState>&&) override { }
    void dispatchWillSubmitForm(FormState&, WTF::CompletionHandler<void()>&&) override;

    void revertToProvisionalState(DocumentLoader*) override;
    void setMainDocumentError(DocumentLoader*, const ResourceError&) override;
    bool dispatchDidLoadResourceFromMemoryCache(DocumentLoader*,
                                                        const ResourceRequest&,
                                                        const ResourceResponse&, int) override;

    void setMainFrameDocumentReady(bool) override;

    void startDownload(const ResourceRequest&, const String&, FromDownloadAttribute) override;

    void willChangeTitle(DocumentLoader*) override;
    void didChangeTitle(DocumentLoader*) override;

    virtual void willReplaceMultipartContent() override { }
    virtual void didReplaceMultipartContent() override { }

    void committedLoad(DocumentLoader*, const WebCore::SharedBuffer&) override;
    void finishedLoading(DocumentLoader*) override;
    void updateGlobalHistory() override;
    void updateGlobalHistoryRedirectLinks() override;
    void updateOpener(const WebCore::Frame&) override;

    ShouldGoToHistoryItem shouldGoToHistoryItem(HistoryItem&, WebCore::IsSameDocumentNavigation,
        ProcessSwapDisposition) const override;
    bool supportsAsyncShouldGoToHistoryItem() const override { return false; }
    void shouldGoToHistoryItemAsync(HistoryItem&,
        WTF::CompletionHandler<void(ShouldGoToHistoryItem)>&&) const override;
    RefPtr<HistoryItem> createHistoryItemTree(bool clipAtTarget,
        BackForwardItemIdentifier) const override;

    bool canCachePage() const override;
    void convertMainResourceLoadToDownload(DocumentLoader*,
        const ResourceRequest&, const ResourceResponse&) override;

    void didDisplayInsecureContent() override;

    void didRunInsecureContent(SecurityOrigin&) override;

    bool shouldFallBack(const ResourceError&) const override;

    String userAgent(const URL&) const override;

    void savePlatformDataToCachedFrame(CachedFrame*) override;
    void transitionToCommittedFromCachedFrame(CachedFrame*) override;
    void transitionToCommittedForNewPage(InitializingIframe) override;

    bool canHandleRequest(const ResourceRequest&) const override;
    bool canShowMIMEType(const String& MIMEType) const override;
    bool canShowMIMETypeAsHTML(const String& MIMEType) const override;
    bool representationExistsForURLScheme(StringView URLScheme) const override;
    String generatedMIMETypeForURLScheme(StringView URLScheme) const override;

    void frameLoadCompleted() override;
    void saveViewStateToItem(HistoryItem&) override;
    void restoreViewState() override;
    void provisionalLoadStarted() override;
    void didFinishLoad() override;
    void prepareForDataSourceReplacement() override;
    Ref<DocumentLoader> createDocumentLoader(ResourceRequest&&, SubstituteData&&) override;

    void setTitle(const StringWithDirection&, const URL&) override;

    RefPtr<LocalFrame> createFrame(const AtomString& name, HTMLFrameOwnerElement&) override;
    RefPtr<Widget> createPlugin(HTMLPlugInElement&, const URL&, const Vector<AtomString>&,
        const Vector<AtomString>&, const String&, bool) override;
    void redirectDataToPlugin(Widget& pluginWidget) override;

    ObjectContentType objectContentType(const URL&, const String& mimeType) override;

    AtomString overrideMediaType() const override;

    void dispatchDidClearWindowObjectInWorld(DOMWrapperWorld&) override;
    void dispatchLoadEventToOwnerElementInAnotherProcess() override;

    Ref<FrameNetworkingContext> createNetworkingContext() override;
    void updateCachedDocumentLoader(WebCore::DocumentLoader&) override { }

    void prefetchDNS(const String&) override { }

    void didRestoreFromBackForwardCache() final {}

    void sendH2Ping(const WTF::URL&, WTF::CompletionHandler<void(std::experimental::fundamentals_v3::expected<WTF::Seconds, WebCore::ResourceError>&&)>&&) final { notImplemented(); }
 private:
    bool isTertiaryMouseButton(const NavigationAction& action) const;

    status_t dispatchNavigationRequested(const ResourceRequest& request) const;
    status_t dispatchMessage(BMessage& message, bool allowChildFrame = false) const;

    void loadStorageAccessQuirksIfNeeded() final override {}

private:
    BWebPage* m_webPage;
    BWebFrame* m_webFrame;
    BMessenger m_messenger;

    bool m_loadingErrorPage;

    // IDNA domain encoding and decoding.
    UIDNA* m_uidna_context;
};

}

#endif // FrameLoaderClientHaiku_h

