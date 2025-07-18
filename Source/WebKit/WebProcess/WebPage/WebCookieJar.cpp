/*
 * Copyright (C) 2019-2025 Apple Inc. All rights reserved.
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
#include "WebCookieJar.h"

#include "Logging.h"
#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "WebFrame.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/Cookie.h>
#include <WebCore/CookieRequestHeaderFieldProxy.h>
#include <WebCore/CookieStoreGetOptions.h>
#include <WebCore/DeprecatedGlobalSettings.h>
#include <WebCore/Document.h>
#include <WebCore/EmptyFrameLoaderClient.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameDestructionObserverInlines.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/LocalFrameLoaderClient.h>
#include <WebCore/Page.h>
#include <WebCore/ScriptTrackingPrivacyCategory.h>
#include <WebCore/Settings.h>
#include <WebCore/StorageSessionProvider.h>
#include <optional>
#include <wtf/HashSet.h>
#include <wtf/Vector.h>

namespace WebKit {

using namespace WebCore;

class WebStorageSessionProvider : public WebCore::StorageSessionProvider {
    // NetworkStorageSessions are accessed only in the NetworkProcess.
    WebCore::NetworkStorageSession* storageSession() const final { return nullptr; }
};

WebCookieJar::WebCookieJar()
    : WebCore::CookieJar(adoptRef(*new WebStorageSessionProvider))
    , m_cache(WebCookieCache::create())
{ }

enum class BlockCookies : uint8_t { No, Yes, WillDecideInNetworkProcess };
static BlockCookies shouldBlockCookies(WebFrame* frame, const URL& firstPartyForCookies, const URL& resourceURL)
{
    if (!WebCore::DeprecatedGlobalSettings::trackingPreventionEnabled())
        return BlockCookies::No;

    RegistrableDomain firstPartyDomain { firstPartyForCookies };
    if (firstPartyDomain.isEmpty())
        return BlockCookies::No;

    RegistrableDomain resourceDomain { resourceURL };
    if (resourceDomain.isEmpty())
        return BlockCookies::No;

    if (firstPartyDomain == resourceDomain)
        return BlockCookies::No;

    if (frame) {
        if (frame->localFrameLoaderClient()->hasFrameSpecificStorageAccess())
            return BlockCookies::No;
        if (RefPtr page = frame->page()) {
            if (page->hasPageLevelStorageAccess(firstPartyDomain, resourceDomain))
                return BlockCookies::No;
            if (auto* corePage = page->corePage()) {
                if (corePage->shouldRelaxThirdPartyCookieBlocking() == WebCore::ShouldRelaxThirdPartyCookieBlocking::Yes)
                    return BlockCookies::No;
            }
        }
    }

    // The WebContent process does not have enough information to deal with other policies than ThirdPartyCookieBlockingMode::All so we have to go to the NetworkProcess for all
    // other policies and the request may end up getting blocked on NetworkProcess side.
    if (WebProcess::singleton().thirdPartyCookieBlockingMode() != ThirdPartyCookieBlockingMode::All)
        return BlockCookies::WillDecideInNetworkProcess;

    return BlockCookies::Yes;
}

bool WebCookieJar::isEligibleForCache(WebFrame& frame, const URL& firstPartyForCookies, const URL& resourceURL) const
{
    RefPtr page = frame.page() ? frame.page()->corePage() : nullptr;
    if (!page)
        return false;

    if (!m_cache->isSupported())
        return false;

    // For now, we only cache cookies for first-party content. Third-party cookie caching is a bit more complicated due to partitioning and storage access.
    RegistrableDomain resourceDomain { resourceURL };
    if (resourceDomain.isEmpty())
        return false;

    return frame.isMainFrame() || RegistrableDomain { firstPartyForCookies } == resourceDomain;
}

static WebCore::ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking(const WebFrame* frame)
{
    if (!frame)
        return WebCore::ShouldRelaxThirdPartyCookieBlocking::No;
    RefPtr page = frame->page();
    if (!page)
        return WebCore::ShouldRelaxThirdPartyCookieBlocking::No;
    RefPtr corePage = page->corePage();
    if (!corePage)
        return WebCore::ShouldRelaxThirdPartyCookieBlocking::No;
    return corePage->shouldRelaxThirdPartyCookieBlocking();
}

String WebCookieJar::cookies(WebCore::Document& document, const URL& url) const
{
    RefPtr webFrame = document.frame() ? WebFrame::fromCoreFrame(*document.protectedFrame()) : nullptr;
    if (!webFrame)
        return { };

    RefPtr page = webFrame->page();
    if (!page)
        return { };

    auto sameSiteInfo = CookieJar::sameSiteInfo(document, IsForDOMCookieAccess::Yes);
    if (shouldBlockCookies(webFrame.get(), document.firstPartyForCookies(), url) == BlockCookies::Yes)
        return cookiesInPartitionedCookieStorage(document, url, sameSiteInfo);

    auto includeSecureCookies = CookieJar::shouldIncludeSecureCookies(document, url);
    auto frameID = webFrame->frameID();
    auto pageID = page->identifier();
    auto webPageProxyID = page->webPageProxyIdentifier();

    if (isEligibleForCache(*webFrame, document.firstPartyForCookies(), url))
        return m_cache->cookiesForDOM(document.firstPartyForCookies(), sameSiteInfo, url, frameID, pageID, webPageProxyID, includeSecureCookies);

    auto sendResult = WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::CookiesForDOM(document.firstPartyForCookies(), sameSiteInfo, url, frameID, pageID, includeSecureCookies, webPageProxyID), 0);
    auto [cookieString, secureCookiesAccessed] = sendResult.takeReplyOr(String { }, false);

    return cookieString;
}

void WebCookieJar::setCookies(WebCore::Document& document, const URL& url, const String& cookieString)
{
    RefPtr webFrame = document.frame() ? WebFrame::fromCoreFrame(*document.protectedFrame()) : nullptr;
    if (!webFrame)
        return;

    RefPtr page = webFrame->page();
    if (!page)
        return;

    auto sameSiteInfo = CookieJar::sameSiteInfo(document, IsForDOMCookieAccess::Yes);
    if (shouldBlockCookies(webFrame.get(), document.firstPartyForCookies(), url) == BlockCookies::Yes) {
        setCookiesInPartitionedCookieStorage(document, url, sameSiteInfo, cookieString);
        return;
    }

    auto frameID = webFrame->frameID();
    auto pageID = page->identifier();
    auto requiresPrivacyProtections = document.requiresScriptTrackingPrivacyProtection(ScriptTrackingPrivacyCategory::Cookies) ? RequiresScriptTrackingPrivacy::Yes : RequiresScriptTrackingPrivacy::No;

    if (isEligibleForCache(*webFrame, document.firstPartyForCookies(), url))
        m_cache->setCookiesFromDOM(document.firstPartyForCookies(), sameSiteInfo, url, frameID, pageID, cookieString, shouldRelaxThirdPartyCookieBlocking(webFrame.get()));

    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::SetCookiesFromDOM(document.firstPartyForCookies(), sameSiteInfo, url, frameID, pageID, cookieString, requiresPrivacyProtections, page->webPageProxyIdentifier()), 0);
}

void WebCookieJar::cookiesAdded(const String& host, Vector<WebCore::Cookie>&& cookies)
{
    auto it = m_changeListeners.find(host);
    if (it == m_changeListeners.end())
        return;

    it->value.forEach([&](auto& listener) {
        listener.cookiesAdded(host, cookies);
    });
}

void WebCookieJar::cookiesDeleted(const String& host, Vector<WebCore::Cookie>&& cookies)
{
    auto it = m_changeListeners.find(host);
    if (it == m_changeListeners.end())
        return;

    it->value.forEach([&](auto& listener) {
        listener.cookiesDeleted(host, cookies);
    });
}

void WebCookieJar::allCookiesDeleted()
{
    m_cache->allCookiesDeleted();
}

void WebCookieJar::clearCache()
{
    m_cache->clear();
}

void WebCookieJar::clearCacheForHost(const String& host)
{
    m_cache->clearForHost(host);
}

bool WebCookieJar::cookiesEnabled(Document& document)
{
    RefPtr webFrame = document.frame() ? WebFrame::fromCoreFrame(*document.protectedFrame()) : nullptr;
    if (!webFrame || !webFrame->page())
        return false;

    auto blockCookies = shouldBlockCookies(webFrame.get(), document.firstPartyForCookies(), document.cookieURL());
    if (shouldBlockCookies(webFrame.get(), document.firstPartyForCookies(), document.cookieURL()) == BlockCookies::Yes)
        return false;

    if (blockCookies != BlockCookies::WillDecideInNetworkProcess)
        return true;

    if (!document.cachedCookiesEnabled())
        document.setCachedCookiesEnabled(remoteCookiesEnabledSync(document));

    return *document.cachedCookiesEnabled();
}

bool WebCookieJar::remoteCookiesEnabledSync(Document& document) const
{
    RefPtr webFrame = document.frame() ? WebFrame::fromCoreFrame(*document.protectedFrame()) : nullptr;
    if (!webFrame)
        return false;

    RefPtr page = webFrame->page();
    if (!page)
        return false;

    auto cookieURL = document.cookieURL();
    if (cookieURL.isEmpty())
        return false;

    std::optional<FrameIdentifier> frameID = webFrame ? std::make_optional(webFrame->frameID()) : std::nullopt;
    auto sendResult = WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::CookiesEnabledSync(document.firstPartyForCookies(), cookieURL, frameID, page->identifier(), page->webPageProxyIdentifier()), 0);

    auto [result] = sendResult.takeReplyOr(false);
    return result;
}

void WebCookieJar::remoteCookiesEnabled(const Document& document, CompletionHandler<void(bool)>&& completionHandler) const
{
    RefPtr webFrame = document.frame() ? WebFrame::fromCoreFrame(*document.protectedFrame()) : nullptr;
    if (!webFrame)
        return completionHandler(false);

    RefPtr page = webFrame->page();
    if (!page)
        return completionHandler(false);

    auto cookieURL = document.cookieURL();
    if (cookieURL.isEmpty())
        return completionHandler(false);

    std::optional<FrameIdentifier> frameID = webFrame ? std::make_optional(webFrame->frameID()) : std::nullopt;
    WebProcess::singleton().ensureNetworkProcessConnection().connection().sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::CookiesEnabled(document.firstPartyForCookies(), cookieURL, frameID, page->identifier(), page->webPageProxyIdentifier()), WTFMove(completionHandler));
}

std::pair<String, WebCore::SecureCookiesAccessed> WebCookieJar::cookieRequestHeaderFieldValue(const URL& firstParty, const WebCore::SameSiteInfo& sameSiteInfo, const URL& url, std::optional<FrameIdentifier> frameID, std::optional<PageIdentifier> pageID, WebCore::IncludeSecureCookies includeSecureCookies) const
{
    RefPtr webFrame = frameID ? WebProcess::singleton().webFrame(*frameID) : nullptr;
    if (shouldBlockCookies(webFrame.get(), firstParty, url) == BlockCookies::Yes)
        return { };

    auto webPageProxyID = webFrame && webFrame->page() ? std::make_optional(webFrame->page()->webPageProxyIdentifier()) : std::nullopt;
    auto sendResult = WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::CookieRequestHeaderFieldValue(firstParty, sameSiteInfo, url, frameID, pageID, includeSecureCookies, webPageProxyID), 0);
    if (!sendResult.succeeded())
        return { };

    auto [cookieString, secureCookiesAccessed] = sendResult.takeReply();
    return { cookieString, secureCookiesAccessed ? WebCore::SecureCookiesAccessed::Yes : WebCore::SecureCookiesAccessed::No };
}

bool WebCookieJar::getRawCookies(WebCore::Document& document, const URL& url, Vector<WebCore::Cookie>& rawCookies) const
{
    RefPtr webFrame = document.frame() ? WebFrame::fromCoreFrame(*document.protectedFrame()) : nullptr;
    if (shouldBlockCookies(webFrame.get(), document.firstPartyForCookies(), url) == BlockCookies::Yes)
        return false;

    auto frameID = webFrame ? std::make_optional(webFrame->frameID()) : std::nullopt;
    auto pageID = webFrame && webFrame->page() ? std::make_optional(webFrame->page()->identifier()) : std::nullopt;
    auto webPageProxyID = webFrame && webFrame->page() ? std::make_optional(webFrame->page()->webPageProxyIdentifier()) : std::nullopt;
    auto sendResult = WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::GetRawCookies(document.firstPartyForCookies(), sameSiteInfo(document), url, frameID, pageID, webPageProxyID), 0);
    if (!sendResult.succeeded())
        return false;

    std::tie(rawCookies) = sendResult.takeReply();
    return true;
}

void WebCookieJar::setRawCookie(const WebCore::Document& document, const Cookie& cookie, ShouldPartitionCookie shouldPartitionCookie)
{
    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::SetRawCookie(document.firstPartyForCookies(), document.cookieURL(), cookie, shouldPartitionCookie), 0);
}

void WebCookieJar::deleteCookie(const WebCore::Document& document, const URL& url, const String& cookieName, CompletionHandler<void()>&& completionHandler)
{
    WebProcess::singleton().ensureNetworkProcessConnection().connection().sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::DeleteCookie(document.firstPartyForCookies(), url, cookieName), WTFMove(completionHandler));
}

void WebCookieJar::getCookiesAsync(WebCore::Document& document, const URL& url, const WebCore::CookieStoreGetOptions& options, CompletionHandler<void(std::optional<Vector<WebCore::Cookie>>&&)>&& completionHandler) const
{
    RefPtr frame = document.frame();
    if (!frame) {
        completionHandler({ });
        return;
    }

    RefPtr webFrame = WebFrame::fromCoreFrame(*frame);

    if (shouldBlockCookies(webFrame.get(), document.firstPartyForCookies(), url) == BlockCookies::Yes) {
        completionHandler({ });
        return;
    }

    auto sameSiteInfo = CookieJar::sameSiteInfo(document, IsForDOMCookieAccess::Yes);
    auto includeSecureCookies = CookieJar::shouldIncludeSecureCookies(document, url);
    auto frameID = webFrame ? std::make_optional(webFrame->frameID()) : std::nullopt;
    auto pageID = webFrame && webFrame->page() ? std::make_optional(webFrame->page()->identifier()) : std::nullopt;
    auto webPageProxyID = webFrame && webFrame->page() ? std::make_optional(webFrame->page()->webPageProxyIdentifier()) : std::nullopt;
    WebProcess::singleton().ensureNetworkProcessConnection().connection().sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::CookiesForDOMAsync(document.firstPartyForCookies(), sameSiteInfo, url, frameID, pageID, includeSecureCookies, options, webPageProxyID), WTFMove(completionHandler));
}

void WebCookieJar::setCookieAsync(WebCore::Document& document, const URL& url, const WebCore::Cookie& cookie, CompletionHandler<void(bool)>&& completionHandler) const
{
    RefPtr frame = document.frame();
    if (!frame) {
        completionHandler(false);
        return;
    }

    RefPtr webFrame = WebFrame::fromCoreFrame(*frame);

    if (shouldBlockCookies(webFrame.get(), document.firstPartyForCookies(), url) == BlockCookies::Yes) {
        completionHandler(false);
        return;
    }

    auto requiresPrivacyProtections = document.requiresScriptTrackingPrivacyProtection(ScriptTrackingPrivacyCategory::Cookies) ? RequiresScriptTrackingPrivacy::Yes : RequiresScriptTrackingPrivacy::No;
    auto sameSiteInfo = CookieJar::sameSiteInfo(document, IsForDOMCookieAccess::Yes);
    auto frameID = webFrame ? std::make_optional(webFrame->frameID()) : std::nullopt;
    auto pageID = webFrame && webFrame->page() ? std::make_optional(webFrame->page()->identifier()) : std::nullopt;
    auto webPageProxyIdentifier = webFrame && webFrame->page() ? std::make_optional(webFrame->page()->webPageProxyIdentifier()) : std::nullopt;

    PendingCookieUpdateCounter::Token pendingCookieUpdate;
    bool shouldUpdateCookieCache = webFrame && isEligibleForCache(*webFrame, document.firstPartyForCookies(), url);
    if (shouldUpdateCookieCache)
        pendingCookieUpdate = m_cache->willSetCookieFromDOM();

    auto completionHandlerWrapper = [protectedThis = Ref { *this }, pendingCookieUpdate = WTFMove(pendingCookieUpdate), shouldUpdateCookieCache, document = Ref { document }, webFrame, sameSiteInfo, url, frameID, pageID, cookie, completionHandler = WTFMove(completionHandler)](bool success) mutable {
        if (success) {
            if (shouldUpdateCookieCache)
                protectedThis->m_cache->didSetCookieFromDOM(WTFMove(pendingCookieUpdate), document->firstPartyForCookies(), sameSiteInfo, url, *frameID, *pageID, cookie, shouldRelaxThirdPartyCookieBlocking(webFrame.get()));
        }
        pendingCookieUpdate = nullptr;
        completionHandler(success);
    };

    WebProcess::singleton().ensureNetworkProcessConnection().connection().sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::SetCookieFromDOMAsync(document.firstPartyForCookies(), sameSiteInfo, url, frameID, pageID, cookie, requiresPrivacyProtections, webPageProxyIdentifier), WTFMove(completionHandlerWrapper));
}

#if HAVE(COOKIE_CHANGE_LISTENER_API)
void WebCookieJar::addChangeListenerWithAccess(const URL& url, const URL& firstParty, WebCore::FrameIdentifier frameID, WebCore::PageIdentifier pageID, WebPageProxyIdentifier webPageProxyID, const WebCore::CookieChangeListener& listener)
{
    auto host = url.host().toString();

    if (auto iter = m_changeListeners.find(host); iter != m_changeListeners.end()) {
        if (iter->value.contains(listener))
            return;
    }

    auto completionHandler = [protectedThis = Ref { *this }, this, host, listener = WeakPtr { listener }] (bool listenerAdded)  {
        if (!listenerAdded)
            return;

        if (!listener)
            return;

        auto& listenersForHost = m_changeListeners.add(host, WeakHashSet<CookieChangeListener> { }).iterator->value;
        listenersForHost.add(*listener);
    };

    WebProcess::singleton().ensureNetworkProcessConnection().connection().sendWithAsyncReply(Messages::NetworkConnectionToWebProcess::SubscribeToCookieChangeNotifications(url, firstParty, frameID, pageID, webPageProxyID), WTFMove(completionHandler), 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
}

void WebCookieJar::addChangeListener(const WebCore::Document& document, const WebCore::CookieChangeListener& listener)
{
    RefPtr webFrame = document.frame() ? WebFrame::fromCoreFrame(*document.protectedFrame()) : nullptr;
    if (!webFrame)
        return;

    RefPtr page = webFrame->page();
    if (!page)
        return;

    if (shouldBlockCookies(webFrame.get(), document.firstPartyForCookies(), document.cookieURL()) == BlockCookies::Yes)
        return;

    addChangeListenerWithAccess(document.url(), document.firstPartyForCookies(), webFrame->frameID(), page->identifier(), page->webPageProxyIdentifier(), listener);
}

void WebCookieJar::removeChangeListener(const String& host, const WebCore::CookieChangeListener& listener)
{
    auto it = m_changeListeners.find(host);
    if (it == m_changeListeners.end())
        return;

    it->value.remove(listener);
    if (!it->value.isEmptyIgnoringNullReferences())
        return;

    m_changeListeners.remove(it);
    WebProcess::singleton().ensureNetworkProcessConnection().connection().send(Messages::NetworkConnectionToWebProcess::UnsubscribeFromCookieChangeNotifications(host), 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
}
#endif

#if HAVE(ALLOW_ONLY_PARTITIONED_COOKIES)
void WebCookieJar::setOptInCookiePartitioningEnabled(bool enabled)
{
    m_cache->setOptInCookiePartitioningEnabled(enabled);
}
#endif

#if !PLATFORM(COCOA)

String WebCookieJar::cookiesInPartitionedCookieStorage(const WebCore::Document&, const URL&, const WebCore::SameSiteInfo&) const
{
    return { };
}

void WebCookieJar::setCookiesInPartitionedCookieStorage(const WebCore::Document&, const URL&, const WebCore::SameSiteInfo&, const String&)
{
}

#endif // !PLATFORM(COCOA)

} // namespace WebKit
