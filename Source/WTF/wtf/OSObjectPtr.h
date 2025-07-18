/*
 * Copyright (C) 2014-2021 Apple Inc. All rights reserved.
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

#include <os/object.h>
#include <wtf/StdLibExtras.h>

// Because ARC enablement is a compile-time choice, and we compile this header
// both ways, we need a separate copy of our code when ARC is enabled.
#if __has_feature(objc_arc)
#define adoptOSObject adoptOSObjectArc
#define retainOSObject retainOSObjectArc
#define releaseOSObject releaseOSObjectArc
#define OSObjectPtr OSObjectPtrArc
#endif

namespace WTF {

template<typename> class OSObjectPtr;
template<typename T> OSObjectPtr<T> adoptOSObject(T) WARN_UNUSED_RETURN;

template<typename T> static inline void retainOSObject(T ptr)
{
#if __has_feature(objc_arc)
    UNUSED_PARAM(ptr);
#else
    os_retain(ptr);
#endif
}

template<typename T> static inline void releaseOSObject(T ptr)
{
#if __has_feature(objc_arc)
    UNUSED_PARAM(ptr);
#else
    os_release(ptr);
#endif
}

template<typename T> class OSObjectPtr {
public:
    OSObjectPtr()
        : m_ptr(nullptr)
    {
    }

    ~OSObjectPtr()
    {
        if (m_ptr)
            releaseOSObject(m_ptr);
    }

    T get() const { return m_ptr; }

    explicit operator bool() const { return m_ptr; }
    bool operator!() const { return !m_ptr; }

    OSObjectPtr(const OSObjectPtr& other)
        : m_ptr(other.m_ptr)
    {
        if (m_ptr)
            retainOSObject(m_ptr);
    }

    OSObjectPtr(OSObjectPtr&& other)
        : m_ptr(WTFMove(other.m_ptr))
    {
        other.m_ptr = nullptr;
    }

    OSObjectPtr(T ptr)
        : m_ptr(WTFMove(ptr))
    {
        if (m_ptr)
            retainOSObject(m_ptr);
    }

    OSObjectPtr& operator=(const OSObjectPtr& other)
    {
        OSObjectPtr ptr = other;
        swap(ptr);
        return *this;
    }

    OSObjectPtr& operator=(OSObjectPtr&& other)
    {
        OSObjectPtr ptr = WTFMove(other);
        swap(ptr);
        return *this;
    }

    OSObjectPtr& operator=(std::nullptr_t)
    {
        if (m_ptr)
            releaseOSObject(m_ptr);
        m_ptr = nullptr;
        return *this;
    }

    OSObjectPtr& operator=(T other)
    {
        OSObjectPtr ptr = WTFMove(other);
        swap(ptr);
        return *this;
    }

    void swap(OSObjectPtr& other)
    {
        std::swap(m_ptr, other.m_ptr);
    }

    T leakRef() WARN_UNUSED_RETURN
    {
        return std::exchange(m_ptr, nullptr);
    }

    friend OSObjectPtr adoptOSObject<T>(T) WARN_UNUSED_RETURN;

private:
    struct AdoptOSObject { };
    OSObjectPtr(AdoptOSObject, T ptr)
        : m_ptr(WTFMove(ptr))
    {
    }

    T m_ptr;
};

template<typename T> inline OSObjectPtr<T> adoptOSObject(T ptr)
{
    return OSObjectPtr<T> { typename OSObjectPtr<T>::AdoptOSObject { }, WTFMove(ptr) };
}

template<typename T, typename U>
ALWAYS_INLINE void lazyInitialize(const OSObjectPtr<T>& ptr, OSObjectPtr<U>&& obj)
{
    RELEASE_ASSERT(!ptr);
    const_cast<OSObjectPtr<T>&>(ptr) = std::move(obj);
}

} // namespace WTF

using WTF::OSObjectPtr;
using WTF::adoptOSObject;
using WTF::lazyInitialize;
