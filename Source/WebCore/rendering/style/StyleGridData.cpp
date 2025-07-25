/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 *  THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 *  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 *  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include "StyleGridData.h"

#include "RenderStyleInlines.h"
#include "RenderStyleDifference.h"

namespace WebCore {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleGridData);

StyleGridData::StyleGridData()
    : gridAutoFlow(RenderStyle::initialGridAutoFlow())
    , gridAutoRows(RenderStyle::initialGridAutoRows())
    , gridAutoColumns(RenderStyle::initialGridAutoColumns())
    , gridTemplateAreas(RenderStyle::initialGridTemplateAreas())
    , m_gridColumnTrackSizes(RenderStyle::initialGridColumnTrackSizes())
    , m_gridRowTrackSizes(RenderStyle::initialGridRowTrackSizes())
    , m_namedGridColumnLines(RenderStyle::initialNamedGridColumnLines())
    , m_namedGridRowLines(RenderStyle::initialNamedGridRowLines())
    , m_orderedNamedGridColumnLines(RenderStyle::initialOrderedNamedGridColumnLines())
    , m_orderedNamedGridRowLines(RenderStyle::initialOrderedNamedGridRowLines())
    , m_autoRepeatNamedGridColumnLines(RenderStyle::initialNamedGridColumnLines())
    , m_autoRepeatNamedGridRowLines(RenderStyle::initialNamedGridRowLines())
    , m_autoRepeatOrderedNamedGridColumnLines(RenderStyle::initialOrderedNamedGridColumnLines())
    , m_autoRepeatOrderedNamedGridRowLines(RenderStyle::initialOrderedNamedGridRowLines())
    , m_gridAutoRepeatColumns(RenderStyle::initialGridAutoRepeatTracks())
    , m_gridAutoRepeatRows(RenderStyle::initialGridAutoRepeatTracks())
    , m_autoRepeatColumnsInsertionPoint(RenderStyle::initialGridAutoRepeatInsertionPoint())
    , m_autoRepeatRowsInsertionPoint(RenderStyle::initialGridAutoRepeatInsertionPoint())
    , m_autoRepeatColumnsType(RenderStyle::initialGridAutoRepeatType())
    , m_autoRepeatRowsType(RenderStyle::initialGridAutoRepeatType())
    , m_subgridRows(false)
    , m_subgridColumns(false)
    , m_masonryRows(false)
    , m_masonryColumns(false)
{
}

inline StyleGridData::StyleGridData(const StyleGridData& o)
    : RefCounted<StyleGridData>()
    , gridAutoFlow(o.gridAutoFlow)
    , gridAutoRows(o.gridAutoRows)
    , gridAutoColumns(o.gridAutoColumns)
    , gridTemplateAreas(o.gridTemplateAreas)
    , m_columns(o.m_columns)
    , m_rows(o.m_rows)
    , m_gridColumnTrackSizes(o.m_gridColumnTrackSizes)
    , m_gridRowTrackSizes(o.m_gridRowTrackSizes)
    , m_namedGridColumnLines(o.m_namedGridColumnLines)
    , m_namedGridRowLines(o.m_namedGridRowLines)
    , m_orderedNamedGridColumnLines(o.m_orderedNamedGridColumnLines)
    , m_orderedNamedGridRowLines(o.m_orderedNamedGridRowLines)
    , m_autoRepeatNamedGridColumnLines(o.m_autoRepeatNamedGridColumnLines)
    , m_autoRepeatNamedGridRowLines(o.m_autoRepeatNamedGridRowLines)
    , m_autoRepeatOrderedNamedGridColumnLines(o.m_autoRepeatOrderedNamedGridColumnLines)
    , m_autoRepeatOrderedNamedGridRowLines(o.m_autoRepeatOrderedNamedGridRowLines)
    , m_gridAutoRepeatColumns(o.m_gridAutoRepeatColumns)
    , m_gridAutoRepeatRows(o.m_gridAutoRepeatRows)
    , m_autoRepeatColumnsInsertionPoint(o.m_autoRepeatColumnsInsertionPoint)
    , m_autoRepeatRowsInsertionPoint(o.m_autoRepeatRowsInsertionPoint)
    , m_autoRepeatColumnsType(o.m_autoRepeatColumnsType)
    , m_autoRepeatRowsType(o.m_autoRepeatRowsType)
    , m_subgridRows(o.m_subgridRows)
    , m_subgridColumns(o.m_subgridColumns)
    , m_masonryRows(o.m_masonryRows)
    , m_masonryColumns(o.m_masonryColumns)
{
}

