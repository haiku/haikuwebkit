# Copyright (C) 2019-2024 Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.

# Calling conventions
const CalleeSaveSpaceAsVirtualRegisters = constexpr Wasm::numberOfLLIntCalleeSaveRegisters + constexpr Wasm::numberOfLLIntInternalRegisters
const CalleeSaveSpaceStackAligned = (CalleeSaveSpaceAsVirtualRegisters * SlotSize + StackAlignment - 1) & ~StackAlignmentMask
const WasmEntryPtrTag = constexpr WasmEntryPtrTag
const UnboxedWasmCalleeStackSlot = CallerFrame - constexpr Wasm::numberOfLLIntCalleeSaveRegisters * SlotSize - MachineRegisterSize
const WasmToJSScratchSpaceSize = constexpr Wasm::WasmToJSScratchSpaceSize
const WasmToJSCallableFunctionSlot = constexpr Wasm::WasmToJSCallableFunctionSlot

if HAVE_FAST_TLS
    const WTF_WASM_CONTEXT_KEY = constexpr WTF_WASM_CONTEXT_KEY
end

# Must match GPRInfo.h
if X86_64
    const NumberOfWasmArgumentJSRs = 6
elsif ARM64 or ARM64E or RISCV64
    const NumberOfWasmArgumentJSRs = 8
elsif ARMv7
    const NumberOfWasmArgumentJSRs = 2
else
    error
end

const NumberOfWasmArgumentFPRs = 8

const NumberOfWasmArguments = NumberOfWasmArgumentJSRs + NumberOfWasmArgumentFPRs

# All callee saves must match the definition in WasmCallee.cpp

# These must match the definition in GPRInfo.h
if X86_64 or ARM64 or ARM64E or RISCV64
    const wasmInstance = csr0
    const memoryBase = csr3
    const boundsCheckingSize = csr4
elsif ARMv7
    const wasmInstance = csr0
    const memoryBase = invalidGPR
    const boundsCheckingSize = invalidGPR
else
    error
end

# This must match the definition in LowLevelInterpreter.asm
if X86_64
    const PB = csr2
elsif ARM64 or ARM64E or RISCV64
    const PB = csr7
elsif ARMv7
    const PB = csr1
else
    error
end

# Helper macros

# On JSVALUE64, each 64-bit argument GPR holds one whole Wasm value.
# On JSVALUE32_64, a consecutive pair of even/odd numbered GPRs hold a single
# Wasm value (even if that value is i32/f32, the odd numbered GPR holds the
# more significant word).
macro forEachArgumentJSR(fn)
    if ARM64 or ARM64E
        fn(0 * 8, wa0, wa1)
        fn(2 * 8, wa2, wa3)
        fn(4 * 8, wa4, wa5)
        fn(6 * 8, wa6, wa7)
    elsif JSVALUE64
        fn(0 * 8, wa0)
        fn(1 * 8, wa1)
        fn(2 * 8, wa2)
        fn(3 * 8, wa3)
        fn(4 * 8, wa4)
        fn(5 * 8, wa5)
    else
        fn(0 * 8, wa1, wa0)
        fn(1 * 8, wa3, wa2)
    end
end

macro forEachArgumentFPR(fn)
    if ARM64 or ARM64E
        fn((NumberOfWasmArgumentJSRs + 0) * 8, wfa0, wfa1)
        fn((NumberOfWasmArgumentJSRs + 2) * 8, wfa2, wfa3)
        fn((NumberOfWasmArgumentJSRs + 4) * 8, wfa4, wfa5)
        fn((NumberOfWasmArgumentJSRs + 6) * 8, wfa6, wfa7)
    else
        fn((NumberOfWasmArgumentJSRs + 0) * 8, wfa0)
        fn((NumberOfWasmArgumentJSRs + 1) * 8, wfa1)
        fn((NumberOfWasmArgumentJSRs + 2) * 8, wfa2)
        fn((NumberOfWasmArgumentJSRs + 3) * 8, wfa3)
        fn((NumberOfWasmArgumentJSRs + 4) * 8, wfa4)
        fn((NumberOfWasmArgumentJSRs + 5) * 8, wfa5)
        fn((NumberOfWasmArgumentJSRs + 6) * 8, wfa6)
        fn((NumberOfWasmArgumentJSRs + 7) * 8, wfa7)
    end
end

