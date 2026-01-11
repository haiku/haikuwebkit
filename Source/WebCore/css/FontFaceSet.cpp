/*
 * Copyright (C) 2016-2022 Apple Inc. All rights reserved.
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

#include "config.h"
#include "FontFaceSet.h"

#include "ContextDestructionObserverInlines.h"
#include "DOMPromiseProxy.h"
#include "DocumentQuirks.h"
#include "DocumentView.h"
#include "EventLoop.h"
#include "EventNames.h"
#include "FontFace.h"
#include "FontFaceSetLoadEvent.h"
#include "FontFaceSetLoadEventInit.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameLoader.h"
#include "JSDOMBinding.h"
#include "JSDOMPromiseDeferred.h"
#include "JSFontFace.h"
#include "JSFontFaceSet.h"
#include "Logging.h"
#include "ScriptExecutionContext.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FontFaceSet);

FontFaceSet::Iterator::Iterator(FontFaceSet& set)
    : m_target(set)
{
}

RefPtr<FontFace> FontFaceSet::Iterator::next()
{
    if (m_index >= m_target->size())
        return nullptr;
    return m_target->backing()[m_index++].wrapper(m_target->protectedScriptExecutionContext().get());
}

FontFaceSet::PendingPromise::PendingPromise(LoadPromise&& promise)
    : promise(makeUniqueRef<LoadPromise>(WTF::move(promise)))
{
}

FontFaceSet::PendingPromise::~PendingPromise() = default;

Ref<FontFaceSet> FontFaceSet::create(ScriptExecutionContext& context, const Vector<Ref<FontFace>>& initialFaces)
{
    Ref<FontFaceSet> result = adoptRef(*new FontFaceSet(context));

    for (auto& face : initialFaces)
        result->add(face);

    result->suspendIfNeeded();
    result->setInitialState();
    return result;
}

Ref<FontFaceSet> FontFaceSet::create(ScriptExecutionContext& context, CSSFontFaceSet& backing)
{
    Ref<FontFaceSet> result = adoptRef(*new FontFaceSet(context, backing));
    result->suspendIfNeeded();
    result->setInitialState();
    return result;
}

FontFaceSet::FontFaceSet(ScriptExecutionContext& context)
    : ActiveDOMObject(&context)
    , m_backing(CSSFontFaceSet::create())
    , m_readyPromise(makeUniqueRef<ReadyPromise>(*this, &FontFaceSet::readyPromiseResolve))
{
    m_backing->addFontEventClient(*this);
}

FontFaceSet::FontFaceSet(ScriptExecutionContext& context, CSSFontFaceSet& backing)
    : ActiveDOMObject(&context)
    , m_backing(backing)
    , m_readyPromise(makeUniqueRef<ReadyPromise>(*this, &FontFaceSet::readyPromiseResolve))
{
    m_backing->addFontEventClient(*this);
}

FontFaceSet::~FontFaceSet() = default;

void FontFaceSet::setInitialState()
{
    auto isDocumentLoaded = [&]() {
        if (RefPtr document = dynamicDowncast<Document>(scriptExecutionContext())) {
            if (document->frame())
                return document->loadEventFinished() && !document->processingLoadEvent();
        }
        return true;
    }();

    if (isDocumentLoaded)
        documentDidFinishLoading();
}

void FontFaceSet::documentDidFinishLoading()
{
    LOG_WITH_STREAM(Fonts, stream << "FontFaceSet " << this << " FontFaceSet::documentDidFinishLoading");

    m_isDocumentLoaded = true;
    stopPendingOnEnvironment();
}

bool FontFaceSet::isPendingOnEnvironment() const
{
    if (!m_isDocumentLoaded)
        return true;

    // FIXME: * the document has pending stylesheet requests (haveStylesheetsLoaded()).
    // FIXME: * the document has pending layout operations which might cause the user agent to request a font, or which depend on recently-loaded fonts
    return false;
}

void FontFaceSet::stopPendingOnEnvironment()
{
    if (m_isStuckOnEnvironment && m_loadingFonts.isEmpty())
        switchStateToLoaded();

    m_isStuckOnEnvironment = false;
}

bool FontFaceSet::has(FontFace& face) const
{
    if (face.backing().cssConnection())
        m_backing->updateStyleIfNeeded();
    return m_backing->hasFace(face.backing());
}

size_t FontFaceSet::size()
{
    m_backing->updateStyleIfNeeded();
    return m_backing->faceCount();
}

// https://drafts.csswg.org/css-font-loading/#dom-fontfaceset-add
ExceptionOr<FontFaceSet&> FontFaceSet::add(FontFace& face)
{
    if (m_backing->hasFace(face.backing()))
        return *this;

    if (face.backing().cssConnection())
        return Exception(ExceptionCode::InvalidModificationError);

    if (face.scriptExecutionContext() != scriptExecutionContext())
        return Exception { ExceptionCode::WrongDocumentError };

    m_backing->add(face.backing());
    return *this;
}

// https://drafts.csswg.org/css-font-loading/#dom-fontfaceset-delete
bool FontFaceSet::remove(FontFace& face)
{
    if (face.backing().cssConnection())
        return false;
    bool result = m_backing->hasFace(face.backing());
    if (result)
        m_backing->remove(face.backing());
    return result;
}

// https://drafts.csswg.org/css-font-loading/#dom-fontfaceset-clear
void FontFaceSet::clear()
{
    auto facesPartitionIndex = m_backing->facesPartitionIndex();
    while (m_backing->faceCount() > facesPartitionIndex) {
        m_backing->remove(m_backing.get()[m_backing->faceCount() - 1]);
        ASSERT(m_backing->facesPartitionIndex() == facesPartitionIndex);
    }

    m_failedFonts.removeIf([](const auto& entry) { return !entry->backing().cssConnection(); });
    m_loadedFonts.removeIf([](const auto& entry) { return !entry->backing().cssConnection(); });

    m_loadingFonts.clear();
}

void FontFaceSet::load(ScriptExecutionContext& context, const String& font, const String& text, LoadPromise&& promise)
{
    LOG_WITH_STREAM(Fonts, stream << "FontFaceSet::load - " << font << " " << text);

    m_backing->updateStyleIfNeeded();
    auto matchingFacesResult = m_backing->matchingFacesExcludingPreinstalledFonts(context, font, text);
    if (matchingFacesResult.hasException()) {
        promise.reject(matchingFacesResult.releaseException());
        return;
    }
    auto matchingFaces = matchingFacesResult.releaseReturnValue();

    if (matchingFaces.isEmpty()) {
        promise.resolve({ });
        return;
    }

    for (auto& face : matchingFaces)
        face.get().load();

    auto* document = dynamicDowncast<Document>(scriptExecutionContext());
    if (document && document->quirks().shouldEnableFontLoadingAPIQuirk()) {
        // HBOMax.com expects that loading fonts will succeed, and will totally break when it doesn't. But when lockdown mode is enabled, fonts
        // fail to load, because that's the whole point of lockdown mode.
        //
        // This is a bit of a hack to say "When lockdown mode is enabled, and lockdown mode has removed all the remote fonts, then just pretend
        // that the fonts loaded successfully." If there are any non-remote fonts still present, don't make any behavior change.
        //
        // See also: https://github.com/w3c/csswg-drafts/issues/7680

        bool hasSource = false;
        for (auto& face : matchingFaces) {
            if (face.get().sourceCount()) {
                hasSource = true;
                break;
            }
        }
        if (!hasSource) {
            promise.resolve(matchingFaces.map([scriptExecutionContext = scriptExecutionContext()] (const auto& matchingFace) {
                return matchingFace.get().wrapper(scriptExecutionContext);
            }));
            return;
        }
    }

    for (auto& face : matchingFaces) {
        if (face.get().status() == CSSFontFace::Status::Failure) {
            promise.reject(ExceptionCode::NetworkError);
            return;
        }
    }

    auto pendingPromise = PendingPromise::create(WTF::move(promise));
    bool waiting = false;

    for (auto& face : matchingFaces) {
        pendingPromise->faces.append(face.get().wrapper(protectedScriptExecutionContext().get()));
        if (face.get().status() == CSSFontFace::Status::Success)
            continue;
        waiting = true;
        ASSERT(face.get().existingWrapper());
        m_pendingPromises.add(face.get().existingWrapper(), Vector<Ref<PendingPromise>>()).iterator->value.append(pendingPromise.copyRef());
    }

    if (!waiting)
        pendingPromise->promise->resolve(pendingPromise->faces);
}

ExceptionOr<bool> FontFaceSet::check(ScriptExecutionContext& context, const String& family, const String& text)
{
    m_backing->updateStyleIfNeeded();
    return m_backing->check(context, family, text);
}

void FontFaceSet::faceDidStartLoading(CSSFontFace& face)
{
    // Eagerly create the wrapper because we'll need it for the `loading` event anway.
    Ref wrapper = face.wrapper(protectedScriptExecutionContext().get());
    LOG_WITH_STREAM(Fonts, stream << " FontFaceSet::faceDidStartLoading " << face.family());

    if (m_loadingFonts.isEmpty())
        switchStateToLoading();

    m_loadingFonts.add(wrapper);
}

void FontFaceSet::faceDidFinishLoading(CSSFontFace& face, CSSFontFace::Status newStatus)
{
    Ref wrapper = face.wrapper(protectedScriptExecutionContext().get());
    LOG_WITH_STREAM(Fonts, stream << "FontFaceSet::faceDidFinishLoading - " << face.family() << " " << face.style() << " " << face.weight() << " - status " << (unsigned)newStatus);

    auto pendingPromises = m_pendingPromises.take(wrapper.ptr());

    queueTaskKeepingObjectAlive(*this, TaskSource::DOMManipulation, [fontFace = WTF::move(wrapper), promises = WTF::move(pendingPromises), newStatus](auto& fontFaceSet) {
        LOG_WITH_STREAM(Fonts, stream << " FontFaceSet::faceDidFinishLoading task for " << fontFace->family() << " - resolving " << promises.size() << " load promises");

        for (auto& pendingPromise : promises) {
            if (pendingPromise->hasReachedTerminalState)
                continue;

            if (newStatus == CSSFontFace::Status::Success) {
                if (pendingPromise->hasOneRef()) {
                    pendingPromise->promise->resolve(pendingPromise->faces);
                    pendingPromise->hasReachedTerminalState = true;
                }
            } else {
                ASSERT(newStatus == CSSFontFace::Status::Failure);
                pendingPromise->promise->reject(ExceptionCode::NetworkError);
                pendingPromise->hasReachedTerminalState = true;
            }
        }

        if (newStatus == CSSFontFace::Status::Success)
            fontFaceSet.m_loadedFonts.add(fontFace);
        else
            fontFaceSet.m_failedFonts.add(fontFace);

        fontFaceSet.m_loadingFonts.remove(fontFace);

        if (fontFaceSet.m_loadingFonts.isEmpty())
            fontFaceSet.switchStateToLoaded();
    });
}

void FontFaceSet::didAddFace(CSSFontFace&)
{
}

void FontFaceSet::didDeletedFace(CSSFontFace& face)
{
    // If the face is being deleted, we know it's losing its cssConnection, so don't check that here (despite what the spec says).
    RefPtr wrapper = face.existingWrapper();
    if (!wrapper)
        return;

    if (m_loadingFonts.remove(*wrapper) && m_loadingFonts.isEmpty())
        switchStateToLoaded();

    m_failedFonts.remove(*wrapper);
    m_loadedFonts.remove(*wrapper);
    LOG_WITH_STREAM(Fonts, stream << " FontFaceSet::didDeletedFace " << face.family() << " (now have " << m_loadingFonts.size() << " loading fonts)");
}

void FontFaceSet::startedLoading()
{
}

void FontFaceSet::completedLoading()
{
}

// https://drafts.csswg.org/css-font-loading/#switch-the-fontfaceset-to-loading
void FontFaceSet::switchStateToLoading()
{
    m_status = LoadStatus::Loading;

    LOG_WITH_STREAM(Fonts, stream << "FontFaceSet::switchStateToLoading (" << m_loadingFonts.size() << " loading fonts; making new promise " << m_readyPromise->isFulfilled() << ")");

    if (m_readyPromise->isFulfilled())
        m_readyPromise = makeUniqueRef<ReadyPromise>(*this, &FontFaceSet::readyPromiseResolve);

    queueTaskKeepingObjectAlive(*this, TaskSource::DOMManipulation, [](auto& fontFaceSet) {
        FontFaceSetLoadEventInit eventInit;
        for (auto& fontFace : fontFaceSet.m_loadingFonts)
            eventInit.fontfaces.append(fontFace);

        LOG_WITH_STREAM(Fonts, stream << " FontFaceSet::switchStateToLoading task - dispatching loading event with " << eventInit.fontfaces.size() << " faces");

        fontFaceSet.dispatchEvent(FontFaceSetLoadEvent::create(eventNames().loadingEvent, eventInit));
    });
}

// https://drafts.csswg.org/css-font-loading/#switch-the-fontfaceset-to-loaded
void FontFaceSet::switchStateToLoaded()
{
    LOG_WITH_STREAM(Fonts, stream << "FontFaceSet " << this << " switchStateToLoaded (promise fulfilled " << m_readyPromise->isFulfilled() << ", stuck on environment " << isPendingOnEnvironment() << ")");

    if (isPendingOnEnvironment()) {
        m_isStuckOnEnvironment = true;
        return;
    }

    m_status = LoadStatus::Loaded;

    if (!m_readyPromise->isFulfilled())
        m_readyPromise->resolve(*this);

    queueTaskKeepingObjectAlive(*this, TaskSource::DOMManipulation, [](auto& fontFaceSet) mutable {

        auto fireFontFaceSetEvent = [&](const AtomString& eventName, HashSet<Ref<FontFace>>&& faces) {
            FontFaceSetLoadEventInit eventInit;

            for (auto& face : faces)
                eventInit.fontfaces.append(face);

            LOG_WITH_STREAM(Fonts, stream << " FontFaceSet::switchStateToLoaded task - dispatching " << eventName << " event with " << eventInit.fontfaces.size() << " fonts");

            fontFaceSet.dispatchEvent(FontFaceSetLoadEvent::create(eventName, eventInit));
        };

        if (!fontFaceSet.m_loadedFonts.isEmpty())
            fireFontFaceSetEvent(eventNames().loadingdoneEvent, std::exchange(fontFaceSet.m_loadedFonts, { }));

        if (!fontFaceSet.m_failedFonts.isEmpty())
            fireFontFaceSetEvent(eventNames().loadingerrorEvent, std::exchange(fontFaceSet.m_failedFonts, { }));
    });
}

FontFaceSet& FontFaceSet::readyPromiseResolve()
{
    return *this;
}

ScriptExecutionContext* FontFaceSet::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}


}
