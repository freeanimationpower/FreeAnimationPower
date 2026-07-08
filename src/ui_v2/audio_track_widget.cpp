#include "audio_track_widget.hpp"
#include "timeline_panel_v2.hpp"

#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QFileInfo>
#include <QDebug>
#include <QPainter>
#include <QResizeEvent>
#include <QIcon>
#include <algorithm>
#include <cmath>

#include "engine/animation/audio_file_decoder.hpp"

namespace fap {

static const QColor kWaveformColor("#00D4AA");
static const QColor kAudioTrackBg("#121A16");
static const QColor kAudioHeaderBg("#0D1410");
static const QColor kBorderColor("#1E2128");
static const QColor kBtnHover("#252838");
static const QColor kPlayheadColor("#FF6B4A");
static const QColor kBtnText("#C8CCD8");
static const QColor kTrackNameText("#8B8FA3");
static const QColor kRedColor("#FF4A4A");
static const QColor kMutedColor("#4A4E60");

AudioTrackWidget::AudioTrackWidget(const QString& filepath, int index,
                                    std::shared_ptr<AppState> appState,
                                    TimelinePanelV2* panel,
                                    QWidget* parent)
    : QWidget(parent)
    , filepath_(filepath)
    , index_(index)
    , appState_(std::move(appState))
    , panel_(panel)
{
    setFixedHeight(TimelinePanelV2::kTrackHeight);
    setMouseTracking(true);

    QFileInfo fi(filepath_);
    displayName_ = fi.fileName();

    nameEdit_ = new QLineEdit(displayName_, this);
    nameEdit_->setStyleSheet(QString(
        "QLineEdit { background:transparent; color:%1; border:none; "
        "font-size:11px; font-family:'Inter'; padding:0px 8px; }"
        "QLineEdit:focus { background:#1A1E28; color:#E8ECF0; }")
        .arg(kTrackNameText.name()));
    nameEdit_->setReadOnly(true);
    nameEdit_->setCursorPosition(0);

    muteBtn_ = new QPushButton(QString::fromUtf8("\U0001F50A"), this);
    muteBtn_->setFixedSize(28, 28);
    muteBtn_->setToolTip("Mute / Unmute");
    muteBtn_->setStyleSheet(QString(
        "QPushButton { background:transparent; border:1px solid transparent; "
        "border-radius:3px; font-size:14px; }"
        "QPushButton:hover { background:%1; border-color:%2; }")
        .arg(kBtnHover.name(), kPlayheadColor.name()));
    connect(muteBtn_, &QPushButton::clicked, this, [this]() {
        muted_ = !muted_;
        if (audioOutput_) audioOutput_->setMuted(muted_);
        updateMuteStyle();
    });

    upBtn_ = new QPushButton(QIcon(":/icons/svg/move_up.svg"), "", this);
    upBtn_->setFixedSize(28, 28);
    upBtn_->setIconSize(QSize(20, 20));
    upBtn_->setToolTip("Move Audio Track Up");
    upBtn_->setStyleSheet(QString(
        "QPushButton { background:transparent; border:1px solid transparent; "
        "border-radius:3px; }"
        "QPushButton:hover { background:%1; border-color:%2; }")
        .arg(kBtnHover.name(), kPlayheadColor.name()));
    connect(upBtn_, &QPushButton::clicked, this, [this]() { emit moveUpRequested(); });

    downBtn_ = new QPushButton(QIcon(":/icons/svg/move_down.svg"), "", this);
    downBtn_->setFixedSize(28, 28);
    downBtn_->setIconSize(QSize(20, 20));
    downBtn_->setToolTip("Move Audio Track Down");
    downBtn_->setStyleSheet(QString(
        "QPushButton { background:transparent; border:1px solid transparent; "
        "border-radius:3px; }"
        "QPushButton:hover { background:%1; border-color:%2; }")
        .arg(kBtnHover.name(), kPlayheadColor.name()));
    connect(downBtn_, &QPushButton::clicked, this, [this]() { emit moveDownRequested(); });

    delBtn_ = new QPushButton(QString::fromUtf8("\u2715"), this);
    delBtn_->setFixedSize(28, 28);
    delBtn_->setToolTip("Remove Audio Track");
    delBtn_->setStyleSheet(QString(
        "QPushButton { background:transparent; color:%1; border:1px solid transparent; "
        "border-radius:3px; font-size:12px; }"
        "QPushButton:hover { background:%2; border-color:%3; }")
        .arg(kBtnText.name(), kBtnHover.name(), kRedColor.name()));
    connect(delBtn_, &QPushButton::clicked, this, [this]() {
        if (panel_ && player_) {
            player_->stop();
            panel_->removeAudioTrack(this);
        }
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
    connect(volumeSlider_, &QSlider::valueChanged, this, [this](int val) {
        if (audioOutput_) audioOutput_->setVolume(val / 100.0f);
    });

    player_ = new QMediaPlayer(this);
    audioOutput_ = new QAudioOutput(this);
    player_->setAudioOutput(audioOutput_);
    audioOutput_->setVolume(0.8f);
    player_->setSource(QUrl::fromLocalFile(filepath_));

    positionHeader();
    decodeAudio();
}

AudioTrackWidget::~AudioTrackWidget()
{
    if (player_) {
        player_->stop();
    }
}

void AudioTrackWidget::positionHeader()
{
    const int hdrW = TimelinePanelV2::kHeaderWidth;
    nameEdit_->setGeometry(8, 4, hdrW - 132, 22);
    upBtn_->move(hdrW - 126, 3);
    downBtn_->move(hdrW - 96, 3);
    muteBtn_->move(hdrW - 66, 3);
    delBtn_->move(hdrW - 34, 3);
    volumeLabel_->setGeometry(8, 38, 28, 16);
    volumeSlider_->setGeometry(40, 38, hdrW - 48, 16);
}

void AudioTrackWidget::setMuted(bool m)
{
    muted_ = m;
    if (audioOutput_) audioOutput_->setMuted(muted_);
    updateMuteStyle();
    update();
}

void AudioTrackWidget::setVolume(int v)
{
    int clamped = std::clamp(v, 0, 100);
    if (volumeSlider_) {
        volumeSlider_->blockSignals(true);
        volumeSlider_->setValue(clamped);
        volumeSlider_->blockSignals(false);
    }
    if (audioOutput_) audioOutput_->setVolume(clamped / 100.0f);
}

void AudioTrackWidget::syncToFrame(int frame, int fps, bool playing)
{
    if (!player_) return;
    qint64 ms = (fps > 0) ? (static_cast<qint64>(frame) * 1000LL / fps) : 0;

    if (playing) {
        if (player_->playbackState() != QMediaPlayer::PlayingState) {
            player_->setPosition(ms);
            player_->play();
        }
    } else {
        player_->setPosition(ms);
    }
}

void AudioTrackWidget::decodeAudio()
{
    QFileInfo fi(filepath_);
    qDebug() << "decodeAudio — starting:" << filepath_
             << "exists:" << fi.exists() << "size:" << fi.size()
             << "suffix:" << fi.suffix().toLower();

    if (!fi.exists()) {
        qWarning() << "decodeAudio — file not found:" << filepath_;
        decoded_ = true;
        decodeError_ = true;
        lastError_ = "File not found";
        update();
        return;
    }

    auto result = decodeAudioFile(filepath_.toStdString());

    if (!result.success) {
        qWarning() << "decodeAudio — failed to decode:" << filepath_;
        decoded_ = true;
        decodeError_ = true;
        lastError_ = "Unsupported format or corrupted file";
        update();
        return;
    }

    waveformPicks_ = std::move(result.peaks);
    durationMs_ = result.durationMs;
    decoded_ = true;

    qDebug() << "decodeAudio — success: peaks:" << waveformPicks_.size()
             << "channels:" << result.channels
             << "sampleRate:" << result.sampleRate
             << "durationMs:" << result.durationMs;
    update();
}

void AudioTrackWidget::drawWaveform(QPainter& p)
{
    if (waveformPicks_.empty()) return;

    const int hdrW = TimelinePanelV2::kHeaderWidth;
    const int w = width();
    if (w <= hdrW) return;

    const int midY = height() / 2;
    const int offset = panel_->sharedScrollOffset();
    const int maxAmplitude = 24;

    const float spp = static_cast<float>(waveformPicks_.size())
                    / static_cast<float>(w - hdrW);

    p.setPen(QPen(kWaveformColor, 1));

    for (int x = hdrW; x < w; ++x) {
        int idx = static_cast<int>(static_cast<float>(x - hdrW + offset) * spp);
        if (idx >= 0 && idx < static_cast<int>(waveformPicks_.size())) {
            int h = static_cast<int>(waveformPicks_[idx] * maxAmplitude);
            p.drawLine(x, midY - h, x, midY + h);
        }
    }
}

void AudioTrackWidget::updateMuteStyle()
{
    QColor color = muted_ ? kMutedColor : kTrackNameText;
    nameEdit_->setStyleSheet(QString(
        "QLineEdit { background:transparent; color:%1; border:none; "
        "font-size:11px; font-family:'Inter'; padding:0px 8px; }"
        "QLineEdit:focus { background:#1A1E28; color:#E8ECF0; }")
        .arg(color.name()));
    muteBtn_->setText(muted_ ? QString::fromUtf8("\U0001F507")
                             : QString::fromUtf8("\U0001F50A"));
}

void AudioTrackWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const int hdrW = TimelinePanelV2::kHeaderWidth;
    const int w = width();
    const int h = height();

    p.fillRect(0, 0, hdrW, h, kAudioHeaderBg);
    p.setPen(QPen(kBorderColor, 1));
    p.drawLine(hdrW, 0, hdrW, h);
    p.fillRect(hdrW, 0, w - hdrW, h, kAudioTrackBg);
    p.drawLine(0, h - 1, w, h - 1);

    if (!decoded_) {
        p.setPen(kTrackNameText);
        QFont f("Inter", 9);
        p.setFont(f);
        p.drawText(QRect(hdrW + 8, 0, w - hdrW - 16, h),
                   Qt::AlignVCenter | Qt::AlignLeft,
                   "Decoding audio...");
    } else if (waveformPicks_.empty()) {
        p.setPen(kTrackNameText);
        QFont f("Inter", 8);
        p.setFont(f);
        QString msg;
        if (decodeError_) {
            msg = lastError_.empty()
                ? QString("Decoder error")
                : QString::fromStdString("Error: " + lastError_);
        } else {
            msg = QString("No waveform data");
        }
        p.drawText(QRect(hdrW + 8, 0, w - hdrW - 16, h),
                   Qt::AlignVCenter | Qt::AlignLeft, msg);
    } else {
        drawWaveform(p);
    }

    int curFrame = panel_->currentFrame();
    int cellTotal = TimelinePanelV2::kCellTotal;
    int cellW = TimelinePanelV2::kCellWidth;
    int px = hdrW + curFrame * cellTotal - panel_->sharedScrollOffset() + cellW / 2;
    if (px >= hdrW && px < w) {
        p.setPen(QPen(kPlayheadColor, 1));
        p.drawLine(px, 0, px, h);
    }
}

void AudioTrackWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    positionHeader();
}

} // namespace fap
