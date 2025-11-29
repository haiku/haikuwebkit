/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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
#include "JSPromiseConstructor.h"

#include "BuiltinNames.h"
#include "CachedCall.h"
#include "GetterSetter.h"
#include "InterpreterInlines.h"
#include "IteratorOperations.h"
#include "JSCBuiltins.h"
#include "JSCInlines.h"
#include "JSPromise.h"
#include "JSPromisePrototype.h"
#include "runtime/JSCJSValue.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSPromiseConstructor);
static JSC_DECLARE_HOST_FUNCTION(promiseConstructorFuncResolve);
static JSC_DECLARE_HOST_FUNCTION(promiseConstructorFuncReject);
static JSC_DECLARE_HOST_FUNCTION(promiseConstructorFuncWithResolvers);
static JSC_DECLARE_HOST_FUNCTION(promiseConstructorFuncRace);

}

#include "JSPromiseConstructor.lut.h"

namespace JSC {

const ClassInfo JSPromiseConstructor::s_info = { "Function"_s, &Base::s_info, &promiseConstructorTable, nullptr, CREATE_METHOD_TABLE(JSPromiseConstructor) };

/* Source for JSPromiseConstructor.lut.h
@begin promiseConstructorTable
  resolve         promiseConstructorFuncResolve        DontEnum|Function 1 PromiseConstructorResolveIntrinsic
  reject          promiseConstructorFuncReject         DontEnum|Function 1 PromiseConstructorRejectIntrinsic
  race            promiseConstructorFuncRace           DontEnum|Function 1
  all             JSBuiltin                            DontEnum|Function 1
  allSettled      JSBuiltin                            DontEnum|Function 1
  any             JSBuiltin                            DontEnum|Function 1
  withResolvers   promiseConstructorFuncWithResolvers  DontEnum|Function 0
@end
*/

JSPromiseConstructor* JSPromiseConstructor::create(VM& vm, Structure* structure, JSPromisePrototype* promisePrototype)
{
    JSGlobalObject* globalObject = structure->globalObject();
    FunctionExecutable* executable = promiseConstructorPromiseConstructorCodeGenerator(vm);
    JSPromiseConstructor* constructor = new (NotNull, allocateCell<JSPromiseConstructor>(vm)) JSPromiseConstructor(vm, executable, globalObject, structure);
    constructor->finishCreation(vm, promisePrototype);
    return constructor;
}

Structure* JSPromiseConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(JSFunctionType, StructureFlags), info());
}

JSPromiseConstructor::JSPromiseConstructor(VM& vm, FunctionExecutable* executable, JSGlobalObject* globalObject, Structure* structure)
    : Base(vm, executable, globalObject, structure)
{
}

void JSPromiseConstructor::finishCreation(VM& vm, JSPromisePrototype* promisePrototype)
{
    Base::finishCreation(vm);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, promisePrototype, PropertyAttribute::DontEnum | PropertyAttribute::DontDelete | PropertyAttribute::ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(1), PropertyAttribute::DontEnum | PropertyAttribute::ReadOnly);

    JSGlobalObject* globalObject = this->globalObject();

    putDirectNonIndexAccessorWithoutTransition(vm, vm.propertyNames->speciesSymbol, globalObject->promiseSpeciesGetterSetter(), PropertyAttribute::Accessor | PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum);
    JSC_BUILTIN_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->tryKeyword, promiseConstructorTryCodeGenerator, static_cast<unsigned>(PropertyAttribute::DontEnum));
}

JSC_DEFINE_HOST_FUNCTION(promiseConstructorFuncResolve, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());
    JSValue argument = callFrame->argument(0);

    if (!thisValue.isObject()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "|this| is not an object"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(JSPromise::promiseResolve(globalObject, asObject(thisValue), argument)));
}

JSC_DEFINE_HOST_FUNCTION(promiseConstructorFuncReject, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());
    JSValue argument = callFrame->argument(0);

    if (!thisValue.isObject()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "|this| is not an object"_s);

    RELEASE_AND_RETURN(scope, JSValue::encode(JSPromise::promiseReject(globalObject, asObject(thisValue), argument)));
}

JSC_DEFINE_HOST_FUNCTION(promiseConstructorFuncWithResolvers, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    JSValue thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());
    return JSValue::encode(JSPromise::createNewPromiseCapability(globalObject, thisValue));
}

static bool isFastPromiseConstructor(JSGlobalObject* globalObject, JSValue value)
{
    if (value != globalObject->promiseConstructor()) [[unlikely]]
        return false;

    if (!globalObject->promiseResolveWatchpointSet().isStillValid()) [[unlikely]]
        return false;

    return true;
}

