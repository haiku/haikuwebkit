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
#include "WebView.h"

#include "APINavigation.h"
#include "APIPageConfiguration.h"
#include "APIProcessPoolConfiguration.h"
#include "NavigationClient.h"
#include "PageLoadStateObserver.h"
#include "WebPageProxy.h"
#include "WebPreferences.h"
#include "WebProcessPool.h"
#include "WebViewBase.h"
#include <wtf/FastMalloc.h>
#include <wtf/RefPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/text/WTFString.h>

#include <Message.h>
#include <Window.h>
#include <View.h>
#include <Looper.h>
#include <Rect.h>

using namespace WebKit;

BWebView::BWebView(BRect frame, BWindow* myWindow)
    : fAppLooper(myWindow->Looper())
{
    RefPtr<API::PageConfiguration> config = API::PageConfiguration::create();

    RefPtr<WebPreferences> prefs = WebPreferences::create(String(), "WebKit2."_s, "WebKit2."_s);
    prefs->setDeveloperExtrasEnabled(true);
    config->setPreferences(WTFMove(prefs));

    RefPtr<API::ProcessPoolConfiguration> apiConfiguration = API::ProcessPoolConfiguration::create();
    RefPtr<WebProcessPool> processPool = WebProcessPool::create(*apiConfiguration.get());
    config->setProcessPool(WTFMove(processPool));

    fWebViewBase = WebViewBase::create("Webkit", frame, myWindow, *config.get());

    // TODO: Can we run WebKit's main thread on its own thread instead of on
    // BApplication's main thread?
    WTF::RunLoop::run();
}

void BWebView::navigationCallbacks()
{
    fWebViewBase->page()->setNavigationClient(makeUniqueRef<NavigationClient>(this));

    fObserver = adoptRef(*new PageLoadStateObserver(this, fAppLooper));
    fWebViewBase->page()->pageLoadState().addObserver(*fObserver);
}

void BWebView::loadURIRequest(const char* uri)
{
    BMessage message(URL_LOAD_HANDLE);
    message.AddString("url", uri);
    be_app->PostMessage(&message);
}

void BWebView::paintContent()
{
    fWebViewBase->LockLooper();
    fWebViewBase->Invalidate();
    fWebViewBase->UnlockLooper();
}

WebViewBase* BWebView::getRenderView()
{
    return fWebViewBase.get();
}

const char* BWebView::getCurrentURL()
{
    return fWebViewBase->currentURL();
}

void BWebView::loadURI(BMessage* message)
{
    const char* uri;
    message->FindString("url", &uri);
    fWebViewBase->page()->loadRequest(URL { WTF::String::fromUTF8(uri) });
}

void BWebView::goForward()
{
    fWebViewBase->page()->goForward();
    BMessage message(URL_CHANGE);
    message.AddString("url", BString(getCurrentURL()));
    fAppLooper->PostMessage(&message);
}

void BWebView::goBackward()
{
    fWebViewBase->page()->goBack();
    BMessage message(URL_CHANGE);
    message.AddString("url", BString(getCurrentURL()));
    fAppLooper->PostMessage(&message);
}

void BWebView::stop()
{
    fWebViewBase->page()->close();
}

double BWebView::progress()
{
    return fWebViewBase->page()->estimatedProgress();
}

const char* BWebView::title()
{
    return fWebViewBase->page()->pageLoadState().title().utf8().data();
}
