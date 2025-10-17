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
#include "ReadableStreamDefaultReader.h"

#include "JSDOMPromise.h"
#include "JSDOMPromiseDeferred.h"
#include "JSReadableStreamDefaultReader.h"
#include "JSReadableStreamReadResult.h"
#include "ReadableByteStreamController.h"
#include "ReadableStream.h"
#include "ReadableStreamReadRequest.h"
#include "WebCoreOpaqueRootInlines.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(ReadableStreamDefaultReader);

ExceptionOr<Ref<ReadableStreamDefaultReader>> ReadableStreamDefaultReader::create(JSDOMGlobalObject& globalObject, ReadableStream& stream)
{
    RefPtr internalReadableStream = stream.internalReadableStream();
    if (!internalReadableStream) {
        ASSERT(stream.hasByteStreamController());

        auto [promise, deferred] = createPromiseAndWrapper(globalObject);
        Ref reader = adoptRef(*new ReadableStreamDefaultReader(stream, WTFMove(promise), WTFMove(deferred)));
        auto result = reader->setup(globalObject);
        if (result.hasException())
            return result.releaseException();
        return reader;
    }

    return create(globalObject, internalReadableStream.releaseNonNull());
}

ExceptionOr<Ref<ReadableStreamDefaultReader>> ReadableStreamDefaultReader::create(JSDOMGlobalObject& globalObject, InternalReadableStream& stream)
{
    auto internalReaderOrException = InternalReadableStreamDefaultReader::create(globalObject, stream);
    if (internalReaderOrException.hasException())
        return internalReaderOrException.releaseException();

    auto [promise, deferred] = createPromiseAndWrapper(globalObject);
    return create(internalReaderOrException.releaseReturnValue(), WTFMove(promise), WTFMove(deferred));
}

Ref<ReadableStreamDefaultReader> ReadableStreamDefaultReader::create(Ref<InternalReadableStreamDefaultReader>&& internalDefaultReader, Ref<DOMPromise>&& promise, Ref<DeferredPromise>&& deferred)
{
    return adoptRef(*new ReadableStreamDefaultReader(WTFMove(internalDefaultReader), WTFMove(promise), WTFMove(deferred)));
}

ReadableStreamDefaultReader::ReadableStreamDefaultReader(Ref<InternalReadableStreamDefaultReader>&& internalDefaultReader, Ref<DOMPromise>&& promise, Ref<DeferredPromise>&& deferred)
    : m_closedPromise(WTFMove(promise))
    , m_closedDeferred(WTFMove(deferred))
    , m_internalDefaultReader(WTFMove(internalDefaultReader))
{
}

ReadableStreamDefaultReader::ReadableStreamDefaultReader(Ref<ReadableStream>&& stream, Ref<DOMPromise>&& promise, Ref<DeferredPromise>&& deferred)
    : m_closedPromise(WTFMove(promise))
    , m_closedDeferred(WTFMove(deferred))
    , m_stream(WTFMove(stream))
{
    ASSERT(m_stream->hasByteStreamController());
}

ReadableStreamDefaultReader::~ReadableStreamDefaultReader() = default;

// https://streams.spec.whatwg.org/#generic-reader-closed
DOMPromise& ReadableStreamDefaultReader::closedPromise() const
{
    return m_closedPromise;
}

// https://streams.spec.whatwg.org/#default-reader-read
void ReadableStreamDefaultReader::readForBindings(JSDOMGlobalObject& globalObject, Ref<DeferredPromise>&& promise)
{
    read(globalObject, ReadableStreamReadRequest::create(WTFMove(promise)));
}

void ReadableStreamDefaultReader::read(JSDOMGlobalObject& globalObject, Ref<ReadableStreamReadRequest>&& readRequest)
{
    RefPtr stream = m_stream;
    if (!stream) {
        readRequest->runErrorSteps(Exception { ExceptionCode::TypeError, "stream is undefined"_s });
        return;
    }

    // https://streams.spec.whatwg.org/#readable-stream-default-reader-read
    ASSERT(stream->defaultReader() == this);
    ASSERT(stream->hasByteStreamController());

    stream->markAsDisturbed();
    switch (stream->state()) {
    case ReadableStream::State::Closed:
        readRequest->runCloseSteps();
        break;
    case ReadableStream::State::Errored:
        readRequest->runErrorSteps(stream->storedError(globalObject));
        break;
    case ReadableStream::State::Readable:
        RefPtr { stream->controller() }->runPullSteps(globalObject, WTFMove(readRequest));
    }
}

