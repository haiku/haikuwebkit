/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013, 2014 Igalia S.L.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "StyleGridTrackBreadth.h"

namespace WebCore {

using namespace CSS::Literals;

enum class GridTrackSizeType : uint8_t {
    Length,
    MinMax,
    FitContent
};

// This class represents a <track-size> from the spec. Although there are 3 different types of
// <track-size> there is always an equivalent minmax() representation that could represent any of
// them. The only special case is fit-content(argument) which is similar to minmax(auto,
// max-content) except that the track size is clamped at argument if it is greater than the auto
// minimum. At the GridTrackSize level we don't need to worry about clamping so we treat that case
// exactly as auto.
//
// We're using a separate attribute to store fit-content argument even though we could directly use
// m_maxTrackBreadth. The reason why we don't do it is because the maxTrackBreadth() call is a hot
// spot, so adding a conditional statement there (to distinguish between fit-content and any other
// case) was causing a severe performance drop.
class GridTrackSize {
public:
    GridTrackSize(const Style::GridTrackBreadth& breadth = Style::GridTrackBreadth(CSS::Keyword::Auto { }), GridTrackSizeType trackSizeType = GridTrackSizeType::Length)
        : m_type(trackSizeType)
        , m_minTrackBreadth(trackSizeType == GridTrackSizeType::FitContent ? Style::GridTrackBreadth(CSS::Keyword::Auto { }) : breadth)
        , m_maxTrackBreadth(trackSizeType == GridTrackSizeType::FitContent ? Style::GridTrackBreadth(CSS::Keyword::Auto { }) : breadth)
        , m_fitContentTrackBreadth(trackSizeType == GridTrackSizeType::FitContent ? breadth : Style::GridTrackBreadth(0_css_px))
    {
        ASSERT(trackSizeType == GridTrackSizeType::Length || trackSizeType == GridTrackSizeType::FitContent);
        ASSERT(trackSizeType != GridTrackSizeType::FitContent || breadth.isLength());
        cacheMinMaxTrackBreadthTypes();
    }

    GridTrackSize(const Style::GridTrackBreadth& minTrackBreadth, const Style::GridTrackBreadth& maxTrackBreadth)
        : m_type(GridTrackSizeType::MinMax)
        , m_minTrackBreadth(minTrackBreadth)
        , m_maxTrackBreadth(maxTrackBreadth)
        , m_fitContentTrackBreadth(0_css_px)
    {
        cacheMinMaxTrackBreadthTypes();
    }

    GridTrackSize(Style::GridTrackBreadth&& minTrackBreadth, Style::GridTrackBreadth&& maxTrackBreadth)
        : m_type(GridTrackSizeType::MinMax)
        , m_minTrackBreadth(WTFMove(minTrackBreadth))
        , m_maxTrackBreadth(WTFMove(maxTrackBreadth))
        , m_fitContentTrackBreadth(0_css_px)
    {
        cacheMinMaxTrackBreadthTypes();
    }

    const Style::GridTrackBreadth& fitContentTrackBreadth() const
    {
        ASSERT(m_type == GridTrackSizeType::FitContent);
        return m_fitContentTrackBreadth;
    }

    const Style::GridTrackBreadth& minTrackBreadth() const { return m_minTrackBreadth; }
    const Style::GridTrackBreadth& maxTrackBreadth() const { return m_maxTrackBreadth; }

    GridTrackSizeType type() const { return m_type; }

    bool isContentSized() const { return m_minTrackBreadth.isContentSized() || m_maxTrackBreadth.isContentSized(); }
    bool isFitContent() const { return m_type == GridTrackSizeType::FitContent; }

    bool operator==(const GridTrackSize& other) const
    {
        return m_type == other.m_type
            && m_minTrackBreadth == other.m_minTrackBreadth
            && m_maxTrackBreadth == other.m_maxTrackBreadth
            && m_fitContentTrackBreadth == other.m_fitContentTrackBreadth;
    }

