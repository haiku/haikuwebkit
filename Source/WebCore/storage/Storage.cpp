/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
#include "Storage.h"

#include "Document.h"
#include "ExceptionOr.h"
#include "LegacySchemeRegistry.h"
#include "LocalFrame.h"
#include "Page.h"
#include "ScriptTrackingPrivacyCategory.h"
#include "SecurityOrigin.h"
#include "StorageArea.h"
#include "StorageType.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(Storage);

Ref<Storage> Storage::create(LocalDOMWindow& window, Ref<StorageArea>&& storageArea)
{
    return adoptRef(*new Storage(window, WTFMove(storageArea)));
}

Storage::Storage(LocalDOMWindow& window, Ref<StorageArea>&& storageArea)
    : LocalDOMWindowProperty(&window)
    , m_storageArea(WTFMove(storageArea))
{
    ASSERT(frame());

    m_storageArea->incrementAccessCount();
}

Storage::~Storage()
{
    m_storageArea->decrementAccessCount();
}

unsigned Storage::length() const
{
    if (requiresScriptTrackingPrivacyProtection())
        return 0;

    return m_storageArea->length();
}

String Storage::key(unsigned index) const
{
    if (requiresScriptTrackingPrivacyProtection())
        return { };

    return m_storageArea->key(index);
}

String Storage::getItem(const String& key) const
{
    if (requiresScriptTrackingPrivacyProtection())
        return { };

    return m_storageArea->item(key);
}

ExceptionOr<void> Storage::setItem(const String& key, const String& value)
{
    auto* frame = this->frame();
    if (!frame)
        return Exception { ExceptionCode::InvalidAccessError };

    if (requiresScriptTrackingPrivacyProtection())
        return { };

    bool quotaException = false;
    m_storageArea->setItem(*frame, key, value, quotaException);
    if (quotaException)
        return Exception { ExceptionCode::QuotaExceededError };
    return { };
}

ExceptionOr<void> Storage::removeItem(const String& key)
{
    auto* frame = this->frame();
    if (!frame)
        return Exception { ExceptionCode::InvalidAccessError };

    if (requiresScriptTrackingPrivacyProtection())
        return { };

    m_storageArea->removeItem(*frame, key);
    return { };
}

ExceptionOr<void> Storage::clear()
{
    auto* frame = this->frame();
    if (!frame)
        return Exception { ExceptionCode::InvalidAccessError };

    m_storageArea->clear(*frame);
    return { };
}

bool Storage::contains(const String& key) const
{
    return m_storageArea->contains(key);
}

bool Storage::isSupportedPropertyName(const String& propertyName) const
{
    return m_storageArea->contains(propertyName);
}

Vector<AtomString> Storage::supportedPropertyNames() const
{
    unsigned length = m_storageArea->length();
    return Vector<AtomString>(length, [this](size_t i) {
        return m_storageArea->key(i);
    });
}

Ref<StorageArea> Storage::protectedArea() const
{
    return m_storageArea;
}

bool Storage::requiresScriptTrackingPrivacyProtection() const
{
    RefPtr document = window() ? window()->document() : nullptr;
    return document && document->requiresScriptTrackingPrivacyProtection(ScriptTrackingPrivacyCategory::LocalStorage);
}

} // namespace WebCore
