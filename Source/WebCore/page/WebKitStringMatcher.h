/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <span>
#include <wtf/RefCounted.h>
#include <wtf/text/Latin1Character.h>

namespace JSC {
class JSGlobalObject;
class JSValue;
}

namespace WebCore {

struct WebKitStringMatcherOptions;

class WEBCORE_EXPORT WebKitStringMatcher : public RefCounted<WebKitStringMatcher> {
public:
    struct State {
        // a transitionsEndIndexOrMatchSentinel of 0 means transitionsBeginIndexOrMatchIdentifier is an identifier
        // and jump to the next State without progressing through the string.
        static constexpr uint16_t matchSentinel { 0 };

        uint16_t transitionsBeginIndexOrMatchIdentifier { 0 };
        uint16_t transitionsEndIndexOrMatchSentinel { 0 };
    };

    struct Transition {
        uint16_t codeUnitToCheck { 0 };
        uint16_t stateIndexToTransitionToIfMatched { 0 };
    };

    virtual ~WebKitStringMatcher();

    static std::optional<Vector<uint16_t>> dataForMatchingStrings(const Vector<std::pair<String, uint16_t>>&);
    static std::optional<std::pair<std::span<const State>, std::span<const Transition>>> statesAndTransitionsFromVersionedData(std::span<const uint8_t>);

    struct MatchInfo {
        uint16_t identifier { 0 };
        size_t substringBeginIndex { 0 };
        size_t substringEndIndex { 0 };
    };
    Vector<MatchInfo> match(std::span<const char16_t>, const WebKitStringMatcherOptions&);
    Vector<MatchInfo> match(std::span<const Latin1Character>, const WebKitStringMatcherOptions&);
    JSC::JSValue match(JSC::JSGlobalObject&, const String&, const WebKitStringMatcherOptions&);

private:
    struct Trie;

    virtual std::span<const State> states() const = 0;
    virtual std::span<const Transition> transitions() const = 0;
};

}
