/*
 * Copyright (C) 2010, 2013 Apple Inc. All rights reserved.
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


#if ENABLE(VIDEO)

#include "FullscreenVideoController.h"

#include "WebView.h"
#include <WebCore/Chrome.h>
#include <WebCore/ExceptionOr.h>
#include <WebCore/FloatRoundedRect.h>
#include <WebCore/FontCascade.h>
#include <WebCore/FontSelector.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/ImageAdapter.h>
#include <WebCore/Page.h>
#include <WebCore/TextRun.h>
#include <WebCore/Timer.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringConcatenateNumbers.h>

#include <ControlLook.h>

using namespace WebCore;

static const Seconds timerInterval { 33_ms };

// HUD Size
static const int windowHeight = 59;
static const int windowWidth = 438;

// Margins and button sizes
static const int margin = 9;
static const int marginTop = 9;
static const int buttonSize = 25;
static const int buttonMiniSize = 16;
static const int volumeSliderWidth = 50;
static const int timeSliderWidth = 310;
static const int sliderHeight = 8;
static const int volumeSliderButtonSize = 10;
static const int timeSliderButtonSize = 8;
static const int textSize = 11;
static const float initialHUDPositionY = 0.9; // Initial Y position of HUD in percentage from top of screen

// Background values
static const int borderRadius = 12;
static const int borderThickness = 2;

// Colors
static constexpr auto backgroundColor = SRGBA<uint8_t> { 32, 32, 32, 160 };
static constexpr auto borderColor = SRGBA<uint8_t> { 160, 160, 160 };
static constexpr auto textColor = Color::white;

HUDButton::HUDButton(HUDButtonType type, const IntPoint& position)
    : HUDWidget(IntRect(position, IntSize()))
    , m_type(type)
    , m_showAltButton(false)
{
    const char* buttonResource = 0;
    const char* buttonResourceAlt = 0;
    switch (m_type) {
    case PlayPauseButton:
        buttonResource = "fsVideoPlay";
        buttonResourceAlt = "fsVideoPause";
        break;
    case TimeSliderButton:
        break;
    case VolumeUpButton:
        buttonResource = "fsVideoAudioVolumeHigh";
        break;
    case VolumeSliderButton:
        break;
    case VolumeDownButton:
        buttonResource = "fsVideoAudioVolumeLow";
        break;
    case ExitFullscreenButton:
        buttonResource = "fsVideoExitFullscreen";
        break;
    }

    if (buttonResource) {
        m_buttonImage = ImageAdapter::loadPlatformResource(buttonResource);
        m_rect.setWidth(m_buttonImage->width());
        m_rect.setHeight(m_buttonImage->height());
    }
    if (buttonResourceAlt)
        m_buttonImageAlt = ImageAdapter::loadPlatformResource(buttonResourceAlt);
}

void HUDButton::draw(GraphicsContext& context)
{
    Image* image = (m_showAltButton && m_buttonImageAlt) ? m_buttonImageAlt.get() : m_buttonImage.get();
    context.drawImage(*image, m_rect.location());
}

HUDSlider::HUDSlider(HUDSliderButtonShape shape, int buttonSize, const IntRect& rect)
    : HUDWidget(rect)
    , m_buttonShape(shape)
    , m_buttonSize(buttonSize)
    , m_buttonPosition(0)
    , m_dragStartOffset(0)
{
}

void HUDSlider::draw(GraphicsContext& context)
{
    BView* view = context.platformContext();
    rgb_color barColor = be_control_look->SliderBarColor(ui_color(B_PANEL_BACKGROUND_COLOR));
    // Draw gutter
    be_control_look->DrawSliderBar(view, m_rect, view->Bounds(), ui_color(B_PANEL_BACKGROUND_COLOR),
            ui_color(B_DOCUMENT_TEXT_COLOR), barColor, 1, 0, B_HORIZONTAL);

    // Draw button
    float half = static_cast<float>(m_buttonSize) / 2;

    BRect r = m_rect;
    r.left += m_buttonPosition;
    r.InsetBy(-half, -half);

    if (m_buttonShape == RoundButton) {
        be_control_look->DrawSliderThumb(view, r, view->Bounds(), ui_color(B_PANEL_BACKGROUND_COLOR),
                0, B_HORIZONTAL);
    } else {
        be_control_look->DrawSliderTriangle(view, r, view->Bounds(), ui_color(B_PANEL_BACKGROUND_COLOR),
                0, B_HORIZONTAL);
    }
}

void HUDSlider::drag(const IntPoint& point, bool start)
{
    if (start) {
        // When we start, we need to snap the slider position to the x position if we clicked the gutter.
        // But if we click the button, we need to drag relative to where we clicked down. We only need
        // to check X because we would not even get here unless Y were already inside.
        int relativeX = point.x() - m_rect.location().x();
        if (relativeX >= m_buttonPosition && relativeX <= m_buttonPosition + m_buttonSize)
            m_dragStartOffset = point.x() - m_buttonPosition;
        else
            m_dragStartOffset = m_rect.location().x() + m_buttonSize / 2;
    }

    m_buttonPosition = std::max(0, std::min(m_rect.width() - m_buttonSize, point.x() - m_dragStartOffset));
}

#if USE(CA)
class FullscreenVideoController::LayerClient : public WebCore::PlatformCALayerClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    LayerClient(FullscreenVideoController* parent) : m_parent(parent) { }

private:
    virtual void platformCALayerLayoutSublayersOfLayer(PlatformCALayer*);
    virtual bool platformCALayerRespondsToLayoutChanges() const { return true; }

    virtual void platformCALayerAnimationStarted(MonotonicTime beginTime) { }
    virtual GraphicsLayer::CompositingCoordinatesOrientation platformCALayerContentsOrientation() const { return GraphicsLayer::CompositingCoordinatesOrientation::BottomUp; }
    virtual void platformCALayerPaintContents(PlatformCALayer*, GraphicsContext&, const FloatRect&, GraphicsLayerPaintBehavior) { }
    virtual bool platformCALayerShowDebugBorders() const { return false; }
    virtual bool platformCALayerShowRepaintCounter(PlatformCALayer*) const { return false; }
    virtual int platformCALayerIncrementRepaintCount(PlatformCALayer*) { return 0; }

    virtual bool platformCALayerContentsOpaque() const { return false; }
    virtual bool platformCALayerDrawsContent() const { return false; }
    virtual void platformCALayerLayerDidDisplay(PlatformLayer*) { }
    virtual void platformCALayerDidCreateTiles(const Vector<FloatRect>&) { }
    float platformCALayerDeviceScaleFactor() const override { return 1; }

    FullscreenVideoController* m_parent;
};

void FullscreenVideoController::LayerClient::platformCALayerLayoutSublayersOfLayer(PlatformCALayer* layer) 
{
    ASSERT_ARG(layer, layer == m_parent->m_rootChild);

    HTMLVideoElement* videoElement = m_parent->m_videoElement.get();
    if (!videoElement)
        return;


    auto videoLayer = PlatformCALayer::platformCALayerForLayer(videoElement->platformLayer());
    if (!videoLayer || videoLayer->superlayer() != layer)
        return;

    FloatRect layerBounds = layer->bounds();

    FloatSize videoSize = videoElement->player()->naturalSize();
    float scaleFactor;
    if (videoSize.aspectRatio() > layerBounds.size().aspectRatio())
        scaleFactor = layerBounds.width() / videoSize.width();
    else
        scaleFactor = layerBounds.height() / videoSize.height();
    videoSize.scale(scaleFactor);

    // Calculate the centered position based on the videoBounds and layerBounds:
    FloatPoint videoPosition;
    FloatPoint videoOrigin;
    videoOrigin.setX((layerBounds.width() - videoSize.width()) * 0.5);
    videoOrigin.setY((layerBounds.height() - videoSize.height()) * 0.5);
    videoLayer->setPosition(videoOrigin);
    videoLayer->setBounds(FloatRect(FloatPoint(), videoSize));
}
#endif

FullscreenVideoController::FullscreenVideoController()
    : m_playPauseButton(HUDButton::PlayPauseButton, IntPoint((windowWidth - buttonSize) / 2, marginTop))
    , m_timeSliderButton(HUDButton::TimeSliderButton, IntPoint(0, 0))
    , m_volumeUpButton(HUDButton::VolumeUpButton, IntPoint(margin + buttonMiniSize + volumeSliderWidth + buttonMiniSize / 2, marginTop + (buttonSize - buttonMiniSize) / 2))
    , m_volumeSliderButton(HUDButton::VolumeSliderButton, IntPoint(0, 0))
    , m_volumeDownButton(HUDButton::VolumeDownButton, IntPoint(margin, marginTop + (buttonSize - buttonMiniSize) / 2))
    , m_exitFullscreenButton(HUDButton::ExitFullscreenButton, IntPoint(windowWidth - 2 * margin - buttonMiniSize, marginTop + (buttonSize - buttonMiniSize) / 2))
    , m_volumeSlider(HUDSlider::RoundButton, volumeSliderButtonSize, IntRect(IntPoint(margin + buttonMiniSize, marginTop + (buttonSize - buttonMiniSize) / 2 + buttonMiniSize / 2 - sliderHeight / 2), IntSize(volumeSliderWidth, sliderHeight)))
    , m_timeSlider(HUDSlider::DiamondButton, timeSliderButtonSize, IntRect(IntPoint(windowWidth / 2 - timeSliderWidth / 2, windowHeight - margin - sliderHeight), IntSize(timeSliderWidth, sliderHeight)))
    , m_hitWidget(0)
    , m_movingWindow(false)
    , m_timer(*this, &FullscreenVideoController::timerFired)
#if USE(CA)
    , m_layerClient(makeUnique<LayerClient>(this))
    , m_rootChild(PlatformCALayerWin::create(PlatformCALayer::LayerTypeLayer, m_layerClient.get()))
#endif
{
}

FullscreenVideoController::~FullscreenVideoController()
{
#if USE(CA)
    m_rootChild->setOwner(0);
#endif
}

void FullscreenVideoController::setVideoElement(HTMLVideoElement* videoElement)
{
    if (videoElement == m_videoElement)
        return;

    m_videoElement = videoElement;
    if (!m_videoElement) {
        // Can't do full-screen, just get out
        exitFullscreen();
    }
}

void FullscreenVideoController::enterFullscreen()
{
#if ENABLE(FULLSCREEN_API)
    if (!m_videoElement)
        return;

    createHUDWindow();
#endif
}

void FullscreenVideoController::exitFullscreen()
{
    // We previously ripped the videoElement's platform layer out
    // of its orginial layer tree to display it in our fullscreen
    // window.  Now, we need to get the layer back in its original
    // tree.
    // 
    // As a side effect of setting the player to invisible/visible,
    // the player's layer will be recreated, and will be picked up 
    // the next time the layer tree is synched.
    m_videoElement->player()->setPageIsVisible(0);
    m_videoElement->player()->setPageIsVisible(1);
}

bool FullscreenVideoController::canPlay() const
{
    return m_videoElement && m_videoElement->canPlay();
}

void FullscreenVideoController::play()
{
    if (m_videoElement)
        m_videoElement->play();
}

void FullscreenVideoController::pause()
{
    if (m_videoElement)
        m_videoElement->pause();
}

float FullscreenVideoController::volume() const
{
    return m_videoElement ? m_videoElement->volume() : 0;
}

void FullscreenVideoController::setVolume(float volume)
{
    if (m_videoElement)
        m_videoElement->setVolume(volume);
}

float FullscreenVideoController::currentTime() const
{
    return m_videoElement ? m_videoElement->currentTime() : 0;
}

void FullscreenVideoController::setCurrentTime(float value)
{
    if (m_videoElement)
        m_videoElement->setCurrentTime(value);
}

float FullscreenVideoController::duration() const
{
    return m_videoElement ? m_videoElement->duration() : 0;
}

void FullscreenVideoController::beginScrubbing()
{
    if (m_videoElement) 
        m_videoElement->beginScrubbing();
}

void FullscreenVideoController::endScrubbing()
{
    if (m_videoElement) 
        m_videoElement->endScrubbing();
}

void FullscreenVideoController::registerHUDWindowClass()
{
    static bool haveRegisteredHUDWindowClass;
    if (haveRegisteredHUDWindowClass)
        return;

    haveRegisteredHUDWindowClass = true;
}

void FullscreenVideoController::createHUDWindow()
{
#if ENABLE(FULLSCREEN_API)
    m_hudPosition.setX((m_fullscreenSize.width() - windowWidth) / 2);
    m_hudPosition.setY(m_fullscreenSize.height() * initialHUDPositionY - windowHeight / 2);

    // Local variable that will hold the returned pixels. No need to cleanup this value. It
    // will get cleaned up when m_bitmap is destroyed in the dtor
    void* pixels;

    m_playPauseButton.setShowAltButton(!canPlay());
    m_volumeSlider.setValue(volume());
    m_timeSlider.setValue(currentTime() / duration());

    if (!canPlay())
        m_timer.startRepeating(timerInterval);

    registerHUDWindowClass();

    draw();
#endif
}

static String timeToString(float time)
{
    if (!std::isfinite(time))
        time = 0;
    int seconds = fabsf(time); 
    int hours = seconds / (60 * 60);
    int minutes = (seconds / 60) % 60;
    seconds %= 60;
    if (hours)
        return makeString((time < 0 ? "-"_s : ""_s), hours, ':', pad('0', 2, minutes), ':', pad('0', 2, seconds));
    return makeString((time < 0 ? "-"_s : ""_s), pad('0', 2, minutes), ':', pad('0', 2, seconds));
}

void FullscreenVideoController::draw()
{
    // Draw the background
    IntSize outerRadius(borderRadius, borderRadius);
    IntRect outerRect(0, 0, windowWidth, windowHeight);
    IntSize innerRadius(borderRadius - borderThickness, borderRadius - borderThickness);
    IntRect innerRect(borderThickness, borderThickness, windowWidth - borderThickness * 2, windowHeight - borderThickness * 2);

    // Draw the text strings
    FontCascadeDescription desc;

    desc.setComputedSize(textSize);
    FontCascade font = FontCascade(WTFMove(desc));
    font.update(nullptr);

    String s;

    // The y positioning of these two text strings is tricky because they are so small. They
    // are currently positioned relative to the center of the slider and then down the font
    // height / 4 (which is actually half of font height /2), which positions the center of
    // the text at the center of the slider.
    // Left string
    s = timeToString(currentTime());
    int fontHeight = font.metricsOfPrimaryFont().height();
    TextRun leftText(s);

    // Right string
    s = timeToString(currentTime() - duration());
    TextRun rightText(s);
}

void FullscreenVideoController::onChar(int c)
{
	// TODO exit if escape is pressed
	// TODO play/pause if space is pressed
}

void FullscreenVideoController::onKeyDown(int virtualKey)
{
	// TODO exit if escape is pressed
}

void FullscreenVideoController::timerFired()
{
    // Update the time slider
    m_timeSlider.setValue(currentTime() / duration());
    draw();
}

void FullscreenVideoController::onMouseDown(const IntPoint& point)
{
    IntPoint convertedPoint(fullscreenToHUDCoordinates(point));

    // Don't bother hit testing if we're outside the bounds of the window
    if (convertedPoint.x() < 0 || convertedPoint.x() >= windowWidth || convertedPoint.y() < 0 || convertedPoint.y() >= windowHeight)
        return;

    m_hitWidget = 0;
    m_movingWindow = false;

    if (m_playPauseButton.hitTest(convertedPoint))
        m_hitWidget = &m_playPauseButton;
    else if (m_exitFullscreenButton.hitTest(convertedPoint))
        m_hitWidget = &m_exitFullscreenButton;
    else if (m_volumeUpButton.hitTest(convertedPoint))
        m_hitWidget = &m_volumeUpButton;
    else if (m_volumeDownButton.hitTest(convertedPoint))
        m_hitWidget = &m_volumeDownButton;
    else if (m_volumeSlider.hitTest(convertedPoint)) {
        m_hitWidget = &m_volumeSlider;
        m_volumeSlider.drag(convertedPoint, true);
        setVolume(m_volumeSlider.value());
    } else if (m_timeSlider.hitTest(convertedPoint)) {
        m_hitWidget = &m_timeSlider;
        m_timeSlider.drag(convertedPoint, true);
        beginScrubbing();
        setCurrentTime(m_timeSlider.value() * duration());
    }

    // If we did not pick any of our widgets we are starting a window move
    if (!m_hitWidget) {
        m_moveOffset = convertedPoint;
        m_movingWindow = true;
    }

    draw();
}

void FullscreenVideoController::onMouseMove(const IntPoint& point)
{
    IntPoint convertedPoint(fullscreenToHUDCoordinates(point));

    if (m_hitWidget) {
        m_hitWidget->drag(convertedPoint, false);
        if (m_hitWidget == &m_volumeSlider)
            setVolume(m_volumeSlider.value());
        else if (m_hitWidget == &m_timeSlider)
            setCurrentTime(m_timeSlider.value() * duration());
        draw();
    } else if (m_movingWindow)
        m_hudPosition.move(convertedPoint.x() - m_moveOffset.x(), convertedPoint.y() - m_moveOffset.y());
}

void FullscreenVideoController::onMouseUp(const IntPoint& point)
{
    IntPoint convertedPoint(fullscreenToHUDCoordinates(point));
    m_movingWindow = false;

    if (m_hitWidget) {
        if (m_hitWidget == &m_playPauseButton && m_playPauseButton.hitTest(convertedPoint))
            togglePlay();
        else if (m_hitWidget == &m_volumeUpButton && m_volumeUpButton.hitTest(convertedPoint)) {
            setVolume(1);
            m_volumeSlider.setValue(1);
        } else if (m_hitWidget == &m_volumeDownButton && m_volumeDownButton.hitTest(convertedPoint)) {
            setVolume(0);
            m_volumeSlider.setValue(0);
        } else if (m_hitWidget == &m_timeSlider)
            endScrubbing();
        else if (m_hitWidget == &m_exitFullscreenButton && m_exitFullscreenButton.hitTest(convertedPoint)) {
            m_hitWidget = 0;
            if (m_videoElement)
                m_videoElement->exitFullscreen();
            return;
        }
    }

    m_hitWidget = 0;
    draw();
}

void FullscreenVideoController::togglePlay()
{
    if (canPlay())
        play();
    else
        pause();

    m_playPauseButton.setShowAltButton(!canPlay());

    // Run a timer while the video is playing so we can keep the time
    // slider and time values up to date.
    if (!canPlay())
        m_timer.startRepeating(timerInterval);
    else
        m_timer.stop();

    draw();
}

#endif
