/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 *
 */

#pragma once

#include <wtf/HashMap.h>
#include <wtf/Ref.h>
#include <wtf/Seconds.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakRef.h>
#include <wtf/text/AtomString.h>
#include <wtf/text/AtomStringHash.h>

namespace WebCore {

class CSSVariableData;
class Document;
class WeakPtrImplWithEventTargetData;

namespace Style {
class CustomProperty;
}

enum class ConstantProperty {
    SafeAreaInsetTop,
    SafeAreaInsetRight,
    SafeAreaInsetBottom,
    SafeAreaInsetLeft,
    FullscreenInsetTop,
    FullscreenInsetRight,
    FullscreenInsetBottom,
    FullscreenInsetLeft,
    FullscreenAutoHideDuration,
};

class ConstantPropertyMap {
    WTF_MAKE_TZONE_ALLOCATED(ConstantPropertyMap);
public:
    explicit ConstantPropertyMap(Document&);

    using Values = HashMap<AtomString, Ref<const Style::CustomProperty>>;
    const Values& values() const;

    void didChangeSafeAreaInsets();
    void didChangeFullscreenInsets();
    void setFullscreenAutoHideDuration(Seconds);

private:
    void buildValues();

    const AtomString& nameForProperty(ConstantProperty) const;
    void setValueForProperty(ConstantProperty, Ref<CSSVariableData>&&);

    void updateConstantsForSafeAreaInsets();
    void updateConstantsForFullscreen();

    Ref<Document> protectedDocument() const;

    std::optional<Values> m_values;

    WeakRef<Document, WeakPtrImplWithEventTargetData> m_document;
};

} // namespace WebCore
