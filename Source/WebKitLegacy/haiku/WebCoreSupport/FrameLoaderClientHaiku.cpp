/*
 * Copyright (C) 2006 Don Gibson <dgibson77@gmail.com>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2007 Trolltech ASA
 * Copyright (C) 2007 Ryan Leavengood <leavengood@gmail.com> All rights reserved.
 * Copyright (C) 2009 Maxime Simon <simon.maxime@gmail.com> All rights reserved.
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
 * Copyright (C) 2010 Michael Lotz <mmlr@mlotz.ch>
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

#include "config.h"
#include "FrameLoaderClientHaiku.h"

#include "DumpRenderTreeClient.h"
#include "FrameNetworkingContextHaiku.h"
#include "IconDatabase.h"

#include "WebCore/AuthenticationChallenge.h"
#include "WebCore/BackForwardController.h"
#include "WebCore/Credential.h"
#include "WebCore/CachedFrame.h"
#include "WebCore/DocumentLoader.h"
#include "WebCore/DOMWrapperWorld.h"
#include "WebCore/FormState.h"
#include "WebCore/Frame.h"
#include "WebCore/FrameInlines.h"
#include "WebCore/FrameLoader.h"
#include "WebCore/FrameTree.h"
#include "WebCore/FrameView.h"
#include "WebCore/HistoryController.h"
#include "WebCore/HistoryItem.h"
#include "WebCore/HitTestResult.h"
#include "WebCore/HTMLFormElement.h"
#include "WebCore/HTMLFrameOwnerElement.h"
#include "WebCore/HTTPParsers.h"
#include "WebCore/MouseEvent.h"
#include "WebCore/MIMETypeRegistry.h"
#include "WebCore/NotImplemented.h"
#include "WebCore/Page.h"
#include "WebCore/PluginData.h"
#include "WebCore/ProgressTracker.h"
#include "WebCore/RenderFrame.h"
#include "WebCore/ResourceRequest.h"
#include "WebCore/ScriptController.h"
#include "WebCore/Settings.h"
#include "WebCore/UserAgent.h"
#include "WebFrame.h"
#include "WebFramePrivate.h"
#include "WebKitInfo.h"
#include "WebPage.h"
#include "WebView.h"
#include "WebViewConstants.h"

#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/JSContextRef.h>
#include <wtf/CompletionHandler.h>

#include <Alert.h>
#include <Bitmap.h>
#include <Entry.h>
#include <locale/Collator.h>
#include <Locale.h>
#include <Message.h>
#include <MimeType.h>
#include <String.h>
#include <Url.h>
#include <assert.h>
#include <debugger.h>

//#define TRACE_FRAME_LOADER_CLIENT
#ifdef TRACE_FRAME_LOADER_CLIENT
#   define CLASS_NAME "FLC"
#   include "FunctionTracer.h"
#else
#    define CALLED(x...)
#    define TRACE(x...)
#endif

namespace WebCore {

FrameLoaderClientHaiku::FrameLoaderClientHaiku(WebCore::FrameLoader& loader, BWebPage* webPage)
    : LocalFrameLoaderClient(loader)
    , m_webPage(webPage)
    , m_webFrame(nullptr)
    , m_messenger()
    , m_loadingErrorPage(false)
    , m_uidna_context(nullptr)
{
    CALLED("BWebPage: %p", webPage);
    ASSERT(m_webPage);
}


FrameLoaderClientHaiku::~FrameLoaderClientHaiku()
{
    uidna_close(m_uidna_context);
}


void FrameLoaderClientHaiku::setDispatchTarget(const BMessenger& messenger)
{
    m_messenger = messenger;
}

BWebPage* FrameLoaderClientHaiku::page() const
{
    return m_webPage;
}

bool FrameLoaderClientHaiku::hasWebView() const
{
    return m_webPage->WebView();
}

void FrameLoaderClientHaiku::makeRepresentation(DocumentLoader*)
{
}

void FrameLoaderClientHaiku::forceLayoutForNonHTML()
{
}

void FrameLoaderClientHaiku::setCopiesOnScroll()
{
    // Other ports mention "apparently mac specific", but I believe it may have to
    // do with achieving that WebCore does not repaint the parts that we can scroll
    // by blitting.
}

void FrameLoaderClientHaiku::detachedFromParent2()
{
}

void FrameLoaderClientHaiku::detachedFromParent3()
{
}

bool FrameLoaderClientHaiku::dispatchDidLoadResourceFromMemoryCache(DocumentLoader* /*loader*/,
                                                                    const ResourceRequest& /*request*/,
                                                                    const ResourceResponse& /*response*/,
                                                                    int /*length*/)
{
    notImplemented();
    return false;
}

