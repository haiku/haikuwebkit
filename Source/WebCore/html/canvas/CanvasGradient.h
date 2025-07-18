/*
 * Copyright (C) 2006-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
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

#pragma once

#include "FloatPoint.h"

namespace WebCore {

class Gradient;
class ScriptExecutionContext;
template<typename> class ExceptionOr;

class CanvasGradient : public RefCounted<CanvasGradient> {
public:
    static Ref<CanvasGradient> create(const FloatPoint& p0, const FloatPoint& p1);
    static Ref<CanvasGradient> create(const FloatPoint& p0, float r0, const FloatPoint& p1, float r1);
    static Ref<CanvasGradient> create(const FloatPoint& centerPoint, float angleInRadians);
    ~CanvasGradient();

    Gradient& gradient() { return m_gradient; }
    const Gradient& gradient() const { return m_gradient; }

    ExceptionOr<void> addColorStop(ScriptExecutionContext&, double value, const String& color);

private:
    CanvasGradient(const FloatPoint& p0, const FloatPoint& p1);
    CanvasGradient(const FloatPoint& p0, float r0, const FloatPoint& p1, float r1);
    CanvasGradient(const FloatPoint& centerPoint, float angleInRadians);

    const Ref<Gradient> m_gradient;
};

} // namespace WebCore
