/*
 * Copyright (C) 2014, 2024 Haiku, Inc
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
#include "wtf/RunLoop.h"

#include <Application.h>
#include <Handler.h>
#include <MessageRunner.h>
#include <errno.h>
#include <stdio.h>

namespace WTF {


class LoopHandler: public BHandler
{
    public:
        LoopHandler()
            : BHandler("RunLoop")
        {
        }

        void MessageReceived(BMessage* message) override
        {
            if (message->what == 'loop') {
                RunLoop::currentSingleton().performWork();
            } else if (message->what == 'tmrf') {
                RunLoop::TimerBase* timer
                    = (RunLoop::TimerBase*)message->GetPointer("timer");
                timer->timerFired();
            } else {
                message->PrintToStream();
                BHandler::MessageReceived(message);
            }
        }
};


RunLoop::RunLoop()
	: m_looper(nullptr)
{
    m_handler = new LoopHandler();
}

RunLoop::~RunLoop()
{
    stop();
    delete m_handler;
}

void RunLoop::run()
{
    BLooper* looper = BLooper::LooperForThread(find_thread(NULL));
    if (!looper) {
        currentSingleton().m_looper = looper = new BLooper();
    } else if (looper != be_app) {
        fprintf(stderr, "Add handler to existing RunLoop looper\n");
    }
    looper->LockLooper();
    looper->AddHandler(currentSingleton().m_handler);
    looper->UnlockLooper();

    if (currentSingleton().m_looper) {
        // Make sure the thread will start calling performWork as soon as it can
        RunLoop::currentSingleton().wakeUp();
        // Then start the normal event loop
        currentSingleton().m_looper->Loop();
    }
}

void RunLoop::stop()
{
    if (!m_handler->LockLooper())
        return;

    BLooper* looper = m_handler->Looper();
    looper->RemoveHandler(m_handler);
    looper->Unlock();

    if (m_looper) {
        thread_id thread = m_looper->Thread();
        status_t ret;

        m_looper->PostMessage(B_QUIT_REQUESTED);
        m_looper = nullptr;

        wait_for_thread(thread, &ret);
    }
}

void RunLoop::wakeUp()
{
    m_handler->Looper()->PostMessage('loop', m_handler);
}

RunLoop::TimerBase::TimerBase(WTF::Ref<RunLoop>&& runLoop, WTF::ASCIILiteral)
    : m_runLoop(runLoop)
{
    m_messageRunner = NULL;
}

RunLoop::TimerBase::~TimerBase()
{
    stop();
}

void RunLoop::TimerBase::timerFired()
{
    // was timer stopped?
    if (m_messageRunner == nullptr)
        return;

    // do we need to stop it?
    bigtime_t interval = 0;
    int32 count = 0;

    m_messageRunner->GetInfo(&interval, &count);
    if (count == 1)
        stop();

    fired();
}

void RunLoop::TimerBase::start(Seconds nextFireInterval, bool repeat)
{
    if (m_messageRunner) {
        delete m_messageRunner;
        m_messageRunner = nullptr;
    }

    BMessage* message = new BMessage('tmrf');
    message->AddPointer("timer", this);

    m_messageRunner = new BMessageRunner(m_runLoop->m_handler,
        message, nextFireInterval.microseconds(), repeat ? -1 : 1);
}

bool RunLoop::TimerBase::isActive() const
{
    return m_messageRunner != NULL && m_messageRunner->GetInfo(NULL, NULL) == B_OK;
}

void RunLoop::TimerBase::stop()
{
    delete m_messageRunner;
    m_messageRunner = NULL;
}

RunLoop::CycleResult RunLoop::cycle(RunLoopMode)
{
    RunLoop::currentSingleton().performWork();

    if (RunLoop::currentSingleton().m_handler->Looper()->IsMessageWaiting())
        return CycleResult::Continue;
    else
        return CycleResult::Stop;
}

Seconds RunLoop::TimerBase::secondsUntilFire() const
{
    // FIXME implement
    return 0_s;
}
}