macro preserveCalleeSavesUsedByWasm()
    # NOTE: We intentionally don't save memoryBase and boundsCheckingSize here. See the comment
    # in restoreCalleeSavesUsedByWasm() below for why.
    subp CalleeSaveSpaceStackAligned, sp
    if ARM64 or ARM64E
        storepairq wasmInstance, PB, -16[cfr]
    elsif X86_64 or RISCV64
        storep PB, -0x8[cfr]
        storep wasmInstance, -0x10[cfr]
    elsif ARMv7
        storep PB, -4[cfr]
        storep wasmInstance, -8[cfr]
    else
        error
    end
end

macro restoreCalleeSavesUsedByWasm()
    # NOTE: We intentionally don't restore memoryBase and boundsCheckingSize here. These are saved
    # and restored when entering Wasm by the JSToWasm wrapper and changes to them are meant
    # to be observable within the same Wasm module.
    if ARM64 or ARM64E
        loadpairq -16[cfr], wasmInstance, PB
    elsif X86_64 or RISCV64
        loadp -0x8[cfr], PB
        loadp -0x10[cfr], wasmInstance
    elsif ARMv7
        loadp -4[cfr], PB
        loadp -8[cfr], wasmInstance
    else
        error
    end
end

macro preserveGPRsUsedByTailCall(gpr0, gpr1)
    storep gpr0, ThisArgumentOffset[sp]
    storep gpr1, ArgumentCountIncludingThis[sp]
end

macro restoreGPRsUsedByTailCall(gpr0, gpr1)
    loadp ThisArgumentOffset[sp], gpr0
    loadp ArgumentCountIncludingThis[sp], gpr1
end

macro preserveReturnAddress(scratch)
if X86_64
    loadp ReturnPC[cfr], scratch
    storep scratch, ReturnPC[sp]
elsif ARM64 or ARM64E or ARMv7 or RISCV64
    loadp ReturnPC[cfr], lr
end
end

macro usePreviousFrame()
    if ARM64 or ARM64E
        loadpairq -PtrSize[cfr], PB, cfr
    elsif ARMv7 or X86_64 or RISCV64
        loadp -PtrSize[cfr], PB
        loadp [cfr], cfr
    else
        error
    end
end

macro reloadMemoryRegistersFromInstance(instance, scratch1)
if not ARMv7
    loadp JSWebAssemblyInstance::m_cachedMemory[instance], memoryBase
    loadp JSWebAssemblyInstance::m_cachedBoundsCheckingSize[instance], boundsCheckingSize
    cagedPrimitiveMayBeNull(memoryBase, scratch1) # If boundsCheckingSize is 0, pointer can be a nullptr.
end
end

macro throwException(exception)
    storei constexpr Wasm::ExceptionType::%exception%, ArgumentCountIncludingThis + PayloadOffset[cfr]
    jmp _wasm_throw_from_slow_path_trampoline
end

macro callWasmSlowPath(slowPath)
    storei PC, CallSiteIndex[cfr]
    prepareStateForCCall()
    move cfr, a0
    move PC, a1
    move wasmInstance, a2
    cCall3(slowPath)
    restoreStateAfterCCall()
end

macro callWasmCallSlowPath(slowPath, action)
    storei PC, CallSiteIndex[cfr]
    prepareStateForCCall()
    move cfr, a0
    move PC, a1
    move wasmInstance, a2
    cCall3(slowPath)
    action(r0, r1)
end

macro forEachVectorArgument(fn)
     const base = NumberOfWasmArgumentJSRs * 8
     fn(base + 0 * 16, -(base + 0 * 16 + 8), wfa0)
     fn(base + 1 * 16, -(base + 1 * 16 + 8), wfa1)
     fn(base + 2 * 16, -(base + 2 * 16 + 8), wfa2)
     fn(base + 3 * 16, -(base + 3 * 16 + 8), wfa3)
     fn(base + 4 * 16, -(base + 4 * 16 + 8), wfa4)
     fn(base + 5 * 16, -(base + 5 * 16 + 8), wfa5)
     fn(base + 6 * 16, -(base + 6 * 16 + 8), wfa6)
     fn(base + 7 * 16, -(base + 7 * 16 + 8), wfa7)
end

