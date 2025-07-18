/*
 * Copyright (C) 2013 Apple Inc.  All rights reserved.
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
 * Copyright (C) 2017 NAVER Corp.
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

#if USE(CURL)

#include "CurlRequest.h"
#include "CurlRequestClient.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"

#include "wtf/RefCounted.h"

namespace WebCore {

class CurlRequest;
class ResourceHandle;
class SharedBuffer;

class CurlDownloadListener {
public:
    virtual void didReceiveResponse(const ResourceResponse&) { }
    virtual void didReceiveDataOfLength(int) { }
    virtual void didFinish() { }
    virtual void didFail() { }
};

class CurlDownload final
    : public RefCounted<CurlDownload>
    , public CurlRequestClient
{
public:
    CurlDownload() = default;
    WEBCORE_EXPORT ~CurlDownload();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    WEBCORE_EXPORT void init(CurlDownloadListener&, URL&&);
    WEBCORE_EXPORT void init(CurlDownloadListener&, ResourceHandle*, const ResourceRequest&, const ResourceResponse&);

    void setListener(CurlDownloadListener* listener) { m_listener = listener; }

    WEBCORE_EXPORT void start(const String& destination);
    WEBCORE_EXPORT bool cancel();

    bool deletesFileUponFailure() const { return m_deletesFileUponFailure; }
    void setDeletesFileUponFailure(bool deletesFileUponFailure) { m_deletesFileUponFailure = deletesFileUponFailure; }

    void setDestination(const String& destination) { m_destination = destination; }

private:
    Ref<CurlRequest> createCurlRequest(ResourceRequest&);
    void curlDidSendData(CurlRequest&, unsigned long long, unsigned long long) override { }
    WEBCORE_EXPORT void curlDidReceiveResponse(CurlRequest&, CurlResponse&&) override;
    WEBCORE_EXPORT void curlDidReceiveData(CurlRequest&, Ref<SharedBuffer>&&) override;
    WEBCORE_EXPORT void curlDidComplete(CurlRequest&, NetworkLoadMetrics&&) override;
    WEBCORE_EXPORT void curlDidFailWithError(CurlRequest&, ResourceError&&, CertificateInfo&&) override;

    bool shouldRedirectAsGET(const ResourceRequest&, bool crossOrigin);
    void willSendRequest();

    CurlDownloadListener* m_listener { nullptr };
    bool m_isCancelled { false };

    ResourceRequest m_request;
    ResourceResponse m_response;
    ResourceHandle* m_resourceHandle { nullptr };
    bool m_deletesFileUponFailure { false };
    String m_destination;
    unsigned m_redirectCount { 0 };
    RefPtr<CurlRequest> m_curlRequest;
};

} // namespace WebCore

#endif
