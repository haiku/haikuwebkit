/*
 * Copyright (C) 2010 Ryan Leavengood <leavengood@gmail.com>
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
 *
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "config.h"
#include "WebPage.h"

#include "BackForwardList.h"
#include "ChromeClientHaiku.h"
#include "ContextMenuClientHaiku.h"
#include "Cursor.h"
#include "DragClientHaiku.h"
#include "EditorClientHaiku.h"
#include "FrameLoaderClientHaiku.h"
#include "IconDatabase.h"
#include "InspectorClientHaiku.h"
#include "LegacyHistoryItemClient.h"
#include "NotificationClientHaiku.h"
#include "PageStorageSessionProvider.h"
#include "PlatformStrategiesHaiku.h"
#include "ProgressTrackerHaiku.h"
#include "pal/text/UnencodableHandling.h"

#include "WebCore/BackForwardController.h"
#include "WebCore/CacheStorageProvider.h"
#include "WebCore/Chrome.h"
#include "WebCore/ContextMenu.h"
#include "WebCore/ContextMenuController.h"
#include "WebCore/CookieJar.h"
#include "WebCore/DeviceOrientationClientMock.h"
#include "WebCore/DiagnosticLoggingClient.h"
#include "WebCore/DOMTimer.h"
#include <WebCore/DummyModelPlayerProvider.h>
#include <WebCore/DummyStorageProvider.h>
#include "WebCore/DummySpeechRecognitionProvider.h"
#include "WebCore/DummyWebRTCProvider.h"
#include "WebCore/Editor.h"
#include "WebCore/EmptyBadgeClient.h"
#include "WebCore/EmptyClients.h"
#include "WebCore/EventHandler.h"
#include "WebCore/FileChooser.h"
#include "WebCore/FocusController.h"
#include "WebCore/FontCache.h"
#include "WebCore/Frame.h"
#include "WebCore/FrameLoader.h"
#include "WebCore/FrameView.h"
#include "WebCore/GraphicsContextHaiku.h"
#include "WebCore/HandleUserInputEventResult.h"
#include "WebCore/LibWebRTCProvider.h"
#include "WebCore/LogInitialization.h"
#include "WebCore/MemoryCache.h"
#include "WebNavigatorContentUtilsClient.h"
#include "WebCore/MHTMLArchive.h"
#include "WebCore/Page.h"
#include "WebCore/PageConfiguration.h"
#include "WebCore/PageGroup.h"
#include "WebCore/PermissionController.h"
#include "WebCore/PlatformKeyboardEvent.h"
#include "WebCore/PlatformMouseEvent.h"
#include "WebCore/PlatformWheelEvent.h"
#include "WebCore/PluginInfoProvider.h"
#include "WebCore/PointerLockController.h"
#include "WebCore/ProcessSyncClient.h"
#include "WebCore/ProgressTracker.h"
#include "WebCore/ProgressTrackerClient.h"
#include "WebCore/RemoteFrameClient.h"
#include "WebCore/ResourceHandle.h"
#include "WebCore/ResourceRequest.h"
#include "WebCore/ScriptController.h"
#include "WebCore/ScrollingCoordinatorTypes.h"
#include "WebCore/Settings.h"
#include "WebCore/ThreadGlobalData.h"
#include "WebCore/UserContentController.h"
#include <WebCore/WebLockRegistry.h>

#include "LegacySocketProvider.h"

#include "WebApplicationCache.h"
#include "WebBroadcastChannelRegistry.h"
#include "WebCryptoClient.h"
#include "WebDatabaseProvider.h"
#include "WebDiagnosticLoggingClient.h"
#include "WebDownload.h"
#include "WebDownloadPrivate.h"
#include "WebFrame.h"
#include "WebFramePrivate.h"
#include "WebSettings.h"
#include "WebStorageNamespaceProvider.h"
#include "WebView.h"
#include "WebViewConstants.h"
#include "WebViewGroup.h"
#include "WebVisitedLinkStore.h"

#include <Bitmap.h>
#include <Entry.h>
#include <FilePanel.h>
#include <FindDirectory.h>
#include <Font.h>
#include <MenuItem.h>
#include <Message.h>
#include <app/MessageQueue.h>
#include <Messenger.h>
#include <PopUpMenu.h>
#include <Region.h>
#include <Window.h>

#include <wtf/text/AtomString.h>
#include <wtf/Assertions.h>
#include <wtf/Threading.h>

#if USE(GCRYPT)
#include <gcrypt.h>
#endif

/*
     The basic idea here is to dispatch all public methods to the BLooper
     to which the handler is attached (should be the be_app), such that
     the calls into WebCore code happen from within that thread *only*.
     In current WebCore with pthread threading backend, this must be the
     same thread that called WTF::initializeThreading(), respectively the
     initializeOnce() method of this class.
 */

enum {
    HANDLE_SHUTDOWN = 'sdwn',

    HANDLE_LOAD_URL = 'lurl',
    HANDLE_RELOAD = 'reld',
    HANDLE_GO_BACK = 'back',
    HANDLE_GO_FORWARD = 'fwrd',
    HANDLE_STOP_LOADING = 'stop',

    HANDLE_FOCUSED = 'focs',
    HANDLE_ACTIVATED = 'actd',

    HANDLE_SET_VISIBLE = 'vsbl',
    HANDLE_DRAW = 'draw',
    HANDLE_FRAME_RESIZED = 'rszd',

    HANDLE_CHANGE_ZOOM_FACTOR = 'zmfr',
    HANDLE_FIND_STRING = 'find',

    HANDLE_SET_STATUS_MESSAGE = 'stsm',
    HANDLE_RESEND_NOTIFICATIONS = 'rsnt',
    HANDLE_SEND_EDITING_CAPABILITIES = 'sedc',
    HANDLE_SEND_PAGE_SOURCE = 'spsc'
};

using namespace WebCore;

class EmptyPluginInfoProvider final : public PluginInfoProvider {
    void refreshPlugins() final { };
    Vector<PluginInfo> pluginInfo(Page&, std::optional<Vector<SupportedPluginIdentifier>>&) final { return { }; }
	Vector<PluginInfo> webVisiblePluginInfo(Page&, const URL&) final { return { }; }
};

