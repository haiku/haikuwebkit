/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "Connection.h"
#include "EditorState.h"
#include "RemoteLayerTreeTransaction.h"
#include "RemoteScrollingCoordinatorTransaction.h"
#include <WebCore/Color.h>
#include <WebCore/LayoutMilestone.h>
#include <optional>
#include <wtf/Vector.h>

namespace WebKit {

struct PageData {
    using TransactionCallbackID = IPC::AsyncReplyID;

    Vector<TransactionCallbackID> callbackIDs;
};

struct MainFrameData {
    ActivityStateChangeID activityStateChangeID { ActivityStateChangeAsynchronous };
    WebCore::Color themeColor;
    WebCore::Color pageExtendedBackgroundColor;
    WebCore::Color sampledPageTopColor;
    std::optional<EditorState> editorState;
    OptionSet<WebCore::LayoutMilestone> newlyReachedPaintingMilestones;
    std::optional<WebCore::FixedContainerEdges> fixedContainerEdges;
    bool isInStableState { false };
#if PLATFORM(MAC)
    Markable<WebCore::PlatformLayerIdentifier> pageScalingLayerID;
    Markable<WebCore::PlatformLayerIdentifier> scrolledContentsLayerID;
    Markable<WebCore::PlatformLayerIdentifier> mainFrameClipLayerID;
#endif
};

struct RemoteLayerTreeCommitBundle {
    using RootFrameData = std::pair<RemoteLayerTreeTransaction, RemoteScrollingCoordinatorTransaction>;

    Vector<RootFrameData> transactions;
    PageData pageData;
    std::optional<MainFrameData> mainFrameData;
    MonotonicTime startTime;
};

} // namespace WebKit
