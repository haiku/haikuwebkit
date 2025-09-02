/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "CharacterRange.h"
#include "FloatRect.h"
#include "FloatSize.h"
#include <wtf/Forward.h>
#include <wtf/URL.h>

namespace WebCore {
namespace TextExtraction {

enum class EventListenerCategory : uint16_t {
    Click       = 1 << 0,
    Hover       = 1 << 1,
    Touch       = 1 << 2,
    Wheel       = 1 << 3,
    Gesture     = 1 << 4,
    Pointer     = 1 << 5,
    Keyboard    = 1 << 6,
    Focus       = 1 << 7,
    Form        = 1 << 8,
    Media       = 1 << 9,
};

struct Editable {
    String label;
    String placeholder;
    bool isSecure { false };
    bool isFocused { false };
};

struct TextItemData {
    Vector<std::pair<URL, CharacterRange>> links;
    std::optional<CharacterRange> selectedRange;
    String content;
    std::optional<Editable> editable;
};

struct ScrollableItemData {
    FloatSize contentSize;
};

struct ImageItemData {
    String name;
    String altText;
};

enum class ContainerType : uint8_t {
    Root,
    ViewportConstrained,
    List,
    ListItem,
    BlockQuote,
    Article,
    Section,
    Nav,
    Button,
};

using ItemData = Variant<ContainerType, TextItemData, ScrollableItemData, ImageItemData>;

struct Item {
    ItemData data;
    FloatRect rectInRootView;
    Vector<Item> children;
};

} // namespace TextExtraction
} // namespace WebCore