BMessenger BWebPage::sDownloadListener;
void WebKitInitializeLogChannelsIfNecessary();

/*static*/ void BWebPage::InitializeOnce()
{
	// NOTE: This needs to be called when the BApplication is ready.
	// It won't work as static initialization.
#if !LOG_DISABLED
    WebKitInitializeLogChannelsIfNecessary();
#endif
    PlatformStrategiesHaiku::initialize();

#if USE(GCRYPT)
    // Call gcry_check_version() before any other libgcrypt call, ignoring the
    // returned version string.
    gcry_check_version(nullptr);

    // Pre-allocate 16kB of secure memory and finish the initialization.
    gcry_control(GCRYCTL_INIT_SECMEM, 16384, nullptr);
    gcry_control(GCRYCTL_INITIALIZATION_FINISHED, nullptr);
#endif

    WTF::initializeMainThread();
    ScriptController::initializeMainThread();
    PAL::UTF8Encoding();

    WebVisitedLinkStore::setShouldTrackVisitedLinks(true);

    RunLoop::run(); // This attaches it to the existing be_app looper
}

/*static*/ void BWebPage::ShutdownOnce()
{
	WebKit::iconDatabase().close();

	// There is a crash on exit if the font cache is not empty, so make sure it is
	// Destroying the thread global data also helps cutting that dependency cycle
	WebCore::FontCache::invalidateAllFontCaches(WebCore::FontCache::ShouldRunInvalidationCallback::No);
	WebCore::threadGlobalData().destroy();
}

/*static*/ void BWebPage::SetCacheModel(BWebKitCacheModel model)
{
    // FIXME: Add disk cache handling when CURL has the API
    uint32 cacheTotalCapacity;
    uint32 cacheMinDeadCapacity;
    uint32 cacheMaxDeadCapacity;
    WTF::Seconds deadDecodedDataDeletionInterval;

    switch (model) {
    case B_WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER:
        cacheTotalCapacity = 0;
        cacheMinDeadCapacity = 0;
        cacheMaxDeadCapacity = 0;
        deadDecodedDataDeletionInterval = WTF::Seconds(0);
        break;
    case B_WEBKIT_CACHE_MODEL_WEB_BROWSER:
        cacheTotalCapacity = 32 * 1024 * 1024;
        cacheMinDeadCapacity = cacheTotalCapacity / 4;
        cacheMaxDeadCapacity = cacheTotalCapacity / 2;
        deadDecodedDataDeletionInterval = WTF::Seconds(60);
        break;
    default:
        return;
    }

    MemoryCache::singleton().setCapacities(cacheMinDeadCapacity, cacheMaxDeadCapacity, cacheTotalCapacity);
    MemoryCache::singleton().setDeadDecodedDataDeletionInterval(deadDecodedDataDeletionInterval);
}


BWebPage::BWebPage(BWebView* webView, BPrivate::Network::BUrlContext* context)
    : BHandler("BWebPage")
    , fWebView(webView)
    , fMainFrame(NULL)
    , fSettings(NULL)
    , fContext(context)
    , fDumpRenderTree(NULL)
    , fLoadingProgress(100)
    , fStatusMessage()
    , fDisplayedStatusMessage()
    , fPageVisible(true)
    , fPageDirty(false)
    , fToolbarsVisible(true)
    , fStatusbarVisible(true)
    , fMenubarVisible(true)
{
    // FIXME we should get this from the page settings, but they are created
    // after the page, and we need this before the page is created.
    BPath storagePath;
    find_directory(B_USER_SETTINGS_DIRECTORY, &storagePath);

    storagePath.Append("WebKit/LocalStorage");

    RefPtr<WebViewGroup> viewGroup = WebViewGroup::getOrCreate(ASCIILiteral::fromLiteralUnsafe("default"),
        String::fromUTF8(storagePath.Path()));

    auto storageProvider = PageStorageSessionProvider::create();

    PageConfiguration pageClients(
        WebCore::PageIdentifier::generate(),
        PAL::SessionID::defaultSessionID(),
        makeUniqueRef<EditorClientHaiku>(this),
        LegacySocketProvider::create(),
#if USE(WEBRTC)
        makeUniqueRef<LibWebRTCProvider>(),
#else
        makeUniqueRef<DummyWebRTCProvider>(),
#endif
        CacheStorageProvider::create(),
        viewGroup->userContentController(),
        BackForwardList::create(),
        CookieJar::create(storageProvider.copyRef()),
        makeUniqueRef<ProgressTrackerClientHaiku>(this),
        WebCore::PageConfiguration::LocalMainFrameCreationParameters {
            CompletionHandler<UniqueRef<WebCore::LocalFrameLoaderClient>(WebCore::LocalFrame&, WebCore::FrameLoader&)> { [this] (WebCore::LocalFrame&, auto& frameLoader) {
                return makeUniqueRefWithoutRefCountedCheck<FrameLoaderClientHaiku>(frameLoader, this);
            } },
            WebCore::SandboxFlags {}
        },
        WebCore::FrameIdentifier::generate(),
        nullptr,
        makeUniqueRef<WebCore::DummySpeechRecognitionProvider>(),
        WebBroadcastChannelRegistry::getOrCreate(false),
        makeUniqueRef<WebCore::DummyStorageProvider>(),
        WebCore::DummyModelPlayerProvider::create(),
        EmptyBadgeClient::create(),
        LegacyHistoryItemClient::singleton(),
        makeUniqueRef<ContextMenuClientHaiku>(this),
        makeUniqueRef<ChromeClientHaiku>(this, webView),
        makeUniqueRef<WebCryptoClient>(),
        makeUniqueRef<WebCore::ProcessSyncClient>()
    );

    // alternativeText
    pageClients.dragClient = std::make_unique<DragClientHaiku>(webView);
    pageClients.inspectorClient = std::make_unique<InspectorClientHaiku>();
    pageClients.diagnosticLoggingClient = std::make_unique<WebKit::WebDiagnosticLoggingClient>();
    pageClients.applicationCacheStorage = &WebApplicationCache::storage();
    pageClients.databaseProvider = &WebDatabaseProvider::singleton();
    // performanceLogging
    // pluginInClient
    pageClients.pluginInfoProvider = adoptRef(*new EmptyPluginInfoProvider);
    pageClients.storageNamespaceProvider = &viewGroup->storageNamespaceProvider();
    // validationMessage *
    pageClients.visitedLinkStore = &viewGroup->visitedLinkStore();
    // webGLStateTracker *

    fPage = std::make_unique<WTF::Ref<Page>>(Page::create(WTFMove(pageClients)));
    storageProvider->setPage(fPage->get());

#if ENABLE(GEOLOCATION)
    WebCore::provideGeolocationTo(*fPage, new GeolocationClientMock());
#endif
#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    WebCore::provideNotification(fPage->ptr(), new NotificationClientHaiku(this));
#endif
#if ENABLE(DEVICE_ORIENTATION)
    // No actual support, we only want to get the html5test points...
    WebCore::provideDeviceOrientationTo(*fPage, new DeviceOrientationClientMock());
#endif
#if ENABLE(MEDIA_STREAM)
    WebCore::provideUserMediaTo(*fPage, new WebUserMediaClient(this));
#endif
#if ENABLE(NAVIGATOR_CONTENT_UTILS)
    WebCore::provideNavigatorContentUtilsTo(*fPage,
        std::make_unique<WebKit::WebNavigatorContentUtilsClient>());
#endif

    fSettings = new BWebSettings(&fPage->get().settings());
}