# Tier up immediately, while saving full vectors in argument FPRs
macro wasmPrologueSIMD(slow_path)
if (WEBASSEMBLY_BBQJIT or WEBASSEMBLY_OMGJIT) and not ARMv7
    preserveCallerPCAndCFR()
    preserveCalleeSavesUsedByWasm()
    reloadMemoryRegistersFromInstance(wasmInstance, ws0)

    storep wasmInstance, CodeBlock[cfr]
    loadp Callee[cfr], ws0
if JSVALUE64
    andp ~(constexpr JSValue::NativeCalleeTag), ws0
end
    leap WTFConfig + constexpr WTF::offsetOfWTFConfigLowestAccessibleAddress, ws1
    loadp [ws1], ws1
    addp ws1, ws0
    storep ws0, UnboxedWasmCalleeStackSlot[cfr]

    # Get new sp in ws1 and check stack height.
    # This should match the calculation of m_stackSize, but with double the size for fpr arg storage and no locals.
    move 8 + 8 * 2 + constexpr CallFrame::headerSizeInRegisters + 1, ws1
    lshiftp 3, ws1
    addp maxFrameExtentForSlowPathCall, ws1
    subp cfr, ws1, ws1

if not JSVALUE64
    subp 8, ws1 # align stack pointer
end

if not ADDRESS64
    bpa ws1, cfr, .stackOverflow
end
    bplteq JSWebAssemblyInstance::m_softStackLimit[wasmInstance], ws1, .stackHeightOK

.stackOverflow:
    throwException(StackOverflow)

.stackHeightOK:
    move ws1, sp

if ARM64 or ARM64E
    forEachArgumentJSR(macro (offset, gpr1, gpr2)
        storepairq gpr2, gpr1, -offset - 16 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr]
    end)
elsif JSVALUE64
    forEachArgumentJSR(macro (offset, gpr)
        storeq gpr, -offset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr]
    end)
else
    forEachArgumentJSR(macro (offset, gprMsw, gpLsw)
        store2ia gpLsw, gprMsw, -offset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr]
    end)
end

    forEachVectorArgument(macro (offset, negOffset, fpr)
        storev fpr, negOffset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr]
    end)

    slow_path()
    move r0, ws0

if ARM64 or ARM64E
    forEachArgumentJSR(macro (offset, gpr1, gpr2)
        loadpairq -offset - 16 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr], gpr2, gpr1
    end)
elsif JSVALUE64
    forEachArgumentJSR(macro (offset, gpr)
        loadq -offset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr], gpr
    end)
else
    forEachArgumentJSR(macro (offset, gprMsw, gpLsw)
        load2ia -offset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr], gpLsw, gprMsw
    end)
end

    forEachVectorArgument(macro (offset, negOffset, fpr)
        loadv negOffset - 8 - CalleeSaveSpaceAsVirtualRegisters * 8[cfr], fpr
    end)

    restoreCalleeSavesUsedByWasm()
    restoreCallerPCAndCFR()
    if ARM64E
        leap _g_config, ws1
        jmp JSCConfigGateMapOffset + (constexpr Gate::wasmOSREntry) * PtrSize[ws1], NativeToJITGatePtrTag # WasmEntryPtrTag
    else
        jmp ws0, WasmEntryPtrTag
    end
end
    break
end

if ARMv7
macro branchIfWasmException(exceptionTarget)
    loadp CodeBlock[cfr], t3
    loadp JSWebAssemblyInstance::m_vm[t3], t3
    btpz VM::m_exception[t3], .noException
    jmp exceptionTarget
.noException:
end
end

macro zeroExtend32ToWord(r)
    if JSVALUE64
        andq 0xffffffff, r
    end
end

macro boxInt32(r, rTag)
    if JSVALUE64
        orq constexpr JSValue::NumberTag, r
    else
        move constexpr JSValue::Int32Tag, rTag
    end
end

