/*
 * Copyright (C) 2011-2024 Apple Inc. All rights reserved.
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
#include <wtf/persistence/PersistentCoders.h>

#include <wtf/StdLibExtras.h>
#include <wtf/URL.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WTF::Persistence {

void Coder<AtomString>::encodeForPersistence(Encoder& encoder, const AtomString& atomString)
{
    encoder << atomString.string();
}

// FIXME: Constructing a String and then looking it up in the AtomStringTable is inefficient.
// Ideally, we wouldn't need to allocate a String when it is already in the AtomStringTable.
std::optional<AtomString> Coder<AtomString>::decodeForPersistence(Decoder& decoder)
{
    std::optional<String> string;
    decoder >> string;
    if (!string)
        return std::nullopt;

    return { AtomString { WTFMove(*string) } };
}

void Coder<CString>::encodeForPersistence(Encoder& encoder, const CString& string)
{
    // Special case the null string.
    if (string.isNull()) {
        encoder << std::numeric_limits<uint32_t>::max();
        return;
    }

    uint32_t length = string.length();
    encoder << length;
    encoder.encodeFixedLengthData(byteCast<uint8_t>(string.span()));
}

std::optional<CString> Coder<CString>::decodeForPersistence(Decoder& decoder)
{
    std::optional<uint32_t> length;
    decoder >> length;
    if (!length)
        return std::nullopt;

    if (length == std::numeric_limits<uint32_t>::max()) {
        // This is the null string.
        return CString();
    }

    // Before allocating the string, make sure that the decoder buffer is big enough.
    if (!decoder.bufferIsLargeEnoughToContain<char>(*length))
        return std::nullopt;

    std::span<char> buffer;
    CString string = CString::newUninitialized(*length, buffer);
    if (!decoder.decodeFixedLengthData(byteCast<uint8_t>(buffer)))
        return std::nullopt;

    return string;
}

void Coder<String>::encodeForPersistence(Encoder& encoder, const String& string)
{
    // Special case the null string.
    if (string.isNull()) {
        encoder << std::numeric_limits<uint32_t>::max();
        return;
    }

    bool is8Bit = string.is8Bit();

    encoder << string.length() << is8Bit;

    if (is8Bit)
        encoder.encodeFixedLengthData(string.span8());
    else
        encoder.encodeFixedLengthData(asBytes(string.span16()));
}

template <typename CharacterType>
static inline std::optional<String> decodeStringText(Decoder& decoder, uint32_t length)
{
    // Before allocating the string, make sure that the decoder buffer is big enough.
    if (!decoder.bufferIsLargeEnoughToContain<CharacterType>(length))
        return std::nullopt;

    std::span<CharacterType> buffer;
    String string = String::createUninitialized(length, buffer);
    if (!decoder.decodeFixedLengthData(asMutableByteSpan(buffer)))
        return std::nullopt;
    
    return string;
}

std::optional<String> Coder<String>::decodeForPersistence(Decoder& decoder)
{
    std::optional<uint32_t> length;
    decoder >> length;
    if (!length)
        return std::nullopt;

    if (*length == std::numeric_limits<uint32_t>::max()) {
        // This is the null string.
        return String();
    }

    std::optional<bool> is8Bit;
    decoder >> is8Bit;
    if (!is8Bit)
        return std::nullopt;

    if (*is8Bit)
        return decodeStringText<LChar>(decoder, *length);
    return decodeStringText<char16_t>(decoder, *length);
}

void Coder<URL>::encodeForPersistence(Encoder& encoder, const URL& url)
{
    encoder << url.string();
}

std::optional<URL> Coder<URL>::decodeForPersistence(Decoder& decoder)
{
    std::optional<String> string;
    decoder >> string;
    if (!string)
        return std::nullopt;
    return URL(WTFMove(*string));
}

void Coder<SHA1::Digest>::encodeForPersistence(Encoder& encoder, const SHA1::Digest& digest)
{
    encoder.encodeFixedLengthData({ digest });
}

std::optional<SHA1::Digest> Coder<SHA1::Digest>::decodeForPersistence(Decoder& decoder)
{
    SHA1::Digest tmp;
    if (!decoder.decodeFixedLengthData({ tmp }))
        return std::nullopt;
    return tmp;
}

void Coder<WallTime>::encodeForPersistence(Encoder& encoder, const WallTime& time)
{
    encoder << time.secondsSinceEpoch().value();
}

std::optional<WallTime> Coder<WallTime>::decodeForPersistence(Decoder& decoder)
{
    std::optional<double> value;
    decoder >> value;
    if (!value)
        return std::nullopt;

    return WallTime::fromRawSeconds(*value);
}

void Coder<Seconds>::encodeForPersistence(Encoder& encoder, const Seconds& seconds)
{
    encoder << seconds.value();
}

std::optional<Seconds> Coder<Seconds>::decodeForPersistence(Decoder& decoder)
{
    std::optional<double> value;
    decoder >> value;
    if (!value)
        return std::nullopt;
    return Seconds(*value);
}

}
