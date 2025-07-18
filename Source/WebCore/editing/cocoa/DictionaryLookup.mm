/*
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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
#import "DictionaryLookup.h"

#import "SimpleRange.h"

#if PLATFORM(COCOA)

#if ENABLE(REVEAL)

#import "Document.h"
#import "Editing.h"
#import "FocusController.h"
#import "FrameSelection.h"
#import "GraphicsContextCG.h"
#import "HitTestResult.h"
#import "LocalFrame.h"
#import "NotImplemented.h"
#import "Page.h"
#import "Range.h"
#import "RenderObject.h"
#import "RevealUtilities.h"
#import "TextIterator.h"
#import "VisiblePosition.h"
#import "VisibleSelection.h"
#import "VisibleUnits.h"
#import <pal/cocoa/RevealSoftLink.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/RefPtr.h>

#if PLATFORM(MAC)
#import <Quartz/Quartz.h>
#else
#import <PDFKit/PDFKit.h>
#import <pal/spi/ios/UIKitSPI.h>

#import <pal/ios/UIKitSoftLink.h>
#endif

#if PLATFORM(MACCATALYST)
#import <UIKitMacHelper/UINSRevealController.h>
SOFT_LINK_PRIVATE_FRAMEWORK(UIKitMacHelper)
SOFT_LINK(UIKitMacHelper, UINSSharedRevealController, id<UINSRevealController>, (void), ())
#endif // PLATFORM(MACCATALYST)

#if PLATFORM(MAC)

@interface WebRevealHighlight : NSObject<RVPresenterHighlightDelegate>

@property (nonatomic, readonly) NSRect highlightRect;
@property (nonatomic, readonly) BOOL useDefaultHighlight;
@property (nonatomic, readonly) RetainPtr<NSAttributedString> attributedString;

- (instancetype)initWithHighlightRect:(NSRect)highlightRect useDefaultHighlight:(BOOL)useDefaultHighlight attributedString:(NSAttributedString *)attributedString clearTextIndicatorCallback:(Function<void()>&&)clearTextIndicatorCallback;

@end

@implementation WebRevealHighlight {
    Function<void()> _clearTextIndicator;
}

- (instancetype)initWithHighlightRect:(NSRect)highlightRect useDefaultHighlight:(BOOL)useDefaultHighlight attributedString:(NSAttributedString *)attributedString clearTextIndicatorCallback:(Function<void()>&&)clearTextIndicatorCallback
{
    if (!(self = [super init]))
        return nil;
    
    _highlightRect = highlightRect;
    _useDefaultHighlight = useDefaultHighlight;
    _attributedString = adoptNS([attributedString copy]);
    _clearTextIndicator = WTFMove(clearTextIndicatorCallback);
    
    return self;
}

- (NSArray<NSValue *> *)revealContext:(RVPresentingContext *)context rectsForItem:(RVItem *)item
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(item);
    return @[[NSValue valueWithRect:self.highlightRect]];
}

- (void)revealContext:(RVPresentingContext *)context drawRectsForItem:(RVItem *)item
{
    UNUSED_PARAM(item);
    
    for (NSValue *rectVal in context.itemRectsInView) {
        NSRect rect = rectVal.rectValue;

        // Get current font attributes from the attributed string above, and add paragraph style attribute in order to center text.
        auto attributes = adoptNS([[NSMutableDictionary alloc] initWithDictionary:[self.attributedString fontAttributesInRange:NSMakeRange(0, [self.attributedString length])]]);
        auto paragraph = adoptNS([[NSMutableParagraphStyle alloc] init]);
        [paragraph setAlignment:NSTextAlignmentCenter];
        [attributes setObject:paragraph.get() forKey:NSParagraphStyleAttributeName];
    
        auto string = adoptNS([[NSAttributedString alloc] initWithString:[self.attributedString string] attributes:attributes.get()]);
        [string drawInRect:rect];
    }
}

- (BOOL)revealContext:(RVPresentingContext *)context shouldUseDefaultHighlightForItem:(RVItem *)item
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(item);
    return self.useDefaultHighlight;
}

- (void)revealContext:(RVPresentingContext *)context stopHighlightingItem:(RVItem *)item
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(item);
    if (auto block = std::exchange(_clearTextIndicator, nullptr))
        block();
}

@end

#elif PLATFORM(MACCATALYST) // PLATFORM(MAC)

@interface WebRevealHighlight : NSObject<UIRVPresenterHighlightDelegate>

- (instancetype)initWithHighlightRect:(NSRect)highlightRect view:(UIView *)view image:(RefPtr<WebCore::Image>&&)image;

@end

@implementation WebRevealHighlight {
    RefPtr<WebCore::Image> _image;
    CGRect _highlightRect;
    BOOL _highlighting;
    UIView *_view;
}

- (instancetype)initWithHighlightRect:(NSRect)highlightRect view:(UIView *)view image:(RefPtr<WebCore::Image>&&)image
{
    if (!(self = [super init]))
        return nil;
    
    _highlightRect = highlightRect;
    _view = view;
    _highlighting = NO;
    _image = image;
    
    return self;
}

- (void)setImage:(RefPtr<WebCore::Image>&&)image
{
    _image = WTFMove(image);
}

- (NSArray<NSValue *> *)highlightRectsForItem:(RVItem *)item
{
    UNUSED_PARAM(item);
    return @[[NSValue valueWithCGRect:_highlightRect]];
}

- (void)startHighlightingItem:(RVItem *)item
{
    UNUSED_PARAM(item);
    _highlighting = YES;
}

- (void)highlightItem:(RVItem *)item withProgress:(CGFloat)progress
{
    UNUSED_PARAM(item);
    UNUSED_PARAM(progress);
}

- (void)completeHighlightingItem:(RVItem *)item
{
    UNUSED_PARAM(item);
}

- (void)stopHighlightingItem:(RVItem *)item
{
    UNUSED_PARAM(item);
    _highlighting = NO;
}

- (void)highlightRangeChangedForItem:(RVItem *)item
{
    UNUSED_PARAM(item);
}

- (BOOL)highlighting
{
    return _highlighting;
}

- (void)drawHighlightContentForItem:(RVItem *)item context:(CGContextRef)context
{
    NSArray <NSValue *> *rects = [self highlightRectsForItem:item];
    if (!rects.count)
        return;
    
    CGRect highlightRect = rects.firstObject.CGRectValue;
    for (NSValue *rect in rects)
        highlightRect = CGRectUnion(highlightRect, rect.CGRectValue);
    highlightRect = [_view convertRect:highlightRect fromView:nil];
    
    WebCore::CGContextStateSaver saveState(context);
    CGAffineTransform contextTransform = CGContextGetCTM(context);
    CGFloat backingScale = contextTransform.a;
    CGFloat macCatalystScaleFactor = [PAL::getUIApplicationClass() sharedApplication]._iOSMacScale;
    CGAffineTransform transform = CGAffineTransformMakeScale(macCatalystScaleFactor * backingScale, macCatalystScaleFactor * backingScale);
    CGContextSetCTM(context, transform);
    
    for (NSValue *v in rects) {
        CGRect imgSrcRect = [_view convertRect:v.CGRectValue fromView:nil];
        auto nativeImage = _image->nativeImage();
        CGRect origin = CGRectMake(imgSrcRect.origin.x - highlightRect.origin.x, imgSrcRect.origin.y - highlightRect.origin.y, highlightRect.size.width, highlightRect.size.height);
        CGContextDrawImage(context, origin, nativeImage->platformImage().get());
    }
}

@end

#endif // PLATFORM(MACCATALYST)

#endif // ENABLE(REVEAL)

namespace WebCore {

#if ENABLE(REVEAL)

static bool canCreateRevealItems()
{
    static bool result;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [&] {
        result = PAL::isRevealCoreFrameworkAvailable() && PAL::getRVItemClass();
    });
    return result;
}

std::optional<SimpleRange> DictionaryLookup::rangeForSelection(const VisibleSelection& selection)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    if (!canCreateRevealItems())
        return std::nullopt;

    // Since we already have the range we want, we just need to grab the returned options.
    auto selectionStart = selection.visibleStart();
    auto selectionEnd = selection.visibleEnd();

    // As context, we are going to use the surrounding paragraphs of text.
    auto paragraphStart = startOfParagraph(selectionStart);
    auto paragraphEnd = endOfParagraph(selectionEnd);
    if (paragraphStart.isNull() || paragraphEnd.isNull())
        return std::nullopt;

    auto lengthToSelectionStart = characterCount(*makeSimpleRange(paragraphStart, selectionStart));
    auto selectionCharacterCount = characterCount(*makeSimpleRange(selectionStart, selectionEnd));
    NSRange rangeToPass = NSMakeRange(lengthToSelectionStart, selectionCharacterCount);

    auto fullCharacterRange = *makeSimpleRange(paragraphStart, paragraphEnd);
    String itemString = plainText(fullCharacterRange);
    NSRange highlightRange = adoptNS([PAL::allocRVItemInstance() initWithText:itemString.createNSString().get() selectedRange:rangeToPass]).get().highlightRange;

    return { { resolveCharacterRange(fullCharacterRange, highlightRange) } };

    END_BLOCK_OBJC_EXCEPTIONS

    return std::nullopt;
}

std::optional<SimpleRange> DictionaryLookup::rangeAtHitTestResult(const HitTestResult& hitTestResult)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    
    if (!canCreateRevealItems())
        return std::nullopt;
    
    auto* node = hitTestResult.innerNonSharedNode();
    if (!node || !node->renderer())
        return std::nullopt;

    auto* frame = node->document().frame();
    if (!frame)
        return std::nullopt;

    // Don't do anything if there is no character at the point.
    auto framePoint = hitTestResult.roundedPointInInnerNodeFrame();
    if (!frame->rangeForPoint(framePoint))
        return std::nullopt;

    auto position = frame->visiblePositionForPoint(framePoint);
    if (position.isNull())
        position = firstPositionInOrBeforeNode(node);

    RefPtr focusedOrMainFrame = frame->page()->focusController().focusedOrMainFrame();
    if (!focusedOrMainFrame)
        return std::nullopt;

    auto selection = focusedOrMainFrame->selection().selection();
    NSRange selectionRange;
    NSUInteger hitIndex;
    std::optional<SimpleRange> fullCharacterRange;
    
    if (selection.isRange()) {
        auto selectionStart = selection.visibleStart();
        auto selectionEnd = selection.visibleEnd();

        // As context, we are going to use the surrounding paragraphs of text.
        fullCharacterRange = makeSimpleRange(startOfParagraph(selectionStart), endOfParagraph(selectionEnd));
        if (!fullCharacterRange)
            return std::nullopt;

        selectionRange = NSMakeRange(characterCount(*makeSimpleRange(fullCharacterRange->start, selectionStart)),
            characterCount(*makeSimpleRange(selectionStart, selectionEnd)));
        hitIndex = characterCount(*makeSimpleRange(fullCharacterRange->start, position));
    } else {
        VisibleSelection selectionAccountingForLineRules { position };
        selectionAccountingForLineRules.expandUsingGranularity(TextGranularity::WordGranularity);
        position = selectionAccountingForLineRules.start();

        // As context, we are going to use 250 characters of text before and after the point.
        fullCharacterRange = rangeExpandedAroundPositionByCharacters(position, 250);
        if (!fullCharacterRange)
            return std::nullopt;

        selectionRange = NSMakeRange(NSNotFound, 0);
        hitIndex = characterCount(*makeSimpleRange(fullCharacterRange->start, position));
    }

    NSRange selectedRange = [PAL::getRVSelectionClass() revealRangeAtIndex:hitIndex selectedRanges:@[[NSValue valueWithRange:selectionRange]] shouldUpdateSelection:nil];

    String itemString = plainText(*fullCharacterRange);
    auto highlightRange = adoptNS([PAL::allocRVItemInstance() initWithText:itemString.createNSString().get() selectedRange:selectedRange]).get().highlightRange;

    if (highlightRange.location == NSNotFound || !highlightRange.length)
        return std::nullopt;
    
    return resolveCharacterRange(*fullCharacterRange, highlightRange);
    
    END_BLOCK_OBJC_EXCEPTIONS
    
    return std::nullopt;
    
}

static void expandSelectionByCharacters(PDFSelection *selection, NSInteger numberOfCharactersToExpand, NSInteger& charactersAddedBeforeStart, NSInteger& charactersAddedAfterEnd)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    size_t originalLength = selection.string.length;
    [selection extendSelectionAtStart:numberOfCharactersToExpand];
    
    charactersAddedBeforeStart = selection.string.length - originalLength;
    
    [selection extendSelectionAtEnd:numberOfCharactersToExpand];
    charactersAddedAfterEnd = selection.string.length - originalLength - charactersAddedBeforeStart;

    END_BLOCK_OBJC_EXCEPTIONS
}

NSString *DictionaryLookup::stringForPDFSelection(PDFSelection *selection)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    if (!canCreateRevealItems())
        return nil;

    // Don't do anything if there is no character at the point.
    if (!selection || !selection.string.length)
        return @"";

    RetainPtr<PDFSelection> selectionForLookup = adoptNS([selection copy]);

    // As context, we are going to use 250 characters of text before and after the point.
    auto originalLength = [selectionForLookup string].length;
    NSInteger charactersAddedBeforeStart = 0;
    NSInteger charactersAddedAfterEnd = 0;
    expandSelectionByCharacters(selectionForLookup.get(), 250, charactersAddedBeforeStart, charactersAddedAfterEnd);

    auto fullPlainTextString = [selectionForLookup string];
    auto rangeToPass = NSMakeRange(charactersAddedBeforeStart, 0);

    NSRange extractedRange = adoptNS([PAL::allocRVItemInstance() initWithText:fullPlainTextString selectedRange:rangeToPass]).get().highlightRange;
    if (extractedRange.location == NSNotFound)
        return selection.string;

    NSInteger lookupAddedBefore = rangeToPass.location - extractedRange.location;
    NSInteger lookupAddedAfter = (extractedRange.location + extractedRange.length) - (rangeToPass.location + originalLength);

    [selection extendSelectionAtStart:lookupAddedBefore];
    [selection extendSelectionAtEnd:lookupAddedAfter];

    NSString *selectionString = selection.string;

    auto extractedStringMatchesSelection = [](NSString *selection, NSString *extracted) -> bool {
        return selection ? [selection isEqualToString:extracted] : !extracted.length;
    };
    ASSERT_UNUSED(extractedStringMatchesSelection, extractedStringMatchesSelection(selectionString, [fullPlainTextString substringWithRange:extractedRange]));

    return selectionString;

    END_BLOCK_OBJC_EXCEPTIONS

    return @"";
}

static WKRevealController showPopupOrCreateAnimationController(bool createAnimationController, const DictionaryPopupInfo& dictionaryPopupInfo, CocoaView *view, NOESCAPE const WTF::Function<void(TextIndicator&)>& textIndicatorInstallationCallback, NOESCAPE const WTF::Function<FloatRect(FloatRect)>& rootViewToViewConversionCallback, WTF::Function<void()>&& clearTextIndicator)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    
#if PLATFORM(MAC)
    if (!PAL::isRevealFrameworkAvailable() || !canCreateRevealItems() || !PAL::getRVPresenterClass())
        return nil;

    auto textIndicator = dictionaryPopupInfo.textIndicator;
    if (!textIndicator)
        return nil;

    auto presenter = adoptNS([PAL::allocRVPresenterInstance() init]);

    NSRect highlightRect;
    NSPoint pointerLocation;

    if (textIndicator->contentImage()) {
        textIndicatorInstallationCallback(*textIndicator);

        FloatRect firstTextRectInViewCoordinates = textIndicator->textRectsInBoundingRectCoordinates()[0];
        FloatRect textBoundingRectInViewCoordinates = textIndicator->textBoundingRectInRootViewCoordinates();
        FloatRect selectionBoundingRectInViewCoordinates = textIndicator->selectionRectInRootViewCoordinates();

        if (rootViewToViewConversionCallback) {
            textBoundingRectInViewCoordinates = rootViewToViewConversionCallback(textBoundingRectInViewCoordinates);
            selectionBoundingRectInViewCoordinates = rootViewToViewConversionCallback(selectionBoundingRectInViewCoordinates);
        }

        firstTextRectInViewCoordinates.moveBy(textBoundingRectInViewCoordinates.location());
        highlightRect = selectionBoundingRectInViewCoordinates;
        pointerLocation = firstTextRectInViewCoordinates.location();
    } else {
        NSPoint textBaselineOrigin = dictionaryPopupInfo.origin;
        highlightRect = textIndicator->selectionRectInRootViewCoordinates();
        pointerLocation = [view convertPoint:textBaselineOrigin toView:nil];
    }

#if ENABLE(LEGACY_PDFKIT_PLUGIN)
    auto attributedString = dictionaryPopupInfo.platformData.attributedString.nsAttributedString();
    auto webHighlight = adoptNS([[WebRevealHighlight alloc] initWithHighlightRect:highlightRect useDefaultHighlight:!textIndicator->contentImage() attributedString:attributedString.get() clearTextIndicatorCallback:WTFMove(clearTextIndicator)]);
    auto item = adoptNS([PAL::allocRVItemInstance() initWithText:attributedString.get().string selectedRange:NSMakeRange(0, attributedString.get().string.length)]);
#else
    RetainPtr text = dictionaryPopupInfo.text.createNSString();
    RetainPtr webHighlight = adoptNS([[WebRevealHighlight alloc] initWithHighlightRect:highlightRect useDefaultHighlight:!textIndicator->contentImage() attributedString:adoptNS([[NSAttributedString alloc] initWithString:text.get()]).get() clearTextIndicatorCallback:WTFMove(clearTextIndicator)]);
    RetainPtr item = adoptNS([PAL::allocRVItemInstance() initWithText:text.get() selectedRange:NSMakeRange(0, text.get().length)]);
#endif

    auto context = createRVPresentingContextWithRetainedDelegate(pointerLocation, view, webHighlight.get());
    if (createAnimationController)
        return [presenter animationControllerForItem:item.get() documentContext:nil presentingContext:context.get() options:nil];

    [presenter revealItem:item.get() documentContext:nil presentingContext:context.get() options:@{ @"forceLookup": @YES }];
    return nil;
#elif PLATFORM(MACCATALYST)
    UNUSED_PARAM(textIndicatorInstallationCallback);
    UNUSED_PARAM(rootViewToViewConversionCallback);
    UNUSED_PARAM(clearTextIndicator);
    ASSERT_UNUSED(createAnimationController, !createAnimationController);

    RefPtr textIndicator = dictionaryPopupInfo.textIndicator;
    if (!textIndicator)
        return nil;

    auto webHighlight = adoptNS([[WebRevealHighlight alloc] initWithHighlightRect:[view convertRect:textIndicator->selectionRectInRootViewCoordinates() toView:nil] view:view image:textIndicator->contentImage()]);
#if ENABLE(LEGACY_PDFKIT_PLUGIN)
    auto attributedString = dictionaryPopupInfo.platformData.attributedString.nsAttributedString();
    auto item = adoptNS([PAL::allocRVItemInstance() initWithText:attributedString.get().string selectedRange:NSMakeRange(0, attributedString.get().string.length)]);
#else
    RetainPtr text = dictionaryPopupInfo.text.createNSString();
    RetainPtr item = adoptNS([PAL::allocRVItemInstance() initWithText:text.get() selectedRange:NSMakeRange(0, text.get().length)]);
#endif

    [UINSSharedRevealController() revealItem:item.get() locationInWindow:dictionaryPopupInfo.origin window:view.window highlighter:webHighlight.get()];
    return nil;
#else // PLATFORM(IOS_FAMILY)
    UNUSED_PARAM(createAnimationController);
    UNUSED_PARAM(dictionaryPopupInfo);
    UNUSED_PARAM(view);
    UNUSED_PARAM(textIndicatorInstallationCallback);
    UNUSED_PARAM(rootViewToViewConversionCallback);
    UNUSED_PARAM(clearTextIndicator);
    return nil;
#endif // PLATFORM(IOS_FAMILY)

    END_BLOCK_OBJC_EXCEPTIONS
}

void DictionaryLookup::showPopup(const DictionaryPopupInfo& dictionaryPopupInfo, CocoaView *view, NOESCAPE const WTF::Function<void(TextIndicator&)>& textIndicatorInstallationCallback, NOESCAPE const WTF::Function<FloatRect(FloatRect)>& rootViewToViewConversionCallback, WTF::Function<void()>&& clearTextIndicator)
{
    showPopupOrCreateAnimationController(false, dictionaryPopupInfo, view, textIndicatorInstallationCallback, rootViewToViewConversionCallback, WTFMove(clearTextIndicator));
}

void DictionaryLookup::hidePopup()
{
    notImplemented();
}

#if PLATFORM(MAC)

WKRevealController DictionaryLookup::animationControllerForPopup(const DictionaryPopupInfo& dictionaryPopupInfo, NSView *view, NOESCAPE const WTF::Function<void(TextIndicator&)>& textIndicatorInstallationCallback, NOESCAPE const WTF::Function<FloatRect(FloatRect)>& rootViewToViewConversionCallback, WTF::Function<void()>&& clearTextIndicator)
{
    return showPopupOrCreateAnimationController(true, dictionaryPopupInfo, view, textIndicatorInstallationCallback, rootViewToViewConversionCallback, WTFMove(clearTextIndicator));
}

#endif // PLATFORM(MAC)

#elif PLATFORM(IOS_FAMILY) // PLATFORM(IOS_FAMILY) && !ENABLE(REVEAL)

std::optional<SimpleRange> DictionaryLookup::rangeForSelection(const VisibleSelection&)
{
    return std::nullopt;
}

std::optional<SimpleRange> DictionaryLookup::rangeAtHitTestResult(const HitTestResult&)
{
    return std::nullopt;
}

#endif

} // namespace WebCore

#endif // PLATFORM(COCOA)
