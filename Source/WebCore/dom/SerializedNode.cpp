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
#include "SerializedNode.h"

#include "Attr.h"
#include "CDATASection.h"
#include "Comment.h"
#include "DocumentType.h"
#include "JSNode.h"
#include "ProcessingInstruction.h"
#include "SecurityOriginPolicy.h"
#include "Text.h"

namespace WebCore {

JSC::JSValue SerializedNode::deserialize(SerializedNode&& serializedNode, JSC::JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject* domGlobalObject, WebCore::Document& document)
{
    // FIXME: Support other kinds of nodes and change RefPtr to Ref.
    RefPtr node = WTF::switchOn(WTFMove(serializedNode.data), [&] (SerializedNode::Text&& text) -> RefPtr<Node> {
        return WebCore::Text::create(document, WTFMove(text.data));
    }, [&] (SerializedNode::ProcessingInstruction&& instruction) -> RefPtr<Node> {
        return WebCore::ProcessingInstruction::create(document, WTFMove(instruction.target), WTFMove(instruction.data));
    }, [&] (SerializedNode::DocumentType&& type) -> RefPtr<Node> {
        return WebCore::DocumentType::create(document, type.name, type.publicId, type.systemId);
    }, [&] (SerializedNode::Comment&& comment) -> RefPtr<Node> {
        return WebCore::Comment::create(document, WTFMove(comment.data));
    }, [&] (SerializedNode::CDATASection&& section) -> RefPtr<Node> {
        return WebCore::CDATASection::create(document, WTFMove(section.data));
    }, [&] (SerializedNode::Attr&& attr) -> RefPtr<Node> {
        QualifiedName name(AtomString(WTFMove(attr.prefix)), AtomString(WTFMove(attr.localName)), AtomString(WTFMove(attr.namespaceURI)));
        return WebCore::Attr::create(document, name, AtomString(WTFMove(attr.value)));
    }, [&] (SerializedNode::Document&& serializedDocument) -> RefPtr<Node> {
        return WebCore::Document::createCloned(
            serializedDocument.type,
            document.settings(),
            serializedDocument.url,
            serializedDocument.baseURL,
            serializedDocument.baseURLOverride,
            serializedDocument.documentURI,
            document.compatibilityMode(),
            document,
            RefPtr { document.securityOriginPolicy() }.get(),
            serializedDocument.contentType,
            document.protectedDecoder().get()
        );
    }, [] (auto&&) -> RefPtr<Node> {
        return nullptr;
    });
    return toJSNewlyCreated(lexicalGlobalObject, domGlobalObject, WTFMove(node));
}

}
