/*
 * Copyright (C) 2014 Haiku, Inc
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


struct RunLoopDispatchHandler {
	RunLoopDispatchHandler(Function<void()>&& f)
		: function(WTFMove(f))
	{}
	~RunLoopDispatchHandler() { delete runner; }

	BMessageRunner* runner;
	Function<void()>&& function;
};


class LoopHandler: public BHandler
{
    public:
        LoopHandler(RunLoop* loop)
            : BHandler("RunLoop")
            , m_loop(loop)
        {
        }

        void MessageReceived(BMessage* message)
        {
            if (message->what == 'loop')
                m_loop->iterate();
            else if (message->what == 'tmrf') {
                RunLoop::TimerBase* timer
                    = (RunLoop::TimerBase*)message->GetPointer("timer");
                timer->fired();
            } else if (message->what == 'daft') {
                RunLoopDispatchHandler* handler
                    = (RunLoopDispatchHandler*)message->GetPointer("handler");
                handler->function();
                delete handler;
            } else
                BHandler::MessageReceived(message);
        }

    private:
        RunLoop* m_loop;
};


RunLoop::RunLoop()
	: m_looper(nullptr)
{
    m_handler = new LoopHandler(this);
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
		current().m_looper = looper = new BLooper();
	} else if (looper != be_app) {
		fprintf(stderr, "Add handler to existing RunLoop looper\n");
	}
	looper->LockLooper();
	looper->AddHandler(current().m_handler);
	looper->UnlockLooper();

	if (current().m_looper)
		current().m_looper->Loop();
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

RunLoop::TimerBase::TimerBase(RunLoop& runLoop)
    : m_runLoop(runLoop)
{
    m_messageRunner = NULL;
}

RunLoop::TimerBase::~TimerBase()
{
    stop();
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
    /* TODO do we need to check if the messages have all been sent already? */
    return m_messageRunner != NULL;
}

void RunLoop::TimerBase::stop()
{
    delete m_messageRunner;
    m_messageRunner = NULL;
}

void RunLoop::iterate()
{
	RunLoop::current().performWork();
}

RunLoop::CycleResult RunLoop::cycle(RunLoopMode)
{
    iterate();
    return CycleResult::Continue;
}

Seconds RunLoop::TimerBase::secondsUntilFire() const
{
    // FIXME implement
    return 0_s;
}
}
