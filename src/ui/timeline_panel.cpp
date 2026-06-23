#include "ui/timeline_panel.hpp"

#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMenu>
#include <QApplication>

namespace fap {

TimelinePanel::GridView::GridView(TimelinePanel* panel)
    : QWidget(panel), panel_(panel) {
    setMouseTracking(true);
}

void TimelinePanel::GridView::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(37, 37, 38));

    int startFrame = panel_->scroll_x_ / panel_->kCellW;
    int endFrame = std::min(startFrame + (width() / panel_->kCellW) + 2, panel_->total_frames_);
    endFrame = std::max(endFrame, startFrame + 1);

    QFont numFont("Arial", 9);
    p.setFont(numFont);

    for (int f = startFrame; f < endFrame; ++f) {
        int x = f * panel_->kCellW - panel_->scroll_x_;
        QRect cellRect(x, panel_->kHeaderH, panel_->kCellW - 1, panel_->kCellH);

        if (f == panel_->current_frame_) {
            p.fillRect(QRect(x, 0, panel_->kCellW - 1, height()), QColor(9, 71, 113, 100));
        } else if (f % 2 == 0) {
            p.fillRect(QRect(x, 0, panel_->kCellW - 1, height()), QColor(42, 42, 44));
        } else {
            p.fillRect(QRect(x, 0, panel_->kCellW - 1, height()), QColor(37, 37, 38));
        }

        p.setPen(QColor(180, 180, 180));
        p.drawText(QRect(x, 0, panel_->kCellW - 1, panel_->kHeaderH),
                   Qt::AlignCenter, QString::number(f + 1));

        if (static_cast<size_t>(f) < panel_->thumbnails_.size() &&
            !panel_->thumbnails_[f].isNull()) {
            QImage thumb = panel_->thumbnails_[f];
            int tw = panel_->kCellW - panel_->kThumbPad * 2;
            int th = panel_->kCellH - panel_->kThumbPad * 2;
            QRect thumbRect(x + panel_->kThumbPad,
                           panel_->kHeaderH + panel_->kThumbPad, tw, th);
            p.drawImage(thumbRect, thumb.scaled(tw, th, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }

        p.setPen(QPen(QColor(80, 80, 80), 1));
        p.drawRect(x, 0, panel_->kCellW - 1, height() - 1);

        if (f == panel_->current_frame_) {
            int px = x + panel_->kCellW / 2;
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(255, 80, 60));
            QPointF tri[3] = { QPointF(px - 5, 0), QPointF(px + 5, 0), QPointF(px, 14) };
            p.drawPolygon(tri, 3);
            p.setPen(QPen(QColor(255, 80, 60), 2));
            p.drawLine(px, 14, px, height());
        }

        int prevStart = panel_->current_frame_ - panel_->onion_prev_;
        int prevEnd = panel_->current_frame_;
        if (f >= prevStart && f < prevEnd && f >= 0) {
            QColor tint(255, 60, 60, 40);
            p.fillRect(QRect(x, panel_->kHeaderH, panel_->kCellW - 1, panel_->kCellH), tint);
        }
        int nextStart = panel_->current_frame_ + 1;
        int nextEnd = panel_->current_frame_ + 1 + panel_->onion_next_;
        if (f >= nextStart && f < nextEnd && f < panel_->total_frames_) {
            QColor tint(60, 100, 255, 40);
            p.fillRect(QRect(x, panel_->kHeaderH, panel_->kCellW - 1, panel_->kCellH), tint);
        }

        p.setPen(QPen(QColor(80, 80, 80), 1));
        p.drawLine(x, 0, x, height());
    }

    p.setPen(QPen(QColor(60, 60, 60), 1));
    p.drawLine(0, panel_->kHeaderH - 1, width(), panel_->kHeaderH - 1);
}

void TimelinePanel::GridView::mousePressEvent(QMouseEvent* ev) {
    if (ev->button() != Qt::LeftButton) return;
    int f = (ev->pos().x() + panel_->scroll_x_) / panel_->kCellW;
    if (f >= 0 && f < panel_->total_frames_) {
        panel_->current_frame_ = f;
        emit panel_->frameChanged(f + 1);
        panel_->update();
    }
}

