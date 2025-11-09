/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "JSWebExtensionWrapper.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "JSWebExtensionWrappable.h"
#include "WebFrame.h"
#include "WebPage.h"
#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSWeakObjectMapRefPrivate.h>

namespace WebKit {

static HashMap<JSGlobalContextRef, JSWeakObjectMapRef>& wrapperCache()
{
    static NeverDestroyed<HashMap<JSGlobalContextRef, JSWeakObjectMapRef>> wrappers;
    return wrappers;
}

static void cacheMapDestroyed(JSWeakObjectMapRef map, void* context)
{
    wrapperCache().remove(static_cast<JSGlobalContextRef>(context));
}

static inline JSWeakObjectMapRef wrapperCacheMap(JSContextRef context)
{
    auto globalContext = JSContextGetGlobalContext(context);
    return wrapperCache().ensure(globalContext, [&] {
        return JSWeakObjectMapCreate(globalContext, globalContext, cacheMapDestroyed);
    }).iterator->value;
}

static inline JSValueRef getCachedWrapper(JSContextRef context, JSWeakObjectMapRef wrappers, JSWebExtensionWrappable* object)
{
    ASSERT(context);
    ASSERT(wrappers);
    ASSERT(object);

    if (auto wrapper = JSWeakObjectMapGet(context, wrappers, object)) {
        // Check if the wrapper is still valid. Objects invalidated through finalize
        // will not get removed from the map automatically.
        if (JSObjectGetPrivate(wrapper))
            return wrapper;

        // Remove from the map, since the object is invalid.
        JSWeakObjectMapRemove(context, wrappers, object);
    }

    return nullptr;
}

JSValueRef JSWebExtensionWrapper::wrap(JSContextRef context, JSWebExtensionWrappable* object)
{
    ASSERT(context);

    if (!object)
        return JSValueMakeNull(context);

    // JSWeakObjectMapRef is an opaque type that isn't refcounted. Static analysis can see internal refcounting APIs via internal headers, but clients aren't supposed to use those internal APIs.
    SUPPRESS_UNCOUNTED_LOCAL auto wrappers = wrapperCacheMap(context);
    if (auto result = getCachedWrapper(context, wrappers, object))
        return result;

    RefPtr objectClass = object->wrapperClass();
    ASSERT(objectClass);

    auto wrapper = JSObjectMake(context, objectClass.get(), object);
    ASSERT(wrapper);

    JSWeakObjectMapSet(context, wrappers, object, wrapper);

    return wrapper;
}

JSWebExtensionWrappable* JSWebExtensionWrapper::unwrap(JSContextRef context, JSValueRef value)
{
    ASSERT(context);
    ASSERT(value);

    if (!context || !value)
        return nullptr;

    return static_cast<JSWebExtensionWrappable*>(JSObjectGetPrivate(JSValueToObject(context, value, nullptr)));
}

static JSWebExtensionWrappable* unwrapObject(JSObjectRef object)
{
    ASSERT(object);

    ASSERT(JSObjectGetPrivate(object));
    return static_cast<JSWebExtensionWrappable*>(JSObjectGetPrivate(object));
}

void JSWebExtensionWrapper::initialize(JSContextRef, JSObjectRef object)
{
    if (RefPtr wrappable = unwrapObject(object))
        wrappable->ref();
}

void JSWebExtensionWrapper::finalize(JSObjectRef object)
{
    if (RefPtr wrappable = unwrapObject(object)) {
        JSObjectSetPrivate(object, nullptr);
        wrappable->deref();
    }
}

RefPtr<WebFrame> toWebFrame(JSContextRef context)
{
    ASSERT(context);
    return WebFrame::frameForContext(JSContextGetGlobalContext(context));
}

RefPtr<WebPage> toWebPage(JSContextRef context)
{
    ASSERT(context);
    auto frame = toWebFrame(context);
    return frame ? frame->page() : nullptr;
}

String toJSONString(JSContextRef context, JSValueRef value)
{
    if (!context)
        return nullString();

    return serializeJSObject(context, value, nullptr);
}

bool isFunction(JSContextRef context, JSValueRef value)
{
    if (!context || !value || !JSValueIsObject(context, value))
        return false;

    JSObjectRef functionRef = JSValueToObject(context, value, nullptr);
    return functionRef && JSObjectIsFunction(context, functionRef);
}

bool isDictionary(JSContextRef context, JSValueRef value)
{
    // Equivalent to JavaScript: this.__proto__ === Object.prototype
    if (!context || !JSValueIsObject(context, value))
        return false;

    if (isThenable(context, value))
        return false;

    JSRetainPtr protoString = toJSString("__proto__");
    JSRetainPtr objectString = toJSString("Object");
    JSRetainPtr prototypeString = toJSString("prototype");

    JSObjectRef thisObject = JSValueToObject(context, value, nullptr);
    JSObjectRef globalObject = JSContextGetGlobalObject(context);

    // This is a safer cpp false positive (rdar://163760990).
    SUPPRESS_UNCOUNTED_ARG JSValueRef protoObject = JSObjectGetProperty(context, thisObject, protoString.get(), nullptr);
    SUPPRESS_UNCOUNTED_ARG JSObjectRef contextObject = JSValueToObject(context, JSObjectGetProperty(context, globalObject, objectString.get(), nullptr), nullptr);
    SUPPRESS_UNCOUNTED_ARG JSValueRef prototypeObject = JSObjectGetProperty(context, contextObject, prototypeString.get(), nullptr);

    return JSValueIsStrictEqual(context, protoObject, prototypeObject);
}

bool isRegularExpression(JSContextRef context, JSValueRef value)
{
    if (!context || !JSValueIsObject(context, value))
        return false;

    JSRetainPtr regexpString = toJSString("RegExp");
    JSObjectRef globalObject = JSContextGetGlobalObject(context);
    // This is a safer cpp false positive (rdar://163760990).
    SUPPRESS_UNCOUNTED_ARG JSObjectRef regexpValue = JSValueToObject(context, JSObjectGetProperty(context, globalObject, regexpString.get(), nullptr), nullptr);

    return JSValueIsInstanceOfConstructor(context, value, regexpValue, nullptr);
}

bool isThenable(JSContextRef context, JSValueRef value)
{
    if (!context || !JSValueIsObject(context, value))
        return false;

    JSRetainPtr thenableString = toJSString("then");
    JSObjectRef valueObject = JSValueToObject(context, value, nullptr);
    // This is a safer cpp false positive (rdar://163760990).
    SUPPRESS_UNCOUNTED_ARG JSValueRef thenableObject = JSObjectGetProperty(context, valueObject, thenableString.get(), nullptr);

    return isFunction(context, thenableObject);
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
