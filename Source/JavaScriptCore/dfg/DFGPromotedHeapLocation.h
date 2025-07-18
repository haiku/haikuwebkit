/*
 * Copyright (C) 2014-2019 Apple Inc. All rights reserved.
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

#if ENABLE(DFG_JIT)

#include "DFGEdge.h"
#include "DFGNodeOrigin.h"
#include <wtf/HashTable.h>
#include <wtf/PrintStream.h>

namespace JSC { namespace DFG {

struct Node;

// Promoted locations are like heap locations but are meant to be more precise. A heap location is
// applicable to CSE scenarios, where it makes sense to speak of a location very abstractly. A
// promoted heap location is for cases where we speak of a specific object and the compiler knows
// this object's identity - for example, the object allocation has been eliminated and we turned the
// fields into local variables. Because these two cases have subtly different needs, we use subtly
// different structures. One of the really significant differences is that promoted locations can be
// spoken of using either a descriptor which does not refer to any Node*'s or with a heap location,
// which is a descriptor with a Node* base.

enum PromotedLocationKind {
    InvalidPromotedLocationKind,

    ActivationScopePLoc,
    ActivationSymbolTablePLoc,
    ArgumentCountPLoc,
    ArgumentPLoc,
    ArgumentsCalleePLoc,
    ArrayPLoc,
    ArrayLengthPropertyPLoc,
    ArrayButterflyPropertyPLoc,
    ArrayIndexedPropertyPLoc,
    ClosureVarPLoc,
    InternalFieldObjectPLoc,
    FunctionActivationPLoc,
    FunctionExecutablePLoc,
    IndexedPropertyPLoc,
    NamedPropertyPLoc,
    PublicLengthPLoc,
    StructurePLoc,
    VectorLengthPLoc,
    SpreadPLoc,
    NewArrayWithSpreadArgumentPLoc,
    NewArrayBufferPLoc,
    RegExpObjectRegExpPLoc,
    RegExpObjectLastIndexPLoc,
};

class PromotedLocationDescriptor {
public:
    PromotedLocationDescriptor(
        PromotedLocationKind kind = InvalidPromotedLocationKind, unsigned info = 0)
        : m_kind(kind)
        , m_info(info)
    {
    }

    PromotedLocationDescriptor(WTF::HashTableDeletedValueType)
        : m_kind(InvalidPromotedLocationKind)
        , m_info(1)
    {
    }

    bool operator!() const { return m_kind == InvalidPromotedLocationKind; }

    explicit operator bool() const { return !!*this; }
    
    PromotedLocationKind kind() const { return m_kind; }
    unsigned info() const { return m_info; }
    
    unsigned imm1() const { return static_cast<uint32_t>(m_kind); }
    unsigned imm2() const { return static_cast<uint32_t>(m_info); }
    
    unsigned hash() const
    {
        return m_kind + m_info;
    }
    
    friend bool operator==(const PromotedLocationDescriptor&, const PromotedLocationDescriptor&) = default;
    
    bool isHashTableDeletedValue() const
    {
        return m_kind == InvalidPromotedLocationKind && m_info;
    }

    bool neededForMaterialization() const
    {
        switch (kind()) {
        case NamedPropertyPLoc:
        case ClosureVarPLoc:
        case RegExpObjectLastIndexPLoc:
        case InternalFieldObjectPLoc:
            return false;

        default:
            return true;
        }
    }
    
    void dump(PrintStream& out) const;

private:
    PromotedLocationKind m_kind;
    unsigned m_info;
};

struct PromotedLocationDescriptorHash {
    static unsigned hash(const PromotedLocationDescriptor& key) { return key.hash(); }
    static bool equal(const PromotedLocationDescriptor& a, const PromotedLocationDescriptor& b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};

class PromotedHeapLocation {
public:
    PromotedHeapLocation(
        PromotedLocationKind kind = InvalidPromotedLocationKind,
        Node* base = nullptr, unsigned info = 0)
        : m_base(base)
        , m_meta(kind, info)
    {
    }
    
    PromotedHeapLocation(
        PromotedLocationKind kind, Edge base, unsigned info = 0)
        : PromotedHeapLocation(kind, base.node(), info)
    {
    }
    
    PromotedHeapLocation(Node* base, PromotedLocationDescriptor meta)
        : m_base(base)
        , m_meta(meta)
    {
    }
    
    PromotedHeapLocation(WTF::HashTableDeletedValueType)
        : m_base(nullptr)
        , m_meta(InvalidPromotedLocationKind, 1)
    {
    }
    
    Node* createHint(Graph&, NodeOrigin, Node* value);
    
    bool operator!() const { return kind() == InvalidPromotedLocationKind; }
    
    PromotedLocationKind kind() const { return m_meta.kind(); }
    Node* base() const { return m_base; }
    unsigned info() const { return m_meta.info(); }
    PromotedLocationDescriptor descriptor() const { return m_meta; }
    
    unsigned hash() const
    {
        return m_meta.hash() + WTF::PtrHash<Node*>::hash(m_base);
    }
    
    friend bool operator==(const PromotedHeapLocation&, const PromotedHeapLocation&) = default;
    
    bool isHashTableDeletedValue() const
    {
        return m_meta.isHashTableDeletedValue();
    }
    
    void dump(PrintStream& out) const;
    
private:
    Node* m_base;
    PromotedLocationDescriptor m_meta;
};

struct PromotedHeapLocationHash {
    static unsigned hash(const PromotedHeapLocation& key) { return key.hash(); }
    static bool equal(const PromotedHeapLocation& a, const PromotedHeapLocation& b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};

} } // namespace JSC::DFG

namespace WTF {

template<typename T> struct DefaultHash;
template<> struct DefaultHash<JSC::DFG::PromotedHeapLocation> : JSC::DFG::PromotedHeapLocationHash { };

template<typename T> struct HashTraits;
template<> struct HashTraits<JSC::DFG::PromotedHeapLocation> : SimpleClassHashTraits<JSC::DFG::PromotedHeapLocation> {
    static constexpr bool emptyValueIsZero = false;
};

template<typename T> struct DefaultHash;
template<> struct DefaultHash<JSC::DFG::PromotedLocationDescriptor> : JSC::DFG::PromotedLocationDescriptorHash { };

template<typename T> struct HashTraits;
template<> struct HashTraits<JSC::DFG::PromotedLocationDescriptor> : SimpleClassHashTraits<JSC::DFG::PromotedLocationDescriptor> {
    static constexpr bool emptyValueIsZero = false;
};

} // namespace WTF

#endif // ENABLE(DFG_JIT)
