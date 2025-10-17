/*
 * Copyright (C) 2022-2025 Apple Inc. All rights reserved.
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

#include "InternalReadableStream.h"
#include <JavaScriptCore/Strong.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class DeferredPromise;
class InternalReadableStream;
class JSDOMGlobalObject;
class ReadableStreamDefaultReader;
class ReadableStreamSource;
class WritableStream;

struct UnderlyingSource;

using ReadableStreamReader = ReadableStreamDefaultReader;

class ReadableStream : public RefCountedAndCanMakeWeakPtr<ReadableStream> {
public:
    enum class ReaderMode { Byob };
    struct GetReaderOptions {
        std::optional<ReaderMode> mode;
    };
    struct WritablePair {
        RefPtr<ReadableStream> readable;
        RefPtr<WritableStream> writable;
    };
    
    static ExceptionOr<Ref<ReadableStream>> create(JSC::JSGlobalObject&, std::optional<JSC::Strong<JSC::JSObject>>&&, std::optional<JSC::Strong<JSC::JSObject>>&&);
    static ExceptionOr<Ref<ReadableStream>> create(JSDOMGlobalObject&, Ref<ReadableStreamSource>&&);
    static ExceptionOr<Ref<ReadableStream>> createFromByteUnderlyingSource(JSDOMGlobalObject&, JSC::JSValue underlyingSource, UnderlyingSource&&, double highWaterMark);
    static Ref<ReadableStream> create(Ref<InternalReadableStream>&&);
    
    virtual ~ReadableStream();
    
    void cancel(JSDOMGlobalObject&, JSC::JSValue, Ref<DeferredPromise>&&);
    ExceptionOr<Ref<ReadableStreamReader>> getReader(const GetReaderOptions&);
    ExceptionOr<Vector<Ref<ReadableStream>>> tee(bool shouldClone = false);
    
    using State = InternalReadableStream::State;
    State state() const;
    
    void lock();
    bool isLocked() const;
    WEBCORE_EXPORT bool isDisturbed() const;
    
    void cancel(Exception&&);
    
    InternalReadableStream* internalReadableStream() { return m_internalReadableStream.get(); }
    
    void setDefaultReader(ReadableStreamDefaultReader*);
    ReadableStreamDefaultReader* defaultReader();
    
    void markAsDisturbed() { m_disturbed = true; }
    
    void close();
    void pipeTo(ReadableStreamSink& sink) { m_internalReadableStream->pipeTo(sink); }
    
    JSC::JSValue storedError(JSDOMGlobalObject&) const;
    
    size_t getNumReadRequests() const;
    void addReadRequest(Ref<DeferredPromise>&&);
    
protected:
    static ExceptionOr<Ref<ReadableStream>> createFromJSValues(JSC::JSGlobalObject&, JSC::JSValue, JSC::JSValue);
    static ExceptionOr<Ref<InternalReadableStream>> createInternalReadableStream(JSDOMGlobalObject&, Ref<ReadableStreamSource>&&);
    explicit ReadableStream(RefPtr<InternalReadableStream>&&);
    
private:
    bool m_disturbed { false };
    WeakPtr<ReadableStreamDefaultReader> m_defaultReader;
    State m_state { State::Readable };
    
    const RefPtr<InternalReadableStream> m_internalReadableStream;
};

} // namespace WebCore
