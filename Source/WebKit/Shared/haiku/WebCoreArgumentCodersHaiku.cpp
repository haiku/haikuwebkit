/*
 * Copyright (C) 2014,2019 Haiku, Inc.
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
#include "WebCoreArgumentCoders.h"

#include "DaemonDecoder.h"
#include "DaemonEncoder.h"
#include "DataReference.h"

#include <WebCore/CertificateInfo.h>
#include <WebCore/FontAttributes.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <wtf/text/CString.h>

using namespace WebCore;

namespace IPC {

#if !USE(CURL)
template<typename Encoder>
void ArgumentCoder<CertificateInfo>::encode(Encoder& encoder, const CertificateInfo& certificateInfo)
{
    //nothing to encode ceriticateinfo is null
}
template void ArgumentCoder<WebCore::CertificateInfo>::encode<Encoder>(Encoder&, const WebCore::CertificateInfo&);
template void ArgumentCoder<WebCore::CertificateInfo>::encode<WebKit::Daemon::Encoder>(WebKit::Daemon::Encoder&, const WebCore::CertificateInfo&);

template<typename Decoder>
std::optional<WebCore::CertificateInfo> ArgumentCoder<CertificateInfo>::decode(Decoder& decoder)
{
    //nothing to decode just return true
    return {};
}
template std::optional<WebCore::CertificateInfo> ArgumentCoder<WebCore::CertificateInfo>::decode<Decoder>(Decoder&);
template std::optional<WebCore::CertificateInfo> ArgumentCoder<WebCore::CertificateInfo>::decode<WebKit::Daemon::Decoder>(WebKit::Daemon::Decoder&);

void ArgumentCoder<ResourceError>::encodePlatformData(Encoder& encoder, const ResourceError& resourceError)
{
    encoder << resourceError.domain();
    encoder << resourceError.errorCode();
    encoder << resourceError.failingURL().string();
    encoder << resourceError.localizedDescription();
}

bool ArgumentCoder<ResourceError>::decodePlatformData(Decoder& decoder, ResourceError& resourceError)
{
    String domain;
    if (!decoder.decode(domain))
        return false;

    int errorCode;
    if (!decoder.decode(errorCode))
        return false;

    String failingURL;
    if (!decoder.decode(failingURL))
        return false;

    String localizedDescription;
    if (!decoder.decode(localizedDescription))
        return false;

    resourceError = ResourceError(domain, errorCode, URL(URL(), failingURL), localizedDescription);
    return true;
}

void ArgumentCoder<ProtectionSpace>::encodePlatformData(Encoder&, const ProtectionSpace&)
{
    ASSERT_NOT_REACHED();
}

bool ArgumentCoder<ProtectionSpace>::decodePlatformData(Decoder&, ProtectionSpace&)
{
    ASSERT_NOT_REACHED();
    return false;
}

void ArgumentCoder<Credential>::encodePlatformData(Encoder&, const Credential&)
{
    ASSERT_NOT_REACHED();
}

bool ArgumentCoder<Credential>::decodePlatformData(Decoder&, Credential&)
{
    ASSERT_NOT_REACHED();
    return false;
}
#endif

void ArgumentCoder<BMessenger>::encode(Encoder& encoder, BMessenger&& messenger)
{
    constexpr size_t count = sizeof(BMessenger) / sizeof(uint32_t);
    uint32_t* data = (uint32_t*)&messenger;
    for (size_t i = 0; i < count; i++) {
        encoder << data[i];
    }
}

std::optional<BMessenger> ArgumentCoder<BMessenger>::decode(Decoder& decoder)
{
    constexpr size_t count = sizeof(BMessenger) / sizeof(uint32_t);
    uint32_t data[count];
    for (size_t i = 0; i < count; i++) {
        std::optional<uint32_t> value;
        if (decoder.decode(value))
            data[i] = *value;
    }
    BMessenger* messenger = (BMessenger*)data;
    return *messenger;
}

void ArgumentCoder<Font>::encodePlatformData(Encoder& encoder, const Font& font)
{
    ASSERT_NOT_REACHED();
}

std::optional<FontPlatformData> ArgumentCoder<Font>::decodePlatformData(Decoder&)
{
    ASSERT_NOT_REACHED();
    return std::nullopt;
}

void ArgumentCoder<WebCore::FontPlatformData::Attributes>::encodePlatformData(Encoder&, const WebCore::FontPlatformData::Attributes&)
{
    ASSERT_NOT_REACHED();
}

bool ArgumentCoder<WebCore::FontPlatformData::Attributes>::decodePlatformData(Decoder&, WebCore::FontPlatformData::Attributes&)
{
    ASSERT_NOT_REACHED();
    return false;
}

#if ENABLE(VIDEO) && !USE(CURL)
void ArgumentCoder<SerializedPlatformDataCueValue>::encodePlatformData(Encoder& encoder, const SerializedPlatformDataCueValue& value)
{
    ASSERT_NOT_REACHED();
}

std::optional<SerializedPlatformDataCueValue>  ArgumentCoder<SerializedPlatformDataCueValue>::decodePlatformData(Decoder& decoder, WebCore::SerializedPlatformDataCueValue::PlatformType platformType)
{
    ASSERT_NOT_REACHED();
    return std::nullopt;
}
#endif

}

