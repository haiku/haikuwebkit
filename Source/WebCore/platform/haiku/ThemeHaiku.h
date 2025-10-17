/*
 * Copyright (C) 2023 Sony Interactive Entertainment Inc.
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

#pragma once

#include "Theme.h"

#include "Color.h"
#include "FloatRect.h"
#include "FontCascade.h"
#include "StyleAppearance.h"

#include <InterfaceDefs.h>

namespace WebCore {

class Path;

class ThemeHaiku final : public Theme {
public:
    enum class PaintRounded : bool { No, Yes };

    static void paintFocus(GraphicsContext&, const FloatRect&, int offset, const Color&, PaintRounded = PaintRounded::No);
    static void paintFocus(GraphicsContext&, const Path&, const Color&);
    static void paintFocus(GraphicsContext&, const Vector<FloatRect>&, const Color&, PaintRounded = PaintRounded::No);
    enum class ArrowDirection { Up, Down };
    static void paintArrow(GraphicsContext&, const FloatRect&, ArrowDirection, bool);
private:
    static rgb_color colorForValue(color_which colorConstant, bool useDarkAppearance);

    static Color focusColor(const Color&);

    Color m_accentColor { SRGBA<uint8_t> { 52, 132, 228 } };

    friend NeverDestroyed<ThemeHaiku>;
};

} // namespace WebCore