BWebPage::~BWebPage()
{
	// We need to make sure there are no more timers running, since those
	// arrive to a different, global handler (the timer handler), and the
	// timer functions would then operate on stale pointers.
	// Calling detachFromParent() on the FrameLoader will recursively detach
	// all child frames, as well as stop all loaders before doing that.
    if (fMainFrame && fMainFrame->Frame())
        fMainFrame->Frame()->loader().detachFromParent();

    // NOTE: The m_webFrame member will be deleted by the
    // FrameLoaderClientHaiku, when the WebCore::Frame/FrameLoader instance is
    // free'd. For sub-frames, we don't maintain them anyway, and for the
    // main frame, the same mechanism is used.
    delete fSettings;
}

// #pragma mark - public

void BWebPage::Init()
{
	WebFramePrivate* data = new WebFramePrivate(fPage->ptr());
	fMainFrame = new BWebFrame(this, 0, data);
}

void BWebPage::Shutdown()
{
    Looper()->PostMessage(HANDLE_SHUTDOWN, this);
}

void BWebPage::SetListener(const BMessenger& listener)
{
	fListener = listener;
	fMainFrame->SetListener(listener);
	static_cast<ProgressTrackerClientHaiku&>(fPage->get().progress().client()).setDispatchTarget(listener);
}

void BWebPage::SetDownloadListener(const BMessenger& listener)
{
    sDownloadListener = listener;
}

BPrivate::Network::BUrlContext* BWebPage::GetContext()
{
    return fContext;
}

void BWebPage::LoadURL(const char* urlString)
{
    BMessage message(HANDLE_LOAD_URL);
    message.AddString("url", urlString);
    Looper()->PostMessage(&message, this);
}

void BWebPage::Reload()
{
    Looper()->PostMessage(HANDLE_RELOAD, this);
}

void BWebPage::GoBack()
{
    Looper()->PostMessage(HANDLE_GO_BACK, this);
}

void BWebPage::GoForward()
{
    Looper()->PostMessage(HANDLE_GO_FORWARD, this);
}

void BWebPage::StopLoading()
{
    Looper()->PostMessage(HANDLE_STOP_LOADING, this);
}

void BWebPage::ChangeZoomFactor(float increment, bool textOnly)
{
	BMessage message(HANDLE_CHANGE_ZOOM_FACTOR);
	message.AddFloat("increment", increment);
	message.AddBool("text only", textOnly);
    Looper()->PostMessage(&message, this);
}

void BWebPage::FindString(const char* string, bool forward, bool caseSensitive,
    bool wrapSelection, bool startInSelection)
{
	BMessage message(HANDLE_FIND_STRING);
	message.AddString("string", string);
	message.AddBool("forward", forward);
	message.AddBool("case sensitive", caseSensitive);
	message.AddBool("wrap selection", wrapSelection);
	message.AddBool("start in selection", startInSelection);
    Looper()->PostMessage(&message, this);
}

void BWebPage::SetDeveloperExtrasEnabled(bool enable)
{
    page()->settings().setDeveloperExtrasEnabled(enable);
}

void BWebPage::SetStatusMessage(const BString& status)
{
    BMessage message(HANDLE_SET_STATUS_MESSAGE);
    message.AddString("string", status);
    Looper()->PostMessage(&message, this);
}

void BWebPage::ResendNotifications()
{
    Looper()->PostMessage(HANDLE_RESEND_NOTIFICATIONS, this);
}

void BWebPage::SendEditingCapabilities()
{
    Looper()->PostMessage(HANDLE_SEND_EDITING_CAPABILITIES, this);
}

void BWebPage::SendPageSource()
{
	Looper()->PostMessage(HANDLE_SEND_PAGE_SOURCE, this);
}

void BWebPage::RequestDownload(const BString& url)
{
	ResourceRequest request(String::fromUTF8(url.String()));
	requestDownload(request, false);
}

BWebFrame* BWebPage::MainFrame() const
{
    return fMainFrame;
};

BWebSettings* BWebPage::Settings() const
{
    return fSettings;
};

BWebView* BWebPage::WebView() const
{
    return fWebView;
}

BString BWebPage::MainFrameTitle() const
{
    return fMainFrame->Title();
}

BString BWebPage::MainFrameRequestedURL() const
{
    return fMainFrame->RequestedURL();
}

BString BWebPage::MainFrameURL() const
{
    return fMainFrame->URL();
}

