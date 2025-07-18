/*
 * Copyright (C) 2008-2025 Apple Inc. All rights reserved.
 * Copyright (C) 2009, 2011 Google Inc. All rights reserved.
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
 *
 */

#include "config.h"
#include "WorkerGlobalScope.h"

#include "CSSFontSelector.h"
#include "CSSValueList.h"
#include "CSSValuePool.h"
#include "CacheStorageProvider.h"
#include "CommonVM.h"
#include "ContentSecurityPolicy.h"
#include "CrossOriginMode.h"
#include "Crypto.h"
#include "CryptoKeyData.h"
#include "DocumentInlines.h"
#include "FontCustomPlatformData.h"
#include "FontFaceSet.h"
#include "GCController.h"
#include "IDBConnectionProxy.h"
#include "ImageBitmapOptions.h"
#include "InspectorInstrumentation.h"
#include "JSDOMExceptionHandling.h"
#include "Logging.h"
#include "NotImplemented.h"
#include "PageConsoleClient.h"
#include "Performance.h"
#include "RTCDataChannelRemoteHandlerConnection.h"
#include "ReportingScope.h"
#include "ScheduledAction.h"
#include "ScriptSourceCode.h"
#include "SecurityOrigin.h"
#include "SecurityOriginPolicy.h"
#include "ServiceWorker.h"
#include "ServiceWorkerClientData.h"
#include "ServiceWorkerGlobalScope.h"
#include "SocketProvider.h"
#include "TrustedType.h"
#include "URLKeepingBlobAlive.h"
#include "ViolationReportType.h"
#include "WindowOrWorkerGlobalScopeTrustedTypes.h"
#include "WorkerClient.h"
#include "WorkerFileSystemStorageConnection.h"
#include "WorkerFontLoadRequest.h"
#include "WorkerLoaderProxy.h"
#include "WorkerLocation.h"
#include "WorkerMessagePortChannelProvider.h"
#include "WorkerMessagingProxy.h"
#include "WorkerNavigator.h"
#include "WorkerOrWorkletGlobalScope.h"
#include "WorkerReportingProxy.h"
#include "WorkerSWClientConnection.h"
#include "WorkerScriptLoader.h"
#include "WorkerStorageConnection.h"
#include "WorkerThread.h"
#include <JavaScriptCore/ScriptArguments.h>
#include <JavaScriptCore/ScriptCallStack.h>
#include <wtf/Lock.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/WorkQueue.h>
#include <wtf/threads/BinarySemaphore.h>

#if ENABLE(WEBDRIVER_BIDI)
#include "AutomationInstrumentation.h"
#endif

