/*
 * Copyright (C) 2014 Haiku Inc. All rights reserved.
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

#ifndef CertificateInfo_h
#define CertificateInfo_h

#include "CertificateSummary.h"
#include "NotImplemented.h"
#include <optional>
#include <support/Locker.h>
#include <wtf/persistence/PersistentCoders.h>
#include <wtf/persistence/PersistentDecoder.h>
#include <wtf/persistence/PersistentEncoder.h>

#include <Certificate.h>

namespace WebCore {

class ResourceError;
class ResourceResponse;

class CertificateInfo {
public:
    CertificateInfo();
    explicit CertificateInfo(const BCertificate& certificate)
        : m_certificate(&certificate)
    { }

    CertificateInfo isolatedCopy() const;

    std::optional<CertificateSummary> summary() const { notImplemented(); return std::nullopt; }

    bool isEmpty() const { return m_certificate != nullptr; }

    const BCertificate& certificate() const { return *m_certificate; }

    bool operator==(const CertificateInfo& other) const
    {
        if (m_certificate == nullptr && other.m_certificate == nullptr)
            return true;
        else if (m_certificate == nullptr)
            return false;
        else if (other.m_certificate == nullptr)
            return false;
        else
            return *m_certificate == *other.m_certificate;
    }

    bool containsNonRootSHA1SignedCertificate() const { notImplemented(); return false; }
private:
    const BCertificate* m_certificate;
};

} // namespace WebCore

#endif // CertificateInfo_h
