/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2010-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2016 Google Inc. All rights reserved.
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
#include "SVGAElement.h"

#include "DOMTokenList.h"
#include "Document.h"
#include "EventHandler.h"
#include "FrameLoader.h"
#include "FrameLoaderTypes.h"
#include "HTMLAnchorElement.h"
#include "KeyboardEvent.h"
#include "LegacyRenderSVGTransformableContainer.h"
#include "LocalFrame.h"
#include "MouseEvent.h"
#include "PlatformMouseEvent.h"
#include "RenderObjectInlines.h"
#include "RenderSVGInline.h"
#include "RenderSVGText.h"
#include "RenderSVGTransformableContainer.h"
#include "ResourceRequest.h"
#include "SVGElementInlines.h"
#include "SVGElementTypeHelpers.h"
#include "SVGNames.h"
#include "SVGSMILElement.h"
#include "XLinkNames.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(SVGAElement);

inline SVGAElement::SVGAElement(const QualifiedName& tagName, Document& document)
    : SVGGraphicsElement(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
    , SVGURIReference(this)
{
    ASSERT(hasTagName(SVGNames::aTag));

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::targetAttr, &SVGAElement::m_target>();
    });
}

SVGAElement::~SVGAElement() = default;

Ref<SVGAElement> SVGAElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGAElement(tagName, document));
}

String SVGAElement::title() const
{
    // If the xlink:title is set (non-empty string), use it.
    const AtomString& title = attributeWithoutSynchronization(XLinkNames::titleAttr);
    if (!title.isEmpty())
        return title;

    // Otherwise, use the title of this element.
    return SVGElement::title();
}

void SVGAElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    if (name == SVGNames::targetAttr) {
        Ref { m_target }->setBaseValInternal(newValue);
        return;
    } else if (name == SVGNames::relAttr) {
        if (m_relList)
            m_relList->associatedAttributeValueChanged();
    }

    SVGURIReference::parseAttribute(name, newValue);
    SVGGraphicsElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

void SVGAElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (SVGURIReference::isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);
        setIsLink(!href().isNull() && !shouldProhibitLinks(this));
        return;
    }

    SVGGraphicsElement::svgAttributeChanged(attrName);
}

RenderPtr<RenderElement> SVGAElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    RefPtr svgParent = dynamicDowncast<SVGElement>(parentNode());
    if (svgParent && svgParent->isTextContent())
        return createRenderer<RenderSVGInline>(RenderObject::Type::SVGInline, *this, WTFMove(style));

    if (document().settings().layerBasedSVGEngineEnabled())
        return createRenderer<RenderSVGTransformableContainer>(*this, WTFMove(style));

    return createRenderer<LegacyRenderSVGTransformableContainer>(*this, WTFMove(style));
}

void SVGAElement::defaultEventHandler(Event& event)
{
    if (isLink()) {
        if (focused() && isEnterKeyKeydownEvent(event)) {
            event.setDefaultHandled();
            dispatchSimulatedClick(&event);
            return;
        }

        if (MouseEvent::canTriggerActivationBehavior(event)) {
            auto url = href().trim(isASCIIWhitespace);

            if (url[0] == '#') {
                if (RefPtr targetElement = dynamicDowncast<SVGSMILElement>(treeScope().getElementById(url.substringSharingImpl(1)))) {
                    targetElement->beginByLinkActivation();
                    event.setDefaultHandled();
                    return;
                }
            }

            auto target = this->target();
            if (target.isEmpty() && attributeWithoutSynchronization(XLinkNames::showAttr) == "new"_s)
                target = blankTargetFrameName();
            event.setDefaultHandled();

            if (RefPtr frame = document().frame())
                frame->loader().changeLocation(protectedDocument()->completeURL(url), target, &event, ReferrerPolicy::EmptyString, document().shouldOpenExternalURLsPolicyToPropagate());
            return;
        }
    }

    SVGGraphicsElement::defaultEventHandler(event);
}

int SVGAElement::defaultTabIndex() const
{
    return 0;
}

bool SVGAElement::supportsFocus() const
{
    if (hasEditableStyle())
        return SVGGraphicsElement::supportsFocus();
    // If not a link we should still be able to focus the element if it has a tabIndex.
    return isLink() || SVGGraphicsElement::supportsFocus();
}

bool SVGAElement::isURLAttribute(const Attribute& attribute) const
{
    return SVGURIReference::isKnownAttribute(attribute.name()) || SVGGraphicsElement::isURLAttribute(attribute);
}

bool SVGAElement::isMouseFocusable() const
{
    // Links are focusable by default, but only allow links with tabindex or contenteditable to be mouse focusable.
    // https://bugs.webkit.org/show_bug.cgi?id=26856
    if (isLink())
        return Element::supportsFocus();
    
    return SVGElement::isMouseFocusable();
}

bool SVGAElement::isKeyboardFocusable(const FocusEventData& focusEventData) const
{
    if (isFocusable() && Element::supportsFocus())
        return SVGElement::isKeyboardFocusable(focusEventData);

    RefPtr frame = document().frame();
    if (!frame)
        return false;

    if (isLink() && !frame->eventHandler().tabsToLinks(focusEventData))
        return false;

    return SVGElement::isKeyboardFocusable(focusEventData);
}

bool SVGAElement::canStartSelection() const
{
    if (!isLink())
        return SVGElement::canStartSelection();

    return hasEditableStyle();
}

bool SVGAElement::childShouldCreateRenderer(const Node& child) const
{
    // http://www.w3.org/2003/01/REC-SVG11-20030114-errata#linking-text-environment
    // The 'a' element may contain any element that its parent may contain, except itself.
    if (child.hasTagName(SVGNames::aTag))
        return false;

    if (parentElement() && parentElement()->isSVGElement())
        return parentElement()->childShouldCreateRenderer(child);

    return SVGElement::childShouldCreateRenderer(child);
}

bool SVGAElement::willRespondToMouseClickEventsWithEditability(Editability editability) const
{ 
    return isLink() || SVGGraphicsElement::willRespondToMouseClickEventsWithEditability(editability); 
}

SharedStringHash SVGAElement::visitedLinkHash() const
{
    ASSERT(isLink());
    if (!m_storedVisitedLinkHash)
        m_storedVisitedLinkHash = computeVisitedLinkHash(document().baseURL(), getAttribute(SVGNames::hrefAttr, XLinkNames::hrefAttr));
    return *m_storedVisitedLinkHash;
}

DOMTokenList& SVGAElement::relList()
{
    if (!m_relList) {
        lazyInitialize(m_relList, makeUniqueWithoutRefCountedCheck<DOMTokenList>(*this, SVGNames::relAttr, [](Document&, StringView token) {
#if USE(SYSTEM_PREVIEW)
            if (equalLettersIgnoringASCIICase(token, "ar"_s))
                return true;
#endif
            return equalLettersIgnoringASCIICase(token, "noreferrer"_s) || equalLettersIgnoringASCIICase(token, "noopener"_s);
        }));
    }
    return *m_relList;
}

} // namespace WebCore
