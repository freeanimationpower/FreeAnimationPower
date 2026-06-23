#include "timeline_panel_v2.hpp"

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QMenu>
#include <QtGui/QPainter>
#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>
#include <QtGui/QResizeEvent>
#include <QtGui/QContextMenuEvent>
#include <QtGui/QFont>
#include <QtGui/QImage>
#include <algorithm>

#include "core/app_state.hpp"
#include "core/document.hpp"
#include "engine/animation/frame_thumbnail.hpp"

namespace fap {

static const QColor kBgColor("#14161C");
static const QColor kCellBg("#1A1D24");
static const QColor kCellBorder("#2A2D38");
static const QColor kCellActiveBorder("#FF6B4A");
static const QColor kCellActiveBg("#1F2230");
static const QColor kCellHoverBg("#222538");
static const QColor kFrameNumColor("#6B7088");
static const QColor kPlayheadColor("#FF6B4A");
static const QColor kBtnBg("#1E2130");
static const QColor kBtnHover("#252838");
static const QColor kBtnPressed("#FF6B4A");
static const QColor kBtnText("#C8CCD8");
static const QColor kTransportBg("#0F1115");
static const QColor kScrollBg("#14161C");
static const QColor kScrollHandle("#2A2D38");
static const QColor kAddBtnBg("#1A2D20");
static const QColor kAddBtnHover("#1F3A28");

TimelinePanelV2::TimelinePanelV2(std::shared_ptr<AppState> state, QWidget* parent)
    : QWidget(parent)
    , appState_(std::move(state))
{
    setMouseTracking(true);
    setMinimumHeight(120);
    setMaximumHeight(140);

    if (appState_) {
        auto& doc = appState_->document();
        totalFrames_ = doc.totalFrames();
        fps_ = doc.fps();
    }

    playbackTimer_ = new QTimer(this);
    playbackTimer_->setTimerType(Qt::PreciseTimer);
    connect(playbackTimer_, &QTimer::timeout, this, &TimelinePanelV2::onPlaybackTick);

    setupUI();
}

void TimelinePanelV2::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 2, 4, 2);
    mainLayout->setSpacing(2);

    auto* topBar = new QHBoxLayout();
    topBar->setSpacing(4);

    auto makeBtn = [&](const QString& text, const QString& tip, int w = 26, int h = 22) -> QPushButton* {
        auto* btn = new QPushButton(text, this);
        btn->setFixedSize(w, h);
        btn->setToolTip(tip);
        btn->setStyleSheet(QString(
            "QPushButton { background:%1; color:%2; border:none; border-radius:3px; "
            "font-size:10px; font-weight:bold; }"
            "QPushButton:hover { background:%3; }"
            "QPushButton:pressed { background:%4; color:#fff; }")
            .arg(kBtnBg.name(), kBtnText.name(), kBtnHover.name(), kBtnPressed.name()));
        return btn;
    };

    prevBtn_ = makeBtn("\u25C0", "Previous Frame (,)");
    topBar->addWidget(prevBtn_);

    playBtn_ = makeBtn("\u25B6", "Play / Pause (Space)");
    topBar->addWidget(playBtn_);

    stopBtn_ = makeBtn("\u25A0", "Stop");
    topBar->addWidget(stopBtn_);

    nextBtn_ = makeBtn("\u25B6", "Next Frame (.)");
    topBar->addWidget(nextBtn_);

    topBar->addSpacing(6);

    auto* fpsLabel = new QLabel("FPS", this);
    fpsLabel->setStyleSheet(QString("color:%1; font-size:7px; font-weight:600;").arg(kFrameNumColor.name()));
    topBar->addWidget(fpsLabel);

    fpsSpin_ = new QSpinBox(this);
    fpsSpin_->setRange(1, 120);
    fpsSpin_->setValue(fps_);
    fpsSpin_->setFixedWidth(42);
    fpsSpin_->setStyleSheet(QString(
        "QSpinBox { background:%1; color:%2; border:1px solid %3; border-radius:3px; "
        "padding:1px 3px; font-size:10px; font-family:'JetBrains Mono',monospace; }"
        "QSpinBox:focus { border-color:%4; }")
        .arg(kBtnBg.name(), kBtnText.name(), kCellBorder.name(), kPlayheadColor.name()));
    topBar->addWidget(fpsSpin_);

    topBar->addSpacing(6);

    frameLabel_ = new QLabel("1 / 24", this);
    frameLabel_->setStyleSheet(QString(
        "color:%1; font-size:9px; font-family:'JetBrains Mono',monospace;").arg(kBtnText.name()));
    topBar->addWidget(frameLabel_);

    topBar->addStretch();

    mainLayout->addLayout(topBar);

    mainLayout->addStretch();

    connect(prevBtn_, &QPushButton::clicked, this, &TimelinePanelV2::onPrevFrame);
    connect(nextBtn_, &QPushButton::clicked, this, &TimelinePanelV2::onNextFrame);
    connect(playBtn_, &QPushButton::clicked, this, &TimelinePanelV2::onPlayPause);
    connect(stopBtn_, &QPushButton::clicked, this, &TimelinePanelV2::onStop);
    connect(fpsSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &TimelinePanelV2::onFPSChanged);
}

