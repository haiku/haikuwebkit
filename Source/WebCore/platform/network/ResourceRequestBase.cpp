/*
 * Copyright (C) 2003-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2009, 2012 Google Inc. All rights reserved.
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
#include "config.h"
#include "ResourceRequestBase.h"

#include "HTTPHeaderNames.h"
#include "HTTPStatusCodes.h"
#include "Logging.h"
#include "PublicSuffixStore.h"
#include "RegistrableDomain.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SecurityOrigin.h"
#include "SecurityPolicy.h"
#include <wtf/PointerComparison.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ResourceRequestBase);

#if PLATFORM(IOS_FAMILY)
double ResourceRequestBase::s_defaultTimeoutInterval = INT_MAX;
#else
// Will use NSURLRequest default timeout unless set to a non-zero value with setDefaultTimeoutInterval().
// For libsoup the timeout enabled with integer milliseconds. We set 0 as the default value to avoid integer overflow.
double ResourceRequestBase::s_defaultTimeoutInterval = 0;
#endif

inline const ResourceRequest& ResourceRequestBase::asResourceRequest() const
{
    return *static_cast<const ResourceRequest*>(this);
}

ResourceRequest ResourceRequestBase::isolatedCopy() const
{
    ResourceRequest request;
    request.setAsIsolatedCopy(asResourceRequest());
    return request;
}

void ResourceRequestBase::setAsIsolatedCopy(const ResourceRequest& other)
{
    setURL(other.url().isolatedCopy());
    setCachePolicy(other.cachePolicy());
    setTimeoutInterval(other.timeoutInterval());
    setFirstPartyForCookies(other.firstPartyForCookies().isolatedCopy());
    setHTTPMethod(other.httpMethod().isolatedCopy());
    setPriority(other.priority());
    setRequester(other.requester());
    setInitiatorIdentifier(other.initiatorIdentifier().isolatedCopy());
    setCachePartition(other.cachePartition().isolatedCopy());

    if (auto inspectorInitiatorNodeIdentifier = other.inspectorInitiatorNodeIdentifier())
        setInspectorInitiatorNodeIdentifier(*inspectorInitiatorNodeIdentifier);

    if (!other.isSameSiteUnspecified())
        setIsSameSite(other.isSameSite());
    setIsTopSite(other.isTopSite());

    updateResourceRequest();
    m_requestData.m_httpHeaderFields = other.httpHeaderFields().isolatedCopy();

    size_t encodingCount = other.m_requestData.m_responseContentDispositionEncodingFallbackArray.size();
    if (encodingCount > 0) {
        String encoding1 = other.m_requestData.m_responseContentDispositionEncodingFallbackArray[0].isolatedCopy();
        String encoding2;
        String encoding3;
        if (encodingCount > 1) {
            encoding2 = other.m_requestData.m_responseContentDispositionEncodingFallbackArray[1].isolatedCopy();
            if (encodingCount > 2)
                encoding3 = other.m_requestData.m_responseContentDispositionEncodingFallbackArray[2].isolatedCopy();
        }
        ASSERT(encodingCount <= 3);
        setResponseContentDispositionEncodingFallbackArray(encoding1, encoding2, encoding3);
    }
    if (RefPtr httpBody = other.m_httpBody)
        setHTTPBody(httpBody->isolatedCopy());
    setAllowCookies(other.m_requestData.m_allowCookies);
    setIsAppInitiated(other.isAppInitiated());
    setPrivacyProxyFailClosedForUnreachableNonMainHosts(other.privacyProxyFailClosedForUnreachableNonMainHosts());
    setUseAdvancedPrivacyProtections(other.useAdvancedPrivacyProtections());
    setDidFilterLinkDecoration(other.didFilterLinkDecoration());
    setIsPrivateTokenUsageByThirdPartyAllowed(other.isPrivateTokenUsageByThirdPartyAllowed());
}

bool ResourceRequestBase::isEmpty() const
{
    updateResourceRequest(); 
    
    return url().isEmpty();
}

bool ResourceRequestBase::isNull() const
{
    updateResourceRequest(); 
    
    return url().isNull();
}

const URL& ResourceRequestBase::url() const 
{
    updateResourceRequest(); 
    
    return m_requestData.m_url;
}

void ResourceRequestBase::setURL(URL&& url, bool didFilterLinkDecoration)
{
    updateResourceRequest();

    m_requestData.m_url = WTFMove(url);
    m_requestData.m_didFilterLinkDecoration = didFilterLinkDecoration;

    m_platformRequestUpdated = false;
}

static bool shouldUseGet(const ResourceRequestBase& request, const ResourceResponse& redirectResponse)
{
    if (equalLettersIgnoringASCIICase(request.httpMethod(), "get"_s) || equalLettersIgnoringASCIICase(request.httpMethod(), "head"_s))
        return false;
    if (redirectResponse.httpStatusCode() == httpStatus301MovedPermanently || redirectResponse.httpStatusCode() == httpStatus302Found)
        return equalLettersIgnoringASCIICase(request.httpMethod(), "post"_s);
    return redirectResponse.httpStatusCode() == httpStatus303SeeOther;
}

// https://fetch.spec.whatwg.org/#concept-http-redirect-fetch Step 11
void ResourceRequestBase::redirectAsGETIfNeeded(const ResourceRequestBase &redirectRequest, const ResourceResponse& redirectResponse)
{
    if (shouldUseGet(redirectRequest, redirectResponse)) {
        setHTTPMethod("GET"_s);
        setHTTPBody(nullptr);
        m_requestData.m_httpHeaderFields.remove(HTTPHeaderName::ContentLength);
        m_requestData.m_httpHeaderFields.remove(HTTPHeaderName::ContentLanguage);
        m_requestData.m_httpHeaderFields.remove(HTTPHeaderName::ContentEncoding);
        m_requestData.m_httpHeaderFields.remove(HTTPHeaderName::ContentLocation);
        clearHTTPContentType();
    }
}

ResourceRequest ResourceRequestBase::redirectedRequest(const ResourceResponse& redirectResponse, bool shouldClearReferrerOnHTTPSToHTTPRedirect, ShouldSetHash shouldSetHash) const
{
    ASSERT(redirectResponse.isRedirection());
    // This method is based on https://fetch.spec.whatwg.org/#http-redirect-fetch.
    // It also implements additional processing like done by CFNetwork layer.

    auto request = asResourceRequest();
    auto location = redirectResponse.httpHeaderField(HTTPHeaderName::Location);

    // https://fetch.spec.whatwg.org/#concept-response-location-url
    auto url = location.isEmpty() ? URL { } : URL { redirectResponse.url(), location };
    if (shouldSetHash == ShouldSetHash::Yes && url.fragmentIdentifier().isEmpty() && !redirectResponse.url().fragmentIdentifier().isEmpty())
        url.setFragmentIdentifier(redirectResponse.url().fragmentIdentifier());

    request.setURL(WTFMove(url));

    request.redirectAsGETIfNeeded(*this, redirectResponse);

    if (shouldClearReferrerOnHTTPSToHTTPRedirect && !request.url().protocolIs("https"_s) && WTF::protocolIs(request.httpReferrer(), "https"_s))
        request.clearHTTPReferrer();

    if (!protocolHostAndPortAreEqual(request.url(), redirectResponse.url()))
        request.clearHTTPOrigin();
    request.clearHTTPAuthorization();
    request.m_requestData.m_httpHeaderFields.remove(HTTPHeaderName::ProxyAuthorization);

    return request;
}

bool ResourceRequestBase::upgradeInsecureRequest(URL& url)
{
    if (!url.protocolIs("http"_s) && !url.protocolIs("ws"_s))
        return false;

    if (url.protocolIs("http"_s))
        url.setProtocol("https"_s);
    else {
        ASSERT(url.protocolIs("ws"_s));
        url.setProtocol("wss"_s);
    }

    if (url.port() == 80)
        url.setPort(std::nullopt);

    return true;
}

bool ResourceRequestBase::upgradeInsecureRequestIfNeeded(URL& url, ShouldUpgradeLocalhostAndIPAddress shouldUpgradeLocalhostAndIPAddress, const std::optional<uint16_t>& upgradePort)
{
    if (!url.protocolIs("http"_s) && !url.protocolIs("ws"_s))
        return false;

    // Do not automatically upgrade localhost or IP address connections unless the CSP policy requires it.
    bool isHostLocalhostOrIPaddress = SecurityOrigin::isLocalhostAddress(url.host()) || URL::hostIsIPAddress(url.host());
    if (isHostLocalhostOrIPaddress && shouldUpgradeLocalhostAndIPAddress == ShouldUpgradeLocalhostAndIPAddress::No)
        return false;

    if (!upgradeInsecureRequest(url))
        return false;

    if (url.port() && upgradePort)
        url.setPort(*upgradePort);

    return true;
}

void ResourceRequestBase::upgradeInsecureRequestIfNeeded(ShouldUpgradeLocalhostAndIPAddress shouldUpgradeLocalhostAndIPAddress, const std::optional<uint16_t>& upgradePort)
{
    bool wasUpgraded = upgradeInsecureRequestIfNeeded(m_requestData.m_url, shouldUpgradeLocalhostAndIPAddress, upgradePort);
    setWasSchemeOptimisticallyUpgraded(wasUpgraded);
}

void ResourceRequestBase::upgradeInsecureRequest()
{
    upgradeInsecureRequest(m_requestData.m_url);
}

void ResourceRequestBase::removeCredentials()
{
    updateResourceRequest(); 

    if (!m_requestData.m_url.hasCredentials())
        return;

    m_requestData.m_url.removeCredentials();
    m_platformRequestUpdated = false;
}

ResourceRequestCachePolicy ResourceRequestBase::cachePolicy() const
{
    updateResourceRequest(); 
    
    return m_requestData.m_cachePolicy;
}

void ResourceRequestBase::setCachePolicy(ResourceRequestCachePolicy cachePolicy)
{
    updateResourceRequest(); 

    if (m_requestData.m_cachePolicy == cachePolicy)
        return;
    
    m_requestData.m_cachePolicy = cachePolicy;
    
    m_platformRequestUpdated = false;
}

double ResourceRequestBase::timeoutInterval() const
{
    updateResourceRequest(); 
    
    return m_requestData.m_timeoutInterval;
}

void ResourceRequestBase::setTimeoutInterval(double timeoutInterval) 
{
    updateResourceRequest(); 
    
    if (m_requestData.m_timeoutInterval == timeoutInterval)
        return;

    m_requestData.m_timeoutInterval = timeoutInterval;
    
    m_platformRequestUpdated = false;
}

void ResourceRequestBase::resetTimeoutInterval()
{
    setTimeoutInterval(s_defaultTimeoutInterval);
}

const URL& ResourceRequestBase::firstPartyForCookies() const
{
    updateResourceRequest(); 
    
    return m_requestData.m_firstPartyForCookies;
}

void ResourceRequestBase::setFirstPartyForCookies(const URL& firstPartyForCookies)
{ 
    updateResourceRequest(); 

    if (m_requestData.m_firstPartyForCookies == firstPartyForCookies)
        return;

    m_requestData.m_firstPartyForCookies = firstPartyForCookies;
    
    m_platformRequestUpdated = false;
}

bool ResourceRequestBase::isSameSite() const
{
    updateResourceRequest();

    return m_requestData.m_sameSiteDisposition == SameSiteDisposition::SameSite;
}

void ResourceRequestBase::setIsSameSite(bool isSameSite)
{
    updateResourceRequest();

    SameSiteDisposition newDisposition = isSameSite ? SameSiteDisposition::SameSite : SameSiteDisposition::CrossSite;
    if (m_requestData.m_sameSiteDisposition == newDisposition)
        return;

    m_requestData.m_sameSiteDisposition = newDisposition;

    m_platformRequestUpdated = false;
}

bool ResourceRequestBase::isTopSite() const
{
    updateResourceRequest();

    return m_requestData.m_isTopSite;
}

void ResourceRequestBase::setIsTopSite(bool isTopSite)
{
    updateResourceRequest();

    if (m_requestData.m_isTopSite == isTopSite)
        return;

    m_requestData.m_isTopSite = isTopSite;

    m_platformRequestUpdated = false;
}

const String& ResourceRequestBase::httpMethod() const
{
    updateResourceRequest(); 
    
    return m_requestData.m_httpMethod;
}

void ResourceRequestBase::setHTTPMethod(const String& httpMethod) 
{
    updateResourceRequest(); 

    if (m_requestData.m_httpMethod == httpMethod)
        return;

    m_requestData.m_httpMethod = httpMethod;
    
    m_platformRequestUpdated = false;
}

const HTTPHeaderMap& ResourceRequestBase::httpHeaderFields() const
{
    updateResourceRequest(); 

    return m_requestData.m_httpHeaderFields;
}

String ResourceRequestBase::httpHeaderField(StringView name) const
{
    updateResourceRequest();

    return m_requestData.m_httpHeaderFields.get(name);
}

String ResourceRequestBase::httpHeaderField(HTTPHeaderName name) const
{
    updateResourceRequest(); 
    
    return m_requestData.m_httpHeaderFields.get(name);
}

void ResourceRequestBase::setHTTPHeaderField(const String& name, const String& value)
{
    updateResourceRequest();

    m_requestData.m_httpHeaderFields.set(name, value);
    
    m_platformRequestUpdated = false;
}

void ResourceRequestBase::setHTTPHeaderField(HTTPHeaderName name, const String& value)
{
    updateResourceRequest();

    m_requestData.m_httpHeaderFields.set(name, value);

    m_platformRequestUpdated = false;
}

void ResourceRequestBase::clearHTTPAuthorization()
{
    updateResourceRequest(); 

    if (!m_requestData.m_httpHeaderFields.remove(HTTPHeaderName::Authorization))
        return;

    m_platformRequestUpdated = false;
}

String ResourceRequestBase::httpContentType() const
{
    return httpHeaderField(HTTPHeaderName::ContentType);
}

void ResourceRequestBase::setHTTPContentType(const String& httpContentType)
{
    setHTTPHeaderField(HTTPHeaderName::ContentType, httpContentType);
}

void ResourceRequestBase::clearHTTPContentType()
{
    updateResourceRequest(); 

    m_requestData.m_httpHeaderFields.remove(HTTPHeaderName::ContentType);

    m_platformRequestUpdated = false;
}

void ResourceRequestBase::clearPurpose()
{
    updateResourceRequest();

    m_requestData.m_httpHeaderFields.remove(HTTPHeaderName::SecPurpose);

    m_platformRequestUpdated = false;
}

String ResourceRequestBase::httpReferrer() const
{
    return httpHeaderField(HTTPHeaderName::Referer);
}

bool ResourceRequestBase::hasHTTPReferrer() const
{
    return m_requestData.m_httpHeaderFields.contains(HTTPHeaderName::Referer);
}

void ResourceRequestBase::setHTTPReferrer(const String& httpReferrer)
{
    // https://w3c.github.io/webappsec-referrer-policy/#determine-requests-referrer
    constexpr size_t maxLength = 4096;
    if (httpReferrer.length() > maxLength) {
        RELEASE_LOG(Loading, "Truncating HTTP referer");
        String origin = URL(SecurityOrigin::create(URL { httpReferrer })->toString()).string();
        if (origin.length() <= maxLength)
            setHTTPHeaderField(HTTPHeaderName::Referer, origin);
    } else
        setHTTPHeaderField(HTTPHeaderName::Referer, httpReferrer);
}

void ResourceRequestBase::setExistingHTTPReferrerToOriginString()
{
    if (!hasHTTPReferrer())
        return;

    setHTTPHeaderField(HTTPHeaderName::Referer, SecurityPolicy::referrerToOriginString(URL { httpReferrer() }));
}
    
void ResourceRequestBase::clearHTTPReferrer()
{
    updateResourceRequest(); 

    m_requestData.m_httpHeaderFields.remove(HTTPHeaderName::Referer);

    m_platformRequestUpdated = false;
}

String ResourceRequestBase::httpOrigin() const
{
    return httpHeaderField(HTTPHeaderName::Origin);
}

void ResourceRequestBase::setHTTPOrigin(const String& httpOrigin)
{
    setHTTPHeaderField(HTTPHeaderName::Origin, httpOrigin);
}

bool ResourceRequestBase::hasHTTPOrigin() const
{
    return m_requestData.m_httpHeaderFields.contains(HTTPHeaderName::Origin);
}

void ResourceRequestBase::clearHTTPOrigin()
{
    updateResourceRequest();

    m_requestData.m_httpHeaderFields.remove(HTTPHeaderName::Origin);

    m_platformRequestUpdated = false;
}

bool ResourceRequestBase::hasHTTPHeader(HTTPHeaderName name) const
{
    return m_requestData.m_httpHeaderFields.contains(name);
}

String ResourceRequestBase::httpUserAgent() const
{
    return httpHeaderField(HTTPHeaderName::UserAgent);
}

void ResourceRequestBase::setHTTPUserAgent(const String& httpUserAgent)
{
    setHTTPHeaderField(HTTPHeaderName::UserAgent, httpUserAgent);
}

void ResourceRequestBase::clearHTTPUserAgent()
{
    updateResourceRequest(); 

    m_requestData.m_httpHeaderFields.remove(HTTPHeaderName::UserAgent);

    m_platformRequestUpdated = false;
}

void ResourceRequestBase::clearHTTPAcceptEncoding()
{
    updateResourceRequest();

    m_requestData.m_httpHeaderFields.remove(HTTPHeaderName::AcceptEncoding);

    m_platformRequestUpdated = false;
}

void ResourceRequestBase::setResponseContentDispositionEncodingFallbackArray(const String& encoding1, const String& encoding2, const String& encoding3)
{
    updateResourceRequest(); 
    
    m_requestData.m_responseContentDispositionEncodingFallbackArray.clear();
    m_requestData.m_responseContentDispositionEncodingFallbackArray.reserveInitialCapacity(!encoding1.isNull() + !encoding2.isNull() + !encoding3.isNull());
    if (!encoding1.isNull())
        m_requestData.m_responseContentDispositionEncodingFallbackArray.append(encoding1);
    if (!encoding2.isNull())
        m_requestData.m_responseContentDispositionEncodingFallbackArray.append(encoding2);
    if (!encoding3.isNull())
        m_requestData.m_responseContentDispositionEncodingFallbackArray.append(encoding3);
    
    m_platformRequestUpdated = false;
}

RefPtr<FormData> ResourceRequestBase::httpBody() const
{
    updateResourceRequest(HTTPBodyUpdatePolicy::UpdateHTTPBody);

    return m_httpBody;
}

bool ResourceRequestBase::hasUpload() const
{
    if (auto body = httpBody()) {
        for (auto& element : body->elements()) {
            if (std::holds_alternative<WebCore::FormDataElement::EncodedFileData>(element.data) || std::holds_alternative<WebCore::FormDataElement::EncodedBlobData>(element.data))
                return true;
        }
    }
    
    return false;
}

void ResourceRequestBase::setHTTPBody(RefPtr<FormData>&& httpBody)
{
    updateResourceRequest();

    m_httpBody = WTFMove(httpBody);

    m_resourceRequestBodyUpdated = true;

    m_platformRequestBodyUpdated = false;
}

bool ResourceRequestBase::allowCookies() const
{
    updateResourceRequest(); 
    
    return m_requestData.m_allowCookies;
}

void ResourceRequestBase::setAllowCookies(bool allowCookies)
{
    updateResourceRequest(); 

    if (m_requestData.m_allowCookies == allowCookies)
        return;

    m_requestData.m_allowCookies = allowCookies;
    
    m_platformRequestUpdated = false;
}

ResourceLoadPriority ResourceRequestBase::priority() const
{
    updateResourceRequest();

    return m_requestData.m_priority;
}

void ResourceRequestBase::setPriority(ResourceLoadPriority priority)
{
    updateResourceRequest();

    if (m_requestData.m_priority == priority)
        return;

    m_requestData.m_priority = priority;

    m_platformRequestUpdated = false;
}

void ResourceRequestBase::addHTTPHeaderFieldIfNotPresent(HTTPHeaderName name, const String& value)
{
    updateResourceRequest();

    if (!m_requestData.m_httpHeaderFields.addIfNotPresent(name, value))
        return;

    m_platformRequestUpdated = false;
}

void ResourceRequestBase::addHTTPHeaderField(HTTPHeaderName name, const String& value)
{
    updateResourceRequest();

    m_requestData.m_httpHeaderFields.add(name, value);

    m_platformRequestUpdated = false;
}

void ResourceRequestBase::addHTTPHeaderField(const String& name, const String& value)
{
    updateResourceRequest();

    m_requestData.m_httpHeaderFields.add(name, value);

    m_platformRequestUpdated = false;
}

bool ResourceRequestBase::hasHTTPHeaderField(HTTPHeaderName headerName) const
{
    return m_requestData.m_httpHeaderFields.contains(headerName);
}

void ResourceRequestBase::setHTTPHeaderFields(HTTPHeaderMap headerFields)
{
    updateResourceRequest();

    m_requestData.m_httpHeaderFields = WTFMove(headerFields);

    m_platformRequestUpdated = false;
}

void ResourceRequestBase::removeHTTPHeaderField(const String& name)
{
    updateResourceRequest();

    m_requestData.m_httpHeaderFields.remove(name);

    m_platformRequestUpdated = false;
}

void ResourceRequestBase::removeHTTPHeaderField(HTTPHeaderName name)
{
    updateResourceRequest();

    m_requestData.m_httpHeaderFields.remove(name);

    m_platformRequestUpdated = false;
}

void ResourceRequestBase::setIsAppInitiated(bool isAppInitiated)
{
    updateResourceRequest();

    if (m_requestData.m_isAppInitiated == isAppInitiated)
        return;

    m_requestData.m_isAppInitiated = isAppInitiated;

    m_platformRequestUpdated = false;
}

void ResourceRequestBase::setPrivacyProxyFailClosedForUnreachableNonMainHosts(bool privacyProxyFailClosedForUnreachableNonMainHosts)
{
    updateResourceRequest();

    if (m_requestData.m_privacyProxyFailClosedForUnreachableNonMainHosts == privacyProxyFailClosedForUnreachableNonMainHosts)
        return;

    m_requestData.m_privacyProxyFailClosedForUnreachableNonMainHosts = privacyProxyFailClosedForUnreachableNonMainHosts;

    m_platformRequestUpdated = false;
}

void ResourceRequestBase::setUseAdvancedPrivacyProtections(bool useAdvancedPrivacyProtections)
{
    updateResourceRequest();

    if (m_requestData.m_useAdvancedPrivacyProtections == useAdvancedPrivacyProtections)
        return;

    m_requestData.m_useAdvancedPrivacyProtections = useAdvancedPrivacyProtections;

    m_platformRequestUpdated = false;
}

void ResourceRequestBase::setDidFilterLinkDecoration(bool didFilterLinkDecoration)
{
    if (m_requestData.m_didFilterLinkDecoration == didFilterLinkDecoration)
        return;
    m_requestData.m_didFilterLinkDecoration = didFilterLinkDecoration;
}

void ResourceRequestBase::setIsPrivateTokenUsageByThirdPartyAllowed(bool isPrivateTokenUsageByThirdPartyAllowed)
{
    m_requestData.m_isPrivateTokenUsageByThirdPartyAllowed = isPrivateTokenUsageByThirdPartyAllowed;
}

void ResourceRequestBase::setWasSchemeOptimisticallyUpgraded(bool wasSchemeOptimisticallyUpgraded)
{
    m_requestData.m_wasSchemeOptimisticallyUpgraded = wasSchemeOptimisticallyUpgraded;
}

bool equalIgnoringHeaderFields(const ResourceRequestBase& a, const ResourceRequestBase& b)
{
    if (a.url() != b.url())
        return false;
    
    if (a.cachePolicy() != b.cachePolicy())
        return false;
    
    if (a.timeoutInterval() != b.timeoutInterval())
        return false;
    
    if (a.firstPartyForCookies() != b.firstPartyForCookies())
        return false;

    if (a.isSameSite() != b.isSameSite())
        return false;

    if (a.isTopSite() != b.isTopSite())
        return false;
    
    if (a.httpMethod() != b.httpMethod())
        return false;
    
    if (a.allowCookies() != b.allowCookies())
        return false;
    
    if (a.priority() != b.priority())
        return false;

    if (a.requester() != b.requester())
        return false;

    return arePointingToEqualData(a.httpBody(), b.httpBody());
}

bool ResourceRequestBase::equal(const ResourceRequest& a, const ResourceRequest& b)
{
    if (!equalIgnoringHeaderFields(a, b))
        return false;
    
    if (a.httpHeaderFields() != b.httpHeaderFields())
        return false;
        
    return ResourceRequest::platformCompare(a, b);
}

static const HTTPHeaderName conditionalHeaderNames[] = {
    HTTPHeaderName::IfMatch,
    HTTPHeaderName::IfModifiedSince,
    HTTPHeaderName::IfNoneMatch,
    HTTPHeaderName::IfRange,
    HTTPHeaderName::IfUnmodifiedSince
};

bool ResourceRequestBase::isConditional() const
{
    updateResourceRequest();

    for (auto headerName : conditionalHeaderNames) {
        if (m_requestData.m_httpHeaderFields.contains(headerName))
            return true;
    }

    return false;
}

void ResourceRequestBase::makeUnconditional()
{
    updateResourceRequest();

    for (auto headerName : conditionalHeaderNames)
        m_requestData.m_httpHeaderFields.remove(headerName);
}

double ResourceRequestBase::defaultTimeoutInterval()
{
    return s_defaultTimeoutInterval;
}

void ResourceRequestBase::setDefaultTimeoutInterval(double timeoutInterval)
{
    s_defaultTimeoutInterval = timeoutInterval;
}

void ResourceRequestBase::updatePlatformRequest(HTTPBodyUpdatePolicy bodyPolicy) const
{
    if (!m_platformRequestUpdated) {
        ASSERT(m_resourceRequestUpdated);
        const_cast<ResourceRequest&>(asResourceRequest()).doUpdatePlatformRequest();
        m_platformRequestUpdated = true;
    }

    if (!m_platformRequestBodyUpdated && bodyPolicy == HTTPBodyUpdatePolicy::UpdateHTTPBody) {
        ASSERT(m_resourceRequestBodyUpdated);
        const_cast<ResourceRequest&>(asResourceRequest()).doUpdatePlatformHTTPBody();
        m_platformRequestBodyUpdated = true;
    }
}

void ResourceRequestBase::updateResourceRequest(HTTPBodyUpdatePolicy bodyPolicy) const
{
    if (!m_resourceRequestUpdated) {
        ASSERT(m_platformRequestUpdated);
        const_cast<ResourceRequest&>(asResourceRequest()).doUpdateResourceRequest();
        m_resourceRequestUpdated = true;
    }

    if (!m_resourceRequestBodyUpdated && bodyPolicy == HTTPBodyUpdatePolicy::UpdateHTTPBody) {
        ASSERT(m_platformRequestBodyUpdated);
        const_cast<ResourceRequest&>(asResourceRequest()).doUpdateResourceHTTPBody();
        m_resourceRequestBodyUpdated = true;
    }
}

#if !PLATFORM(COCOA) && !USE(SOUP)
unsigned initializeMaximumHTTPConnectionCountPerHost()
{
    // This is used by the loader to control the number of issued parallel load requests. 
    // Four seems to be a common default in HTTP frameworks.
    return 4;
}
#endif

void ResourceRequestBase::setCachePartition(const String& cachePartition)
{
#if ENABLE(CACHE_PARTITIONING)
    ASSERT(!cachePartition.isNull());
    ASSERT(cachePartition == partitionName(cachePartition));
    m_cachePartition = cachePartition;
#else
    UNUSED_PARAM(cachePartition);
#endif
}

String ResourceRequestBase::partitionName(const String& domain)
{
    if (domain.isNull())
        return emptyString();
    auto highLevel = PublicSuffixStore::singleton().topPrivatelyControlledDomain(domain);
    if (highLevel.isNull())
        return emptyString();
    return highLevel;
}

bool ResourceRequestBase::isThirdParty() const
{
    return !areRegistrableDomainsEqual(url(), firstPartyForCookies());
}

}
