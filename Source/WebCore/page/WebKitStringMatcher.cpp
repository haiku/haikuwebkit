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

#include "config.h"
#include "WebKitStringMatcher.h"

#include "WebKitStringMatcherOptions.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/JSCJSValue.h>
#include <wtf/CheckedRef.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WebKitStringMatcher::~WebKitStringMatcher() = default;

template<typename CodeUnit>
Vector<WebKitStringMatcher::MatchInfo> match(std::span<const CodeUnit> input, std::span<const WebKitStringMatcher::State> states, std::span<const WebKitStringMatcher::Transition> transitions, const WebKitStringMatcherOptions& options)
{
    Vector<WebKitStringMatcher::MatchInfo> result;

    size_t start = options.searchReverse ? input.size() - 1 : 0;
    size_t end = options.searchReverse ? -1 : input.size();
    size_t increment = options.searchReverse ? -1 : 1;

    for (size_t i = start; i < end; i += increment) {
        size_t stateIndex { 0 };
        for (size_t stringIndex = i; stringIndex < input.size(); stringIndex++) {
            auto& state = states[stateIndex];
            CodeUnit codeUnit = input[stringIndex];
            bool foundMatch { false };
            for (uint16_t transitionIndex = state.transitionsBeginIndexOrMatchIdentifier; transitionIndex < state.transitionsEndIndexOrMatchSentinel; transitionIndex++) {
                auto& transition = transitions[transitionIndex];
                if (codeUnit == transition.codeUnitToCheck) {
                    stateIndex = transition.stateIndexToTransitionToIfMatched;
                    foundMatch = true;
                    break;
                }
            }
            if (!foundMatch)
                break;
            while (states[stateIndex].transitionsEndIndexOrMatchSentinel == WebKitStringMatcher::State::matchSentinel) {
                result.append(WebKitStringMatcher::MatchInfo {
                    states[stateIndex].transitionsBeginIndexOrMatchIdentifier,
                    i,
                    stringIndex
                });
                if (!options.matchAll)
                    return result;
                stateIndex++;
            }
        }
    }

    return result;
}

Vector<WebKitStringMatcher::MatchInfo> WebKitStringMatcher::match(std::span<const char16_t> input, const WebKitStringMatcherOptions& options)
{
    return WebCore::match(input, states(), transitions(), options);
}

Vector<WebKitStringMatcher::MatchInfo> WebKitStringMatcher::match(std::span<const Latin1Character> input, const WebKitStringMatcherOptions& options)
{
    return WebCore::match(input, states(), transitions(), options);
}

JSC::JSValue WebKitStringMatcher::match(JSC::JSGlobalObject& globalObject, const String& input, const WebKitStringMatcherOptions& options)
{
    auto result = input.is8Bit() ? match(input.span8(), options) : match(input.span16(), options);
    JSContextRef context = ::toRef(&globalObject);
    Vector<JSValueRef> values;
    for (auto& matchInfo : result) {
        JSObjectRef object = JSObjectMake(context, nullptr, nullptr);
        JSObjectSetProperty(context, object, OpaqueJSString::tryCreate("substring"_s).get(), JSValueMakeString(context, OpaqueJSString::tryCreate(input.substring(matchInfo.substringBeginIndex, matchInfo.substringEndIndex - matchInfo.substringBeginIndex + 1)).get()), 0, 0);
        JSObjectSetProperty(context, object, OpaqueJSString::tryCreate("index"_s).get(), JSValueMakeNumber(context, matchInfo.substringBeginIndex), 0, 0);
        JSObjectSetProperty(context, object, OpaqueJSString::tryCreate("identifier"_s).get(), JSValueMakeNumber(context, matchInfo.identifier), 0, 0);
        if (!options.matchAll)
            return ::toJS(&globalObject, JSObjectMakeArray(context, 1, &object, nullptr));
        values.append(object);
    }
    return ::toJS(&globalObject, JSObjectMakeArray(context, values.size(), values.span().data(), nullptr));
}

struct WebKitStringMatcher::Trie {
    Trie(const Vector<std::pair<String, uint16_t>>&);

    bool operator==(const Trie&) const = default;
    struct Node;
    using Edges = HashMap<uint32_t, Node, WTF::IntHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>>;
    struct Node {
    public:
        bool operator==(const Node&) const = default;
        bool traverse(const Function<bool(const Node&)>& visitor) const;

        Vector<uint16_t> identifiers;
        Edges edges;
    };

    struct SerializedDFA {
        bool operator==(const SerializedDFA&) const = default;

        Vector<std::optional<WebKitStringMatcher::State>> states;
        Vector<WebKitStringMatcher::Transition> transitions;
    };
    std::optional<SerializedDFA> serialize() const;

    Node root;
};

bool WebKitStringMatcher::Trie::Node::traverse(const Function<bool(const Node&)>& visitor) const
{
    if (!visitor(*this))
        return false;
    for (auto& child : edges.values()) {
        if (!child.traverse(visitor))
            return false;
    }
    return true;
}

