/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
#include "AccessibilityUIElement.h"

#include "DumpRenderTree.h"
#include "JSBasics.h"
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>

#if HAVE(ACCESSIBILITY_FRAMEWORK)
#include <Accessibility/Accessibility.h>
#endif

AccessibilityUIElement::AccessibilityUIElement(void*)
{
}

static JSRetainPtr<JSStringRef> descriptionOfElements(Vector<AccessibilityUIElement>& elementVector)
{
}

template<typename T> static JSObjectRef convertVectorToObjectArray(JSContextRef context, Vector<T> const& elements)
{
    auto array = JSObjectMakeArray(context, 0, nullptr, nullptr);
    unsigned size = elements.size();
    for (unsigned i = 0; i < size; ++i)
        JSObjectSetPropertyAtIndex(context, array, i, JSObjectMake(context, elements[i].getJSClass(), new T(elements[i])), nullptr);
    return array;
}

#if PLATFORM(MAC)
static NSDictionary *searchTextParameterizedAttributeForCriteria(JSContextRef context, JSValueRef searchStrings, JSStringRef startFrom, JSStringRef direction)
{
    NSMutableDictionary *parameterizedAttribute = [NSMutableDictionary dictionary];

    if (searchStrings) {
        NSMutableArray *searchStringsParameter = [NSMutableArray array];
        if (JSValueIsString(context, searchStrings)) {
            auto searchStringsString = adopt(JSValueToStringCopy(context, searchStrings, nullptr));
            if (searchStringsString)
                [searchStringsParameter addObject:[NSString stringWithJSStringRef:searchStringsString.get()]];
        } else if (JSValueIsObject(context, searchStrings)) {
            auto searchStringsArray = (JSObjectRef)searchStrings;
            auto searchStringsArrayLength = WTR::arrayLength(context, searchStringsArray);
            for (unsigned i = 0; i < searchStringsArrayLength; ++i) {
                auto searchStringsString = adopt(JSValueToStringCopy(context, JSObjectGetPropertyAtIndex(context, searchStringsArray, i, nullptr), nullptr));
                if (searchStringsString)
                    [searchStringsParameter addObject:[NSString stringWithJSStringRef:searchStringsString.get()]];
            }
        }
        [parameterizedAttribute setObject:searchStringsParameter forKey:@"AXSearchTextSearchStrings"];
    }

    if (startFrom)
        [parameterizedAttribute setObject:[NSString stringWithJSStringRef:startFrom] forKey:@"AXSearchTextStartFrom"];

    if (direction)
        [parameterizedAttribute setObject:[NSString stringWithJSStringRef:direction] forKey:@"AXSearchTextDirection"];

    return parameterizedAttribute;
}
#endif

bool AccessibilityUIElement::isEqual(AccessibilityUIElement* otherElement)
{
    if (!otherElement)
        return false;
    return platformUIElement() == otherElement->platformUIElement();
}

void AccessibilityUIElement::getLinkedUIElements(Vector<AccessibilityUIElement>& elementVector)
{
}

void AccessibilityUIElement::getDocumentLinks(Vector<AccessibilityUIElement>& elementVector)
{
}

void AccessibilityUIElement::getChildren(Vector<AccessibilityUIElement>& elementVector)
{
}

void AccessibilityUIElement::getChildrenWithRange(Vector<AccessibilityUIElement>& elementVector, unsigned location, unsigned length)
{
}

int AccessibilityUIElement::childrenCount()
{
    Vector<AccessibilityUIElement> children;
    getChildren(children);
    
    return children.size();
}

AccessibilityUIElement AccessibilityUIElement::elementAtPoint(int x, int y)
{
}

unsigned AccessibilityUIElement::indexOfChild(AccessibilityUIElement* element)
{
	return 0;
}

AccessibilityUIElement AccessibilityUIElement::getChildAtIndex(unsigned index)
{
    Vector<AccessibilityUIElement> children;
    getChildrenWithRange(children, index, 1);

    if (children.size() == 1)
        return children[0];
    return nullptr;
}

