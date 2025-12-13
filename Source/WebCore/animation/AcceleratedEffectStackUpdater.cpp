/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "AcceleratedEffectStackUpdater.h"

#if ENABLE(THREADED_ANIMATIONS)

#include "AcceleratedTimeline.h"
#include "KeyframeEffectStack.h"
#include "RenderElement.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderLayerModelObject.h"
#include "RenderStyleConstants.h"
#include "ScrollTimeline.h"
#include "Styleable.h"

namespace WebCore {

void AcceleratedEffectStackUpdater::update()
{
    if (!hasTargetsPendingUpdate())
        return;

    HashSet<Ref<AcceleratedTimeline>> timelinesInUpdate;

    auto targetsPendingUpdate = std::exchange(m_targetsPendingUpdate, { });
    for (auto [element, pseudoElementIdentifier] : targetsPendingUpdate) {
        if (!element)
            continue;

        Styleable target { *element, pseudoElementIdentifier };

        auto* renderer = dynamicDowncast<RenderLayerModelObject>(target.renderer());
        if (!renderer || !renderer->isComposited())
            continue;

        auto* renderLayer = renderer->layer();
        ASSERT(renderLayer && renderLayer->backing());
        renderLayer->backing()->updateAcceleratedEffectsAndBaseValues(timelinesInUpdate);
    }

    for (auto& timeline : timelinesInUpdate) {
        auto& timelineIdentifier = timeline->identifier();
        auto addResult = m_timelines.add(timelineIdentifier, timeline.ptr());
        if (addResult.isNewEntry)
            m_timelinesUpdate.created.add(timeline);
    }
}

void AcceleratedEffectStackUpdater::scheduleUpdateForTarget(const Styleable& target)
{
    m_targetsPendingUpdate.add({ &target.element, target.pseudoElementIdentifier });
}

void AcceleratedEffectStackUpdater::scrollTimelineDidChange(ScrollTimeline& timeline)
{
    m_scrollTimelinesPendingUpdate.add(timeline);
}

AcceleratedTimelinesUpdate AcceleratedEffectStackUpdater::takeTimelinesUpdate()
{
    // All known accelerated timelines that got destroyed since the last update
    // will now be null references. Add them to the list of destroyed timelines.
    for (auto& [timelineIdentifier, timeline] : m_timelines) {
        if (!timeline)
            m_timelinesUpdate.destroyed.add(timelineIdentifier);
    }

    // Prune all those destroyed timelines from our list of know accelerated timelines.
    for (auto& identifierToRemove : m_timelinesUpdate.destroyed)
        m_timelines.remove(identifierToRemove);

    // Finally, process all timelines that were marked as requiring an update, either
    // marking them as modified or destroyed if they no longer are accelerated.
    auto scrollTimelinesPendingUpdate = std::exchange(m_scrollTimelinesPendingUpdate, { });
    for (auto& scrollTimeline : scrollTimelinesPendingUpdate) {
        auto timelineIdentifier = scrollTimeline->acceleratedTimelineIdentifier();
        auto acceleratedTimeline = m_timelines.getOptional(timelineIdentifier);
        if (acceleratedTimeline && scrollTimeline->canBeAccelerated()) {
            ASSERT(*acceleratedTimeline);
            scrollTimeline->updateAcceleratedRepresentation();
            m_timelinesUpdate.modified.add(**acceleratedTimeline);
        } else
            m_timelinesUpdate.destroyed.add(timelineIdentifier);
    }

    return std::exchange(m_timelinesUpdate, { });
}

} // namespace WebCore

#endif // ENABLE(THREADED_ANIMATIONS)
