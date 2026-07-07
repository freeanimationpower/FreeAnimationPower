#include "timeline_panel_v2.hpp"

#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QFrame>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QSlider>
#include <QtWidgets/QMenu>
#include <QtGui/QPainter>
#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>
#include <QtGui/QResizeEvent>
#include <QtGui/QEnterEvent>
#include <QtGui/QFont>
#include <QtGui/QIcon>
#include <QtCore/QTimer>
#include <algorithm>

#include "core/app_state.hpp"
#include "core/document.hpp"
#include "core/sequence.hpp"
#include "audio_track_widget.hpp"
#include "engine/animation/frame_thumbnail.hpp"

namespace fap {

// ========================================================================
// Colors
// ========================================================================

static const QColor kBgColor("#14161C");
static const QColor kTrackBg("#121418");
static const QColor kTrackActiveBg("#191D26");
static const QColor kTrackHoverBg("#161A22");
static const QColor kHeaderBg("#0D1014");
static const QColor kHeaderActiveBg("#11131B");
static const QColor kCellEmpty("#161920");
static const QColor kCellFilled("#2E333D");
static const QColor kCellBorder("#1E2128");
static const QColor kCellActiveBorder("#FF6B4A");
static const QColor kBorderColor("#1E2128");
static const QColor kAccentColor("#FF6B4A");
static const QColor kAccentDim("#3A2018");
static const QColor kPlayheadColor("#FF6B4A");
static const QColor kBtnBg("#1E2130");
static const QColor kBtnHover("#252838");
static const QColor kBtnPressed("#FF6B4A");
static const QColor kBtnText("#C8CCD8");
static const QColor kFrameNumColor("#4A4E60");
static const QColor kRulerBg("#0F1115");
static const QColor kRulerTick("#3A3E48");
static const QColor kRulerText("#5A5E68");
static const QColor kScrollBg("#14161C");
static const QColor kScrollHandle("#2A2D38");
static const QColor kTrackNameText("#8B8FA3");
static const QColor kTrackNameActiveText("#FF6B4A");
static const QColor kAddBtnColor("#3A5A40");

// ========================================================================
// RulerWidget
// ========================================================================

class RulerWidget : public QWidget {
public:
    explicit RulerWidget(TimelinePanelV2* panel, QWidget* parent = nullptr)
        : QWidget(parent), panel_(panel) {
        setFixedHeight(TimelinePanelV2::kRulerHeight);
        setMouseTracking(true);
    }

    TimelinePanelV2* panel() const { return panel_; }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), kRulerBg);

        const int offset = panel_->sharedScrollOffset();
        const int cellTotal = TimelinePanelV2::kCellTotal;
        const int cellW = TimelinePanelV2::kCellWidth;
        const int hdrW = TimelinePanelV2::kHeaderWidth;
        const int totalFrames = panel_->totalFrames();
        const int curFrame = panel_->currentFrame();
        const int w = width();
        const int h = height();

        int firstVisible = std::max(0, offset / cellTotal);
        int lastVisible = std::min(totalFrames - 1, (offset + w - hdrW) / cellTotal + 1);

        QFont tickFont("JetBrains Mono", 7);
        p.setFont(tickFont);

        for (int f = firstVisible; f <= lastVisible; ++f) {
            int x = hdrW + f * cellTotal - offset + cellW / 2;

            bool isMajor = (f % 5 == 0);
            int tickH = isMajor ? 12 : 6;
            QColor tickColor = isMajor ? kRulerText : kRulerTick;

            p.setPen(QPen(tickColor, 1));
            p.drawLine(x, h - tickH, x, h);

            if (isMajor) {
                p.setPen(kRulerText);
                p.drawText(QRect(x - 16, 0, 32, h - tickH - 1),
                           Qt::AlignHCenter | Qt::AlignBottom,
                           QString::number(f + 1));
            }
        }

        if (panel_->appState()) {
            auto& seq = panel_->appState()->activeSequence();
            int waStart = seq.workAreaStart();
            int waEnd = seq.effectiveWorkAreaEnd();
            if (waEnd > waStart) {
                int x1 = hdrW + waStart * cellTotal - offset;
                int x2 = hdrW + waEnd * cellTotal - offset;
                p.fillRect(QRect(x1, 0, x2 - x1, 4), QColor(255, 107, 74, 80));
                p.setPen(QPen(QColor(255, 107, 74, 180), 1));
                p.drawLine(x1, 0, x1, h);
                p.drawLine(x2, 0, x2, h);
            }
        }

        int px = hdrW + curFrame * cellTotal - offset + cellW / 2;
        QPolygon tri;
        tri << QPoint(px, 2)
            << QPoint(px - 4, h - 2)
            << QPoint(px + 4, h - 2);
        p.setPen(Qt::NoPen);
        p.setBrush(kPlayheadColor);
        p.drawPolygon(tri);
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() != Qt::LeftButton) return;

        int hdrW = TimelinePanelV2::kHeaderWidth;
        int cellTotal = TimelinePanelV2::kCellTotal;
        int offset = panel_->sharedScrollOffset();
        int mx = event->pos().x();

        if (panel_->appState()) {
            auto& seq = panel_->appState()->activeSequence();
            int waStart = seq.workAreaStart();
            int waEnd = seq.effectiveWorkAreaEnd();
            int x1 = hdrW + waStart * cellTotal - offset;
            int x2 = hdrW + waEnd * cellTotal - offset;

            if (waEnd > waStart && std::abs(mx - x1) <= 5) {
                currentDrag_ = DragTarget::WorkAreaIn;
                return;
            }
            if (waEnd > waStart && std::abs(mx - x2) <= 5) {
                currentDrag_ = DragTarget::WorkAreaOut;
                return;
            }
        }

        currentDrag_ = DragTarget::Playhead;
        if (panel_->isPlaying()) {
            panel_->togglePlayback();
        }
        int frame = hitFrame(mx);
        if (frame >= 0) {
            panel_->setCurrentFrame(frame);
            panel_->update();
            emit panel_->frameChanged(frame);
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (!panel_->appState()) return;

        int hdrW = TimelinePanelV2::kHeaderWidth;
        int cellTotal = TimelinePanelV2::kCellTotal;
        int offset = panel_->sharedScrollOffset();
        int mx = event->pos().x();

        auto& seq = panel_->appState()->activeSequence();

        if (currentDrag_ == DragTarget::WorkAreaIn) {
            int frame = std::max(0, (mx - hdrW + offset) / cellTotal);
            int waEnd = seq.effectiveWorkAreaEnd();
            frame = std::min(frame, waEnd - 1);
            panel_->appState()->setWorkAreaStart(frame);
            update();
            return;
        }

        if (currentDrag_ == DragTarget::WorkAreaOut) {
            int waStart = seq.workAreaStart();
            int totalFrames = seq.totalFrames();
            int frame = std::max(waStart + 1, (mx - hdrW + offset) / cellTotal);
            frame = std::min(frame, totalFrames);
            panel_->appState()->setWorkAreaEnd(frame);
            update();
            return;
        }

        if (currentDrag_ == DragTarget::Playhead) {
            int frame = hitFrame(mx);
            if (frame >= 0) {
                panel_->setCurrentFrame(frame);
                panel_->update();
                emit panel_->frameChanged(frame);
            }
            return;
        }

        int waStart = seq.workAreaStart();
        int waEnd = seq.effectiveWorkAreaEnd();
        int x1 = hdrW + waStart * cellTotal - offset;
        int x2 = hdrW + waEnd * cellTotal - offset;

        if (waEnd > waStart && (std::abs(mx - x1) <= 5 || std::abs(mx - x2) <= 5)) {
            setCursor(Qt::SizeHorCursor);
        } else {
            setCursor(Qt::ArrowCursor);
        }
    }

    void mouseReleaseEvent(QMouseEvent*) override {
        if (currentDrag_ == DragTarget::None) return;
        currentDrag_ = DragTarget::None;

        if (panel_->appState()) {
            int hdrW = TimelinePanelV2::kHeaderWidth;
            int cellTotal = TimelinePanelV2::kCellTotal;
            int offset = panel_->sharedScrollOffset();
            auto& seq = panel_->appState()->activeSequence();
            int waStart = seq.workAreaStart();
            int waEnd = seq.effectiveWorkAreaEnd();

            QPoint pos = mapFromGlobal(QCursor::pos());
            int mx = pos.x();
            int x1 = hdrW + waStart * cellTotal - offset;
            int x2 = hdrW + waEnd * cellTotal - offset;

            if (waEnd > waStart && (std::abs(mx - x1) <= 5 || std::abs(mx - x2) <= 5)) {
                setCursor(Qt::SizeHorCursor);
            } else {
                setCursor(Qt::ArrowCursor);
            }
        }
    }