void FrameLoaderClientHaiku::assignIdentifierToInitialRequest(ResourceLoaderIdentifier identifier,
                                                              WebCore::IsMainResourceLoad, 
                                                              DocumentLoader* loader,
                                                              const ResourceRequest& request)
{
#if 0
    WebCore::Page* webPage = m_webFrame->Frame()->page();
    if (!webPage)
        return;

    bool pageIsProvisionallyLoading = false;
    if (FrameLoader* frameLoader = loader ? loader->frameLoader() : nullptr)
        pageIsProvisionallyLoading = frameLoader->provisionalDocumentLoader() == loader;

    webPage->injectedBundleResourceLoadClient().didInitiateLoadForResource(*webPage, m_webFrame, identifier, request, pageIsProvisionallyLoading);

    webPage->addResourceRequest(identifier, request);
#endif
}

void FrameLoaderClientHaiku::dispatchWillSendRequest(DocumentLoader* /*loader*/, ResourceLoaderIdentifier /*identifier*/,
                                                     ResourceRequest& request,
                                                     const ResourceResponse& /*redirectResponse*/)
{
    WebCore::Page* webPage = m_webFrame->Frame()->page();
    if (!webPage)
        return;

    // The API can return a completely new request. We should ensure that at least the requester
    // is kept, so that if this is a main resource load it's still considered as such.
    auto requester = request.requester();
    auto appInitiatedValue = request.isAppInitiated();
#if 0
    webPage->injectedBundleResourceLoadClient().willSendRequestForFrame(*webPage, m_frame, identifier, request, redirectResponse);
#endif
    if (!request.isNull()) {
        request.setRequester(requester);
        request.setIsAppInitiated(appInitiatedValue);
    }
}

bool FrameLoaderClientHaiku::shouldUseCredentialStorage(DocumentLoader*, ResourceLoaderIdentifier)
{
    notImplemented();
    return false;
}

void FrameLoaderClientHaiku::dispatchDidReceiveAuthenticationChallenge(DocumentLoader*, ResourceLoaderIdentifier, const AuthenticationChallenge& challenge)
{
    const ProtectionSpace& space = challenge.protectionSpace();
    StringBuilder text;
    text.append("Host \""_s);
    text.append(space.host());
    text.append("\" requests authentication for realm \""_s);
    text.append(space.realm());
    text.append("\"\n"_s);
    text.append("Authentication Scheme: "_s);
    switch (space.authenticationScheme()) {
        case WebCore::ProtectionSpaceBase::AuthenticationScheme::HTTPBasic:
        text.append("Basic (data will be sent as plain text)"_s);
        break;
        case WebCore::ProtectionSpaceBase::AuthenticationScheme::HTTPDigest:
        text.append("Digest (data will not be sent plain text)"_s);
        break;
    default:
        text.append("Unknown (possibly plaintext)"_s);
        break;
    }

    BMessage challengeMessage(AUTHENTICATION_CHALLENGE);
    challengeMessage.AddString("text", text.toString());
    challengeMessage.AddString("user", challenge.proposedCredential().user());
    challengeMessage.AddString("password", challenge.proposedCredential().password());
    challengeMessage.AddUInt32("failureCount", challenge.previousFailureCount());
    challengeMessage.AddPointer("view", m_webPage->WebView());

    BMessage authenticationReply;
    m_messenger.SendMessage(&challengeMessage, &authenticationReply);

    BString user;
    BString password;
    if (authenticationReply.FindString("user", &user) != B_OK
        || authenticationReply.FindString("password", &password) != B_OK) {
        challenge.authenticationClient()->receivedCancellation(challenge);
    } else {
        if (!user.Length() && !password.Length())
            challenge.authenticationClient()->receivedRequestToContinueWithoutCredential(challenge);
        else {
            bool rememberCredentials = false;
            CredentialPersistence persistence = CredentialPersistence::ForSession;
            if (authenticationReply.FindBool("rememberCredentials",
                &rememberCredentials) == B_OK && rememberCredentials) {
                persistence = CredentialPersistence::Permanent;
            }

            Credential credential(String::fromUTF8(user.String()),
                String::fromUTF8(password.String()), persistence);
            challenge.authenticationClient()->receivedCredential(challenge, credential);
        }
    }
}


