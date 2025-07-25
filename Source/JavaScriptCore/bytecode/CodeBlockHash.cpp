/*
 * Copyright (C) 2012-2020 Apple Inc. All rights reserved.
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
#include "CodeBlockHash.h"

#include "SourceCode.h"
#include <wtf/SHA1.h>
#include <wtf/SixCharacterHash.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace JSC {

CodeBlockHash::CodeBlockHash(std::span<const char, stringLength> string)
    : m_hash(sixCharacterHashStringToInteger(string))
{
}

CodeBlockHash::CodeBlockHash(StringView codeBlockSourceCode, StringView entireSourceCode, CodeSpecializationKind kind)
{
    SHA1 sha1;

    // The maxSourceCodeLengthToHash is a heuristic to avoid crashing fuzzers
    // due to resource exhaustion. This is OK to do because:
    // 1. CodeBlockHash is not a critical hash.
    // 2. In practice, reasonable source code are not 500 MB or more long.
    // 3. And if they are that long, then we are still diversifying the hash on
    //    their length. But if they do collide, it's OK.
    // The only invariant here is that we should always produce the same hash
    // for the same source string. The algorithm below achieves that.
    constexpr unsigned maxSourceCodeLengthToHash = 500 * MB;
    if (static_cast<unsigned>(codeBlockSourceCode.length()) < maxSourceCodeLengthToHash)
        sha1.addUTF8Bytes(codeBlockSourceCode);
    else {
        // Just hash with the length and samples of the source string instead.
        unsigned index = 0;
        unsigned oldIndex = 0;
        unsigned length = entireSourceCode.length();
        unsigned step = (length >> 10) + 1;

        sha1.addBytes(std::span { std::bit_cast<uint8_t*>(&length), sizeof(length) });
        do {
            char16_t character = entireSourceCode[index];
            sha1.addBytes(std::span { std::bit_cast<uint8_t*>(&character), sizeof(character) });
            oldIndex = index;
            index += step;
        } while (index > oldIndex && index < length);
    }

    SHA1::Digest digest;
    sha1.computeHash(digest);
    m_hash = digest[0] | (digest[1] << 8) | (digest[2] << 16) | (digest[3] << 24);

    if (m_hash == 0 || m_hash == 1)
        m_hash += 0x2d5a93d0; // Ensures a non-zero hash, and gets us #Azero0 for CodeForCall and #Azero1 for CodeForConstruct.
    static_assert(!static_cast<unsigned>(CodeSpecializationKind::CodeForCall));
    static_assert(static_cast<unsigned>(CodeSpecializationKind::CodeForConstruct));
    m_hash ^= static_cast<unsigned>(kind);
    ASSERT(m_hash);
}

CodeBlockHash::CodeBlockHash(const SourceCode& sourceCode, CodeSpecializationKind kind)
    : CodeBlockHash(sourceCode.view(), sourceCode.provider()->source(), kind)
{
}

void CodeBlockHash::dump(PrintStream& out) const
{
    auto buffer = integerToSixCharacterHashString(m_hash);
    
#if ASSERT_ENABLED
    CodeBlockHash recompute(buffer);
    ASSERT(recompute == *this);
#endif // ASSERT_ENABLED
    
    out.print(std::span<const char> { buffer });
}

} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
