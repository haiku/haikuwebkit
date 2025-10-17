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

#include "JSInternalFieldObjectImpl.h"

namespace JSC {

const static uint8_t JSPromiseReactionNumberOfInternalFields = 5;

class JSPromiseReaction final : public JSInternalFieldObjectImpl<JSPromiseReactionNumberOfInternalFields> {
public:
    using Base = JSInternalFieldObjectImpl<JSPromiseReactionNumberOfInternalFields>;

    DECLARE_EXPORT_INFO;
    DECLARE_VISIT_CHILDREN;

    enum class Field : uint8_t {
        Promise = 0,
        OnFulfilled,
        OnRejected,
        Context,
        Next,
    };
    static_assert(numberOfInternalFields == JSPromiseReactionNumberOfInternalFields);

    static std::array<JSValue, numberOfInternalFields> initialValues()
    {
        return { {
            jsUndefined(),
            jsUndefined(),
            jsUndefined(),
            jsUndefined(),
            jsUndefined(),
        } };
    }

    const WriteBarrier<Unknown>& internalField(Field field) const { return Base::internalField(static_cast<uint32_t>(field)); }
    WriteBarrier<Unknown>& internalField(Field field) { return Base::internalField(static_cast<uint32_t>(field)); }

    template<typename CellType, SubspaceAccess mode>
    static GCClient::IsoSubspace* subspaceFor(VM& vm)
    {
        return vm.promiseReactionSpace<mode>();
    }

    static Structure* createStructure(VM&, JSGlobalObject*, JSValue);

    static JSPromiseReaction* createWithInitialValues(VM&, Structure*);
    static JSPromiseReaction* create(VM&, Structure*, JSValue promise, JSValue onFulfilled, JSValue onRejected, JSValue context, JSValue next);

    JSValue promise() const { return internalField(Field::Promise).get(); }
    JSValue onFulfilled() const { return internalField(Field::OnFulfilled).get(); }
    JSValue onRejected() const { return internalField(Field::OnRejected).get(); }
    JSValue context() const { return internalField(Field::Context).get(); }
    JSValue next() const { return internalField(Field::Next).get(); }

    void setPromise(VM& vm, JSValue value) { internalField(Field::Promise).set(vm, this, value); }
    void setOnFulfilled(VM& vm, JSValue value) { internalField(Field::OnFulfilled).set(vm, this, value); }
    void setOnRejected(VM& vm, JSValue value) { internalField(Field::OnRejected).set(vm, this, value); }
    void setContext(VM& vm, JSValue value) { internalField(Field::Context).set(vm, this, value); }
    void setNext(VM& vm, JSValue value) { internalField(Field::Next).set(vm, this, value); }

private:
    JSPromiseReaction(VM& vm, Structure* structure, JSValue promise, JSValue onFulfilled, JSValue onRejected, JSValue context, JSValue next)
        : Base(vm, structure)
    {
        internalField(Field::Promise).setWithoutWriteBarrier(promise);
        internalField(Field::OnFulfilled).setWithoutWriteBarrier(onFulfilled);
        internalField(Field::OnRejected).setWithoutWriteBarrier(onRejected);
        internalField(Field::Context).setWithoutWriteBarrier(context);
        internalField(Field::Next).setWithoutWriteBarrier(next);
    }
};

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSPromiseReaction);

JSC_DECLARE_HOST_FUNCTION(promiseReactionPrivateFuncCreate);

} // namespace JSC