// If you change this, make sure to modify JSToWasm.cpp:createJSToWasmJITShared
op(js_to_wasm_wrapper_entry, macro ()
    if not WEBASSEMBLY or C_LOOP
        error
    end

    macro clobberVolatileRegisters()
        if ARM64 or ARM64E
            emit "movz  x9, #0xBAD"
            emit "movz x10, #0xBAD"
            emit "movz x11, #0xBAD"
            emit "movz x12, #0xBAD"
            emit "movz x13, #0xBAD"
            emit "movz x14, #0xBAD"
            emit "movz x15, #0xBAD"
            emit "movz x16, #0xBAD"
            emit "movz x17, #0xBAD"
            emit "movz x18, #0xBAD"
        elsif ARMv7
            emit "mov r4, #0xBAD"
            emit "mov r5, #0xBAD"
            emit "mov r6, #0xBAD"
            emit "mov r8, #0xBAD"
            emit "mov r9, #0xBAD"
            emit "mov r12, #0xBAD"
        end
    end

    macro repeat(scratch, f)
        move 0xBEEF, scratch
        f(0)
        f(1)
        f(2)
        f(3)
        f(4)
        f(5)
        f(6)
        f(7)
        f(8)
        f(9)
        f(10)
        f(11)
        f(12)
        f(13)
        f(14)
        f(15)
        f(16)
        f(17)
        f(18)
        f(19)
        f(20)
        f(21)
        f(22)
        f(23)
        f(24)
        f(25)
        f(26)
        f(27)
        f(28)
        f(29)
    end

    macro saveJSEntrypointRegisters()
        subp constexpr Wasm::JSEntrypointCallee::SpillStackSpaceAligned, sp
        if ARM64 or ARM64E
            storepairq memoryBase, boundsCheckingSize, -2 * SlotSize[cfr]
            storep wasmInstance, -3 * SlotSize[cfr]
        elsif X86_64
            # These must match the wasmToJS thunk, since the unwinder won't be able to tell who made this frame.
            storep boundsCheckingSize, -1 * SlotSize[cfr]
            storep memoryBase, -2 * SlotSize[cfr]
            storep wasmInstance, -3 * SlotSize[cfr]
        else
            storei wasmInstance, -1 * SlotSize[cfr]
        end
    end

    macro restoreJSEntrypointRegisters()
        if ARM64 or ARM64E
            loadpairq -2 * SlotSize[cfr], memoryBase, boundsCheckingSize
            loadp -3 * SlotSize[cfr], wasmInstance
        elsif X86_64
            loadp -1 * SlotSize[cfr], boundsCheckingSize
            loadp -2 * SlotSize[cfr], memoryBase
            loadp -3 * SlotSize[cfr], wasmInstance
        else
            loadi -1 * SlotSize[cfr], wasmInstance
        end
        addp constexpr Wasm::JSEntrypointCallee::SpillStackSpaceAligned, sp
    end

    macro getWebAssemblyFunctionAndSetNativeCalleeAndInstance(webAssemblyFunctionOut, scratch)
        # Re-load WebAssemblyFunction Callee
        loadp Callee[cfr], webAssemblyFunctionOut

        # Replace the WebAssemblyFunction Callee with our JSToWasm NativeCallee
        loadp WebAssemblyFunction::m_boxedJSToWasmCallee[webAssemblyFunctionOut], scratch
        storep scratch, Callee[cfr] # JSToWasmCallee
        if not JSVALUE64
            move constexpr JSValue::NativeCalleeTag, scratch
            storep scratch, TagOffset + Callee[cfr]
        end
        storep wasmInstance, CodeBlock[cfr]
    end

if ASSERT_ENABLED
    clobberVolatileRegisters()
end

    tagReturnAddress sp
    preserveCallerPCAndCFR()
    saveJSEntrypointRegisters()

    # Load data from the entry callee
    # This was written by doVMEntry
    loadp Callee[cfr], ws0 # WebAssemblyFunction*
    loadp WebAssemblyFunction::m_instance[ws0], wasmInstance

    # Memory
    if ARM64 or ARM64E
        loadpairq JSWebAssemblyInstance::m_cachedMemory[wasmInstance], memoryBase, boundsCheckingSize
    elsif X86_64
        loadp JSWebAssemblyInstance::m_cachedMemory[wasmInstance], memoryBase
        loadp JSWebAssemblyInstance::m_cachedBoundsCheckingSize[wasmInstance], boundsCheckingSize
    end
    if not ARMv7
        cagedPrimitiveMayBeNull(memoryBase, wa0)
    end

    # Allocate stack space
    loadi WebAssemblyFunction::m_frameSize[ws0], wa0
    subp sp, wa0, wa0

if not ADDRESS64
    bpa wa0, cfr, .stackOverflow
end
    bplteq wa0, JSWebAssemblyInstance::m_softStackLimit[wasmInstance], .stackOverflow

    move wa0, sp

if ASSERT_ENABLED
    repeat(wa0, macro (i)
        storep wa0, -i * SlotSize + constexpr Wasm::JSEntrypointCallee::RegisterStackSpaceAligned[sp]
    end)
end

    # Prepare frame
    move ws0, a2
    move cfr, a1
    move sp, a0
    cCall3(_operationJSToWasmEntryWrapperBuildFrame)

    btpnz r1, .buildEntryFrameThrew
    move r0, ws0

    # Arguments

if ARM64 or ARM64E
    forEachArgumentJSR(macro (offset, gpr1, gpr2)
        loadpairq offset[sp], gpr1, gpr2
    end)
elsif JSVALUE64
    forEachArgumentJSR(macro (offset, gpr)
        loadq offset[sp], gpr
    end)
else
    forEachArgumentJSR(macro (offset, gprMsw, gpLsw)
        load2ia offset[sp], gpLsw, gprMsw
    end)
end

if ARM64 or ARM64E
    forEachArgumentFPR(macro (offset, fpr1, fpr2)
        loadpaird offset[sp], fpr1, fpr2
    end)
else
    forEachArgumentFPR(macro (offset, fpr)
        loadd offset[sp], fpr
    end)
end

    # Pop argument space values
    addp constexpr Wasm::JSEntrypointCallee::RegisterStackSpaceAligned, sp

if ASSERT_ENABLED
    repeat(ws1, macro (i)
        storep ws1, -i * SlotSize[sp]
    end)
end

    getWebAssemblyFunctionAndSetNativeCalleeAndInstance(ws1, ws0)

    # Load callee entrypoint
    loadp WebAssemblyFunction::m_importableFunction + Wasm::WasmOrJSImportableFunction::entrypointLoadLocation[ws1], ws0
    loadp [ws0], ws0

    # Set the callee's interpreter Wasm::Callee
if JSVALUE64
    transferp WebAssemblyFunction::m_boxedWasmCallee[ws1], constexpr (CallFrameSlot::callee - CallerFrameAndPC::sizeInRegisters) * 8[sp]
else
    transferp WebAssemblyFunction::m_boxedWasmCallee + PayloadOffset[ws1], constexpr (CallFrameSlot::callee - CallerFrameAndPC::sizeInRegisters) * 8 + PayloadOffset[sp]
    transferp WebAssemblyFunction::m_boxedWasmCallee + TagOffset[ws1], constexpr (CallFrameSlot::callee - CallerFrameAndPC::sizeInRegisters) * 8 + TagOffset[sp]
end

    call ws0, WasmEntryPtrTag

if ASSERT_ENABLED
    clobberVolatileRegisters()
end

    # Restore SP
    loadp Callee[cfr], ws0 # CalleeBits(JSEntrypointCallee*)
if JSVALUE64
    andp ~(constexpr JSValue::NativeCalleeTag), ws0
end
    leap WTFConfig + constexpr WTF::offsetOfWTFConfigLowestAccessibleAddress, ws1
    loadp [ws1], ws1
    addp ws1, ws0
    loadi Wasm::JSEntrypointCallee::m_frameSize[ws0], ws1
    subp cfr, ws1, ws1
    move ws1, sp
    subp constexpr Wasm::JSEntrypointCallee::SpillStackSpaceAligned, sp

if ASSERT_ENABLED
    repeat(ws0, macro (i)
        storep ws0, -i * SlotSize + constexpr Wasm::JSEntrypointCallee::RegisterStackSpaceAligned[sp]
    end)
end

    # Save return registers
    macro forEachReturnWasmJSR(fn)
        if ARM64 or ARM64E
            fn(0 * 8, wa0, wa1)
            fn(2 * 8, wa2, wa3)
            fn(4 * 8, wa4, wa5)
            fn(6 * 8, wa6, wa7)
        elsif X86_64
            fn(0 * 8, wa0)
            fn(1 * 8, wa1)
            fn(2 * 8, wa2)
            fn(3 * 8, wa3)
            fn(4 * 8, wa4)
            fn(5 * 8, wa5)
        elsif JSVALUE64
            fn(0 * 8, wa0)
            fn(1 * 8, wa1)
            fn(2 * 8, wa2)
            fn(3 * 8, wa3)
            fn(4 * 8, wa4)
            fn(5 * 8, wa5)
        else
            fn(0 * 8, wa1, wa0)
            fn(1 * 8, wa3, wa2)
        end
    end
    macro forEachReturnJSJSR(fn)
        if ARM64 or ARM64E
            fn(0 * 8, r0, r1)
        elsif X86_64
            fn(0 * 8, r0)
            fn(1 * 8, r1)
        elsif JSVALUE64
            fn(0 * 8, r0)
            fn(1 * 8, r1)
        else
            fn(0 * 8, r1, r0)
        end
    end

if ARM64 or ARM64E
    forEachReturnWasmJSR(macro (offset, gpr1, gpr2)
        storepairq gpr1, gpr2, offset[sp]
    end)
elsif JSVALUE64
    forEachReturnWasmJSR(macro (offset, gpr)
        storeq gpr, offset[sp]
    end)
else
    forEachReturnWasmJSR(macro (offset, gprMsw, gpLsw)
        store2ia gpLsw, gprMsw, offset[sp]
    end)
end

if ARM64 or ARM64E
    forEachArgumentFPR(macro (offset, fpr1, fpr2)
        storepaird fpr1, fpr2, offset[sp]
    end)
else
    forEachArgumentFPR(macro (offset, fpr)
        stored fpr, offset[sp]
    end)
end

    # Prepare frame
    move sp, a0
    move cfr, a1
    cCall2(_operationJSToWasmEntryWrapperBuildReturnFrame)

if ARMv7
    branchIfWasmException(.unwind)
else
    btpnz r1, .unwind
end

    # Clean up and return
    restoreJSEntrypointRegisters()
if ASSERT_ENABLED
    clobberVolatileRegisters()
end
    restoreCallerPCAndCFR()
    ret

    # We need to set our NativeCallee/instance here since haven't done it already and wasm_throw_from_slow_path_trampoline expects them.
.stackOverflow:
    getWebAssemblyFunctionAndSetNativeCalleeAndInstance(ws1, ws0)
    throwException(StackOverflow)

.buildEntryFrameThrew:
    getWebAssemblyFunctionAndSetNativeCalleeAndInstance(ws1, ws0)

.unwind:
    loadp JSWebAssemblyInstance::m_vm[wasmInstance], a0
    copyCalleeSavesToVMEntryFrameCalleeSavesBuffer(a0, a1)

# Should be (not USE_BUILTIN_FRAME_ADDRESS) but need to keep down the size of LLIntAssembly.h
if ASSERT_ENABLED or ARMv7
    storep cfr, JSWebAssemblyInstance::m_temporaryCallFrame[wasmInstance]
end

    move wasmInstance, a0
    call _operationWasmUnwind
    jumpToException()
end)