bool FrameLoaderClientHaiku::dispatchDidReceiveInvalidCertificate(DocumentLoader* loader,
    const CertificateInfo& certificate, const char* message)
{
    String text = makeString("The SSL certificate received from "_s,
        loader->url().string(),
        " could not be authenticated for the following reason: "_s, StringView::fromLatin1(message),
        ".\n\nThe secure connection to the website may be compromised, make sure to not send any sensitive information."_s);

    BMessage warningMessage(SSL_CERT_ERROR);
    warningMessage.AddString("text", text.utf8().data());
    warningMessage.AddPointer("certificate info", &certificate);

    BMessage reply;
    m_messenger.SendMessage(&warningMessage, &reply);

    bool continueAnyway = reply.FindBool("continue");

    return continueAnyway;
}


void FrameLoaderClientHaiku::dispatchDidReceiveResponse(DocumentLoader* loader,
                                                        ResourceLoaderIdentifier identifier,
                                                        const ResourceResponse& coreResponse)
{
    loader->writer().setEncoding(coreResponse.textEncodingName(),
        WebCore::DocumentWriter::IsEncodingUserChosen::No);

    BMessage message(RESPONSE_RECEIVED);
    message.AddInt32("status", coreResponse.httpStatusCode());
    message.AddInt64("identifier", identifier.toUInt64());
    message.AddString("url", coreResponse.url().string());
    message.AddString("mimeType", coreResponse.mimeType().utf8().data());
    dispatchMessage(message);
}

void FrameLoaderClientHaiku::dispatchDidReceiveContentLength(DocumentLoader* /*loader*/,
                                                             ResourceLoaderIdentifier /*id*/, int /*length*/)
{
    notImplemented();
}

void FrameLoaderClientHaiku::dispatchDidFinishLoading(DocumentLoader* /*loader*/, WebCore::IsMainResourceLoad, ResourceLoaderIdentifier /*identifier*/)
{
    notImplemented();
}

void FrameLoaderClientHaiku::dispatchDidFailLoading(DocumentLoader* loader, WebCore::IsMainResourceLoad, ResourceLoaderIdentifier, const ResourceError& error)
{
    if (error.isCancellation())
        return;
    BMessage message(LOAD_FAILED);
    message.AddString("url", loader->url().string());
    dispatchMessage(message);
}

void FrameLoaderClientHaiku::dispatchDidDispatchOnloadEvents()
{
    CALLED();
    BMessage message(LOAD_ONLOAD_HANDLE);
    dispatchMessage(message);
}

void FrameLoaderClientHaiku::dispatchDidReceiveServerRedirectForProvisionalLoad()
{
    notImplemented();
}

void FrameLoaderClientHaiku::dispatchDidCancelClientRedirect()
{
    notImplemented();
}

void FrameLoaderClientHaiku::dispatchWillPerformClientRedirect(const URL&, double /*interval*/, WallTime /*fireDate*/, LockBackForwardList)
{
    notImplemented();
}

void FrameLoaderClientHaiku::dispatchDidChangeLocationWithinPage()
{
    BMessage message(LOAD_DOC_COMPLETED);
    message.AddPointer("frame", m_webFrame);
    message.AddString("url", m_webFrame->Frame()->document()->url().string());
    dispatchMessage(message);
}

void FrameLoaderClientHaiku::dispatchDidPushStateWithinPage()
{
    notImplemented();
}

void FrameLoaderClientHaiku::dispatchDidReplaceStateWithinPage()
{
    notImplemented();
}

void FrameLoaderClientHaiku::dispatchDidPopStateWithinPage()
{
    notImplemented();
}

void FrameLoaderClientHaiku::dispatchWillClose()
{
    notImplemented();
}

void FrameLoaderClientHaiku::dispatchDidReceiveIcon()
{
    if (m_loadingErrorPage)
        return;

    BMessage message(ICON_RECEIVED);
    message.AddString("url", m_webFrame->Frame()->document()->url().string());
    dispatchMessage(message);
}

