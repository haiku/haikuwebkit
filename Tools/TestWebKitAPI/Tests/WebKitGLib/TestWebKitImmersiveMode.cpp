/*
 * Copyright (C) 2025 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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

#include "WebKitTestServer.h"
#include "WebViewTest.h"

static WebKitTestServer* kHttpsServer = nullptr;

static const char indexHTML[] =
"<html><body>"
"<input id='enterXR' type=\"button\" value=\"click to enter experience\"/>"
"<script>"
"document.getElementById('enterXR').addEventListener('click', () => {"
"  navigator.xr.requestSession('immersive-vr').then(session => {"
"    console.log('XR session started');"
"    session.addEventListener('end', (event) => {"
"        console.log('XR session ended');"
"    });"
"  }).catch(err => console.error(`XR session failed to start: ${err}`));"
"});"
"</script></body></html>";

class ImmersiveModeTest : public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(ImmersiveModeTest);

    static void isImmersiveModeEnabledChanged(GObject*, GParamSpec*, ImmersiveModeTest* test)
    {
        g_signal_handlers_disconnect_by_func(test->webView(), reinterpret_cast<void*>(isImmersiveModeEnabledChanged), test);
        g_main_loop_quit(test->m_mainLoop);
    }

    static gboolean permissionRequestCallback(WebKitWebView*, WebKitPermissionRequest *request, ImmersiveModeTest* test)
    {
        g_assert_true(WEBKIT_IS_XR_PERMISSION_REQUEST(request));
        g_assert_true(test->m_isExpectingPermissionRequest);

        webkit_permission_request_allow(request);

        g_signal_handlers_disconnect_by_func(test->webView(), reinterpret_cast<void*>(permissionRequestCallback), test);

        return TRUE;
    }

    void waitUntilIsImmersiveModeEnabledChanged()
    {
        g_signal_connect(m_webView.get(), "notify::is-immersive-mode-enabled", G_CALLBACK(isImmersiveModeEnabledChanged), this);
        g_main_loop_run(m_mainLoop);
    }

    void leaveImmersiveModeAndWaitUntilImmersiveModeChanged()
    {
        webkit_web_view_leave_immersive_mode(m_webView.get());

        if (webkit_web_view_is_immersive_mode_enabled(m_webView.get()))
            waitUntilIsImmersiveModeEnabledChanged();
    }

    void clickOnEnterXRButtonAndWaitUntilImmersiveModeChanged()
    {
        g_signal_connect(m_webView.get(), "permission-request", G_CALLBACK(permissionRequestCallback), this);

        m_isExpectingPermissionRequest = true;

        runJavaScriptAndWaitUntilFinished("document.getElementById('enterXR').focus()", nullptr);
        runJavaScriptAndWaitUntilFinished("document.getElementById('enterXR').click();", nullptr);

        if (!webkit_web_view_is_immersive_mode_enabled(m_webView.get()))
            waitUntilIsImmersiveModeEnabledChanged();
    }

    bool m_isExpectingPermissionRequest { false };
};

#if USE(SOUP2)
static void serverCallback(SoupServer*, SoupMessage* message, const char* path, GHashTable*, SoupClientContext*, gpointer)
#else
static void serverCallback(SoupServer*, SoupServerMessage* message, const char* path, GHashTable*, gpointer)
#endif
{
    g_assert(soup_server_message_get_method(message) == SOUP_METHOD_GET);

    if (g_str_equal(path, "/xr-session/")) {
        soup_server_message_set_status(message, SOUP_STATUS_OK, nullptr);

        auto* responseBody = soup_server_message_get_response_body(message);
        soup_message_body_append(responseBody, SOUP_MEMORY_STATIC, indexHTML, strlen(indexHTML));
        soup_message_body_complete(responseBody);
    } else
        g_assert_not_reached();
}

static void testWebKitImmersiveModeLeaveImmersiveModeAndWaitUntilImmersiveModeChanged(ImmersiveModeTest* test, gconstpointer)
{
    if (!g_getenv("WITH_OPENXR_RUNTIME")) {
        g_test_skip("Unable to run without an OpenXR runtime");
        return;
    }

    WebViewTest::NetworkPolicyGuard guard(test, WEBKIT_TLS_ERRORS_POLICY_IGNORE);

    g_assert_false(webkit_web_view_is_immersive_mode_enabled(test->m_webView.get()));

    test->loadURI(kHttpsServer->getURIForPath("/xr-session/").data());
    test->waitUntilLoadFinished();
    test->showInWindow();

    test->clickOnEnterXRButtonAndWaitUntilImmersiveModeChanged();
    g_assert_true(webkit_web_view_is_immersive_mode_enabled(test->m_webView.get()));

    test->leaveImmersiveModeAndWaitUntilImmersiveModeChanged();
    g_assert_false(webkit_web_view_is_immersive_mode_enabled(test->m_webView.get()));
}

void beforeAll()
{
    kHttpsServer = new WebKitTestServer(WebKitTestServer::ServerHTTPS);
    kHttpsServer->run(serverCallback);

    ImmersiveModeTest::add("WebKitImmersiveMode", "leave-immersive-mode", testWebKitImmersiveModeLeaveImmersiveModeAndWaitUntilImmersiveModeChanged);
}

void afterAll()
{
    delete kHttpsServer;
}
