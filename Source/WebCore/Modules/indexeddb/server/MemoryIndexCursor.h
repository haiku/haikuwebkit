/*
 * Copyright (C) 2015-2025 Apple Inc. All rights reserved.
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

#include "IDBCursorInfo.h"
#include "IndexValueStore.h"
#include "MemoryCursor.h"
#include <wtf/CheckedRef.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {
namespace IDBServer {

class MemoryIndex;

class MemoryIndexCursor : public MemoryCursor {
    WTF_MAKE_TZONE_ALLOCATED(MemoryIndexCursor);
public:
    static Ref<MemoryIndexCursor> create(MemoryIndex&, const IDBCursorInfo&, MemoryBackingStoreTransaction&);

    virtual ~MemoryIndexCursor();

    void indexRecordsAllChanged();
    void indexValueChanged(const IDBKeyData& indexKey, const IDBKeyData& primaryKey);

private:
    MemoryIndexCursor(MemoryIndex&, const IDBCursorInfo&, MemoryBackingStoreTransaction&);
    void currentData(IDBGetResult&) final;
    void iterate(const IDBKeyData&, const IDBKeyData& primaryKey, uint32_t count, IDBGetResult&) final;

    const CheckedRef<MemoryIndex> m_index;

    IndexValueStore::Iterator m_currentIterator;
    IDBKeyData m_currentKey;
    IDBKeyData m_currentPrimaryKey;
};

} // namespace IDBServer
} // namespace WebCore
