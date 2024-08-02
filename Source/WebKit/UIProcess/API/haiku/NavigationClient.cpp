/*
 * Copyright (C) 2019, 2024 Haiku, Inc. All rights reserved.
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
#include "NavigationClient.h"

#include "APINavigation.h"
#include "APIObject.h"
#include "WebPageProxy.h"
#include "WebView.h"
#include "WebViewConstants.h"

#include <Looper.h>
#include <Message.h>
#include <String.h>

using namespace WebKit;

void NavigationClient::didCommitNavigation(WebPageProxy& page, API::Navigation* navigation, API::Object* userData)
{
    BMessage message(DID_COMMIT_NAVIGATION);
    m_webView->getAppLooper()->PostMessage(&message);
}

void NavigationClient::didReceiveServerRedirectForProvisionalNavigation(WebPageProxy& page, API::Navigation* navigation, API::Object* userData)
{
    BMessage message(URL_CHANGE);
    message.AddString("url", BString(m_webView->getCurrentURL()));
    m_webView->getAppLooper()->PostMessage(&message);
}

void NavigationClient::didFinishNavigation(WebPageProxy& page, API::Navigation* navigation, API::Object* userData)
{
    BMessage message(DID_FINISH_NAVIGATION);
    m_webView->getAppLooper()->PostMessage(&message);
}
