/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(CSS_SELECTOR_JIT)

#include <JavaScriptCore/MacroAssembler.h>
#include <wtf/Deque.h>
#include <wtf/Vector.h>

namespace WebCore {

#if CPU(ARM64)
static constexpr std::array<JSC::MacroAssembler::RegisterID, 15> callerSavedRegisters {
    JSC::ARM64Registers::x0,
    JSC::ARM64Registers::x1,
    JSC::ARM64Registers::x2,
    JSC::ARM64Registers::x3,
    JSC::ARM64Registers::x4,
    JSC::ARM64Registers::x5,
    JSC::ARM64Registers::x6,
    JSC::ARM64Registers::x7,
    JSC::ARM64Registers::x8,
    JSC::ARM64Registers::x9,
    JSC::ARM64Registers::x10,
    JSC::ARM64Registers::x11,
    JSC::ARM64Registers::x12,
    JSC::ARM64Registers::x13,
    JSC::ARM64Registers::x14,
};
static constexpr std::array<JSC::MacroAssembler::RegisterID, 1> calleeSavedRegisters = {
    JSC::ARM64Registers::x19
};
static const JSC::MacroAssembler::RegisterID tempRegister = JSC::ARM64Registers::x15;
#elif CPU(X86_64)
static constexpr std::array<JSC::MacroAssembler::RegisterID, 8> callerSavedRegisters {
    JSC::X86Registers::eax,
    JSC::X86Registers::ecx,
    JSC::X86Registers::edx,
    JSC::X86Registers::esi,
    JSC::X86Registers::edi,
    JSC::X86Registers::r8,
    JSC::X86Registers::r9,
    JSC::X86Registers::r10,
};
static constexpr std::array<JSC::MacroAssembler::RegisterID, 4> calleeSavedRegisters {
    JSC::X86Registers::r12,
    JSC::X86Registers::r13,
    JSC::X86Registers::r14,
    JSC::X86Registers::r15
};
#else
#error RegisterAllocator has no defined registers for the architecture.
#endif

static constexpr unsigned calleeSavedRegisterCount = std::size(calleeSavedRegisters);
static constexpr unsigned maximumRegisterCount = calleeSavedRegisterCount + std::size(callerSavedRegisters);

using RegisterVector = Vector<JSC::MacroAssembler::RegisterID, maximumRegisterCount>;

class RegisterAllocator {
public:
    RegisterAllocator() { }
    ~RegisterAllocator();

    unsigned availableRegisterCount() const { return m_registers.size(); }

    JSC::MacroAssembler::RegisterID allocateRegister()
    {
        RELEASE_ASSERT(m_registers.size());
        JSC::MacroAssembler::RegisterID registerID = m_registers.first();
        m_registers.removeFirst();
        ASSERT(!m_allocatedRegisters.contains(registerID));
        m_allocatedRegisters.append(registerID);
        return registerID;
    }

    void allocateRegister(JSC::MacroAssembler::RegisterID registerID)
    {
        for (auto it = m_registers.begin(); it != m_registers.end(); ++it) {
            if (*it == registerID) {
                m_registers.remove(it);
                ASSERT(!m_allocatedRegisters.contains(registerID));
                m_allocatedRegisters.append(registerID);
                return;
            }
        }
        RELEASE_ASSERT_NOT_REACHED();
    }
    
    JSC::MacroAssembler::RegisterID allocateRegisterWithPreference(JSC::MacroAssembler::RegisterID preferredRegister)
    {
        for (auto it = m_registers.begin(); it != m_registers.end(); ++it) {
            if (*it == preferredRegister) {
                m_registers.remove(it);
                ASSERT(!m_allocatedRegisters.contains(preferredRegister));
                m_allocatedRegisters.append(preferredRegister);
                return preferredRegister;
            }
        }
        return allocateRegister();
    }

    void deallocateRegister(JSC::MacroAssembler::RegisterID registerID)
    {
        ASSERT(m_allocatedRegisters.contains(registerID));
        // Most allocation/deallocation happen in stack-like order. In the common case, this
        // just removes the last item.
        m_allocatedRegisters.removeLast(registerID);
        for (auto unallocatedRegister : m_registers)
            RELEASE_ASSERT(unallocatedRegister != registerID);
        m_registers.append(registerID);
    }

