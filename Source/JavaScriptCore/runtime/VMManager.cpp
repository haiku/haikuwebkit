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
#include "VMManager.h"

#include "VM.h"

namespace JSC {

Lock g_vmListLock;
VM* VMManager::s_recentVM { nullptr };

static DoublyLinkedList<VM>& vmList() WTF_REQUIRES_LOCK(g_vmListLock)
{
    static NeverDestroyed<DoublyLinkedList<VM>> list;
    return list;
}

void VMManager::add(VM* vm)
{
    Locker locker { g_vmListLock };
    s_recentVM = vm;
    vmList().append(vm);
}

void VMManager::remove(VM* vm)
{
    Locker locker { g_vmListLock };
    if (s_recentVM == vm)
        s_recentVM = nullptr;
    vmList().remove(vm);
}

bool VMManager::isValidVMSlow(VM* vm)
{
    bool found = false;
    forEachVM([&] (VM& nextVM) {
        if (vm == &nextVM) {
            s_recentVM = vm;
            found = true;
            return IterationStatus::Done;
        }
        return IterationStatus::Continue;
    });
    return found;
}

void VMManager::dumpVMs()
{
    unsigned i = 0;
    WTFLogAlways("Registered VMs:");
    forEachVM([&] (VM& nextVM) {
        WTFLogAlways("  [%u] VM %p", i++, &nextVM);
        return IterationStatus::Continue;
    });
}

static void iterateVMs(const Invocable<IterationStatus(VM&)> auto& functor) WTF_REQUIRES_LOCK(g_vmListLock)
{
    for (VM* vm = vmList().head(); vm; vm = vm->next()) {
        IterationStatus status = functor(*vm);
        if (status == IterationStatus::Done)
            return;
    }
}

VM* VMManager::findMatchingVMImpl(const ScopedLambda<VMManager::TestCallback>& test)
{
    Locker lock { g_vmListLock };
    if (s_recentVM && test(*s_recentVM))
        return s_recentVM;

    VM* result = nullptr;
    iterateVMs(scopedLambda<IteratorCallback>([&] (VM& vm) {
        if (test(vm)) {
            result = &vm;
            s_recentVM = &vm;
            return IterationStatus::Done;
        }
        return IterationStatus::Continue;
    }));
    return result;
}

void VMManager::forEachVMImpl(const ScopedLambda<VMManager::IteratorCallback>& func)
{
    Locker lock { g_vmListLock };
    iterateVMs(func);
}

VMManager::Error VMManager::forEachVMWithTimeoutImpl(Seconds timeout, const ScopedLambda<VMManager::IteratorCallback>& func)
{
    if (!g_vmListLock.tryLockWithTimeout(timeout))
        return Error::TimedOut;

    Locker locker { AdoptLock, g_vmListLock };
    iterateVMs(func);
    return Error::None;
}

} // namespace JSC
