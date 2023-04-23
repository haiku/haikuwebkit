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
				puts("MessageReceived");
				message->PrintToStream();
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
		puts("finalize");
		message->PrintToStream();
        //unwrap the message
        status_t result = message->FindMessenger("target", &targetMessenger);
        if (result == B_OK)
            m_isConnected = true;

        m_connectionQueue->dispatch([protectedThis = Ref(*this)]() mutable {
            protectedThis->sendOutgoingMessages();
        });
    }

    void Connection::platformInitialize(Identifier identifier)
    {
		puts("platformInitialize");
        m_connectedProcess = identifier;
    }

    void Connection::platformInvalidate()
    {
    }

    void Connection::prepareIncomingMessage(BMessage* message)
    {
		puts("prepareIncoming");
		message->PrintToStream();
        size_t size;
        const uint8_t* Buffer;
        status_t result;

        result = message->FindData("bufferData", B_ANY_TYPE, (const void**)&Buffer, (ssize_t*)&size);

        if (result == B_OK)
        {
            Vector<Attachment> attachments(0);
            auto decoder = Decoder::create(Buffer, size, nullptr, WTFMove(attachments));
            processIncomingMessage(WTFMove(decoder));
        }
        else
        {
            //return
        }
    }

    void Connection::runReadEventLoop()
    {
		puts("runReadEventLoop");
        status_t result;
        BLooper* looper = m_connectionQueue->runLoop().runLoopLooper();

        looper->Lock();
        looper->AddHandler(m_readHandler);
        looper->SetPreferredHandler(m_readHandler);
        looper->Unlock();
        /*
        notify the mainloop about our workqueue
        */
        BMessage init('inil');
        init.AddString("identifier", m_connectedProcess.key.String());
        init.AddPointer("looper", (const void*)looper);
        BMessenger hostProcess(NULL, getpid());
        result = hostProcess.SendMessage(&init);
        /*
        notify the other process  about our workqueue
        */
        BMessenger target(looper->PreferredHandler(), looper, &result);
        BMessage inig('inig');
        inig.AddString("identifier", m_connectedProcess.key.String());
        inig.AddMessenger("target", target);
        BMessage reply;
        m_messenger.SendMessage(&inig);
        //sent to the other process
    }

    void Connection::runWriteEventLoop()
    {
		puts("runWriteEventLoop");
        // write the pending encoding but do we need this will messaging fail
        //probably when the message queue is full
    }

    void Connection::platformOpen()
    {
		puts("platformOpen");
        status_t result = m_messenger.SetTo(NULL, m_connectedProcess.connectedProcess);
        m_readHandler = new ReadLoop(this);
        m_connectionQueue->dispatch([this, protectedThis = Ref(*this)]{
            this->runReadEventLoop();
        });
    }

    bool Connection::platformCanSendOutgoingMessages() const
    {
        return true;
    }

    bool Connection::sendOutgoingMessage(UniqueRef<Encoder>&& encoder)
    {
		puts("sendOutgoingMessage");
        BMessage processMessage('ipcm');
        processMessage.AddString("identifier", m_connectedProcess.key.String());
        const uint8_t* Buffer = encoder->buffer();
        status_t result = processMessage.AddData("bufferData", B_ANY_TYPE, (void*)Buffer, encoder->bufferSize());

        processMessage.AddInt32("sender",getpid());
        result = targetMessenger.SendMessage(&processMessage);

		processMessage.PrintToStream();

        if(result == B_OK)
            return true;
        else
        {
            m_pendingWriteEncoder = encoder.moveToUniquePtr();
            return false;
        }
    }

    void Connection::willSendSyncMessage(OptionSet<SendSyncOption>)
    {
    }

    void Connection::didReceiveSyncReply(OptionSet<SendSyncOption>)
    {
    }

    std::optional<Connection::ConnectionIdentifierPair> Connection::createConnectionIdentifierPair()
    {
         // FIXME implement this
         notImplemented();
         return std::nullopt;
    }
}