void FrameLoaderClientHaiku::dispatchDidStartProvisionalLoad()
{
    CALLED();
    if (m_loadingErrorPage) {
        TRACE("m_loadingErrorPage\n");
        m_loadingErrorPage = false;
    }

    BMessage message(LOAD_NEGOTIATING);
    message.AddString("url",
		m_webFrame->Frame()->loader().provisionalDocumentLoader()->request().url().string());
    dispatchMessage(message);
}

void FrameLoaderClientHaiku::dispatchDidReceiveTitle(const StringWithDirection& title)
{
    CALLED();
    if (m_loadingErrorPage) {
        TRACE("m_loadingErrorPage\n");
        return;
    }

    m_webFrame->SetTitle(title.string);

    BMessage message(TITLE_CHANGED);
    message.AddString("title", title.string);
    message.AddBool("ltr", title.direction == TextDirection::LTR);
    dispatchMessage(message);
}

void FrameLoaderClientHaiku::dispatchDidCommitLoad(
	std::optional<WebCore::HasInsecureContent>,
	std::optional<WebCore::UsedLegacyTLS>, std::optional<WasPrivateRelayed>)
{
    CALLED();
    if (m_loadingErrorPage) {
        TRACE("m_loadingErrorPage\n");
        return;
    }

    BMessage message(LOAD_COMMITTED);
    URL url = m_webFrame->Frame()->loader().documentLoader()->request().url();
    BUrl decoded(url);

    // In WebKit URL, the host may be IDN-encoded. Decode it for displaying.
    char dest[2048];
    UErrorCode error = U_ZERO_ERROR;
    if (!m_uidna_context)
        m_uidna_context = uidna_openUTS46(UIDNA_DEFAULT, &error);
    UIDNAInfo info = UIDNA_INFO_INITIALIZER;

    uidna_nameToUnicodeUTF8(m_uidna_context, url.host().utf8().data(),
        -1 /* NULL-terminated */, dest, sizeof(dest), &info, &error);

    if (U_SUCCESS(error) && info.errors == 0)
        decoded.SetHost(dest);

    message.AddString("url", decoded);
    dispatchMessage(message);

    // We should assume first the frame has no title. If it has, then the above
    // dispatchDidReceiveTitle() will be called very soon with the correct title.
    // This properly resets the title when we navigate to a URI without a title.
    BMessage titleMessage(TITLE_CHANGED);
    titleMessage.AddString("title", "");
    dispatchMessage(titleMessage);
}

void FrameLoaderClientHaiku::dispatchDidFailProvisionalLoad(const ResourceError& error, WillContinueLoading, WillInternallyHandleFailure)
{
    dispatchDidFailLoad(error);
}

void FrameLoaderClientHaiku::dispatchDidFailLoad(const ResourceError& error)
{
    CALLED();
    if (m_loadingErrorPage) {
        TRACE("m_loadingErrorPage\n");
        return;
    }
    if (!shouldFallBack(error)) {
        TRACE("should not fall back\n");
        return;
    }

    m_loadingErrorPage = true;

    // NOTE: This could be used to display the error right in the page. However, I find
    // the error alert somehow better to manage. For example, on a partial load error,
    // at least some content stays readable if we don't overwrite it with the error text... :-)
//    BString content("<html><body>");
//    content << error.localizedDescription().utf8().data();
//    content << "</body></html>";
//
//    m_webFrame->SetFrameSource(content);
}

void FrameLoaderClientHaiku::dispatchDidFinishDocumentLoad()
{
    BMessage message(LOAD_DOC_COMPLETED);
    message.AddPointer("frame", m_webFrame);
    message.AddString("url", m_webFrame->Frame()->document()->url().string());
    dispatchMessage(message);
}

void FrameLoaderClientHaiku::dispatchDidFinishLoad()
{
    CALLED();
    if (m_loadingErrorPage) {
        m_loadingErrorPage = false;
        TRACE("m_loadingErrorPage\n");
        return;
    }

    BMessage message(LOAD_FINISHED);
    message.AddPointer("frame", m_webFrame);
    message.AddString("url", m_webFrame->Frame()->document()->url().string());
    dispatchMessage(message);
}

void FrameLoaderClientHaiku::dispatchWillSubmitForm(FormState&, WTF::CompletionHandler<void()>&& function)
{
    CALLED();
    notImplemented();
	// It seems we can access the form content here, and maybe store it for auto-complete and the like.
    function();
}

