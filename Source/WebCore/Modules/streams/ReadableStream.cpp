/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "ReadableStream.h"

#include "JSDOMPromise.h"
#include "JSDOMPromiseDeferred.h"
#include "JSQueuingStrategy.h"
#include "JSReadableStream.h"
#include "JSReadableStreamReadResult.h"
#include "JSReadableStreamSource.h"
#include "JSUnderlyingSource.h"
#include "ReadableStreamDefaultReader.h"
#include "ScriptExecutionContext.h"
#include "WritableStream.h"

namespace WebCore {

ExceptionOr<Ref<ReadableStream>> ReadableStream::create(JSC::JSGlobalObject& globalObject, std::optional<JSC::Strong<JSC::JSObject>>&& underlyingSourceValue, std::optional<JSC::Strong<JSC::JSObject>>&& strategyValue)
{
    JSC::JSValue underlyingSource = JSC::jsUndefined();
    if (underlyingSourceValue)
        underlyingSource = underlyingSourceValue->get();

    JSC::JSValue strategy = JSC::jsUndefined();
    if (strategyValue)
        strategy = strategyValue->get();

    // FIXME: We convert strategy twice for regular readable streams.
    auto throwScope = DECLARE_THROW_SCOPE(globalObject.vm());
    auto strategyDictOrException = convertDictionary<QueuingStrategy>(globalObject, strategy);
    if (strategyDictOrException.hasException(throwScope))
        return Exception { ExceptionCode::ExistingExceptionError };

    return createFromJSValues(globalObject, underlyingSource, strategy);
}

ExceptionOr<Ref<ReadableStream>> ReadableStream::createFromJSValues(JSC::JSGlobalObject& globalObject, JSC::JSValue underlyingSource, JSC::JSValue strategy)
{
    auto& jsDOMGlobalObject = *JSC::jsCast<JSDOMGlobalObject*>(&globalObject);
    RefPtr protectedContext { jsDOMGlobalObject.scriptExecutionContext() };
    auto result = InternalReadableStream::createFromUnderlyingSource(jsDOMGlobalObject, underlyingSource, strategy);
    if (result.hasException())
        return result.releaseException();

    return adoptRef(*new ReadableStream(result.releaseReturnValue()));
}

ExceptionOr<Ref<InternalReadableStream>> ReadableStream::createInternalReadableStream(JSDOMGlobalObject& globalObject, Ref<ReadableStreamSource>&& source)
{
    return InternalReadableStream::createFromUnderlyingSource(globalObject, toJSNewlyCreated(&globalObject, &globalObject, WTFMove(source)), JSC::jsUndefined());
}

ExceptionOr<Ref<ReadableStream>> ReadableStream::create(JSDOMGlobalObject& globalObject, Ref<ReadableStreamSource>&& source)
{
    return createFromJSValues(globalObject, toJSNewlyCreated(&globalObject, &globalObject, WTFMove(source)), JSC::jsUndefined());
}

Ref<ReadableStream> ReadableStream::create(Ref<InternalReadableStream>&& internalReadableStream)
{
    return adoptRef(*new ReadableStream(WTFMove(internalReadableStream)));
}

ReadableStream::ReadableStream(RefPtr<InternalReadableStream>&& internalReadableStream)
    : m_internalReadableStream(WTFMove(internalReadableStream))
{
}

ReadableStream::~ReadableStream() = default;

// https://streams.spec.whatwg.org/#rs-cancel
void ReadableStream::cancel(Exception&& exception)
{
    if (RefPtr internalReadableStream = m_internalReadableStream)
        internalReadableStream->cancel(WTFMove(exception));
}

// https://streams.spec.whatwg.org/#rs-get-reader
ExceptionOr<Ref<ReadableStreamReader>> ReadableStream::getReader(const GetReaderOptions& options)
{
    if (options.mode)
        return Exception { ExceptionCode::TypeError, "Invalid mode is specified"_s };

    auto* jsDOMGlobalObject = JSC::jsCast<JSDOMGlobalObject*>(m_internalReadableStream->globalObject());
    if (!jsDOMGlobalObject)
        return Exception { ExceptionCode::InvalidStateError, "No global object"_s };

    return ReadableStreamDefaultReader::create(*jsDOMGlobalObject, *m_internalReadableStream);
}

// https://streams.spec.whatwg.org/#rs-tee
ExceptionOr<Vector<Ref<ReadableStream>>> ReadableStream::tee(bool shouldClone)
{
    auto result = m_internalReadableStream->tee(shouldClone);
    if (result.hasException())
        return result.releaseException();

    auto pair = result.releaseReturnValue();

    return Vector {
        ReadableStream::create(WTFMove(pair.first)),
        ReadableStream::create(WTFMove(pair.second))
    };
}

void ReadableStream::lock()
{
    ASSERT(m_internalReadableStream);
    if (RefPtr internalReadableStream = m_internalReadableStream)
        internalReadableStream->lock();
}

// https://streams.spec.whatwg.org/#is-readable-stream-locked
bool ReadableStream::isLocked() const
{
    return !!m_defaultReader || (m_internalReadableStream && m_internalReadableStream->isLocked());
}

bool ReadableStream::isDisturbed() const
{
    return m_disturbed || (m_internalReadableStream && m_internalReadableStream->isDisturbed());
}

ReadableStream::State ReadableStream::state() const
{
    if (RefPtr internalReadableStream = m_internalReadableStream)
        return internalReadableStream->state();

    return m_state;
}

void ReadableStream::setDefaultReader(ReadableStreamDefaultReader* reader)
{
    ASSERT(!m_defaultReader || !reader);
    m_defaultReader = reader;
}

ReadableStreamDefaultReader* ReadableStream::defaultReader()
{
    return m_defaultReader.get();
}

// https://streams.spec.whatwg.org/#readable-stream-close
void ReadableStream::close()
{
    ASSERT(m_state == ReadableStream::State::Readable);
    m_state = ReadableStream::State::Closed;

    if (RefPtr defaultReader = m_defaultReader.get()) {
        defaultReader->resolveClosedPromise();
        while (defaultReader->getNumReadRequests())
            defaultReader->takeFirstReadRequest()->resolve<IDLDictionary<ReadableStreamReadResult>>({ JSC::jsUndefined(), true });
    }
}

// https://streams.spec.whatwg.org/#readable-stream-cancel
void ReadableStream::cancel(JSDOMGlobalObject&, JSC::JSValue, Ref<DeferredPromise>&&)
{
    // FIXME: Add support
}

// https://streams.spec.whatwg.org/#readable-stream-get-num-read-requests
size_t ReadableStream::getNumReadRequests() const
{
    ASSERT(m_defaultReader);
    RefPtr defaultReader = m_defaultReader.get();
    return defaultReader->getNumReadRequests();
}

// https://streams.spec.whatwg.org/#readable-stream-add-read-request
void ReadableStream::addReadRequest(Ref<DeferredPromise>&& promise)
{
    ASSERT(m_defaultReader);
    RefPtr defaultReader = m_defaultReader.get();
    return defaultReader->addReadRequest(WTFMove(promise));
}

JSC::JSValue ReadableStream::storedError(JSDOMGlobalObject& globalObject) const
{
    return m_internalReadableStream->storedError(globalObject);
}

JSC::JSValue JSReadableStream::cancel(JSC::JSGlobalObject& globalObject, JSC::CallFrame& callFrame)
{
    RefPtr internalReadableStream = wrapped().internalReadableStream();
    return internalReadableStream->cancelForBindings(globalObject, callFrame.argument(0));
}

JSC::JSValue JSReadableStream::pipeTo(JSC::JSGlobalObject& globalObject, JSC::CallFrame& callFrame)
{
    RefPtr internalReadableStream = wrapped().internalReadableStream();
    return internalReadableStream->pipeTo(globalObject, callFrame.argument(0), callFrame.argument(1));
}

JSC::JSValue JSReadableStream::pipeThrough(JSC::JSGlobalObject& globalObject, JSC::CallFrame& callFrame)
{
    RefPtr internalReadableStream = wrapped().internalReadableStream();
    return internalReadableStream->pipeThrough(globalObject, callFrame.argument(0), callFrame.argument(1));
}

} // namespace WebCore
