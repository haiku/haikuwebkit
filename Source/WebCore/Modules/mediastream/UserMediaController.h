/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(MEDIA_STREAM)

#include "Exception.h"
#include "Page.h"
#include "UserMediaClient.h"
#include <wtf/CompletionHandler.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {

class UserMediaRequest;

class UserMediaController : public Supplement<Page> {
    WTF_MAKE_TZONE_ALLOCATED(UserMediaController);
public:
    explicit UserMediaController(Ref<UserMediaClient>&&);

    UserMediaClient* client() const { return m_client.get(); }

    void requestUserMediaAccess(UserMediaRequest&);
    void cancelUserMediaAccessRequest(UserMediaRequest&);

    void enumerateMediaDevices(Document&, UserMediaClient::EnumerateDevicesCallback&&);

    UserMediaClient::DeviceChangeObserverToken addDeviceChangeObserver(Function<void()>&&);
    void removeDeviceChangeObserver(UserMediaClient::DeviceChangeObserverToken);

    void updateCaptureState(const Document&, bool isActive, MediaProducerMediaCaptureKind, CompletionHandler<void(std::optional<Exception>&&)>&&);

    void logGetUserMediaDenial(Document&);
    void logGetDisplayMediaDenial(Document&);
    void logEnumerateDevicesDenial(Document&);

    void setShouldListenToVoiceActivity(Document&, bool);
    void checkDocumentForVoiceActivity(const Document*);
    void voiceActivityDetected();

    WEBCORE_EXPORT static ASCIILiteral supplementName();
    static UserMediaController* from(Page* page) { return downcast<UserMediaController>(Supplement<Page>::from(page, supplementName())); }

private:
    bool isUserMediaController() const final { return true; }

    RefPtr<UserMediaClient> m_client;

    WeakHashSet<Document, WeakPtrImplWithEventTargetData> m_voiceActivityDocuments;
    bool m_shouldListenToVoiceActivity { false };
};

inline void UserMediaController::requestUserMediaAccess(UserMediaRequest& request)
{
    if (RefPtr mediaClient = m_client)
        mediaClient->requestUserMediaAccess(request);
}

inline void UserMediaController::cancelUserMediaAccessRequest(UserMediaRequest& request)
{
    if (RefPtr mediaClient = m_client)
        mediaClient->cancelUserMediaAccessRequest(request);
}

inline void UserMediaController::enumerateMediaDevices(Document& document, UserMediaClient::EnumerateDevicesCallback&& completionHandler)
{
    if (RefPtr mediaClient = m_client)
        mediaClient->enumerateMediaDevices(document, WTFMove(completionHandler));
}

inline UserMediaClient::DeviceChangeObserverToken UserMediaController::addDeviceChangeObserver(Function<void()>&& observer)
{
    if (RefPtr mediaClient = m_client)
        return mediaClient->addDeviceChangeObserver(WTFMove(observer));
    return UserMediaClient::DeviceChangeObserverToken { 0 };
}

inline void UserMediaController::removeDeviceChangeObserver(UserMediaClient::DeviceChangeObserverToken token)
{
    if (RefPtr mediaClient = m_client)
        mediaClient->removeDeviceChangeObserver(token);
}

inline void UserMediaController::updateCaptureState(const Document& document, bool isActive, MediaProducerMediaCaptureKind kind, CompletionHandler<void(std::optional<Exception>&&)>&& completionHandler)
{
    if (RefPtr mediaClient = m_client)
        mediaClient->updateCaptureState(document, isActive, kind, WTFMove(completionHandler));
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::UserMediaController)
    static bool isType(const WebCore::SupplementBase& supplement) { return supplement.isUserMediaController(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(MEDIA_STREAM)
