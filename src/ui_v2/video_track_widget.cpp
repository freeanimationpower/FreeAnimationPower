#include "video_track_widget.hpp"
#include "timeline_panel_v2.hpp"
#include "core/app_state.hpp"
#include "core/document.hpp"
#include "engine/animation/video_decoder.hpp"

#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QFileInfo>
#include <QDebug>
#include <QPainter>
#include <QTimer>
#include <QIcon>
#include <algorithm>

namespace fap {

static const QColor kVideoTrackBg("#12161A");
static const QColor kVideoHeaderBg("#0D1014");
static const QColor kBorderColor("#1E2128");
static const QColor kBtnHover("#252838");
static const QColor kPlayheadColor("#FF4800");
static const QColor kBtnText("#C8CCD8");
static const QColor kTrackNameText("#8B8FA3");
static const QColor kRedColor("#FF4A4A");
static const QColor kMutedColor("#4A4E60");

VideoTrackWidget::VideoTrackWidget(const QString& filepath, int trackIndex,
                                     std::shared_ptr<AppState> state,
                                     TimelinePanelV2* panel, QWidget* parent)
    : QWidget(parent)
    , filepath_(filepath)
    , trackIndex_(trackIndex)
    , appState_(std::move(state))
    , panel_(panel)
{
    QFileInfo fi(filepath);
    displayName_ = fi.fileName();

    setFixedHeight(64);
    setMouseTracking(true);

    nameEdit_ = new QLineEdit(displayName_, this);
    nameEdit_->setStyleSheet(QString(
        "QLineEdit { background:transparent; color:%1; border:none; "
        "font-size:11px; font-family:'Avenir'; padding:0px 8px; }"
        "QLineEdit:focus { background:#1A1E28; color:#E8ECF0; }")
        .arg(kTrackNameText.name()));
    nameEdit_->setReadOnly(true);
    nameEdit_->setCursorPosition(0);

    muteBtn_ = new QPushButton(QString::fromUtf8("\U0001F50A"), this);
    muteBtn_->setFixedSize(28, 28);
    muteBtn_->setToolTip("Mute / Unmute video audio");
    muteBtn_->setStyleSheet(QString(
        "QPushButton { background:%1; border:1px solid transparent; "
        "border-radius:3px; font-size:14px; }"
        "QPushButton:hover { background:%2; border-color:%3; }")
        .arg(kPlayheadColor.name(), kBtnHover.name(), kPlayheadColor.name()));
    connect(muteBtn_, &QPushButton::clicked, this, [this]() {
        setMuted(!muted_);
    });

    upBtn_ = new QPushButton(QIcon(":/icons/svg/move_up.svg"), "", this);
    upBtn_->setFixedSize(28, 28);
    upBtn_->setIconSize(QSize(20, 20));
    upBtn_->setToolTip("Move Video Track Up");
    upBtn_->setStyleSheet(QString(
        "QPushButton { background:%1; border:1px solid transparent; "
        "border-radius:3px; }"
        "QPushButton:hover { background:%2; border-color:%3; }")
        .arg(kPlayheadColor.name(), kBtnHover.name(), kPlayheadColor.name()));
    connect(upBtn_, &QPushButton::clicked, this, [this]() { emit moveUpRequested(); });

    downBtn_ = new QPushButton(QIcon(":/icons/svg/move_down.svg"), "", this);
    downBtn_->setFixedSize(28, 28);
    downBtn_->setIconSize(QSize(20, 20));
    downBtn_->setToolTip("Move Video Track Down");
    downBtn_->setStyleSheet(QString(
        "QPushButton { background:%1; border:1px solid transparent; "
        "border-radius:3px; }"
        "QPushButton:hover { background:%2; border-color:%3; }")
        .arg(kPlayheadColor.name(), kBtnHover.name(), kPlayheadColor.name()));
    connect(downBtn_, &QPushButton::clicked, this, [this]() { emit moveDownRequested(); });

    delBtn_ = new QPushButton(QString::fromUtf8("\u2715"), this);
    delBtn_->setFixedSize(28, 28);
    delBtn_->setToolTip("Remove Video Track");
    delBtn_->setStyleSheet(QString(
        "QPushButton { background:%1; color:%2; border:1px solid transparent; "
        "border-radius:3px; font-size:12px; }"
        "QPushButton:hover { background:%3; border-color:%4; }")
        .arg(kPlayheadColor.name(), kBtnText.name(), kRedColor.name(), kRedColor.name()));
    connect(delBtn_, &QPushButton::clicked, this, [this]() {
        if (panel_) panel_->removeVideoTrack(this);
    });

    volumeLabel_ = new QLabel("Vol:", this);
    volumeLabel_->setStyleSheet(QString(
        "color:%1; font-size:9px; background:transparent;").arg(kTrackNameText.name()));

    volumeSlider_ = new QSlider(Qt::Horizontal, this);
    volumeSlider_->setRange(0, 100);
    volumeSlider_->setValue(80);
    volumeSlider_->setStyleSheet(QString(
        "QSlider::groove:horizontal { background:#1E2128; height:2px; border-radius:1px; }"
        "QSlider::handle:horizontal { background:%1; width:8px; height:8px; "
        "margin:-3px 0; border-radius:4px; }"
        "QSlider::sub-page:horizontal { background:%1; border-radius:1px; }")
        .arg(kPlayheadColor.name()));
    connect(volumeSlider_, &QSlider::valueChanged, this, [this](int v) {
        volume_ = v;
    });

    opacityLabel_ = new QLabel("Op:100%", this);
    opacityLabel_->setStyleSheet(QString(
        "color:%1; font-size:9px; background:transparent;").arg(kTrackNameText.name()));

    opacitySlider_ = new QSlider(Qt::Horizontal, this);
    opacitySlider_->setRange(0, 100);
    opacitySlider_->setValue(100);
    opacitySlider_->setStyleSheet(QString(
        "QSlider::groove:horizontal { background:#1E2128; height:2px; border-radius:1px; }"
        "QSlider::handle:horizontal { background:%1; width:8px; height:8px; "
        "margin:-3px 0; border-radius:4px; }"
        "QSlider::sub-page:horizontal { background:%1; border-radius:1px; }")
        .arg(kPlayheadColor.name()));
    connect(opacitySlider_, &QSlider::valueChanged, this, [this](int v) {
        opacity_ = v / 100.0f;
        opacityLabel_->setText(QString("Op:%1%").arg(v));
        emit opacityChanged();
        update();
    });

    positionHeader();

    auto meta = probeVideoMetadata(filepath);
    if (meta.valid) {
        videoWidth_  = meta.width;
        videoHeight_ = meta.height;
        videoFps_    = meta.fps;
        totalFrames_ = meta.totalFrames;
        if (meta.durationSecs > 60.0) {
            totalFrames_ = static_cast<int>(60.0 * meta.fps);
        }
        QTimer::singleShot(10, this, [this]() { decodeThumbnails(); });
    }
}

VideoTrackWidget::~VideoTrackWidget() = default;

void VideoTrackWidget::positionHeader()
{
    const int hdrW = TimelinePanelV2::kHeaderWidth;
    nameEdit_->setGeometry(8, 4, hdrW - 132, 22);
    upBtn_->move(hdrW - 126, 3);
    downBtn_->move(hdrW - 96, 3);
    muteBtn_->move(hdrW - 66, 3);
    delBtn_->move(hdrW - 34, 3);

    int sw = hdrW / 2 - 12;
    volumeLabel_->setGeometry(8, 38, 22, 16);
    volumeSlider_->setGeometry(30, 38, sw - 30, 16);
    opacityLabel_->setGeometry(sw + 8, 38, 22, 16);
    opacitySlider_->setGeometry(sw + 28, 38, hdrW - sw - 36, 16);
}

void VideoTrackWidget::setMuted(bool m)
{
    muted_ = m;
    muteBtn_->setStyleSheet(muted_
        ? QString("QPushButton { background:%1; color:%2; border:1px solid %3; "
                  "border-radius:3px; font-size:14px; }"
                  "QPushButton:hover { background:#3A2030; border-color:%2; }")
              .arg(kMutedColor.name(), kRedColor.name(), kRedColor.name())
        : QString("QPushButton { background:%1; border:1px solid transparent; "
                  "border-radius:3px; font-size:14px; }"
                  "QPushButton:hover { background:%2; border-color:%3; }")
              .arg(kPlayheadColor.name(), kBtnHover.name(), kPlayheadColor.name()));
    update();
}

void VideoTrackWidget::setVolume(int v)
{
    volume_ = std::clamp(v, 0, 100);
    if (volumeSlider_) {
        volumeSlider_->blockSignals(true);
        volumeSlider_->setValue(volume_);
        volumeSlider_->blockSignals(false);
    }
}

void VideoTrackWidget::setOpacity(float o)
{
    opacity_ = std::clamp(o, 0.0f, 1.0f);
    int pct = static_cast<int>(opacity_ * 100);
    if (opacitySlider_) {
        opacitySlider_->blockSignals(true);
        opacitySlider_->setValue(pct);
        opacitySlider_->blockSignals(false);
    }
    opacityLabel_->setText(QString("Op:%1%").arg(pct));
}

void VideoTrackWidget::decodeThumbnails()
{
    if (totalFrames_ <= 0) return;
    int step = std::max(1, totalFrames_ / 30);
    for (int f = 0; f < totalFrames_ && thumbnails_.size() < 30; f += step) {
        if (frameCache_.has(f)) {
            thumbnails_.push_back({f, frameCache_.get(f)});
        } else {
            QImage img = decodeVideoFrame(filepath_, f, videoFps_, 320, 180);
            if (!img.isNull()) {
                frameCache_.put(f, img);
                thumbnails_.push_back({f, img});
            }
        }
    }
    update();
}

QImage VideoTrackWidget::currentFrame(int frameIdx)
{
    if (totalFrames_ <= 0) return {};
    int clamped = frameIdx % totalFrames_;
    if (clamped < 0) clamped += totalFrames_;

    auto cached = frameCache_.get(clamped);
    if (!cached.isNull()) return cached;

    QImage img = decodeVideoFrame(filepath_, clamped, videoFps_);
    if (!img.isNull()) frameCache_.put(clamped, img);
    return img;
}

void VideoTrackWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    const int w = width();
    const int h = height();
    const int hdrW = TimelinePanelV2::kHeaderWidth;