bool StyleGridData::operator==(const StyleGridData& o) const
{
    return m_columns == o.m_columns
        && m_rows == o.m_rows
        && gridAutoFlow == o.gridAutoFlow
        && gridAutoRows == o.gridAutoRows
        && gridAutoColumns == o.gridAutoColumns
        && gridTemplateAreas == o.gridTemplateAreas
        && m_masonryRows == o.m_masonryRows
        && m_masonryColumns == o.m_masonryColumns;
}

void StyleGridData::setRows(const GridTrackList& list)
{
    m_rows = list;
    computeCachedTrackData(m_rows, m_gridRowTrackSizes, m_namedGridRowLines, m_orderedNamedGridRowLines, m_gridAutoRepeatRows, m_autoRepeatNamedGridRowLines, m_autoRepeatOrderedNamedGridRowLines, m_autoRepeatRowsInsertionPoint, m_autoRepeatRowsType, m_subgridRows, m_masonryRows);
}

void StyleGridData::setColumns(const GridTrackList& list)
{
    m_columns = list;
    computeCachedTrackData(m_columns, m_gridColumnTrackSizes, m_namedGridColumnLines, m_orderedNamedGridColumnLines, m_gridAutoRepeatColumns, m_autoRepeatNamedGridColumnLines, m_autoRepeatOrderedNamedGridColumnLines, m_autoRepeatColumnsInsertionPoint, m_autoRepeatColumnsType, m_subgridColumns, m_masonryColumns);
}

static void createGridLineNamesList(const Vector<String>& names, unsigned currentNamedGridLine, Style::GridNamedLinesMap& namedGridLines, Style::GridOrderedNamedLinesMap& orderedNamedGridLines)
{
    auto orderedResult = orderedNamedGridLines.map.add(currentNamedGridLine, Vector<String>());

    for (auto& name : names) {
        auto result = namedGridLines.map.add(name, Vector<unsigned>());
        result.iterator->value.append(currentNamedGridLine);

        orderedResult.iterator->value.append(name);
    }
}

