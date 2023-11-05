/*
 * Copyright (C) 2019 Haiku, Inc. All rights reserved.
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
#include "Connection.h"

#include <unistd.h>
#include <Message.h>
#include <Looper.h>
#include <stdlib.h>

#include <WebCore/NotImplemented.h>
#include <WebCore/ProcessIdentifier.h>

namespace IPC
{
    class ReadLoop: public BHandler
    {
        public:
            ReadLoop(IPC::Connection* con)
                : BHandler("Read Message Loop"),
                connection(con)
            {
            }

            void MessageReceived(BMessage* message)
            {
                switch(message->what)
                {
                    case 'ipcm':
                        connection->prepareIncomingMessage(message);
                        break;
                    case 'inig':
                        connection->finalizeConnection(message);
                        break;
                    default:
                        BHandler::MessageReceived(message);
                }
            }
        private:
            IPC::Connection* connection;
    };

    void Connection::finalizeConnection(BMessage* message)
    {
        //unwrap the message
        status_t result = message->FindMessenger("messenger", &targetMessenger);
        if (result == B_OK)
            m_isConnected = true;

        m_connectionQueue->dispatch([protectedThis = Ref(*this)]() mutable {
            protectedThis->sendOutgoingMessages();
        });
    }

    void Connection::platformInitialize(Identifier identifier)
    {
        m_connectedProcess = identifier;
    }

    void Connection::platformInvalidate()
    {
    }

    void Connection::prepareIncomingMessage(BMessage* message)
    {
        size_t size;
        const uint8_t* buffer;
        status_t result;

        result = message->FindData("bufferData", B_ANY_TYPE, (const void**)&buffer, (ssize_t*)&size);

        if (result == B_OK)
        {
            // Make a copy that's properly aligned, because the BMessage data isn't
            uint8_t* b2 = (uint8_t*)malloc(size);
            memcpy(b2, buffer, size);

            Vector<Attachment> attachments(0);
            auto decoder = Decoder::create({b2, size}, WTFMove(attachments));
            ASSERT(decoder);
            if (!decoder) {
                puts("OOPS");
                free(b2);
                return;
            }

            processIncomingMessage(WTFMove(decoder));
            free(b2);
        }
        else
        {
            CRASH();
        }
    }

    void Connection::runReadEventLoop()
    {
        status_t result;
        BLooper* looper = m_connectionQueue->runLoop().runLoopLooper();

        looper->Lock();
        looper->AddHandler(m_readHandler);
        looper->SetPreferredHandler(m_readHandler);
        looper->Unlock();

        /*
        notify the other process  about our workqueue
        */
        BMessenger target(m_readHandler, looper, &result);
        BMessage inig('inig');
        inig.AddInt64("processID", getpid());
        inig.AddMessenger("messenger", target);
        m_connectedProcess.handle.SendMessage(&inig);
    }

    void Connection::platformOpen()
    {

        m_readHandler = new ReadLoop(this);
        m_connectionQueue->dispatch([this, protectedThis = Ref(*this)]{
            this->runReadEventLoop();
        });

        if (m_connectedProcess.m_isCreatedFromMessage) {
            targetMessenger = m_connectedProcess.handle;
            m_isConnected = true;
            m_connectionQueue->dispatch([protectedThis = Ref(*this)]() mutable {
                protectedThis->sendOutgoingMessages();
            });
        }
    }

    bool Connection::platformCanSendOutgoingMessages() const
    {
        return true;
    }

    bool Connection::sendOutgoingMessage(UniqueRef<Encoder>&& encoder)
    {
        BMessage processMessage('ipcm');
        processMessage.AddMessenger("identifier", BMessenger(m_readHandler, m_readHandler->Looper()));
        const uint8_t* Buffer = encoder->buffer();
        status_t result = processMessage.AddData("bufferData", B_ANY_TYPE, (void*)Buffer, encoder->bufferSize());

        processMessage.AddInt32("sender",getpid());
        result = targetMessenger.SendMessage(&processMessage);

        if(result == B_OK)
            return true;
        else
        {
            m_pendingWriteEncoder = encoder.moveToUniquePtr();
            return false;
        }
    }

    std::optional<Connection::ConnectionIdentifierPair> Connection::createConnectionIdentifierPair()
    {
         // FIXME implement this
         notImplemented();
         return std::nullopt;
    }
}