private:
    int hitFrame(int x) const {
        int hdrW = TimelinePanelV2::kHeaderWidth;
        int rel = x - hdrW + panel_->sharedScrollOffset();
        if (rel < 0) return -1;
        int cellTotal = TimelinePanelV2::kCellTotal;
        int frame = rel / cellTotal;
        if (frame >= panel_->totalFrames()) return -1;
        return frame;
    }

    TimelinePanelV2* panel_;

    enum class DragTarget { None, Playhead, WorkAreaIn, WorkAreaOut };
    DragTarget currentDrag_ = DragTarget::None;
};

// ========================================================================
// SequenceTrackWidget
// ========================================================================

class SequenceTrackWidget : public QWidget {
public:
    SequenceTrackWidget(int seqIndex, const QString& name, bool isActive,
                         std::shared_ptr<AppState> appState,
                         TimelinePanelV2* panel,
                         QWidget* parent = nullptr)
        : QWidget(parent)
        , seqIndex_(seqIndex)
        , name_(name)
        , isActive_(isActive)
        , appState_(std::move(appState))
        , panel_(panel) {
        setFixedHeight(TimelinePanelV2::kTrackHeight);
        setMouseTracking(true);

        nameEdit_ = new QLineEdit(name_, this);
        nameEdit_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        nameEdit_->setStyleSheet(QString(
            "QLineEdit { background:transparent; color:%1; border:none; "
            "font-size:11px; font-family:'Inter'; padding:0px 8px; }"
            "QLineEdit:focus { background:#1A1E28; color:#E8ECF0; }")
            .arg(isActive_ ? kTrackNameActiveText.name() : kTrackNameText.name()));
        nameEdit_->setCursorPosition(0);
        connect(nameEdit_, &QLineEdit::editingFinished, this, [this]() {
            QString txt = nameEdit_->text().trimmed();
            if (!txt.isEmpty() && txt != name_) {
                name_ = txt;
                panel_->onRenameTrack(seqIndex_, name_);
            }
            nameEdit_->setText(name_);
            nameEdit_->setCursorPosition(0);
        });

        lockBtn_ = new QPushButton(QIcon(":/icons/layers/unlock.png"), "", this);
        lockBtn_->setFixedSize(28, 28);
        lockBtn_->setIconSize(QSize(20, 20));
        lockBtn_->setCheckable(true);
        lockBtn_->setToolTip("Lock / Unlock Sequence");
        lockBtn_->setStyleSheet(QString(
            "QPushButton { background:transparent; border:1px solid transparent; "
            "border-radius:3px; }"
            "QPushButton:hover { background:%1; border-color:%2; }")
            .arg(kBtnHover.name(), kPlayheadColor.name()));
        connect(lockBtn_, &QPushButton::clicked, this, [this](bool checked) {
            if (appState_) {
                appState_->setSequenceLocked(seqIndex_, checked);
                lockBtn_->setIcon(QIcon(checked ? ":/icons/layers/lock.png"
                                                : ":/icons/layers/unlock.png"));
                updateNameStyle();
            }
        });

        dupBtn_ = new QPushButton(QIcon(":/icons/layers/duplicate.png"), "", this);
        dupBtn_->setFixedSize(28, 28);
        dupBtn_->setIconSize(QSize(20, 20));
        dupBtn_->setToolTip("Duplicate Sequence");
        dupBtn_->setStyleSheet(QString(
            "QPushButton { background:transparent; border:1px solid transparent; "
            "border-radius:3px; }"
            "QPushButton:hover { background:%1; border-color:%2; }")
            .arg(kBtnHover.name(), kPlayheadColor.name()));
        connect(dupBtn_, &QPushButton::clicked, this, [this]() { panel_->onDupTrack(seqIndex_); });

        delBtn_ = new QPushButton(QIcon(":/icons/layers/delete.png"), "", this);
        delBtn_->setFixedSize(28, 28);
        delBtn_->setIconSize(QSize(20, 20));
        delBtn_->setToolTip("Delete Sequence");
        delBtn_->setStyleSheet(dupBtn_->styleSheet());
        connect(delBtn_, &QPushButton::clicked, this, [this]() { panel_->onDelTrack(seqIndex_); });

        upBtn_ = new QPushButton(QIcon(":/icons/layers/move_up.png"), "", this);
        upBtn_->setFixedSize(28, 28);
        upBtn_->setIconSize(QSize(20, 20));
        upBtn_->setToolTip("Move Up");
        upBtn_->setStyleSheet(dupBtn_->styleSheet());
        connect(upBtn_, &QPushButton::clicked, this, [this]() { panel_->onMoveTrack(seqIndex_, -1); });

        downBtn_ = new QPushButton(QIcon(":/icons/layers/move_down.png"), "", this);
        downBtn_->setFixedSize(28, 28);
        downBtn_->setIconSize(QSize(20, 20));
        downBtn_->setToolTip("Move Down");
        downBtn_->setStyleSheet(dupBtn_->styleSheet());
        connect(downBtn_, &QPushButton::clicked, this, [this]() { panel_->onMoveTrack(seqIndex_, 1); });

        opacityLabel_ = new QLabel("Opacity:", this);
        opacityLabel_->setStyleSheet(QString(
            "color:%1; font-size:9px; background:transparent;").arg(kTrackNameText.name()));

        opacitySlider_ = new QSlider(Qt::Horizontal, this);
        opacitySlider_->setRange(0, 100);
        opacitySlider_->setStyleSheet(QString(
            "QSlider::groove:horizontal { background:#1E2128; height:2px; border-radius:1px; }"
            "QSlider::handle:horizontal { background:%1; width:8px; height:8px; "
            "margin:-3px 0; border-radius:4px; }"
            "QSlider::sub-page:horizontal { background:%1; border-radius:1px; }")
            .arg(kAccentColor.name()));
        float initOpacity = appState_
            ? appState_->document().sequenceAt(static_cast<size_t>(seqIndex_)).opacity() : 1.0f;
        opacitySlider_->blockSignals(true);
        opacitySlider_->setValue(static_cast<int>(initOpacity * 100.0f));
        opacitySlider_->blockSignals(false);
        connect(opacitySlider_, &QSlider::valueChanged, this, [this](int val) {
            if (appState_) appState_->setSequenceOpacity(seqIndex_, val / 100.0f);
        });

        if (appState_) {
            bool initLocked = appState_->document().sequenceAt(static_cast<size_t>(seqIndex_)).locked();
            lockBtn_->setChecked(initLocked);
            lockBtn_->setIcon(QIcon(initLocked ? ":/icons/layers/lock.png"
                                               : ":/icons/layers/unlock.png"));
        }

        positionHeader();
    }

