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

#include <WebCore/StyleLengthWrapper.h>

namespace WebCore {
namespace Style {

struct SVGStrokeDashoffsetLength : LengthWrapperBase<LengthPercentage<>> {
    using Base::Base;
};

// <'stroke-dashoffset'> = <length-percentage> | <number>@(converted-to-px)
// https://svgwg.org/svg2-draft/painting.html#StrokeDashoffsetProperty
struct SVGStrokeDashoffset {
    using Fixed = SVGStrokeDashoffsetLength::Fixed;
    using Percentage = SVGStrokeDashoffsetLength::Percentage;
    using Calc = SVGStrokeDashoffsetLength::Calc;

    SVGStrokeDashoffsetLength value;

    SVGStrokeDashoffset(SVGStrokeDashoffsetLength&& length) : value { WTFMove(length) } { }
    SVGStrokeDashoffset(CSS::ValueLiteral<CSS::LengthUnit::Px> literal) : value { literal } { }
    SVGStrokeDashoffset(CSS::ValueLiteral<CSS::PercentageUnit::Percentage> literal) : value { literal } { }

    bool isZero() const { return value.isZero(); }
    bool isPositive() const { return value.isPositive(); }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(value, std::forward<F>(f)...);
    }

    bool operator==(const SVGStrokeDashoffset&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(SVGStrokeDashoffset, value);

// MARK: - Conversion

template<> struct CSSValueConversion<SVGStrokeDashoffset> { auto operator()(BuilderState&, const CSSValue&) -> SVGStrokeDashoffset; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::SVGStrokeDashoffsetLength)
DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::Style::SVGStrokeDashoffset)
