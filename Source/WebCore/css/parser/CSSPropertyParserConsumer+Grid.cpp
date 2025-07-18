/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSPropertyParserConsumer+Grid.h"

#include "CSSFunctionValue.h"
#include "CSSGridAutoRepeatValue.h"
#include "CSSGridIntegerRepeatValue.h"
#include "CSSGridLineNamesValue.h"
#include "CSSGridLineValue.h"
#include "CSSGridTemplateAreasValue.h"
#include "CSSParserIdioms.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserConsumer+CSSPrimitiveValueResolver.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+IntegerDefinitions.h"
#include "CSSPropertyParserConsumer+LengthPercentageDefinitions.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParserState.h"
#include "CSSSubgridValue.h"
#include "CSSValueList.h"
#include "CSSValuePool.h"
#include "GridArea.h"
#include "GridPosition.h"
#include <wtf/Vector.h>
#include <wtf/text/StringView.h>

namespace WebCore {
namespace CSSPropertyParserHelpers {

bool isGridBreadthIdent(CSSValueID id)
{
    return identMatches<CSSValueMinContent, CSSValueWebkitMinContent, CSSValueMaxContent, CSSValueWebkitMaxContent, CSSValueAuto>(id);
}

static RefPtr<CSSPrimitiveValue> consumeCustomIdentForGridLine(CSSParserTokenRange& range)
{
    if (range.peek().id() == CSSValueAuto || range.peek().id() == CSSValueSpan)
        return nullptr;
    return consumeCustomIdent(range);
}

static Vector<String> parseGridTemplateAreasColumnNames(StringView gridRowNames)
{
    ASSERT(!gridRowNames.isEmpty());
    Vector<String> columnNames;
    StringBuilder areaName;
    for (auto character : gridRowNames.codeUnits()) {
        if (isCSSSpace(character)) {
            if (!areaName.isEmpty()) {
                columnNames.append(areaName.toString());
                areaName.clear();
            }
            continue;
        }
        if (character == '.') {
            if (areaName == "."_s)
                continue;
            if (!areaName.isEmpty()) {
                columnNames.append(areaName.toString());
                areaName.clear();
            }
        } else {
            if (!isNameCodePoint(character))
                return Vector<String>();
            if (areaName == "."_s) {
                columnNames.append(areaName.toString());
                areaName.clear();
            }
        }
        areaName.append(character);
    }
    if (!areaName.isEmpty())
        columnNames.append(areaName.toString());
    return columnNames;
}

bool parseGridTemplateAreasRow(StringView gridRowNames, CSS::GridNamedAreaMap& gridAreaMap)
{
    if (gridRowNames.containsOnly<isCSSSpace>())
        return false;

    auto columnNames = parseGridTemplateAreasColumnNames(gridRowNames);
    if (!gridAreaMap.rowCount) {
        gridAreaMap.columnCount = columnNames.size();
        if (!gridAreaMap.columnCount)
            return false;
    } else if (gridAreaMap.columnCount != columnNames.size()) {
        // The declaration is invalid if all the rows don't have the number of columns.
        return false;
    }

    for (size_t currentColumn = 0; currentColumn < gridAreaMap.columnCount; ++currentColumn) {
        auto& gridAreaName = columnNames[currentColumn];

        // Unnamed areas are always valid (we consider them to be 1x1).
        if (gridAreaName == "."_s)
            continue;

        auto lookAheadColumn = currentColumn + 1;
        while (lookAheadColumn < gridAreaMap.columnCount && columnNames[lookAheadColumn] == gridAreaName)
            lookAheadColumn++;

        auto result = gridAreaMap.map.ensure(gridAreaName, [&] {
            return GridArea {
                GridSpan::translatedDefiniteGridSpan(gridAreaMap.rowCount, gridAreaMap.rowCount + 1),
                GridSpan::translatedDefiniteGridSpan(currentColumn, lookAheadColumn)
            };
        });
        if (!result.isNewEntry) {
            auto& gridArea = result.iterator->value;

            // The following checks test that the grid area is a single filled-in rectangle.
            // 1. The new row is adjacent to the previously parsed row.
            if (gridAreaMap.rowCount != gridArea.rows.endLine())
                return false;

            // 2. The new area starts at the same position as the previously parsed area.
            if (currentColumn != gridArea.columns.startLine())
                return false;

            // 3. The new area ends at the same position as the previously parsed area.
            if (lookAheadColumn != gridArea.columns.endLine())
                return false;

            gridArea.rows = GridSpan::translatedDefiniteGridSpan(gridArea.rows.startLine(), gridArea.rows.endLine() + 1);
        }
        currentColumn = lookAheadColumn - 1;
    }

    ++gridAreaMap.rowCount;
    return true;
}

RefPtr<CSSValue> consumeGridLine(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <grid-line> = auto
    //             | <custom-ident>
    //             | [ [ <integer [-∞,-1]> | <integer [1,∞]> ] && <custom-ident>? ]
    //             | [ span && [ <integer [1,∞]> || <custom-ident> ] ]
    //
    // https://drafts.csswg.org/css-grid/#typedef-grid-row-start-grid-line

    if (range.peek().id() == CSSValueAuto)
        return consumeIdent(range);

    RefPtr<CSSPrimitiveValue> spanValue;
    RefPtr<CSSPrimitiveValue> gridLineName;
    RefPtr<CSSPrimitiveValue> numericValue = CSSPrimitiveValueResolver<CSS::Integer<>>::consumeAndResolve(range, state);
    if (numericValue) {
        gridLineName = consumeCustomIdentForGridLine(range);
        spanValue = consumeIdent<CSSValueSpan>(range);
    } else {
        spanValue = consumeIdent<CSSValueSpan>(range);
        if (spanValue) {
            numericValue = CSSPrimitiveValueResolver<CSS::Integer<>>::consumeAndResolve(range, state);
            gridLineName = consumeCustomIdentForGridLine(range);
            if (!numericValue)
                numericValue = CSSPrimitiveValueResolver<CSS::Integer<>>::consumeAndResolve(range, state);
        } else {
            gridLineName = consumeCustomIdentForGridLine(range);
            if (gridLineName) {
                numericValue = CSSPrimitiveValueResolver<CSS::Integer<>>::consumeAndResolve(range, state);
                spanValue = consumeIdent<CSSValueSpan>(range);
                if (!spanValue && !numericValue)
                    return gridLineName;
            } else
                return nullptr;
        }
    }

    if (spanValue && !numericValue && !gridLineName)
        return nullptr; // "span" keyword alone is invalid.
    if (spanValue && numericValue && numericValue->isNegative().value_or(false))
        return nullptr; // Negative numbers are not allowed for span.
    if (numericValue && numericValue->isZero().value_or(false))
        return nullptr; // An <integer> value of zero makes the declaration invalid.

    return CSSGridLineValue::create(WTFMove(spanValue), WTFMove(numericValue), WTFMove(gridLineName));
}

static bool isGridTrackFixedSized(const CSSPrimitiveValue& primitiveValue)
{
    switch (primitiveValue.valueID()) {
    case CSSValueMinContent:
    case CSSValueWebkitMinContent:
    case CSSValueMaxContent:
    case CSSValueWebkitMaxContent:
    case CSSValueAuto:
        return false;
    default:
        return !primitiveValue.isFlex();
    }
}

static bool isGridTrackFixedSized(const CSSValue& value)
{
    if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value))
        return isGridTrackFixedSized(*primitiveValue);
    auto& function = downcast<CSSFunctionValue>(value);
    if (function.name() == CSSValueFitContent || function.length() < 2)
        return false;