WebKitStringMatcher::Trie::Trie(const Vector<std::pair<String, uint16_t>>& stringsAndIdentifiers)
{
    for (auto& [string, identifier] : stringsAndIdentifiers) {
        auto* node = &root;
        for (auto codeUnit : StringView(string).codeUnits()) {
            uint32_t key = codeUnit & std::numeric_limits<uint16_t>::max();
            RELEASE_ASSERT(Edges::isValidKey(key));
            auto addResult = node->edges.ensure(key, [] {
                return Trie::Node { };
            });
            node = &addResult.iterator->value;
        }
        node->identifiers.append(identifier);
    }
}

auto WebKitStringMatcher::Trie::serialize() const -> std::optional<SerializedDFA>
{
    Vector<std::optional<WebKitStringMatcher::State>> states;
    Vector<WebKitStringMatcher::Transition> transitions;

    HashMap<const Trie::Node*, uint16_t> nodeToStateIndex;
    bool serializedStates = root.traverse([&] (const Trie::Node& node) {
        ASSERT(!nodeToStateIndex.contains(&node));
        if (states.size() > std::numeric_limits<uint16_t>::max())
            return false;
        nodeToStateIndex.set(&node, static_cast<uint16_t>(states.size()));
        for (auto& identifier : node.identifiers)
            states.append(WebKitStringMatcher::State { identifier, WebKitStringMatcher::State::matchSentinel });
        states.append(std::nullopt);
        return true;
    });
    if (!serializedStates)
        return std::nullopt;

    bool success = root.traverse([&] (const Trie::Node& node) {
        size_t transitionsBegin = transitions.size();
        for (auto& [codeUnit, child] : node.edges) {
            RELEASE_ASSERT(codeUnit <= std::numeric_limits<uint16_t>::max());
            transitions.append(WebKitStringMatcher::Transition { static_cast<uint16_t>(codeUnit), nodeToStateIndex.get(&child) });
        }
        size_t transitionsEnd = transitions.size();
        ASSERT(nodeToStateIndex.contains(&node));
        size_t stateIndex = nodeToStateIndex.get(&node) + node.identifiers.size();
        ASSERT(!states[stateIndex]);
        if (transitionsBegin > std::numeric_limits<uint16_t>::max() || transitionsEnd > std::numeric_limits<uint16_t>::max())
            return false;
        states[stateIndex] = WebKitStringMatcher::State {
            static_cast<uint16_t>(transitionsBegin),
            static_cast<uint16_t>(transitionsEnd)
        };
        return true;
    });
    if (!success)
        return std::nullopt;

#if ASSERT_ENABLED
    for (auto& state : states)
        ASSERT(state);
#endif

    return { { WTFMove(states), WTFMove(transitions) } };
}

std::optional<Vector<uint16_t>> WebKitStringMatcher::dataForMatchingStrings(const Vector<std::pair<String, uint16_t>>& vector)
{
    Trie trie(vector);
    auto dfa = trie.serialize();
    if (!dfa)
        return std::nullopt;

    size_t statesSize = dfa->states.size();
    size_t transitionsSize = dfa->transitions.size();
    RELEASE_ASSERT(statesSize <= std::numeric_limits<uint16_t>::max());
    RELEASE_ASSERT(transitionsSize <= std::numeric_limits<uint16_t>::max());

    Vector<uint16_t> data;

    constexpr size_t headerSize { 4 };
    data.reserveInitialCapacity(headerSize + statesSize * 2 + transitionsSize * 2);

    data.append(0);
    data.append(0);
    data.append(statesSize);
    data.append(transitionsSize);

    for (auto& state : dfa->states) {
        data.append(state->transitionsBeginIndexOrMatchIdentifier);
        data.append(state->transitionsEndIndexOrMatchSentinel);
    }
    for (auto& transition : dfa->transitions) {
        data.append(transition.codeUnitToCheck);
        data.append(transition.stateIndexToTransitionToIfMatched);
    }

    return { WTFMove(data) };
}

std::optional<std::pair<std::span<const WebKitStringMatcher::State>, std::span<const WebKitStringMatcher::Transition>>> WebKitStringMatcher::statesAndTransitionsFromVersionedData(std::span<const uint8_t> bytes)
{
#if CPU(BIG_ENDIAN)
    UNUSED_PARAM(bytes);
    return std::nullopt;
#else
    static_assert(sizeof(State) == sizeof(Transition));
    static_assert(sizeof(State) == sizeof(uint32_t));
    if (bytes.size() % sizeof(State))
        return std::nullopt;
    auto integers = spanReinterpretCast<const uint32_t>(bytes);
    constexpr uint32_t currentVersion { 0 };
    constexpr size_t headerLength { 2 };
    if (integers.size() < headerLength || integers[0] != currentVersion)
        return std::nullopt;
    uint32_t secondInteger = integers[1];
    size_t stateLength = static_cast<uint16_t>(secondInteger);
    size_t transitionLength = static_cast<uint16_t>(secondInteger >> 16);
    if (integers.size() != headerLength + stateLength + transitionLength)
        return std::nullopt;
    auto states = spanReinterpretCast<const State>(integers.subspan(headerLength, stateLength));
    auto transitions = spanReinterpretCast<const Transition>(integers.subspan(headerLength + stateLength));
    return { { states, transitions } };
#endif
}

}
