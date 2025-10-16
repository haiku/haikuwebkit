/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2012-2014 Google Inc. All rights reserved.
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

#pragma once

#include <wtf/Forward.h>

namespace WebCore {

class Animation;
class CSSValue;

namespace Style {
class BuilderState;
}

class CSSToStyleMap {
public:
    explicit CSSToStyleMap(Style::BuilderState&);

    void mapAnimationDelay(Animation&, const CSSValue&);
    static void mapAnimationDirection(Animation&, const CSSValue&);
    void mapAnimationDuration(Animation&, const CSSValue&);
    static void mapAnimationFillMode(Animation&, const CSSValue&);
    void mapAnimationIterationCount(Animation&, const CSSValue&);
    void mapAnimationName(Animation&, const CSSValue&);
    static void mapAnimationPlayState(Animation&, const CSSValue&);
    static void mapAnimationProperty(Animation&, const CSSValue&);
    void mapAnimationTimeline(Animation&, const CSSValue&);
    void mapAnimationTimingFunction(Animation&, const CSSValue&);
    static void mapAnimationCompositeOperation(Animation&, const CSSValue&);
    static void mapAnimationAllowsDiscreteTransitions(Animation&, const CSSValue&);
    void mapAnimationRangeStart(Animation&, const CSSValue&);
    void mapAnimationRangeEnd(Animation&, const CSSValue&);

private:
    Style::BuilderState& m_builderState;
};

} // namespace WebCore