void TimelinePanelV2::setTotalFrames(int count)
{
    totalFrames_ = std::max(1, count);
    if (currentFrame_ >= totalFrames_) currentFrame_ = totalFrames_ - 1;
    int maxScroll = std::max(0, totalFrames_ * (cellWidth_ + cellSpacing_) - (width() - kTransportWidth - kFpsWidth - kLabelWidth - 20));
    scrollOffset_ = std::min(scrollOffset_, maxScroll);
    updateLabels();
    update();
}

void TimelinePanelV2::setCurrentFrame(int frame)
{
    currentFrame_ = std::clamp(frame, 0, totalFrames_ - 1);

    int stripWidth = width() - kTransportWidth - kFpsWidth - kLabelWidth - 20;
    int cellTotal = cellWidth_ + cellSpacing_;
    int frameLeft = currentFrame_ * cellTotal - scrollOffset_;
    int frameRight = frameLeft + cellWidth_;
    if (frameLeft < 0) {
        scrollOffset_ = std::max(0, scrollOffset_ + frameLeft - cellTotal);
    } else if (frameRight > stripWidth) {
        scrollOffset_ += frameRight - stripWidth + cellTotal;
    }

    int maxScroll = std::max(0, totalFrames_ * cellTotal - stripWidth);
    scrollOffset_ = std::min(scrollOffset_, maxScroll);

    updateLabels();
    update();
}

void TimelinePanelV2::setFPS(int fps)
{
    fps_ = std::max(1, fps);
    fpsSpin_->blockSignals(true);
    fpsSpin_->setValue(fps_);
    fpsSpin_->blockSignals(false);
    if (playbackTimer_->isActive()) {
        playbackTimer_->setInterval(1000 / fps_);
    }
    updateLabels();
}

void TimelinePanelV2::invalidateFrameThumbnail(int frame)
{
    if (appState_) {
        appState_->thumbnailCache().invalidateFrame(frame);
    }
    update();
}

void TimelinePanelV2::togglePlayback()
{
    onPlayPause();
}

int TimelinePanelV2::frameToX(int frame) const
{
    return txPort_ + frame * (cellWidth_ + cellSpacing_) - scrollOffset_;
}

int TimelinePanelV2::frameAtPos(int x) const
{
    int rel = x - txPort_ + scrollOffset_;
    if (rel < 0) return -1;
    int frame = rel / (cellWidth_ + cellSpacing_);
    int posInCell = rel - frame * (cellWidth_ + cellSpacing_);
    if (posInCell >= cellWidth_) return -1;
    if (frame >= totalFrames_) return -1;
    return frame;
}

int TimelinePanelV2::visibleFrameCount() const
{
    int stripWidth = width() - kTransportWidth - kFpsWidth - kLabelWidth - 20;
    return (stripWidth + cellSpacing_) / (cellWidth_ + cellSpacing_);
}

void TimelinePanelV2::updateLabels()
{
    frameLabel_->setText(QString("%1 / %2").arg(currentFrame_ + 1).arg(totalFrames_));
}

