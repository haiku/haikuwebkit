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
#include "JSReadableStream.h"
#include "JSReadableStreamBYOBReader.h"
#include "JSReadableStreamDefaultReader.h"
#include "JSReadableStreamReadResult.h"
#include "JSReadableStreamSource.h"
#include "JSUnderlyingSource.h"
#include "QueuingStrategy.h"
#include "ReadableByteStreamController.h"
#include "ReadableStreamBYOBReader.h"
#include "ReadableStreamBYOBRequest.h"
#include "ScriptExecutionContext.h"
#include "Settings.h"
#include "StreamTeeUtilities.h"
#include "WritableStream.h"
#include <wtf/Compiler.h>

namespace WebCore {

// https://streams.spec.whatwg.org/#validate-and-normalize-high-water-mark
static inline ExceptionOr<double> extractHighWaterMark(const QueuingStrategy& strategy, double defaultValue)
{
    if (!strategy.highWaterMark)
        return defaultValue;
    auto highWaterMark = *strategy.highWaterMark;
    if (std::isnan(highWaterMark) || highWaterMark < 0)
        return Exception { ExceptionCode::RangeError, "highWaterMark value is invalid"_s };
    return highWaterMark;
}

static ExceptionOr<bool> isReadableByteSource(JSC::ThrowScope& throwScope, JSDOMGlobalObject& globalObject, JSC::JSValue underlyingSource)
{
    bool isNullOrUndefined = underlyingSource.isUndefinedOrNull();
    auto* object = isNullOrUndefined ? nullptr : underlyingSource.getObject();
    if (!object)
        return false;

    Ref vm = globalObject.vm();
    auto typeValue = object->get(&globalObject, JSC::Identifier::fromString(vm, "type"_s));
    RETURN_IF_EXCEPTION(throwScope, Exception { ExceptionCode::ExistingExceptionError });

    if (typeValue.isUndefined())
        return false;

    convert<IDLEnumeration<ReadableStreamType>>(globalObject, typeValue);
    RETURN_IF_EXCEPTION(throwScope, Exception { ExceptionCode::ExistingExceptionError });

    return true;
}

ExceptionOr<Ref<ReadableStream>> ReadableStream::create(JSDOMGlobalObject& globalObject, std::optional<JSC::Strong<JSC::JSObject>>&& underlyingSourceValue, std::optional<JSC::Strong<JSC::JSObject>>&& strategyValue)
{
    JSC::JSValue underlyingSource = JSC::jsUndefined();
    if (underlyingSourceValue)
        underlyingSource = underlyingSourceValue->get();

    JSC::JSValue strategy = JSC::jsUndefined();
    if (strategyValue)
        strategy = strategyValue->get();

    Ref vm = globalObject.vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    RefPtr context = globalObject.scriptExecutionContext();
    if (context->settingsValues().readableByteStreamAPIEnabled) {
        // FIXME: We convert strategy twice for regular readable streams.
        auto strategyDictOrException = convertDictionary<QueuingStrategy>(globalObject, strategy);
        RETURN_IF_EXCEPTION(throwScope, Exception { ExceptionCode::ExistingExceptionError });

        auto isByteSourceOrException = isReadableByteSource(throwScope, globalObject, underlyingSource);
        if (isByteSourceOrException.hasException())
            return isByteSourceOrException.releaseException();

        if (isByteSourceOrException.returnValue()) {
            auto underlyingSourceDictOrException = convertDictionary<UnderlyingSource>(globalObject, underlyingSource);
            RETURN_IF_EXCEPTION(throwScope, Exception { ExceptionCode::ExistingExceptionError });

            auto underlyingSourceDict = underlyingSourceDictOrException.releaseReturnValue();
            auto strategyDict = strategyDictOrException.releaseReturnValue();

            if (strategyDict.size)
                return Exception { ExceptionCode::RangeError, "size should not be present"_s };

            auto highWaterMarkOrException = extractHighWaterMark(strategyDict, 0);
            if (highWaterMarkOrException.hasException())
                return highWaterMarkOrException.releaseException();
            auto highWatermark = highWaterMarkOrException.releaseReturnValue();

            return createFromByteUnderlyingSource(globalObject, underlyingSource, WTFMove(underlyingSourceDict), highWatermark);
        }
    }

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

ExceptionOr<Ref<ReadableStream>> ReadableStream::createFromByteUnderlyingSource(JSDOMGlobalObject& globalObject, JSC::JSValue underlyingSource, UnderlyingSource&& underlyingSourceDict, double highWaterMark)
{
    Ref readableStream = adoptRef(*new ReadableStream());

    auto exception = readableStream->setupReadableByteStreamControllerFromUnderlyingSource(globalObject, underlyingSource, WTFMove(underlyingSourceDict), highWaterMark);
    if (exception.hasException())
        return exception.releaseException();

    return readableStream;
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

ReadableStream::ReadableStream(RefPtr<InternalReadableStream>&& internalReadableStream, RefPtr<ReadableStream>&& relatedStreamForGC)
    : m_internalReadableStream(WTFMove(internalReadableStream))
    , m_relatedStreamForGC(WTFMove(relatedStreamForGC))
{
}

ReadableStream::~ReadableStream() = default;

// https://streams.spec.whatwg.org/#rs-get-reader
ExceptionOr<ReadableStreamReader> ReadableStream::getReader(JSDOMGlobalObject& currentGlobalObject, const GetReaderOptions& options)
{
    if (!m_internalReadableStream) {
        ASSERT(m_controller);
        if (options.mode) {
            auto readerOrException = ReadableStreamBYOBReader::create(currentGlobalObject, *this);
            if (readerOrException.hasException())
                return readerOrException.releaseException();

            return ReadableStreamReader { RefPtr { readerOrException.releaseReturnValue() } };
        }

        auto readerOrException = ReadableStreamDefaultReader::create(currentGlobalObject, *this);
        if (readerOrException.hasException())
            return readerOrException.releaseException();

        return ReadableStreamReader { RefPtr { readerOrException.releaseReturnValue() } };
    }

    if (options.mode)
        return Exception { ExceptionCode::TypeError, "Invalid mode is specified"_s };

    auto* jsDOMGlobalObject = JSC::jsCast<JSDOMGlobalObject*>(m_internalReadableStream->globalObject());
    if (!jsDOMGlobalObject)
        return Exception { ExceptionCode::InvalidStateError, "No global object"_s };

    auto readerOrException = ReadableStreamDefaultReader::create(*jsDOMGlobalObject, *m_internalReadableStream);
    if (readerOrException.hasException())
        return readerOrException.releaseException();

    return ReadableStreamReader { RefPtr { readerOrException.releaseReturnValue() } };
}

// https://streams.spec.whatwg.org/#rs-tee
ExceptionOr<Vector<Ref<ReadableStream>>> ReadableStream::tee(JSDOMGlobalObject& globalObject, bool shouldClone)
{
    RefPtr internalReadableStream = m_internalReadableStream;
    if (!internalReadableStream) {
        ASSERT(m_controller);
        return byteStreamTee(globalObject, *this);
    }

    auto result = internalReadableStream->tee(shouldClone);
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
    return !!m_byobReader || !!m_defaultReader || (m_internalReadableStream && m_internalReadableStream->isLocked());
}

bool ReadableStream::isDisturbed() const
{
    return m_disturbed || (m_internalReadableStream && m_internalReadableStream->isDisturbed());
}

void ReadableStream::cancel(Exception&& exception)
{
    // FIXME: support byte stream.
    if (RefPtr internalReadableStream = m_internalReadableStream)
        internalReadableStream->cancel(WTFMove(exception));
}

void ReadableStream::pipeTo(ReadableStreamSink& sink)
{
    // FIXME: support byte stream.
    if (RefPtr internalReadableStream = m_internalReadableStream)
        internalReadableStream->pipeTo(sink);
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
    ASSERT(!m_byobReader);
    m_defaultReader = reader;
}

ReadableStreamDefaultReader* ReadableStream::defaultReader()
{
    return m_defaultReader.get();
}

// https://streams.spec.whatwg.org/#abstract-opdef-createreadablebytestream
Ref<ReadableStream> ReadableStream::createReadableByteStream(JSDOMGlobalObject& globalObject, ReadableByteStreamController::PullAlgorithm&& pullAlgorithm, ReadableByteStreamController::CancelAlgorithm&& cancelAlgorithm, RefPtr<ReadableStream>&& relatedStreamForGC)
{
    Ref readableStream = adoptRef(*new ReadableStream({ }, WTFMove(relatedStreamForGC)));
    readableStream->setupReadableByteStreamController(globalObject, WTFMove(pullAlgorithm), WTFMove(cancelAlgorithm), 0);
    return readableStream;
}

// https://streams.spec.whatwg.org/#readable-stream-fulfill-read-request
void ReadableStream::fulfillReadRequest(JSDOMGlobalObject& globalObject, RefPtr<JSC::ArrayBufferView>&& filledView, bool done)
{
    RefPtr defaultReader = this->defaultReader();
    ASSERT(defaultReader);
    ASSERT(defaultReader->getNumReadRequests());

    Ref readRequest = defaultReader->takeFirstReadRequest();

    if (done) {
        readRequest->runCloseSteps();
        return;
    }

    auto chunk = toJS<IDLNullable<IDLArrayBufferView>>(globalObject, globalObject, WTFMove(filledView));
    readRequest->runChunkSteps(chunk);
}

void ReadableStream::setByobReader(ReadableStreamBYOBReader* reader)
{
    ASSERT(!m_byobReader || !reader);
    ASSERT(!m_defaultReader);
    m_byobReader = reader;
}

ReadableStreamBYOBReader* ReadableStream::byobReader()
{
    return m_byobReader.get();
}

// https://streams.spec.whatwg.org/#readable-stream-fulfill-read-into-request
void ReadableStream::fulfillReadIntoRequest(JSDOMGlobalObject& globalObject, RefPtr<JSC::ArrayBufferView>&& filledView, bool done)
{
    RefPtr byobReader = this->byobReader();
    ASSERT(byobReader);
    ASSERT(byobReader->readIntoRequestsSize());

    Ref readRequest = byobReader->takeFirstReadIntoRequest();

    auto chunk = toJS<IDLNullable<IDLArrayBufferView>>(globalObject, globalObject, WTFMove(filledView));
    if (done) {
        readRequest->runCloseSteps(chunk);
        return;
    }

    readRequest->runChunkSteps(chunk);
}

ExceptionOr<void> ReadableStream::setupReadableByteStreamControllerFromUnderlyingSource(JSDOMGlobalObject& globalObject, JSC::JSValue underlyingSource, UnderlyingSource&& underlyingSourceDict, double highWaterMark)
{
    // handle start, pull, cancel algorithms.
    if (underlyingSourceDict.autoAllocateChunkSize && !*underlyingSourceDict.autoAllocateChunkSize)
        return Exception { ExceptionCode::TypeError, "autoAllocateChunkSize is zero"_s };

    // https://streams.spec.whatwg.org/#set-up-readable-byte-stream-controller
    ASSERT(!m_controller);
    lazyInitialize(m_controller, std::unique_ptr<ReadableByteStreamController>(new ReadableByteStreamController(*this, underlyingSource, WTFMove(underlyingSourceDict.pull), WTFMove(underlyingSourceDict.cancel), highWaterMark, underlyingSourceDict.autoAllocateChunkSize.value_or(0))));

    return m_controller->start(globalObject, underlyingSourceDict.start.get());
}

void ReadableStream::setupReadableByteStreamController(JSDOMGlobalObject& globalObject, ReadableByteStreamController::PullAlgorithm&& pullAlgorithm, ReadableByteStreamController::CancelAlgorithm&& cancelAlgorithm, double highWaterMark)
{
    lazyInitialize(m_controller, std::unique_ptr<ReadableByteStreamController>(new ReadableByteStreamController(*this, WTFMove(pullAlgorithm), WTFMove(cancelAlgorithm), highWaterMark, 0)));
    m_controller->start(globalObject, nullptr);
}

// https://streams.spec.whatwg.org/#readable-stream-close
void ReadableStream::close()
{
    ASSERT(m_state == ReadableStream::State::Readable);
    m_state = ReadableStream::State::Closed;

    if (RefPtr defaultReader = m_defaultReader.get()) {
        defaultReader->resolveClosedPromise();
        while (defaultReader->getNumReadRequests())
            defaultReader->takeFirstReadRequest()->runCloseSteps();
    } else if (RefPtr byobReader = m_byobReader.get())
        byobReader->resolveClosedPromise();
}

// https://streams.spec.whatwg.org/#readable-stream-error
void ReadableStream::error(JSDOMGlobalObject& globalObject, JSC::JSValue reason)
{
    ASSERT(m_state == ReadableStream::State::Readable);
    m_state = ReadableStream::State::Errored;

    m_controller->storeError(globalObject, reason);

    if (RefPtr defaultReader = m_defaultReader.get()) {
        defaultReader->rejectClosedPromise(reason);
        defaultReader->errorReadRequests(reason);
        return;
    }

    RefPtr byobReader = m_byobReader.get();
    if (!byobReader)
        return;

    byobReader->rejectClosedPromise(reason);
    byobReader->errorReadIntoRequests(reason);
}

// https://streams.spec.whatwg.org/#readable-stream-cancel
void ReadableStream::cancel(JSDOMGlobalObject& globalObject, JSC::JSValue reason, Ref<DeferredPromise>&& promise)
{
    ASSERT(!m_internalReadableStream);

    m_disturbed = true;
    if (m_state == State::Closed) {
        promise->resolve();
        return;
    }

    if (m_state == State::Errored) {
        promise->rejectWithCallback([&] (auto&) {
            return m_controller->storedError();
        });
        return;
    }

    close();

    RefPtr byobReader = m_byobReader.get();
    if (byobReader) {
        while (byobReader->readIntoRequestsSize())
            byobReader->takeFirstReadIntoRequest()->runCloseSteps(JSC::jsUndefined());
    }

    m_controller->runCancelSteps(globalObject, reason, [promise = WTFMove(promise)] (auto&& error) mutable {
        if (error) {
            promise->rejectWithCallback([&] (auto&) {
                return *error;
            });
            return;
        }
        promise->resolve();
    });
}

// https://streams.spec.whatwg.org/#readable-stream-get-num-read-into-requests
size_t ReadableStream::getNumReadIntoRequests() const
{
    ASSERT(m_byobReader);
    RefPtr byobReader = m_byobReader.get();
    return byobReader->readIntoRequestsSize();
}

// https://streams.spec.whatwg.org/#readable-stream-get-num-read-requests
size_t ReadableStream::getNumReadRequests() const
{
    ASSERT(m_defaultReader);
    RefPtr defaultReader = m_defaultReader.get();
    return defaultReader->getNumReadRequests();
}

// https://streams.spec.whatwg.org/#readable-stream-add-read-into-request
void ReadableStream::addReadIntoRequest(Ref<ReadableStreamReadIntoRequest>&& readRequest)
{
    ASSERT(m_byobReader);
    RefPtr byobReader = m_byobReader.get();
    return byobReader->addReadIntoRequest(WTFMove(readRequest));
}

// https://streams.spec.whatwg.org/#readable-stream-add-read-request
void ReadableStream::addReadRequest(Ref<ReadableStreamReadRequest>&& readRequest)
{
    ASSERT(m_defaultReader);
    RefPtr defaultReader = m_defaultReader.get();
    return defaultReader->addReadRequest(WTFMove(readRequest));
}

void ReadableStream::pipeTo(JSDOMGlobalObject&, WritableStream& destination, StreamPipeOptions&&, Ref<DeferredPromise>&& promise)
{
    if (isLocked()) {
        promise->reject(Exception { ExceptionCode::TypeError, "stream is locked"_s }, RejectAsHandled::Yes);
        return;
    }

    if (destination.locked()) {
        promise->reject(Exception { ExceptionCode::TypeError, "destination is locked"_s }, RejectAsHandled::Yes);
        return;
    }

    promise->reject(Exception { ExceptionCode::NotSupportedError, "not implemented"_s }, RejectAsHandled::Yes);
}

ExceptionOr<Ref<ReadableStream>> ReadableStream::pipeThrough(JSDOMGlobalObject&, WritablePair&& transform, StreamPipeOptions&&)
{
    if (isLocked())
        return Exception { ExceptionCode::TypeError, "stream is locked"_s };

    SUPPRESS_UNCOUNTED_ARG if (transform.writable->locked())
        return Exception { ExceptionCode::TypeError, "transform writable is locked"_s };

    return Exception { ExceptionCode::NotSupportedError, "not implemented"_s };
}

JSC::JSValue ReadableStream::storedError(JSDOMGlobalObject& globalObject) const
{
    if (RefPtr internalReadableStream = m_internalReadableStream)
        return internalReadableStream->storedError(globalObject);

    return m_controller->storedError();
}

JSC::JSValue JSReadableStream::cancel(JSC::JSGlobalObject& globalObject, JSC::CallFrame& callFrame)
{
    RefPtr internalReadableStream = wrapped().internalReadableStream();
    if (!internalReadableStream) {
        return callPromiseFunction(globalObject, callFrame, [this](auto& globalObject, auto& callFrame, auto&& promise) {
            Ref protectedWrapped = this->wrapped();
            if (protectedWrapped->isLocked()) {
                promise->reject(Exception { ExceptionCode::TypeError, "ReadableStream is locked"_s });
                return;
            }

            protectedWrapped->cancel(globalObject, callFrame.argument(0), WTFMove(promise));
        });
    }

    return internalReadableStream->cancelForBindings(globalObject, callFrame.argument(0));
}

JSC::JSValue JSReadableStream::pipeTo(JSC::JSGlobalObject& globalObject, JSC::CallFrame& callFrame)
{
    RefPtr internalReadableStream = wrapped().internalReadableStream();
    if (!internalReadableStream) {
        return callPromiseFunction(globalObject, callFrame, [](auto&, auto&, auto&& promise) {
            promise->reject(Exception { ExceptionCode::NotSupportedError, "piping to a byte stream is not yet supported"_s });
        });
    }

    return internalReadableStream->pipeTo(globalObject, callFrame.argument(0), callFrame.argument(1));
}

JSC::JSValue JSReadableStream::pipeThrough(JSC::JSGlobalObject& globalObject, JSC::CallFrame& callFrame)
{
    RefPtr internalReadableStream = wrapped().internalReadableStream();
    if (!internalReadableStream) {
        Ref vm = globalObject.vm();
        auto scope = DECLARE_THROW_SCOPE(vm);
        throwNotSupportedError(globalObject, scope, "piping through a byte stream is not yet supported"_s);
        return { };
    }

    return internalReadableStream->pipeThrough(globalObject, callFrame.argument(0), callFrame.argument(1));
}

void ReadableStream::visitAdditionalChildren(JSC::AbstractSlotVisitor& visitor)
{
    SUPPRESS_UNCOUNTED_ARG addWebCoreOpaqueRoot(visitor, m_byobReader.get());
    SUPPRESS_UNCOUNTED_ARG addWebCoreOpaqueRoot(visitor, m_defaultReader.get());

    if (m_relatedStreamForGC)
        m_relatedStreamForGC->visitAdditionalChildren(visitor);

    if (m_controller) {
        m_controller->underlyingSourceConcurrently().visit(visitor);
        m_controller->storedErrorConcurrently().visit(visitor);
    }
}

template<typename Visitor>
void JSReadableStream::visitAdditionalChildren(Visitor& visitor)
{
    SUPPRESS_UNCOUNTED_ARG wrapped().visitAdditionalChildren(visitor);
}

DEFINE_VISIT_ADDITIONAL_CHILDREN(JSReadableStream);

} // namespace WebCore
