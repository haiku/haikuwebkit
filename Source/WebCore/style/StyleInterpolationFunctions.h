/*
 * Copyright (C) 2007-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2012, 2013 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2025 Sam Weinig. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "CSSPropertyNames.h"
#include "CachedImage.h"
#include "CalculationValue.h"
#include "ColorBlending.h"
#include "Document.h"
#include "FloatConversion.h"
#include "FontCascade.h"
#include "FontSelectionAlgorithm.h"
#include "FontSelectionValueInlines.h"
#include "FontTaggedSettings.h"
#include "IdentityTransformOperation.h"
#include "LengthPoint.h"
#include "Logging.h"
#include "Matrix3DTransformOperation.h"
#include "MatrixTransformOperation.h"
#include "PathOperation.h"
#include "RenderBox.h"
#include "RenderStyleSetters.h"
#include "SVGRenderStyle.h"
#include "ScopedName.h"
#include "Settings.h"
#include "StyleBoxShadow.h"
#include "StyleDynamicRangeLimit.h"
#include "StyleImageWrapper.h"
#include "StyleInterpolationClient.h"
#include "StyleInterpolationContext.h"
#include "StyleLengthWrapper+Blending.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StyleResolver.h"
#include "StyleTextEdge.h"
#include <algorithm>
#include <wtf/MathExtras.h>
#include <wtf/PointerComparison.h>

namespace WebCore::Style::Interpolation {

inline int blendFunc(int from, int to, const Context& context)
{
    return WebCore::blend(from, to, context);
}

inline double blendFunc(double from, double to, const Context& context)
{
    return WebCore::blend(from, to, context);
}

inline float blendFunc(float from, float to, const Context& context)
{
    if (context.iterationCompositeOperation == IterationCompositeOperation::Accumulate && context.currentIteration) {
        auto iterationIncrement = context.currentIteration * to;
        from += iterationIncrement;
        to += iterationIncrement;
    }

    if (context.compositeOperation == CompositeOperation::Replace)
        return narrowPrecisionToFloat(from + (to - from) * context.progress);
    return narrowPrecisionToFloat(from + from + (to - from) * context.progress);
}

inline WebCore::Color blendFunc(const WebCore::Color& from, const WebCore::Color& to, const Context& context)
{
    return WebCore::blend(from, to, context);
}

inline WebCore::Length blendFunc(const WebCore::Length& from, const WebCore::Length& to, const Context& context, ValueRange valueRange = ValueRange::All)
{
    return WebCore::blend(from, to, context, valueRange);
}

inline TabSize blendFunc(const TabSize& from, const TabSize& to, const Context& context)
{
    auto blendedValue = WebCore::blend(from.value(), to.value(), context);
    return { blendedValue < 0 ? 0 : blendedValue, from.isSpaces() ? SpaceValueType : LengthValueType };
}

inline LengthSize blendFunc(const LengthSize& from, const LengthSize& to, const Context& context)
{
    return WebCore::blend(from, to, context, ValueRange::NonNegative);
}

inline bool canInterpolateLengthVariants(const LengthSize& from, const LengthSize& to)
{
    bool isLengthPercentage = true;
    return canInterpolateLengths(from.width, to.width, isLengthPercentage)
        && canInterpolateLengths(from.height, to.height, isLengthPercentage);
}

inline bool lengthVariantRequiresInterpolationForAccumulativeIteration(const LengthSize& from, const LengthSize& to)
{
    return lengthsRequireInterpolationForAccumulativeIteration(from.width, to.width)
        || lengthsRequireInterpolationForAccumulativeIteration(from.height, to.height);
}

inline LengthPoint blendFunc(const LengthPoint& from, const LengthPoint& to, const Context& context)
{
    return WebCore::blend(from, to, context);
}

inline TransformOperations blendFunc(const TransformOperations& from, const TransformOperations& to, const Context& context)
{
    if (context.compositeOperation == CompositeOperation::Add) {
        ASSERT(context.progress == 1.0);

        Vector<Ref<TransformOperation>> operations;
        operations.reserveInitialCapacity(from.size() + to.size());

        operations.appendRange(from.begin(), from.end());
        operations.appendRange(to.begin(), to.end());

        return TransformOperations { WTFMove(operations) };
    }

    auto prefix = [&]() -> std::optional<unsigned> {
        // We cannot use the pre-computed prefix when dealing with accumulation
        // since the values used to accumulate may be different than those held
        // in the initial keyframe list. We must do the same with any property
        // other than "transform" since we only pre-compute the prefix for that
        // property.
        if (context.compositeOperation == CompositeOperation::Accumulate || std::holds_alternative<AtomString>(context.property) || std::get<CSSPropertyID>(context.property) != CSSPropertyTransform)
            return std::nullopt;
        return context.client.transformFunctionListPrefix();
    };

    auto* renderBox = dynamicDowncast<RenderBox>(context.client.renderer());
    auto boxSize = renderBox ? renderBox->borderBoxRect().size() : LayoutSize();
    return to.blend(from, context, boxSize, prefix());
}

inline Ref<TransformOperation> blendFunc(TransformOperation& from, TransformOperation& to, const Context& context)
{
    return to.blend(&from, context);
}

inline FilterOperations blendFunc(const FilterOperations& from, const FilterOperations& to, const Context& context)
{
    return from.blend(to, context);
}

inline ContentVisibility blendFunc(ContentVisibility from, ContentVisibility to, const Context& context)
{
    // https://drafts.csswg.org/css-contain-3/#content-visibility-animation
    // In general, the content-visibility property's animation type is discrete. However, similar to interpolation of
    // visibility, during interpolation between hidden and any other content-visibility value, p values between 0 and 1
    // map to the non-hidden value.
    if (from != ContentVisibility::Hidden && to != ContentVisibility::Hidden)
        return context.progress < 0.5 ? from : to;
    if (context.progress <= 0)
        return from;
    if (context.progress >= 1)
        return to;
    return from == ContentVisibility::Hidden ? to : from;
}

inline Visibility blendFunc(Visibility from, Visibility to, const Context& context)
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1.0);
        return context.progress ? to : from;
    }

    // Any non-zero result means we consider the object to be visible. Only at 0 do we consider the object to be
    // invisible. The invisible value we use (Visibility::Hidden vs. Visibility::Collapse) depends on the specified from/to values.
    double fromVal = from == Visibility::Visible ? 1. : 0.;
    double toVal = to == Visibility::Visible ? 1. : 0.;
    if (fromVal == toVal)
        return to;
    // The composite operation here is irrelevant.
    double result = blendFunc(fromVal, toVal, { context.property, context.progress, false, CompositeOperation::Replace, IterationCompositeOperation::Replace, 0, { }, { }, context.client });
    return result > 0. ? Visibility::Visible : (to != Visibility::Visible ? to : from);
}

inline DisplayType blendFunc(DisplayType from, DisplayType to, const Context& context)
{
    // https://drafts.csswg.org/css-display-4/#display-animation
    // In general, the display property's animation type is discrete. However, similar to interpolation of
    // visibility, during interpolation between none and any other display value, p values between 0 and 1
    // map to the non-none value. Additionally, the element is inert as long as its display value would
    // compute to none when ignoring the Transitions and Animations cascade origins.
    if (from != DisplayType::None && to != DisplayType::None)
        return context.progress < 0.5 ? from : to;
    if (context.progress <= 0)
        return from;
    if (context.progress >= 1)
        return to;
    return from == DisplayType::None ? to : from;
}

inline LengthBox blendFunc(const LengthBox& from, const LengthBox& to, const Context& context, ValueRange valueRange = ValueRange::NonNegative)
{
    return LengthBox {
        blendFunc(from.top(), to.top(), context, valueRange),
        blendFunc(from.right(), to.right(), context, valueRange),
        blendFunc(from.bottom(), to.bottom(), context, valueRange),
        blendFunc(from.left(), to.left(), context, valueRange),
    };
}

inline RefPtr<StyleImage> blendFunc(StyleImage* from, StyleImage* to, const Context& context)
{
    if (!context.progress)
        return from;

    if (context.progress == 1.0)
        return to;

    ASSERT(from);
    ASSERT(to);
    return blend(ImageWrapper { *from }, ImageWrapper { *to }, context).value.ptr();
}

#if ENABLE(VARIATION_FONTS)

inline FontVariationSettings blendFunc(const FontVariationSettings& from, const FontVariationSettings& to, const Context& context)
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1.0);
        return context.progress ? to : from;
    }

    ASSERT(from.size() == to.size());
    FontVariationSettings result;
    size_t size = from.size();
    for (size_t i = 0; i < size; ++i) {
        auto& fromItem = from.at(i);
        auto& toItem = to.at(i);
        ASSERT(fromItem.tag() == toItem.tag());
        result.insert({ fromItem.tag(), blendFunc(fromItem.value(), toItem.value(), context) });
    }
    return result;
}

#endif

inline FontSelectionValue blendFunc(FontSelectionValue from, FontSelectionValue to, const Context& context)
{
    return FontSelectionValue(std::max(0.0f, blendFunc(static_cast<float>(from), static_cast<float>(to), context)));
}

inline std::optional<FontSelectionValue> blendFunc(std::optional<FontSelectionValue> from, std::optional<FontSelectionValue> to, const Context& context)
{
    if (!from && !to)
        return std::nullopt;

    auto valueOrDefault = [](std::optional<FontSelectionValue> fontSelectionValue) {
        if (!fontSelectionValue)
            return 0.0f;
        return static_cast<float>(fontSelectionValue.value());
    };

    return normalizedFontItalicValue(blendFunc(valueOrDefault(from), valueOrDefault(to), context));
}

inline RefPtr<StylePathData> blendFunc(StylePathData* from, StylePathData* to, const Context& context)
{
    if (context.isDiscrete)
        return context.progress < 0.5 ? from : to;
    ASSERT(from);
    ASSERT(to);
    return from->blend(*to, context);
}

} // namespace WebCore::Style::Interpolation
