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

#include "WKRetainPtr.h"
#include <optional>

#if PLATFORM(COCOA)
#include <wtf/RetainPtr.h>
#endif

namespace API {
class SerializedScriptValue;
}

namespace WebCore {
struct ExceptionDetails;
}

namespace WebKit {

class JavaScriptEvaluationResult {
public:
    JavaScriptEvaluationResult(std::span<const uint8_t> wireBytes)
        : m_wireBytes(wireBytes) { }
    JavaScriptEvaluationResult(JavaScriptEvaluationResult&&) = default;
    JavaScriptEvaluationResult& operator=(JavaScriptEvaluationResult&&) = default;

    std::span<const uint8_t> wireBytes() const { return m_wireBytes; }

#if PLATFORM(COCOA)
    RetainPtr<id> toID() const;
#endif
    WKRetainPtr<WKTypeRef> toWK() const;

    Ref<API::SerializedScriptValue> legacySerializedScriptValue() const;
private:
    std::span<const uint8_t> m_wireBytes;
};

}

namespace IPC {

template<typename> struct AsyncReplyError;
template<> struct AsyncReplyError<Expected<WebKit::JavaScriptEvaluationResult, std::optional<WebCore::ExceptionDetails>>> {
    static Expected<WebKit::JavaScriptEvaluationResult, std::optional<WebCore::ExceptionDetails>> create();
};

}
