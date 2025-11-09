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
#import "RemoteProgressBasedTimeline.h"

#if ENABLE(THREADED_ANIMATION_RESOLUTION)

#import <WebCore/AcceleratedTimeline.h>
#import <WebCore/ScrollingTreeScrollingNode.h>
#import <WebCore/WebAnimationTime.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(RemoteProgressBasedTimeline);

Ref<RemoteProgressBasedTimeline> RemoteProgressBasedTimeline::create(TimelineID identifier, const WebCore::ProgressResolutionData& resolutionData)
{
    return adoptRef(*new RemoteProgressBasedTimeline(identifier, resolutionData));
}

RemoteProgressBasedTimeline::RemoteProgressBasedTimeline(TimelineID identifier, const WebCore::ProgressResolutionData& resolutionData)
    : RemoteAnimationTimeline(identifier, resolutionData.duration)
    , m_resolutionData(resolutionData)
{
    updateCurrentTime();
}

void RemoteProgressBasedTimeline::updateCurrentTime(const WebCore::ScrollingTreeScrollingNode& node)
{
    auto unconstrainedScrollOffset = m_resolutionData.isVertical ? node.currentScrollOffset().y() : node.currentScrollOffset().x();
    m_resolutionData.scrollOffset = std::clamp(unconstrainedScrollOffset, m_resolutionData.rangeStart, m_resolutionData.rangeEnd);
    updateCurrentTime();
}

void RemoteProgressBasedTimeline::updateCurrentTime()
{
    auto range = m_resolutionData.rangeEnd - m_resolutionData.rangeStart;
    ASSERT(range);

    auto distance = m_resolutionData.isReversed ? m_resolutionData.rangeEnd - m_resolutionData.scrollOffset : m_resolutionData.scrollOffset - m_resolutionData.rangeStart;
    auto progress = distance / range;
    m_currentTime = WebCore::WebAnimationTime::fromPercentage(progress * 100);
}

} // namespace WebKit

#endif // ENABLE(THREADED_ANIMATION_RESOLUTION)