// https://streams.spec.whatwg.org/#default-reader-release-lock
ExceptionOr<void> ReadableStreamDefaultReader::releaseLock(JSDOMGlobalObject& globalObject)
{
    if (RefPtr internalReader = this->internalDefaultReader())
        return internalReader->releaseLock();

    genericRelease(globalObject);
    errorReadRequests(globalObject, Exception { ExceptionCode::TypeError, "lock released"_s });
    return { };
}

// https://streams.spec.whatwg.org/#set-up-readable-stream-default-reader
ExceptionOr<void> ReadableStreamDefaultReader::setup(JSDOMGlobalObject& globalObject)
{
    RefPtr stream = m_stream;

    if (stream->isLocked())
        return Exception { ExceptionCode::TypeError, "ReadableStream is locked"_s };

    stream->setDefaultReader(this);

    switch (stream->state()) {
    case ReadableStream::State::Readable:
        break;
    case ReadableStream::State::Closed:
        resolveClosedPromise();
        break;
    case ReadableStream::State::Errored:
        rejectClosedPromise(stream->storedError(globalObject));
        break;
    }

    return { };
}

// https://streams.spec.whatwg.org/#readable-stream-reader-generic-release
void ReadableStreamDefaultReader::genericRelease(JSDOMGlobalObject& globalObject)
{
    RefPtr stream = m_stream;

    ASSERT(stream);
    ASSERT(stream->defaultReader() == this);

    if (stream->state() == ReadableStream::State::Readable)
        Ref { m_closedDeferred }->reject(Exception { ExceptionCode::TypeError, "releasing stream"_s }, RejectAsHandled::Yes);
    else {
        auto [promise, deferred] = createPromiseAndWrapper(globalObject);
        deferred->reject(Exception { ExceptionCode::TypeError, "releasing stream"_s }, RejectAsHandled::Yes);
        m_closedDeferred = WTFMove(deferred);
        m_closedPromise = WTFMove(promise);
    }

    if (RefPtr controller = m_stream->controller())
        controller->runReleaseSteps();

    stream->setDefaultReader(nullptr);
    m_stream = nullptr;
}

// https://streams.spec.whatwg.org/#abstract-opdef-readablestreamdefaultreadererrorreadrequests
void ReadableStreamDefaultReader::errorReadRequests(JSDOMGlobalObject& globalObject, const Exception& exception)
{
    UNUSED_PARAM(globalObject);
    auto readRequests = std::exchange(m_readRequests, { });
    for (auto& readRequest : readRequests)
        readRequest->runErrorSteps(Exception { exception });
}

// https://streams.spec.whatwg.org/#readable-stream-reader-generic-cancel
void ReadableStreamDefaultReader::genericCancel(JSDOMGlobalObject& globalObject, JSC::JSValue value, Ref<DeferredPromise>&& promise)
{
    RefPtr stream = m_stream;

    ASSERT(stream);
    ASSERT(stream->defaultReader() == this);

    stream->cancel(globalObject, value, WTFMove(promise));
}

// https://streams.spec.whatwg.org/#abstract-opdef-readablestreamdefaultreadererrorreadrequests
void ReadableStreamDefaultReader::errorReadRequests(JSC::JSValue reason)
{
    auto readRequests = std::exchange(m_readRequests, { });
    for (auto& request : readRequests)
        request->runErrorSteps(reason);
}

void ReadableStreamDefaultReader::addReadRequest(Ref<ReadableStreamReadRequest>&& promise)
{
    m_readRequests.append(WTFMove(promise));
}

Ref<ReadableStreamReadRequest> ReadableStreamDefaultReader::takeFirstReadRequest()
{
    return m_readRequests.takeFirst();
}

void ReadableStreamDefaultReader::resolveClosedPromise()
{
    Ref { m_closedDeferred }->resolve();
}

void ReadableStreamDefaultReader::rejectClosedPromise(JSC::JSValue reason)
{
    Ref { m_closedDeferred }->reject<IDLAny>(reason, RejectAsHandled::Yes);
}

