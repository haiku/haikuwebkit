/*
 * Copyright (C) 2013-2023 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(VIDEO)

#include "HTMLElement.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

enum WebVTTNodeType {
    WebVTTNodeTypeNone = 0,
    WebVTTNodeTypeClass,
    WebVTTNodeTypeItalic,
    WebVTTNodeTypeLanguage,
    WebVTTNodeTypeBold,
    WebVTTNodeTypeUnderline,
    WebVTTNodeTypeRuby,
    WebVTTNodeTypeRubyText,
    WebVTTNodeTypeVoice
};

class WebVTTElement final : public Element {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(WebVTTElement);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(WebVTTElement);
public:
    static Ref<Element> create(const WebVTTNodeType, AtomString language, Document&);
    Ref<HTMLElement> createEquivalentHTMLElement(Document&);

    Ref<Element> cloneElementWithoutAttributesAndChildren(Document&, CustomElementRegistry*);

    void setWebVTTNodeType(WebVTTNodeType type) { m_webVTTNodeType = type; }
    WebVTTNodeType webVTTNodeType() const { return m_webVTTNodeType; }

    bool isPastNode() const { return m_isPastNode; }
    void setIsPastNode(bool value) { m_isPastNode = value; }

    AtomString language() const { return m_language; }
    void setLanguage(const AtomString& value) { m_language = value; }

    static const QualifiedName& voiceAttributeName()
    {
        static NeverDestroyed<QualifiedName> voiceAttr(nullAtom(), "voice"_s, nullAtom());
        return voiceAttr;
    }

    static const QualifiedName& langAttributeName()
    {
        static NeverDestroyed<QualifiedName> voiceAttr(nullAtom(), "lang"_s, nullAtom());
        return voiceAttr;
    }

protected:
    WebVTTElement(WebVTTNodeType, AtomString language, Document&);

    bool isWebVTTElement() const final { return true; }

    bool m_isPastNode { false };
    WebVTTNodeType m_webVTTNodeType;
    AtomString m_language;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::WebVTTElement)
    static bool isType(const WebCore::Node& node) { return node.isWebVTTElement(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(VIDEO)