status_t BWebPage::GetContentsAsMHTML(BDataIO& output)
{
    ssize_t size = 0;

    WTF::Function<void(std::span<const uint8_t>)> write = [&size, &output](const std::span<const uint8_t>& span) {
        if (size < 0)
            return;
        ssize_t tmpSize;
        tmpSize += output.Write(span.data(), span.size());
        if (tmpSize < 0)
            size = tmpSize;
        else if (tmpSize == span.size())
            size += tmpSize;
        else
            size = -1;
    };

    RefPtr<FragmentedSharedBuffer> buffer = MHTMLArchive::generateMHTMLData(fPage->ptr());
    buffer->forEachSegment(write);
    if (size > 0)
        return B_OK;
    else
        return B_ERROR;
}

// #pragma mark - BWebView API

void BWebPage::setVisible(bool visible)
{
    BMessage message(HANDLE_SET_VISIBLE);
    message.AddBool("visible", visible);
    Looper()->PostMessage(&message, this);
}

void BWebPage::draw(const BRect& updateRect)
{
    BMessage message(HANDLE_DRAW);
    message.AddPointer("target", this);
    message.AddRect("update rect", updateRect);
    Looper()->PostMessage(&message, this);
}

void BWebPage::frameResized(float width, float height)
{
    BMessage message(HANDLE_FRAME_RESIZED);
    message.AddPointer("target", this);
    message.AddFloat("width", width);
    message.AddFloat("height", height);
    Looper()->PostMessage(&message, this);
}

void BWebPage::focused(bool focused)
{
    BMessage message(HANDLE_FOCUSED);
    message.AddBool("focused", focused);
    Looper()->PostMessage(&message, this);
}

void BWebPage::activated(bool activated)
{
    BMessage message(HANDLE_ACTIVATED);
    message.AddBool("activated", activated);
    Looper()->PostMessage(&message, this);
}

void BWebPage::mouseEvent(const BMessage* message,
    const BPoint& /*where*/, const BPoint& /*screenWhere*/)
{
    BMessage copiedMessage(*message);
    copiedMessage.AddPointer("target", this);
    Looper()->PostMessage(&copiedMessage, this);
}

void BWebPage::mouseWheelChanged(const BMessage* message,
    const BPoint& where, const BPoint& screenWhere)
{
    BMessage copiedMessage(*message);
    copiedMessage.AddPoint("be:view_where", where);
    copiedMessage.AddPoint("screen_where", screenWhere);
    copiedMessage.AddInt32("modifiers", modifiers());
    Looper()->PostMessage(&copiedMessage, this);
}

void BWebPage::keyEvent(const BMessage* message)
{
    BMessage copiedMessage(*message);
    Looper()->PostMessage(&copiedMessage, this);
}

void BWebPage::standardShortcut(const BMessage* message)
{
	// Simulate a B_KEY_DOWN event. The message is not complete,
	// but enough to trigger short cut generation in EditorClientHaiku.
    const char* bytes = 0;
    switch (message->what) {
    case B_SELECT_ALL:
        bytes = "a";
       break;
    case B_CUT:
        bytes = "x";
        break;
    case B_COPY:
        bytes = "c";
        break;
    case B_PASTE:
        bytes = "v";
        break;
    case B_UNDO:
        bytes = "z";
        break;
    case B_REDO:
        bytes = "Z";
        break;
    }
    BMessage keyDownMessage(B_KEY_DOWN);
    keyDownMessage.AddInt32("modifiers", modifiers() | B_COMMAND_KEY);
    keyDownMessage.AddString("bytes", bytes);
    keyDownMessage.AddInt64("when", system_time());
    Looper()->PostMessage(&keyDownMessage, this);
}


// #pragma mark - WebCoreSupport methods

WebCore::Page* BWebPage::page() const
{
    return fPage->ptr();
}

WebCore::Page* BWebPage::createNewPage(BRect frame, bool modalDialog,
    bool resizable, bool activate, BPrivate::Network::BUrlContext* context)
{
    // Creating the BWebView in the application thread is exactly what we need anyway.
	BWebView* view = new BWebView("web view", context);
	BWebPage* page = view->WebPage();

    BMessage message(NEW_PAGE_CREATED);
    message.AddPointer("view", view);
    if (frame.IsValid())
        message.AddRect("frame", frame);
    message.AddBool("modal", modalDialog);
    message.AddBool("resizable", resizable);
    message.AddBool("activate", activate);

    // Block until some window has embedded this view.
    BMessage reply;
    fListener.SendMessage(&message, &reply);

    return page->page();
}

BRect BWebPage::windowFrame()
{
    BRect frame;
    if (fWebView->LockLooper()) {
        frame = fWebView->Window()->Frame();
        fWebView->UnlockLooper();
    }
    return frame;
}

BRect BWebPage::windowBounds()
{
    return windowFrame().OffsetToSelf(B_ORIGIN);
}

void BWebPage::setWindowBounds(const BRect& bounds)
{
	BMessage message(RESIZING_REQUESTED);
	message.AddRect("rect", bounds);
	BMessenger windowMessenger(fWebView->Window());
	if (windowMessenger.IsValid()) {
		// Better make this synchronous, since I don't know if it is
		// perhaps meant to be (called from ChromeClientHaiku::setWindowRect()).
		BMessage reply;
		windowMessenger.SendMessage(&message, &reply);
	}
}

BRect BWebPage::viewBounds()
{
    BRect bounds;
    if (fWebView->LockLooper()) {
        bounds = fWebView->Bounds();
        fWebView->UnlockLooper();
    }
    return bounds;
}

void BWebPage::setViewBounds(const BRect& /*bounds*/)
{
    if (fWebView->LockLooper()) {
        // TODO: Implement this with layout management, i.e. SetExplicitMinSize() or something...
        fWebView->UnlockLooper();
    }
}

void BWebPage::setToolbarsVisible(bool flag)
{
    fToolbarsVisible = flag;

    BMessage message(TOOLBARS_VISIBILITY);
    message.AddBool("flag", flag);
    dispatchMessage(message);
}

void BWebPage::setStatusbarVisible(bool flag)
{
    fStatusbarVisible = flag;

    BMessage message(STATUSBAR_VISIBILITY);
    message.AddBool("flag", flag);
    dispatchMessage(message);
}