AccessibilityUIElement AccessibilityUIElement::linkedUIElementAtIndex(unsigned index)
{
    return nullptr;
}

AccessibilityUIElement AccessibilityUIElement::ariaOwnsElementAtIndex(unsigned index)
{
    return nullptr;
}

AccessibilityUIElement AccessibilityUIElement::ariaFlowToElementAtIndex(unsigned index)
{
    return nullptr;
}

AccessibilityUIElement AccessibilityUIElement::ariaControlsElementAtIndex(unsigned index)
{
    // Per spec, aria-controls is exposed via AXLinkedUIElements on the Mac.
    // Note that a few other things are exposed via AXLinkedUIElements (aria-flowto), so this function
    // may provide unexpected results for tests that use a combination of these attributes.
    return linkedUIElementAtIndex(index);
}

AccessibilityUIElement AccessibilityUIElement::disclosedRowAtIndex(unsigned index)
{
    return nullptr;
}

AccessibilityUIElement AccessibilityUIElement::selectedChildAtIndex(unsigned index) const
{
    return nullptr;
}

unsigned AccessibilityUIElement::selectedChildrenCount() const
{
    return 0;
}

AccessibilityUIElement AccessibilityUIElement::selectedRowAtIndex(unsigned index)
{
    return nullptr;
}

AccessibilityUIElement AccessibilityUIElement::rowAtIndex(unsigned index)
{
    return nullptr;
}

AccessibilityUIElement AccessibilityUIElement::titleUIElement()
{
    return nullptr;
}

AccessibilityUIElement AccessibilityUIElement::parentElement()
{
    return nullptr;
}

