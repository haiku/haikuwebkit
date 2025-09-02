/*
 * Copyright (C) 2024-2025 Apple Inc. All rights reserved.
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
#include "TextExtraction.h"

#include "AXObjectCache.h"
#include "AccessibilityObject.h"
#include "BoundaryPointInlines.h"
#include "ComposedTreeIterator.h"
#include "ContainerNodeInlines.h"
#include "Editor.h"
#include "ElementInlines.h"
#include "EventListenerMap.h"
#include "EventNames.h"
#include "EventTargetInlines.h"
#include "ExceptionCode.h"
#include "ExceptionOr.h"
#include "FrameSelection.h"
#include "HTMLAnchorElement.h"
#include "HTMLBodyElement.h"
#include "HTMLButtonElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "HTMLSelectElement.h"
#include "HandleUserInputEventResult.h"
#include "HitTestResult.h"
#include "ImageOverlay.h"
#include "LocalFrame.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "PositionInlines.h"
#include "RenderBox.h"
#include "RenderDescendantIterator.h"
#include "RenderIFrame.h"
#include "RenderLayer.h"
#include "RenderLayerModelObject.h"
#include "RenderLayerScrollableArea.h"
#include "RenderObjectInlines.h"
#include "RenderView.h"
#include "SimpleRange.h"
#include "Text.h"
#include "TextIterator.h"
#include "UserGestureIndicator.h"
#include "UserTypingGestureIndicator.h"
#include "VisibleSelection.h"
#include "WritingMode.h"
#include <ranges>
#include <unicode/uchar.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {
namespace TextExtraction {

static constexpr auto minOpacityToConsiderVisible = 0.05;

using TextNodesAndText = Vector<std::pair<Ref<Text>, String>>;
using TextAndSelectedRange = std::pair<String, std::optional<CharacterRange>>;
using TextAndSelectedRangeMap = HashMap<RefPtr<Text>, TextAndSelectedRange>;

static inline TextNodesAndText collectText(const SimpleRange& range)
{
    TextNodesAndText nodesAndText;
    RefPtr<Text> lastTextNode;
    StringBuilder textForLastTextNode;

    auto emitTextForLastNode = [&] {
        auto text = makeStringByReplacingAll(textForLastTextNode.toString(), noBreakSpace, ' ');
        if (text.isEmpty())
            return;
        nodesAndText.append({ lastTextNode.releaseNonNull(), WTFMove(text) });
    };

    for (TextIterator iterator { range, TextIteratorBehavior::EntersTextControls }; !iterator.atEnd(); iterator.advance()) {
        if (iterator.text().isEmpty())
            continue;

        RefPtr textNode = dynamicDowncast<Text>(iterator.node());
        if (!textNode) {
            textForLastTextNode.append(iterator.text());
            continue;
        }

        if (!lastTextNode)
            lastTextNode = textNode;

        if (lastTextNode == textNode) {
            textForLastTextNode.append(iterator.text());
            continue;
        }

        emitTextForLastNode();
        textForLastTextNode.clear();
        textForLastTextNode.append(iterator.text());
        lastTextNode = textNode;
    }

    if (lastTextNode)
        emitTextForLastNode();

    return nodesAndText;
}

struct TraversalContext {
    const TextAndSelectedRangeMap visibleText;
    const std::optional<WebCore::FloatRect> rectInRootView;
    unsigned onlyCollectTextAndLinksCount { 0 };
    bool mergeParagraphs { false };
    bool skipNearlyTransparentContent { false };
    bool canIncludeIdentifiers { false };

    inline bool shouldIncludeNodeWithRect(const FloatRect& rect) const
    {
        return !rectInRootView || rectInRootView->intersects(rect);
    }
};

static inline TextAndSelectedRangeMap collectText(Document& document)
{
    auto fullRange = makeRangeSelectingNodeContents(*document.body());
    auto selection = document.selection().selection();
    TextNodesAndText textBeforeRangedSelection;
    TextNodesAndText textInRangedSelection;
    TextNodesAndText textAfterRangedSelection;
    [&] {
        if (selection.isRange()) {
            auto selectionStart = selection.start();
            auto selectionEnd = selection.end();
            auto rangeBeforeSelection = makeSimpleRange(fullRange.start, selectionStart);
            auto selectionRange = makeSimpleRange(selectionStart, selectionEnd);
            auto rangeAfterSelection = makeSimpleRange(selectionEnd, fullRange.end);
            if (rangeBeforeSelection && selectionRange && rangeAfterSelection) {
                textBeforeRangedSelection = collectText(*rangeBeforeSelection);
                textInRangedSelection = collectText(*selectionRange);
                textAfterRangedSelection = collectText(*rangeAfterSelection);
                return;
            }
        }
        // Fall back to collecting the full document.
        textBeforeRangedSelection = collectText(fullRange);
    }();

    TextAndSelectedRangeMap result;
    for (auto& [node, text] : textBeforeRangedSelection)
        result.add(node.ptr(), TextAndSelectedRange { text, { } });

    bool isFirstSelectedNode = true;
    for (auto& [node, text] : textInRangedSelection) {
        if (std::exchange(isFirstSelectedNode, false)) {
            if (auto entry = result.find(node.ptr()); entry != result.end() && entry->key == node.ptr()) {
                entry->value = std::make_pair(
                    makeString(entry->value.first, text),
                    CharacterRange { entry->value.first.length(), text.length() }
                );
                continue;
            }
        }
        result.add(node.ptr(), TextAndSelectedRange { text, CharacterRange { 0, text.length() } });
    }

    bool isFirstNodeAfterSelection = true;
    for (auto& [node, text] : textAfterRangedSelection) {
        if (std::exchange(isFirstNodeAfterSelection, false)) {
            if (auto entry = result.find(node.ptr()); entry != result.end() && entry->key == node.ptr()) {
                entry->value.first = makeString(entry->value.first, text);
                continue;
            }
        }
        result.add(node.ptr(), TextAndSelectedRange { text, std::nullopt });
    }

    return result;
}

static inline bool canMerge(const Item& destinationItem, const Item& sourceItem)
{
    if (!destinationItem.children.isEmpty() || !sourceItem.children.isEmpty())
        return false;

    if (!std::holds_alternative<TextItemData>(destinationItem.data) || !std::holds_alternative<TextItemData>(sourceItem.data))
        return false;

    // Don't merge adjacent text runs if they represent two different editable roots.
    auto& destination = std::get<TextItemData>(destinationItem.data);
    auto& source = std::get<TextItemData>(sourceItem.data);
    return !destination.editable && !source.editable;
}

static inline void merge(Item& destinationItem, Item&& sourceItem)
{
    ASSERT(canMerge(destinationItem, sourceItem));

    auto& destination = std::get<TextItemData>(destinationItem.data);
    auto& source = std::get<TextItemData>(sourceItem.data);

    destinationItem.rectInRootView.unite(sourceItem.rectInRootView);

    auto originalContentLength = destination.content.length();
    destination.content = makeString(destination.content, WTFMove(source.content));

    if (source.selectedRange) {
        CharacterRange newSelectedRange;
        if (destination.selectedRange)
            newSelectedRange = { destination.selectedRange->location, destination.selectedRange->length + source.selectedRange->length };
        else
            newSelectedRange = { originalContentLength + source.selectedRange->location, source.selectedRange->length };
        destination.selectedRange = WTFMove(newSelectedRange);
    }

    if (!source.links.isEmpty()) {
        for (auto& [url, range] : source.links)
            range.location += originalContentLength;
        destination.links.appendVector(WTFMove(source.links));
    }
}

static inline FloatRect rootViewBounds(Node& node)
{
    RefPtr view = node.document().view();
    if (!view) [[unlikely]]
        return { };

    CheckedPtr renderer = node.renderer();
    if (!renderer)
        return { };

    IntRect absoluteRect;
    if (CheckedPtr renderElement = dynamicDowncast<RenderElement>(*renderer); renderElement && renderElement->firstChild())
        absoluteRect = renderer->pixelSnappedAbsoluteClippedOverflowRect();

    if (absoluteRect.isEmpty())
        absoluteRect = renderer->absoluteBoundingBoxRect();

    return view->contentsToRootView(absoluteRect);
}

static inline String labelText(HTMLElement& element)
{
    auto labels = element.labels();
    if (!labels)
        return { };

    RefPtr<Element> firstRenderedLabel;
    for (unsigned index = 0; index < labels->length(); ++index) {
        if (RefPtr label = dynamicDowncast<Element>(labels->item(index)); label && label->renderer())
            firstRenderedLabel = WTFMove(label);
    }

    if (firstRenderedLabel)
        return firstRenderedLabel->textContent();

    return { };
}

enum class SkipExtraction : bool {
    Self,
    SelfAndSubtree
};

static bool shouldTreatAsPasswordField(const Element* element)
{
    RefPtr input = dynamicDowncast<HTMLInputElement>(element);
    return input && input->hasEverBeenPasswordField();
}

enum class FallbackPolicy : bool { Skip, Extract };

static inline Variant<SkipExtraction, ItemData, URL, Editable> extractItemData(Node& node, FallbackPolicy policy, TraversalContext& context)
{
    CheckedPtr renderer = node.renderer();

    RefPtr element = dynamicDowncast<Element>(node);
    if (element && element->hasDisplayContents())
        return { SkipExtraction::Self };

    if (!renderer)
        return { SkipExtraction::SelfAndSubtree };

    if (context.skipNearlyTransparentContent && renderer->style().opacity() < minOpacityToConsiderVisible)
        return { SkipExtraction::SelfAndSubtree };

    if (renderer->style().usedVisibility() == Visibility::Hidden)
        return { SkipExtraction::Self };

    if (RefPtr textNode = dynamicDowncast<Text>(node)) {
        if (shouldTreatAsPasswordField(textNode->shadowHost()))
            return { SkipExtraction::Self };

        if (auto iterator = context.visibleText.find(textNode); iterator != context.visibleText.end()) {
            auto& [textContent, selectedRange] = iterator->value;
            return { TextItemData { { }, selectedRange, textContent, { } } };
        }
        return { SkipExtraction::Self };
    }

    if (!element)
        return { SkipExtraction::Self };

    if (element->isLink()) {
        if (auto href = element->attributeWithoutSynchronization(HTMLNames::hrefAttr); !href.isEmpty()) {
            if (auto url = element->document().completeURL(href); !url.isEmpty()) {
                if (context.mergeParagraphs)
                    return { WTFMove(url) };

                if (RefPtr anchor = dynamicDowncast<HTMLAnchorElement>(*element))
                    return { LinkItemData { anchor->target(), WTFMove(url) } };

                return { LinkItemData { { }, WTFMove(url) } };
            }
        }
    }

    if (context.onlyCollectTextAndLinksCount) {
        // FIXME: This isn't quite right in the case where a richly contenteditable element
        // contains more nested editable containers underneath it (for instance, a textarea
        // element inside of a Mail compose draft).
        return { SkipExtraction::Self };
    }

    if (!element->isInUserAgentShadowTree() && element->isRootEditableElement()) {
        if (context.mergeParagraphs)
            return { Editable { } };

        return { ContentEditableData {
            .isPlainTextOnly = !element->hasRichlyEditableStyle(),
            .isFocused = element->document().activeElement() == element,
        } };
    }

    if (RefPtr image = dynamicDowncast<HTMLImageElement>(element))
        return { ImageItemData { image->getURLAttribute(HTMLNames::srcAttr).lastPathComponent().toString(), image->altText() } };

    if (RefPtr control = dynamicDowncast<HTMLTextFormControlElement>(element)) {
        RefPtr input = dynamicDowncast<HTMLInputElement>(control);
        Editable editable {
            labelText(*control),
            input ? input->placeholder() : nullString(),
            shouldTreatAsPasswordField(element.get()),
            element->document().activeElement() == control
        };

        if (context.mergeParagraphs && control->isTextField())
            return { WTFMove(editable) };

        if (!context.mergeParagraphs) {
            RefPtr input = dynamicDowncast<HTMLInputElement>(*control);
            return { TextFormControlData {
                .editable = WTFMove(editable),
                .controlType = control->type(),
                .autocomplete = control->autocomplete(),
                .isReadonly = input && input->isReadOnly(),
                .isDisabled = control->isDisabled(),
                .isChecked = input && input->checked(),
            } };
        }
    }

    if (RefPtr select = dynamicDowncast<HTMLSelectElement>(element)) {
        SelectData selectData;
        for (WeakPtr weakItem : select->listItems()) {
            RefPtr item = weakItem.get();
            if (!item)
                continue;

            if (RefPtr option = dynamicDowncast<HTMLOptionElement>(*item)) {
                if (!option->selected())
                    continue;

                if (auto optionValue = option->value(); !optionValue.isEmpty())
                    selectData.selectedValues.append(WTFMove(optionValue));
            }
        }
        selectData.isMultiple = select->multiple();
        return selectData;
    }

    if (RefPtr button = dynamicDowncast<HTMLButtonElement>(element))
        return { ItemData { ContainerType::Button } };

    if (RefPtr input = dynamicDowncast<HTMLInputElement>(element)) {
        if (input->isTextButton())
            return { ItemData { ContainerType::Button } };
    }

    if (CheckedPtr box = dynamicDowncast<RenderBox>(node.renderer()); box && box->canBeScrolledAndHasScrollableArea()) {
        if (CheckedPtr layer = box->layer(); layer && layer->scrollableArea())
            return { ScrollableItemData { layer->scrollableArea()->totalContentsSize() } };
    }

    if (element->hasTagName(HTMLNames::olTag) || element->hasTagName(HTMLNames::ulTag))
        return { ItemData { ContainerType::List } };

    if (element->hasTagName(HTMLNames::liTag))
        return { ItemData { ContainerType::ListItem } };

    if (element->hasTagName(HTMLNames::blockquoteTag))
        return { ItemData { ContainerType::BlockQuote } };

    if (element->hasTagName(HTMLNames::articleTag))
        return { ItemData { ContainerType::Article } };

    if (element->hasTagName(HTMLNames::sectionTag))
        return { ItemData { ContainerType::Section } };

    if (element->hasTagName(HTMLNames::navTag))
        return { ItemData { ContainerType::Nav } };

    if (CheckedPtr renderElement = dynamicDowncast<RenderBox>(*renderer); renderElement && renderElement->style().hasViewportConstrainedPosition())
        return { ItemData { ContainerType::ViewportConstrained } };

    if (policy == FallbackPolicy::Extract) {
        // As a last resort, if the element doesn't fall into any of the other buckets above,
        // we still need to extract it to preserve data about event listeners and accessibility
        // attributes.
        return { ItemData { ContainerType::Generic } };
    }

    return { SkipExtraction::Self };
}

static inline bool shouldIncludeNodeIdentifier(OptionSet<EventListenerCategory> eventListeners, AccessibilityRole role, const ItemData& data)
{
    return WTF::switchOn(data,
        [eventListeners, role](ContainerType type) {
            switch (type) {
            case ContainerType::Root:
            case ContainerType::Article:
                return false;
            case ContainerType::ViewportConstrained:
            case ContainerType::List:
            case ContainerType::ListItem:
            case ContainerType::BlockQuote:
            case ContainerType::Section:
            case ContainerType::Nav:
            case ContainerType::Generic:
                return eventListeners || AccessibilityObject::isARIAControl(role);
            case ContainerType::Button:
                return true;
            }
            ASSERT_NOT_REACHED();
            return false;
        },
        [](const TextItemData&) {
            return false;
        },
        [](auto&) {
            return true;
        });
}

static inline void extractRecursive(Node& node, Item& parentItem, TraversalContext& context)
{
    std::optional<Item> item;
    std::optional<Editable> editable;
    std::optional<URL> linkURL;
    bool shouldSkipSubtree = false;

    OptionSet<EventListenerCategory> eventListeners;
    node.enumerateEventListenerTypes([&](auto& type, unsigned) {
        auto typeInfo = eventNames().typeInfoForEvent(type);
        if (typeInfo.isInCategory(EventCategory::Wheel))
            eventListeners.add(EventListenerCategory::Wheel);
        else if (typeInfo.isInCategory(EventCategory::MouseClickRelated))
            eventListeners.add(EventListenerCategory::Click);
        else if (typeInfo.isInCategory(EventCategory::MouseMoveRelated))
            eventListeners.add(EventListenerCategory::Hover);
        else if (typeInfo.isInCategory(EventCategory::TouchRelated))
            eventListeners.add(EventListenerCategory::Touch);

        switch (typeInfo.type()) {
        case EventType::keydown:
        case EventType::keypress:
        case EventType::keyup:
            eventListeners.add(EventListenerCategory::Keyboard);
            break;

        default:
            break;
        }
    });

    HashMap<String, String> ariaAttributes;
    String role;
    if (RefPtr element = dynamicDowncast<Element>(node)) {
        auto attributesToExtract = std::array {
            HTMLNames::aria_labelAttr.get(),
            HTMLNames::aria_expandedAttr.get(),
            HTMLNames::aria_modalAttr.get(),
            HTMLNames::aria_disabledAttr.get(),
            HTMLNames::aria_checkedAttr.get(),
            HTMLNames::aria_selectedAttr.get(),
            HTMLNames::aria_readonlyAttr.get(),
            HTMLNames::aria_haspopupAttr.get(),
            HTMLNames::aria_descriptionAttr.get(),
            HTMLNames::aria_multilineAttr.get(),
            HTMLNames::aria_valueminAttr.get(),
            HTMLNames::aria_valuemaxAttr.get(),
            HTMLNames::aria_valuenowAttr.get(),
            HTMLNames::aria_valuetextAttr.get(),
        };
        for (auto& attributeName : attributesToExtract) {
            if (auto value = element->attributeWithoutSynchronization(attributeName); !value.isEmpty())
                ariaAttributes.set(attributeName.toString(), WTFMove(value));
        }
        role = element->attributeWithoutSynchronization(HTMLNames::roleAttr);
    }

    auto policy = eventListeners || !ariaAttributes.isEmpty() || !role.isEmpty() ? FallbackPolicy::Extract : FallbackPolicy::Skip;

    WTF::switchOn(extractItemData(node, policy, context),
        [&](SkipExtraction skipExtraction) {
            switch (skipExtraction) {
            case SkipExtraction::Self:
                return;
            case SkipExtraction::SelfAndSubtree:
                shouldSkipSubtree = true;
                return;
            }
        },
        [&](URL&& result) {
            ASSERT(context.mergeParagraphs);
            linkURL = WTFMove(result);
        },
        [&](Editable&& result) {
            ASSERT(context.mergeParagraphs);
            editable = WTFMove(result);
        },
        [&](ItemData&& result) {
            auto bounds = rootViewBounds(node);
            if (!context.shouldIncludeNodeWithRect(bounds))
                return;

            std::optional<NodeIdentifier> nodeIdentifier;
            if (context.canIncludeIdentifiers && shouldIncludeNodeIdentifier(eventListeners, AccessibilityObject::ariaRoleToWebCoreRole(role), result))
                nodeIdentifier = node.nodeIdentifier();

            item = { {
                WTFMove(result),
                WTFMove(bounds),
                { },
                WTFMove(nodeIdentifier),
                eventListeners,
                WTFMove(ariaAttributes),
                WTFMove(role),
            } };
        });

    if (shouldSkipSubtree)
        return;

    bool onlyCollectTextAndLinks = linkURL || editable;
    if (onlyCollectTextAndLinks) {
        if (auto bounds = rootViewBounds(node); context.shouldIncludeNodeWithRect(bounds)) {
            item = {
                TextItemData { { }, { }, emptyString(), { } },
                WTFMove(bounds),
                { },
                { },
                eventListeners,
                WTFMove(ariaAttributes),
                WTFMove(role),
            };
        }
        context.onlyCollectTextAndLinksCount++;
    }

    if (RefPtr container = dynamicDowncast<ContainerNode>(node)) {
        for (auto& child : composedTreeChildren(*container))
            extractRecursive(child, item ? *item : parentItem, context);
    }

    if (onlyCollectTextAndLinks) {
        if (item) {
            if (linkURL) {
                auto& text = std::get<TextItemData>(item->data);
                text.links.append({ WTFMove(*linkURL), CharacterRange { 0, text.content.length() } });
            }
            if (editable) {
                auto& text = std::get<TextItemData>(item->data);
                text.editable = WTFMove(editable);
            }
        }
        context.onlyCollectTextAndLinksCount--;
    }

    if (!item)
        return;

    if (context.mergeParagraphs) {
        if (parentItem.children.isEmpty()) {
            if (canMerge(parentItem, *item))
                return merge(parentItem, WTFMove(*item));
        } else if (auto& lastChild = parentItem.children.last(); canMerge(lastChild, *item))
            return merge(lastChild, WTFMove(*item));
    }

    parentItem.children.append(WTFMove(*item));
}

static void pruneRedundantItemsRecursive(Item& item)
{
    item.children.removeAllMatching([](auto& child) {
        if (!child.children.isEmpty() || !std::holds_alternative<TextItemData>(child.data))
            return false;

        auto& text = std::get<TextItemData>(child.data);
        return !text.editable && text.content.template containsOnly<isASCIIWhitespace>();
    });

    for (auto& child : item.children)
        pruneRedundantItemsRecursive(child);
}

Item extractItem(Request&& request, Page& page)
{
    Item root { ContainerType::Root, { }, { }, { }, { }, { }, { } };
    RefPtr mainFrame = dynamicDowncast<LocalFrame>(page.mainFrame());
    if (!mainFrame) {
        // FIXME: Propagate text extraction to RemoteFrames.
        return root;
    }

    RefPtr mainDocument = mainFrame->document();
    if (!mainDocument)
        return root;

    RefPtr bodyElement = mainDocument->body();
    if (!bodyElement)
        return root;

    mainDocument->updateLayoutIgnorePendingStylesheets();
    root.rectInRootView = rootViewBounds(*bodyElement);

    {
        TraversalContext context {
            .visibleText = collectText(*mainDocument),
            .rectInRootView = WTFMove(request.collectionRectInRootView),
            .onlyCollectTextAndLinksCount = 0,
            .mergeParagraphs = request.mergeParagraphs,
            .skipNearlyTransparentContent = request.skipNearlyTransparentContent,
            .canIncludeIdentifiers = request.canIncludeIdentifiers,
        };
        extractRecursive(*bodyElement, root, context);
    }

    pruneRedundantItemsRecursive(root);

    return root;
}

using Token = Variant<String, IntSize>;
struct TokenAndBlockOffset {
    Vector<Token> tokens;
    int offset { 0 };
};

static IntSize reducePrecision(FloatSize size)
{
    static constexpr auto resolution = 10;
    return {
        static_cast<int>(std::round(size.width() / resolution)) * resolution,
        static_cast<int>(std::round(size.height() / resolution)) * resolution
    };
}

static void extractRenderedTokens(Vector<TokenAndBlockOffset>& tokensAndOffsets, ContainerNode& node, FlowDirection direction)
{
    CheckedPtr renderer = node.renderer();
    if (!renderer)
        return;

    auto appendTokens = [&](Vector<Token>&& tokens, IntRect bounds) mutable {
        static constexpr auto minPixelDistanceForNearbyText = 5;
        if (tokens.isEmpty() || bounds.width() <= minPixelDistanceForNearbyText || bounds.height() <= minPixelDistanceForNearbyText)
            return;

        auto offset = [&] {
            switch (direction) {
            case FlowDirection::TopToBottom:
                return bounds.y();
            case FlowDirection::BottomToTop:
                return bounds.maxY();
            case FlowDirection::LeftToRight:
                return bounds.x();
            case FlowDirection::RightToLeft:
                return bounds.maxX();
            }
            ASSERT_NOT_REACHED();
            return 0;
        }();

        auto foundIndex = tokensAndOffsets.reverseFindIf([&](auto& item) {
            return std::abs(offset - item.offset) <= minPixelDistanceForNearbyText;
        });

        if (foundIndex == notFound) {
            tokensAndOffsets.append({ WTFMove(tokens), offset });
            return;
        }

        tokensAndOffsets[foundIndex].tokens.appendVector(WTFMove(tokens));
    };

    if (CheckedPtr frameRenderer = dynamicDowncast<RenderIFrame>(*renderer)) {
        if (RefPtr contentDocument = frameRenderer->iframeElement().contentDocument())
            extractRenderedTokens(tokensAndOffsets, *contentDocument, direction);
        return;
    }

    RefPtr frameView = renderer->view().frameView();
    auto appendReplacedContentOrBackgroundImage = [&](auto& renderer) {
        if (!renderer.style().hasBackgroundImage() && !is<RenderReplaced>(renderer))
            return;

        auto absoluteRect = renderer.absoluteBoundingBoxRect();
        auto roundedSize = reducePrecision(frameView->absoluteToDocumentRect(absoluteRect).size());
        appendTokens({ { roundedSize } }, frameView->contentsToRootView(absoluteRect));
    };

    appendReplacedContentOrBackgroundImage(*renderer);

    for (auto& descendant : descendantsOfType<RenderObject>(*renderer)) {
        if (descendant.style().usedVisibility() == Visibility::Hidden)
            continue;

        if (descendant.style().opacity() < minOpacityToConsiderVisible)
            continue;

        if (RefPtr node = descendant.node(); node && ImageOverlay::isInsideOverlay(*node))
            continue;

        if (CheckedPtr textRenderer = dynamicDowncast<RenderText>(descendant)) {
            if (textRenderer->hasRenderedText()) {
                Vector<Token> tokens;
                for (auto token : textRenderer->text().simplifyWhiteSpace(isASCIIWhitespace).split(' ')) {
                    auto candidate = token.removeCharacters([](char16_t character) {
                        return !u_isalpha(character) && !u_isdigit(character);
                    });
                    if (!candidate.isEmpty())
                        tokens.append({ WTFMove(candidate) });
                }
                appendTokens(WTFMove(tokens), frameView->contentsToRootView(descendant.absoluteBoundingBoxRect()));
            }
            continue;
        }

        if (CheckedPtr frameRenderer = dynamicDowncast<RenderIFrame>(descendant)) {
            if (RefPtr contentDocument = frameRenderer->iframeElement().contentDocument())
                extractRenderedTokens(tokensAndOffsets, *contentDocument, direction);
            continue;
        }

        appendReplacedContentOrBackgroundImage(downcast<RenderElement>(descendant));
    }
}

RenderedText extractRenderedText(Element& element)
{
    CheckedPtr renderer = element.renderer();
    if (!renderer)
        return { };

    RefPtr frameView = renderer->view().frameView();
    auto direction = renderer->writingMode().blockDirection();
    auto elementRectInDocument = frameView->absoluteToDocumentRect(renderer->absoluteBoundingBoxRect());

    Vector<TokenAndBlockOffset> allTokensAndOffsets;
    extractRenderedTokens(allTokensAndOffsets, element, direction);

    bool ascendingOrder = [&] {
        switch (direction) {
        case FlowDirection::TopToBottom:
        case FlowDirection::LeftToRight:
            return true;
        case FlowDirection::BottomToTop:
        case FlowDirection::RightToLeft:
            return false;
        }
        ASSERT_NOT_REACHED();
        return true;
    }();

    if (ascendingOrder)
        std::ranges::sort(allTokensAndOffsets, std::ranges::less { }, &TokenAndBlockOffset::offset);
    else
        std::ranges::sort(allTokensAndOffsets, std::ranges::greater { }, &TokenAndBlockOffset::offset);

    bool hasLargeReplacedDescendant = false;
    StringBuilder textWithReplacedContent;
    StringBuilder textWithoutReplacedContent;
    auto appendText = [](StringBuilder& builder, const String& string) {
        if (!builder.isEmpty())
            builder.append(' ');
        builder.append(string);
    };

    for (auto& [tokens, offset] : allTokensAndOffsets) {
        for (auto& token : tokens) {
            switchOn(token, [&](const String& text) {
                appendText(textWithReplacedContent, text);
                appendText(textWithoutReplacedContent, text);
            }, [&](const IntSize& size) {
                constexpr auto ratioToConsiderLengthAsLarge = 0.9;
                if (size.width() > ratioToConsiderLengthAsLarge * elementRectInDocument.width() && size.height() > ratioToConsiderLengthAsLarge * elementRectInDocument.height())
                    hasLargeReplacedDescendant = true;
                appendText(textWithReplacedContent, makeString('{', size.width(), ',', size.height(), '}'));
            });
        }
    }

    return { textWithReplacedContent.toString(), textWithoutReplacedContent.toString(), hasLargeReplacedDescendant };
}

static Vector<std::pair<String, FloatRect>> extractAllTextAndRectsRecursive(Document& document)
{
    RefPtr bodyElement = document.body();
    if (!bodyElement)
        return { };

    RefPtr view = document.view();
    if (!view)
        return { };

    ListHashSet<Ref<HTMLFrameOwnerElement>> frameOwners;
    Vector<std::pair<String, FloatRect>> result;
    auto fullRange = makeRangeSelectingNodeContents(*bodyElement);
    for (TextIterator iterator { fullRange, TextIteratorBehavior::EntersTextControls }; !iterator.atEnd(); iterator.advance()) {
        RefPtr node = iterator.node();
        if (!node)
            continue;

        if (RefPtr frameOwner = dynamicDowncast<HTMLFrameOwnerElement>(*node))
            frameOwners.add(frameOwner.releaseNonNull());

        auto trimmedText = iterator.text().trim(isASCIIWhitespace<char16_t>);
        if (trimmedText.isEmpty())
            continue;

        CheckedPtr renderer = node->renderer();
        if (!renderer)
            continue;

        IntRect absoluteBounds;
        auto textRange = iterator.range();
        if (!textRange.collapsed()) {
            auto textRects = RenderObject::absoluteTextRects(textRange, {
                RenderObject::BoundingRectBehavior::IgnoreTinyRects,
                RenderObject::BoundingRectBehavior::IgnoreEmptyTextSelections,
                RenderObject::BoundingRectBehavior::UseSelectionHeight,
            });

            for (auto textRect : textRects)
                absoluteBounds.unite(textRect);
        }

        if (absoluteBounds.isEmpty())
            absoluteBounds = renderer->absoluteBoundingBoxRect();

        result.append({ trimmedText.toString(), view->contentsToRootView(absoluteBounds) });
    }

    for (auto& frameOwner : frameOwners) {
        RefPtr contentDocument = frameOwner->contentDocument();
        if (!contentDocument)
            continue;

        result.appendVector(extractAllTextAndRectsRecursive(*contentDocument));
    }

    return result;
}

Vector<std::pair<String, FloatRect>> extractAllTextAndRects(Page& page)
{
    RefPtr mainFrame = dynamicDowncast<LocalFrame>(page.mainFrame());
    if (!mainFrame)
        return { };

    RefPtr document = mainFrame->document();
    if (!document)
        return { };

    return extractAllTextAndRectsRecursive(*document);
}

static std::optional<SimpleRange> searchForText(Node& node, const String& searchText)
{
    auto searchRange = makeRangeSelectingNodeContents(node);
    auto foundRange = findPlainText(searchRange, searchText, {
        FindOption::DoNotRevealSelection,
        FindOption::DoNotSetSelection,
    });

    if (foundRange.collapsed())
        return { };

    return { WTFMove(foundRange) };
}

static void dispatchSimulatedClick(Page& page, IntPoint location, CompletionHandler<void(bool)>&& completion)
{
    RefPtr frame = page.localMainFrame();
    if (!frame)
        return completion(false);

    frame->eventHandler().handleMouseMoveEvent({
        location, location, MouseButton::Left, PlatformEvent::Type::MouseMoved, 0, { }, WallTime::now(), ForceAtClick, SyntheticClickType::NoTap
    });

    frame->eventHandler().handleMousePressEvent({
        location, location, MouseButton::Left, PlatformEvent::Type::MousePressed, 1, { }, WallTime::now(), ForceAtClick, SyntheticClickType::NoTap
    });

    frame->eventHandler().handleMouseReleaseEvent({
        location, location, MouseButton::Left, PlatformEvent::Type::MouseReleased, 1, { }, WallTime::now(), ForceAtClick, SyntheticClickType::NoTap
    });

    completion(true);
}

static void dispatchSimulatedClick(Node& targetNode, const String& searchText, CompletionHandler<void(bool)>&& completion)
{
    RefPtr element = dynamicDowncast<Element>(targetNode);
    if (!element)
        return completion(false);

    if (!element->isConnected())
        return completion(false);

    {
        CheckedPtr renderer = element->renderer();
        if (!renderer)
            return completion(false);

        if (renderer->style().usedVisibility() != Visibility::Visible)
            return completion(false);
    }

    Ref document = element->document();
    RefPtr view = document->view();
    if (!view)
        return completion(false);

    RefPtr page = document->page();
    if (!page)
        return completion(false);

    static constexpr OptionSet defaultHitTestOptions {
        HitTestRequest::Type::ReadOnly,
        HitTestRequest::Type::DisallowUserAgentShadowContent,
    };

    std::optional<FloatRect> targetRectInRootView;
    if (!searchText.isEmpty()) {
        auto foundRange = searchForText(*element, searchText);
        if (!foundRange) {
            // Err on the side of failing, if the text has changed since the interaction was triggered.
            return completion(false);
        }

        if (auto absoluteQuads = RenderObject::absoluteTextQuads(*foundRange); !absoluteQuads.isEmpty()) {
            // If the text match wraps across multiple lines, arbitrarily click over the first rect to avoid
            // missing the text node altogether.
            targetRectInRootView = view->contentsToRootView(absoluteQuads.first().boundingBox());
        }
    }

    if (!targetRectInRootView)
        targetRectInRootView = rootViewBounds(*element);

    auto centerInRootView = roundedIntPoint(targetRectInRootView->center());
    auto centerInContents = view->rootViewToContents(centerInRootView);
    HitTestResult result { centerInContents };
    if (document->hitTest(defaultHitTestOptions, result)) {
        if (RefPtr target = result.innerNode(); target && (target == element || target->isShadowIncludingDescendantOf(*element))) {
            // Dispatch mouse events over the center of the element, if possible.
            return dispatchSimulatedClick(*page, centerInRootView, WTFMove(completion));
        }
    }

    UserGestureIndicator indicator { IsProcessingUserGesture::Yes, element->protectedDocument().ptr() };

    // Fall back to dispatching a programmatic click.
    completion(element->dispatchSimulatedClick(nullptr, SendMouseUpDownEvents));
}

static void dispatchSimulatedClick(NodeIdentifier identifier, const String& searchText, CompletionHandler<void(bool)>&& completion)
{
    RefPtr foundNode = Node::fromIdentifier(identifier);
    if (!foundNode)
        return completion(false);

    dispatchSimulatedClick(*foundNode, searchText, WTFMove(completion));
}

static bool selectOptionByValue(NodeIdentifier identifier, const String& optionText)
{
    RefPtr foundNode = Node::fromIdentifier(identifier);
    if (!foundNode)
        return false;

    if (RefPtr select = dynamicDowncast<HTMLSelectElement>(*foundNode)) {
        if (optionText.isEmpty())
            return false;

        select->setValue(optionText);
        return select->selectedIndex() != -1;
    }

    return false;
}

static void selectText(NodeIdentifier identifier, const String& searchText, CompletionHandler<void(bool)>&& completion)
{
    RefPtr foundNode = Node::fromIdentifier(identifier);
    if (!foundNode)
        return completion(false);

    if (RefPtr control = dynamicDowncast<HTMLTextFormControlElement>(*foundNode)) {
        // FIXME: This should probably honor `searchText`.
        control->select();
        return completion(true);
    }

    std::optional<SimpleRange> targetRange;
    if (searchText.isEmpty())
        targetRange = makeRangeSelectingNodeContents(*foundNode);
    else
        targetRange = searchForText(*foundNode, searchText);

    if (!targetRange)
        return completion(false);

    completion(foundNode->protectedDocument()->selection().setSelectedRange(*targetRange, Affinity::Downstream, FrameSelection::ShouldCloseTyping::Yes, UserTriggered::Yes));
}

static void focusAndInsertText(NodeIdentifier identifier, String&& text, bool replaceAll, CompletionHandler<void(bool)>&& completion)
{
    RefPtr foundNode = Node::fromIdentifier(identifier);
    if (!foundNode)
        return completion(false);

    RefPtr<Element> elementToFocus;
    if (RefPtr element = dynamicDowncast<Element>(*foundNode); element && element->isTextField())
        elementToFocus = element;
    else if (RefPtr host = foundNode->shadowHost(); host && host->isTextField()) {
        if (RefPtr formControl = dynamicDowncast<HTMLTextFormControlElement>(host.get()))
            elementToFocus = WTFMove(formControl);
    }

    if (!elementToFocus)
        elementToFocus = foundNode->isRootEditableElement() ? dynamicDowncast<Element>(*foundNode) : foundNode->rootEditableElement();

    if (!elementToFocus)
        return completion(false);

    Ref document = elementToFocus->document();
    RefPtr frame = document->frame();
    if (!frame)
        return completion(false);

    // First, attempt to dispatch a click over the editable area (and fall back to programmatically setting focus).
    dispatchSimulatedClick(*elementToFocus, { }, [document = document.copyRef(), elementToFocus, frame, replaceAll, text = WTFMove(text), completion = WTFMove(completion)](bool clicked) mutable {
        if (!clicked || elementToFocus != document->activeElement())
            elementToFocus->focus();

        if (replaceAll) {
            if (elementToFocus->isRootEditableElement())
                document->selection().setSelectedRange(makeRangeSelectingNodeContents(*elementToFocus), Affinity::Downstream, FrameSelection::ShouldCloseTyping::Yes, UserTriggered::Yes);
            else
                document->selection().selectAll();
        }

        UserTypingGestureIndicator indicator { *frame };

        document->protectedEditor()->pasteAsPlainText(text, false);
        completion(true);
    });
}

void handleInteraction(Interaction&& interaction, Page& page, CompletionHandler<void(bool)>&& completion)
{
    switch (interaction.action) {
    case Action::Click: {
        if (auto location = interaction.locationInRootView)
            return dispatchSimulatedClick(page, roundedIntPoint(*location), WTFMove(completion));

        if (auto identifier = interaction.nodeIdentifier)
            return dispatchSimulatedClick(*identifier, WTFMove(interaction.text), WTFMove(completion));

        return completion(false);
    }
    case Action::SelectMenuItem: {
        if (auto identifier = interaction.nodeIdentifier) {
            if (selectOptionByValue(*identifier, interaction.text))
                return completion(true);

            return dispatchSimulatedClick(*identifier, interaction.text, WTFMove(completion));
        }

        return completion(false);
    }
    case Action::SelectText: {
        if (auto identifier = interaction.nodeIdentifier) {
            if (selectOptionByValue(*identifier, interaction.text))
                return completion(true);

            return selectText(*identifier, WTFMove(interaction.text), WTFMove(completion));
        }

        return completion(false);
    }
    case Action::TextInput: {
        if (auto identifier = interaction.nodeIdentifier)
            return focusAndInsertText(*identifier, WTFMove(interaction.text), interaction.replaceAll, WTFMove(completion));

        return completion(false);
    }
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    completion(false);
}

} // namespace TextExtraction
} // namespace WebCore