void BWebPage::setMenubarVisible(bool flag)
{
    fMenubarVisible = flag;

    BMessage message(MENUBAR_VISIBILITY);
    message.AddBool("flag", flag);
    dispatchMessage(message);
}

void BWebPage::setResizable(bool flag)
{
    BMessage message(SET_RESIZABLE);
    message.AddBool("flag", flag);
    dispatchMessage(message);
}

void BWebPage::closeWindow()
{
	BMessage message(CLOSE_WINDOW_REQUESTED);
    dispatchMessage(message);
}

void BWebPage::linkHovered(const BString& url, const BString& /*title*/, const BString& /*content*/)
{
	if (url.Length())
		setDisplayedStatusMessage(url);
	else
		setDisplayedStatusMessage(fStatusMessage);
}

void BWebPage::requestDownload(const WebCore::ResourceRequest& request,
	bool isAsynchronousRequest)
{
    BWebDownload* download = new BWebDownload(new BPrivate::WebDownloadPrivate(
        request, MainFrame()->Frame()->loader().networkingContext()));
    downloadCreated(download, isAsynchronousRequest);
}

/*static*/ void BWebPage::downloadCreated(BWebDownload* download,
	bool isAsynchronousRequest)
{
	if (sDownloadListener.IsValid()) {
        BMessage message(B_DOWNLOAD_ADDED);
        message.AddPointer("download", download);
        if (isAsynchronousRequest) {
	        // Block until the listener has pulled all the information...
	        BMessage reply;
	        sDownloadListener.SendMessage(&message, &reply);
        } else {
	        sDownloadListener.SendMessage(&message);
        }
	} else {
		BPath desktopPath;
		find_directory(B_DESKTOP_DIRECTORY, &desktopPath);
        download->Start(desktopPath);
	}
}

void BWebPage::paint(BRect rect, bool immediate)
{
    if (!rect.IsValid())
        return;
    // Block any drawing as long as the BWebView is hidden
    // (should be extended to when the containing BWebWindow is not
    // currently on screen either...)
    if (!fPageVisible) {
        fPageDirty = true;
        return;
    }

    // NOTE: fMainFrame can be 0 because init() eventually ends up calling
    // paint()! BWebFrame seems to cause an initial page to be loaded, maybe
    // this ought to be avoided also for start-up speed reasons!
    if (!fMainFrame)
        return;
    WebCore::LocalFrame* frame = fMainFrame->Frame();
    WebCore::LocalFrameView* view = frame->view();

    if (!view || !frame->contentRenderer())
        return;

    page()->isolatedUpdateRendering();

    view->updateLayoutAndStyleIfNeededRecursive();

    if (!fWebView->LockLooper())
        return;
    BView* offscreenView = fWebView->OffscreenView();

    // Lock the offscreen bitmap while we still have the
    // window locked. This cannot deadlock and makes sure
    // the window is not deleting the offscreen view right
    // after we unlock it and before locking the bitmap.
    if (offscreenView == NULL || !offscreenView->LockLooper()) {
        fWebView->UnlockLooper();
        return;
    }

    fWebView->UnlockLooper();
    MainFrame()->Frame()->view()->flushCompositingStateIncludingSubframes();

    offscreenView->PushState();
    BRegion region(rect);
    offscreenView->ConstrainClippingRegion(&region);

    // TODO do not recreate a context everytime this is called, we can preserve
    // it alongside the offscreen view in BWebView?
    WebCore::GraphicsContextHaiku context(offscreenView);
    view->paint(context, IntRect(rect));

    offscreenView->PopState();
    offscreenView->Sync();
    offscreenView->UnlockLooper();

    // Notify the window that it can now pull the bitmap in its own thread
    fWebView->SetOffscreenViewClean(rect, immediate);

    fPageDirty = false;
}


void BWebPage::scroll(int xOffset, int yOffset, const BRect& rectToScroll,
       const BRect& clipRect)
{
    if (!rectToScroll.IsValid() || !clipRect.IsValid()
        || (xOffset == 0 && yOffset == 0) || !fWebView->LockLooper()) {
        return;
    }

    BBitmap* bitmap = fWebView->OffscreenBitmap();
    BView* offscreenView = fWebView->OffscreenView();

    // Lock the offscreen bitmap while we still have the
    // window locked. This cannot deadlock and makes sure
    // the window is not deleting the offscreen view right
    // after we unlock it and before locking the bitmap.
    if (!bitmap->Lock()) {
       fWebView->UnlockLooper();
       return;
    }
    fWebView->UnlockLooper();

    BRect clip = offscreenView->Bounds();
    if (clipRect.IsValid())
        clip = clip & clipRect;

    BRect rectAtSrc = rectToScroll;
    BRect rectAtDst = rectAtSrc.OffsetByCopy(xOffset, yOffset);

    if (clip.Intersects(rectAtSrc) && clip.Intersects(rectAtDst)) {
        // clip source rect
        rectAtSrc = rectAtSrc & clip;
        // clip dest rect
        rectAtDst = rectAtDst & clip;

        // move dest back over source and clip source to dest
        rectAtDst.OffsetBy(-xOffset, -yOffset);
        rectAtSrc = rectAtSrc & rectAtDst;
        rectAtDst.OffsetBy(xOffset, yOffset);

        offscreenView->CopyBits(rectAtSrc, rectAtDst);
    }

    bitmap->Unlock();
}


void BWebPage::setLoadingProgress(float progress)
{
	fLoadingProgress = progress;

	BMessage message(LOAD_PROGRESS);
	message.AddFloat("progress", progress);
	dispatchMessage(message);
}

void BWebPage::setStatusMessage(const BString& statusMessage)
{
	if (fStatusMessage == statusMessage)
		return;

	fStatusMessage = statusMessage;

	setDisplayedStatusMessage(statusMessage);
}

void BWebPage::setDisplayedStatusMessage(const BString& statusMessage, bool force)
{
	if (fDisplayedStatusMessage == statusMessage && !force)
		return;

	fDisplayedStatusMessage = statusMessage;

	BMessage message(SET_STATUS_TEXT);
	message.AddString("text", statusMessage);
	dispatchMessage(message);
}