    void cacheMinMaxTrackBreadthTypes()
    {
        m_minTrackBreadthIsAuto = minTrackBreadth().isLength() && minTrackBreadth().length().isAuto();
        m_minTrackBreadthIsMinContent = minTrackBreadth().isLength() && minTrackBreadth().length().isMinContent();
        m_minTrackBreadthIsMaxContent = minTrackBreadth().isLength() && minTrackBreadth().length().isMaxContent();
        m_maxTrackBreadthIsMaxContent = maxTrackBreadth().isLength() && maxTrackBreadth().length().isMaxContent();
        m_maxTrackBreadthIsMinContent = maxTrackBreadth().isLength() && maxTrackBreadth().length().isMinContent();
        m_maxTrackBreadthIsAuto = maxTrackBreadth().isLength() && maxTrackBreadth().length().isAuto();
        m_maxTrackBreadthIsFixed = maxTrackBreadth().isLength() && maxTrackBreadth().length().isSpecified();

        // These values depend on the above ones so keep them here.
        m_minTrackBreadthIsIntrinsic = m_minTrackBreadthIsMaxContent || m_minTrackBreadthIsMinContent
            || m_minTrackBreadthIsAuto || isFitContent();
        m_maxTrackBreadthIsIntrinsic = m_maxTrackBreadthIsMaxContent || m_maxTrackBreadthIsMinContent
            || m_maxTrackBreadthIsAuto || isFitContent();
    }

    bool hasIntrinsicMinTrackBreadth() const { return m_minTrackBreadthIsIntrinsic; }
    bool hasIntrinsicMaxTrackBreadth() const { return m_maxTrackBreadthIsIntrinsic; }
    bool hasMinOrMaxContentMinTrackBreadth() const { return m_minTrackBreadthIsMaxContent || m_minTrackBreadthIsMinContent; }
    bool hasAutoMinTrackBreadth() const { return m_minTrackBreadthIsAuto; }
    bool hasAutoMaxTrackBreadth() const { return m_maxTrackBreadthIsAuto; }
    bool hasMaxContentMaxTrackBreadth() const { return m_maxTrackBreadthIsMaxContent; }
    bool hasMaxContentOrAutoMaxTrackBreadth() const { return m_maxTrackBreadthIsMaxContent || m_maxTrackBreadthIsAuto; }
    bool hasMinContentMaxTrackBreadth() const { return m_maxTrackBreadthIsMinContent; }
    bool hasMinOrMaxContentMaxTrackBreadth() const { return m_maxTrackBreadthIsMaxContent || m_maxTrackBreadthIsMinContent; }
    bool hasMaxContentMinTrackBreadth() const { return m_minTrackBreadthIsMaxContent; }
    bool hasMinContentMinTrackBreadth() const { return m_minTrackBreadthIsMinContent; }
    bool hasMaxContentMinTrackBreadthAndMaxContentMaxTrackBreadth() const { return m_minTrackBreadthIsMaxContent && m_maxTrackBreadthIsMaxContent; }
    bool hasAutoOrMinContentMinTrackBreadthAndIntrinsicMaxTrackBreadth() const { return (m_minTrackBreadthIsMinContent || m_minTrackBreadthIsAuto) && m_maxTrackBreadthIsIntrinsic; }
    bool hasFixedMaxTrackBreadth() const { return m_maxTrackBreadthIsFixed; }

private:
    GridTrackSizeType m_type;
    Style::GridTrackBreadth m_minTrackBreadth;
    Style::GridTrackBreadth m_maxTrackBreadth;
    Style::GridTrackBreadth m_fitContentTrackBreadth;

    bool m_minTrackBreadthIsAuto : 1;
    bool m_maxTrackBreadthIsAuto : 1;
    bool m_minTrackBreadthIsMaxContent : 1;
    bool m_minTrackBreadthIsMinContent : 1;
    bool m_maxTrackBreadthIsMaxContent : 1;
    bool m_maxTrackBreadthIsMinContent : 1;
    bool m_minTrackBreadthIsIntrinsic : 1;
    bool m_maxTrackBreadthIsIntrinsic : 1;
    bool m_maxTrackBreadthIsFixed : 1;
};

WTF::TextStream& operator<<(WTF::TextStream&, const GridTrackSize&);

} // namespace WebCore
