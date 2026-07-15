#include "video_track_widget.hpp"
#include "timeline_panel_v2.hpp"
#include "core/app_state.hpp"
#include "core/document.hpp"
#include "engine/animation/video_decoder.hpp"

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtGui/QPainter>
#include <QtGui/QMouseEvent>
#include <QtCore/QFileInfo>
#include <QtCore/QTimer>

namespace fap {

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

    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto* headerArea = new QWidget(this);
    headerArea->setFixedWidth(TimelinePanelV2::kHeaderWidth);
    auto* headerLayout = new QVBoxLayout(headerArea);
    headerLayout->setContentsMargins(4, 2, 4, 2);
    headerLayout->setSpacing(2);

    auto* nameRow = new QHBoxLayout();
    nameEdit_ = new QLineEdit(displayName_, headerArea);
    nameEdit_->setReadOnly(true);
    nameEdit_->setStyleSheet(QString(
        "QLineEdit { background:transparent; color:#C8CCD8; border:none; font-size:10px; }"));
    nameRow->addWidget(nameEdit_);

    muteBtn_ = new QPushButton("M", headerArea);
    muteBtn_->setFixedSize(18, 18);
    muteBtn_->setToolTip("Mute video audio");
    muteBtn_->setStyleSheet(QString(
        "QPushButton { background:#1E2130; color:#C8CCD8; border:none; border-radius:2px; font-size:8px; }"
        "QPushButton:hover { background:#252838; }"));
    connect(muteBtn_, &QPushButton::clicked, this, [this]() {
        setMuted(!muted_);
    });
    nameRow->addWidget(muteBtn_);

    upBtn_ = new QPushButton("▲", headerArea);
    upBtn_->setFixedSize(18, 18);
    upBtn_->setToolTip("Move up");
    upBtn_->setStyleSheet(muteBtn_->styleSheet());
    connect(upBtn_, &QPushButton::clicked, this, &VideoTrackWidget::moveUpRequested);
    nameRow->addWidget(upBtn_);

    downBtn_ = new QPushButton("▼", headerArea);
    downBtn_->setFixedSize(18, 18);
    downBtn_->setToolTip("Move down");
    downBtn_->setStyleSheet(muteBtn_->styleSheet());
    connect(downBtn_, &QPushButton::clicked, this, &VideoTrackWidget::moveDownRequested);
    nameRow->addWidget(downBtn_);

    delBtn_ = new QPushButton("✕", headerArea);
    delBtn_->setFixedSize(18, 18);
    delBtn_->setToolTip("Remove video track");
    delBtn_->setStyleSheet(QString(
        "QPushButton { background:#1E2130; color:#E85D5D; border:none; border-radius:2px; font-size:9px; }"
        "QPushButton:hover { background:#E85D5D; color:#fff; }"));
    connect(delBtn_, &QPushButton::clicked, this, [this]() {
        panel_->removeVideoTrack(this);
    });
    nameRow->addWidget(delBtn_);
    nameRow->addStretch();

    headerLayout->addLayout(nameRow);

    auto* ctrlRow = new QHBoxLayout();
    opacityLabel_ = new QLabel("Op:100%", headerArea);
    opacityLabel_->setStyleSheet("color:#8B8FA3; font-size:8px;");
    ctrlRow->addWidget(opacityLabel_);

    opacitySlider_ = new QSlider(Qt::Horizontal, headerArea);
    opacitySlider_->setRange(0, 100);
    opacitySlider_->setValue(100);
    opacitySlider_->setFixedWidth(60);
    opacitySlider_->setStyleSheet(QString(
        "QSlider::groove:horizontal { background:#2D3139; height:4px; border-radius:2px; }"
        "QSlider::handle:horizontal { background:#FF4800; width:8px; margin:-2px 0; border-radius:4px; }"));
    connect(opacitySlider_, &QSlider::valueChanged, this, [this](int v) {
        opacity_ = v / 100.0f;
        opacityLabel_->setText(QString("Op:%1%").arg(v));
        update();
    });
    ctrlRow->addWidget(opacitySlider_);

    volumeLabel_ = new QLabel("Vol:80%", headerArea);
    volumeLabel_->setStyleSheet("color:#8B8FA3; font-size:8px;");
    ctrlRow->addWidget(volumeLabel_);

    volumeSlider_ = new QSlider(Qt::Horizontal, headerArea);
    volumeSlider_->setRange(0, 100);
    volumeSlider_->setValue(80);
    volumeSlider_->setFixedWidth(60);
    volumeSlider_->setStyleSheet(opacitySlider_->styleSheet());
    connect(volumeSlider_, &QSlider::valueChanged, this, [this](int v) {
        volume_ = v;
        volumeLabel_->setText(QString("Vol:%1%").arg(v));
    });
    ctrlRow->addWidget(volumeSlider_);

    ctrlRow->addStretch();
    headerLayout->addLayout(ctrlRow);

    mainLayout->addWidget(headerArea);

    auto* trackArea = new QWidget(this);
    trackArea->setStyleSheet("background:#13161D;");
    mainLayout->addWidget(trackArea, 1);

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

void VideoTrackWidget::setMuted(bool m) {
    muted_ = m;
    muteBtn_->setStyleSheet(muted_
        ? "QPushButton { background:#FF4800; color:#fff; border:none; border-radius:2px; font-size:8px; }"
        : "QPushButton { background:#1E2130; color:#C8CCD8; border:none; border-radius:2px; font-size:8px; }"
        "QPushButton:hover { background:#252838; }");
}

void VideoTrackWidget::setVolume(int v) {
    volume_ = std::clamp(v, 0, 100);
    volumeSlider_->setValue(volume_);
    volumeLabel_->setText(QString("Vol:%1%").arg(volume_));
}

void VideoTrackWidget::setOpacity(float o) {
    opacity_ = std::clamp(o, 0.0f, 1.0f);
    opacitySlider_->setValue(static_cast<int>(opacity_ * 100));
    opacityLabel_->setText(QString("Op:%1%").arg(static_cast<int>(opacity_ * 100)));
}

void VideoTrackWidget::positionHeader() {
}

void VideoTrackWidget::decodeThumbnails() {
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

QImage VideoTrackWidget::currentFrame(int frameIdx) {
    if (totalFrames_ <= 0) return {};

    int clamped = frameIdx % totalFrames_;
    if (clamped < 0) clamped += totalFrames_;

    auto cached = frameCache_.get(clamped);
    if (!cached.isNull()) return cached;

    QImage img = decodeVideoFrame(filepath_, clamped, videoFps_);
    if (!img.isNull()) {
        frameCache_.put(clamped, img);
    }
    return img;
}

void VideoTrackWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    const int w = width();
    const int h = height();
    const int hdrW = TimelinePanelV2::kHeaderWidth;

    p.fillRect(hdrW, 0, w - hdrW, h, QColor("#13161D"));
    p.setPen(QPen(QColor("#1E2128"), 1));
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

void VideoTrackWidget::drawThumbnailStrip(QPainter& p) {
    if (thumbnails_.empty()) return;

    const int hdrW = TimelinePanelV2::kHeaderWidth;
    const int w = width();
    const int h = height();
    const int offset = panel_->sharedScrollOffset();
    const int cellTotal = TimelinePanelV2::kCellTotal;

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
