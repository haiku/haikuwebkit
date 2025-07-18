/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "FunctionHasExecutedCache.h"

#include <limits.h>

namespace JSC {

bool FunctionHasExecutedCache::hasExecutedAtOffset(SourceID id, unsigned offset)
{
    auto iterator = m_rangeMap.find(id);
    if (iterator == m_rangeMap.end())
        return false;

    RangeMap& map = iterator->value;
    unsigned distance = UINT_MAX;
    bool hasExecuted = false;
    for (auto& pair : map) {
        const FunctionRange& range = pair.key.key();
        if (range.m_start <= offset && offset <= range.m_end && range.m_end - range.m_start < distance) {
            hasExecuted = pair.value;
            distance = range.m_end - range.m_start;
        }
    }

    return hasExecuted;
}

void FunctionHasExecutedCache::insertUnexecutedRange(SourceID id, unsigned start, unsigned end)
{
    RangeMap& map = m_rangeMap.add(id, RangeMap { }).iterator->value;
    FunctionRange range;
    range.m_start = start;
    range.m_end = end;
    // Only insert unexecuted ranges once for a given sourceID because we may run into a situation where an executable executes, then is GCed, and then is allocated again,
    // and tries to reinsert itself, claiming it has never run, but this is false because it indeed already executed.
    map.add(range, false);
}

void FunctionHasExecutedCache::removeUnexecutedRange(SourceID id, unsigned start, unsigned end)
{
    // FIXME: We should never have an instance where we return here, but currently do in some situations. Find out why.
    auto iterator = m_rangeMap.find(id);
    if (iterator == m_rangeMap.end())
        return;

    RangeMap& map = iterator->value;

    FunctionRange range;
    range.m_start = start;
    range.m_end = end;
    map.set(range, true);
}

Vector<std::tuple<bool, unsigned, unsigned>> FunctionHasExecutedCache::getFunctionRanges(SourceID id)
{
    Vector<std::tuple<bool, unsigned, unsigned>> ranges(0);
    auto iterator = m_rangeMap.find(id);
    if (iterator == m_rangeMap.end())
        return ranges;

    RangeMap& map = iterator->value;
    for (auto& pair : map) {
        const FunctionRange& range = pair.key.key();
        bool hasExecuted = pair.value;
        ranges.append(std::tuple<bool, unsigned, unsigned>(hasExecuted, range.m_start, range.m_end));
    }

    return ranges;
}

} // namespace JSC