    return isGridTrackFixedSized(downcast<CSSPrimitiveValue>(*function.protectedItem(0)))
        || isGridTrackFixedSized(downcast<CSSPrimitiveValue>(*function.protectedItem(1)));
}

static RefPtr<CSSPrimitiveValue> consumeGridBreadth(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <track-breadth>       = <length-percentage [0,∞]> | <flex [0,∞]> | min-content | max-content | auto
    // https://drafts.csswg.org/css-grid/#typedef-track-breadth

    const CSSParserToken& token = range.peek();
    if (isGridBreadthIdent(token.id()))
        return consumeIdent(range);
    if (token.type() == DimensionToken && token.unitType() == CSSUnitType::CSS_FR) {
        if (range.peek().numericValue() < 0)
            return nullptr;
        return CSSPrimitiveValue::create(range.consumeIncludingWhitespace().numericValue(), CSSUnitType::CSS_FR);
    }
    return CSSPrimitiveValueResolver<CSS::LengthPercentage<CSS::Nonnegative>>::consumeAndResolve(range, state);
}

static RefPtr<CSSValue> consumeFitContent(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    CSSParserTokenRange rangeCopy = range;
    CSSParserTokenRange args = consumeFunction(rangeCopy);
    auto length = CSSPrimitiveValueResolver<CSS::LengthPercentage<CSS::Nonnegative>>::consumeAndResolve(args, state);
    if (!length || !args.atEnd())
        return nullptr;
    range = rangeCopy;
    return CSSFunctionValue::create(CSSValueFitContent, length.releaseNonNull());
}

