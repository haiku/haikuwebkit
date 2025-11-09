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

#import "config.h"
#import "RemoteMonotonicTimelineRegistry.h"

#if ENABLE(THREADED_ANIMATIONS)

#import <WebCore/AcceleratedTimeline.h>
#import <wtf/MonotonicTime.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteMonotonicTimelineRegistry);

void RemoteMonotonicTimelineRegistry::update(WebCore::ProcessIdentifier processIdentifier, const HashSet<Ref<WebCore::AcceleratedTimeline>>& timelineRepresentations, MonotonicTime now)
{
    // If there are no active timelines for this process identifier, simply remove that entry.
    if (timelineRepresentations.isEmpty()) {
        m_timelines.remove(processIdentifier);
        return;
    }

    // Populate the list of active timelines, creating new timelines as necessary.
    HashSet<Ref<RemoteMonotonicTimeline>> activeTimelines;
    for (auto& timelineRepresentation : timelineRepresentations) {
        if (!timelineRepresentation->isMonotonic())
            continue;

        TimelineID timelineID { timelineRepresentation->identifier(), processIdentifier };
        ASSERT(timelineRepresentation->originTime());
        if (RefPtr existingTimeline = get(timelineID))
            activeTimelines.add(existingTimeline.releaseNonNull());
        else
            activeTimelines.add(RemoteMonotonicTimeline::create(timelineID, *timelineRepresentation->originTime(), now));
    }

    // Replace the timelines, which will clear any remaining timeline.
    if (activeTimelines.isEmpty())
        m_timelines.remove(processIdentifier);
    else
        m_timelines.set(processIdentifier, WTFMove(activeTimelines));
}

RemoteMonotonicTimeline* RemoteMonotonicTimelineRegistry::get(const TimelineID& timelineID) const
{
    auto it = m_timelines.find(timelineID.processIdentifier());
    if (it == m_timelines.end())
        return nullptr;

    for (auto& timeline : it->value) {
        if (timeline->identifier() == timelineID)
            return timeline.ptr();
    }

    return nullptr;
}

void RemoteMonotonicTimelineRegistry::advanceCurrentTime(MonotonicTime now)
{
    for (auto& timelines : m_timelines.values()) {
        for (auto& timeline : timelines)
            timeline->updateCurrentTime(now);
    }
}

} // namespace WebKit

#endif // ENABLE(THREADED_ANIMATIONS)