    void setName(const QString& name) { name_ = name; nameEdit_->setText(name); }
    void setActive(bool active) {
        isActive_ = active;
        updateNameStyle();
        update();
    }
    void updateNameStyle() {
        bool locked = appState_
            && appState_->document().sequenceAt(static_cast<size_t>(seqIndex_)).locked();
        QColor color = locked ? QColor("#FF4A4A")
                      : (isActive_ ? kTrackNameActiveText : kTrackNameText);
        nameEdit_->setStyleSheet(QString(
            "QLineEdit { background:transparent; color:%1; border:none; "
            "font-size:11px; font-family:'Inter'; padding:0px 8px; }"
            "QLineEdit:focus { background:#1A1E28; color:#E8ECF0; }")
            .arg(color.name()));
    }
    int seqIndex() const { return seqIndex_; }
    const QString& name() const { return name_; }
    bool isActive() const { return isActive_; }
    void positionHeader() {
        const int hdrW = TimelinePanelV2::kHeaderWidth;
        nameEdit_->setGeometry(8, 4, hdrW - 164, 22);
        lockBtn_->move(hdrW - 156, 3);
        upBtn_->move(hdrW - 126, 3);
        downBtn_->move(hdrW - 96, 3);
        dupBtn_->move(hdrW - 66, 3);
        delBtn_->move(hdrW - 36, 3);
        opacityLabel_->setGeometry(8, 38, 42, 16);
        opacitySlider_->setGeometry(50, 38, hdrW - 58, 16);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const int w = width();
        const int h = height();
        const int hdrW = TimelinePanelV2::kHeaderWidth;
        const int cellW = TimelinePanelV2::kCellWidth;
        const int cellTotal = TimelinePanelV2::kCellTotal;
        const int cellH = TimelinePanelV2::kTrackHeight - 22;
        const int offset = panel_->sharedScrollOffset();
        const int totalFrames = panel_->totalFrames();
        const int curFrame = panel_->currentFrame();

        QColor trackBg = isActive_ ? kTrackActiveBg
                        : (isHovered_ ? kTrackHoverBg : kTrackBg);
        QColor headerBg = isActive_ ? kHeaderActiveBg : kHeaderBg;

        p.fillRect(0, 0, hdrW, h, headerBg);

        if (isActive_) {
            p.fillRect(0, 0, 2, h, kAccentColor);
        }

        p.setPen(QPen(kBorderColor, 1));
        p.drawLine(hdrW, 0, hdrW, h);

        p.fillRect(hdrW, 0, w - hdrW, h, trackBg);
        p.setPen(QPen(kBorderColor, 1));
        p.drawLine(0, h - 1, w, h - 1);

        int cellY = 16;
        const auto& seq = appState_->document().sequenceAt(static_cast<size_t>(seqIndex_));
        int durFrames = seq.durationFrames();
        int seqTotal = seq.totalFrames();
        int firstVisible = std::max(0, offset / cellTotal);
        int lastVisible = std::min(durFrames - 1, (offset + w - hdrW) / cellTotal + 1);

        for (int f = firstVisible; f <= lastVisible; ++f) {
            int cx = hdrW + f * cellTotal - offset;
            QRect cellRect(cx, cellY, cellW, cellH);

            bool isCurFrame = (f == curFrame) && isActive_;
            bool hasContent = (f < seqTotal) && frameHasContent(f);

            QColor fillColor = hasContent ? kCellFilled : kCellEmpty;
            QColor borderColor = isCurFrame ? kCellActiveBorder : kCellBorder;
            if (isCurFrame && isActive_) fillColor = kAccentDim;

            p.setPen(Qt::NoPen);
            p.setBrush(fillColor);
            p.drawRoundedRect(cellRect, 2, 2);

            p.setPen(QPen(borderColor, isCurFrame ? 1.5 : 1.0));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(cellRect.adjusted(0, 0, -1, -1), 2, 2);

            if (f % 5 == 0) {
                p.setPen(kFrameNumColor);
                QFont numFont("JetBrains Mono", 5);
                p.setFont(numFont);
                p.drawText(cellRect, Qt::AlignCenter, QString::number(f + 1));
            }
        }

        int addX = hdrW + durFrames * cellTotal - offset;
        if (addX < w) {
            QRect addRect(addX + 1, cellY, cellW, cellH);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor("#141A18"));
            p.drawRoundedRect(addRect, 2, 2);
            p.setPen(QPen(QColor("#2A4030"), 1));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(addRect.adjusted(0, 0, -1, -1), 2, 2);

            int ax = addRect.center().x();
            int ay = addRect.center().y();
            p.setPen(QPen(kAddBtnColor, 1.5));
            p.drawLine(ax - 5, ay, ax + 5, ay);
            p.drawLine(ax, ay - 5, ax, ay + 5);
        }