void TimelinePanelV2::onPlayPause()
{
    if (playing_) {
        playbackTimer_->stop();
        playing_ = false;
        playBtn_->setText("\u25B6");
        emit playbackToggled(false);
    } else {
        playbackTimer_->start(1000 / fps_);
        playing_ = true;
        playBtn_->setText("\u23F8");
        emit playbackToggled(true);
    }
}

void TimelinePanelV2::onStop()
{
    playbackTimer_->stop();
    playing_ = false;
    playBtn_->setText("\u25B6");
    currentFrame_ = 0;
    scrollOffset_ = 0;
    appState_->setCurrentFrame(0);
    updateLabels();
    update();
    emit frameChanged(currentFrame_);
    emit playbackToggled(false);
}

void TimelinePanelV2::onPrevFrame()
{
    if (currentFrame_ > 0) {
        currentFrame_--;
        appState_->setCurrentFrame(currentFrame_);
        updateLabels();
        update();
        emit frameChanged(currentFrame_);
    }
}

void TimelinePanelV2::onNextFrame()
{
    if (currentFrame_ < totalFrames_ - 1) {
        currentFrame_++;
        appState_->setCurrentFrame(currentFrame_);
        updateLabels();
        update();
        emit frameChanged(currentFrame_);
    }
}

void TimelinePanelV2::onFPSChanged(int fps)
{
    fps_ = std::max(1, fps);
    appState_->document().setFPS(fps_);
    if (playbackTimer_->isActive()) {
        playbackTimer_->setInterval(1000 / fps_);
    }
    updateLabels();
    emit fpsChanged(fps_);
}

void TimelinePanelV2::onPlaybackTick()
{
    if (currentFrame_ >= totalFrames_ - 1) {
        currentFrame_ = 0;
        scrollOffset_ = 0;
    } else {
        currentFrame_++;
    }
    appState_->setCurrentFrame(currentFrame_);
    updateLabels();
    update();
    emit frameChanged(currentFrame_);
}

void TimelinePanelV2::onScrollChanged(int value)
{
    scrollOffset_ = value;
    update();
}

void TimelinePanelV2::addFrame()
{
    totalFrames_++;
    appState_->document().setTotalFrames(totalFrames_);
    updateLabels();
    update();
    emit frameCountChanged(totalFrames_);
}

void TimelinePanelV2::duplicateFrame()
{
    if (currentFrame_ < 0 || currentFrame_ >= totalFrames_) return;

    totalFrames_++;
    appState_->document().setTotalFrames(totalFrames_);

    auto& srcRoot = appState_->document().rootLayerForFrame(currentFrame_);
    auto& dstRoot = appState_->document().rootLayerForFrame(totalFrames_ - 1);

    dstRoot.layers().clear();
    for (const auto& layer : srcRoot.layers()) {
        dstRoot.addLayer(layer->clone());
    }

    appState_->document().setModified(true);
    currentFrame_ = totalFrames_ - 1;
    appState_->setCurrentFrame(currentFrame_);

    updateLabels();
    update();
    emit frameCountChanged(totalFrames_);
    emit frameChanged(currentFrame_);
}

void TimelinePanelV2::deleteFrame()
{
    if (totalFrames_ <= 1) return;
    if (currentFrame_ < 0 || currentFrame_ >= totalFrames_) return;

    int delFrame = currentFrame_;

    appState_->document().shiftFrameData(delFrame + 1, -1);
    appState_->document().removeFrameData(totalFrames_ - 1);
    totalFrames_--;
    appState_->document().setTotalFrames(totalFrames_);

    if (currentFrame_ >= totalFrames_) {
        currentFrame_ = totalFrames_ - 1;
    }
    appState_->setCurrentFrame(currentFrame_);

    updateLabels();
    update();
    emit frameCountChanged(totalFrames_);
    emit frameChanged(currentFrame_);
}

