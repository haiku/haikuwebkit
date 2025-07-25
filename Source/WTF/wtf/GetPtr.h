/*
 *  Copyright (C) 2006-2019 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#pragma once

#include <memory>

namespace WTF {

enum HashTableDeletedValueType { HashTableDeletedValue };
enum HashTableEmptyValueType { HashTableEmptyValue };

template <typename T> inline T* getPtr(T* p) { return p; }

template <typename T> struct IsSmartPtr {
    static constexpr bool value = false;
    static constexpr bool isNullable = true;
};

template<typename T>
inline constexpr bool IsSmartPtrV = IsSmartPtr<T>::value;

template<typename T>
inline constexpr bool IsSmartPtrNullableV = IsSmartPtr<T>::isNullable;

template <typename T, bool isSmartPtr>
struct GetPtrHelperBase;

template <typename T>
struct GetPtrHelperBase<T, false /* isSmartPtr */> {
    using PtrType = T*;
    using UnderlyingType = T;
    static T* getPtr(T& p) { return std::addressof(p); }
};

template <typename T>
struct GetPtrHelperBase<T, true /* isSmartPtr */> {
    using PtrType = typename T::PtrType;
    using UnderlyingType = typename T::ValueType;
    static PtrType getPtr(const T& p) { return p.get(); }
};

template <typename T>
struct GetPtrHelper : GetPtrHelperBase<T, IsSmartPtr<T>::value> {
};

template <typename T>
inline typename GetPtrHelper<T>::PtrType getPtr(T& p)
{
    return GetPtrHelper<T>::getPtr(p);
}

template <typename T>
inline typename GetPtrHelper<T>::PtrType getPtr(const T& p)
{
    return GetPtrHelper<T>::getPtr(p);
}

// Explicit specialization for C++ standard library types.

template <typename T, typename Deleter> struct IsSmartPtr<std::unique_ptr<T, Deleter>> {
    static constexpr bool value = true;
    static constexpr bool isNullable = true;
};

template <typename T, typename Deleter>
struct GetPtrHelper<std::unique_ptr<T, Deleter>> {
    using PtrType = T*;
    using UnderlyingType = T;
    static T* getPtr(const std::unique_ptr<T, Deleter>& p) { return p.get(); }
};

} // namespace WTF