RefPtr<CSSValue> consumeGridTrackSize(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <track-size>          = <track-breadth> | minmax( <inflexible-breadth> , <track-breadth> ) | fit-content( <length-percentage [0,∞]> )
    // <track-breadth>       = <length-percentage [0,∞]> | <flex [0,∞]> | min-content | max-content | auto
    // <inflexible-breadth>  = <length-percentage [0,∞]> | min-content | max-content | auto
    //
    // https://drafts.csswg.org/css-grid/#typedef-track-size

    const CSSParserToken& token = range.peek();
    if (identMatches<CSSValueAuto>(token.id()))
        return consumeIdent(range);

    if (token.functionId() == CSSValueMinmax) {
        CSSParserTokenRange rangeCopy = range;
        CSSParserTokenRange args = consumeFunction(rangeCopy);
        auto minTrackBreadth = consumeGridBreadth(args, state);
        if (!minTrackBreadth || minTrackBreadth->isFlex() || !consumeCommaIncludingWhitespace(args))
            return nullptr;
        auto maxTrackBreadth = consumeGridBreadth(args, state);
        if (!maxTrackBreadth || !args.atEnd())
            return nullptr;
        range = rangeCopy;
        return CSSFunctionValue::create(CSSValueMinmax, minTrackBreadth.releaseNonNull(), maxTrackBreadth.releaseNonNull());
    }

    if (token.functionId() == CSSValueFitContent)
        return consumeFitContent(range, state);

    return consumeGridBreadth(range, state);
}

RefPtr<CSSGridLineNamesValue> consumeGridLineNames(CSSParserTokenRange& range, CSS::PropertyParserState&, AllowEmpty allowEmpty)
{
    CSSParserTokenRange rangeCopy = range;
    if (rangeCopy.consumeIncludingWhitespace().type() != LeftBracketToken)
        return nullptr;

    Vector<String, 4> lineNames;
    while (auto lineName = consumeCustomIdentForGridLine(rangeCopy))
        lineNames.append(lineName->customIdent());
    if (rangeCopy.consumeIncludingWhitespace().type() != RightBracketToken)
        return nullptr;
    range = rangeCopy;
    if (allowEmpty == AllowEmpty::No && lineNames.isEmpty())
        return nullptr;
    return CSSGridLineNamesValue::create(lineNames);
}