void TimelinePanelV2::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    p.fillRect(rect(), kBgColor);

    int topBarH = 28;
    int w = width();
    int h = height();

    txPort_ = kTransportWidth + kFpsWidth + kLabelWidth + 12;

    int stripWidth = w - txPort_ - 12;
    trackTop_ = topBarH + 2;
    trackHeight_ = h - trackTop_ - 4;
    int thumbH = cellHeight_ - thumbPad_ * 2 - 14;

    QRect trackRect(txPort_, trackTop_, stripWidth, trackHeight_);
    p.fillRect(trackRect, QColor("#0D0F14"));
    p.setPen(QPen(kCellBorder, 1));
    p.drawRect(trackRect);

    int cellTotal = cellWidth_ + cellSpacing_;

    int firstVisible = std::max(0, scrollOffset_ / cellTotal);
    int lastVisible = std::min(totalFrames_ - 1,
        (scrollOffset_ + stripWidth) / cellTotal + 1);

    for (int f = firstVisible; f <= lastVisible; ++f) {
        int cellX = txPort_ + f * cellTotal - scrollOffset_;
        int cellY = trackTop_ + 4;

        QRect cellRect(cellX, cellY, cellWidth_, cellHeight_);

        bool isCurrent = (f == currentFrame_);
        bool isHovered = (f == hoveredFrame_);

        QColor bgColor = kCellBg;
        if (isCurrent) bgColor = kCellActiveBg;
        if (isHovered && !isCurrent) bgColor = kCellHoverBg;

        p.setPen(Qt::NoPen);
        p.setBrush(bgColor);
        p.drawRoundedRect(cellRect, 4, 4);

        QColor borderColor = isCurrent ? kCellActiveBorder : kCellBorder;
        if (isHovered && !isCurrent) borderColor = QColor("#4A4E60");
        p.setPen(QPen(borderColor, isCurrent ? 2.0 : 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(cellRect.adjusted(0, 0, -1, -1), 4, 4);

        QRect thumbRect(cellX + thumbPad_, cellY + thumbPad_,
                        cellWidth_ - thumbPad_ * 2, thumbH);

        p.fillRect(thumbRect, Qt::white);

        if (appState_) {
            auto* entry = appState_->thumbnailCache().getThumbnail(f);
            if (entry && !entry->pixels.empty() && entry->width > 0 && entry->height > 0) {
                QImage img(reinterpret_cast<const uchar*>(entry->pixels.data()),
                           entry->width, entry->height,
                           entry->width * static_cast<int>(sizeof(uint32_t)),
                           QImage::Format_ARGB32_Premultiplied);
                p.drawImage(thumbRect, img);
            }
        }

        p.setPen(kFrameNumColor);
        QFont numFont("JetBrains Mono", 8);
        p.setFont(numFont);
        QRect numRect(cellX, cellY + cellHeight_ - 14, cellWidth_, 12);
        p.drawText(numRect, Qt::AlignCenter, QString::number(f + 1));

        if (isCurrent) {
            int px = cellX + cellWidth_ / 2;
            int py = cellY + cellHeight_ + 1;
            QPolygon triangle;
            triangle << QPoint(px, py)
                     << QPoint(px + 4, py + 5)
                     << QPoint(px - 4, py + 5);
            p.setPen(Qt::NoPen);
            p.setBrush(kPlayheadColor);
            p.drawPolygon(triangle);
        }
    }

    int addX = txPort_ + totalFrames_ * cellTotal - scrollOffset_;
    QRect addRect(addX, trackTop_ + 4, cellWidth_, cellHeight_);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor("#141A18"));
    p.drawRoundedRect(addRect, 4, 4);
    p.setPen(QPen(QColor("#2A4030"), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(addRect.adjusted(0, 0, -1, -1), 4, 4);

    p.setPen(QPen(QColor("#3A5A40"), 2));
    int cx = addX + cellWidth_ / 2;
    int cy = addRect.center().y();
    p.drawLine(cx - 8, cy, cx + 8, cy);
    p.drawLine(cx, cy - 8, cx, cy + 8);

    addFrameBtn_ = nullptr;

    int contentWidth = (totalFrames_ + 1) * cellTotal + 8;
    if (contentWidth > stripWidth) {
        int maxScroll = std::max(0, contentWidth - stripWidth);
        scrollOffset_ = std::clamp(scrollOffset_, 0, maxScroll);

        int scrollY = h - 10;
        int scrollH = 6;
        QRect scrollBgRect(txPort_, scrollY, stripWidth, scrollH);
        p.fillRect(scrollBgRect, kScrollBg);

        if (maxScroll > 0) {
            float visibleRatio = static_cast<float>(stripWidth) / contentWidth;
            int handleWidth = std::max(20, static_cast<int>(stripWidth * visibleRatio));
            int handleX = txPort_ + static_cast<int>(
                static_cast<float>(scrollOffset_) / maxScroll * (stripWidth - handleWidth));
            p.fillRect(QRect(handleX, scrollY, handleWidth, scrollH), kScrollHandle);
        }
    }

    if (!playing_ && currentFrame_ >= 0 && currentFrame_ < totalFrames_) {
        int px = txPort_ + currentFrame_ * cellTotal - scrollOffset_ + cellWidth_ / 2;
        p.setPen(QPen(kPlayheadColor, 1, Qt::DotLine));
        p.drawLine(px, trackTop_, px, trackTop_ + trackHeight_);
    }
}

void TimelinePanelV2::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        int addX = txPort_ + totalFrames_ * (cellWidth_ + cellSpacing_) - scrollOffset_;
        QRect addRect(addX, trackTop_ + 4, cellWidth_, cellHeight_);

        if (addRect.contains(event->pos())) {
            addFrame();
            return;
        }

        int frame = frameAtPos(event->pos().x());
        if (frame >= 0 && frame < totalFrames_ &&
            event->pos().y() >= trackTop_ && event->pos().y() <= trackTop_ + trackHeight_) {
            scrubbing_ = true;
            currentFrame_ = frame;
            appState_->setCurrentFrame(currentFrame_);
            updateLabels();
            update();
            emit frameChanged(currentFrame_);
        }
    }
}

