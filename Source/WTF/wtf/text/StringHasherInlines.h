/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include <wtf/Assertions.h>
#include <wtf/text/StringHasher.h>
#include <wtf/text/SuperFastHash.h>
#include <wtf/text/WYHash.h>

namespace WTF {

template<typename T, typename Converter>
unsigned StringHasher::computeHashAndMaskTop8Bits(std::span<const T> data)
{
#if ENABLE(WYHASH_STRING_HASHER)
    if (data.size() <= smallStringThreshold) [[likely]]
        return SuperFastHash::computeHashAndMaskTop8Bits<T, Converter>(data);
    return WYHash::computeHashAndMaskTop8Bits<T, Converter>(data);
#else
    return SuperFastHash::computeHashAndMaskTop8Bits<T, Converter>(data);
#endif
}

template<typename T, unsigned characterCount>
constexpr unsigned StringHasher::computeLiteralHashAndMaskTop8Bits(const T (&characters)[characterCount])
{
    constexpr unsigned characterCountWithoutNull = characterCount - 1;
#if ENABLE(WYHASH_STRING_HASHER)
    if constexpr (characterCountWithoutNull <= smallStringThreshold)
        return SuperFastHash::computeHashAndMaskTop8Bits<T>(unsafeMakeSpan(characters, characterCountWithoutNull));
    return WYHash::computeHashAndMaskTop8Bits<T>(unsafeMakeSpan(characters, characterCountWithoutNull));
#else
    return SuperFastHash::computeHashAndMaskTop8Bits<T>(unsafeMakeSpan(characters, characterCountWithoutNull));
#endif
}

inline void StringHasher::addCharacter(char16_t character)
{
#if ENABLE(WYHASH_STRING_HASHER)
    if (m_bufferSize == smallStringThreshold) {
        // This algorithm must stay in sync with WYHash::hash function.
        if (!m_pendingHashValue) {
            m_seed = WYHash::initSeed();
            m_see1 = m_seed;
            m_see2 = m_seed;
            m_pendingHashValue = true;
        }
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
        char16_t* p = m_buffer.data();
        while (m_bufferSize >= 24) {
            WYHash::consume24Characters(p, WYHash::Reader16Bit<char16_t>::wyr8, m_seed, m_see1, m_see2);
            p += 24;
            m_bufferSize -= 24;
        }
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
        ASSERT(!m_bufferSize);
        m_numberOfProcessedCharacters += smallStringThreshold;
    }

    ASSERT(m_bufferSize < smallStringThreshold);
    m_buffer[m_bufferSize++] = character;
#else
    SuperFastHash::addCharacterImpl(character, m_hasPendingCharacter, m_pendingCharacter, m_hash);
#endif
}

inline unsigned StringHasher::hashWithTop8BitsMasked()
{
#if ENABLE(WYHASH_STRING_HASHER)
    unsigned hashValue;
    if (!m_pendingHashValue) {
        ASSERT(m_bufferSize <= smallStringThreshold);
        hashValue = SuperFastHash::computeHashAndMaskTop8Bits<char16_t>(std::span { m_buffer }.first(m_bufferSize));
    } else {
        // This algorithm must stay in sync with WYHash::hash function.
        auto wyr8 = WYHash::Reader16Bit<char16_t>::wyr8;
        unsigned i = m_bufferSize;
        if (i <= 24)
            m_seed ^= m_see1 ^ m_see2;
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
        char16_t* p = m_buffer.data();
        WYHash::handleGreaterThan8CharactersCase(p, i, wyr8, m_seed, m_see1, m_see2);

        uint64_t a = 0;
        uint64_t b = 0;
        if (m_bufferSize >= 8) {
            a = wyr8(p + i - 8);
            b = wyr8(p + i - 4);
        } else {
            char16_t tmp[8];
            unsigned bufferIndex = smallStringThreshold - (8 - i);
            for (unsigned tmpIndex = 0; tmpIndex < 8; tmpIndex++) {
                tmp[tmpIndex] = m_buffer[bufferIndex];
                bufferIndex = (bufferIndex + 1) % smallStringThreshold;
            }

            char16_t* tmpPtr = tmp;
            a = wyr8(tmpPtr);
            b = wyr8(tmpPtr + 4);
        }
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

        const uint64_t totalByteCount = (static_cast<uint64_t>(m_numberOfProcessedCharacters) + static_cast<uint64_t>(m_bufferSize)) << 1;
        hashValue = StringHasher::avoidZero(WYHash::handleEndCase(a, b, m_seed, totalByteCount) & StringHasher::maskHash);

        m_pendingHashValue = false;
        m_numberOfProcessedCharacters = m_seed = m_see1 = m_see2 = 0;
    }
    m_bufferSize = 0;
    return hashValue;
#else
    unsigned hashValue = SuperFastHash::hashWithTop8BitsMaskedImpl(m_hasPendingCharacter, m_pendingCharacter, m_hash);
    m_hasPendingCharacter = false;
    m_pendingCharacter = 0;
    m_hash = stringHashingStartValue;
    return hashValue;
#endif
}

} // namespace WTF

using WTF::StringHasher;