        if (isActive_) {
            int px = hdrW + curFrame * cellTotal - offset + cellW / 2;
            if (px >= hdrW && px < w) {
                p.setPen(QPen(kPlayheadColor, 1));
                p.drawLine(px, 0, px, h);
            }
        }
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() != Qt::LeftButton) return;

        const int hdrW = TimelinePanelV2::kHeaderWidth;
        const int cellTotal = TimelinePanelV2::kCellTotal;
        const int totalFrames = panel_->totalFrames();
        int x = event->pos().x();

        if (x < hdrW) {
            if (!isActive_ && event->pos().y() < height() - 24) {
                panel_->onActivateTrack(seqIndex_);
            }
            return;
        }

        int addX = hdrW + totalFrames * cellTotal - panel_->sharedScrollOffset();
        if (x >= addX && event->pos().y() >= 16 && event->pos().y() < 16 + (TimelinePanelV2::kTrackHeight - 22)) {
            panel_->onTrackFrameClicked(-1);
            return;
        }

        int frame = hitFrame(x);
        if (frame >= 0 && !isActive_) panel_->onActivateTrack(seqIndex_);
        if (frame >= 0) panel_->onTrackFrameClicked(frame);
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        int frame = hitFrame(event->pos().x());
        if (frame != hoveredFrame_) { hoveredFrame_ = frame; update(); }
    }

    void enterEvent(QEnterEvent*) override { isHovered_ = true; update(); }
    void leaveEvent(QEvent*) override { isHovered_ = false; update(); }

    void resizeEvent(QResizeEvent*) override { positionHeader(); }

private:
    int hitFrame(int x) const {
        int rel = x - TimelinePanelV2::kHeaderWidth + panel_->sharedScrollOffset();
        if (rel < 0) return -1;
        int frame = rel / TimelinePanelV2::kCellTotal;
        if (frame >= panel_->totalFrames()) return -1;
        return frame;
    }

    bool frameHasContent(int frame) const {
        if (!appState_) return false;
        const auto& seq = appState_->document().sequenceAt(static_cast<size_t>(seqIndex_));
        const GroupLayer* root = seq.peekRootLayerForFrame(frame);
        if (!root || root->layerCount() == 0) return false;

        for (size_t i = 0; i < root->layerCount(); ++i) {
            const Layer* layer = root->layerAt(static_cast<int>(i));
            if (!layer || !layer->visible()) continue;

            if (layer->type() == LayerType::Raster) {
                const auto* rl = static_cast<const RasterLayer*>(layer);
                if (rl->hasContent()) return true;
            }

            if (layer->type() == LayerType::Vector) {
                const auto* vl = static_cast<const VectorLayer*>(layer);
                if (vl->strokeCount() > 0) return true;
            }
        }
        return false;
    }

    int seqIndex_;
    QString name_;
    bool isActive_ = false;
    bool isHovered_ = false;
    int hoveredFrame_ = -1;
    std::shared_ptr<AppState> appState_;
    TimelinePanelV2* panel_;
    QLineEdit* nameEdit_ = nullptr;
    QSlider* opacitySlider_ = nullptr;
    QLabel* opacityLabel_ = nullptr;
    QPushButton* dupBtn_ = nullptr;
    QPushButton* delBtn_ = nullptr;
    QPushButton* upBtn_ = nullptr;
    QPushButton* downBtn_ = nullptr;
    QPushButton* lockBtn_ = nullptr;
};