void TimelinePanelV2::mouseMoveEvent(QMouseEvent* event)
{
    if (scrubbing_ && (event->buttons() & Qt::LeftButton)) {
        int frame = frameAtPos(event->pos().x());
        if (frame >= 0 && frame < totalFrames_ && frame != currentFrame_) {
            currentFrame_ = frame;
            appState_->setCurrentFrame(currentFrame_);
            updateLabels();
            update();
            emit frameChanged(currentFrame_);
        }
    }

    int frame = frameAtPos(event->pos().x());
    if (frame != hoveredFrame_) {
        hoveredFrame_ = frame;
        update();
    }
}

void TimelinePanelV2::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && scrubbing_) {
        scrubbing_ = false;
        update();
    }
}

void TimelinePanelV2::wheelEvent(QWheelEvent* event)
{
    int delta = event->angleDelta().y();
    int contentWidth = (totalFrames_ + 1) * (cellWidth_ + cellSpacing_) + 8;
    int stripWidth = width() - txPort_ - 12;
    int maxScroll = std::max(0, contentWidth - stripWidth);

    scrollOffset_ = std::clamp(scrollOffset_ - delta / 2, 0, maxScroll);
    update();
}

void TimelinePanelV2::contextMenuEvent(QContextMenuEvent* event)
{
    int frame = frameAtPos(event->pos().x());
    if (frame < 0 || frame >= totalFrames_) return;

    QMenu menu(this);
    menu.setStyleSheet(QString(
        "QMenu { background:%1; color:%2; border:1px solid %3; border-radius:4px; padding:4px; }"
        "QMenu::item { padding:4px 20px; border-radius:3px; }"
        "QMenu::item:selected { background:%4; }")
        .arg(kCellBg.name(), kBtnText.name(), kCellBorder.name(), kBtnHover.name()));

    QAction* dupAction = menu.addAction("Duplicate Frame");
    QAction* delAction = menu.addAction("Delete Frame");
    menu.addSeparator();
    QAction* addAction = menu.addAction("Insert Frame After");

    QAction* chosen = menu.exec(event->globalPos());
    if (chosen == dupAction) {
        currentFrame_ = frame;
        duplicateFrame();
    } else if (chosen == delAction) {
        currentFrame_ = frame;
        deleteFrame();
    } else if (chosen == addAction) {
        currentFrame_ = frame;
        addFrame();
    }
}

void TimelinePanelV2::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    update();
}

} // namespace fap
