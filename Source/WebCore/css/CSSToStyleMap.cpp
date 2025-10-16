/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Google Inc. All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "CSSToStyleMap.h"

#include "Animation.h"
#include "CSSBackgroundRepeatValue.h"
#include "CSSBorderImageSliceValue.h"
#include "CSSBorderImageWidthValue.h"
#include "CSSImageSetValue.h"
#include "CSSImageValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSPropertyParser.h"
#include "CSSQuadValue.h"
#include "CSSScrollValue.h"
#include "CSSValueKeywords.h"
#include "CSSViewValue.h"
#include "CompositeOperation.h"
#include "ScrollTimeline.h"
#include "StyleBuilderConverter.h"
#include "StyleResolver.h"
#include "ViewTimeline.h"

namespace WebCore {

static bool treatAsInitialValue(const CSSValue& value, CSSPropertyID propertyID)
{
    switch (valueID(value)) {
    case CSSValueInitial:
        return true;
    case CSSValueUnset:
        return !CSSProperty::isInheritedProperty(propertyID);
    default:
        return false;
    }
}

CSSToStyleMap::CSSToStyleMap(Style::BuilderState& builderState)
    : m_builderState(builderState)
{
}

void CSSToStyleMap::mapAnimationDelay(Animation& animation, const CSSValue& value)
{
    if (treatAsInitialValue(value, CSSPropertyAnimationDelay)) {
        animation.setDelay(Animation::initialDelay());
        return;
    }

    auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value);
    if (!primitiveValue)
        return;

    animation.setDelay(primitiveValue->resolveAsTime(m_builderState.cssToLengthConversionData()));
}

void CSSToStyleMap::mapAnimationDirection(Animation& layer, const CSSValue& value)
{
    if (treatAsInitialValue(value, CSSPropertyAnimationDirection)) {
        layer.setDirection(Animation::initialDirection());
        return;
    }

    if (!is<CSSPrimitiveValue>(value))
        return;

    switch (value.valueID()) {
    case CSSValueNormal:
        layer.setDirection(Animation::Direction::Normal);
        break;
    case CSSValueAlternate:
        layer.setDirection(Animation::Direction::Alternate);
        break;
    case CSSValueReverse:
        layer.setDirection(Animation::Direction::Reverse);
        break;
    case CSSValueAlternateReverse:
        layer.setDirection(Animation::Direction::AlternateReverse);
        break;
    default:
        break;
    }
}

void CSSToStyleMap::mapAnimationDuration(Animation& animation, const CSSValue& value)
{
    if (treatAsInitialValue(value, CSSPropertyAnimationDuration)) {
        animation.setDuration(Animation::initialDuration());
        return;
    }

    auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value);
    if (!primitiveValue)
        return;

    if (value.valueID() == CSSValueAuto) {
        animation.setDuration(std::nullopt);
        return;
    }

    auto duration = std::max<double>(primitiveValue->resolveAsTime(m_builderState.cssToLengthConversionData()), 0);
    animation.setDuration(duration);
}

void CSSToStyleMap::mapAnimationFillMode(Animation& layer, const CSSValue& value)
{
    if (treatAsInitialValue(value, CSSPropertyAnimationFillMode)) {
        layer.setFillMode(Animation::initialFillMode());
        return;
    }

    if (!is<CSSPrimitiveValue>(value))
        return;

    switch (value.valueID()) {
    case CSSValueNone:
        layer.setFillMode(AnimationFillMode::None);
        break;
    case CSSValueForwards:
        layer.setFillMode(AnimationFillMode::Forwards);
        break;
    case CSSValueBackwards:
        layer.setFillMode(AnimationFillMode::Backwards);
        break;
    case CSSValueBoth:
        layer.setFillMode(AnimationFillMode::Both);
        break;
    default:
        break;
    }
}

void CSSToStyleMap::mapAnimationIterationCount(Animation& animation, const CSSValue& value)
{
    if (treatAsInitialValue(value, CSSPropertyAnimationIterationCount)) {
        animation.setIterationCount(Animation::initialIterationCount());
        return;
    }

    auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value);
    if (!primitiveValue)
        return;

    if (primitiveValue->valueID() == CSSValueInfinite)
        animation.setIterationCount(Animation::IterationCountInfinite);
    else
        animation.setIterationCount(primitiveValue->resolveAsNumber<float>(m_builderState.cssToLengthConversionData()));
}

void CSSToStyleMap::mapAnimationName(Animation& layer, const CSSValue& value)
{
    if (treatAsInitialValue(value, CSSPropertyAnimationName)) {
        layer.setName(Animation::initialName());
        return;
    }

    auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value);
    if (!primitiveValue)
        return;

    if (primitiveValue->valueID() == CSSValueNone)
        layer.setName(Animation::initialName());
    else
        layer.setName({ AtomString { primitiveValue->stringValue() }, m_builderState.styleScopeOrdinal(), primitiveValue->isCustomIdent() });
}

void CSSToStyleMap::mapAnimationPlayState(Animation& layer, const CSSValue& value)
{
    if (treatAsInitialValue(value, CSSPropertyAnimationPlayState)) {
        layer.setPlayState(Animation::initialPlayState());
        return;
    }

    if (!is<CSSPrimitiveValue>(value))
        return;

    AnimationPlayState playState = (value.valueID() == CSSValuePaused) ? AnimationPlayState::Paused : AnimationPlayState::Playing;
    layer.setPlayState(playState);
}

