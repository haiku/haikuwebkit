/*
 * Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007-2025 Apple Inc. All rights reserved.
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
#include "SVGLengthValue.h"

#include "AnimationUtilities.h"
#include "CSSParserContext.h"
#include "CSSParserTokenRange.h"
#include "CSSPrimitiveNumericTypes+Serialization.h"
#include "CSSPropertyParserConsumer+LengthPercentageDefinitions.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+NumberDefinitions.h"
#include "CSSTokenizer.h"
#include "ExceptionOr.h"
#include "SVGElement.h"
#include "SVGLengthContext.h"
#include "SVGParsingError.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SVGLengthValue);

static float adjustValueForPercentageStorage(float value, SVGLengthType type)
{
    // 100% = 100.0 instead of 1.0 for historical reasons, this could eventually be changed
    if (type == SVGLengthType::Percentage)
        return value / 100;
    return value;
}

static inline SVGLengthType primitiveTypeToLengthType(CSSUnitType primitiveType)
{
    switch (primitiveType) {
    case CSSUnitType::CSS_UNKNOWN:
        return SVGLengthType::Unknown;
    case CSSUnitType::CSS_NUMBER:
        return SVGLengthType::Number;
    case CSSUnitType::CSS_PERCENTAGE:
        return SVGLengthType::Percentage;
    case CSSUnitType::CSS_EM:
        return SVGLengthType::Ems;
    case CSSUnitType::CSS_EX:
        return SVGLengthType::Exs;
    case CSSUnitType::CSS_PX:
        return SVGLengthType::Pixels;
    case CSSUnitType::CSS_CM:
        return SVGLengthType::Centimeters;
    case CSSUnitType::CSS_MM:
        return SVGLengthType::Millimeters;
    case CSSUnitType::CSS_IN:
        return SVGLengthType::Inches;
    case CSSUnitType::CSS_PT:
        return SVGLengthType::Points;
    case CSSUnitType::CSS_PC:
        return SVGLengthType::Picas;
    case CSSUnitType::CSS_LH:
        return SVGLengthType::Lh;
    case CSSUnitType::CSS_CH:
        return SVGLengthType::Ch;
    default:
        return SVGLengthType::Unknown;
    }

    return SVGLengthType::Unknown;
}

static inline CSSUnitType lengthTypeToPrimitiveType(SVGLengthType lengthType)
{
    switch (lengthType) {
    case SVGLengthType::Unknown:
        return CSSUnitType::CSS_UNKNOWN;
    case SVGLengthType::Number:
        return CSSUnitType::CSS_NUMBER;
    case SVGLengthType::Percentage:
        return CSSUnitType::CSS_PERCENTAGE;
    case SVGLengthType::Ems:
        return CSSUnitType::CSS_EM;
    case SVGLengthType::Exs:
        return CSSUnitType::CSS_EX;
    case SVGLengthType::Pixels:
        return CSSUnitType::CSS_PX;
    case SVGLengthType::Centimeters:
        return CSSUnitType::CSS_CM;
    case SVGLengthType::Millimeters:
        return CSSUnitType::CSS_MM;
    case SVGLengthType::Inches:
        return CSSUnitType::CSS_IN;
    case SVGLengthType::Points:
        return CSSUnitType::CSS_PT;
    case SVGLengthType::Picas:
        return CSSUnitType::CSS_PC;
    case SVGLengthType::Lh:
        return CSSUnitType::CSS_LH;
    case SVGLengthType::Ch:
        return CSSUnitType::CSS_CH;
    }

    ASSERT_NOT_REACHED();
    return CSSUnitType::CSS_UNKNOWN;
}

static Variant<CSS::Number<>, CSS::LengthPercentage<>> createVariantForLengthType(float value, SVGLengthType lengthType)
{
    if (lengthType == SVGLengthType::Number)
        return CSS::Number<>(value);

    if (lengthType == SVGLengthType::Unknown) {
        // For unknown types (like container units), fall back to Number
        // FIXME: Add support for container units
        return CSS::Number<>(value);
    }

    auto unitType = lengthTypeToPrimitiveType(lengthType);
    auto unit = CSS::toLengthPercentageUnit(unitType);

    if (!unit) {
        ASSERT_NOT_REACHED();
        return CSS::Number<>(value);
    }

    return CSS::LengthPercentage<>(*unit, value);
}


SVGLengthValue::SVGLengthValue(SVGLengthMode lengthMode, const String& valueAsString)
    : m_value(CSS::Number<>(0))
    , m_lengthMode(lengthMode)
{
    setValueAsString(valueAsString);
}

SVGLengthValue::SVGLengthValue(float valueInSpecifiedUnits, SVGLengthType lengthType, SVGLengthMode lengthMode)
    : m_value(createVariantForLengthType(valueInSpecifiedUnits, lengthType))
    , m_lengthMode(lengthMode)
{
}

SVGLengthValue::SVGLengthValue(const SVGLengthContext& context, float value, SVGLengthType lengthType, SVGLengthMode lengthMode)
    : m_value(createVariantForLengthType(0, lengthType))
    , m_lengthMode(lengthMode)
{
    setValue(context, value);
}

std::optional<SVGLengthValue> SVGLengthValue::construct(SVGLengthMode lengthMode, StringView valueAsString)
{
    SVGLengthValue length { lengthMode };
    if (length.setValueAsString(valueAsString).hasException())
        return std::nullopt;
    return length;
}

SVGLengthValue SVGLengthValue::construct(SVGLengthMode lengthMode, StringView valueAsString, SVGParsingError& parseError, SVGLengthNegativeValuesMode negativeValuesMode)
{
    SVGLengthValue length(lengthMode);

    if (length.setValueAsString(valueAsString).hasException())
        parseError = SVGParsingError::ParsingFailed;
    else if (negativeValuesMode == SVGLengthNegativeValuesMode::Forbid && length.valueInSpecifiedUnits() < 0)
        parseError = SVGParsingError::ForbiddenNegativeValue;

    return length;
}

SVGLengthValue SVGLengthValue::blend(const SVGLengthValue& from, const SVGLengthValue& to, float progress)
{
    if ((from.isZero() && to.isZero())
        || from.lengthType() == SVGLengthType::Unknown
        || to.lengthType() == SVGLengthType::Unknown
        || (!from.isZero() && from.lengthType() != SVGLengthType::Percentage && to.lengthType() == SVGLengthType::Percentage)
        || (!to.isZero() && from.lengthType() == SVGLengthType::Percentage && to.lengthType() != SVGLengthType::Percentage)
        || (!from.isZero() && !to.isZero() && (from.lengthType() == SVGLengthType::Ems || from.lengthType() == SVGLengthType::Exs) && from.lengthType() != to.lengthType()))
        return to;

    if (from.lengthType() == SVGLengthType::Percentage || to.lengthType() == SVGLengthType::Percentage) {
        auto fromPercent = from.valueAsPercentage() * 100;
        auto toPercent = to.valueAsPercentage() * 100;
        return { WebCore::blend(fromPercent, toPercent, { progress }), SVGLengthType::Percentage };
    }

    if (from.lengthType() == to.lengthType() || from.isZero() || to.isZero() || from.isRelative()) {
        auto fromValue = from.valueInSpecifiedUnits();
        auto toValue = to.valueInSpecifiedUnits();
        return { WebCore::blend(fromValue, toValue, { progress }), to.isZero() ? from.lengthType() : to.lengthType() };
    }

    SVGLengthContext nonRelativeLengthContext(nullptr);
    auto fromValueInUserUnits = nonRelativeLengthContext.convertValueToUserUnits(from.valueInSpecifiedUnits(), from.lengthType(), from.lengthMode());
    if (fromValueInUserUnits.hasException())
        return { };

    auto fromValue = nonRelativeLengthContext.convertValueFromUserUnits(fromValueInUserUnits.releaseReturnValue(), to.lengthType(), to.lengthMode());
    if (fromValue.hasException())
        return { };

    float toValue = to.valueInSpecifiedUnits();
    return { WebCore::blend(fromValue.releaseReturnValue(), toValue, { progress }), to.lengthType() };
}

ExceptionOr<void> SVGLengthValue::setValueAsString(StringView valueAsString, SVGLengthMode lengthMode)
{
    m_lengthMode = lengthMode;
    return setValueAsString(valueAsString);
}

SVGLengthType SVGLengthValue::lengthType() const
{
    return WTF::switchOn(m_value,
        [](const CSS::Number<>&) -> SVGLengthType {
            return SVGLengthType::Number;
        },
        [](const CSS::LengthPercentage<>& length) -> SVGLengthType {
            if (auto raw = length.raw())
                return primitiveTypeToLengthType(toCSSUnitType(raw->unit));

            return SVGLengthType::Unknown;
        }
    );
}

bool SVGLengthValue::isZero() const
{
    return WTF::switchOn(m_value,
        [](const auto& value) {
            return value.isKnownZero();
        }
    );
}

bool SVGLengthValue::isRelative() const
{
    return WTF::switchOn(m_value,
        [](const CSS::Number<>&) {
            return false;
        },
        [](const CSS::LengthPercentage<>& length) {
            if (auto raw = length.raw()) {
                using Unit = CSS::LengthPercentageUnit;
                switch (raw->unit) {
                case Unit::Percentage:
                case Unit::Em:
                case Unit::Ex:
                case Unit::Ch:
                case Unit::Lh:
                case Unit::Rem:
                case Unit::Rex:
                case Unit::Rlh:
                case Unit::Rch:
                    return true;
                default:
                    return false;
                }
            }

            return false;
        }
    );
}

float SVGLengthValue::value(const SVGLengthContext& context) const
{
    auto result = valueForBindings(context);
    if (result.hasException())
        return 0;
    return result.releaseReturnValue();
}

float SVGLengthValue::valueAsPercentage() const
{
    return WTF::switchOn(m_value,
        [](const CSS::Number<>& number) -> float {
            if (auto raw = number.raw())
                return raw->value;

            return 0.0f;
        },
        [](const CSS::LengthPercentage<>& length) -> float {
            if (auto raw = length.raw()) {
                if (raw->unit == CSS::LengthPercentageUnit::Percentage)
                    return raw->value / 100.0f;

                return raw->value;
            }

            return 0.0f;
        }
    );
}

float SVGLengthValue::valueInSpecifiedUnits() const
{
    // Per SVG spec: return 0 for non-scalar values like calc()
    // https://svgwg.org/svg2-draft/types.html#__svg__SVGLength__valueInSpecifiedUnits

    return WTF::switchOn(m_value,
        [](const auto& value) {
            if (auto raw = value.raw())
                return clampTo<float>(raw->value);

            return 0.f;
        }
    );
}

String SVGLengthValue::valueAsString() const
{
    return WTF::switchOn(m_value,
        [](const auto& value) {
            if (auto raw = value.raw()) {
                // FIXME: Handle calc() expressions and consider exponential notation for very large/small values
                float numericValue = clampTo<float>(raw->value);
                return formatCSSNumberValue(CSS::SerializableNumber { numericValue, CSS::unitString(raw->unit) });
            }

            return String();
        }
    );
}

AtomString SVGLengthValue::valueAsAtomString() const
{
    return makeAtomString(valueAsString());
}

ExceptionOr<float> SVGLengthValue::valueForBindings(const SVGLengthContext& context) const
{
    return WTF::switchOn(m_value,
        [&](const CSS::Number<>& number) -> ExceptionOr<float> {
            if (auto raw = number.raw())
                return context.convertValueToUserUnits(raw->value, SVGLengthType::Number, m_lengthMode);

            return Exception { ExceptionCode::NotFoundError };
        },
        [&](const CSS::LengthPercentage<>& length) -> ExceptionOr<float> {
            if (length.isCalc())
                return Exception { ExceptionCode::NotSupportedError };

            if (auto raw = length.raw()) {
                auto lengthType = primitiveTypeToLengthType(toCSSUnitType(raw->unit));
                return context.convertValueToUserUnits(raw->value, lengthType, m_lengthMode);
            }

            return Exception { ExceptionCode::NotFoundError };
        }
    );
}

void SVGLengthValue::setValueInSpecifiedUnits(float value)
{
    m_value = WTF::switchOn(m_value,
        [&](const CSS::Number<>&) -> decltype(m_value) {
            return CSS::Number<>(value);
        },
        [&](const CSS::LengthPercentage<>& current) -> decltype(m_value) {
            if (auto raw = current.raw())
                return CSS::LengthPercentage<>(raw->unit, value);

            return CSS::Number<>(value);
        }
    );
}

ExceptionOr<void> SVGLengthValue::setValue(const SVGLengthContext& context, float value)
{
    auto svgLengthType = lengthType();
    auto adjustedValue = adjustValueForPercentageStorage(value, svgLengthType);

    auto convertedValue = context.convertValueFromUserUnits(adjustedValue, svgLengthType, m_lengthMode);
    if (convertedValue.hasException())
        return convertedValue.releaseException();

    m_value = WTF::switchOn(m_value,
        [&](const CSS::Number<>&) -> decltype(m_value) {
            return CSS::Number<>(convertedValue.releaseReturnValue());
        },
        [&](const CSS::LengthPercentage<>& current) -> decltype(m_value) {
            if (auto raw = current.raw())
                return CSS::LengthPercentage<>(raw->unit, convertedValue.releaseReturnValue());

            return CSS::Number<>(convertedValue.releaseReturnValue());
        }
    );

    return { };
}

ExceptionOr<void> SVGLengthValue::setValue(const SVGLengthContext& context, float value, SVGLengthType lengthType, SVGLengthMode lengthMode)
{
    // FIXME: Seems like a bug that we change the value of m_unit even if setValue throws an exception.
    m_lengthMode = lengthMode;
    m_value = createVariantForLengthType(value, lengthType);

    return setValue(context, value);
}

ExceptionOr<void> SVGLengthValue::setValueAsString(StringView string)
{
    if (string.isEmpty())
        return { };

    // FIXME: Allow leading and trailing whitespace in SVG attributes
    // using <integer>, <angle>, <number>, <length>, and <percentage>
    // rdar://115963075
    if (isASCIIWhitespace(string[string.length() - 1]))
        return Exception { ExceptionCode::SyntaxError };

    // CSS::Range only clamps to boundaries, but we historically handled
    // overflow values like "-45e58" to 0 instead of FLT_MAX.
    // FIXME: Consider setting to a proper value
    auto isFloatOverflow = [](const auto& parsedValue) {
        if (auto raw = parsedValue.raw()) {
            double value = raw->value;
            return value > FLT_MAX || value < -FLT_MAX;
        }
        return true;
    };

    auto parserContext = CSSParserContext { SVGAttributeMode };
    auto parserState = CSS::PropertyParserState {
        .context = parserContext
    };

    String newString = string.toString();
    CSSTokenizer tokenizer(newString);
    auto tokenRange = tokenizer.tokenRange();

    if (auto number = CSSPropertyParserHelpers::MetaConsumer<CSS::Number<>>::consume(tokenRange, parserState, { })) {
        if (!tokenRange.atEnd())
            return Exception { ExceptionCode::SyntaxError };

        m_value = isFloatOverflow(*number) ? CSS::Number<>(0) : WTFMove(*number);

        return { };
    }

    tokenRange = tokenizer.tokenRange();
    if (auto length = CSSPropertyParserHelpers::MetaConsumer<CSS::LengthPercentage<>>::consume(tokenRange, parserState, { })) {
        if (!tokenRange.atEnd())
            return Exception { ExceptionCode::SyntaxError };

        // FIXME: Add support for calculated lengths.
        if (length->isCalc())
            return Exception { ExceptionCode::SyntaxError };

        m_value = WTFMove(*length);

        return { };
    }

    return Exception { ExceptionCode::SyntaxError };
}

ExceptionOr<void> SVGLengthValue::convertToSpecifiedUnits(const SVGLengthContext& context, SVGLengthType targetType)
{
    auto valueInUserUnits = valueForBindings(context);
    if (valueInUserUnits.hasException())
        return valueInUserUnits.releaseException();

    auto convertedValue = context.convertValueFromUserUnits(valueInUserUnits.releaseReturnValue(), targetType, m_lengthMode);

    if (convertedValue.hasException())
        return convertedValue.releaseException();

    float adjustedValue = adjustValueForPercentageStorage(convertedValue.releaseReturnValue(), targetType);

    m_value = createVariantForLengthType(adjustedValue, targetType);

    return { };
}

TextStream& operator<<(TextStream& ts, const SVGLengthValue& length)
{
    ts << length.valueAsString();
    return ts;
}

}