TimelinePanel::TimelinePanel(QWidget* parent) : QWidget(parent) {
    thumbnails_.resize(total_frames_);

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto topBar = new QHBoxLayout();
    topBar->setContentsMargins(4, 2, 4, 2);
    topBar->setSpacing(3);

    auto makeBtn = [](const QString& text, const QString& tip) {
        auto* b = new QPushButton(text);
        b->setToolTip(tip);
        b->setFixedSize(24, 24);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(
            "QPushButton{background:#3c3c3c;color:#cccccc;border:1px solid #555;border-radius:2px;font-size:12px;}"
            "QPushButton:hover{background:#4a4a4a;}"
            "QPushButton:pressed{background:#2a2a2a;}");
        return b;
    };

    play_btn_ = makeBtn(QStringLiteral("\u25B6"), "Play / Pause");
    prev_btn_ = makeBtn(QStringLiteral("\u25C0"), "Previous Frame");
    next_btn_ = makeBtn(QStringLiteral("\u25B6"), "Next Frame");
    add_btn_  = makeBtn("+", "Add Frame");
    dup_btn_  = makeBtn("++", "Duplicate Frame");
    del_btn_  = makeBtn("\u2212", "Delete Frame");

    frame_label_ = new QLabel("1 / 24");
    frame_label_->setStyleSheet("color:#75beff;font-size:10px;font-weight:bold;");
    frame_label_->setFixedWidth(60);
    frame_label_->setAlignment(Qt::AlignCenter);

    fps_label_ = new QLabel("24 fps");
    fps_label_->setStyleSheet("color:#888;font-size:10px;");
    fps_label_->setFixedWidth(36);
    fps_label_->setAlignment(Qt::AlignCenter);

    topBar->addWidget(prev_btn_);
    topBar->addWidget(play_btn_);
    topBar->addWidget(next_btn_);
    topBar->addSpacing(4);
    topBar->addWidget(frame_label_);
    topBar->addWidget(fps_label_);
    topBar->addStretch();
    topBar->addWidget(add_btn_);
    topBar->addWidget(dup_btn_);
    topBar->addWidget(del_btn_);

    grid_view_ = new GridView(this);

    scroll_bar_ = new QScrollBar(Qt::Horizontal, this);
    scroll_bar_->setFixedHeight(14);
    scroll_bar_->setStyleSheet(
        "QScrollBar:horizontal{background:#1e1e1e;border:none;height:14px;}"
        "QScrollBar::handle:horizontal{background:#555;min-width:30px;border-radius:2px;}"
        "QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal{width:0px;}");

    mainLayout->addLayout(topBar);
    mainLayout->addWidget(grid_view_, 1);
    mainLayout->addWidget(scroll_bar_);

    connect(play_btn_, &QPushButton::clicked, [this]() {
        togglePlayback();
    });

    auto nav = [this](int delta) {
        int f = current_frame_ + delta;
        if (f >= 0 && f < total_frames_) { current_frame_ = f; emit frameChanged(f + 1); update(); }
    };
    connect(prev_btn_, &QPushButton::clicked, [this, nav]() { nav(-1); });
    connect(next_btn_, &QPushButton::clicked, [this, nav]() { nav(1); });

    connect(add_btn_, &QPushButton::clicked, [this]() {
        addFrameAt(current_frame_);
    });

    connect(dup_btn_, &QPushButton::clicked, [this]() {
        total_frames_++;
        int src = current_frame_ + 1;
        current_frame_ = total_frames_ - 1;
        thumbnails_.resize(total_frames_);
        scroll_bar_->setRange(0, std::max(0, total_frames_ * kCellW - grid_view_->width()));
        emit frameCountChanged(total_frames_);
        emit frameDuplicated(src, current_frame_ + 1);
        emit frameChanged(current_frame_ + 1);
        update();
    });

    connect(del_btn_, &QPushButton::clicked, [this]() {
        removeFrameAt(current_frame_);
    });

    connect(scroll_bar_, &QScrollBar::valueChanged, [this](int v) {
        scroll_x_ = v;
        grid_view_->update();
    });

    setStyleSheet("QWidget{background:#252526;}");
}