    p.fillRect(0, 0, hdrW, h, kVideoHeaderBg);
    p.fillRect(hdrW, 0, w - hdrW, h, kVideoTrackBg);

    p.fillRect(0, 0, 2, h, kPlayheadColor);

    p.setPen(QPen(kBorderColor, 1));
    p.drawLine(hdrW, 0, hdrW, h);
    p.drawLine(0, h - 1, w, h - 1);

    drawThumbnailStrip(p);

    if (thumbnails_.empty() && totalFrames_ <= 0) {
        p.setPen(QColor("#5A5E68"));
        QFont f("Avenir", 9);
        p.setFont(f);
        p.drawText(QRect(hdrW, 0, w - hdrW, h), Qt::AlignCenter,
                   "Decoding video...");
    }
}

void VideoTrackWidget::drawThumbnailStrip(QPainter& p)
{
    if (thumbnails_.empty()) return;

    const int hdrW = TimelinePanelV2::kHeaderWidth;
    const int w = width();
    const int h = height();
    const int offset = panel_->sharedScrollOffset();

    int availableW = w - hdrW - 4;
    if (availableW <= 0) return;

    int thumbW = std::max(24, availableW / static_cast<int>(thumbnails_.size()));
    int thumbH = h - 4;
    int y = 2;

    for (size_t i = 0; i < thumbnails_.size(); ++i) {
        int x = hdrW + 2 + static_cast<int>(i) * thumbW - offset % thumbW;
        if (x + thumbW < hdrW || x > w) continue;

        QRect r(x, y, thumbW, thumbH);
        if (!thumbnails_[i].image.isNull()) {
            p.drawImage(r, thumbnails_[i].image.scaled(thumbW, thumbH,
                              Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
        p.setPen(QPen(QColor("#2D3139"), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRect(r);
    }
}

} // namespace fap
