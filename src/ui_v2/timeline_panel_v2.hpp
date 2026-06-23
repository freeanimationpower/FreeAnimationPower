#pragma once

#include <QtWidgets/QWidget>
#include <QtCore/QTimer>
#include <QtWidgets/QScrollBar>
#include <memory>

class QPushButton;
class QSpinBox;
class QLabel;

namespace fap {

class AppState;
class FrameThumbnailCache;

class TimelinePanelV2 : public QWidget {
    Q_OBJECT
public:
    explicit TimelinePanelV2(std::shared_ptr<AppState> state, QWidget* parent = nullptr);

    void setTotalFrames(int count);
    void setCurrentFrame(int frame);
    void setFPS(int fps);

    int currentFrame() const { return currentFrame_; }
    int totalFrames() const { return totalFrames_; }

    void togglePlayback();
    void invalidateFrameThumbnail(int frame);

signals:
    void frameChanged(int frame);
    void frameCountChanged(int count);
    void fpsChanged(int fps);
    void playbackToggled(bool playing);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void setupUI();
    void updateLabels();
    void onPlayPause();
    void onStop();
    void onPrevFrame();
    void onNextFrame();
    void onFPSChanged(int fps);
    void onPlaybackTick();
    void onScrollChanged(int value);
    void addFrame();
    void duplicateFrame();
    void deleteFrame();

    int frameToX(int frame) const;
    int frameAtPos(int x) const;
    int visibleFrameCount() const;

    std::shared_ptr<AppState> appState_;

    QPushButton* playBtn_ = nullptr;
    QPushButton* stopBtn_ = nullptr;
    QPushButton* prevBtn_ = nullptr;
    QPushButton* nextBtn_ = nullptr;
    QPushButton* addFrameBtn_ = nullptr;
    QSpinBox* fpsSpin_ = nullptr;
    QLabel* frameLabel_ = nullptr;
    QScrollBar* scrollBar_ = nullptr;

    QTimer* playbackTimer_ = nullptr;

    int currentFrame_ = 0;
    int totalFrames_ = 24;
    int fps_ = 24;
    bool playing_ = false;

    int scrollOffset_ = 0;
    int cellWidth_ = 58;
    int cellHeight_ = 78;
    int cellSpacing_ = 4;
    int thumbPad_ = 2;
    int trackTop_ = 0;
    int trackHeight_ = 0;

    bool scrubbing_ = false;
    int hoveredFrame_ = -1;

    int txPort_ = 0;

    static constexpr int kTransportWidth = 48;
    static constexpr int kFpsWidth = 56;
    static constexpr int kLabelWidth = 72;
};
} // namespace fap
