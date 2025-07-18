/*
 * Copyright (C) 2019-2020 Apple Inc. All rights reserved.
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
#include "HighlightRegistry.h"

#include "IDLTypes.h"
#include "JSDOMMapLike.h"
#include "JSHighlight.h"

namespace WebCore {
    
void HighlightRegistry::initializeMapLike(DOMMapAdapter& map)
{
    for (auto& keyValue : m_map)
        map.set<IDLDOMString, IDLInterface<Highlight>>(keyValue.key, keyValue.value);
}

void HighlightRegistry::setFromMapLike(AtomString&& key, Ref<Highlight>&& value)
{
    auto addResult = m_map.set(key, WTFMove(value));
    if (addResult.isNewEntry) {
        ASSERT(!m_highlightNames.contains(key));
        m_highlightNames.append(WTFMove(key));
    }
}

void HighlightRegistry::clear()
{
    m_highlightNames.clear();
    auto map = std::exchange(m_map, { });
    for (auto& highlight : map.values())
        highlight->clearFromSetLike();
}

bool HighlightRegistry::remove(const AtomString& key)
{
    m_highlightNames.removeFirst(key);
    return m_map.remove(key);
}

#if ENABLE(APP_HIGHLIGHTS)
void HighlightRegistry::setHighlightVisibility(HighlightVisibility highlightVisibility)
{
    if (m_highlightVisibility == highlightVisibility)
        return;
    
    m_highlightVisibility = highlightVisibility;
    
    for (auto& highlight : m_map)
        Ref { highlight.value }->repaint();
}
#endif

static ASCIILiteral annotationHighlightKey()
{
    return "annotationHighlightKey"_s;
}

void HighlightRegistry::addAnnotationHighlightWithRange(Ref<StaticRange>&& value)
{
    if (m_map.contains(annotationHighlightKey()))
        Ref { *m_map.get(annotationHighlightKey()) }->addToSetLike(value);
    else
        setFromMapLike(annotationHighlightKey(), Highlight::create({ std::ref<AbstractRange>(value.get()) }));
}

}
