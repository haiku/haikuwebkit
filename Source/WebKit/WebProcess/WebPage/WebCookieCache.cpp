/*
 * Copyright (C) 2020-2025 Apple Inc. All rights reserved.
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
#include "WebCookieCache.h"

#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "WebCookieJar.h"
#include "WebProcess.h"
#include <WebCore/Cookie.h>
#include <wtf/text/StringBuilder.h>

namespace WebKit {

using namespace WebCore;

WebCookieCache::~WebCookieCache() = default;

bool WebCookieCache::isSupported()
{
#if HAVE(COOKIE_CHANGE_LISTENER_API)
    return true;
#else
    return false;
#endif
}

static String cookiesToString(const Vector<WebCore::Cookie>& cookies)
{
    StringBuilder cookiesBuilder;
    for (auto& cookie : cookies) {
        if (cookie.name.isEmpty())
            continue;
        ASSERT(!cookie.httpOnly);
        if (cookie.httpOnly)
            continue;
        if (!cookiesBuilder.isEmpty())
            cookiesBuilder.append("; "_s);
        cookiesBuilder.append(cookie.name);
        cookiesBuilder.append('=');
        cookiesBuilder.append(cookie.value);
    }
    return cookiesBuilder.toString();
}

String WebCookieCache::cookiesForDOM(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, FrameIdentifier frameID, PageIdentifier pageID, WebPageProxyIdentifier webPageProxyID, IncludeSecureCookies includeSecureCookies)
{
    bool hasCacheForHost = m_hostsWithInMemoryStorage.contains<StringViewHashTranslator>(url.host());
    if (!hasCacheForHost || cacheMayBeOutOfSync()) {
        auto host = url.host().toString();
#if HAVE(COOKIE_CHANGE_LISTENER_API)
        if (!hasCacheForHost)
            WebProcess::singleton().protectedCookieJar()->addChangeListenerWithAccess(url, firstParty, frameID, pageID, webPageProxyID, *this);
#endif
        auto sendResult = WebProcess::singleton().ensureNetworkProcessConnection().connection().sendSync(Messages::NetworkConnectionToWebProcess::DomCookiesForHost(url), 0);
        if (!sendResult.succeeded())
            return { };

        auto& [cookies] = sendResult.reply();

        if (hasCacheForHost)
            return cookiesToString(cookies);

        pruneCacheIfNecessary();
        m_hostsWithInMemoryStorage.add(WTFMove(host));

        CheckedRef inMemoryStorageSession = this->inMemoryStorageSession();
        for (auto& cookie : cookies)
            inMemoryStorageSession->setCookie(cookie);
    }
    return checkedInMemoryStorageSession()->cookiesForDOM(firstParty, sameSiteInfo, url, frameID, pageID, includeSecureCookies, ApplyTrackingPrevention::No, ShouldRelaxThirdPartyCookieBlocking::No).first;
}

void WebCookieCache::setCookiesFromDOM(const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, FrameIdentifier frameID, PageIdentifier pageID, const String& cookieString, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking)
{
    if (m_hostsWithInMemoryStorage.contains<StringViewHashTranslator>(url.host()))
        checkedInMemoryStorageSession()->setCookiesFromDOM(firstParty, sameSiteInfo, url, frameID, pageID, ApplyTrackingPrevention::No, RequiresScriptTrackingPrivacy::No, cookieString, shouldRelaxThirdPartyCookieBlocking);
}

PendingCookieUpdateCounter::Token WebCookieCache::willSetCookieFromDOM()
{
    return m_pendingCookieUpdateCounter.count();
}

void WebCookieCache::didSetCookieFromDOM(PendingCookieUpdateCounter::Token, const URL& firstParty, const SameSiteInfo& sameSiteInfo, const URL& url, FrameIdentifier frameID, PageIdentifier pageID, const WebCore::Cookie& cookie, ShouldRelaxThirdPartyCookieBlocking shouldRelaxThirdPartyCookieBlocking)
{
    if (m_hostsWithInMemoryStorage.contains<StringViewHashTranslator>(url.host()))
        checkedInMemoryStorageSession()->setCookieFromDOM(firstParty, sameSiteInfo, url, frameID, pageID, ApplyTrackingPrevention::No, RequiresScriptTrackingPrivacy::No, cookie, shouldRelaxThirdPartyCookieBlocking);
}

void WebCookieCache::cookiesAdded(const String& host, const Vector<WebCore::Cookie>& cookies)
{
    if (!m_hostsWithInMemoryStorage.contains(host))
        return;

    CheckedRef inMemoryStorageSession = this->inMemoryStorageSession();
    for (auto& cookie : cookies)
        inMemoryStorageSession->setCookie(cookie);
}

void WebCookieCache::cookiesDeleted(const String& host, const Vector<WebCore::Cookie>& cookies)
{
    if (!m_hostsWithInMemoryStorage.contains(host))
        return;

    CheckedRef inMemoryStorageSession = this->inMemoryStorageSession();
    for (auto& cookie : cookies)
        inMemoryStorageSession->deleteCookie(cookie, [] { });
}

void WebCookieCache::allCookiesDeleted()
{
    clear();
}

void WebCookieCache::clear()
{
#if HAVE(COOKIE_CHANGE_LISTENER_API)
    for (auto& host : m_hostsWithInMemoryStorage)
        WebProcess::singleton().protectedCookieJar()->removeChangeListener(host, *this);
#endif
    m_hostsWithInMemoryStorage.clear();
    m_inMemoryStorageSession = nullptr;
}

void WebCookieCache::clearForHost(const String& host)
{
    String removedHost = m_hostsWithInMemoryStorage.take(host);
    if (removedHost.isNull())
        return;

    checkedInMemoryStorageSession()->deleteCookiesForHostnames(Vector<String> { removedHost }, [] { });
#if HAVE(COOKIE_CHANGE_LISTENER_API)
    WebProcess::singleton().protectedCookieJar()->removeChangeListener(removedHost, *this);
#endif
}

void WebCookieCache::pruneCacheIfNecessary()
{
    // We may want to raise this limit if we start using the cache for third-party iframes.
    static const unsigned maxCachedHosts = 5;

    while (m_hostsWithInMemoryStorage.size() >= maxCachedHosts)
        clearForHost(*m_hostsWithInMemoryStorage.random());
}

#if !PLATFORM(COCOA) && !USE(SOUP)
NetworkStorageSession& WebCookieCache::inMemoryStorageSession()
{
    ASSERT_NOT_IMPLEMENTED_YET();
    return *m_inMemoryStorageSession;
}

#if HAVE(ALLOW_ONLY_PARTITIONED_COOKIES)
void WebCookieCache::setOptInCookiePartitioningEnabled(bool)
{
    ASSERT_NOT_IMPLEMENTED_YET();
}
#endif
#endif

bool WebCookieCache::cacheMayBeOutOfSync() const
{
    return m_pendingCookieUpdateCounter.value() > 0;
}

} // namespace WebKit
