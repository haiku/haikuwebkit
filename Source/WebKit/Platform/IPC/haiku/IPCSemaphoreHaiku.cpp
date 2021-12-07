/*
 * Copyright (C) 2023 Haiku, Inc. All rights reserved.
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
#include "IPCSemaphore.h"

#include "WebCoreArgumentCoders.h"
#include <OS.h>

namespace IPC {

Semaphore::Semaphore()
{
    m_semaphore = create_sem(0, "IPC Semaphore");
}

Semaphore::Semaphore(Semaphore&& other)
    : m_semaphore(std::exchange(other.m_semaphore, 0))
{
}

Semaphore::Semaphore(sem_id sem)
	: m_semaphore(sem)
{
}

Semaphore::~Semaphore()
{
    destroy();
}

Semaphore& Semaphore::operator=(Semaphore&& other)
{
    if (this != &other) {
        destroy();
        m_semaphore = std::exchange(other.m_semaphore, 0);
    }

    return *this;
}

void Semaphore::signal()
{
    release_sem(m_semaphore);
}

bool Semaphore::wait()
{
    auto ret = acquire_sem(m_semaphore);
    return ret == B_OK;
}

bool Semaphore::waitFor(Timeout timeout)
{
    Seconds waitTime = timeout.secondsUntilDeadline();
    auto ret = acquire_sem_etc(m_semaphore, 1, 0, waitTime.nanosecondsAs<uint64_t>());
    return ret == B_OK;
}

void Semaphore::encode(Encoder& encoder) const
{
    encoder << m_semaphore;
}

std::optional<Semaphore> Semaphore::decode(Decoder& decoder)
{
    sem_id semaphore;
    if (!decoder.decode(semaphore))
        return std::nullopt;
    return std::optional<Semaphore> { std::in_place, semaphore };
}


void Semaphore::destroy()
{
    if (m_semaphore == 0)
        return;
    delete_sem(m_semaphore);
    m_semaphore = 0;
}

} // namespace IPC
