/*
 * Copyright (C) 2019 Haiku, Inc. All rights reserved.
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

#include <wtf/RefPtr.h>

#include <memory>
#include <Rect.h>

class BLooper;
class BMessage;
class BWindow;

namespace WebKit {
class PageLoadStateObserver;
class WebViewBase;
}

class BWebView {
public:
    BWebView(BRect, BWindow*);
    void loadURIRequest(const char*); // use this in app to load a url
    void loadURI(BMessage*);
    void goForward();
    void goBackward();
    void stop();
    void paintContent();
    WebKit::WebViewBase* getRenderView();
    const char* getCurrentURL();
    BLooper* getAppLooper() { return fAppLooper; }

    void navigationCallbacks();
    double progress();
    const char* title();

private:
    RefPtr<WebKit::WebViewBase> fWebViewBase;
    RefPtr<WebKit::PageLoadStateObserver> fObserver;
    BLooper* fAppLooper;
};