void StyleGridData::computeCachedTrackData(const GridTrackList& list, Vector<GridTrackSize>& sizes, Style::GridNamedLinesMap& namedLines, Style::GridOrderedNamedLinesMap& orderedNamedLines, Vector<GridTrackSize>& autoRepeatSizes, Style::GridNamedLinesMap& autoRepeatNamedLines, Style::GridOrderedNamedLinesMap& autoRepeatOrderedNamedLines, unsigned& autoRepeatInsertionPoint, AutoRepeatType& autoRepeatType, bool& subgrid, bool& masonry)
{
    sizes.clear();
    namedLines.map.clear();
    orderedNamedLines.map.clear();
    autoRepeatSizes.clear();
    autoRepeatNamedLines.map.clear();
    autoRepeatOrderedNamedLines.map.clear();
    autoRepeatInsertionPoint = RenderStyle::initialGridAutoRepeatInsertionPoint();
    autoRepeatType = RenderStyle::initialGridAutoRepeatType();
    subgrid = false;

    if (list.list.isEmpty())
        return;

    unsigned currentNamedGridLine = 0;
    unsigned autoRepeatIndex = 0;
    auto visitor = WTF::makeVisitor([&](const GridTrackSize& size) {
        ++currentNamedGridLine;
        sizes.append(size);
    }, [&](const Vector<String>& names) {
        createGridLineNamesList(names, currentNamedGridLine, namedLines, orderedNamedLines);
        // Subgrids only have line names defined, not track sizes, so we want our count
        // to be the number of lines named rather than number of sized tracks.
        if (subgrid)
            currentNamedGridLine++;
    }, [&](const GridTrackEntryRepeat& repeat) {
        for (size_t i = 0; i < repeat.repeats; ++i) {
            for (auto& repeatEntry : repeat.list) {
                if (std::holds_alternative<Vector<String>>(repeatEntry)) {
                    createGridLineNamesList(std::get<Vector<String>>(repeatEntry), currentNamedGridLine, namedLines, orderedNamedLines);
                    // Subgrids only have line names defined, not track sizes, so we want our count
                    // to be the number of lines named rather than number of sized tracks.
                    if (subgrid)
                        currentNamedGridLine++;
                } else {
                    ++currentNamedGridLine;
                    sizes.append(std::get<GridTrackSize>(repeatEntry));
                }
            }
        }
    }, [&](const GridTrackEntryAutoRepeat& repeat) {
        ASSERT(!autoRepeatIndex);
        autoRepeatIndex = 0;
        autoRepeatType = repeat.type;
        for (auto& autoRepeatEntry : repeat.list) {
            if (std::holds_alternative<Vector<String>>(autoRepeatEntry)) {
                createGridLineNamesList(std::get<Vector<String>>(autoRepeatEntry), autoRepeatIndex, autoRepeatNamedLines, autoRepeatOrderedNamedLines);
                if (subgrid)
                    ++autoRepeatIndex;
                continue;
            }
            ++autoRepeatIndex;
            autoRepeatSizes.append(std::get<GridTrackSize>(autoRepeatEntry));
        }
        autoRepeatInsertionPoint = currentNamedGridLine;
        if (!subgrid)
            currentNamedGridLine++;
    }, [&](const GridTrackEntrySubgrid&) {
        subgrid = true;
    }, [&](const GridTrackEntryMasonry&) {
        masonry = true;
    });

    for (const auto& entry : list.list)
        WTF::visit(visitor, entry);

    // The parser should have rejected any <track-list> without any <track-size> as
    // this is not conformant to the syntax.
    ASSERT(!sizes.isEmpty() || !autoRepeatSizes.isEmpty() || subgrid || masonry);
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const RepeatEntry& entry)
{
    auto visitor = WTF::makeVisitor([&](const GridTrackSize& size) {
        ts << size;
    }, [&](const Vector<String>& names) {
        ts << names;
    });

    WTF::visit(visitor, entry);
    return ts;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const GridTrackEntry& entry)
{
    auto visitor = WTF::makeVisitor([&](const GridTrackSize& size) {
        ts << size;
    }, [&](const Vector<String>& names) {
        ts << names;
    }, [&](const GridTrackEntryRepeat& repeat) {
        ts << "repeat("_s << repeat.repeats << ", "_s << repeat.list << ')';
    }, [&](const GridTrackEntryAutoRepeat& repeat) {
        ts << "repeat("_s << repeat.type << ", "_s << repeat.list << ')';
    }, [&](const GridTrackEntrySubgrid&) {
        ts << "subgrid"_s;
    }, [&](const GridTrackEntryMasonry&) {
        ts << "masonry"_s;
    });

    WTF::visit(visitor, entry);
    return ts;
}

Ref<StyleGridData> StyleGridData::copy() const
{
    return adoptRef(*new StyleGridData(*this));
}

#if !LOG_DISABLED
void StyleGridData::dumpDifferences(TextStream& ts, const StyleGridData& other) const
{
    LOG_IF_DIFFERENT(m_columns);
    LOG_IF_DIFFERENT(m_rows);

    LOG_IF_DIFFERENT(m_gridColumnTrackSizes);
    LOG_IF_DIFFERENT(m_gridRowTrackSizes);

    LOG_IF_DIFFERENT(m_namedGridColumnLines);
    LOG_IF_DIFFERENT(m_namedGridColumnLines);

    // m_orderedNamedGridColumnLines and m_orderedNamedGridRowLines are not part of the diff.

    LOG_IF_DIFFERENT(m_autoRepeatNamedGridColumnLines);
    LOG_IF_DIFFERENT(m_autoRepeatNamedGridRowLines);

    // m_autoRepeatOrderedNamedGridColumnLines and m_autoRepeatOrderedNamedGridRowLines are not part of the diff.

    LOG_IF_DIFFERENT(m_gridAutoRepeatColumns);
    LOG_IF_DIFFERENT(m_gridAutoRepeatRows);

    LOG_IF_DIFFERENT(m_autoRepeatColumnsInsertionPoint);
    LOG_IF_DIFFERENT(m_autoRepeatRowsInsertionPoint);

    LOG_IF_DIFFERENT(m_autoRepeatColumnsType);
    LOG_IF_DIFFERENT(m_autoRepeatRowsType);

    LOG_IF_DIFFERENT(m_subgridRows);
    LOG_IF_DIFFERENT(m_subgridColumns);

    LOG_IF_DIFFERENT(m_masonryRows);
    LOG_IF_DIFFERENT(m_masonryRows);
}
#endif // !LOG_DISABLED

} // namespace WebCore
