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
#include "JSPromiseReaction.h"

#include "JSCInlines.h"
#include "JSInternalFieldObjectImplInlines.h"

namespace JSC {

const ClassInfo JSPromiseReaction::s_info = { "PromiseReaction"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSPromiseReaction) };

JSPromiseReaction* JSPromiseReaction::createWithInitialValues(VM& vm, Structure* structure)
{
    auto values = initialValues();
    JSPromiseReaction* result = new (NotNull, allocateCell<JSPromiseReaction>(vm)) JSPromiseReaction(vm, structure, values[0], values[1], values[2], values[3], values[4]);
    result->finishCreation(vm);
    return result;
}

JSPromiseReaction* JSPromiseReaction::create(VM& vm, Structure* structure, JSValue promise, JSValue onFulfilled, JSValue onRejected, JSValue context, JSValue next)
{
    JSPromiseReaction* result = new (NotNull, allocateCell<JSPromiseReaction>(vm)) JSPromiseReaction(vm, structure, promise, onFulfilled, onRejected, context, next);
    result->finishCreation(vm);
    return result;
}

template<typename Visitor>
void JSPromiseReaction::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = jsCast<JSPromiseReaction*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
}

Structure* JSPromiseReaction::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(JSPromiseReactionType, StructureFlags), info());
}

DEFINE_VISIT_CHILDREN(JSPromiseReaction);

JSC_DEFINE_HOST_FUNCTION(promiseReactionPrivateFuncCreate, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();

    JSValue promise = callFrame->uncheckedArgument(0);
    JSValue onFulfilled = callFrame->uncheckedArgument(1);
    JSValue onRejected = callFrame->uncheckedArgument(2);
    JSValue context = callFrame->uncheckedArgument(3);
    JSValue next = callFrame->uncheckedArgument(4);

    return JSValue::encode(JSPromiseReaction::create(vm, globalObject->promiseReactionStructure(), promise, onFulfilled, onRejected, context, next));
}

} // namespace JSC