static bool consumeGridTrackRepeatFunction(CSSParserTokenRange& range, CSS::PropertyParserState& state, CSSValueListBuilder& list, bool& isAutoRepeat, bool& allTracksAreFixedSized)
{
    CSSParserTokenRange args = consumeFunction(range);

    CSSValueListBuilder repeatedValues;

    RefPtr<CSSPrimitiveValue> repetitions;
    auto autoRepeatType = consumeIdentRaw<CSSValueAutoFill, CSSValueAutoFit>(args);
    isAutoRepeat = autoRepeatType.has_value();
    if (!isAutoRepeat) {
        repetitions = CSSPrimitiveValueResolver<CSS::Integer<CSS::Range{1, CSS::Range::infinity}, unsigned>>::consumeAndResolve(args, state);
        if (!repetitions)
            return false;
    }
    if (!consumeCommaIncludingWhitespace(args))
        return false;
    if (auto lineNames = consumeGridLineNames(args, state))
        repeatedValues.append(lineNames.releaseNonNull());

    size_t numberOfTracks = 0;
    while (!args.atEnd()) {
        auto trackSize = consumeGridTrackSize(args, state);
        if (!trackSize)
            return false;
        if (allTracksAreFixedSized)
            allTracksAreFixedSized = isGridTrackFixedSized(*trackSize);
        repeatedValues.append(trackSize.releaseNonNull());
        ++numberOfTracks;
        if (auto lineNames = consumeGridLineNames(args, state))
            repeatedValues.append(lineNames.releaseNonNull());
    }
    // We should have found at least one <track-size> or else it is not a valid <track-list>.
    if (!numberOfTracks)
        return false;

    if (isAutoRepeat)
        list.append(CSSGridAutoRepeatValue::create(*autoRepeatType, WTFMove(repeatedValues)));
    else {
        auto maxRepetitions = GridPosition::max() / numberOfTracks;
        if (auto repetitionsInteger = repetitions->resolveAsIntegerIfNotCalculated(); repetitionsInteger && repetitionsInteger > maxRepetitions)
            repetitions = CSSPrimitiveValue::createInteger(maxRepetitions);
        list.append(CSSGridIntegerRepeatValue::create(repetitions.releaseNonNull(), WTFMove(repeatedValues)));
    }
    return true;
}

static bool consumeSubgridNameRepeatFunction(CSSParserTokenRange& range, CSS::PropertyParserState& state, CSSValueListBuilder& list, bool& isAutoRepeat)
{
    CSSParserTokenRange args = consumeFunction(range);
    RefPtr<CSSPrimitiveValue> repetitions;
    isAutoRepeat = consumeIdentRaw<CSSValueAutoFill>(args).has_value();
    if (!isAutoRepeat) {
        repetitions = CSSPrimitiveValueResolver<CSS::Integer<CSS::Range{1, CSS::Range::infinity}, unsigned>>::consumeAndResolve(args, state);
        if (!repetitions)
            return false;
        if (auto repetitionsInteger = repetitions->resolveAsIntegerIfNotCalculated(); repetitionsInteger && repetitionsInteger > GridPosition::max())
            repetitions = CSSPrimitiveValue::createInteger(GridPosition::max());
    }
    if (!consumeCommaIncludingWhitespace(args))
        return false;

    CSSValueListBuilder repeatedValues;
    do {
        auto lineNames = consumeGridLineNames(args, state, AllowEmpty::Yes);
        if (!lineNames)
            return false;
        repeatedValues.append(lineNames.releaseNonNull());
    } while (!args.atEnd());

    if (isAutoRepeat)
        list.append(CSSGridAutoRepeatValue::create(CSSValueAutoFill, WTFMove(repeatedValues)));
    else
        list.append(CSSGridIntegerRepeatValue::create(repetitions.releaseNonNull(), WTFMove(repeatedValues)));
    return true;
}

