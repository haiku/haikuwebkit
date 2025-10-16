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
#include "WebKitJSHandle.h"

#include "DOMWindow.h"
#include "Document.h"
#include "Frame.h"
#include "JSWindowProxy.h"
#include <JavaScriptCore/JSCellInlines.h>
#include <JavaScriptCore/JSObject.h>

namespace WebCore {

// FIXME: Make PairHashTraits work with PeekTypes and use a map<identifier, pair> instead of 2 hash lookups.
using HandleMap = HashMap<JSHandleIdentifier, JSC::Strong<JSC::JSObject>>;
static HandleMap& handleMap()
{
    static MainThreadNeverDestroyed<HandleMap> map;
    return map.get();
}

using DocumentMap = HashMap<JSHandleIdentifier, WeakPtr<Document, WeakPtrImplWithEventTargetData>>;
static DocumentMap& documentMap()
{
    static MainThreadNeverDestroyed<DocumentMap> map;
    return map.get();
}

void WebKitJSHandle::jsHandleDestroyed(JSHandleIdentifier identifier)
{
    handleMap().remove(identifier);
    documentMap().remove(identifier);
}

std::pair<RefPtr<Document>, JSC::JSObject*> WebKitJSHandle::objectForIdentifier(JSHandleIdentifier identifier)
{
    return { documentMap().get(identifier).get(), handleMap().get(identifier).get() };
}

static Markable<FrameIdentifier> windowFrameIdentifier(JSC::JSObject* object)
{
    if (auto* window = jsDynamicCast<WebCore::JSWindowProxy*>(object)) {
        if (RefPtr frame = window->protectedWrapped()->frame())
            return frame->frameID();
    }
    return std::nullopt;
}

WebKitJSHandle::WebKitJSHandle(Document& document, JSC::JSObject* object)
    : m_identifier(JSHandleIdentifier::generate())
    , m_windowFrameIdentifier(WebCore::windowFrameIdentifier(object))
{
    handleMap().add(m_identifier, JSC::Strong<JSC::JSObject> { document.vm(), object });
    documentMap().add(m_identifier, WeakPtr { document });
}

}