void BWebPage::runJavaScriptAlert(const BString& text)
{
    BMessage message(SHOW_JS_ALERT);
	message.AddString("text", text);
	dispatchMessage(message);
}


bool BWebPage::runJavaScriptConfirm(const BString& text)
{
    BMessage message(SHOW_JS_CONFIRM);
	message.AddString("text", text);
    BMessage reply;
	dispatchMessage(message, &reply);

    return reply.FindBool("result");
}


void BWebPage::addMessageToConsole(const BString& source, int lineNumber,
    int columnNumber, const BString& text)
{
    BMessage message(ADD_CONSOLE_MESSAGE);
    message.AddString("source", source);
    message.AddInt32("line", lineNumber);
    message.AddInt32("column", columnNumber);
    message.AddString("string", text);
	dispatchMessage(message);
}



// #pragma mark - private

void BWebPage::MessageReceived(BMessage* message)
{
    switch (message->what) {
    case HANDLE_SHUTDOWN:
        // NOTE: This message never arrives here when the BApplication is already
        // processing B_QUIT_REQUESTED. Then the view will be detached and instruct
        // the BWebPage handler to shut itself down, but BApplication will not
        // process additional messages. That's why the windows containing WebViews
        // are detaching the views already in their QuitRequested() hooks and
        // LauncherApp calls these hooks already in its own QuitRequested() hook.
        Looper()->RemoveHandler(this);
        delete this;
        // TOAST!
        return;
    case HANDLE_LOAD_URL:
        handleLoadURL(message);
        break;
    case HANDLE_RELOAD:
    	handleReload(message);
    	break;
    case HANDLE_GO_BACK:
        handleGoBack(message);
        break;
    case HANDLE_GO_FORWARD:
        handleGoForward(message);
        break;
    case HANDLE_STOP_LOADING:
        handleStop(message);
        break;

    case HANDLE_SET_VISIBLE:
        handleSetVisible(message);
        break;

    case HANDLE_DRAW: {
        bool first = true;
        BMessageQueue* queue = Looper()->MessageQueue();
        BRect updateRect;
        message->FindRect("update rect", &updateRect);
        int32 index = 0;
        while (BMessage* nextMessage = queue->FindMessage(message->what, index)) {
            BHandler* target = 0;
            nextMessage->FindPointer("target", reinterpret_cast<void**>(&target));
            if (target != this) {
                index++;
                continue;
            }

            if (!first) {
                delete message;
                first = false;
            }

            message = nextMessage;
            queue->RemoveMessage(message);

            BRect rect;
            message->FindRect("update rect", &rect);
            updateRect = updateRect | rect;
        }
        paint(updateRect, false);
        break;
    }
    case HANDLE_FRAME_RESIZED:
        skipToLastMessage(message);
        handleFrameResized(message);
        break;

    case HANDLE_FOCUSED:
        handleFocused(message);
        break;
    case HANDLE_ACTIVATED:
        handleActivated(message);
        break;

    case B_MOUSE_MOVED:
        skipToLastMessage(message);
        // fall through
    case B_MOUSE_DOWN:
    case B_MOUSE_UP:
        handleMouseEvent(message);
        break;
    case B_MOUSE_WHEEL_CHANGED:
        handleMouseWheelChanged(message);
        break;
    case B_KEY_DOWN:
    case B_KEY_UP:
        handleKeyEvent(message);
        break;

	case HANDLE_CHANGE_ZOOM_FACTOR:
		handleChangeZoomFactor(message);
		break;
    case HANDLE_FIND_STRING:
        handleFindString(message);
        break;

	case HANDLE_SET_STATUS_MESSAGE: {
		BString status;
		if (message->FindString("string", &status) == B_OK)
			setStatusMessage(status);
		break;
	}

    case HANDLE_RESEND_NOTIFICATIONS:
        handleResendNotifications(message);
        break;
    case HANDLE_SEND_EDITING_CAPABILITIES:
        handleSendEditingCapabilities(message);
        break;
    case HANDLE_SEND_PAGE_SOURCE:
        handleSendPageSource(message);
        break;

    case B_REFS_RECEIVED: {
	FileChooser* chooser;
        if (message->FindPointer("chooser", reinterpret_cast<void**>(&chooser)) == B_OK) {
            entry_ref ref;
            BPath path;
            Vector<String> filenames;
            for (int32 i = 0; message->FindRef("refs", i, &ref) == B_OK; i++) {
                path.SetTo(&ref);
                filenames.append(String::fromUTF8(path.Path()));
            }
            chooser->chooseFiles(filenames);
        }
    	break;
    }
    case B_CANCEL: {
    	int32 oldWhat;
    	BFilePanel* panel;
    	if (message->FindPointer("source", reinterpret_cast<void**>(&panel)) == B_OK
    		&& message->FindInt32("old_what", &oldWhat) == B_OK
    		&& oldWhat == B_REFS_RECEIVED) {

            // Remember the directory so we can reuse it next time we open a
            // file panel
            entry_ref panelDirectory;
            panel->GetPanelDirectory(&panelDirectory);
            static_cast<ChromeClientHaiku&>(fPage->get().chrome().client())
                .setPanelDirectory(panelDirectory);

            // Delete the panel, it can't be reused because we can switch
            // between multi- and single-file modes.
    		delete panel;
    	}
    	break;
    }

    default:
        BHandler::MessageReceived(message);
    }
}

void BWebPage::skipToLastMessage(BMessage*& message)
{
	// NOTE: All messages that are fast-forwarded like this
	// need to be flagged with the intended target BWebPage,
	// or else we steal or process messages intended for another
	// BWebPage here!
    bool first = true;
    BMessageQueue* queue = Looper()->MessageQueue();
    int32 index = 0;
    while (BMessage* nextMessage = queue->FindMessage(message->what, index)) {
    	BHandler* target = 0;
    	nextMessage->FindPointer("target", reinterpret_cast<void**>(&target));
    	if (target != this) {
    		index++;
    	    continue;
    	}
        if (!first)
            delete message;
        message = nextMessage;
        queue->RemoveMessage(message);
        first = false;
    }
}

void BWebPage::handleLoadURL(const BMessage* message)
{
    const char* urlString;
    if (message->FindString("url", &urlString) != B_OK)
        return;

    fMainFrame->LoadURL(urlString);
}