LocalFrame* FrameLoaderClientHaiku::dispatchCreatePage(const NavigationAction& /*action*/, NewFrameOpenerPolicy)
{
    CALLED();
    WebCore::Page* page = m_webPage->createNewPage();
    if (page)
        return static_cast<WebCore::LocalFrame*>(&page->mainFrame());

    return 0;
}

void FrameLoaderClientHaiku::dispatchShow()
{
    CALLED();
    notImplemented();
}

void FrameLoaderClientHaiku::dispatchDecidePolicyForResponse(
	const WebCore::ResourceResponse& response,
	const WebCore::ResourceRequest& request, const WTF::String&, FramePolicyFunction&& function)
{
    if (request.isNull()) {
        function(PolicyAction::Ignore);
        return;
    }
    // we need to call directly here
    if (!response.isAttachment() && canShowMIMEType(response.mimeType())) {
        function(PolicyAction::Use);
    } else if (!request.url().protocolIsFile() && response.mimeType() != ASCIILiteral::fromLiteralUnsafe("application/x-shockwave-flash")) {
        function(PolicyAction::Download);
    } else {
        function(PolicyAction::Ignore);
    }
}

void FrameLoaderClientHaiku::dispatchDecidePolicyForNewWindowAction(
	const NavigationAction& action, const ResourceRequest& request,
	FormState* /*formState*/, const String& /*targetName*/,
	std::optional<HitTestResult>&&,
	FramePolicyFunction&& function)
{
    ASSERT(function);
    if (!function)
        return;

    if (request.isNull()) {
        function(PolicyAction::Ignore);
        return;
    }

    if (!m_messenger.IsValid() || !isTertiaryMouseButton(action)) {
        dispatchNavigationRequested(request);
        function(PolicyAction::Use);
        return;
    }

    // Clicks with the tertiary mouse button shall open a new window,
    // (or tab respectively depending on browser) - *ignore* the request for this page
    // then, since we create it ourself.
    BMessage message(NEW_WINDOW_REQUESTED);
    message.AddString("url", request.url().string());

    bool switchTab = false;

    // Switch to the new tab, when shift is pressed.
    if (action.mouseEventData().has_value()) {
        switchTab = action.mouseEventData()->shiftKey;
    }

    message.AddBool("primary", switchTab);
    dispatchMessage(message, true);

    if (action.type() == NavigationType::FormSubmitted || action.type() == NavigationType::FormResubmitted)
        m_webFrame->Frame()->loader().resetMultipleFormSubmissionProtection();

    if (action.type() == NavigationType::LinkClicked) {
        ResourceRequest emptyRequest;
        m_webFrame->Frame()->loader().activeDocumentLoader()->setLastCheckedRequest(WTFMove(emptyRequest));
    }

    function(PolicyAction::Ignore);
}

void FrameLoaderClientHaiku::dispatchDecidePolicyForNavigationAction(
	const NavigationAction& action, const ResourceRequest& request,
	const WebCore::ResourceResponse& response, FormState* formState,
	const String&, std::optional<NavigationIdentifier> identifier, std::optional<HitTestResult>&& hit, bool, IsPerformingHTTPFallback, SandboxFlags, PolicyDecisionMode, FramePolicyFunction&& function)
{
    // Potentially we want to open a new window, when the user clicked with the
    // tertiary mouse button. That's why we can reuse the other method.
    dispatchDecidePolicyForNewWindowAction(action, request, formState, String(),
		std::move(hit), std::move(function));
}

void FrameLoaderClientHaiku::cancelPolicyCheck()
{
    CALLED();
    notImplemented();
}

void FrameLoaderClientHaiku::dispatchUnableToImplementPolicy(const ResourceError&)
{
    CALLED();
    notImplemented();
}

void FrameLoaderClientHaiku::revertToProvisionalState(DocumentLoader*)
{
    CALLED();
    notImplemented();
}

void FrameLoaderClientHaiku::setMainDocumentError(WebCore::DocumentLoader* /*loader*/, const WebCore::ResourceError& error)
{
    CALLED();

    if (error.isCancellation())
        return;

    BMessage message(MAIN_DOCUMENT_ERROR);
    message.AddString("url", error.failingURL().string().utf8().data());
    message.AddString("error", error.localizedDescription());
    dispatchMessage(message);
}

