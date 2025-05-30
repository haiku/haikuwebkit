/*
 * Copyright (C) 2023 Apple Inc.  All rights reserved.
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

#include <Shape.h>

#include "PathImpl.h"
#include "PathStream.h"
#include "PlatformPath.h"
#include "WindRule.h"

namespace WebCore {

class GraphicsContext;

class PathHaiku final: public PathImpl {
public:
    static Ref<PathHaiku> create(std::span<const PathSegment> = { });
    static Ref<PathHaiku> create(const PathSegment&);
    static Ref<PathHaiku> create(const BShape&, RefPtr<PathStream>&& = nullptr);
    static PlatformPathPtr emptyPlatformPath();

    PathHaiku();
    PathHaiku(const BShape&, RefPtr<PathStream>&&);
    ~PathHaiku();

    PlatformPathPtr platformPath() const;

    void addPath(const PathHaiku&, const AffineTransform&);

    bool applyElements(const PathElementApplier&) const;

    bool transform(const AffineTransform&);

    bool contains(const FloatPoint&, WindRule) const;
    bool strokeContains(const FloatPoint&, const Function<void(GraphicsContext&)>& strokeStyleApplier) const;

    FloatRect strokeBoundingRect(const Function<void(GraphicsContext&)>& strokeStyleApplier) const;

private:
    Ref<PathImpl> copy() const final;

    void add(PathMoveTo) final;

    void add(PathLineTo) final;
    void add(PathQuadCurveTo) final;
    void add(PathBezierCurveTo) final;
    void add(PathArcTo) final;

    void add(PathArc) final;
    void add(PathClosedArc) final;
    void add(PathEllipse) final;
    void add(PathEllipseInRect) final;
    void add(PathRect) final;
    void add(PathRoundedRect) final;
    void add(PathContinuousRoundedRect) final;

    void add(PathCloseSubpath) final;

    void applySegments(const PathSegmentApplier&) const final;

    FloatPoint currentPoint() const final;

    FloatRect fastBoundingRect() const final;
    FloatRect boundingRect() const final;

    bool definitelyEqual(const WebCore::PathImpl& other) const { return m_platformPath == ((PathHaiku&)other).m_platformPath; }

    BShape m_platformPath;
    RefPtr<PathStream> m_elementsStream;
};

};

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::PathHaiku)
    static bool isType(const WebCore::PathImpl& pathImpl) { return !pathImpl.isPathStream(); }
SPECIALIZE_TYPE_TRAITS_END()
