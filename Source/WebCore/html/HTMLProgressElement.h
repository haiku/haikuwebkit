/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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
 *
 */

#pragma once

#include "HTMLElement.h"

namespace WebCore {

class ProgressValueElement;
class RenderProgress;

class HTMLProgressElement final : public HTMLElement {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(HTMLProgressElement);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(HTMLProgressElement);
public:
    static const double IndeterminatePosition;
    static const double InvalidPosition;

    static Ref<HTMLProgressElement> create(const QualifiedName&, Document&);

    double value() const;

    double max() const;
    void setMax(double);

    double position() const;

    bool isDevolvableWidget() const override { return true; }

private:
    HTMLProgressElement(const QualifiedName&, Document&);
    virtual ~HTMLProgressElement();

    bool matchesIndeterminatePseudoClass() const final;
    bool isLabelable() const final { return true; }

    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;
    RenderProgress* renderProgress() const;

    void attributeChanged(const QualifiedName&, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason) final;

    void didAttachRenderers() final;

    void updateDeterminateState();
    void didElementStateChange();
    void didAddUserAgentShadowRoot(ShadowRoot&) final;
    bool isDeterminate() const { return m_isDeterminate; };

    bool canContainRangeEndPoint() const final { return false; }

    RefPtr<ProgressValueElement> protectedValueElement();

    WeakPtr<ProgressValueElement, WeakPtrImplWithEventTargetData> m_valueElement;
    bool m_isDeterminate { false };
};

} // namespace
