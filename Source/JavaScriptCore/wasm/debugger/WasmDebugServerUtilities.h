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

#pragma once

#if ENABLE(WEBASSEMBLY)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#include <wtf/HexNumber.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringView.h>

namespace JSC {

class CallFrame;
class JSWebAssemblyInstance;

namespace IPInt {
struct IPIntLocal;
} // namespace IPInt

namespace Wasm {

class VirtualAddress;
enum class TypeKind : int8_t;
struct Type;

enum class ProtocolError : uint8_t {
    None = 0,
    InvalidPacket = 1,
    InvalidAddress = 2,
    InvalidRegister = 3,
    MemoryError = 4,
    UnknownCommand = 5
};

class StepIntoEvent {
public:
    using BitField = uint8_t;

    enum Event : BitField {
        NoEvent = 0,
        StepIntoCall = 1 << 0, // Step into a function call
        StepIntoThrow = 1 << 1, // Step into an exception handler
    };

    StepIntoEvent() = default;

    void set(Event event)
    {
        RELEASE_ASSERT(!hasAny());
        RELEASE_ASSERT(event == StepIntoCall || event == StepIntoThrow);
        m_event = event;
    }

    bool take(Event event)
    {
        if (m_event == event) {
            m_event = NoEvent;
            return true;
        }
        return false;
    }

    bool hasAny() const { return m_event != NoEvent; }

private:
    BitField m_event { NoEvent };
};

struct Breakpoint {
    enum class Type : uint8_t {
        // User-set breakpoint (persistent, tracked by virtual address)
        Regular = 0,

        // One-time breakpoint (auto-removed after each stop)
        Interrupt = 1,
        Step = 2,
    };

    Breakpoint() = default;
    Breakpoint(uint8_t* pc, Type type)
        : type(type)
        , pc(pc)
        , originalBytecode(*pc)
    {
    }

    void patchBreakpoint() { *pc = 0x00; }
    void restorePatch() { *pc = originalBytecode; }

    bool isOneTimeBreakpoint() { return type != Type::Regular; }

    void dump(PrintStream& out) const
    {
        out.print("Breakpoint(type:", type);
        out.print(", pc:", RawPointer(pc));
        out.print(", *pc:", (int)*pc);
        out.print(", originalBytecode:", originalBytecode, ")");
    }

    Type type { Type::Regular };
    uint8_t* pc { nullptr };
    uint8_t originalBytecode { 0 };
};

template<typename T>
inline String toNativeEndianHex(const T& value)
{
    static_assert(sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8 || sizeof(T) == 16,
        "toNativeEndianHex only supports 1, 2, 4, 8, or 16 byte types");

    StringBuilder hexString;
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
    for (size_t i = 0; i < sizeof(T); i++)
        hexString.append(hex(bytes[i], 2, Lowercase));
    return hexString.toString();
}

String stringToHex(StringView);

void logWasmLocalValue(size_t index, const JSC::IPInt::IPIntLocal&, const Wasm::Type&);

uint64_t parseHex(StringView, uint64_t defaultValue = 0);

uint32_t parseDecimal(StringView, uint32_t defaultValue = 0);

Vector<StringView> splitWithDelimiters(StringView packet, StringView delimiters);

bool getWasmReturnPC(CallFrame* currentFrame, uint8_t*& returnPC, VirtualAddress& virtualReturnPC);

} // namespace Wasm
} // namespace JSC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // ENABLE(WEBASSEMBLY)