void FrameLoaderClientHaiku::setMainFrameDocumentReady(bool)
{
    // this is only interesting once we provide an external API for the DOM
}

void FrameLoaderClientHaiku::startDownload(const ResourceRequest& request, const String&, FromDownloadAttribute)
{
    m_webPage->requestDownload(request);
}

void FrameLoaderClientHaiku::willChangeTitle(DocumentLoader*)
{
    // We act in didChangeTitle
}

void FrameLoaderClientHaiku::didChangeTitle(DocumentLoader* docLoader)
{
    setTitle(docLoader->title(), docLoader->url());
}

void FrameLoaderClientHaiku::committedLoad(WebCore::DocumentLoader* loader, const WebCore::SharedBuffer& buffer)
{
    CALLED();

    ASSERT(loader->frame());
    loader->commitData(buffer);

#if 0
    Frame* coreFrame = loader->frame();
    if (coreFrame && coreFrame->document()->isMediaDocument())
        loader->cancelMainResourceLoad(coreFrame->loader().client().pluginWillHandleLoadError(loader->response()));
#endif
}

void FrameLoaderClientHaiku::finishedLoading(DocumentLoader* /*documentLoader*/)
{
    CALLED();
}

void FrameLoaderClientHaiku::updateGlobalHistory()
{
    WebCore::LocalFrame* frame = m_webFrame->Frame();
    if (!frame)
        return;

    BMessage message(UPDATE_HISTORY);
    message.AddString("url", frame->loader().documentLoader()->urlForHistory().string());
    dispatchMessage(message);
}

void FrameLoaderClientHaiku::updateGlobalHistoryRedirectLinks()
{
    updateGlobalHistory();
}

void FrameLoaderClientHaiku::updateOpener(const WebCore::Frame& newOpener)
{
}

ShouldGoToHistoryItem FrameLoaderClientHaiku::shouldGoToHistoryItem(HistoryItem&,
    WebCore::IsSameDocumentNavigation, ProcessSwapDisposition) const
{
    // FIXME this may want to ask the user for confirmation if the request contained a form post or
    // similar, and re-doing the request could have side effects. It may be easier to use the async
    // version to wait for a BAlert without locking everything.
    return ShouldGoToHistoryItem::Yes;
}

void FrameLoaderClientHaiku::shouldGoToHistoryItemAsync(WebCore::HistoryItem&, CompletionHandler<void(ShouldGoToHistoryItem)>&& handler) const
{
    handler(ShouldGoToHistoryItem::Yes);
}

RefPtr<HistoryItem> FrameLoaderClientHaiku::createHistoryItemTree(bool clipAtTarget,
    BackForwardItemIdentifier identifier) const
{
    WebCore::LocalFrame* frame = m_webFrame->Frame();
    if (!frame)
        return nullptr;

    return frame->loader().history().createItemTree(*frame, clipAtTarget, identifier);
}

void FrameLoaderClientHaiku::didDisplayInsecureContent()
{
}

void FrameLoaderClientHaiku::didRunInsecureContent(WebCore::SecurityOrigin&)
{
    notImplemented();
}

void FrameLoaderClientHaiku::convertMainResourceLoadToDownload(DocumentLoader*,
    const ResourceRequest& request, const ResourceResponse&)
{
    startDownload(request, {}, FromDownloadAttribute::No);
}

bool FrameLoaderClientHaiku::shouldFallBack(const WebCore::ResourceError& error) const
{
    return !(error.isCancellation()
             || error.errorCode() == WebKitErrorFrameLoadInterruptedByPolicyChange
             || error.errorCode() == WebKitErrorPlugInWillHandleLoad);
}

bool FrameLoaderClientHaiku::canHandleRequest(const WebCore::ResourceRequest&) const
{
    // notImplemented();
    return true;
}

bool FrameLoaderClientHaiku::canShowMIMETypeAsHTML(const String& /*MIMEType*/) const
{
    notImplemented();
    return false;
}

bool FrameLoaderClientHaiku::canShowMIMEType(const String& mimeType) const
{
    CALLED("%s", mimeType.utf8().data());
    // FIXME: Usually, the mime type will have been detexted. This is supposed to work around
    // downloading some empty files, that can be observed.
    if (!mimeType.length())
        return true;

    if (MIMETypeRegistry::canShowMIMEType(mimeType))
        return true;

#if 0
    Frame* frame = m_webFrame->Frame();
    if (frame && frame->settings() && frame->settings()->arePluginsEnabled()
        && PluginDatabase::installedPlugins()->isMIMETypeRegistered(mimeType))
        return true;
#endif

    return false;
}

