/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#pragma once

#include <WebCore/AnimationUtilities.h>
#include <WebCore/CalculationValue.h>
#include <WebCore/StyleLengthWrapper.h>

namespace WebCore {
namespace Style {

// MARK: - Blending

template<LengthWrapperBaseDerived T> struct Blending<T> {
    auto canBlend(const T& a, const T& b) -> bool
    {
        return a.hasSameType(b) || (a.isSpecified() && b.isSpecified());
    }
    auto requiresInterpolationForAccumulativeIteration(const T& a, const T& b) -> bool
    {
        return !a.hasSameType(b) || a.isCalculated() || b.isCalculated();
    }
    static Calculation::Child copyCalculation(const T& value)
    {
        if (value.isPercent())
            return Calculation::percentage(value.m_value.value());
        if (value.isCalculated())
            return value.m_value.protectedCalculationValue()->copyRoot();
        ASSERT(value.isFixed());
        return Calculation::dimension(value.m_value.value());
    }
    T blendMixedSpecifiedTypes(const T& a, const T& b, const BlendingContext& context)
    {
        if (context.compositeOperation != CompositeOperation::Replace)
            return typename T::Calc { Calculation::add(copyCalculation(a), copyCalculation(b)) };

        if (!b.isCalculated() && !a.isPercent() && (context.progress == 1 || a.isZero())) {
            if (b.isPercent())
                return Style::blend(typename T::Percentage { 0 }, typename T::Percentage { b.m_value.value() }, context);
            return Style::blend(typename T::Fixed { 0 }, typename T::Fixed { b.m_value.value() }, context);
        }

        if (!a.isCalculated() && !b.isPercent() && (!context.progress || b.isZero())) {
            if (a.isPercent())
                return Style::blend(typename T::Percentage { a.m_value.value() }, typename T::Percentage { 0 }, context);
            return Style::blend(typename T::Fixed { a.m_value.value() }, typename T::Fixed { 0 }, context);
        }

        return typename T::Calc { Calculation::blend(copyCalculation(a), copyCalculation(b), context.progress) };
    }
    auto blend(const T& a, const T& b, const BlendingContext& context) -> T
    {
        if (!a.isSpecified() || !b.isSpecified())
            return context.progress < 0.5 ? a : b;

        if (a.isCalculated() || b.isCalculated() || !a.hasSameType(b))
            return blendMixedSpecifiedTypes(a, b, context);

        if (!context.progress && context.isReplace())
            return a;

        if (context.progress == 1 && context.isReplace())
            return b;

        auto resultType = b.m_value.type();

        ASSERT(resultType == T::indexForPercentage || resultType == T::indexForFixed);

        if (resultType == T::indexForPercentage) {
            return Style::blend(
                typename T::Percentage { a.m_value.value() },
                typename T::Percentage { b.m_value.value() },
                context
            );
        } else {
            return Style::blend(
                typename T::Fixed { a.m_value.value() },
                typename T::Fixed { b.m_value.value() },
                context
            );
        }
    }
};

} // namespace Style
} // namespace WebCore
