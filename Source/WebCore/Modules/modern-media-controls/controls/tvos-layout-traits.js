/*
 * Copyright (C) 2021-2024 Apple Inc. All rights reserved.
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

class TVOSLayoutTraits extends LayoutTraits
{
    mediaControlsClass()
    {
        if (this.isFullscreen)
            return TVOSMediaControls;
        return IOSInlineMediaControls;
    }

    supportingObjectClasses()
    {
        var supportingObjectClasses = super.supportingObjectClasses();
        if (this.isFullscreen)
            supportingObjectClasses.push(MetadataSupport);
        return supportingObjectClasses;
    }

    resourceDirectory()
    {
        return "iOS";
    }

    controlsNeverAvailable()
    {
        return false;
    }

    supportsIconWithFullscreenVariant()
    {
        return false;
    }

    supportsDurationTimeLabel()
    {
        return false;
    }

    supportsAirPlay()
    {
        return false;
    }

    supportsPiP()
    {
        return false;
    }

    controlsDependOnPageScaleFactor()
    {
        return true;
    }

    skipDuration()
    {
        return 10;
    }

    promoteSubMenusWhenShowingMediaControlsContextMenu()
    {
        return false;
    }

    inheritsBorderRadius()
    {
        return true;
    }

    toString()
    {
        return `[TVOSLayoutTraits]`;
    }
}

window.layoutTraitsClasses["TVOSLayoutTraits"] = TVOSLayoutTraits;
