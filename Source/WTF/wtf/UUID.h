/*
* Copyright (C) 2010 Google Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*
*     * Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Hasher.h>
#include <wtf/HexNumber.h>
#include <wtf/Int128.h>
#include <wtf/SHA1.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringConcatenate.h>
#include <wtf/text/WTFString.h>

#ifdef __OBJC__
@class NSUUID;
#endif

namespace WTF {

class StringView;

class UUID {
WTF_MAKE_FAST_ALLOCATED;
public:
    static constexpr UInt128 emptyValue = 0;
    static constexpr UInt128 deletedValue = 1;

    static UUID createVersion4()
    {
        return UUID { };
    }

    static UUID createVersion4Weak()
    {
        return UUID { generateWeakRandomUUIDVersion4() };
    }

    WTF_EXPORT_PRIVATE static UUID createVersion5(const SHA1::Digest&);
    WTF_EXPORT_PRIVATE static UUID createVersion5(UUID, std::span<const uint8_t>);

#ifdef __OBJC__
    WTF_EXPORT_PRIVATE RetainPtr<NSUUID> createNSUUID() const;
    WTF_EXPORT_PRIVATE static std::optional<UUID> fromNSUUID(NSUUID *);
#endif

    WTF_EXPORT_PRIVATE static std::optional<UUID> parse(StringView);
    WTF_EXPORT_PRIVATE static std::optional<UUID> parseVersion4(StringView);

    explicit UUID(std::span<const uint8_t, 16> span)
    {
        memcpySpan(asMutableByteSpan(m_data), span);
    }

    explicit UUID(std::span<const uint8_t> span)
    {
        RELEASE_ASSERT(span.size() == 16);
        memcpySpan(asMutableByteSpan(m_data), span);
    }

    explicit constexpr UUID(UInt128 data)
        : m_data(data)
    {
    }

    explicit UUID(uint64_t high, uint64_t low)
        : m_data((static_cast<UInt128>(high) << 64) | low)
    {
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!isHashTableDeletedValue());
    }

    std::span<const uint8_t, 16> span() const LIFETIME_BOUND
    {
        return asByteSpan<UInt128, 16>(m_data);
    }

    friend bool operator==(const UUID&, const UUID&) = default;

    explicit constexpr UUID(HashTableDeletedValueType)
        : m_data(deletedValue)
    {
    }

    explicit constexpr UUID(HashTableEmptyValueType)
        : m_data(emptyValue)
    {
    }

    static bool isValid(uint64_t high, uint64_t low)
    {
        auto data = (static_cast<UInt128>(high) << 64) | low;
        return data != deletedValue && data != emptyValue;
    }

    constexpr bool isHashTableDeletedValue() const { return m_data == deletedValue; }
    constexpr bool isHashTableEmptyValue() const { return m_data == emptyValue; }
    WTF_EXPORT_PRIVATE String toString() const;

    constexpr operator bool() const { return !!m_data; }
    bool isValid() const { return m_data != emptyValue && m_data != deletedValue; }

    UInt128 data() const { return m_data; }

    uint64_t low() const { return static_cast<uint64_t>(m_data); }
    uint64_t high() const { return static_cast<uint64_t>(m_data >> 64);  }

private:
    WTF_EXPORT_PRIVATE UUID();
    friend void add(Hasher&, UUID);

    WTF_EXPORT_PRIVATE static UInt128 generateWeakRandomUUIDVersion4();

    UInt128 m_data;
};

template<>
struct MarkableTraits<UUID> {
    static bool isEmptyValue(const UUID& uuid) { return !uuid; }
    static UUID emptyValue() { return UUID { UInt128 { 0 } }; }
};

inline void add(Hasher& hasher, UUID uuid)
{
    add(hasher, uuid.m_data);
}

struct UUIDHash {
    static unsigned hash(const UUID& key) { return computeHash(key); }
    static bool equal(const UUID& a, const UUID& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

template<> struct HashTraits<UUID> : GenericHashTraits<UUID> {
    static UUID emptyValue() { return UUID { HashTableEmptyValue }; }
    static bool isEmptyValue(const UUID& value) { return value.isHashTableEmptyValue(); }
    static void constructDeletedValue(UUID& slot) { slot = UUID { HashTableDeletedValue }; }
    static bool isDeletedValue(const UUID& value) { return value.isHashTableDeletedValue(); }
};
template<> struct DefaultHash<UUID> : UUIDHash { };

// Creates a UUID that consists of 32 hexadecimal digits and returns its canonical form.
// The canonical form is displayed in 5 groups separated by hyphens, in the form 8-4-4-4-12 for a total of 36 characters.
// The hexadecimal values "a" through "f" are output as lower case characters.
//
// Note: for security reason, we should always generate version 4 UUID that use a scheme relying only on random numbers.
// This algorithm sets the version number as well as two reserved bits. All other bits are set using a random or pseudorandom
// data source. Version 4 UUIDs have the form xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx with hexadecimal digits for x and one of 8,
// 9, A, or B for y.

WTF_EXPORT_PRIVATE String createVersion4UUIDString();
WTF_EXPORT_PRIVATE String createVersion4UUIDStringWeak();

WTF_EXPORT_PRIVATE String bootSessionUUIDString();
WTF_EXPORT_PRIVATE bool isVersion4UUID(StringView);

template<>
class StringTypeAdapter<UUID> {
public:
    StringTypeAdapter(UUID uuid)
        : m_uuid { uuid }
    {
    }

    template<typename Func>
    auto handle(Func&& func) const -> decltype(auto)
    {
        UInt128 data = m_uuid.data();
        auto high = static_cast<uint64_t>(data >> 64);
        auto low = static_cast<uint64_t>(data);
        return handleWithAdapters(std::forward<Func>(func),
            hex(high >> 32, 8, Lowercase),
            '-',
            hex((high >> 16) & 0xffff, 4, Lowercase),
            '-',
            hex(high & 0xffff, 4, Lowercase),
            '-',
            hex(low >> 48, 4, Lowercase),
            '-',
            hex(low & 0xffffffffffff, 12, Lowercase));
    }

    unsigned length() const
    {
        return handle([](auto&&... adapters) -> unsigned {
            auto sum = checkedSum<int32_t>(adapters.length()...);
            if (sum.hasOverflowed())
                return UINT_MAX;
            return sum;
        });
    }

    bool is8Bit() const { return true; }

    template<typename CharacterType>
    void writeTo(std::span<CharacterType> destination) const
    {
        handle([&](auto&&... adapters) {
            stringTypeAdapterAccumulator(destination, std::forward<decltype(adapters)>(adapters)...);
        });
    }

private:
    UUID m_uuid;
};

}

using WTF::createVersion4UUIDString;
using WTF::createVersion4UUIDStringWeak;
using WTF::bootSessionUUIDString;