bool FrameLoaderClientHaiku::representationExistsForURLScheme(StringView /*URLScheme*/) const
{
    return false;
}

String FrameLoaderClientHaiku::generatedMIMETypeForURLScheme(StringView /*URLScheme*/) const
{
    notImplemented();
    return String();
}

void FrameLoaderClientHaiku::frameLoadCompleted()
{
}

void FrameLoaderClientHaiku::saveViewStateToItem(HistoryItem&)
{
    notImplemented();
}

void FrameLoaderClientHaiku::restoreViewState()
{
    // This seems unimportant, the Qt port mentions this for it's corresponding signal:
    //   "This signal is emitted when the load of \a frame is finished and the application
    //   may now update its state accordingly."
    // Could be this is important for ports which use actual platform widgets.
    notImplemented();
}

void FrameLoaderClientHaiku::provisionalLoadStarted()
{
    notImplemented();
}

void FrameLoaderClientHaiku::didFinishLoad()
{
    notImplemented();
}

void FrameLoaderClientHaiku::prepareForDataSourceReplacement()
{
    // notImplemented(); // Nor does any port except Apple one.
}

WTF::Ref<DocumentLoader> FrameLoaderClientHaiku::createDocumentLoader(ResourceRequest&& request, SubstituteData&& substituteData)
{
    CALLED("request: %s", request.url().string().utf8().data());
    return DocumentLoader::create(std::move(request), std::move(substituteData));
}

void FrameLoaderClientHaiku::setTitle(const StringWithDirection&, const URL&)
{
    // no need for, dispatchDidReceiveTitle is the right callback
}

void FrameLoaderClientHaiku::savePlatformDataToCachedFrame(CachedFrame* /*cachedPage*/)
{
    CALLED();
    // Nothing to be done here for the moment. We don't associate any platform data
}

void FrameLoaderClientHaiku::transitionToCommittedFromCachedFrame(CachedFrame* cachedFrame)
{
    CALLED();
    ASSERT(cachedFrame->view());

    // FIXME: I guess we would have to restore platform data from the cachedFrame here,
    // data associated in savePlatformDataToCachedFrame().

    cachedFrame->view()->setTopLevelPlatformWidget(m_webPage->WebView());
}

void FrameLoaderClientHaiku::transitionToCommittedForNewPage(InitializingIframe)
{
    CALLED();
    ASSERT(m_webFrame);

    LocalFrame* frame = m_webFrame->Frame();

    assert(frame);

    BRect bounds = m_webPage->viewBounds();
    IntSize size = IntSize(bounds.IntegerWidth() + 1, bounds.IntegerHeight() + 1);

    std::optional<Color> backgroundColor;
    if (m_webFrame->IsTransparent())
        backgroundColor = Color(Color::transparentBlack);
    frame->createView(size, backgroundColor, {}, {});

    frame->view()->setTopLevelPlatformWidget(m_webPage->WebView());
}

String FrameLoaderClientHaiku::userAgent(const URL&) const
{
    // FIXME: Get the app name from the app. Hardcoded WebPositive for now.
    return WebCore::standardUserAgent("WebPositive"_s, "1.3"_s);
}

bool FrameLoaderClientHaiku::canCachePage() const
{
    return true;
}

RefPtr<LocalFrame> FrameLoaderClientHaiku::createFrame(const AtomString& name,
    HTMLFrameOwnerElement& ownerElement)
{
    ASSERT(m_webFrame);
    ASSERT(m_webPage);

    BWebFrame* subFrame = m_webFrame->AddChild(m_webPage, name.string().utf8().data(), &ownerElement);
    if (!subFrame)
        return nullptr;

    RefPtr<WebCore::LocalFrame> coreSubFrame = subFrame->Frame();
    ASSERT(coreSubFrame);

    subFrame->SetListener(m_messenger);
    return coreSubFrame;
}

