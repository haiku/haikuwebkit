/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "BrowsingContextGroup.h"

#include "APIPageConfiguration.h"
#include "APIWebsitePolicies.h"
#include "FrameProcess.h"
#include "PageLoadState.h"
#include "ProvisionalPageProxy.h"
#include "RemotePageProxy.h"
#include "WebFrameProxy.h"
#include "WebPageProxy.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"

namespace WebKit {

using namespace WebCore;

BrowsingContextGroup::BrowsingContextGroup() = default;

BrowsingContextGroup::~BrowsingContextGroup() = default;

RefPtr<FrameProcess> BrowsingContextGroup::sharedProcessForSite(WebsiteDataStore& websiteDataStore, API::WebsitePolicies* websitePolicies, const WebPreferences& preferences, const WebCore::Site& site, WebProcessProxy::LockdownMode lockdownMode, API::PageConfiguration& pageConfiguration, IsMainFrame isMainFrame)
{
    if (!preferences.siteIsolationEnabled() || !preferences.siteIsolationSharedProcessEnabled())
        return nullptr;
    if (isMainFrame == IsMainFrame::Yes)
        return nullptr;
    if (site.isEmpty())
        return nullptr;
    if (websitePolicies && !websitePolicies->allowSharedProcess() && !m_sharedProcessSites.contains(site))
        return nullptr;
    m_sharedProcessSites.add(site);
    if (m_sharedProcess)
        return m_sharedProcess.get();
    Ref process = pageConfiguration.protectedProcessPool()->processForSite(websiteDataStore, site, lockdownMode, pageConfiguration, ProcessSwapDisposition::Other);
    Ref frameProcess = FrameProcess::create(process, *this, std::nullopt, preferences, InjectBrowsingContextIntoProcess::Yes);
    m_sharedProcess = frameProcess.ptr();
    return frameProcess;
}

Ref<FrameProcess> BrowsingContextGroup::ensureProcessForSite(const Site& site, WebProcessProxy& process, const WebPreferences& preferences, InjectBrowsingContextIntoProcess injectBrowsingContextIntoProcess)
{
    if (preferences.siteIsolationEnabled()) {
        if (m_sharedProcess && m_sharedProcessSites.contains(site)) {
            ASSERT(&m_sharedProcess->process() == &process);
            return *m_sharedProcess;
        }
        if (RefPtr existingProcess = processForSite(site)) {
            if (existingProcess->process().coreProcessIdentifier() == process.coreProcessIdentifier())
                return existingProcess.releaseNonNull();
        }
    }

    return FrameProcess::create(process, *this, site, preferences, injectBrowsingContextIntoProcess);
}

FrameProcess* BrowsingContextGroup::processForSite(const Site& site)
{
    if (m_sharedProcessSites.contains(site))
        return m_sharedProcess.get();
    auto process = m_processMap.get(site);
    if (!process)
        return nullptr;
    if (process->process().state() == WebProcessProxy::State::Terminated)
        return nullptr;
    return process.get();
}

void BrowsingContextGroup::processDidTerminate(WebPageProxy& page, WebProcessProxy& process)
{
    if (&page.siteIsolatedProcess() == &process)
        m_pages.remove(page);
}

void BrowsingContextGroup::addFrameProcess(FrameProcess& process)
{
    addFrameProcessAndInjectPageContextIf(process, [](auto&) {
        return true;
    });
}

void BrowsingContextGroup::addFrameProcessAndInjectPageContextIf(FrameProcess& process, Function<bool(WebPageProxy&)> functor)
{
    Ref processProxy = process.process();
    auto createRemotePageIfNeeded = [&](WebPageProxy& page, const Site& site) {
        if (!functor(page))
            return;
        auto& set = m_remotePages.ensure(page, [] {
            return HashSet<Ref<RemotePageProxy>> { };
        }).iterator->value;
        Ref newRemotePage = RemotePageProxy::create(page, processProxy, site);
        newRemotePage->injectPageIntoNewProcess();
#if ASSERT_ENABLED
        for (auto& existingPage : set) {
            ASSERT(existingPage->process().coreProcessIdentifier() != newRemotePage->process().coreProcessIdentifier() || existingPage->site() != newRemotePage->site());
            ASSERT(existingPage->page() == newRemotePage->page());
        }
#endif
        set.add(WTFMove(newRemotePage));
    };

    if (process.isSharedProcess()) {
        Ref processProxy = process.process();
        for (Ref page : m_pages) {
            for (auto site : m_sharedProcessSites)
                createRemotePageIfNeeded(page, site);
        }
        return;
    }
    auto& site = *process.site();
    if (m_processMap.get(site) == &process)
        return;
    ASSERT(!m_processMap.get(site) || m_processMap.get(site)->process().state() == WebProcessProxy::State::Terminated);
    m_processMap.set(site, process);
    for (Ref page : m_pages) {
        if (site == Site(URL(page->currentURL())))
            continue;
        createRemotePageIfNeeded(page, site);
    }
}

void BrowsingContextGroup::removeFrameProcess(FrameProcess& process)
{
    if (process.isSharedProcess()) {
        m_sharedProcess = nullptr;
        m_sharedProcessSites.clear();
    } else {
        auto& site = *process.site();
        ASSERT(site.isEmpty() || m_processMap.get(site).get() == &process || process.process().state() == WebProcessProxy::State::Terminated);
        m_processMap.remove(site);
    }
    m_remotePages.removeIf([&] (auto& pair) {
        auto& set = pair.value;
        set.removeIf([&] (auto& remotePage) {
            if (remotePage->process().coreProcessIdentifier() != process.process().coreProcessIdentifier())
                return false;
            return true;
        });
        return set.isEmpty();
    });
}

void BrowsingContextGroup::addPage(WebPageProxy& page)
{
    ASSERT(!m_pages.contains(page));
    m_pages.add(page);
    auto& set = m_remotePages.ensure(page, [] {
        return HashSet<Ref<RemotePageProxy>> { };
    }).iterator->value;
    m_processMap.removeIf([&] (auto& pair) {
        auto& site = pair.key;
        auto& process = pair.value;
        if (!process) {
            ASSERT_NOT_REACHED_WITH_MESSAGE("FrameProcess should remove itself in the destructor so we should never find a null WeakPtr");
            return true;
        }

        if (process->process().coreProcessIdentifier() == page.legacyMainFrameProcess().coreProcessIdentifier())
            return false;
        Ref processProxy = process->process();
        Ref newRemotePage = RemotePageProxy::create(page, processProxy, site);
        newRemotePage->injectPageIntoNewProcess();
#if ASSERT_ENABLED
        for (auto& existingPage : set) {
            ASSERT(existingPage->process().coreProcessIdentifier() != newRemotePage->process().coreProcessIdentifier() || existingPage->site() != newRemotePage->site());
            ASSERT(existingPage->page() == newRemotePage->page());
        }
#endif
        set.add(WTFMove(newRemotePage));
        return false;
    });
}

void BrowsingContextGroup::addRemotePage(WebPageProxy& page, Ref<RemotePageProxy>&& remotePage)
{
    m_remotePages.ensure(page, [] {
        return HashSet<Ref<RemotePageProxy>> { };
    }).iterator->value.add(WTFMove(remotePage));
}

void BrowsingContextGroup::removePage(WebPageProxy& page)
{
    m_pages.remove(page);
    m_remotePages.remove(page);
}

void BrowsingContextGroup::forEachRemotePage(const WebPageProxy& page, Function<void(RemotePageProxy&)>&& function)
{
    auto it = m_remotePages.find(page);
    if (it == m_remotePages.end())
        return;
    for (Ref remotePage : it->value)
        function(remotePage);
}

RemotePageProxy* BrowsingContextGroup::remotePageInProcess(const WebPageProxy& page, const WebProcessProxy& process)
{
    auto it = m_remotePages.find(page);
    if (it == m_remotePages.end())
        return nullptr;
    for (Ref remotePage : it->value) {
        if (remotePage->process().coreProcessIdentifier() == process.coreProcessIdentifier())
            return remotePage.ptr();
    }
    return nullptr;
}

RefPtr<RemotePageProxy> BrowsingContextGroup::takeRemotePageInProcessForProvisionalPage(const WebPageProxy& page, const WebProcessProxy& process)
{
    auto it = m_remotePages.find(page);
    if (it == m_remotePages.end())
        return nullptr;
    RefPtr remotePage = remotePageInProcess(page, process);
    if (!remotePage)
        return nullptr;
    return it->value.take(remotePage.get());
}

void BrowsingContextGroup::transitionPageToRemotePage(WebPageProxy& page, const Site& openerSite)
{
    auto& set = m_remotePages.ensure(page, [] {
        return HashSet<Ref<RemotePageProxy>> { };
    }).iterator->value;

    Ref newRemotePage = RemotePageProxy::create(page, page.protectedLegacyMainFrameProcess(), openerSite, &page.messageReceiverRegistration(), page.webPageIDInMainFrameProcess());
#if ASSERT_ENABLED
    for (auto& existingPage : set) {
        ASSERT(existingPage->process().coreProcessIdentifier() != newRemotePage->process().coreProcessIdentifier() || existingPage->site() != newRemotePage->site());
        ASSERT(existingPage->page() == newRemotePage->page());
    }
#endif
    set.add(WTFMove(newRemotePage));
}

void BrowsingContextGroup::transitionProvisionalPageToRemotePage(ProvisionalPageProxy& page, const Site& provisionalNavigationFailureSite)
{
    auto& set = m_remotePages.ensure(*page.protectedPage(), [] {
        return HashSet<Ref<RemotePageProxy>> { };
    }).iterator->value;

    Ref newRemotePage = RemotePageProxy::create(*page.protectedPage(), page.protectedProcess(), provisionalNavigationFailureSite, &page.messageReceiverRegistration(), page.webPageID());
#if ASSERT_ENABLED
    for (auto& existingPage : set) {
        ASSERT(existingPage->process().coreProcessIdentifier() != newRemotePage->process().coreProcessIdentifier() || existingPage->site() != newRemotePage->site());
        ASSERT(existingPage->page() == newRemotePage->page());
    }
#endif
    set.add(WTFMove(newRemotePage));
}

bool BrowsingContextGroup::hasRemotePages(const WebPageProxy& page)
{
    auto it = m_remotePages.find(page);
    return it != m_remotePages.end() && !it->value.isEmpty();
}

} // namespace WebKit