// ========================================================================
// TimelinePanelV2 Implementation
// ========================================================================

TimelinePanelV2::TimelinePanelV2(std::shared_ptr<AppState> state, QWidget* parent)
    : QWidget(parent)
    , appState_(std::move(state))
{
    setMouseTracking(true);
    setMinimumHeight(120);

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
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ---- Top bar ----
    auto* topBar = new QHBoxLayout();
    topBar->setContentsMargins(8, 4, 8, 4);
    topBar->setSpacing(6);

    auto makeBtn = [&](const QString& iconPath, const QString& tip, int w = 26, int h = 22) -> QPushButton* {
        auto* btn = new QPushButton(this);
        btn->setIcon(QIcon(iconPath));
        btn->setIconSize(QSize(w > 20 ? w - 4 : w, h > 18 ? h - 4 : h));
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

    prevBtn_ = makeBtn(":/icons/timeline/prev_frame.png", "Previous Frame (,)");
    topBar->addWidget(prevBtn_);
    playBtn_ = makeBtn(":/icons/timeline/play_pause.png", "Play / Pause (Space)");
    topBar->addWidget(playBtn_);
    stopBtn_ = makeBtn(":/icons/timeline/stop.png", "Stop");
    topBar->addWidget(stopBtn_);
    nextBtn_ = makeBtn(":/icons/timeline/next_frame.png", "Next Frame (.)");
    topBar->addWidget(nextBtn_);

    topBar->addSpacing(16);

    auto* fpsLabel = new QLabel("FPS", this);
    fpsLabel->setStyleSheet(QString("color:%1; font-size:10px; font-weight:600;").arg(kFrameNumColor.name()));
    topBar->addWidget(fpsLabel);

    fpsMinusBtn_ = new QPushButton("-", this);
    fpsMinusBtn_->setFixedSize(20, 22);
    fpsMinusBtn_->setToolTip("Decrease FPS");
    fpsMinusBtn_->setStyleSheet(QString(
        "QPushButton { background:%1; color:%2; border:1px solid %3; border-radius:3px; "
        "font-size:11px; font-weight:bold; }"
        "QPushButton:hover { background:%4; border-color:%5; }")
        .arg(kBtnBg.name(), kBtnText.name(), kCellBorder.name(),
             kBtnHover.name(), kPlayheadColor.name()));
    topBar->addWidget(fpsMinusBtn_);

    fpsSpin_ = new QSpinBox(this);
    fpsSpin_->setRange(1, 120);
    fpsSpin_->setValue(fps_);
    fpsSpin_->setFixedWidth(56);
    fpsSpin_->setStyleSheet(QString(
        "QSpinBox { background:%1; color:%2; border:1px solid %3; border-radius:3px; "
        "padding:1px 3px; font-size:12px; font-family:'JetBrains Mono',monospace; }"
        "QSpinBox:focus { border-color:%4; }")
        .arg(kBtnBg.name(), kBtnText.name(), kCellBorder.name(), kPlayheadColor.name()));
    topBar->addWidget(fpsSpin_);

    fpsPlusBtn_ = new QPushButton("+", this);
    fpsPlusBtn_->setFixedSize(20, 22);
    fpsPlusBtn_->setToolTip("Increase FPS");
    fpsPlusBtn_->setStyleSheet(fpsMinusBtn_->styleSheet());
    topBar->addWidget(fpsPlusBtn_);

    topBar->addSpacing(16);

    frameLabel_ = new QLabel("1 / 24", this);
    frameLabel_->setStyleSheet(QString(
        "color:%1; font-size:11px; font-family:'JetBrains Mono',monospace;").arg(kBtnText.name()));
    topBar->addWidget(frameLabel_);

    topBar->addStretch();

    newSeqBtn_ = new QPushButton("+ Track \u25BE", this);
    newSeqBtn_->setFixedSize(94, 24);
    newSeqBtn_->setToolTip("Add track");
    newSeqBtn_->setStyleSheet(QString(
        "QPushButton { background:%1; color:%2; border:1px solid %3; border-radius:4px; "
        "font-size:10px; font-weight:bold; padding:2px 10px; }"
        "QPushButton:hover { background:%4; border-color:%5; color:#E8ECF0; }"
        "QPushButton::menu-indicator { image:none; subcontrol-position:right center; "
        "subcontrol-origin:padding; left:2px; }")
        .arg(kBtnBg.name(), kBtnText.name(), kCellBorder.name(),
             kBtnHover.name(), kPlayheadColor.name()));
    auto* addMenu = new QMenu(this);
    addMenu->setStyleSheet(QString(
        "QMenu { background:%1; color:%2; border:1px solid %3; border-radius:4px; "
        "padding:4px; }"
        "QMenu::item { padding:4px 16px; border-radius:3px; }"
        "QMenu::item:selected { background:%4; color:#E8ECF0; }")
        .arg(kBtnBg.name(), kBtnText.name(), kCellBorder.name(), kBtnHover.name()));
    addMenu->addAction("Add Animation Sequence", this, &TimelinePanelV2::onNewSequence);
    addMenu->addAction("Add Audio Track", this, &TimelinePanelV2::onImportAudio);
    newSeqBtn_->setMenu(addMenu);
    topBar->addWidget(newSeqBtn_);

    mainLayout->addLayout(topBar);

    // ---- Ruler ----
    rulerWidget_ = new RulerWidget(this, this);
    mainLayout->addWidget(rulerWidget_);

    // ---- Scroll area ----
    scrollArea_ = new QScrollArea(this);
    scrollArea_->setWidgetResizable(true);
    scrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    scrollArea_->setFrameShape(QFrame::NoFrame);
    scrollArea_->setStyleSheet(QString(
        "QScrollArea { background:%1; border:none; }"
        "QScrollBar:vertical { background:%1; width:6px; margin:0; }"
        "QScrollBar::handle:vertical { background:%2; border-radius:3px; min-height:20px; }"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical { height:0; }")
        .arg(kScrollBg.name(), kScrollHandle.name()));

    tracksContainer_ = new QWidget();
    tracksLayout_ = new QVBoxLayout(tracksContainer_);
    tracksLayout_->setContentsMargins(0, 0, 0, 0);
    tracksLayout_->setSpacing(0);
    scrollArea_->setWidget(tracksContainer_);

    mainLayout->addWidget(scrollArea_, 1);

    // ---- Horizontal scrollbar ----
    hScrollBar_ = new QScrollBar(Qt::Horizontal, this);
    hScrollBar_->setFixedHeight(8);
    hScrollBar_->setMinimum(0);
    hScrollBar_->setStyleSheet(QString(
        "QScrollBar:horizontal { background:%1; height:8px; border:none; }"
        "QScrollBar::handle:horizontal { background:%2; border-radius:4px; min-width:30px; }"
        "QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal { width:0; }")
        .arg(kScrollBg.name(), kScrollHandle.name()));
    mainLayout->addWidget(hScrollBar_);

    connect(prevBtn_, &QPushButton::clicked, this, &TimelinePanelV2::onPrevFrame);
    connect(nextBtn_, &QPushButton::clicked, this, &TimelinePanelV2::onNextFrame);
    connect(playBtn_, &QPushButton::clicked, this, &TimelinePanelV2::onPlayPause);
    connect(stopBtn_, &QPushButton::clicked, this, &TimelinePanelV2::onStop);
    connect(fpsSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &TimelinePanelV2::onFPSChanged);
    connect(hScrollBar_, &QScrollBar::valueChanged, this, &TimelinePanelV2::onScrollChanged);

    connect(fpsMinusBtn_, &QPushButton::clicked, this, [this]() {
        if (!appState_) return;
        int currentFps = appState_->activeSequence().fps();
        if (currentFps > 1) {
            appState_->setFps(currentFps - 1);
        }
    });

    connect(fpsPlusBtn_, &QPushButton::clicked, this, [this]() {
        if (!appState_) return;
        int currentFps = appState_->activeSequence().fps();
        if (currentFps < 120) {
            appState_->setFps(currentFps + 1);
        }
    });

    connect(appState_.get(), &AppState::documentChanged, this, [this]() {
        int seqFps = appState_->activeSequence().fps();
        if (fps_ != seqFps) {
            fps_ = seqFps;
            fpsSpin_->blockSignals(true);
            fpsSpin_->setValue(fps_);
            fpsSpin_->blockSignals(false);
            if (playbackTimer_->isActive()) {
                playbackTimer_->setInterval(1000 / fps_);
            }
            double rate = static_cast<double>(fps_) / 24.0;
            for (auto* at : audioTrackWidgets_)
                at->player()->setPlaybackRate(rate);
            updateLabels();
        }
    });

    rebuildTracks();
}

void TimelinePanelV2::rebuildTracks()
{
    QWidget* focused = QApplication::focusWidget();
    if (focused && tracksContainer_->isAncestorOf(focused)) {
        focused->clearFocus();
    }

    for (auto* at : audioTrackWidgets_) {
        tracksLayout_->removeWidget(at);
    }

    for (auto* tw : trackWidgets_) {
        tracksLayout_->removeWidget(tw);
        tw->deleteLater();
    }
    trackWidgets_.clear();

    for (int i = tracksLayout_->count() - 1; i >= 0; --i) {
        auto* item = tracksLayout_->itemAt(i);
        if (item && item->spacerItem()) {
            tracksLayout_->takeAt(i);
            delete item;
        }
    }

    if (!appState_) {
        for (auto* at : audioTrackWidgets_) {
            tracksLayout_->addWidget(at);
            at->positionHeader();
        }
        tracksLayout_->addStretch();
        return;
    }

    int count = appState_->sequenceCount();
    if (count == 0) {
        appState_->addSequence("Sequence 1");
        count = 1;
    }
    int activeIdx = appState_->activeSequenceIndex();

    for (int i = 0; i < count; ++i) {
        auto& seq = appState_->document().sequenceAt(static_cast<size_t>(i));
        auto* track = new SequenceTrackWidget(
            i,
            QString::fromStdString(seq.name()),
            (i == activeIdx),
            appState_,
            this,
            tracksContainer_);

        tracksLayout_->addWidget(track);
        track->positionHeader();
        trackWidgets_.push_back(track);
    }

    for (auto* at : audioTrackWidgets_) {
        tracksLayout_->addWidget(at);
        at->positionHeader();
    }

    tracksLayout_->addStretch();
    tracksLayout_->update();
    updateScrollBarRange();
    rulerWidget_->update();
}

void TimelinePanelV2::refreshTimelineLayout()
{
    updateScrollBarRange();
    tracksLayout_->update();
    rulerWidget_->update();
    for (auto* t : trackWidgets_) t->update();
    for (auto* at : audioTrackWidgets_) at->update();
}

void TimelinePanelV2::updateScrollBarRange()
{
    int durFrames = totalFrames_;
    if (appState_) {
        durFrames = appState_->activeSequence().durationFrames();
    }
    int contentWidth = (durFrames + 1) * kCellTotal + kHeaderWidth + 20;
    int viewWidth = scrollArea_->viewport()->width();
    int maxScroll = std::max(0, contentWidth - viewWidth);
    hScrollBar_->setRange(0, maxScroll);
    hScrollBar_->setPageStep(viewWidth);
    if (scrollOffset_ > maxScroll) {
        scrollOffset_ = maxScroll;
        hScrollBar_->setValue(scrollOffset_);
    }
}

void TimelinePanelV2::setTotalFrames(int count)
{
    totalFrames_ = std::max(1, count);
    if (currentFrame_ >= totalFrames_) currentFrame_ = totalFrames_ - 1;
    updateScrollBarRange();
    updateLabels();
    rulerWidget_->update();
    for (auto* t : trackWidgets_) t->update();
}

void TimelinePanelV2::setCurrentFrame(int frame)
{
    currentFrame_ = std::clamp(frame, 0, totalFrames_ - 1);

    int viewWidth = scrollArea_->viewport()->width();
    int cellTotal = kCellTotal;
    int frameLeft = currentFrame_ * cellTotal - scrollOffset_;
    int frameRight = frameLeft + kCellWidth;
    int bodyViewWidth = viewWidth - kHeaderWidth;

    if (frameLeft < 0) {
        scrollOffset_ = std::max(0, scrollOffset_ + frameLeft - cellTotal * 2);
    } else if (frameRight > bodyViewWidth) {
        scrollOffset_ += frameRight - bodyViewWidth + cellTotal * 2;
    }

    int durFrames = appState_
        ? appState_->activeSequence().durationFrames()
        : totalFrames_;
    int maxScroll = std::max(0, (durFrames + 1) * cellTotal + kHeaderWidth + 20 - viewWidth);
    scrollOffset_ = std::clamp(scrollOffset_, 0, maxScroll);
    hScrollBar_->setValue(scrollOffset_);

    updateLabels();
    rulerWidget_->update();
    for (auto* t : trackWidgets_) t->update();
    for (auto* at : audioTrackWidgets_) at->update();

    if (!playing_) {
        for (auto* at : audioTrackWidgets_)
            at->syncToFrame(currentFrame_, fps_, false);
    }
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

void TimelinePanelV2::invalidateFrameThumbnail(int)
{
    for (auto* t : trackWidgets_) t->update();
}

void TimelinePanelV2::togglePlayback() { onPlayPause(); }

void TimelinePanelV2::updateLabels()
{
    frameLabel_->setText(QString("%1 / %2").arg(currentFrame_ + 1).arg(totalFrames_));
}

void TimelinePanelV2::onPlayPause()
{
    if (playing_) {
        playbackTimer_->stop();
        playing_ = false;
        for (auto* at : audioTrackWidgets_)
            at->player()->pause();
        playBtn_->setToolTip("Play / Pause (Space)");
        emit playbackToggled(false);
    } else {
        double rate = static_cast<double>(fps_) / 24.0;
        for (auto* at : audioTrackWidgets_) {
            at->player()->setPlaybackRate(rate);
            at->syncToFrame(currentFrame_, fps_, true);
        }
        playbackTimer_->start(1000 / fps_);
        playing_ = true;
        playBtn_->setToolTip("Playing... Click to Pause (Space)");
        emit playbackToggled(true);
    }
}

void TimelinePanelV2::onStop()
{
    playbackTimer_->stop();
    playing_ = false;
    playBtn_->setToolTip("Play / Pause (Space)");
    currentFrame_ = 0;
    scrollOffset_ = 0;
    appState_->setCurrentFrame(0);
    hScrollBar_->setValue(0);
    updateLabels();
    rulerWidget_->update();
    for (auto* t : trackWidgets_) t->update();
    emit frameChanged(currentFrame_);
    emit playbackToggled(false);

    for (auto* at : audioTrackWidgets_) {
        at->player()->stop();
        at->syncToFrame(0, fps_, false);
        at->update();
    }
}

void TimelinePanelV2::onPrevFrame()
{
    if (currentFrame_ > 0) {
        currentFrame_--;
        appState_->setCurrentFrame(currentFrame_);
        setCurrentFrame(currentFrame_);
        emit frameChanged(currentFrame_);
    }
}

void TimelinePanelV2::onNextFrame()
{
    if (currentFrame_ < totalFrames_ - 1) {
        currentFrame_++;
        appState_->setCurrentFrame(currentFrame_);
        setCurrentFrame(currentFrame_);
        emit frameChanged(currentFrame_);
    }
}

void TimelinePanelV2::onFPSChanged(int fps)
{
    if (updatingFps_) return;
    updatingFps_ = true;
    appState_->setFps(std::max(1, std::min(120, fps)));
    updatingFps_ = false;
}

void TimelinePanelV2::onPlaybackTick()
{
    if (!appState_) return;

    int prevFrame = currentFrame_;
    currentFrame_ = appState_->activeSequence().advanceFrame();

    if (currentFrame_ < prevFrame) {
        bool looping = appState_->activeSequence().looping();

        if (looping) {
            for (auto* at : audioTrackWidgets_)
                at->syncToFrame(currentFrame_, fps_, true);
        } else {
            int waEnd = appState_->activeSequence().effectiveWorkAreaEnd();
            currentFrame_ = std::max(0, waEnd - 1);
            appState_->setCurrentFrame(currentFrame_);

            playbackTimer_->stop();
            playing_ = false;
            playBtn_->setToolTip("Play / Pause (Space)");
            emit playbackToggled(false);

            for (auto* at : audioTrackWidgets_) {
                at->player()->pause();
                at->syncToFrame(currentFrame_, fps_, false);
            }

            updateLabels();
            setCurrentFrame(currentFrame_);
            emit frameChanged(currentFrame_);
            return;
        }
    }

    appState_->setCurrentFrame(currentFrame_);
    updateLabels();
    setCurrentFrame(currentFrame_);
    emit frameChanged(currentFrame_);
}

void TimelinePanelV2::onScrollChanged(int value)
{
    scrollOffset_ = value;
    rulerWidget_->update();
    for (auto* t : trackWidgets_) t->update();
    for (auto* at : audioTrackWidgets_) at->update();
}

void TimelinePanelV2::addFrame()
{
    totalFrames_++;
    appState_->document().setTotalFrames(totalFrames_);
    updateScrollBarRange();
    updateLabels();
    rulerWidget_->update();
    for (auto* t : trackWidgets_) t->update();
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

    updateScrollBarRange();
    updateLabels();
    rulerWidget_->update();
    for (auto* t : trackWidgets_) t->update();
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

    if (currentFrame_ >= totalFrames_) currentFrame_ = totalFrames_ - 1;
    appState_->setCurrentFrame(currentFrame_);

    updateScrollBarRange();
    updateLabels();
    rulerWidget_->update();
    for (auto* t : trackWidgets_) t->update();
    emit frameCountChanged(totalFrames_);
    emit frameChanged(currentFrame_);
}

void TimelinePanelV2::onNewSequence()
{
    if (!appState_) return;
    appState_->addSequence();
    QTimer::singleShot(0, this, [this]() { rebuildTracks(); });
}

void TimelinePanelV2::onImportAudio()
{
    QString path = QFileDialog::getOpenFileName(this, "Import Audio", "",
        "Audio Files (*.mp3 *.wav *.ogg *.flac);;All Files (*)");
    if (path.isEmpty()) return;

    auto* track = new AudioTrackWidget(path,
        static_cast<int>(audioTrackWidgets_.size()),
        appState_, this, tracksContainer_);

    connect(track, &AudioTrackWidget::moveUpRequested, this, [this, track]() {
        auto it = std::find(audioTrackWidgets_.begin(), audioTrackWidgets_.end(), track);
        if (it != audioTrackWidgets_.end()) {
            onMoveAudioTrack(static_cast<int>(std::distance(audioTrackWidgets_.begin(), it)), -1);
        }
    });
    connect(track, &AudioTrackWidget::moveDownRequested, this, [this, track]() {
        auto it = std::find(audioTrackWidgets_.begin(), audioTrackWidgets_.end(), track);
        if (it != audioTrackWidgets_.end()) {
            onMoveAudioTrack(static_cast<int>(std::distance(audioTrackWidgets_.begin(), it)), 1);
        }
    });

    int insertPos = std::max(0, tracksLayout_->count() - 1);
    tracksLayout_->insertWidget(insertPos, track);
    audioTrackWidgets_.push_back(track);
    track->positionHeader();
}

void TimelinePanelV2::removeAudioTrack(AudioTrackWidget* track)
{
    auto it = std::find(audioTrackWidgets_.begin(), audioTrackWidgets_.end(), track);
    if (it != audioTrackWidgets_.end()) {
        audioTrackWidgets_.erase(it);
        tracksLayout_->removeWidget(track);
        track->deleteLater();

        for (size_t i = 0; i < audioTrackWidgets_.size(); ++i) {
            audioTrackWidgets_[i]->setTrackIndex(static_cast<int>(i));
        }
    }
}

void TimelinePanelV2::onMoveAudioTrack(int index, int delta)
{
    if (index < 0 || index >= static_cast<int>(audioTrackWidgets_.size())) return;

    auto* widget = audioTrackWidgets_[index];
    int oldPos = tracksLayout_->indexOf(widget);
    if (oldPos < 0) return;

    int newPos = oldPos + delta;
    int maxPos = tracksLayout_->count() - 2;
    if (newPos < 0 || newPos > maxPos) return;

    auto* item = tracksLayout_->takeAt(oldPos);
    if (item) {
        tracksLayout_->insertItem(newPos, item);
        tracksLayout_->update();
        widget->positionHeader();
        widget->update();
    }
}

void TimelinePanelV2::onActivateTrack(int seqIndex)
{
    if (!appState_) return;
    appState_->setActiveSequence(seqIndex);

    auto& seq = appState_->activeSequence();
    totalFrames_ = seq.totalFrames();
    fps_ = seq.fps();
    currentFrame_ = seq.currentFrame();

    fpsSpin_->blockSignals(true);
    fpsSpin_->setValue(fps_);
    fpsSpin_->blockSignals(false);

    double rate = static_cast<double>(fps_) / 24.0;
    for (auto* at : audioTrackWidgets_)
        at->player()->setPlaybackRate(rate);

    scrollOffset_ = 0;
    hScrollBar_->setValue(0);
    updateScrollBarRange();
    updateLabels();

    for (size_t i = 0; i < trackWidgets_.size(); ++i) {
        trackWidgets_[i]->setActive(static_cast<int>(i) == seqIndex);
    }
    rulerWidget_->update();

    emit sequenceChanged(seqIndex);
    emit frameChanged(currentFrame_);
}

void TimelinePanelV2::onRenameTrack(int seqIndex, const QString& name)
{
    if (!appState_) return;
    appState_->renameSequence(seqIndex, name.toStdString());
}

void TimelinePanelV2::onDupTrack(int seqIndex)
{
    if (!appState_) return;
    appState_->duplicateSequence(seqIndex);
    QTimer::singleShot(0, this, [this]() { rebuildTracks(); });
}

void TimelinePanelV2::onDelTrack(int seqIndex)
{
    if (!appState_) return;
    if (appState_->sequenceCount() <= 1) return;
    appState_->removeSequence(seqIndex);
    QTimer::singleShot(0, this, [this]() { rebuildTracks(); });
}

void TimelinePanelV2::onMoveTrack(int seqIndex, int delta)
{
    if (!appState_) return;
    int target = seqIndex + delta;
    if (target < 0 || target >= appState_->sequenceCount()) return;
    appState_->moveSequence(seqIndex, target);
    QTimer::singleShot(0, this, [this]() { rebuildTracks(); });
}

void TimelinePanelV2::onTrackFrameClicked(int frame)
{
    if (frame < 0) { addFrame(); return; }
    currentFrame_ = frame;
    appState_->setCurrentFrame(currentFrame_);
    updateLabels();
    rulerWidget_->update();
    for (auto* t : trackWidgets_) t->update();
    emit frameChanged(currentFrame_);
    for (auto* at : audioTrackWidgets_)
        at->syncToFrame(currentFrame_, fps_, false);
}

void TimelinePanelV2::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateScrollBarRange();
    rulerWidget_->update();
    for (auto* t : trackWidgets_) t->update();
    for (auto* at : audioTrackWidgets_) at->update();
}

} // namespace fap