ObjectContentType FrameLoaderClientHaiku::objectContentType(const URL& url, const String& originalMimeType)
{
    CALLED();
    if (url.isEmpty() && !originalMimeType.length())
        return ObjectContentType::None;

    String mimeType = originalMimeType;
    if (!mimeType.length()) {
        entry_ref ref;
        if (get_ref_for_path(url.path().utf8().data(), &ref) == B_OK) {
            BMimeType type;
            if (BMimeType::GuessMimeType(&ref, &type) == B_OK)
                mimeType = String::fromUTF8(type.Type());
        } else {
            // For non-file URLs, try guessing from the extension (this happens
            // before the request so our content sniffing is of no use)
            mimeType = MIMETypeRegistry::mimeTypeForExtension(toString(url.path().substring(url.path().reverseFind('.') + 1)));
        } 
    }

    if (!mimeType.length())
        return ObjectContentType::Frame;

    if (MIMETypeRegistry::isSupportedImageMIMEType(mimeType))
        return ObjectContentType::Image;

#if 0
    if (PluginDatabase::installedPlugins()->isMIMETypeRegistered(mimeType))
        return ObjectContentNetscapePlugin;
#endif

    if (MIMETypeRegistry::isSupportedNonImageMIMEType(mimeType))
        return ObjectContentType::Frame;

    if (url.protocol() == ASCIILiteral::fromLiteralUnsafe("about"))
        return ObjectContentType::Frame;

    return ObjectContentType::None;
}

RefPtr<Widget> FrameLoaderClientHaiku::createPlugin(HTMLPlugInElement&, const URL&, const Vector<AtomString>&,
                                                        const Vector<AtomString>&, const String&, bool /*loadManually*/)
{
    CALLED();
    notImplemented();
    return nullptr;
}

void FrameLoaderClientHaiku::redirectDataToPlugin(Widget& pluginWidge)
{
    CALLED();
    debugger("plugins are not implemented on Haiku!");
}

AtomString FrameLoaderClientHaiku::overrideMediaType() const
{
    // This will do, until we support printing.
    return ASCIILiteral::fromLiteralUnsafe("screen");
}

void FrameLoaderClientHaiku::dispatchDidClearWindowObjectInWorld(DOMWrapperWorld& world)
{
    if (&world != &mainThreadNormalWorldSingleton())
        return;

    if (m_webFrame) {
        BMessage message(JAVASCRIPT_WINDOW_OBJECT_CLEARED);
        dispatchMessage(message);

    }

    if (m_webPage->fDumpRenderTree)
    {
        // DumpRenderTree registers the TestRunner JavaScript object using this
        // callback. This can't be done using the asynchronous BMessage above:
        // by the time the message is processed by the target, the JS test will
        // already have run!
        JSGlobalContextRef context = toGlobalRef(m_webFrame->Frame()->script().globalObject(mainThreadNormalWorldSingleton()));
        JSObjectRef windowObject = JSContextGetGlobalObject(context);
        m_webPage->fDumpRenderTree->didClearWindowObjectInWorld(world, context, windowObject);
    }

}

Ref<FrameNetworkingContext> FrameLoaderClientHaiku::createNetworkingContext()
{
    return FrameNetworkingContextHaiku::create(m_webFrame->Frame(), m_webPage->GetContext());
}

// #pragma mark - private

bool FrameLoaderClientHaiku::isTertiaryMouseButton(const NavigationAction& action) const
{
    if (action.mouseEventData().has_value()) {
        return (action.mouseEventData()->button == MouseButton::Middle);
    }
    return false;
}

status_t FrameLoaderClientHaiku::dispatchNavigationRequested(const ResourceRequest& request) const
{
    BMessage message(NAVIGATION_REQUESTED);
    message.AddString("url", request.url().string());
    return dispatchMessage(message);
}

void FrameLoaderClientHaiku::dispatchLoadEventToOwnerElementInAnotherProcess()
{
}

status_t FrameLoaderClientHaiku::dispatchMessage(BMessage& message, bool allowChildFrame) const
{
    message.AddPointer("view", m_webPage->WebView());
    message.AddPointer("frame", m_webFrame);

    // Most messages are only relevant when they come from the main frame
    // (setting the title, favicon, url, loading progress, etc). We intercept
    // the ones coming from child frames here.
    // Currently, the only exception is the message for navigation policy. This
    // allows opening a new tab by middle-clicking a link that's in a frame.
    if (allowChildFrame || m_webFrame == m_webPage->MainFrame())
        return m_messenger.SendMessage(&message);
    else
        return B_NOT_ALLOWED;
}

} // namespace WebCore

