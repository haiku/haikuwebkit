/*
 * Copyright (C) 2014-2018 Apple Inc. All rights reserved.
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
#import "InteractionInformationAtPosition.h"

#import "ArgumentCodersCocoa.h"
#import <wtf/cocoa/VectorCocoa.h>
#import <pal/cocoa/DataDetectorsCoreSoftLink.h>

namespace WebKit {

#if PLATFORM(IOS_FAMILY)

InteractionInformationAtPosition::InteractionInformationAtPosition(InteractionInformationRequest&& request, bool canBeValid, std::optional<bool> hitNodeOrWindowHasDoubleClickListener, Selectability&& selectability, bool isSelected, bool prefersDraggingOverTextSelection, bool isNearMarkedText, bool touchCalloutEnabled, bool isLink, bool isImage,
#if ENABLE(MODEL_PROCESS)
    bool isInteractiveModel,
#endif
    bool isAttachment, bool isAnimatedImage, bool isAnimating, bool canShowAnimationControls, bool isPausedVideo, bool isElement, bool isContentEditable, Markable<WebCore::ScrollingNodeID>&& containerScrollingNodeID,
#if ENABLE(DATA_DETECTION)
    bool isDataDetectorLink,
#endif
    bool preventTextInteraction, bool elementContainsImageOverlay, bool isImageOverlayText,
#if ENABLE(SPATIAL_IMAGE_DETECTION)
    bool isSpatialImage,
#endif
    bool isInPlugin, bool needsPointerTouchCompatibilityQuirk, WebCore::FloatPoint&& adjustedPointForNodeRespondingToClickEvents, URL&& url, URL&& imageURL, String&& imageMIMEType, String&& title, String&& idAttribute, WebCore::IntRect&& bounds,
#if PLATFORM(MACCATALYST)
    WebCore::IntRect&& caretRect,
#endif
    RefPtr<WebCore::ShareableBitmap>&& image, String&& textBefore, String&& textAfter, CursorContext&& cursorContext, RefPtr<WebCore::TextIndicator>&& textIndicator,
#if ENABLE(DATA_DETECTION)
    String&& dataDetectorIdentifier, Vector<RetainPtr<DDScannerResult>>&& dataDetectorResults, WebCore::IntRect&& dataDetectorBounds,
#endif
#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
    Vector<WebCore::ElementAnimationContext>&& animationsAtPoint,
#endif
    std::optional<WebCore::ElementContext>&& elementContext, std::optional<WebCore::ElementContext>&& hostImageOrVideoElementContext)
        : request(WTFMove(request))
        , canBeValid(canBeValid)
        , hitNodeOrWindowHasDoubleClickListener(hitNodeOrWindowHasDoubleClickListener)
        , selectability(selectability)
        , isSelected(isSelected)
        , prefersDraggingOverTextSelection(prefersDraggingOverTextSelection)
        , isNearMarkedText(isNearMarkedText)
        , touchCalloutEnabled(touchCalloutEnabled)
        , isLink(isLink)
        , isImage(isImage)
#if ENABLE(MODEL_PROCESS)
        , isInteractiveModel(isInteractiveModel)
#endif
        , isAttachment(isAttachment)
        , isAnimatedImage(isAnimatedImage)
        , isAnimating(isAnimating)
        , canShowAnimationControls(canShowAnimationControls)
        , isPausedVideo(isPausedVideo)
        , isElement(isElement)
        , isContentEditable(isContentEditable)
        , containerScrollingNodeID(WTFMove(containerScrollingNodeID))
#if ENABLE(DATA_DETECTION)
        , isDataDetectorLink(isDataDetectorLink)
#endif
        , preventTextInteraction(preventTextInteraction)
        , elementContainsImageOverlay(elementContainsImageOverlay)
        , isImageOverlayText(isImageOverlayText)
#if ENABLE(SPATIAL_IMAGE_DETECTION)
        , isSpatialImage(isSpatialImage)
#endif
        , isInPlugin(isInPlugin)
        , needsPointerTouchCompatibilityQuirk(needsPointerTouchCompatibilityQuirk)
        , adjustedPointForNodeRespondingToClickEvents(WTFMove(adjustedPointForNodeRespondingToClickEvents))
        , url(WTFMove(url))
        , imageURL(WTFMove(imageURL))
        , imageMIMEType(WTFMove(imageMIMEType))
        , title(WTFMove(title))
        , idAttribute(WTFMove(idAttribute))
        , bounds(WTFMove(bounds))
#if PLATFORM(MACCATALYST)
        , caretRect(WTFMove(caretRect))
#endif
        , image(WTFMove(image))
        , textBefore(WTFMove(textBefore))
        , textAfter(WTFMove(textAfter))
        , cursorContext(WTFMove(cursorContext))
        , textIndicator(WTFMove(textIndicator))
#if ENABLE(DATA_DETECTION)
        , dataDetectorIdentifier(WTFMove(dataDetectorIdentifier))
        , dataDetectorResults(createNSArray(WTFMove(dataDetectorResults), [](RetainPtr<DDScannerResult>&& result) { return result.get(); }))
        , dataDetectorBounds(WTFMove(dataDetectorBounds))
#endif
#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
        , animationsAtPoint(WTFMove(animationsAtPoint))
#endif
        , elementContext(WTFMove(elementContext))
        , hostImageOrVideoElementContext(WTFMove(hostImageOrVideoElementContext))
{
}

void InteractionInformationAtPosition::mergeCompatibleOptionalInformation(const InteractionInformationAtPosition& oldInformation)
{
    if (oldInformation.request.point != request.point)
        return;

    if (oldInformation.request.includeSnapshot && !request.includeSnapshot)
        image = oldInformation.image;

    if (oldInformation.request.includeLinkIndicator && !request.includeLinkIndicator)
        textIndicator = oldInformation.textIndicator;
}

#if ENABLE(DATA_DETECTION)
Vector<RetainPtr<DDScannerResult>> InteractionInformationAtPosition::serializableDataDetectorResults() const
{
    return makeVector(dataDetectorResults.get(), [](DDScannerResult *result) {
        return std::optional(RetainPtr<DDScannerResult>(result));
    });
}
#endif // ENABLE(DATA_DETECTION)

#endif // PLATFORM(IOS_FAMILY)

}