void ReadableStreamDefaultReader::onClosedPromiseRejection(ClosedRejectionCallback&& callback)
{
    if (m_internalDefaultReader) {
        m_internalDefaultReader->onClosedPromiseRejection(WTFMove(callback));
        return;
    }

    if (m_closedRejectionCallback) {
        auto oldCallback = std::exchange(m_closedRejectionCallback, { });
        m_closedRejectionCallback = [oldCallback = WTFMove(oldCallback), callback = WTFMove(callback)](auto& globalObject, auto value) mutable {
            oldCallback(globalObject, value);
            callback(globalObject, value);
        };
        return;
    }

    m_closedRejectionCallback = WTFMove(callback);
    Ref { m_closedPromise }->whenSettled([weakThis = WeakPtr { *this }] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        Ref closedPromise = protectedThis->m_closedPromise;
        if (!closedPromise->globalObject() || !protectedThis->m_closedRejectionCallback || closedPromise->status() != DOMPromise::Status::Rejected)
            return;

        protectedThis->m_closedRejectionCallback(*closedPromise->globalObject(), closedPromise->result());
    });
}

void ReadableStreamDefaultReader::onClosedPromiseResolution(Function<void()>&& callback)
{
    if (m_internalDefaultReader) {
        m_internalDefaultReader->onClosedPromiseResolution(WTFMove(callback));
        return;
    }

    if (m_closedResolutionCallback) {
        auto oldCallback = std::exchange(m_closedResolutionCallback, { });
        m_closedResolutionCallback = [oldCallback = WTFMove(oldCallback), callback = WTFMove(callback)]() mutable {
            oldCallback();
            callback();
        };
        return;
    }

    m_closedResolutionCallback = WTFMove(callback);
    Ref { m_closedPromise }->whenSettled([weakThis = WeakPtr { *this }] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        Ref closedPromise = protectedThis->m_closedPromise;
        if (!closedPromise->globalObject() || !protectedThis->m_closedResolutionCallback || closedPromise->status() != DOMPromise::Status::Fulfilled)
            return;

        protectedThis->m_closedResolutionCallback();
    });
}

JSC::JSValue JSReadableStreamDefaultReader::read(JSC::JSGlobalObject& globalObject, JSC::CallFrame& callFrame)
{
    RefPtr internalDefaultReader = wrapped().internalDefaultReader();
    if (!internalDefaultReader) {
        return callPromiseFunction(globalObject, callFrame, [this](auto& globalObject, auto&, auto&& promise) {
            protectedWrapped()->readForBindings(globalObject, WTFMove(promise));
        });
    }

    return internalDefaultReader->readForBindings(globalObject);
}

JSC::JSValue JSReadableStreamDefaultReader::closed(JSC::JSGlobalObject& globalObject) const
{
    RefPtr internalDefaultReader = wrapped().internalDefaultReader();
    if (!internalDefaultReader)
        return protectedWrapped()->closedPromise().promise();

    return internalDefaultReader->closedForBindings(globalObject);
}

// https://streams.spec.whatwg.org/#generic-reader-cancel
JSC::JSValue JSReadableStreamDefaultReader::cancel(JSC::JSGlobalObject& globalObject, JSC::CallFrame& callFrame)
{
    RefPtr internalDefaultReader = wrapped().internalDefaultReader();
    if (!internalDefaultReader) {
        return callPromiseFunction(globalObject, callFrame, [this](auto& globalObject, auto& callFrame, auto&& promise) {
            protectedWrapped()->genericCancel(globalObject, callFrame.argument(0), WTFMove(promise));
        });
    }

    return internalDefaultReader->cancelForBindings(globalObject, callFrame.argument(0));
}

WebCoreOpaqueRoot root(ReadableStreamDefaultReader* reader)
{
    return WebCoreOpaqueRoot { reader };
}

template<typename Visitor>
void ReadableStreamDefaultReader::visitAdditionalChildren(Visitor& visitor)
{
    if (m_stream)
        SUPPRESS_UNCOUNTED_ARG m_stream->visitAdditionalChildren(visitor);
}

template<typename Visitor>
void JSReadableStreamDefaultReader::visitAdditionalChildren(Visitor& visitor)
{
    // Do not ref `wrapped()` here since this function may get called on the GC thread.
    SUPPRESS_UNCOUNTED_ARG wrapped().visitAdditionalChildren(visitor);
}

DEFINE_VISIT_ADDITIONAL_CHILDREN(JSReadableStreamDefaultReader);

} // namespace WebCore