AccessibilityUIElement AccessibilityUIElement::disclosedByRow()
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfLinkedUIElements()
{
    Vector<AccessibilityUIElement> linkedElements;
    getLinkedUIElements(linkedElements);
    return descriptionOfElements(linkedElements);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfDocumentLinks()
{
    Vector<AccessibilityUIElement> linkElements;
    getDocumentLinks(linkElements);
    return descriptionOfElements(linkElements);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfChildren()
{
    Vector<AccessibilityUIElement> children;
    getChildren(children);
    return descriptionOfElements(children);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::allAttributes()
{
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringAttributeValue(JSStringRef attribute)
{
}

#if 0
JSRetainPtr<JSStringRef> AccessibilityUIElement::stringAttributeValue(NSString *attribute) const
{
    return nullptr;
}
#endif

JSValueRef AccessibilityUIElement::selectedCells(JSContextRef context) const
{
    return nullptr;
}


void AccessibilityUIElement::rowHeaders(Vector<AccessibilityUIElement>& elements) const
{
}

void AccessibilityUIElement::columnHeaders(Vector<AccessibilityUIElement>& elements) const
{
}

double AccessibilityUIElement::numberAttributeValue(JSStringRef attribute)
{
	return 0;
}

bool AccessibilityUIElement::boolAttributeValue(JSStringRef attribute)
{
    return false;
}

bool AccessibilityUIElement::isAttributeSettable(JSStringRef attribute)
{
    return false;
}

bool AccessibilityUIElement::isAttributeSupported(JSStringRef attribute)
{
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::parameterizedAttributeNames()
{
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::role()
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::subrole()
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::roleDescription()
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::computedRoleString()
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::title()
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::description()
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::domIdentifier() const
{
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::liveRegionRelevant() const
{
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::liveRegionStatus() const
{
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::orientation() const
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringValue()
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::language()
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::helpText() const
{
    return nullptr;
}

double AccessibilityUIElement::pageX()
{
    return 0.0f;
}

double AccessibilityUIElement::pageY()
{
    return 0.0f;
}

double AccessibilityUIElement::x()
{
    return 0.0f;
}

double AccessibilityUIElement::y()
{
    return 0.0f;
}

double AccessibilityUIElement::width()
{
    return 0.0f;
}

double AccessibilityUIElement::height()
{
    return 0.0f;
}

double AccessibilityUIElement::clickPointX()
{
    return 0.0f;
}

double AccessibilityUIElement::clickPointY()
{
    return 0.0f;
}

double AccessibilityUIElement::intValue() const
{
    return 0;
}

double AccessibilityUIElement::minValue()
{
    return 0;
}

double AccessibilityUIElement::maxValue()
{
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::valueDescription()
{
    return nullptr;
}

int AccessibilityUIElement::insertionPointLineNumber()
{
    return -1;
}

bool AccessibilityUIElement::isPressActionSupported()
{
    return false;
}

bool AccessibilityUIElement::isIncrementActionSupported()
{
    return false;
}

bool AccessibilityUIElement::isDecrementActionSupported()
{
    return false;
}

bool AccessibilityUIElement::isAtomicLiveRegion() const
{
    return false;
}

bool AccessibilityUIElement::isBusy() const
{
    return false;
}

bool AccessibilityUIElement::isEnabled()
{
    return false;
}

bool AccessibilityUIElement::isRequired() const
{
    return false;
}

bool AccessibilityUIElement::isFocused() const
{
    return false;
}

bool AccessibilityUIElement::isSelected() const
{
    return false;
}

bool AccessibilityUIElement::isExpanded() const
{
    return false;
}

bool AccessibilityUIElement::isChecked() const
{
    // On the Mac, intValue()==1 if a a checkable control is checked.
    return intValue() == 1;
}

int AccessibilityUIElement::hierarchicalLevel() const
{
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::classList() const
{
    return nullptr;
}

bool AccessibilityUIElement::isGrabbed() const
{
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::ariaDropEffects() const
{
    return nullptr;
}

// parameterized attributes
int AccessibilityUIElement::lineForIndex(int index)
{
    return -1;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::boundsForRange(unsigned location, unsigned length)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringForRange(unsigned location, unsigned length)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributedStringForRange(unsigned location, unsigned length)
{
    return nullptr;
}

bool AccessibilityUIElement::attributedStringRangeIsMisspelled(unsigned location, unsigned length)
{
    return false;
}

unsigned AccessibilityUIElement::uiElementCountForSearchPredicate(JSContextRef context, AccessibilityUIElement *startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly)
{
    return 0;
}

AccessibilityUIElement AccessibilityUIElement::uiElementForSearchPredicate(JSContextRef context, AccessibilityUIElement *startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::selectTextWithCriteria(JSContextRef context, JSStringRef ambiguityResolution, JSValueRef searchStrings, JSStringRef replacementString, JSStringRef activity)
{
    return nullptr;
}

#if PLATFORM(MAC)
JSValueRef AccessibilityUIElement::searchTextWithCriteria(JSContextRef context, JSValueRef searchStrings, JSStringRef startFrom, JSStringRef direction)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSDictionary *parameterizedAttribute = searchTextParameterizedAttributeForCriteria(context, searchStrings, startFrom, direction);
    id result = [m_element accessibilityAttributeValue:@"AXSearchTextWithCriteria" forParameter:parameterizedAttribute];
    if ([result isKindOfClass:[NSArray class]])
        return convertVectorToObjectArray(context, makeVector<AccessibilityTextMarkerRange>(result));
    END_AX_OBJC_EXCEPTIONS
    return nullptr;
}
#endif

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfColumnHeaders()
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfRowHeaders()
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfColumns()
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfRows()
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfVisibleCells()
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfHeader()
{
    return nullptr;
}

int AccessibilityUIElement::rowCount()
{
    return 0;
}

int AccessibilityUIElement::columnCount()
{
    return 0;
}

int AccessibilityUIElement::indexInTable()
{
    return -1;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::rowIndexRange()
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::columnIndexRange()
{
    return nullptr;
}

AccessibilityUIElement AccessibilityUIElement::cellForColumnAndRow(unsigned col, unsigned row)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::selectedTextRange()
{
    return nullptr;
}

void AccessibilityUIElement::setSelectedTextRange(unsigned location, unsigned length)
{
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::textInputMarkedRange() const
{
    return nullptr;
}

void AccessibilityUIElement::dismiss()
{
}

void AccessibilityUIElement::increment()
{
}

void AccessibilityUIElement::decrement()
{
}

void AccessibilityUIElement::showMenu()
{
}

void AccessibilityUIElement::press()
{
}

void AccessibilityUIElement::clearSelectedChildren() const
{
    // FIXME: Implement this function.
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::accessibilityValue() const
{
    // FIXME: Implement this function.
    return WTR::createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::url()
{
    return nullptr;
}

bool AccessibilityUIElement::addNotificationListener(JSObjectRef functionCallback)
{
    return true;
}

void AccessibilityUIElement::removeNotificationListener()
{
}

bool AccessibilityUIElement::isFocusable() const
{
    return false;
}

bool AccessibilityUIElement::isSelectable() const
{
    return false;
}

bool AccessibilityUIElement::isMultiSelectable() const
{
    return false;
}

bool AccessibilityUIElement::isSelectedOptionActive() const
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isVisible() const
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isOffScreen() const
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isCollapsed() const
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isIndeterminate() const
{
    return false;
}

bool AccessibilityUIElement::isIgnored() const
{
    return false;
}

bool AccessibilityUIElement::isSingleLine() const
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isMultiLine() const
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::hasPopup() const
{
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::popupValue() const
{
}

bool AccessibilityUIElement::isInDescriptionListDetail() const
{
    return false;
}

bool AccessibilityUIElement::isInDescriptionListTerm() const
{
    return false;
}

bool AccessibilityUIElement::isInCell() const
{
    return false;
}

void AccessibilityUIElement::takeFocus()
{
}

void AccessibilityUIElement::takeSelection()
{
    // FIXME: implement
}

void AccessibilityUIElement::addSelection()
{
    // FIXME: implement
}

void AccessibilityUIElement::removeSelection()
{
    // FIXME: implement
}

#if SUPPORTS_AX_TEXTMARKERS && PLATFORM(MAC)

// Text markers
AccessibilityTextMarkerRange AccessibilityUIElement::lineTextMarkerRangeForTextMarker(AccessibilityTextMarker* textMarker)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarkerRange = [m_element accessibilityAttributeValue:@"AXLineTextMarkerRangeForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarkerRange(textMarkerRange);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityTextMarkerRange AccessibilityUIElement::misspellingTextMarkerRange(AccessibilityTextMarkerRange* start, bool forward)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSMutableDictionary *parameters = [NSMutableDictionary dictionary];
    [parameters setObject:start->platformTextMarkerRange() forKey:@"AXStartTextMarkerRange"];
    [parameters setObject:[NSNumber numberWithBool:forward] forKey:@"AXSearchTextDirection"];
    id textMarkerRange = [m_element accessibilityAttributeValue:@"AXMisspellingTextMarkerRange" forParameter:parameters];
    return AccessibilityTextMarkerRange(textMarkerRange);
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

AccessibilityTextMarkerRange AccessibilityUIElement::textMarkerRangeForElement(AccessibilityUIElement* element)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarkerRange = [m_element accessibilityAttributeValue:@"AXTextMarkerRangeForUIElement" forParameter:element->platformUIElement()];
    return AccessibilityTextMarkerRange(textMarkerRange);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityTextMarkerRange AccessibilityUIElement::selectedTextMarkerRange()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarkerRange = [m_element accessibilityAttributeValue:NSAccessibilitySelectedTextMarkerRangeAttribute];
    return AccessibilityTextMarkerRange(textMarkerRange);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

void AccessibilityUIElement::resetSelectedTextMarkerRange()
{
    id start = [m_element accessibilityAttributeValue:@"AXStartTextMarker"];
    if (!start)
        return;
    
    NSArray* textMarkers = @[start, start];
    id textMarkerRange = [m_element accessibilityAttributeValue:@"AXTextMarkerRangeForUnorderedTextMarkers" forParameter:textMarkers];
    if (!textMarkerRange)
        return;
    
    BEGIN_AX_OBJC_EXCEPTIONS
    [m_element _accessibilitySetValue:textMarkerRange forAttribute:NSAccessibilitySelectedTextMarkerRangeAttribute];
    END_AX_OBJC_EXCEPTIONS
}

bool AccessibilityUIElement::replaceTextInRange(JSStringRef string, int location, int length)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    return [m_element accessibilityReplaceRange:NSMakeRange(location, length) withText:[NSString stringWithJSStringRef:string]];
    END_AX_OBJC_EXCEPTIONS
    return false;
}

bool AccessibilityUIElement::insertText(JSStringRef text)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    return [m_element accessibilityInsertText:[NSString stringWithJSStringRef:text]];
    END_AX_OBJC_EXCEPTIONS
    return false;
}

AccessibilityTextMarkerRange AccessibilityUIElement::textInputMarkedTextMarkerRange() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarkerRange = [m_element accessibilityAttributeValue:NSAccessibilityTextInputMarkedTextMarkerRangeAttribute];
    return AccessibilityTextMarkerRange(textMarkerRange);
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

int AccessibilityUIElement::textMarkerRangeLength(AccessibilityTextMarkerRange* range)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSNumber* lengthValue = [m_element accessibilityAttributeValue:@"AXLengthForTextMarkerRange" forParameter:range->platformTextMarkerRange()];
    return [lengthValue intValue];
    END_AX_OBJC_EXCEPTIONS
    
    return 0;
}

bool AccessibilityUIElement::attributedStringForTextMarkerRangeContainsAttribute(JSStringRef attribute, AccessibilityTextMarkerRange* range)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSAttributedString* string = [m_element accessibilityAttributeValue:@"AXAttributedStringForTextMarkerRange" forParameter:range->platformTextMarkerRange()];
    if (![string isKindOfClass:[NSAttributedString class]])
        return false;
    
    NSDictionary* attrs = [string attributesAtIndex:0 effectiveRange:nil];
    if ([attrs objectForKey:[NSString stringWithJSStringRef:attribute]])
        return true;    
    END_AX_OBJC_EXCEPTIONS
    
    return false;
}

int AccessibilityUIElement::indexForTextMarker(AccessibilityTextMarker* marker)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSNumber* indexNumber = [m_element accessibilityAttributeValue:@"AXIndexForTextMarker" forParameter:marker->platformTextMarker()];
    return [indexNumber intValue];
    END_AX_OBJC_EXCEPTIONS
    
    return -1;
}

AccessibilityTextMarker AccessibilityUIElement::textMarkerForIndex(int textIndex)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarker = [m_element accessibilityAttributeValue:@"AXTextMarkerForIndex" forParameter:[NSNumber numberWithInteger:textIndex]];
    return AccessibilityTextMarker(textMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

bool AccessibilityUIElement::isTextMarkerNull(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return true;

    BEGIN_AX_OBJC_EXCEPTIONS
    return [[m_element accessibilityAttributeValue:@"AXTextMarkerIsNull" forParameter:textMarker->platformTextMarker()] boolValue];
    END_AX_OBJC_EXCEPTIONS

    return true;
}

bool AccessibilityUIElement::isTextMarkerValid(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return false;

    BEGIN_AX_OBJC_EXCEPTIONS
    return [[m_element accessibilityAttributeValue:@"AXTextMarkerIsValid" forParameter:textMarker->platformTextMarker()] boolValue];
    END_AX_OBJC_EXCEPTIONS

    return false;
}

AccessibilityTextMarker AccessibilityUIElement::previousTextMarker(AccessibilityTextMarker* textMarker)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id previousMarker = [m_element accessibilityAttributeValue:@"AXPreviousTextMarkerForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarker(previousMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::nextTextMarker(AccessibilityTextMarker* textMarker)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id nextMarker = [m_element accessibilityAttributeValue:@"AXNextTextMarkerForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarker(nextMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringForTextMarkerRange(AccessibilityTextMarkerRange* markerRange)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textString = [m_element accessibilityAttributeValue:@"AXStringForTextMarkerRange" forParameter:markerRange->platformTextMarkerRange()];
    return [textString createJSStringRef];
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

static JSRetainPtr<JSStringRef> createJSStringRef(id string)
{
    if (!string)
        return nullptr;
    auto mutableString = adoptNS([[NSMutableString alloc] init]);
    id attributes = [string attributesAtIndex:0 effectiveRange:nil];
    id attributeEnumerationBlock = ^(NSDictionary<NSString *, id> *attrs, NSRange range, BOOL *stop) {
        BOOL misspelled = [[attrs objectForKey:NSAccessibilityMisspelledTextAttribute] boolValue];
        if (misspelled)
            misspelled = [[attrs objectForKey:NSAccessibilityMarkedMisspelledTextAttribute] boolValue];
        if (misspelled)
            [mutableString appendString:@"Misspelled, "];
        id font = [attributes objectForKey:(__bridge NSString *)kAXFontTextAttribute];
        if (font)
            [mutableString appendFormat:@"%@ - %@, ", (__bridge NSString *)kAXFontTextAttribute, font];
    };
    [string enumerateAttributesInRange:NSMakeRange(0, [string length]) options:(NSAttributedStringEnumerationOptions)0 usingBlock:attributeEnumerationBlock];
    [mutableString appendString:[string string]];
    return [mutableString createJSStringRef];
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributedStringForTextMarkerRange(AccessibilityTextMarkerRange* markerRange)
{
    id string = [m_element accessibilityAttributeValue:@"AXAttributedStringForTextMarkerRange" forParameter:markerRange->platformTextMarkerRange()];
    return createJSStringRef(string);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributedStringForTextMarkerRangeWithOptions(AccessibilityTextMarkerRange* markerRange, bool includeSpellCheck)
{
    id parameter = nil;
    if (includeSpellCheck)
        parameter = @{ @"AXSpellCheck": includeSpellCheck ? @YES : @NO, @"AXTextMarkerRange": markerRange->platformTextMarkerRange() };
    else
        parameter = markerRange->platformTextMarkerRange();
    id string = [m_element accessibilityAttributeValue:@"AXAttributedStringForTextMarkerRangeWithOptions" forParameter:parameter];
    return createJSStringRef(string);
}

AccessibilityTextMarkerRange AccessibilityUIElement::textMarkerRangeForMarkers(AccessibilityTextMarker* startMarker, AccessibilityTextMarker* endMarker)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSArray *textMarkers = @[startMarker->platformTextMarker(), endMarker->platformTextMarker()];
    id textMarkerRange = [m_element accessibilityAttributeValue:@"AXTextMarkerRangeForTextMarkers" forParameter:textMarkers];
    return AccessibilityTextMarkerRange(textMarkerRange);
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::startTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange* range)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarker = [m_element accessibilityAttributeValue:@"AXStartTextMarkerForTextMarkerRange" forParameter:range->platformTextMarkerRange()];
    return AccessibilityTextMarker(textMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::endTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange* range)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarker = [m_element accessibilityAttributeValue:@"AXEndTextMarkerForTextMarkerRange" forParameter:range->platformTextMarkerRange()];
    return AccessibilityTextMarker(textMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::endTextMarkerForBounds(int x, int y, int width, int height)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarker = [m_element accessibilityAttributeValue:NSAccessibilityEndTextMarkerForBoundsParameterizedAttribute forParameter:[NSValue valueWithRect:NSMakeRect(x, y, width, height)]];
    return AccessibilityTextMarker(textMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::startTextMarkerForBounds(int x, int y, int width, int height)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarker = [m_element accessibilityAttributeValue:NSAccessibilityStartTextMarkerForBoundsParameterizedAttribute forParameter:[NSValue valueWithRect:NSMakeRect(x, y, width, height)]];
    return AccessibilityTextMarker(textMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::textMarkerForPoint(int x, int y)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarker = [m_element accessibilityAttributeValue:@"AXTextMarkerForPosition" forParameter:[NSValue valueWithPoint:NSMakePoint(x, y)]];
    return AccessibilityTextMarker(textMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityUIElement AccessibilityUIElement::accessibilityElementForTextMarker(AccessibilityTextMarker* marker)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id uiElement = [m_element accessibilityAttributeValue:@"AXUIElementForTextMarker" forParameter:marker->platformTextMarker()];
    return AccessibilityUIElement(uiElement);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::startTextMarker()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarker = [m_element accessibilityAttributeValue:@"AXStartTextMarker"];
    return AccessibilityTextMarker(textMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::endTextMarker()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarker = [m_element accessibilityAttributeValue:@"AXEndTextMarker"];
    return AccessibilityTextMarker(textMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

bool AccessibilityUIElement::setSelectedTextMarkerRange(AccessibilityTextMarkerRange* markerRange)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    [m_element accessibilitySetValue:markerRange->platformTextMarkerRange() forAttribute:NSAccessibilitySelectedTextMarkerRangeAttribute];
    END_AX_OBJC_EXCEPTIONS
    
    return true;
}

AccessibilityTextMarkerRange AccessibilityUIElement::leftWordTextMarkerRangeForTextMarker(AccessibilityTextMarker* textMarker)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarkerRange = [m_element accessibilityAttributeValue:@"AXLeftWordTextMarkerRangeForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarkerRange(textMarkerRange);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityTextMarkerRange AccessibilityUIElement::rightWordTextMarkerRangeForTextMarker(AccessibilityTextMarker* textMarker)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarkerRange = [m_element accessibilityAttributeValue:@"AXRightWordTextMarkerRangeForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarkerRange(textMarkerRange);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::previousWordStartTextMarkerForTextMarker(AccessibilityTextMarker* textMarker)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id previousTextMarker = [m_element accessibilityAttributeValue:@"AXPreviousWordStartTextMarkerForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarker(previousTextMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::nextWordEndTextMarkerForTextMarker(AccessibilityTextMarker* textMarker)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id nextTextMarker = [m_element accessibilityAttributeValue:@"AXNextWordEndTextMarkerForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarker(nextTextMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityTextMarkerRange AccessibilityUIElement::paragraphTextMarkerRangeForTextMarker(AccessibilityTextMarker* textMarker)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarkerRange = [m_element accessibilityAttributeValue:@"AXParagraphTextMarkerRangeForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarkerRange(textMarkerRange);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::previousParagraphStartTextMarkerForTextMarker(AccessibilityTextMarker* textMarker)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id previousTextMarker = [m_element accessibilityAttributeValue:@"AXPreviousParagraphStartTextMarkerForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarker(previousTextMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::nextParagraphEndTextMarkerForTextMarker(AccessibilityTextMarker* textMarker)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id nextTextMarker = [m_element accessibilityAttributeValue:@"AXNextParagraphEndTextMarkerForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarker(nextTextMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityTextMarkerRange AccessibilityUIElement::sentenceTextMarkerRangeForTextMarker(AccessibilityTextMarker* textMarker)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarkerRange = [m_element accessibilityAttributeValue:@"AXSentenceTextMarkerRangeForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarkerRange(textMarkerRange);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::previousSentenceStartTextMarkerForTextMarker(AccessibilityTextMarker* textMarker)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id previousTextMarker = [m_element accessibilityAttributeValue:@"AXPreviousSentenceStartTextMarkerForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarker(previousTextMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::nextSentenceEndTextMarkerForTextMarker(AccessibilityTextMarker* textMarker)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id nextTextMarker = [m_element accessibilityAttributeValue:@"AXNextSentenceEndTextMarkerForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarker(nextTextMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

#endif // SUPPORTS_AX_TEXTMARKERS && PLATFORM(MAC)

void AccessibilityUIElement::scrollToMakeVisible()
{
}

void AccessibilityUIElement::scrollToMakeVisibleWithSubFocus(int x, int y, int width, int height)
{
}

void AccessibilityUIElement::scrollToGlobalPoint(int x, int y)
{
}
