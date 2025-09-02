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

#include <WebCore/CalculationValue.h>
#include <WebCore/StyleLengthWrapper.h>

namespace WebCore {
namespace Style {

// MARK: - Platform

template<LengthWrapperBaseDerived T> struct ToPlatform<T> {
    auto operator()(const T& value) -> WebCore::Length
    {
        if (value.template holdsAlternative<typename T::Fixed>())
            return WebCore::Length(value.m_value.value(), WebCore::LengthType::Fixed, value.m_value.hasQuirk());

        if (value.template holdsAlternative<typename T::Percentage>())
            return WebCore::Length(value.m_value.value(), WebCore::LengthType::Percent);

        if (value.template holdsAlternative<typename T::Calc>())
            return WebCore::Length(value.m_value.protectedCalculationValue());

        if constexpr (T::SupportsAuto) {
            if (value.template holdsAlternative<CSS::Keyword::Auto>())
                return WebCore::Length(WebCore::LengthType::Auto);
        }
        if constexpr (T::SupportsContent) {
            if (value.template holdsAlternative<CSS::Keyword::Content>())
                return WebCore::Length(WebCore::LengthType::Content);
        }
        if constexpr (T::SupportsWebkitFillAvailable) {
            if (value.template holdsAlternative<CSS::Keyword::WebkitFillAvailable>())
                return WebCore::Length(WebCore::LengthType::FillAvailable);
        }
        if constexpr (T::SupportsFitContent) {
            if (value.template holdsAlternative<CSS::Keyword::FitContent>())
                return WebCore::Length(WebCore::LengthType::FitContent);
        }
        if constexpr (T::SupportsIntrinsic) {
            if (value.template holdsAlternative<CSS::Keyword::Intrinsic>())
                return WebCore::Length(WebCore::LengthType::Intrinsic);
        }
        if constexpr (T::SupportsMinContent) {
            if (value.template holdsAlternative<CSS::Keyword::MinContent>())
                return WebCore::Length(WebCore::LengthType::MinContent);
        }
        if constexpr (T::SupportsMaxContent) {
            if (value.template holdsAlternative<CSS::Keyword::MaxContent>())
                return WebCore::Length(WebCore::LengthType::MaxContent);
        }
        if constexpr (T::SupportsNormal) {
            if (value.template holdsAlternative<CSS::Keyword::Normal>())
                return WebCore::Length(WebCore::LengthType::Normal);
        }
        if constexpr (T::SupportsNone) {
            if (value.template holdsAlternative<CSS::Keyword::None>())
                return WebCore::Length(WebCore::LengthType::Undefined);
        }

        ASSERT_NOT_REACHED();
        return WebCore::Length();
    }
};

} // namespace Style
} // namespace WebCore
