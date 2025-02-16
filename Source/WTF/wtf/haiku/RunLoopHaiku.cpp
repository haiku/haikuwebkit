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
#include <errno.h>
#include <Handler.h>
#include <MessageRunner.h>
#include <OS.h>
#include <stdio.h>

/*
The main idea behind this implementation of RunLoop for Haiku is to use a
BHandler to receive messages. WebKit uses one RunLoop per thread, including
the main thread, which already has a BApplication on it. So,

* If we're on the main thread, we attach the BHandler to the existing
  BApplication, or
* If we're on a new thread, we create a new BLooper ourselves and attach the
  BHandler to it.

Either way, the RunLoop should then be ready to handle messages sent to it.
*/

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
                RunLoop::current().performWork();
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
    , m_handler(new LoopHandler)
{
}

RunLoop::~RunLoop()
{
    stop();
    delete m_handler;
}

void RunLoop::run()
{
    // Find the looper that we should attach our handler to.
    BLooper* looper;
    BLooper* currentLooper = BLooper::LooperForThread(find_thread(NULL));
    if (currentLooper) {
        // This thread already has a looper (likely the BApplication looper).
        // Attach our handler to it.
        looper = currentLooper;
    } else {
        thread_info main_thread;
        int32 cookie = 0;
        get_next_thread_info(0, &cookie, &main_thread);
        if (find_thread(NULL) == main_thread.thread) {
            if (be_app == NULL)
                debugger("RunLoop needs a BApplication running on the main thread to attach to");

            // BApplication has not been started yet and we are on the main
            // thread. This BApplication will almost certainly become this
            // thread's BLooper in the future.
            looper = be_app;
        } else {
            // No existing BLooper or BApplication is on this thread. Let's
            // create one and manage its lifecycle.
            current().m_looper = looper = new BLooper();
        }
    }

    looper->LockLooper();
    looper->AddHandler(current().m_handler);
    looper->UnlockLooper();

    // There might already be messages available to process, so lets address
    // those if there are any.
    current().wakeUp();

    if (current().m_looper) {
        // We need to run the looper we created.
        current().m_looper->Loop();
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
        // We created the looper that we attached to. We have to stop that as
        // well.
        thread_id thread = m_looper->Thread();
        status_t ret;

        m_looper->PostMessage(B_QUIT_REQUESTED);
        m_looper = nullptr;

        wait_for_thread(thread, &ret);
    }
}

void RunLoop::wakeUp()
{
    // We shouldn't wake up the looper if the RunLoop hasn't been started yet
    // or after it has been shut down. Both of these can be caught simply by
    // checking if there is a Looper available to message in the first place.
    if (m_handler->Looper())
        m_handler->Looper()->PostMessage('loop', m_handler);
}

RunLoop::TimerBase::TimerBase(WTF::Ref<RunLoop>&& runLoop)
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
    RunLoop::current().performWork();

    if (RunLoop::current().m_handler->Looper()->IsMessageWaiting())
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