op(wasm_to_wasm_ipint_wrapper_entry, macro()
    # We have only pushed PC (intel) or pushed nothing(others), and we
    # are still in the caller frame.
if X86_64
    loadp (Callee - CallerFrameAndPCSize + 8)[sp], ws0
else
    loadp (Callee - CallerFrameAndPCSize)[sp], ws0
end

if JSVALUE64
    andp ~(constexpr JSValue::NativeCalleeTag), ws0
end
    leap WTFConfig + constexpr WTF::offsetOfWTFConfigLowestAccessibleAddress, ws1
    loadp [ws1], ws1
    addp ws1, ws0

    loadp JSC::Wasm::IPIntCallee::m_entrypoint[ws0], ws0

    # Load the instance
if X86_64
    loadp (CodeBlock - CallerFrameAndPCSize + 8)[sp], wasmInstance
else
    loadp (CodeBlock - CallerFrameAndPCSize)[sp], wasmInstance
end

    # Memory
    if ARM64 or ARM64E
        loadpairq JSWebAssemblyInstance::m_cachedMemory[wasmInstance], memoryBase, boundsCheckingSize
    elsif X86_64
        loadp JSWebAssemblyInstance::m_cachedMemory[wasmInstance], memoryBase
        loadp JSWebAssemblyInstance::m_cachedBoundsCheckingSize[wasmInstance], boundsCheckingSize
    end
    if not ARMv7
        cagedPrimitiveMayBeNull(memoryBase, ws1)
    end

    jmp ws0, WasmEntryPtrTag
end)