void BWebPage::handleReload(const BMessage*)
{
    fMainFrame->Reload();
}

void BWebPage::handleGoBack(const BMessage*)
{
    fPage->get().backForward().goBack();
}

void BWebPage::handleGoForward(const BMessage*)
{
    fPage->get().backForward().goForward();
}

void BWebPage::handleStop(const BMessage*)
{
    fMainFrame->StopLoading();
}

void BWebPage::handleSetVisible(const BMessage* message)
{
    message->FindBool("visible", &fPageVisible);
    if (fMainFrame->Frame()->view())
        fMainFrame->Frame()->view()->setParentVisible(fPageVisible);
    // Trigger an internal repaint if the page was supposed to be repainted
    // while it was invisible.
    if (fPageVisible && fPageDirty)
        paint(viewBounds(), false);
}

void BWebPage::handleFrameResized(const BMessage* message)
{
    float width;
    float height;
    message->FindFloat("width", &width);
    message->FindFloat("height", &height);

    WebCore::LocalFrame* frame = fMainFrame->Frame();
    frame->view()->resize(width + 1, height + 1);
    frame->view()->forceLayout();
    frame->view()->adjustViewSize();
}

void BWebPage::handleFocused(const BMessage* message)
{
    bool focused;
    message->FindBool("focused", &focused);

    FocusController& focusController = fPage->get().focusController();
    focusController.setFocused(focused);
    if (focused && !focusController.focusedFrame())
        focusController.setFocusedFrame(fMainFrame->Frame());
}

void BWebPage::handleActivated(const BMessage* message)
{
    bool activated;
    message->FindBool("activated", &activated);

    FocusController& focusController = fPage->get().focusController();
    focusController.setActive(activated);
}


static BPopUpMenu*
createPlatformContextMenu(ContextMenu& contents)
{
    const Vector<ContextMenuItem>& items = contents.items();
    BPopUpMenu* menu = new BPopUpMenu("ContextMenu");

    for (auto& item: items) {
        BMessage* message = new BMessage(item.action());
        message->AddPointer("ContextMenuItem", &item);
        BMenuItem* native = nullptr;
        if (item.type() == ContextMenuItemType::Separator)
        {
            native = new BSeparatorItem(message);
        } else {
            native = new BMenuItem(item.title().utf8().data(), message);
            native->SetEnabled(item.enabled());
            native->SetMarked(item.checked());
        }

        if (native) {
            menu->AddItem(native);
        } else {
            delete message;
        }
    }

    return menu;
}


void BWebPage::handleMouseEvent(const BMessage* message)
{
    WebCore::LocalFrame* frame = fMainFrame->Frame();
    if (!frame->view() || !frame->document())
        return;

    PlatformMouseEvent event(message);
    switch (message->what) {
    case B_MOUSE_DOWN:
#if ENABLE(POINTER_LOCK)
        if (WebView()->EventMask() & B_POINTER_EVENTS)
        {
            // We are in mouse lock mode. Events are redirected to pointer lock.
           page()->pointerLockController().dispatchLockedMouseEvent(event,
                eventNames().mousedownEvent);
           break;
        }
#endif

        // Handle context menus, if necessary.
        if (event.button() == MouseButton::Right) {
            fPage->get().contextMenuController().clearContextMenu();

            WebCore::LocalFrame* focusedFrame = fPage->get().focusController().focusedOrMainFrame();
            if (!focusedFrame->eventHandler().sendContextMenuEvent(event)) {
                // event is swallowed.
                return;
            }
            // If the web page implements it's own context menu handling, then
            // the contextMenu() pointer will be zero. In this case, we should
            // also swallow the event.
            ContextMenu* contextMenu = fPage->get().contextMenuController().contextMenu();
            if (contextMenu) {
                BPopUpMenu* platformMenu = createPlatformContextMenu(*contextMenu);
                if (platformMenu) {
                    BPoint screenLocation(event.globalPosition().x() + 2,
                        event.globalPosition().y() + 2);
                    BMenuItem* item = platformMenu->Go(screenLocation, false,
                        true);
                    if (item) {
                        BMessage* message = item->Message();
                        ContextMenuItem* itemHandle;
                        message->FindPointer("ContextMenuItem", (void**)&itemHandle);
                        fPage->get().contextMenuController().contextMenuItemSelected(
                            itemHandle->action(), itemHandle->title());
                    }
                }
            }
        }
        // Handle regular mouse events.
        frame->eventHandler().handleMousePressEvent(event);
        break;
    case B_MOUSE_UP:
#if ENABLE(POINTER_LOCK)
        if (WebView()->EventMask() & B_POINTER_EVENTS)
        {
            // We are in mouse lock mode. Events are redirected to pointer lock.
           page()->pointerLockController().dispatchLockedMouseEvent(event,
                eventNames().mouseupEvent);
           break;
        }
#endif

        frame->eventHandler().handleMouseReleaseEvent(event);
        break;
    case B_MOUSE_MOVED:
#if ENABLE(POINTER_LOCK)
        if (WebView()->EventMask() & B_POINTER_EVENTS)
        {
            // We are in mouse lock mode. Events are redirected to pointer lock.
           page()->pointerLockController().dispatchLockedMouseEvent(event,
                eventNames().mousemoveEvent);
           break;
        }
#endif

    default:
        frame->eventHandler().mouseMoved(event);
        break;
    }
}

void BWebPage::handleMouseWheelChanged(BMessage* message)
{
    WebCore::LocalFrame* frame = fMainFrame->Frame();
    if (!frame || !frame->view() || !frame->document())
        return;

    BPoint position = message->FindPoint("be:view_where");
    BPoint globalPosition = message->FindPoint("screen_where");
    float deltaX = -message->FindFloat("be:wheel_delta_x");
    float deltaY = -message->FindFloat("be:wheel_delta_y");
    float wheelTicksX = deltaX;
    float wheelTicksY = deltaY;

    deltaX *= Scrollbar::pixelsPerLineStep();
    deltaY *= Scrollbar::pixelsPerLineStep();

    int32 modifiers = message->FindInt32("modifiers");

    PlatformWheelEvent event(IntPoint(position), IntPoint(globalPosition), deltaX, deltaY,
        wheelTicksX, wheelTicksY, ScrollByPixelWheelEvent, modifiers & B_SHIFT_KEY,
        modifiers & B_COMMAND_KEY, modifiers & B_CONTROL_KEY, modifiers & B_OPTION_KEY);
    frame->eventHandler().handleWheelEvent(event, { WheelEventProcessingSteps::SynchronousScrolling,
        WheelEventProcessingSteps::NonBlockingDOMEventDispatch });
}

