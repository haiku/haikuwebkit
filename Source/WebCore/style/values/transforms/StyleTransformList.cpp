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

#include "config.h"
#include "StyleTransformList.h"

#include "LayoutSize.h"
#include "Matrix3DTransformOperation.h"
#include "RenderBox.h"
#include "StyleInterpolationClient.h"
#include "StyleInterpolationContext.h"
#include "TransformOperations.h"
#include "TransformationMatrix.h"

namespace WebCore {
namespace Style {

WebCore::TransformOperations TransformList::resolvedCalculatedValues(const FloatSize& size) const
{
    return WebCore::TransformOperations { WTF::map(m_value, [&size](const auto& function) {
        return function->selfOrCopyWithResolvedCalculatedValues(size);
    }) };
}

void TransformList::apply(TransformationMatrix& matrix, const FloatSize& size, unsigned start) const
{
    for (unsigned i = start; i < m_value.size(); ++i)
        m_value[i]->apply(matrix, size);
}

bool TransformList::has3DOperation() const
{
    return std::ranges::any_of(*this, [](auto& function) { return function->is3DOperation(); });
}

bool TransformList::isRepresentableIn2D() const
{
    return std::ranges::all_of(*this, [](auto& function) { return function->isRepresentableIn2D(); });
}

bool TransformList::affectedByTransformOrigin() const
{
    return std::ranges::any_of(*this, [](auto& function) { return function->isAffectedByTransformOrigin(); });
}

bool TransformList::isInvertible(const LayoutSize& size) const
{
    TransformationMatrix transform;
    apply(transform, size);
    return transform.isInvertible();
}

bool TransformList::containsNonInvertibleMatrix(const LayoutSize& boxSize) const
{
    return (hasTransformOfType<TransformOperation::Type::Matrix>() || hasTransformOfType<TransformOperation::Type::Matrix3D>()) && !isInvertible(boxSize);
}

// MARK: - Blending

static bool shouldFallBackToDiscreteInterpolation(const TransformList& from, const TransformList& to, const LayoutSize& boxSize)
{
    return from.containsNonInvertibleMatrix(boxSize) || to.containsNonInvertibleMatrix(boxSize);
}

auto Blending<TransformList>::canBlend(const TransformList& from, const TransformList& to, CompositeOperation compositeOperation) -> bool
{
    if (compositeOperation == CompositeOperation::Replace)
        return !shouldFallBackToDiscreteInterpolation(from, to, { });
    return true;
}

auto Blending<TransformList>::blend(const TransformList& from, const TransformList& to, const Interpolation::Context& context) -> TransformList
{
    unsigned fromLength = from.size();
    unsigned toLength = to.size();
    unsigned maxLength = std::max(fromLength, toLength);

    if (context.compositeOperation == CompositeOperation::Add) {
        ASSERT(context.progress == 1.0);

        return TransformList::Container::createWithSizeFromGenerator(fromLength + toLength, [&](auto index) {
            if (index < fromLength)
                return from[index];
            return to[index - fromLength];
        });
    }

    auto* renderBox = dynamicDowncast<RenderBox>(context.client.renderer());
    auto boxSize = renderBox ? renderBox->borderBoxRect().size() : LayoutSize();

    bool shouldFallBackToDiscrete = shouldFallBackToDiscreteInterpolation(from, to, boxSize);

    auto createBlendedMatrixOperationFromOperationsSuffix = [&](unsigned i) -> TransformFunction {
        TransformationMatrix fromTransform;
        from.apply(fromTransform, boxSize, i);

        TransformationMatrix toTransform;
        to.apply(toTransform, boxSize, i);

        auto progress = context.progress;
        auto compositeOperation = context.compositeOperation;
        if (shouldFallBackToDiscrete) {
            progress = progress < 0.5 ? 0 : 1;
            compositeOperation = CompositeOperation::Replace;
        }

        toTransform.blend(fromTransform, progress, compositeOperation);
        return TransformFunction { Matrix3DTransformOperation::create(toTransform) };
    };

    auto createBlendedOperation = [&](unsigned i) -> TransformFunction {
        RefPtr<TransformOperation> fromOperation = (i < fromLength) ? from.m_value[i].value.ptr() : nullptr;
        RefPtr<TransformOperation> toOperation = (i < toLength) ? to.m_value[i].value.ptr() : nullptr;

        if (fromOperation && toOperation)
            return TransformFunction { toOperation->blend(fromOperation.get(), context) };
        if (!fromOperation)
            return TransformFunction { toOperation->blend(nullptr, 1 - context.progress, true) };
        return TransformFunction { fromOperation->blend(nullptr, context, true) };
    };

    if (shouldFallBackToDiscrete)
        return TransformList { createBlendedMatrixOperationFromOperationsSuffix(0) };

    auto prefixLengthFromContext = [&] -> std::optional<unsigned> {
        // We cannot use the pre-computed prefix when dealing with accumulation
        // since the values used to accumulate may be different than those held
        // in the initial keyframe list. We must do the same with any property
        // other than "transform" since we only pre-compute the prefix for that
        // property.
        if (context.compositeOperation == CompositeOperation::Accumulate
            || std::holds_alternative<AtomString>(context.property)
            || std::get<CSSPropertyID>(context.property) != CSSPropertyTransform)
            return std::nullopt;
        return context.client.transformFunctionListPrefix();
    }();

    auto prefixLength = [&] -> std::optional<unsigned> {
        // If either of the transform list is empty, then we should not attempt to do a matrix blend.
        if (!fromLength || !toLength)
            return { };

        for (unsigned i = 0; i < maxLength; i++) {
            if (prefixLengthFromContext && i >= *prefixLengthFromContext)
                return i;

            RefPtr<TransformOperation> fromOperation = (i < fromLength) ? from.m_value[i].value.ptr() : nullptr;
            RefPtr<TransformOperation> toOperation = (i < toLength) ? to.m_value[i].value.ptr() : nullptr;
            if (fromOperation && toOperation && !fromOperation->sharedPrimitiveType(toOperation.get()))
                return i;
        }

        return { };
    }();

    if (prefixLength) {
        return TransformList::Container::createWithSizeFromGenerator(*prefixLength + 1, [&](auto i) {
            if (i == *prefixLength)
                return createBlendedMatrixOperationFromOperationsSuffix(i);
            return createBlendedOperation(i);
        });
    }

    return TransformList::Container::createWithSizeFromGenerator(maxLength, [&](auto i) {
        return createBlendedOperation(i);
    });
}

} // namespace Style
} // namespace WebCore