# This is the interpreted analogue to WasmToJS.cpp:wasmToJS
op(wasm_to_js_wrapper_entry, macro()
    # We have only pushed PC (intel) or pushed nothing(others), and we
    # are still in the caller frame.
    # Load this before we create the stack frame, since we lose old cfr, which we wrote Callee to

    # We repurpose this slot temporarily for a WasmCallableFunction* from resolveWasmCall and friends.
    tagReturnAddress sp
    preserveCallerPCAndCFR()

    const RegisterSpaceScratchSize = 0x80
    subp (WasmToJSScratchSpaceSize + RegisterSpaceScratchSize), sp

    loadp CodeBlock[cfr], ws0
    storep ws0, WasmToJSCallableFunctionSlot[cfr]

    # Store all the registers here

if ARM64 or ARM64E
    forEachArgumentJSR(macro (offset, gpr1, gpr2)
        storepairq gpr1, gpr2, offset[sp]
    end)
elsif JSVALUE64
    forEachArgumentJSR(macro (offset, gpr)
        storeq gpr, offset[sp]
    end)
else
    forEachArgumentJSR(macro (offset, gprMsw, gpLsw)
        store2ia gpLsw, gprMsw, offset[sp]
    end)
end

if ARM64 or ARM64E
    forEachArgumentFPR(macro (offset, fpr1, fpr2)
        storepaird fpr1, fpr2, offset[sp]
    end)
else
    forEachArgumentFPR(macro (offset, fpr)
        stored fpr, offset[sp]
    end)
end

if ASSERT_ENABLED or ARMv7
    storep cfr, JSWebAssemblyInstance::m_temporaryCallFrame[wasmInstance]
end

    move wasmInstance, a0
    move ws0, a1
    cCall2(_operationGetWasmCalleeStackSize)

    move sp, a2
    subp r0, sp
    move sp, a0
    move cfr, a1
    move wasmInstance, a3
    cCall4(_operationWasmToJSExitMarshalArguments)
    btpnz r1, .oom

    bineq r0, 0, .safe
    move wasmInstance, r0
    move (constexpr Wasm::ExceptionType::TypeErrorInvalidValueUse), r1
    cCall2(_operationWasmToJSException)
    jumpToException()
    break

.safe:
    loadp WasmToJSCallableFunctionSlot[cfr], t2
    loadp JSC::Wasm::WasmOrJSImportableFunctionCallLinkInfo::importFunction[t2], t0
if not JSVALUE64
    move (constexpr JSValue::CellTag), t1
end
    loadp JSC::Wasm::WasmOrJSImportableFunctionCallLinkInfo::callLinkInfo[t2], t2

    # calleeGPR = t0
    # callLinkInfoGPR = t2
    # callTargetGPR = t5
    loadp CallLinkInfo::m_monomorphicCallDestination[t2], t5

    # scratch = t3
    loadp CallLinkInfo::m_callee[t2], t3
    bpeq t3, t0, .found
    btpnz t3, (constexpr CallLinkInfo::polymorphicCalleeMask), .found

.notfound:
if ARM64 or ARM64E
    pcrtoaddr _llint_default_call_trampoline, t5
else
    leap (_llint_default_call_trampoline), t5
end
    loadp CallLinkInfo::m_codeBlock[t2], t3
    storep t3, (CodeBlock - CallerFrameAndPCSize)[sp]
    call _llint_default_call_trampoline
    jmp .postcall
.found:
    # jit.transferPtr CallLinkInfo::codeBlock[t2], CodeBlock[cfr]
    loadp CallLinkInfo::m_codeBlock[t2], t3
    storep t3, (CodeBlock - CallerFrameAndPCSize)[sp]
    call t5, JSEntryPtrTag

.postcall:
    storep r0, [sp]
if not JSVALUE64
    storep r1, TagOffset[sp]
end

    loadp WasmToJSCallableFunctionSlot[cfr], a0
    call _operationWasmToJSExitNeedToUnpack
    btpnz r0, .unpack

    move sp, a0
    move cfr, a1
    move wasmInstance, a2
    cCall3(_operationWasmToJSExitMarshalReturnValues)
    btpnz r0, .handleException
    jmp .end

.unpack:

    move r0, a1
    move wasmInstance, a0
    move sp, a2
    move cfr, a3
    cCall4(_operationWasmToJSExitIterateResults)
    btpnz r0, .handleException

.end:
    macro forEachReturnWasmJSR(fn)
        if ARM64 or ARM64E
            fn(0 * 8, wa0, wa1)
            fn(2 * 8, wa2, wa3)
            fn(4 * 8, wa4, wa5)
            fn(6 * 8, wa6, wa7)
        elsif X86_64
            fn(0 * 8, wa0)
            fn(1 * 8, wa1)
            fn(2 * 8, wa2)
            fn(3 * 8, wa3)
            fn(4 * 8, wa4)
            fn(5 * 8, wa5)
        elsif JSVALUE64
            fn(0 * 8, wa0)
            fn(1 * 8, wa1)
            fn(2 * 8, wa2)
            fn(3 * 8, wa3)
            fn(4 * 8, wa4)
            fn(5 * 8, wa5)
        else
            fn(0 * 8, wa1, wa0)
            fn(1 * 8, wa3, wa2)
        end
    end

if ARM64 or ARM64E
    forEachReturnWasmJSR(macro (offset, gpr1, gpr2)
        loadpairq offset[sp], gpr1, gpr2
    end)
elsif JSVALUE64
    forEachReturnWasmJSR(macro (offset, gpr)
        loadq offset[sp], gpr
    end)
else
    forEachReturnWasmJSR(macro (offset, gprMsw, gprLsw)
        load2ia offset[sp], gprLsw, gprMsw
    end)
end

if ARM64 or ARM64E
    forEachArgumentFPR(macro (offset, fpr1, fpr2)
        loadpaird offset[sp], fpr1, fpr2
    end)
else
    forEachArgumentFPR(macro (offset, fpr)
        loadd offset[sp], fpr
    end)
end

    loadp CodeBlock[cfr], wasmInstance
    restoreCallerPCAndCFR()
    ret

.handleException:
    loadp JSWebAssemblyInstance::m_vm[wasmInstance], a0
    copyCalleeSavesToVMEntryFrameCalleeSavesBuffer(a0, a1)

if ASSERT_ENABLED or ARMv7
    storep cfr, JSWebAssemblyInstance::m_temporaryCallFrame[wasmInstance]
end

    move wasmInstance, a0
    call _operationWasmUnwind
    jumpToException()

.oom:
    throwException(OutOfMemory)

end)