void TimelinePanel::addFrameAt(int frame) {
    total_frames_++;
    thumbnails_.resize(total_frames_);
    emit frameCountChanged(total_frames_);
    emit frameInserted(frame + 1);
    scroll_bar_->setRange(0, std::max(0, total_frames_ * kCellW - grid_view_->width()));
    update();
}

void TimelinePanel::removeFrameAt(int frame) {
    if (total_frames_ <= 1) return;
    total_frames_--;
    if (frame < total_frames_) {
        thumbnails_.erase(thumbnails_.begin() + frame);
    } else {
        thumbnails_.pop_back();
    }
    if (current_frame_ >= total_frames_) current_frame_ = total_frames_ - 1;
    emit frameRemoved(frame + 1);
    emit frameCountChanged(total_frames_);
    emit frameChanged(current_frame_ + 1);
    scroll_bar_->setRange(0, std::max(0, total_frames_ * kCellW - grid_view_->width()));
    update();
}

void TimelinePanel::goToFirst() {
    current_frame_ = 0;
    emit frameChanged(1);
    update();
}

void TimelinePanel::goToLast() {
    current_frame_ = total_frames_ - 1;
    emit frameChanged(total_frames_);
    update();
}

void TimelinePanel::setTotalFrames(int count) {
    total_frames_ = std::max(1, count);
    thumbnails_.resize(total_frames_);
    if (current_frame_ >= total_frames_) current_frame_ = total_frames_ - 1;
    scroll_bar_->setRange(0, std::max(0, total_frames_ * kCellW - grid_view_->width()));
    update();
}

void TimelinePanel::setCurrentFrame(int frame) {
    int f = std::max(0, frame - 1);
    if (f < total_frames_) {
        current_frame_ = f;
        int x = f * kCellW;
        if (x < scroll_x_ || x + kCellW > scroll_x_ + grid_view_->width()) {
            scroll_x_ = std::max(0, x - grid_view_->width() / 2);
            scroll_bar_->setValue(scroll_x_);
        }
        frame_label_->setText(QString("%1 / %2").arg(current_frame_ + 1).arg(total_frames_));
        update();
    }
}

void TimelinePanel::setFPS(int fps) {
    fps_ = std::max(1, fps);
    fps_label_->setText(QString("%1 fps").arg(fps_));
}

void TimelinePanel::setOnionSkin(int prev, int next) {
    onion_prev_ = prev;
    onion_next_ = next;
    update();
}

void TimelinePanel::setFrameThumbnail(int frame, const QImage& thumb) {
    int idx = frame - 1;
    if (idx >= 0 && idx < total_frames_) {
        if (static_cast<size_t>(idx) >= thumbnails_.size())
            thumbnails_.resize(idx + 1);
        thumbnails_[idx] = thumb;
        grid_view_->update();
    }
}

void TimelinePanel::togglePlayback() {
    playing_ = !playing_;
    play_btn_->setText(playing_ ? QStringLiteral("\u25A0") : QStringLiteral("\u25B6"));
    if (playing_) {
        play_timer_ = startTimer(1000 / std::max(1, fps_));
    } else {
        if (play_timer_) { killTimer(play_timer_); play_timer_ = 0; }
    }
}

void TimelinePanel::timerEvent(QTimerEvent*) {
    if (!playing_) return;
    current_frame_ = (current_frame_ + 1) % total_frames_;
    emit frameChanged(current_frame_ + 1);
    int x = current_frame_ * kCellW;
    if (x < scroll_x_ || x + kCellW > scroll_x_ + grid_view_->width()) {
        scroll_x_ = std::max(0, x - grid_view_->width() / 2);
        scroll_bar_->setValue(scroll_x_);
    }
    update();
}

void TimelinePanel::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), QColor(37, 37, 38));
}

} // namespace fap
