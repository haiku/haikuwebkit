/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#if PLATFORM(COCOA)

#import <CoreFoundation/CoreFoundation.h>
#import <wtf/ArgumentCoder.h>
#import <wtf/TZoneMalloc.h>
#import <wtf/text/WTFString.h>

OBJC_CLASS NSError;

namespace WebKit {

class CoreIPCError {
    WTF_MAKE_TZONE_ALLOCATED(CoreIPCError);
public:
    CoreIPCError(CoreIPCError&&) = default;
    CoreIPCError& operator=(CoreIPCError&&) = default;

    CoreIPCError(NSError *);
    CoreIPCError(String&& domain, int64_t code, std::unique_ptr<CoreIPCError>&& underlyingError, std::optional<Vector<RetainPtr<SecCertificateRef>>>&& clientCertificateChain, std::optional<Vector<RetainPtr<SecCertificateRef>>>&& peerCertificateChain, String&& localizedDescription, String&& localizedFailureReasonError, String&& localizedRecoverySuggestionError, std::optional<Vector<String>>&& localizedRecoveryOptionsError, String&& localizedFailureError, String&& helpAnchorError, String&& debugDescriptionError, RetainPtr<NSNumber>&& stringEncodingError, RetainPtr<SecTrustRef>&& failingURLPeerTrustError, RetainPtr<NSURL>&& urlError, RetainPtr<NSURL>&& failingURLError,
#if USE(NSURL_ERROR_FAILING_URL_STRING_KEY)
        String&& failingURLStringError,
#endif
        String&& filePathError, String&& networkTaskDescription, String&& networkTaskMetricsPrivacyStance, String&& description)
        : m_domain(WTFMove(domain))
        , m_code(WTFMove(code))
        , m_underlyingError(WTFMove(underlyingError))
        , m_clientCertificateChain(WTFMove(clientCertificateChain))
        , m_peerCertificateChain(WTFMove(peerCertificateChain))
        , m_localizedDescription(WTFMove(localizedDescription))
        , m_localizedFailureReasonError(WTFMove(localizedFailureReasonError))
        , m_localizedRecoverySuggestionError(WTFMove(localizedRecoverySuggestionError))
        , m_localizedRecoveryOptionsError(WTFMove(localizedRecoveryOptionsError))
        , m_localizedFailureError(WTFMove(localizedFailureError))
        , m_helpAnchorError(WTFMove(helpAnchorError))
        , m_debugDescriptionError(WTFMove(debugDescriptionError))
        , m_stringEncodingError(WTFMove(stringEncodingError))
        , m_failingURLPeerTrustError(WTFMove(failingURLPeerTrustError))
        , m_urlError(WTFMove(urlError))
        , m_failingURLError(WTFMove(failingURLError))
#if USE(NSURL_ERROR_FAILING_URL_STRING_KEY)
        , m_failingURLStringError(WTFMove(failingURLStringError))
#endif
        , m_filePathError(WTFMove(filePathError))
        , m_networkTaskDescription(WTFMove(networkTaskDescription))
        , m_networkTaskMetricsPrivacyStance(WTFMove(networkTaskMetricsPrivacyStance))
        , m_description(WTFMove(description))
    {
    }

    RetainPtr<id> toID() const;

private:
    friend struct IPC::ArgumentCoder<CoreIPCError>;

    String m_domain;
    int64_t m_code;
    std::unique_ptr<CoreIPCError> m_underlyingError;

    std::optional<Vector<RetainPtr<SecCertificateRef>>> m_clientCertificateChain;
    std::optional<Vector<RetainPtr<SecCertificateRef>>> m_peerCertificateChain;
    String m_localizedDescription;
    String m_localizedFailureReasonError;
    String m_localizedRecoverySuggestionError;
    std::optional<Vector<String>> m_localizedRecoveryOptionsError;
    String m_localizedFailureError;

    String m_helpAnchorError;
    String m_debugDescriptionError;

    RetainPtr<NSNumber> m_stringEncodingError;

    RetainPtr<SecTrustRef> m_failingURLPeerTrustError;
    RetainPtr<NSURL> m_urlError;
    RetainPtr<NSURL> m_failingURLError;
#if USE(NSURL_ERROR_FAILING_URL_STRING_KEY)
    String m_failingURLStringError;
#endif

    String m_filePathError;

    String m_networkTaskDescription;
    String m_networkTaskMetricsPrivacyStance;

    String m_description;
};

}

#endif // PLATFORM(COCOA)