macro traceExecution()
    if TRACING
        callWasmSlowPath(_slow_path_wasm_trace)
    end
end

macro commonWasmOp(opcodeName, opcodeStruct, prologue, fn)
    commonOp(opcodeName, prologue, macro(size)
        fn(macro(fn2)
            fn2(opcodeName, opcodeStruct, size)
        end)
    end)
end

# Entry point

op(ipint_function_prologue_simd_trampoline, macro ()
    tagReturnAddress sp
    jmp _ipint_function_prologue_simd
end)

op(ipint_function_prologue_simd, macro ()
    if not WEBASSEMBLY or C_LOOP
        error
    end

    wasmPrologueSIMD(macro()
        move wasmInstance, a0
        move cfr, a1
        cCall2(_ipint_extern_simd_go_straight_to_bbq)
    end)
    break
end)

macro jumpToException()
    if ARM64E
        move r0, a0
        leap _g_config, a1
        jmp JSCConfigGateMapOffset + (constexpr Gate::exceptionHandler) * PtrSize[a1], NativeToJITGatePtrTag # ExceptionHandlerPtrTag
    else
        jmp r0, ExceptionHandlerPtrTag
    end
end

op(wasm_throw_from_slow_path_trampoline, macro ()
    loadp JSWebAssemblyInstance::m_vm[wasmInstance], t5
    loadp VM::topEntryFrame[t5], t5
    copyCalleeSavesToEntryFrameCalleeSavesBuffer(t5)

    move cfr, a0
    move wasmInstance, a1
    # Slow paths and the throwException macro store the exception code in the ArgumentCountIncludingThis slot
    loadi ArgumentCountIncludingThis + PayloadOffset[cfr], a2
    storei 0, CallSiteIndex[cfr]
    cCall3(_slow_path_wasm_throw_exception)
    jumpToException()
end)

macro wasm_throw_from_fault_handler(instance)
    # instance should be in a2 when we get here
    loadp JSWebAssemblyInstance::m_vm[instance], a0
    loadp VM::topEntryFrame[a0], a0
    copyCalleeSavesToEntryFrameCalleeSavesBuffer(a0)

    move cfr, a0
    move a2, a1
    move constexpr Wasm::ExceptionType::OutOfBoundsMemoryAccess, a2

    storei 0, CallSiteIndex[cfr]
    cCall3(_slow_path_wasm_throw_exception)
    jumpToException()
end

op(wasm_throw_from_fault_handler_trampoline_reg_instance, macro ()
    move wasmInstance, a2
    wasm_throw_from_fault_handler(a2)
end)
