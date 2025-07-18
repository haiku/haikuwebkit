/*
 * Copyright (C) Research In Motion Limited 2010-11. All rights reserved.
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 * Copyright (C) 2015 Google Inc. All rights reserved.
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
#include "SVGTextLayoutAttributes.h"

#include "RenderSVGInlineText.h"
#include <stdio.h>
#include <wtf/text/CString.h>

namespace WebCore {

SVGTextLayoutAttributes::SVGTextLayoutAttributes(RenderSVGInlineText& context)
    : m_context(context)
{
}

void SVGTextLayoutAttributes::clear()
{
    m_characterDataMap.clear();
    m_textMetricsValues.shrink(0);
}

RenderSVGInlineText& SVGTextLayoutAttributes::context() { return m_context.get(); }
const RenderSVGInlineText& SVGTextLayoutAttributes::context() const { return m_context.get(); }
}