static JSObject* promiseRaceSlow(JSGlobalObject* globalObject, CallFrame* callFrame, JSValue thisValue)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto [promise, resolve, reject] = JSPromise::newPromiseCapability(globalObject, thisValue);
    RETURN_IF_EXCEPTION(scope, { });

    auto callReject = [&](JSValue exception) -> void {
        MarkedArgumentBuffer rejectArguments;
        rejectArguments.append(exception);
        ASSERT(!rejectArguments.hasOverflowed());
        auto rejectCallData = getCallDataInline(reject);
        scope.release();
        call(globalObject, reject, rejectCallData, jsUndefined(), rejectArguments);
    };
    auto callRejectWithScopeException = [&]() -> void {
        Exception* exception = scope.exception();
        ASSERT(exception);
        scope.clearException();
        callReject(exception->value());
    };

    JSValue promiseResolveValue = thisValue.get(globalObject, vm.propertyNames->resolve);
    if (scope.exception()) [[unlikely]] {
        callRejectWithScopeException();
        return promise;
    }

    if (!promiseResolveValue.isCallable()) [[unlikely]] {
        callReject(createTypeError(globalObject, "Promise resolve is not a function"_s));
        return promise;
    }
    CallData promiseResolveCallData = getCallDataInline(promiseResolveValue);
    ASSERT(promiseResolveCallData.type != CallData::Type::None);

    std::optional<CachedCall> cachedCallHolder;
    CachedCall* cachedCall = nullptr;
    if (promiseResolveCallData.type == CallData::Type::JS) [[likely]] {
        cachedCallHolder.emplace(globalObject, jsCast<JSFunction*>(promiseResolveValue), 1);
        if (scope.exception()) [[unlikely]] {
            callRejectWithScopeException();
            return promise;
        }
        cachedCall = &cachedCallHolder.value();
    }

    JSValue iterable = callFrame->argument(0);
    forEachInIterable(globalObject, iterable, [&](VM& vm, JSGlobalObject* globalObject, JSValue value) {
        auto scope = DECLARE_THROW_SCOPE(vm);

        JSValue nextPromise;
        if (cachedCall) [[likely]] {
            nextPromise = cachedCall->callWithArguments(globalObject, thisValue, value);
            RETURN_IF_EXCEPTION(scope, void());
        } else {
            MarkedArgumentBuffer arguments;
            arguments.append(value);
            ASSERT(!arguments.hasOverflowed());
            nextPromise = call(globalObject, promiseResolveValue, promiseResolveCallData, thisValue, arguments);
            RETURN_IF_EXCEPTION(scope, void());
        }
        ASSERT(nextPromise);

        if (auto* nextPromiseObj = jsDynamicCast<JSPromise*>(nextPromise); nextPromiseObj && nextPromiseObj->isThenFastAndNonObservable()) [[likely]] {
            scope.release();
            nextPromiseObj->performPromiseThen(vm, globalObject, resolve, reject, jsUndefined(), promise);
        } else {
            JSValue then = nextPromise.get(globalObject, vm.propertyNames->then);
            RETURN_IF_EXCEPTION(scope, void());
            CallData thenCallData = getCallDataInline(then);
            MarkedArgumentBuffer thenArguments;
            thenArguments.append(resolve);
            thenArguments.append(reject);
            ASSERT(!thenArguments.hasOverflowed());
            scope.release();
            call(globalObject, then, thenCallData, nextPromise, thenArguments);
        }
    });

    if (scope.exception()) [[unlikely]]
        callRejectWithScopeException();

    return promise;
}
JSC_DEFINE_HOST_FUNCTION(promiseConstructorFuncRace, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSValue thisValue = callFrame->thisValue().toThis(globalObject, ECMAMode::strict());

    if (!thisValue.isObject()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "|this| is not an object"_s);

    if (!isFastPromiseConstructor(globalObject, thisValue)) [[unlikely]]
        RELEASE_AND_RETURN(scope, JSValue::encode(promiseRaceSlow(globalObject, callFrame, thisValue)));

    auto* promise = JSPromise::create(vm, globalObject->promiseStructure());
    auto [resolve, reject] = promise->createFirstResolvingFunctions(vm, globalObject);

    auto callReject = [&]() -> void {
        Exception* exception = scope.exception();
        ASSERT(exception);
        scope.clearException();
        scope.release();
        promise->reject(vm, globalObject, exception);
    };

    JSValue iterable = callFrame->argument(0);
    forEachInIterable(globalObject, iterable, [&](VM& vm, JSGlobalObject* globalObject, JSValue value) {
        auto scope = DECLARE_THROW_SCOPE(vm);

        JSPromise* nextPromise = JSPromise::resolvedPromise(globalObject, value);
        RETURN_IF_EXCEPTION(scope, void());

        if (nextPromise->isThenFastAndNonObservable()) [[likely]] {
            scope.release();
            nextPromise->performPromiseThen(vm, globalObject, resolve, reject, jsUndefined(), promise);
        } else {
            JSValue then = nextPromise->get(globalObject, vm.propertyNames->then);
            RETURN_IF_EXCEPTION(scope, void());
            CallData thenCallData = getCallDataInline(then);
            MarkedArgumentBuffer thenArguments;
            thenArguments.append(resolve);
            thenArguments.append(reject);
            ASSERT(!thenArguments.hasOverflowed());
            scope.release();
            call(globalObject, then, thenCallData, nextPromise, thenArguments);
        }
    });

    if (scope.exception()) [[unlikely]]
        callReject();

    return JSValue::encode(promise);
}

} // namespace JSC
