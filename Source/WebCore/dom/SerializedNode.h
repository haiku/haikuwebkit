/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "SecurityOriginData.h"
#include <wtf/text/WTFString.h>

namespace JSC {
class JSGlobalObject;
class JSValue;
}

namespace WebCore {

class Document;
class JSDOMGlobalObject;

enum class ClonedDocumentType : uint8_t;

struct SerializedNode {
    struct Attr {
        String prefix;
        String localName;
        String namespaceURI;
        String value;
    };
    struct ContainerNode {
        Vector<SerializedNode> children;
    };
    struct Document : public ContainerNode {
        ClonedDocumentType type;
        URL url;
        URL baseURL;
        URL baseURLOverride;
        Variant<String, URL> documentURI;
        String contentType;
    };
    struct DocumentFragment : public ContainerNode {
        // FIXME: Implement.
    };
    struct DocumentType {
        String name;
        String publicId;
        String systemId;
    };
    struct Element : public ContainerNode {
        // FIXME: Implement.
    };
    struct ShadowRoot : public DocumentFragment {
        // FIXME: Implement.
    };
    struct HTMLTemplateElement : public Element {
        // FIXME: Implement.
    };
    struct CharacterData {
        String data;
    };
    struct Comment : public CharacterData { };
    struct Text : public CharacterData { };
    struct CDATASection : public Text { };
    struct ProcessingInstruction : public CharacterData {
        String target;
    };

    Variant<Attr, CDATASection, Comment, Document, DocumentFragment, DocumentType, Element, ProcessingInstruction, ShadowRoot, Text, HTMLTemplateElement> data;

    WEBCORE_EXPORT static JSC::JSValue deserialize(SerializedNode&&, JSC::JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject*, WebCore::Document&);
};

}