RefPtr<CSSValue> consumeGridTrackList(CSSParserTokenRange& range, CSS::PropertyParserState& state, TrackListType trackListType)
{
    if (state.context.masonryEnabled && range.peek().id() == CSSValueMasonry)
        return consumeIdent(range);

    bool seenAutoRepeat = false;
    if (trackListType == GridTemplate && range.peek().id() == CSSValueSubgrid) {
        consumeIdent(range);
        CSSValueListBuilder values;
        while (!range.atEnd() && range.peek().type() != DelimiterToken) {
            if (range.peek().functionId() == CSSValueRepeat) {
                bool isAutoRepeat;
                if (!consumeSubgridNameRepeatFunction(range, state, values, isAutoRepeat))
                    return nullptr;
                if (isAutoRepeat && seenAutoRepeat)
                    return nullptr;
                seenAutoRepeat = seenAutoRepeat || isAutoRepeat;
            } else if (auto value = consumeGridLineNames(range, state, AllowEmpty::Yes))
                values.append(value.releaseNonNull());
            else
                return nullptr;
        }
        return CSSSubgridValue::create(WTFMove(values));
    }

    bool allowGridLineNames = trackListType != GridAuto;
    if (!allowGridLineNames && range.peek().type() == LeftBracketToken)
        return nullptr;

    CSSValueListBuilder values;
    bool allowRepeat = trackListType == GridTemplate;
    bool allTracksAreFixedSized = true;
    if (auto lineNames = consumeGridLineNames(range, state))
        values.append(lineNames.releaseNonNull());
    do {
        bool isAutoRepeat;
        if (range.peek().functionId() == CSSValueRepeat) {
            if (!allowRepeat)
                return nullptr;
            if (!consumeGridTrackRepeatFunction(range, state, values, isAutoRepeat, allTracksAreFixedSized))
                return nullptr;
            if (isAutoRepeat && seenAutoRepeat)
                return nullptr;
            seenAutoRepeat = seenAutoRepeat || isAutoRepeat;
        } else if (RefPtr<CSSValue> value = consumeGridTrackSize(range, state)) {
            if (allTracksAreFixedSized)
                allTracksAreFixedSized = isGridTrackFixedSized(*value);
            values.append(value.releaseNonNull());
        } else
            return nullptr;
        if (seenAutoRepeat && !allTracksAreFixedSized)
            return nullptr;
        if (!allowGridLineNames && range.peek().type() == LeftBracketToken)
            return nullptr;
        if (auto lineNames = consumeGridLineNames(range, state))
            values.append(lineNames.releaseNonNull());
    } while (!range.atEnd() && range.peek().type() != DelimiterToken);
    return CSSValueList::createSpaceSeparated(WTFMove(values));
}

RefPtr<CSSValue> consumeGridTemplatesRowsOrColumns(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // none | <track-list> | <auto-track-list> | subgrid <line-name-list>?
    // https://drafts.csswg.org/css-grid/#track-sizing

    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);
    if (state.context.masonryEnabled && range.peek().id() == CSSValueMasonry)
        return consumeIdent(range);
    return consumeGridTrackList(range, state, GridTemplate);
}

RefPtr<CSSValue> consumeGridTemplateAreas(CSSParserTokenRange& range, CSS::PropertyParserState&)
{
    if (range.peek().id() == CSSValueNone)
        return consumeIdent(range);

    CSS::GridNamedAreaMap map;
    while (range.peek().type() == StringToken) {
        if (!parseGridTemplateAreasRow(range.consumeIncludingWhitespace().value(), map))
            return nullptr;
    }
    if (!map.rowCount)
        return nullptr;
    return CSSGridTemplateAreasValue::create({ WTFMove(map) });
}

RefPtr<CSSValue> consumeGridAutoFlow(CSSParserTokenRange& range, CSS::PropertyParserState&)
{
    auto rowOrColumnValue = consumeIdent<CSSValueRow, CSSValueColumn>(range);
    auto denseAlgorithm = consumeIdent<CSSValueDense>(range);
    if (!rowOrColumnValue) {
        rowOrColumnValue = consumeIdent<CSSValueRow, CSSValueColumn>(range);
        if (!rowOrColumnValue && !denseAlgorithm)
            return nullptr;
    }
    CSSValueListBuilder parsedValues;
    if (rowOrColumnValue) {
        CSSValueID value = rowOrColumnValue->valueID();
        if (value == CSSValueColumn || (value == CSSValueRow && !denseAlgorithm))
            parsedValues.append(rowOrColumnValue.releaseNonNull());
    }
    if (denseAlgorithm)
        parsedValues.append(denseAlgorithm.releaseNonNull());
    return CSSValueList::createSpaceSeparated(WTFMove(parsedValues));
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