void BWebPage::handleKeyEvent(BMessage* message)
{
    WebCore::LocalFrame* frame = fPage->get().focusController().focusedOrMainFrame();
    if (!frame || !frame->view() || !frame->document())
        return;

    PlatformKeyboardEvent event(message);
	// Try to let WebCore handle this event
	if (!frame->eventHandler().keyEvent(event) && message->what == B_KEY_DOWN) {
		// Handle keyboard scrolling (probably should be extracted to a method.)
		ScrollDirection direction;
		ScrollGranularity granularity;
		BString bytes = message->FindString("bytes");
		switch (bytes.ByteAt(0)) {
			case B_UP_ARROW:
				granularity = ScrollGranularity::Line;
				direction = ScrollDirection::ScrollUp;
				break;
			case B_DOWN_ARROW:
				granularity = ScrollGranularity::Line;
				direction = ScrollDirection::ScrollDown;
				break;
			case B_LEFT_ARROW:
				granularity = ScrollGranularity::Line;
				direction = ScrollDirection::ScrollLeft;
				break;
			case B_RIGHT_ARROW:
				granularity = ScrollGranularity::Line;
				direction = ScrollDirection::ScrollRight;
				break;
			case B_HOME:
				granularity = ScrollGranularity::Document;
				direction = ScrollDirection::ScrollUp;
				break;
			case B_END:
				granularity = ScrollGranularity::Document;
				direction = ScrollDirection::ScrollDown;
				break;
			case B_PAGE_UP:
				granularity = ScrollGranularity::Page;
				direction = ScrollDirection::ScrollUp;
				break;
			case B_PAGE_DOWN:
				granularity = ScrollGranularity::Page;
				direction = ScrollDirection::ScrollDown;
				break;
			default:
				return;
		}
		frame->eventHandler().scrollRecursively(direction, granularity);
	}
}

void BWebPage::handleChangeZoomFactor(BMessage* message)
{
    float increment;
    if (message->FindFloat("increment", &increment) != B_OK)
    	increment = 0;

    bool textOnly;
    if (message->FindBool("text only", &textOnly) != B_OK)
    	textOnly = true;

    if (increment > 0)
    	fMainFrame->IncreaseZoomFactor(textOnly);
    else if (increment < 0)
    	fMainFrame->DecreaseZoomFactor(textOnly);
    else
    	fMainFrame->ResetZoomFactor();
}

void BWebPage::handleFindString(BMessage* message)
{
    BMessage reply(B_FIND_STRING_RESULT);

    BString string;
    bool forward;
    bool caseSensitive;
    bool wrapSelection;
    bool startInSelection;
    if (message->FindString("string", &string) != B_OK
        || message->FindBool("forward", &forward) != B_OK
        || message->FindBool("case sensitive", &caseSensitive) != B_OK
        || message->FindBool("wrap selection", &wrapSelection) != B_OK
        || message->FindBool("start in selection", &startInSelection) != B_OK) {
        message->SendReply(&reply);
        return;
    }

    WebCore::FindOptions options;
    if (!forward)
        options.add(WebCore::FindOption::Backwards);
    if (!caseSensitive)
        options.add(WebCore::FindOption::CaseInsensitive);
    if (wrapSelection)
        options.add(WebCore::FindOption::WrapAround);
    if (startInSelection)
        options.add(WebCore::FindOption::StartInSelection);

    bool result = fMainFrame->FindString(string, options);

    reply.AddBool("result", result);
    message->SendReply(&reply);
}

void BWebPage::handleResendNotifications(BMessage*)
{
    // Prepare navigation capabilities notification
    BMessage message(UPDATE_NAVIGATION_INTERFACE);
    message.AddBool("can go backward", fPage->get().backForward().canGoBackOrForward(-1));
    message.AddBool("can go forward", fPage->get().backForward().canGoBackOrForward(1));
    WebCore::FrameLoader& loader = fMainFrame->Frame()->loader();
    message.AddBool("can stop", loader.isLoading());
    dispatchMessage(message);
    // Send loading progress and status text notifications
    setLoadingProgress(fLoadingProgress);
    setDisplayedStatusMessage(fStatusMessage, true);
    // TODO: Other notifications...
}

void BWebPage::handleSendEditingCapabilities(BMessage*)
{
    bool canCut = false;
    bool canCopy = false;
    bool canPaste = false;

    WebCore::LocalFrame* frame = fPage->get().focusController().focusedOrMainFrame();
    WebCore::Editor& editor = frame->editor();

    canCut = editor.canCut() || editor.canDHTMLCut();
    canCopy = editor.canCopy() || editor.canDHTMLCopy();
    canPaste = editor.canEdit() || editor.canDHTMLPaste();

    BMessage message(B_EDITING_CAPABILITIES_RESULT);
    message.AddBool("can cut", canCut);
    message.AddBool("can copy", canCopy);
    message.AddBool("can paste", canPaste);

    dispatchMessage(message);
}

void BWebPage::handleSendPageSource(BMessage*)
{
    BMessage message(B_PAGE_SOURCE_RESULT);
    message.AddString("source", fMainFrame->FrameSource());
    message.AddString("url", fMainFrame->URL());
    message.AddString("type", fMainFrame->MIMEType());

    dispatchMessage(message);
}

// #pragma mark -

status_t BWebPage::dispatchMessage(BMessage& message, BMessage* reply) const
{
	message.AddPointer("view", fWebView);
    if (reply)
	    return fListener.SendMessage(&message, reply);
    else
	    return fListener.SendMessage(&message);
}

