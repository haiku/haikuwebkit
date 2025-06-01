/*
 * Copyright (C) 2009 Brent Fulgham.  All rights reserved.
 * Copyright (C) 2009 Google Inc.  All rights reserved.
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SocketStreamHandleImpl.h"

#include "SocketStreamHandleClient.h"
#include "WebKitLogging.h"

#include "WebCore/SocketStreamError.h"
#include "WebCore/StorageSessionProvider.h"

#include <wtf/MainThread.h>
#include <wtf/URL.h>
#include <wtf/text/CString.h>

#include <NetworkAddress.h>
#include <SecureSocket.h>
#include <Socket.h>

namespace WebCore {



SocketStreamHandleImpl::SocketStreamHandleImpl(const URL& url, SocketStreamHandleClient& client, const StorageSessionProvider* provider)
    : SocketStreamHandle(url, client)
    , m_storageSessionProvider(provider)
{
    LOG(Network, "SocketStreamHandle %p new client %p", this, &m_client);

    ASSERT(url.protocolIs("ws"_s) || url.protocolIs("wss"_s));


    m_workerThread = Thread::create("WebSocket thread"_s, [this, protectedThis = Ref{*this}] {
        threadEntryPoint();
    });
}

SocketStreamHandleImpl::~SocketStreamHandleImpl()
{
    LOG(Network, "SocketStreamHandle %p delete", this);
    stopThread();
}

std::optional<size_t> SocketStreamHandleImpl::platformSendInternal(std::span<const uint8_t> data)
{
    LOG(Network, "SocketStreamHandle %p platformSend", this);
    ASSERT(isMainThread());

    if (m_hasPendingWriteData)
        return 0;

    m_hasPendingWriteData = true;

    auto writeBuffer = makeUniqueArray<uint8_t>(data.size());
    memcpy(writeBuffer.get(), data.data(), data.size());

    callOnWorkerThread([this, writeBuffer = WTFMove(writeBuffer), writeBufferSize = data.size()]() mutable {
        ASSERT(!isMainThread());
        m_writeBuffer = WTFMove(writeBuffer);
        m_writeBufferSize = writeBufferSize;
        m_writeBufferOffset = 0;
    });

    return data.size();
}

void SocketStreamHandleImpl::platformClose()
{
    LOG(Network, "SocketStreamHandle %p platformClose", this);
    ASSERT(isMainThread());

    if (m_state == Closed)
        return;
    m_state = Closed;

    stopThread();
    m_client.didCloseSocketStream(*this);
}

void SocketStreamHandleImpl::threadEntryPoint()
{
    ASSERT(!isMainThread());

    unsigned int port = m_url.port() ? *m_url.port() : (m_url.protocolIs(ASCIILiteral::fromLiteralUnsafe("wss")) ? 443 : 80);
    BNetworkAddress peer(m_url.host().utf8().data(), port);
    BSocket* socket = m_url.protocolIs(ASCIILiteral::fromLiteralUnsafe("wss")) ? new BSecureSocket : new BSocket;

    // Connect to host
    status_t status = socket->Connect(peer);
    if (status != B_OK) {
        handleError(status);
        return;
    }

    callOnMainThread([this, protectedThis = Ref{*this}] {
        if (m_state == Connecting) {
            m_state = Open;
            m_client.didOpenSocketStream(*this);
        }
    });

    while (m_running) {
        executeTasks();

        status_t readable = socket->WaitForReadable(20 * 1000);
        if (readable != B_OK && readable != B_TIMED_OUT) {
            handleError(readable);
            break;
        }

        status_t writable = B_ERROR;
        if (m_writeBuffer.get() != nullptr) {
            writable = socket->WaitForWritable(20 * 1000);
            if (writable != B_OK && writable != B_TIMED_OUT) {
                handleError(writable);
                break;
            }
        }

        // These logic only run when there's data waiting.
        if ((writable == B_OK) && m_running) {
            auto bytesSent = socket->Write(m_writeBuffer.get() + m_writeBufferOffset, m_writeBufferSize - m_writeBufferOffset);
            if (bytesSent < 0) {
                handleError(bytesSent);
                break;
            }
            m_writeBufferOffset += bytesSent;

            if (m_writeBufferSize <= m_writeBufferOffset) {
                m_writeBuffer = nullptr;
                m_writeBufferSize = 0;
                m_writeBufferOffset = 0;

                callOnMainThread([this, protectedThis = Ref{*this}] {
                    m_hasPendingWriteData = false;
                    sendPendingData();
                });
            }
        }

        if ((readable == B_OK) && m_running) {
            auto readBuffer = makeUniqueArray<uint8_t>(kReadBufferSize);
            ssize_t bytesRead = socket->Read(readBuffer.get(), kReadBufferSize);
            if (bytesRead <= 0) {
                m_running = false;
                callOnMainThread([this, protectedThis = Ref{*this}] {
                    if (m_state == Closed)
                        return;

                    m_client.didFailToReceiveSocketStreamData(*this);
                });
                break;
            }

            callOnMainThread([this, protectedThis = Ref{*this}, buffer = WTFMove(readBuffer), size = bytesRead ] {
                if (m_state == Open)
                    m_client.didReceiveSocketStreamData(*this, std::span<const unsigned char>(buffer.get(), size));
            });
        }
    }

    m_writeBuffer = nullptr;
    delete socket;
}

void SocketStreamHandleImpl::handleError(status_t errorCode)
{
    m_running = false;
    callOnMainThread([this, protectedThis = Ref{*this}, errorCode, localizedDescription = strerror(errorCode)] {
        if (m_state == Closed)
            return;

        m_client.didFailSocketStream(*this, SocketStreamError(static_cast<int>(errorCode), String(), String::fromUTF8(localizedDescription)));
    });
}

void SocketStreamHandleImpl::stopThread()
{
    ASSERT(isMainThread());

    m_running = false;

    if (m_workerThread) {
        m_workerThread->waitForCompletion();
        m_workerThread = nullptr;
    }
}

void SocketStreamHandleImpl::callOnWorkerThread(Function<void()>&& task)
{
    ASSERT(isMainThread());
    m_taskQueue.append(std::make_unique<Function<void()>>(WTFMove(task)));
}

void SocketStreamHandleImpl::executeTasks()
{
    ASSERT(!isMainThread());

    auto tasks = m_taskQueue.takeAllMessages();
    for (auto& task : tasks)
        (*task)();
}

} // namespace WebCore