    unsigned reserveCallerSavedRegisters(unsigned count)
    {
#ifdef NDEBUG
        UNUSED_PARAM(count);
        unsigned numberToAllocate = std::size(callerSavedRegisters);
#else
        unsigned numberToAllocate = std::min<unsigned>(std::size(callerSavedRegisters), count);
#endif
        for (unsigned i = 0; i < numberToAllocate; ++i)
            m_registers.append(callerSavedRegisters[i]);
        return numberToAllocate;
    }

    const Vector<JSC::MacroAssembler::RegisterID, calleeSavedRegisterCount>& reserveCalleeSavedRegisters(unsigned count)
    {
        RELEASE_ASSERT(!m_reservedCalleeSavedRegisters.size());
        for (auto registerId : std::span { calleeSavedRegisters }.first(count)) {
            m_reservedCalleeSavedRegisters.append(registerId);
            m_registers.append(registerId);
        }
        return m_reservedCalleeSavedRegisters;
    }

    Vector<JSC::MacroAssembler::RegisterID, calleeSavedRegisterCount> restoreCalleeSavedRegisters()
    {
        Vector<JSC::MacroAssembler::RegisterID, calleeSavedRegisterCount> registers(m_reservedCalleeSavedRegisters);
        m_reservedCalleeSavedRegisters.clear();
        return registers;
    }

    const RegisterVector& allocatedRegisters() const { return m_allocatedRegisters; }

    static bool isValidRegister(JSC::MacroAssembler::RegisterID registerID)
    {
#if CPU(ARM64)
        return (registerID >= JSC::ARM64Registers::x0 && registerID <= JSC::ARM64Registers::x14)
            || registerID == JSC::ARM64Registers::x19;
#elif CPU(X86_64)
        return (registerID >= JSC::X86Registers::eax && registerID <= JSC::X86Registers::edx)
            || (registerID >= JSC::X86Registers::esi && registerID <= JSC::X86Registers::r10)
            || (registerID >= JSC::X86Registers::r12 && registerID <= JSC::X86Registers::r15);
#else
#error RegisterAllocator does not define the valid register range for the current architecture.
#endif
    }
    
    static bool isCallerSavedRegister(JSC::MacroAssembler::RegisterID registerID)
    {
        ASSERT(isValidRegister(registerID));
#if CPU(ARM64)
        return registerID >= JSC::ARM64Registers::x0 && registerID <= JSC::ARM64Registers::x14;
#elif CPU(X86_64)
        return (registerID >= JSC::X86Registers::eax && registerID <= JSC::X86Registers::edx)
            || (registerID >= JSC::X86Registers::esi && registerID <= JSC::X86Registers::r10);
#else
#error RegisterAllocator does not define the valid caller saved register range for the current architecture.
#endif
    }

private:
    Deque<JSC::MacroAssembler::RegisterID, maximumRegisterCount> m_registers;
    RegisterVector m_allocatedRegisters;
    Vector<JSC::MacroAssembler::RegisterID, calleeSavedRegisterCount> m_reservedCalleeSavedRegisters;
};

class LocalRegister {
public:
    explicit LocalRegister(RegisterAllocator& allocator)
        : m_allocator(allocator)
        , m_register(allocator.allocateRegister())
    {
    }

    ~LocalRegister()
    {
        m_allocator.deallocateRegister(m_register);
    }

    operator JSC::MacroAssembler::RegisterID() const
    {
        return m_register;
    }

protected:
    explicit LocalRegister(RegisterAllocator& allocator, JSC::MacroAssembler::RegisterID registerID)
        : m_allocator(allocator)
        , m_register(registerID)
    {
    }
    RegisterAllocator& m_allocator;
    JSC::MacroAssembler::RegisterID m_register;
};

class LocalRegisterWithPreference : public LocalRegister {
public:
    explicit LocalRegisterWithPreference(RegisterAllocator& allocator, JSC::MacroAssembler::RegisterID preferredRegister)
        : LocalRegister(allocator, allocator.allocateRegisterWithPreference(preferredRegister))
    {
    }
};

inline RegisterAllocator::~RegisterAllocator()
{
    RELEASE_ASSERT(m_reservedCalleeSavedRegisters.isEmpty());
}

} // namespace WebCore

#endif // ENABLE(CSS_SELECTOR_JIT)