void CSSToStyleMap::mapAnimationProperty(Animation& animation, const CSSValue& value)
{
    if (treatAsInitialValue(value, CSSPropertyAnimation)) {
        animation.setProperty(Animation::initialProperty());
        return;
    }

    auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value);
    if (!primitiveValue)
        return;

    if (primitiveValue->valueID() == CSSValueAll) {
        animation.setProperty({ Animation::TransitionMode::All, CSSPropertyInvalid });
        return;
    }
    if (primitiveValue->valueID() == CSSValueNone) {
        animation.setProperty({ Animation::TransitionMode::None, CSSPropertyInvalid });
        return;
    }
    if (primitiveValue->propertyID() == CSSPropertyInvalid) {
        auto stringValue = primitiveValue->stringValue();
        auto transitionMode = isCustomPropertyName(stringValue) ? Animation::TransitionMode::SingleProperty : Animation::TransitionMode::UnknownProperty;
        animation.setProperty({ transitionMode, AtomString { stringValue } });
        return;
    }

    animation.setProperty({ Animation::TransitionMode::SingleProperty, primitiveValue->propertyID() });
}

void CSSToStyleMap::mapAnimationTimeline(Animation& animation, const CSSValue& value)
{
    auto mapScrollValue = [](const CSSScrollValue& cssScrollValue) -> Animation::AnonymousScrollTimeline {
        auto scroller = [&] {
            auto& scrollerValue = cssScrollValue.scroller();
            if (!scrollerValue)
                return Scroller::Nearest;

            switch (scrollerValue->valueID()) {
            case CSSValueNearest:
                return Scroller::Nearest;
            case CSSValueRoot:
                return Scroller::Root;
            case CSSValueSelf:
                return Scroller::Self;
            default:
                ASSERT_NOT_REACHED();
                return Scroller::Nearest;
            }
        }();
        auto& axisValue = cssScrollValue.axis();
        auto axis = axisValue ? fromCSSValueID<ScrollAxis>(axisValue->valueID()) : ScrollAxis::Block;
        return { scroller, axis };
    };

    auto mapViewValue = [&](const CSSViewValue& cssViewValue) -> Animation::AnonymousViewTimeline {
        auto& axisValue = cssViewValue.axis();
        auto axis = axisValue ? fromCSSValueID<ScrollAxis>(axisValue->valueID()) : ScrollAxis::Block;
        auto convertInsetValue = [&](CSSValue* value) -> std::optional<Length> {
            if (!value)
                return std::nullopt;
            return Style::BuilderConverter::convertLengthOrAuto(m_builderState, *value);
        };
        auto startInset = convertInsetValue(cssViewValue.startInset().get());
        auto endInset = [&] {
            if (auto& endInsetValue = cssViewValue.endInset())
                return convertInsetValue(endInsetValue.get());
            return convertInsetValue(cssViewValue.startInset().get());
        }();
        return { axis, { startInset, endInset } };
    };

    if (treatAsInitialValue(value, CSSPropertyAnimationTimeline))
        animation.setTimeline(Animation::initialTimeline());
    else if (auto* viewValue = dynamicDowncast<CSSViewValue>(value))
        animation.setTimeline(mapViewValue(*viewValue));
    else if (auto* scrollValue = dynamicDowncast<CSSScrollValue>(value))
        animation.setTimeline(mapScrollValue(*scrollValue));
    else if (value.isCustomIdent())
        animation.setTimeline(AtomString(value.customIdent()));
    else {
        switch (value.valueID()) {
        case CSSValueNone:
            animation.setTimeline(Animation::TimelineKeyword::None);
            break;
        case CSSValueAuto:
            animation.setTimeline(Animation::TimelineKeyword::Auto);
            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }
}

void CSSToStyleMap::mapAnimationTimingFunction(Animation& animation, const CSSValue& value)
{
    if (treatAsInitialValue(value, CSSPropertyAnimationTimingFunction))
        animation.setTimingFunction(Animation::initialTimingFunction());
    else if (auto timingFunction = Style::BuilderConverter::convertTimingFunction(m_builderState, value))
        animation.setTimingFunction(WTFMove(timingFunction));
}

void CSSToStyleMap::mapAnimationCompositeOperation(Animation& animation, const CSSValue& value)
{
    if (treatAsInitialValue(value, CSSPropertyAnimationComposition))
        animation.setCompositeOperation(Animation::initialCompositeOperation());
    else if (auto compositeOperation = toCompositeOperation(value))
        animation.setCompositeOperation(*compositeOperation);
}

void CSSToStyleMap::mapAnimationAllowsDiscreteTransitions(Animation& layer, const CSSValue& value)
{
    if (treatAsInitialValue(value, CSSPropertyTransitionBehavior))
        layer.setAllowsDiscreteTransitions(Animation::initialAllowsDiscreteTransitions());
    else if (is<CSSPrimitiveValue>(value))
        layer.setAllowsDiscreteTransitions(value.valueID() == CSSValueAllowDiscrete);
}

void CSSToStyleMap::mapAnimationRangeStart(Animation& animation, const CSSValue& value)
{
    if (treatAsInitialValue(value, CSSPropertyAnimationRangeStart))
        animation.setRangeStart(Animation::initialRangeStart());
    animation.setRangeStart(SingleTimelineRange::range(value, SingleTimelineRange::Type::Start, &m_builderState));
}

void CSSToStyleMap::mapAnimationRangeEnd(Animation& animation, const CSSValue& value)
{
    if (treatAsInitialValue(value, CSSPropertyAnimationRangeEnd))
        animation.setRangeEnd(Animation::initialRangeEnd());
    animation.setRangeEnd(SingleTimelineRange::range(value, SingleTimelineRange::Type::End, &m_builderState));
}

} // namespace WebCore
