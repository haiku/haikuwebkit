/*
 * Copyright (c) 2005, 2007 Google Inc. All rights reserved.
 * Copyright (C) 2005-2018 Apple Inc. All rights reserved.
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
#include <wtf/FastMalloc.h>

#include <string.h>
#include <wtf/CheckedArithmetic.h>

#if OS(DARWIN)
#include <malloc/malloc.h>
#endif

#if OS(WINDOWS)
#include <windows.h>
#include <psapi.h>
#else
#if HAVE(RESOURCE_H)
#include <sys/resource.h>
#endif // HAVE(RESOURCE_H)
#endif

#if OS(HAIKU)
#include <OS.h>
#endif

#if ENABLE(MALLOC_HEAP_BREAKDOWN)
#include <wtf/Atomics.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/SetForScope.h>
#include <wtf/StackShot.h>

#if PLATFORM(COCOA)
#include <notify.h>
#endif

#endif

namespace WTF {

#if !defined(NDEBUG)
namespace {
// We do not use std::numeric_limits<size_t>::max() here due to the edge case in VC++.
// https://bugs.webkit.org/show_bug.cgi?id=173720
static size_t maxSingleAllocationSize = SIZE_MAX;
};

void fastSetMaxSingleAllocationSize(size_t size)
{
    maxSingleAllocationSize = size;
}

#if ASSERT_ENABLED
#define ASSERT_IS_WITHIN_LIMIT(size) do { \
        size_t size__ = (size); \
        ASSERT_WITH_MESSAGE((size__) <= maxSingleAllocationSize, "Requested size (%zu) exceeds max single allocation size set for testing (%zu)", (size__), maxSingleAllocationSize); \
    } while (false)
#else
#define ASSERT_IS_WITHIN_LIMIT(size)
#endif // ASSERT_ENABLED

#define FAIL_IF_EXCEEDS_LIMIT(size) do { \
        if ((size) > maxSingleAllocationSize) [[unlikely]] \
            return nullptr; \
    } while (false)

#else // !defined(NDEBUG)

#define ASSERT_IS_WITHIN_LIMIT(size)
#define FAIL_IF_EXCEEDS_LIMIT(size)

#endif // !defined(NDEBUG)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

char* fastStrDup(const char* src)
{
    size_t len = strlen(src) + 1;
    char* dup = static_cast<char*>(fastMalloc(len));
    memcpy(dup, src, len);
    return dup;
}

void* fastMemDup(const void* mem, size_t bytes)
{
    if (!mem || !bytes)
        return nullptr;

    void* result = fastMalloc(bytes);
    memcpy(result, mem, bytes);
    return result;
}

char* fastCompactStrDup(const char* src)
{
    size_t len = strlen(src) + 1;
    char* dup = static_cast<char*>(fastCompactMalloc(len));
    memcpy(dup, src, len);
    return dup;
}

void* fastCompactMemDup(const void* mem, size_t bytes)
{
    if (!mem || !bytes)
        return nullptr;

    void* result = fastCompactMalloc(bytes);
    memcpy(result, mem, bytes);
    return result;
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

} // namespace WTF

#if USE(SYSTEM_MALLOC)

#include <wtf/OSAllocator.h>

#if OS(WINDOWS)
#include <malloc.h>
#endif

namespace WTF {

bool isFastMallocEnabled()
{
    return false;
}

size_t fastMallocGoodSize(size_t bytes)
{
#if OS(DARWIN)
    return malloc_good_size(bytes);
#else
    return bytes;
#endif
}

#if OS(WINDOWS)

void* fastAlignedMalloc(size_t alignment, size_t size) 
{
    ASSERT_IS_WITHIN_LIMIT(size);
    void* p = _aligned_malloc(size, alignment);
    if (!p) [[unlikely]]
        CRASH();
    return p;
}

void* tryFastAlignedMalloc(size_t alignment, size_t size) 
{
    FAIL_IF_EXCEEDS_LIMIT(size);
    return _aligned_malloc(size, alignment);
}

void fastAlignedFree(void* p) 
{
    _aligned_free(p);
}

#else

void* fastAlignedMalloc(size_t alignment, size_t size) 
{
    ASSERT_IS_WITHIN_LIMIT(size);
    void* p = aligned_alloc(alignment, size);
    if (!p) [[unlikely]]
        CRASH();
    return p;
}

void* tryFastAlignedMalloc(size_t alignment, size_t size) 
{
    FAIL_IF_EXCEEDS_LIMIT(size);
    return aligned_alloc(alignment, size);
}

void fastAlignedFree(void* p) 
{
    free(p);
}

#endif // OS(WINDOWS)

TryMallocReturnValue tryFastMalloc(size_t n) 
{
    FAIL_IF_EXCEEDS_LIMIT(n);
    assertMallocRestrictionForCurrentThreadScope();
    return malloc(n);
}

void* fastMalloc(size_t n) 
{
    ASSERT_IS_WITHIN_LIMIT(n);
    assertMallocRestrictionForCurrentThreadScope();
    void* result = malloc(n);
    if (!result)
        CRASH();

    return result;
}

void* fastZeroedMalloc(size_t n)
{
    void* result = fastMalloc(n);
    memset(result, 0, n);
    return result;
}

TryMallocReturnValue tryFastZeroedMalloc(size_t n)
{
    void* result;
    if (!tryFastMalloc(n).getValue(result))
        return nullptr;
    memset(result, 0, n);
    return result;
}

TryMallocReturnValue tryFastCalloc(size_t n_elements, size_t element_size)
{
    FAIL_IF_EXCEEDS_LIMIT(n_elements * element_size);
    assertMallocRestrictionForCurrentThreadScope();
    return calloc(n_elements, element_size);
}

void* fastCalloc(size_t n_elements, size_t element_size)
{
    ASSERT_IS_WITHIN_LIMIT(n_elements * element_size);
    assertMallocRestrictionForCurrentThreadScope();
    void* result = calloc(n_elements, element_size);
    if (!result)
        CRASH();

    return result;
}

void fastFree(void* p)
{
    free(p);
}

void* fastRealloc(void* p, size_t n)
{
    ASSERT_IS_WITHIN_LIMIT(n);
    assertMallocRestrictionForCurrentThreadScope();
    void* result = realloc(p, n);
    if (!result)
        CRASH();
    return result;
}

TryMallocReturnValue tryFastRealloc(void* p, size_t n)
{
    FAIL_IF_EXCEEDS_LIMIT(n);
    assertMallocRestrictionForCurrentThreadScope();
    return realloc(p, n);
}

void releaseFastMallocFreeMemory() { }
void releaseFastMallocFreeMemoryForThisThread() { }

FastMallocStatistics fastMallocStatistics()
{
    FastMallocStatistics statistics = { 0, 0, 0 };
    return statistics;
}

size_t fastMallocSize(const void* p)
{
#if OS(DARWIN)
    return malloc_size(p);
#elif OS(WINDOWS)
    return _msize(const_cast<void*>(p));
#else
    UNUSED_PARAM(p);
    return 1;
#endif
}

void fastCommitAlignedMemory(void* ptr, size_t size)
{
    OSAllocator::commit(ptr, size, true, false);
}

void fastDecommitAlignedMemory(void* ptr, size_t size)
{
    OSAllocator::decommit(ptr, size);
}

void fastEnableMiniMode(bool) { }

void fastDisableScavenger() { }

void fastMallocDumpMallocStats() { }

void* fastCompactMalloc(size_t size) { return fastMalloc(size); }
void* fastCompactZeroedMalloc(size_t size) { return fastZeroedMalloc(size); }
void* fastCompactCalloc(size_t numElements, size_t elementSize) { return fastCalloc(numElements, elementSize); }
void* fastCompactRealloc(void* ptr, size_t size) { return fastRealloc(ptr, size); }
TryMallocReturnValue tryFastCompactMalloc(size_t size) { return tryFastMalloc(size); }
TryMallocReturnValue tryFastCompactZeroedMalloc(size_t size) { return tryFastZeroedMalloc(size); }
TryMallocReturnValue tryFastCompactCalloc(size_t numElements, size_t elementSize) { return tryFastCalloc(numElements, elementSize); }
TryMallocReturnValue tryFastCompactRealloc(void* ptr, size_t size) { return tryFastRealloc(ptr, size); }
void* fastCompactAlignedMalloc(size_t alignment, size_t size) { return fastAlignedMalloc(alignment, size); }
void* tryFastCompactAlignedMalloc(size_t alignment, size_t size) { return tryFastAlignedMalloc(alignment, size); }

} // namespace WTF

#else // USE(SYSTEM_MALLOC)

#include <bmalloc/bmalloc.h>

namespace WTF {

#define TRACK_MALLOC_CALLSTACK 0

#if ENABLE(MALLOC_HEAP_BREAKDOWN) && TRACK_MALLOC_CALLSTACK

static ThreadSpecificKey avoidRecordingCountKey { InvalidThreadSpecificKey };
class AvoidRecordingScope {
public:
    AvoidRecordingScope();
    ~AvoidRecordingScope();

    static uintptr_t avoidRecordingCount()
    {
        return std::bit_cast<uintptr_t>(threadSpecificGet(avoidRecordingCountKey));
    }
};

AvoidRecordingScope::AvoidRecordingScope()
{
    static std::once_flag onceKey;
    std::call_once(onceKey, [] {
        // The value stored in TLS is initially 0.
        threadSpecificKeyCreate(&avoidRecordingCountKey, [](void*) { });
    });
    threadSpecificSet(avoidRecordingCountKey, std::bit_cast<void*>(avoidRecordingCount() + 1));
}

AvoidRecordingScope::~AvoidRecordingScope()
{
    threadSpecificSet(avoidRecordingCountKey, std::bit_cast<void*>(avoidRecordingCount() - 1));
}

class MallocCallTracker {
public:
    MallocCallTracker();

    void recordMalloc(void*, size_t);
    void recordRealloc(void* oldAddress, void* newAddress, size_t);
    void recordFree(void*);

    void dumpStats();

    static MallocCallTracker& singleton();

private:
    struct MallocSiteData {
        StackShot stack;
        size_t size;

        MallocSiteData(size_t stackSize, size_t allocationSize)
            : stack(stackSize)
            , size(allocationSize)
        {
        }
    };

    Lock m_lock;
    HashMap<void*, std::unique_ptr<MallocSiteData>> m_addressMallocSiteData WTF_GUARDED_BY_LOCK(m_lock);
};

MallocCallTracker& MallocCallTracker::singleton()
{
    AvoidRecordingScope avoidRecording;
    static LazyNeverDestroyed<MallocCallTracker> tracker;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        tracker.construct();
    });
    return tracker;
}


MallocCallTracker::MallocCallTracker()
{
    int token;
    notify_register_dispatch("com.apple.WebKit.dumpUntrackedMallocs", &token, dispatch_get_main_queue(), ^(int) {
        MallocCallTracker::singleton().dumpStats();
    });
}

void MallocCallTracker::recordMalloc(void* address, size_t allocationSize)
{
    AvoidRecordingScope avoidRecording;

    // Intentionally using std::make_unique not to use FastMalloc for data structure tracking FastMalloc.
    const size_t stackSize = 10;
    auto siteData = std::make_unique<MallocSiteData>(stackSize, allocationSize);

    Locker locker { m_lock };
    auto addResult = m_addressMallocSiteData.add(address, WTFMove(siteData));
    UNUSED_PARAM(addResult);
}

void MallocCallTracker::recordRealloc(void* oldAddress, void* newAddress, size_t newSize)
{
    AvoidRecordingScope avoidRecording;

    Locker locker { m_lock };

    auto it = m_addressMallocSiteData.find(oldAddress);
    if (it == m_addressMallocSiteData.end()) {
        ASSERT_NOT_REACHED();
        return;
    }

    it->value->size = newSize;
    if (oldAddress != newAddress) {
        auto value = WTFMove(it->value);
        m_addressMallocSiteData.remove(it);
        auto addResult = m_addressMallocSiteData.add(newAddress, WTFMove(value));
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
    }
}

void MallocCallTracker::recordFree(void* address)
{
    AvoidRecordingScope avoidRecording;

    Locker locker { m_lock };
    bool removed = m_addressMallocSiteData.remove(address);
    UNUSED_PARAM(removed);
}

void MallocCallTracker::dumpStats()
{
    AvoidRecordingScope avoidRecording;

    {
        Locker locker { m_lock };

        // Build a hash of stack to address vector
        struct MallocSiteTotals {
            Vector<MallocSiteData*> siteData;
            size_t count { 0 };
            size_t totalSize { 0 };
        };

        size_t totalUntrackedSize = 0;
        size_t totalUntrackedCount = 0;

        HashMap<unsigned, std::unique_ptr<MallocSiteTotals>> callSiteToMallocData;
        for (const auto& it : m_addressMallocSiteData) {
            auto result = callSiteToMallocData.ensure(it.value->stack.hash(), [] () {
                // Intentionally using std::make_unique not to use FastMalloc for data structure tracking FastMalloc.
                return std::make_unique<MallocSiteTotals>();
            });
            auto& siteTotal = result.iterator->value;
            siteTotal->siteData.append(it.value.get());
            ++siteTotal->count;
            siteTotal->totalSize += it.value->size;
            totalUntrackedSize += it.value->size;
            ++totalUntrackedCount;
        }

        Vector<unsigned> stackHashes;
        auto stackKeys = callSiteToMallocData.keys();
        for (auto key : stackKeys)
            stackHashes.append(key);

        // Sort by reverse total size.
        std::ranges::sort(stackHashes, [&] (unsigned a, unsigned b) {
            const auto& aSiteTotals = callSiteToMallocData.get(a);
            const auto& bSiteTotals = callSiteToMallocData.get(b);

            return aSiteTotals->totalSize > bSiteTotals->totalSize;
        });

        WTFLogAlways("Total untracked bytes: %lu (%lu allocations)\n", totalUntrackedSize, totalUntrackedCount);

        const size_t numStacksToDump = 100;
        for (size_t i = 0; i < std::min(numStacksToDump, stackHashes.size()); ++i) {
            const auto& mallocDataForStack = callSiteToMallocData.get(stackHashes[i]);

            WTFLogAlways("Total allocation size: %lu (%lu allocations)\n", mallocDataForStack->totalSize, mallocDataForStack->count);
            // FIXME: Add a way to remove some entries in StackShot in a programable way.
            // https://bugs.webkit.org/show_bug.cgi?id=205701
            const size_t framesToSkip = 6;
            WTFPrintBacktrace(mallocDataForStack->siteData[0]->stack.span().subspan(framesToSkip));
            WTFLogAlways("\n");
        }
    }
}
void fastMallocDumpMallocStats()
{
    MallocCallTracker::singleton().dumpStats();
}
#else
void fastMallocDumpMallocStats()
{
}
#endif


bool isFastMallocEnabled()
{
    return bmalloc::api::isEnabled();
}

void* fastMalloc(size_t size)
{
    ASSERT_IS_WITHIN_LIMIT(size);
    assertMallocRestrictionForCurrentThreadScope();
    void* result = bmalloc::api::malloc(size, bmalloc::CompactAllocationMode::NonCompact);
#if ENABLE(MALLOC_HEAP_BREAKDOWN) && TRACK_MALLOC_CALLSTACK
    if (!AvoidRecordingScope::avoidRecordingCount())
        MallocCallTracker::singleton().recordMalloc(result, size);
#endif
    BPROFILE_ALLOCATION(NON_JS_CELL, result, size);
    return result;
}

void* fastZeroedMalloc(size_t size)
{
    ASSERT_IS_WITHIN_LIMIT(size);
    assertMallocRestrictionForCurrentThreadScope();
    void* result = bmalloc::api::zeroedMalloc(size, bmalloc::CompactAllocationMode::NonCompact);
#if ENABLE(MALLOC_HEAP_BREAKDOWN) && TRACK_MALLOC_CALLSTACK
    if (!AvoidRecordingScope::avoidRecordingCount())
        MallocCallTracker::singleton().recordMalloc(result, size);
#endif
    BPROFILE_ALLOCATION(NON_JS_CELL, result, size);
    return result;
}

TryMallocReturnValue tryFastZeroedMalloc(size_t size)
{
    FAIL_IF_EXCEEDS_LIMIT(size);
    assertMallocRestrictionForCurrentThreadScope();
    void* result = bmalloc::api::tryZeroedMalloc(size, bmalloc::CompactAllocationMode::NonCompact);
    BPROFILE_TRY_ALLOCATION(NON_JS_CELL, result, size);
    return result;
}

void* fastCalloc(size_t numElements, size_t elementSize)
{
    ASSERT_IS_WITHIN_LIMIT(numElements * elementSize);
    Checked<size_t> checkedSize = elementSize;
    checkedSize *= numElements;
    void* result = fastZeroedMalloc(checkedSize);
    if (!result)
        CRASH();
    BPROFILE_ALLOCATION(NON_JS_CELL, result, checkedSize);
    return result;
}

void* fastRealloc(void* object, size_t size)
{
    ASSERT_IS_WITHIN_LIMIT(size);
    assertMallocRestrictionForCurrentThreadScope();
    void* result = bmalloc::api::realloc(object, size, bmalloc::CompactAllocationMode::NonCompact);
#if ENABLE(MALLOC_HEAP_BREAKDOWN) && TRACK_MALLOC_CALLSTACK
    if (!AvoidRecordingScope::avoidRecordingCount())
        MallocCallTracker::singleton().recordRealloc(object, result, size);
#endif
    BPROFILE_ALLOCATION(NON_JS_CELL, result, size);
    return result;
}

void fastFree(void* object)
{
    bmalloc::api::free(object);
#if ENABLE(MALLOC_HEAP_BREAKDOWN) && TRACK_MALLOC_CALLSTACK
    if (!AvoidRecordingScope::avoidRecordingCount())
        MallocCallTracker::singleton().recordFree(object);
#endif
}

size_t fastMallocSize(const void* p)
{
#if BENABLE(MALLOC_SIZE)
    return bmalloc::api::mallocSize(p);
#else
    // FIXME: This is incorrect; best fix is probably to remove this function.
    // Caller currently are all using this for assertion, not to actually check
    // the size of the allocation, so maybe we can come up with something for that.
    UNUSED_PARAM(p);
    return 1;
#endif
}

size_t fastMallocGoodSize(size_t size)
{
#if BENABLE(MALLOC_GOOD_SIZE)
    return bmalloc::api::mallocGoodSize(size);
#else
    return size;
#endif
}

void* fastAlignedMalloc(size_t alignment, size_t size)
{
    ASSERT_IS_WITHIN_LIMIT(size);
    assertMallocRestrictionForCurrentThreadScope();
    void* result = bmalloc::api::memalign(alignment, size, bmalloc::CompactAllocationMode::NonCompact);
#if ENABLE(MALLOC_HEAP_BREAKDOWN) && TRACK_MALLOC_CALLSTACK
    if (!AvoidRecordingScope::avoidRecordingCount())
        MallocCallTracker::singleton().recordMalloc(result, size);
#endif
    BPROFILE_ALLOCATION(NON_JS_CELL, result, size);
    return result;
}

void* tryFastAlignedMalloc(size_t alignment, size_t size)
{
    FAIL_IF_EXCEEDS_LIMIT(size);
    assertMallocRestrictionForCurrentThreadScope();
    void* result = bmalloc::api::tryMemalign(alignment, size, bmalloc::CompactAllocationMode::NonCompact);
#if ENABLE(MALLOC_HEAP_BREAKDOWN) && TRACK_MALLOC_CALLSTACK
    if (!AvoidRecordingScope::avoidRecordingCount())
        MallocCallTracker::singleton().recordMalloc(result, size);
#endif
    BPROFILE_TRY_ALLOCATION(NON_JS_CELL, result, size);
    return result;
}

void fastAlignedFree(void* p)
{
    bmalloc::api::free(p);
}

TryMallocReturnValue tryFastMalloc(size_t size)
{
    FAIL_IF_EXCEEDS_LIMIT(size);
    assertMallocRestrictionForCurrentThreadScope();
    void* result = bmalloc::api::tryMalloc(size, bmalloc::CompactAllocationMode::NonCompact);
    BPROFILE_TRY_ALLOCATION(NON_JS_CELL, result, size);
    return result;
}

TryMallocReturnValue tryFastCalloc(size_t numElements, size_t elementSize)
{
    FAIL_IF_EXCEEDS_LIMIT(numElements * elementSize);
    CheckedSize checkedSize = elementSize;
    checkedSize *= numElements;
    if (checkedSize.hasOverflowed())
        return nullptr;
    return tryFastZeroedMalloc(checkedSize);
}

TryMallocReturnValue tryFastRealloc(void* object, size_t newSize)
{
    FAIL_IF_EXCEEDS_LIMIT(newSize);
    assertMallocRestrictionForCurrentThreadScope();
    void* result = bmalloc::api::tryRealloc(object, newSize, bmalloc::CompactAllocationMode::NonCompact);
    BPROFILE_TRY_ALLOCATION(NON_JS_CELL, result, newSize);
    return result;
}

void* fastCompactMalloc(size_t size)
{
    ASSERT_IS_WITHIN_LIMIT(size);
    assertMallocRestrictionForCurrentThreadScope();
    void* result = bmalloc::api::malloc(size, bmalloc::CompactAllocationMode::Compact);
#if ENABLE(MALLOC_HEAP_BREAKDOWN) && TRACK_MALLOC_CALLSTACK
    if (!AvoidRecordingScope::avoidRecordingCount())
        MallocCallTracker::singleton().recordMalloc(result, size);
#endif
    BPROFILE_ALLOCATION(COMPACTIBLE, result, size);
    return result;
}

void* fastCompactZeroedMalloc(size_t size)
{
    ASSERT_IS_WITHIN_LIMIT(size);
    assertMallocRestrictionForCurrentThreadScope();
    void* result = bmalloc::api::zeroedMalloc(size, bmalloc::CompactAllocationMode::Compact);
#if ENABLE(MALLOC_HEAP_BREAKDOWN) && TRACK_MALLOC_CALLSTACK
    if (!AvoidRecordingScope::avoidRecordingCount())
        MallocCallTracker::singleton().recordMalloc(result, size);
#endif
    BPROFILE_ALLOCATION(COMPACTIBLE, result, size);
    return result;
}

TryMallocReturnValue tryFastCompactZeroedMalloc(size_t size)
{
    FAIL_IF_EXCEEDS_LIMIT(size);
    assertMallocRestrictionForCurrentThreadScope();
    return bmalloc::api::tryZeroedMalloc(size, bmalloc::CompactAllocationMode::Compact);
}

void* fastCompactCalloc(size_t numElements, size_t elementSize)
{
    ASSERT_IS_WITHIN_LIMIT(numElements * elementSize);
    Checked<size_t> checkedSize = elementSize;
    checkedSize *= numElements;
    void* result = fastZeroedMalloc(checkedSize);
    if (!result)
        CRASH();
    BPROFILE_ALLOCATION(COMPACTIBLE, result, elementSize);
    return result;
}

void* fastCompactRealloc(void* object, size_t size)
{
    ASSERT_IS_WITHIN_LIMIT(size);
    assertMallocRestrictionForCurrentThreadScope();
    void* result = bmalloc::api::realloc(object, size, bmalloc::CompactAllocationMode::Compact);
#if ENABLE(MALLOC_HEAP_BREAKDOWN) && TRACK_MALLOC_CALLSTACK
    if (!AvoidRecordingScope::avoidRecordingCount())
        MallocCallTracker::singleton().recordRealloc(object, result, size);
#endif
    BPROFILE_ALLOCATION(COMPACTIBLE, result, size);
    return result;
}

void* fastCompactAlignedMalloc(size_t alignment, size_t size)
{
    ASSERT_IS_WITHIN_LIMIT(size);
    assertMallocRestrictionForCurrentThreadScope();
    void* result = bmalloc::api::memalign(alignment, size, bmalloc::CompactAllocationMode::Compact);
#if ENABLE(MALLOC_HEAP_BREAKDOWN) && TRACK_MALLOC_CALLSTACK
    if (!AvoidRecordingScope::avoidRecordingCount())
        MallocCallTracker::singleton().recordMalloc(result, size);
#endif
    BPROFILE_ALLOCATION(COMPACTIBLE, result, size);
    return result;
}

void* tryFastCompactAlignedMalloc(size_t alignment, size_t size)
{
    FAIL_IF_EXCEEDS_LIMIT(size);
    assertMallocRestrictionForCurrentThreadScope();
    void* result = bmalloc::api::tryMemalign(alignment, size, bmalloc::CompactAllocationMode::Compact);
#if ENABLE(MALLOC_HEAP_BREAKDOWN) && TRACK_MALLOC_CALLSTACK
    if (!AvoidRecordingScope::avoidRecordingCount())
        MallocCallTracker::singleton().recordMalloc(result, size);
#endif
    BPROFILE_ALLOCATION(COMPACTIBLE, result, size);
    return result;
}

TryMallocReturnValue tryFastCompactMalloc(size_t size)
{
    FAIL_IF_EXCEEDS_LIMIT(size);
    assertMallocRestrictionForCurrentThreadScope();
    void* result = bmalloc::api::tryMalloc(size, bmalloc::CompactAllocationMode::Compact);
    BPROFILE_ALLOCATION(COMPACTIBLE, result, size);
    return result;
}

TryMallocReturnValue tryFastCompactCalloc(size_t numElements, size_t elementSize)
{
    FAIL_IF_EXCEEDS_LIMIT(numElements * elementSize);
    CheckedSize checkedSize = elementSize;
    checkedSize *= numElements;
    if (checkedSize.hasOverflowed())
        return nullptr;
    return tryFastCompactZeroedMalloc(checkedSize);
}

TryMallocReturnValue tryFastCompactRealloc(void* object, size_t newSize)
{
    FAIL_IF_EXCEEDS_LIMIT(newSize);
    assertMallocRestrictionForCurrentThreadScope();
    void* result = bmalloc::api::tryRealloc(object, newSize, bmalloc::CompactAllocationMode::Compact);
    BPROFILE_ALLOCATION(COMPACTIBLE, result, newSize);
    return result;
}

void releaseFastMallocFreeMemoryForThisThread()
{
    bmalloc::api::scavengeThisThread();
}

void releaseFastMallocFreeMemory()
{
    bmalloc::api::scavenge();
}

FastMallocStatistics fastMallocStatistics()
{

    // FIXME: Can bmalloc itself report the stats instead of relying on the OS?
    FastMallocStatistics statistics;
    statistics.freeListBytes = 0;
    statistics.reservedVMBytes = 0;

#if OS(WINDOWS)
    PROCESS_MEMORY_COUNTERS resourceUsage;
    GetProcessMemoryInfo(GetCurrentProcess(), &resourceUsage, sizeof(resourceUsage));
    statistics.committedVMBytes = resourceUsage.PeakWorkingSetSize;
#elif OS(HAIKU)
	ssize_t cookie = NULL;
	statistics.committedVMBytes = 0;
	area_info info;
	while(get_next_area_info(B_CURRENT_TEAM, &cookie, &info) == B_OK)
	{
		statistics.committedVMBytes += info.ram_size;
	}
#elif HAVE(RESOURCE_H)
    struct rusage resourceUsage;
    getrusage(RUSAGE_SELF, &resourceUsage);

#if OS(DARWIN)
    statistics.committedVMBytes = resourceUsage.ru_maxrss;
#else
    statistics.committedVMBytes = resourceUsage.ru_maxrss * 1024;
#endif // OS(DARWIN)

#endif // OS(WINDOWS)
    return statistics;
}

void fastCommitAlignedMemory(void* ptr, size_t size)
{
    bmalloc::api::commitAlignedPhysical(ptr, size);
}

void fastDecommitAlignedMemory(void* ptr, size_t size)
{
    bmalloc::api::decommitAlignedPhysical(ptr, size);
}

void fastEnableMiniMode(bool forceMiniMode)
{
    bmalloc::api::enableMiniMode(forceMiniMode);
}

void fastDisableScavenger()
{
    bmalloc::api::disableScavenger();
}

void forceEnablePGM(uint16_t guardMallocRate)
{
    bmalloc::api::forceEnablePGM(guardMallocRate);
}

} // namespace WTF

#endif // USE(SYSTEM_MALLOC)