namespace WebCore {
using namespace Inspector;

static Lock allWorkerGlobalScopeIdentifiersLock;
static HashSet<ScriptExecutionContextIdentifier>& allWorkerGlobalScopeIdentifiers() WTF_REQUIRES_LOCK(allWorkerGlobalScopeIdentifiersLock)
{
    static NeverDestroyed<HashSet<ScriptExecutionContextIdentifier>> identifiers;
    ASSERT(allWorkerGlobalScopeIdentifiersLock.isLocked());
    return identifiers;
}

static WorkQueue& sharedFileSystemStorageQueue()
{
    static NeverDestroyed<Ref<WorkQueue>> queue(WorkQueue::create("Shared File System Storage Queue"_s,  WorkQueue::QOS::Default));
    return queue.get();
}

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(WorkerGlobalScope);

WorkerGlobalScope::WorkerGlobalScope(WorkerThreadType type, const WorkerParameters& params, Ref<SecurityOrigin>&& origin, WorkerThread& thread, Ref<SecurityOrigin>&& topOrigin, IDBClient::IDBConnectionProxy* connectionProxy, SocketProvider* socketProvider, std::unique_ptr<WorkerClient>&& workerClient)
    : WorkerOrWorkletGlobalScope(type, params.sessionID, isMainThread() ? Ref { commonVM() } : JSC::VM::create(JSC::HeapType::Medium), params.referrerPolicy, &thread, params.noiseInjectionHashSalt, params.advancedPrivacyProtections, params.clientIdentifier)
    , m_url(params.scriptURL)
    , m_ownerURL(params.ownerURL)
    , m_inspectorIdentifier(params.inspectorIdentifier)
    , m_userAgent(params.userAgent)
    , m_isOnline(params.isOnline)
    , m_shouldBypassMainWorldContentSecurityPolicy(params.shouldBypassMainWorldContentSecurityPolicy)
    , m_topOrigin(WTFMove(topOrigin))
    , m_connectionProxy(connectionProxy)
    , m_socketProvider(socketProvider)
    , m_performance(Performance::create(this, params.timeOrigin))
    , m_reportingScope(ReportingScope::create(*this))
    , m_workerClient(WTFMove(workerClient))
    , m_settingsValues(params.settingsValues)
    , m_workerType(params.workerType)
    , m_credentials(params.credentials)
{
    {
        Locker locker { allWorkerGlobalScopeIdentifiersLock };
        allWorkerGlobalScopeIdentifiers().add(identifier());
    }

    if (m_topOrigin->hasUniversalAccess())
        origin->grantUniversalAccess();
    if (m_topOrigin->needsStorageAccessFromFileURLsQuirk())
        origin->grantStorageAccessFromFileURLsQuirk();

    setStorageBlockingPolicy(m_settingsValues.storageBlockingPolicy);
    setSecurityOriginPolicy(SecurityOriginPolicy::create(WTFMove(origin)));
    setContentSecurityPolicy(makeUnique<ContentSecurityPolicy>(URL { m_url }, *this));
    setCrossOriginEmbedderPolicy(params.crossOriginEmbedderPolicy);
}

WorkerGlobalScope::~WorkerGlobalScope()
{
    ASSERT(thread().thread() == &Thread::currentSingleton());

    {
        Locker locker { allWorkerGlobalScopeIdentifiersLock };
        allWorkerGlobalScopeIdentifiers().remove(identifier());
    }

    m_performance = nullptr;
    m_crypto = nullptr;

    // Notify proxy that we are going away. This can free the WorkerThread object, so do not access it after this.
    if (auto* workerReportingProxy = thread().workerReportingProxy())
        workerReportingProxy->workerGlobalScopeDestroyed();
}

String WorkerGlobalScope::origin() const
{
    RefPtr securityOrigin = this->securityOrigin();
    return securityOrigin ? securityOrigin->toString() : emptyString();
}

void WorkerGlobalScope::prepareForDestruction()
{
    WorkerOrWorkletGlobalScope::prepareForDestruction();

    if (auto* trustedTypes = static_cast<WorkerGlobalScopeTrustedTypes*>(requireSupplement(WorkerGlobalScopeTrustedTypes::supplementName())))
        trustedTypes->prepareForDestruction();

    if (settingsValues().serviceWorkersEnabled)
        swClientConnection().unregisterServiceWorkerClient(identifier());

    if (m_connectionProxy)
        m_connectionProxy->abortActivitiesForCurrentThread();

    if (m_storageConnection)
        m_storageConnection->scopeClosed();

    if (m_fileSystemStorageConnection)
        m_fileSystemStorageConnection->scopeClosed();
}

void WorkerGlobalScope::removeAllEventListeners()
{
    WorkerOrWorkletGlobalScope::removeAllEventListeners();
    m_performance->removeAllEventListeners();
    m_performance->removeAllObservers();
    m_reportingScope->removeAllObservers();
}

bool WorkerGlobalScope::isSecureContext() const
{
    if (!settingsValues().secureContextChecksEnabled)
        return true;

    return m_topOrigin->isPotentiallyTrustworthy();
}

void WorkerGlobalScope::applyContentSecurityPolicyResponseHeaders(const ContentSecurityPolicyResponseHeaders& contentSecurityPolicyResponseHeaders)
{
    checkedContentSecurityPolicy()->didReceiveHeaders(contentSecurityPolicyResponseHeaders, String { });
}

URL WorkerGlobalScope::completeURL(const String& url, ForceUTF8) const
{
    // Always return a null URL when passed a null string.
    // FIXME: Should we change the URL constructor to have this behavior?
    if (url.isNull())
        return URL();
    // Always use UTF-8 in Workers.
    return URL(m_url, url);
}

String WorkerGlobalScope::userAgent(const URL&) const
{
    return m_userAgent;
}

SocketProvider* WorkerGlobalScope::socketProvider()
{
    return m_socketProvider.get();
}

RefPtr<RTCDataChannelRemoteHandlerConnection> WorkerGlobalScope::createRTCDataChannelRemoteHandlerConnection()
{
    RefPtr<RTCDataChannelRemoteHandlerConnection> connection;
    callOnMainThreadAndWait([workerThread = Ref { thread() }, &connection]() mutable {
        if (auto* workerLoaderProxy = workerThread->workerLoaderProxy())
            connection = workerLoaderProxy->createRTCDataChannelRemoteHandlerConnection();
    });
    ASSERT(connection);

    return connection;
}

IDBClient::IDBConnectionProxy* WorkerGlobalScope::idbConnectionProxy()
{
    return m_connectionProxy.get();
}

GraphicsClient* WorkerGlobalScope::graphicsClient()
{
    return workerClient();
}

void WorkerGlobalScope::suspend()
{
    if (m_connectionProxy)
        m_connectionProxy->setContextSuspended(*this, true);

    if (settingsValues().serviceWorkersEnabled)
        swClientConnection().unregisterServiceWorkerClient(identifier());
}

void WorkerGlobalScope::resume()
{
    if (settingsValues().serviceWorkersEnabled)
        updateServiceWorkerClientData();

    if (m_connectionProxy)
        m_connectionProxy->setContextSuspended(*this, false);
}

WorkerStorageConnection& WorkerGlobalScope::storageConnection()
{
    if (!m_storageConnection)
        lazyInitialize(m_storageConnection, WorkerStorageConnection::create(*this));

    return *m_storageConnection;
}

void WorkerGlobalScope::postFileSystemStorageTask(Function<void()>&& task)
{
    sharedFileSystemStorageQueue().dispatch(WTFMove(task));
}

WorkerFileSystemStorageConnection& WorkerGlobalScope::getFileSystemStorageConnection(Ref<FileSystemStorageConnection>&& mainThreadConnection)
{
    if (!m_fileSystemStorageConnection)
        m_fileSystemStorageConnection = WorkerFileSystemStorageConnection::create(*this, WTFMove(mainThreadConnection));
    else if (m_fileSystemStorageConnection->mainThreadConnection() != mainThreadConnection.ptr()) {
        m_fileSystemStorageConnection->connectionClosed();
        m_fileSystemStorageConnection = WorkerFileSystemStorageConnection::create(*this, WTFMove(mainThreadConnection));
    }

    return *m_fileSystemStorageConnection;
}

WorkerFileSystemStorageConnection* WorkerGlobalScope::fileSystemStorageConnection()
{
    return m_fileSystemStorageConnection.get();
}

WorkerLocation& WorkerGlobalScope::location() const
{
    if (!m_location)
        lazyInitialize(m_location, WorkerLocation::create(URL { m_url }, origin()));
    return *m_location;
}

void WorkerGlobalScope::close()
{
    if (isClosing())
        return;

    // Let current script run to completion but prevent future script evaluations.
    // After m_closing is set, all the tasks in the queue continue to be fetched but only
    // tasks with isCleanupTask()==true will be executed.
    markAsClosing();
    postTask({ ScriptExecutionContext::Task::CleanupTask, [] (ScriptExecutionContext& context) {
        ASSERT_WITH_SECURITY_IMPLICATION(is<WorkerGlobalScope>(context));
        WorkerGlobalScope& workerGlobalScope = downcast<WorkerGlobalScope>(context);
        // Notify parent that this context is closed. Parent is responsible for calling WorkerThread::stop().
        if (auto* workerReportingProxy = workerGlobalScope.thread().workerReportingProxy())
            workerReportingProxy->workerGlobalScopeClosed();
    } });
}

WorkerNavigator& WorkerGlobalScope::navigator()
{
    if (!m_navigator)
        lazyInitialize(m_navigator, WorkerNavigator::create(*this, m_userAgent, m_isOnline));
    return *m_navigator;
}

Ref<WorkerNavigator> WorkerGlobalScope::protectedNavigator()
{
    return navigator();
}

void WorkerGlobalScope::setIsOnline(bool isOnline)
{
    m_isOnline = isOnline;
    if (m_navigator)
        m_navigator->setIsOnline(isOnline);
}

ExceptionOr<int> WorkerGlobalScope::setTimeout(std::unique_ptr<ScheduledAction> action, int timeout, FixedVector<JSC::Strong<JSC::Unknown>>&& arguments)
{
    // FIXME: Should this check really happen here? Or should it happen when code is about to eval?
    if (action->type() == ScheduledAction::Type::Code) {
        if (!checkedContentSecurityPolicy()->allowEval(globalObject(), LogToConsole::Yes, action->code()))
            return 0;
    }

    action->addArguments(WTFMove(arguments));

    return DOMTimer::install(*this, WTFMove(action), Seconds::fromMilliseconds(timeout), DOMTimer::Type::SingleShot);
}

void WorkerGlobalScope::clearTimeout(int timeoutId)
{
    DOMTimer::removeById(*this, timeoutId);
}

ExceptionOr<int> WorkerGlobalScope::setInterval(std::unique_ptr<ScheduledAction> action, int timeout, FixedVector<JSC::Strong<JSC::Unknown>>&& arguments)
{
    // FIXME: Should this check really happen here? Or should it happen when code is about to eval?
    if (action->type() == ScheduledAction::Type::Code) {
        if (!checkedContentSecurityPolicy()->allowEval(globalObject(), LogToConsole::Yes, action->code()))
            return 0;
    }

    action->addArguments(WTFMove(arguments));

    return DOMTimer::install(*this, WTFMove(action), Seconds::fromMilliseconds(timeout), DOMTimer::Type::Repeating);
}

void WorkerGlobalScope::clearInterval(int timeoutId)
{
    DOMTimer::removeById(*this, timeoutId);
}

ExceptionOr<void> WorkerGlobalScope::importScripts(const FixedVector<Variant<RefPtr<TrustedScriptURL>, String>>& urls)
{
    ASSERT(contentSecurityPolicy());

    Vector<String> urlStrings;
    urlStrings.reserveInitialCapacity(urls.size());
    for (auto&& entry : urls) {
        auto stringValueHolder = WTF::switchOn(entry,
            [this](const String& str) -> ExceptionOr<String> {
                return trustedTypeCompliantString(TrustedType::TrustedScriptURL, *this, str, "WorkerGlobalScope importScripts"_s);
            },
            [](const RefPtr<TrustedScriptURL>& trustedScriptURL) -> ExceptionOr<String> {
                return trustedScriptURL->toString();
            }
        );

        if (stringValueHolder.hasException())
            return stringValueHolder.releaseException();

        urlStrings.append(stringValueHolder.releaseReturnValue());
    }

    // https://html.spec.whatwg.org/multipage/workers.html#importing-scripts-and-libraries
    // 1. If worker global scope's type is "module", throw a TypeError exception.
    if (m_workerType == WorkerType::Module)
        return Exception { ExceptionCode::TypeError, "importScripts cannot be used if worker type is \"module\""_s };

    Vector<URLKeepingBlobAlive> completedURLs;
    completedURLs.reserveInitialCapacity(urls.size());
    for (auto& entry : urlStrings) {
        URL url = completeURL(entry);
        if (!url.isValid())
            return Exception { ExceptionCode::SyntaxError };
        completedURLs.append({ WTFMove(url), m_topOrigin->data() });
    }

    FetchOptions::Cache cachePolicy = FetchOptions::Cache::Default;

    if (auto* serviceWorkerGlobalScope = dynamicDowncast<ServiceWorkerGlobalScope>(*this)) {
        // FIXME: We need to add support for the 'imported scripts updated' flag as per:
        // https://w3c.github.io/ServiceWorker/#importscripts
        auto& registration = serviceWorkerGlobalScope->registration();
        if (registration.updateViaCache() == ServiceWorkerUpdateViaCache::None || registration.needsUpdate())
            cachePolicy = FetchOptions::Cache::NoCache;
    }

    for (auto& url : completedURLs) {
        // FIXME: Convert this to check the isolated world's Content Security Policy once webkit.org/b/104520 is solved.
        bool shouldBypassMainWorldContentSecurityPolicy = this->shouldBypassMainWorldContentSecurityPolicy();
        if (!shouldBypassMainWorldContentSecurityPolicy && !checkedContentSecurityPolicy()->allowScriptFromSource(url))
            return Exception { ExceptionCode::NetworkError };

        auto scriptLoader = WorkerScriptLoader::create();
        auto cspEnforcement = shouldBypassMainWorldContentSecurityPolicy ? ContentSecurityPolicyEnforcement::DoNotEnforce : ContentSecurityPolicyEnforcement::EnforceScriptSrcDirective;
        if (auto exception = scriptLoader->loadSynchronously(this, url, WorkerScriptLoader::Source::ClassicWorkerImport, FetchOptions::Mode::NoCors, cachePolicy, cspEnforcement, resourceRequestIdentifier()))
            return WTFMove(*exception);

        // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-classic-worker-imported-script (step 7).
        bool mutedErrors = scriptLoader->responseTainting() == ResourceResponse::Tainting::Opaque || scriptLoader->responseTainting() == ResourceResponse::Tainting::Opaqueredirect;

        WeakPtr<ScriptBufferSourceProvider> sourceProvider;
        {
            NakedPtr<JSC::Exception> exception;
            ScriptSourceCode sourceCode(scriptLoader->script(), URL(scriptLoader->responseURL()), scriptLoader->isRedirected() ? URL(scriptLoader->url()) : URL());
            sourceProvider = static_cast<ScriptBufferSourceProvider&>(sourceCode.provider());
            script()->evaluate(sourceCode, exception);
            if (exception) {
                if (mutedErrors)
                    return Exception { ExceptionCode::NetworkError, "Network response is CORS-cross-origin"_s };
                script()->setException(exception);
                return { };
            }
        }
        if (sourceProvider)
            addImportedScriptSourceProvider(url, *sourceProvider);
    }

    return { };
}

EventTarget* WorkerGlobalScope::errorEventTarget()
{
    return this;
}

void WorkerGlobalScope::logExceptionToConsole(const String& errorMessage, const String& sourceURL, int lineNumber, int columnNumber, RefPtr<ScriptCallStack>&&)
{
    if (auto* workerReportingProxy = thread().workerReportingProxy())
        workerReportingProxy->postExceptionToWorkerObject(errorMessage, lineNumber, columnNumber, sourceURL);
}

void WorkerGlobalScope::addConsoleMessage(std::unique_ptr<Inspector::ConsoleMessage>&& message)
{
    if (!isContextThread()) {
        postTask(AddConsoleMessageTask(message->source(), message->level(), message->message()));
        return;
    }

    auto sessionID = this->sessionID();
    if (settingsValues().logsPageMessagesToSystemConsoleEnabled && sessionID && !sessionID->isEphemeral()) [[unlikely]]
        PageConsoleClient::logMessageToSystemConsole(*message);

#if ENABLE(WEBDRIVER_BIDI)
    AutomationInstrumentation::addMessageToConsole(message);
#endif
    InspectorInstrumentation::addMessageToConsole(*this, WTFMove(message));
}

void WorkerGlobalScope::addConsoleMessage(MessageSource source, MessageLevel level, const String& message, unsigned long requestIdentifier)
{
    addMessage(source, level, message, { }, 0, 0, nullptr, nullptr, requestIdentifier);
}

void WorkerGlobalScope::addMessage(MessageSource source, MessageLevel level, const String& messageText, const String& sourceURL, unsigned lineNumber, unsigned columnNumber, RefPtr<ScriptCallStack>&& callStack, JSC::JSGlobalObject* state, unsigned long requestIdentifier)
{
    if (!isContextThread()) {
        postTask(AddConsoleMessageTask(source, level, messageText));
        return;
    }

    std::unique_ptr<Inspector::ConsoleMessage> message;
    if (callStack)
        message = makeUnique<Inspector::ConsoleMessage>(source, MessageType::Log, level, messageText, callStack.releaseNonNull(), requestIdentifier);
    else
        message = makeUnique<Inspector::ConsoleMessage>(source, MessageType::Log, level, messageText, sourceURL, lineNumber, columnNumber, state, requestIdentifier);

#if ENABLE(WEBDRIVER_BIDI)
    AutomationInstrumentation::addMessageToConsole(message);
#endif
    InspectorInstrumentation::addMessageToConsole(*this, WTFMove(message));
}

std::optional<Vector<uint8_t>> WorkerGlobalScope::serializeAndWrapCryptoKey(CryptoKeyData&& keyData)
{
    Ref protectedThis { *this };
    auto* workerLoaderProxy = thread().workerLoaderProxy();
    if (!workerLoaderProxy)
        return std::nullopt;

    BinarySemaphore semaphore;
    std::optional<Vector<uint8_t>> wrappedKey;
    workerLoaderProxy->postTaskToLoader([&semaphore, &wrappedKey, keyData = crossThreadCopy(WTFMove(keyData))](auto& context) mutable  {
        wrappedKey = context.serializeAndWrapCryptoKey(WTFMove(keyData));
        semaphore.signal();
    });
    semaphore.wait();
    return wrappedKey;
}

std::optional<Vector<uint8_t>> WorkerGlobalScope::unwrapCryptoKey(const Vector<uint8_t>& wrappedKey)
{
    Ref protectedThis { *this };
    auto* workerLoaderProxy = thread().workerLoaderProxy();
    if (!workerLoaderProxy)
        return std::nullopt;

    BinarySemaphore semaphore;
    std::optional<Vector<uint8_t>> key;
    workerLoaderProxy->postTaskToLoader([&semaphore, &key, &wrappedKey](auto& context) {
        key = context.unwrapCryptoKey(wrappedKey);
        semaphore.signal();
    });
    semaphore.wait();
    return key;
}

Crypto& WorkerGlobalScope::crypto()
{
    if (!m_crypto)
        m_crypto = Crypto::create(this);
    return *m_crypto;
}

Performance& WorkerGlobalScope::performance() const
{
    return *m_performance;
}

Ref<Performance> WorkerGlobalScope::protectedPerformance() const
{
    return *m_performance;
}

CacheStorageConnection& WorkerGlobalScope::cacheStorageConnection()
{
    if (!m_cacheStorageConnection) {
        RefPtr<CacheStorageConnection> mainThreadConnection;
        callOnMainThreadAndWait([workerThread = Ref { thread() }, &mainThreadConnection]() mutable {
            if (workerThread->runLoop().terminated())
                return;
            if (auto* workerLoaderProxy = workerThread->workerLoaderProxy())
                mainThreadConnection = workerLoaderProxy->createCacheStorageConnection();
        });
        if (!mainThreadConnection) {
            RELEASE_LOG_INFO(ServiceWorker, "Creating worker dummy CacheStorageConnection");
            mainThreadConnection = CacheStorageProvider::DummyCacheStorageConnection::create();
        }
        lazyInitialize(m_cacheStorageConnection, mainThreadConnection.releaseNonNull());
    }
    return *m_cacheStorageConnection;
}

MessagePortChannelProvider& WorkerGlobalScope::messagePortChannelProvider()
{
    if (!m_messagePortChannelProvider)
        lazyInitialize(m_messagePortChannelProvider, makeUnique<WorkerMessagePortChannelProvider>(*this));
    return *m_messagePortChannelProvider;
}

WorkerSWClientConnection& WorkerGlobalScope::swClientConnection()
{
    if (!m_swClientConnection)
        lazyInitialize(m_swClientConnection, WorkerSWClientConnection::create(*this));
    return *m_swClientConnection;
}

void WorkerGlobalScope::createImageBitmap(ImageBitmap::Source&& source, ImageBitmapOptions&& options, ImageBitmap::Promise&& promise)
{
    ImageBitmap::createPromise(*this, WTFMove(source), WTFMove(options), WTFMove(promise));
}

void WorkerGlobalScope::createImageBitmap(ImageBitmap::Source&& source, int sx, int sy, int sw, int sh, ImageBitmapOptions&& options, ImageBitmap::Promise&& promise)
{
    ImageBitmap::createPromise(*this, WTFMove(source), WTFMove(options), sx, sy, sw, sh, WTFMove(promise));
}

CSSValuePool& WorkerGlobalScope::cssValuePool()
{
    if (!m_cssValuePool)
        m_cssValuePool = makeUnique<CSSValuePool>();
    return *m_cssValuePool;
}

CSSFontSelector* WorkerGlobalScope::cssFontSelector()
{
    if (!m_cssFontSelector)
        lazyInitialize(m_cssFontSelector, CSSFontSelector::create(*this));
    return m_cssFontSelector.get();
}

Ref<FontFaceSet> WorkerGlobalScope::fonts()
{
    ASSERT(cssFontSelector());
    return cssFontSelector()->fontFaceSet();
}

std::unique_ptr<FontLoadRequest> WorkerGlobalScope::fontLoadRequest(const String& url, bool, bool, LoadedFromOpaqueSource loadedFromOpaqueSource)
{
    return makeUnique<WorkerFontLoadRequest>(completeURL(url), loadedFromOpaqueSource);
}

void WorkerGlobalScope::beginLoadingFontSoon(FontLoadRequest& request)
{
    downcast<WorkerFontLoadRequest>(request).load(*this);
}

WorkerThread& WorkerGlobalScope::thread() const
{
    return *static_cast<WorkerThread*>(workerOrWorkletThread());
}

Ref<WorkerThread> WorkerGlobalScope::protectedThread() const
{
    return thread();
}

void WorkerGlobalScope::releaseMemory(Synchronous synchronous)
{
    ASSERT(isContextThread());
    deleteJSCodeAndGC(synchronous);
    clearDecodedScriptData();
}

void WorkerGlobalScope::deleteJSCodeAndGC(Synchronous synchronous)
{
    ASSERT(isContextThread());

    JSC::JSLockHolder lock(vm());
    vm().deleteAllCode(JSC::DeleteAllCodeIfNotCollecting);

    if (synchronous == Synchronous::Yes) {
        if (!vm().heap.currentThreadIsDoingGCWork()) {
            vm().heap.collectNow(JSC::Sync, JSC::CollectionScope::Full);
            WTF::releaseFastMallocFreeMemory();
            return;
        }
    }
#if PLATFORM(IOS_FAMILY)
    if (!vm().heap.currentThreadIsDoingGCWork()) {
        vm().heap.collectNowFullIfNotDoneRecently(JSC::Async);
        return;
    }
#endif
    vm().heap.reportAbandonedObjectGraph();
}

void WorkerGlobalScope::releaseMemoryInWorkers(Synchronous synchronous)
{
    Locker locker { allWorkerGlobalScopeIdentifiersLock };
    for (auto& globalScopeIdentifier : allWorkerGlobalScopeIdentifiers()) {
        postTaskTo(globalScopeIdentifier, [synchronous](auto& context) {
            downcast<WorkerGlobalScope>(context).releaseMemory(synchronous);
        });
    }
}

void WorkerGlobalScope::dumpGCHeapForWorkers()
{
    Locker locker { allWorkerGlobalScopeIdentifiersLock };
    for (auto& globalScopeIdentifier : allWorkerGlobalScopeIdentifiers()) {
        postTaskTo(globalScopeIdentifier, [](auto& context) {
            GCController::dumpHeapForVM(downcast<WorkerGlobalScope>(context).vm());
        });
    }
}

void WorkerGlobalScope::setMainScriptSourceProvider(ScriptBufferSourceProvider& provider)
{
    ASSERT(!m_mainScriptSourceProvider);
    m_mainScriptSourceProvider = provider;
}

void WorkerGlobalScope::addImportedScriptSourceProvider(const URL& url, ScriptBufferSourceProvider& provider)
{
    m_importedScriptsSourceProviders.ensure(url, [] {
        return WeakHashSet<ScriptBufferSourceProvider> { };
    }).iterator->value.add(provider);
}

void WorkerGlobalScope::reportErrorToWorkerObject(const String& errorMessage)
{
    if (auto* workerReportingProxy = thread().workerReportingProxy())
        workerReportingProxy->reportErrorToWorkerObject(errorMessage);
}

void WorkerGlobalScope::clearDecodedScriptData()
{
    ASSERT(isContextThread());

    if (m_mainScriptSourceProvider)
        m_mainScriptSourceProvider->clearDecodedData();

    for (auto& sourceProviders : m_importedScriptsSourceProviders.values()) {
        for (Ref sourceProvider : sourceProviders)
            sourceProvider->clearDecodedData();
    }
}

bool WorkerGlobalScope::crossOriginIsolated() const
{
    return ScriptExecutionContext::crossOriginMode() == CrossOriginMode::Isolated;
}

void WorkerGlobalScope::updateSourceProviderBuffers(const ScriptBuffer& mainScript, const HashMap<URL, ScriptBuffer>& importedScripts)
{
    ASSERT(isContextThread());

    if (mainScript && m_mainScriptSourceProvider)
        m_mainScriptSourceProvider->tryReplaceScriptBuffer(mainScript);

    for (auto& pair : importedScripts) {
        auto it = m_importedScriptsSourceProviders.find(pair.key);
        if (it == m_importedScriptsSourceProviders.end())
            continue;
        for (Ref sourceProvider : it->value)
            sourceProvider->tryReplaceScriptBuffer(pair.value);
    }
}

void WorkerGlobalScope::updateServiceWorkerClientData()
{
    if (!settingsValues().serviceWorkersEnabled)
        return;

    ASSERT(type() == WebCore::WorkerGlobalScope::Type::DedicatedWorker || type() == WebCore::WorkerGlobalScope::Type::SharedWorker);
    auto controllingServiceWorkerRegistrationIdentifier = activeServiceWorker() ? std::make_optional<ServiceWorkerRegistrationIdentifier>(activeServiceWorker()->registrationIdentifier()) : std::nullopt;
    swClientConnection().registerServiceWorkerClient(clientOrigin(), ServiceWorkerClientData::from(*this), controllingServiceWorkerRegistrationIdentifier, String { m_userAgent });
}

void WorkerGlobalScope::notifyReportObservers(Ref<Report>&& reports)
{
    reportingScope().notifyReportObservers(WTFMove(reports));
}

String WorkerGlobalScope::endpointURIForToken(const String& token) const
{
    return reportingScope().endpointURIForToken(token);
}

void WorkerGlobalScope::sendReportToEndpoints(const URL&, std::span<const String> /*endpointURIs*/, std::span<const String> /*endpointTokens*/, Ref<FormData>&&, ViolationReportType)
{
    notImplemented();
}

} // namespace WebCore
