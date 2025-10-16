/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "UnplacedGridItem.h"

#include "LayoutElementBox.h"
#include "RenderStyleInlines.h"

namespace WebCore {
namespace Layout {
UnplacedGridItem::UnplacedGridItem(const ElementBox& layoutBox, Style::GridPosition columnStart, Style::GridPosition columnEnd,
    Style::GridPosition rowStart, Style::GridPosition rowEnd)
    : m_layoutBox(layoutBox)
    , m_columnPosition({ columnStart, columnEnd })
    , m_rowPosition({ rowStart, rowEnd })
{
}

UnplacedGridItem::UnplacedGridItem(WTF::HashTableEmptyValueType)
    : m_layoutBox(WTF::HashTableEmptyValue)
    , m_columnPosition({ RenderStyle::initialGridItemColumnStart(), RenderStyle::initialGridItemColumnEnd() })
    , m_rowPosition({ RenderStyle::initialGridItemRowStart(), RenderStyle::initialGridItemRowEnd() })
{
}

int UnplacedGridItem::explicitColumnStart() const
{
    ASSERT(m_columnPosition.first.isExplicit());
    auto explicitColumnStart = m_columnPosition.first.explicitPosition();
    if (explicitColumnStart > 0)
        return explicitColumnStart - 1;

    ASSERT_NOT_IMPLEMENTED_YET();
    return { };
}

int UnplacedGridItem::explicitColumnEnd() const
{
    ASSERT(m_columnPosition.second.isExplicit());
    auto explicitColumnEnd = m_columnPosition.second.explicitPosition();
    if (explicitColumnEnd > 0)
        return explicitColumnEnd - 1;

    ASSERT_NOT_IMPLEMENTED_YET();
    return { };
}

int UnplacedGridItem::explicitRowStart() const
{
    ASSERT(m_rowPosition.first.isExplicit());
    auto explicitRowStart = m_rowPosition.first.explicitPosition();
    if (explicitRowStart > 0)
        return explicitRowStart - 1;

    ASSERT_NOT_IMPLEMENTED_YET();
    return { };
}

int UnplacedGridItem::explicitRowEnd() const
{
    ASSERT(m_rowPosition.second.isExplicit());
    auto explicitRowEnd = m_rowPosition.second.explicitPosition();
    if (explicitRowEnd > 0)
        return explicitRowEnd - 1;

    ASSERT_NOT_IMPLEMENTED_YET();
    return { };
}

bool UnplacedGridItem::operator==(const UnplacedGridItem& other) const
{
    // Since the hash table empty value uses CheckedRef's empty value,
    // we need to check if either |this| or |other| are the empty value
    // so we do not compare the uninitialized ref.
    bool isEmpty = isHashTableEmptyValue();
    if (isEmpty)
        return other.isHashTableEmptyValue();
    if (other.isHashTableEmptyValue())
        return isEmpty;

    return m_layoutBox.ptr() == other.m_layoutBox.ptr() && m_columnPosition == other.m_columnPosition && m_rowPosition == other.m_rowPosition;
}

void add(Hasher& hasher, const WebCore::Layout::UnplacedGridItem& unplacedGridItem)
{
    addArgs(hasher, unplacedGridItem.m_layoutBox.ptr(), unplacedGridItem.m_columnPosition, unplacedGridItem.m_rowPosition);
}

} // namespace Layout
} // namespace WebCore

