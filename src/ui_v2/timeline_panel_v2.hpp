#pragma once

#include <QtWidgets/QWidget>
#include <QtCore/QTimer>
#include <memory>
#include <vector>

class QPushButton;
class QSpinBox;
class QLabel;
class QScrollArea;
class QScrollBar;
class QVBoxLayout;

namespace fap {

class AppState;
class SequenceTrackWidget;
class RulerWidget;

class TimelinePanelV2 : public QWidget {
    Q_OBJECT
public:
    explicit TimelinePanelV2(std::shared_ptr<AppState> state, QWidget* parent = nullptr);

    void setTotalFrames(int count);
    void setCurrentFrame(int frame);
    void setFPS(int fps);

    int currentFrame() const { return currentFrame_; }
    int totalFrames() const { return totalFrames_; }
    int sharedScrollOffset() const { return scrollOffset_; }

    void togglePlayback();
    void invalidateFrameThumbnail(int frame);
    void rebuildTracks();

    void onActivateTrack(int seqIndex);
    void onRenameTrack(int seqIndex, const QString& name);
    void onDupTrack(int seqIndex);
    void onDelTrack(int seqIndex);
    void onMoveTrack(int seqIndex, int delta);
    void onTrackFrameClicked(int frame);

    static constexpr int kHeaderWidth = 280;
    static constexpr int kCellWidth = 32;
    static constexpr int kCellSpacing = 1;
    static constexpr int kCellTotal = kCellWidth + kCellSpacing;
    static constexpr int kTrackHeight = 64;
    static constexpr int kRulerHeight = 18;

signals:
    void frameChanged(int frame);
    void frameCountChanged(int count);
    void fpsChanged(int fps);
    void playbackToggled(bool playing);
    void sequenceChanged(int index);

protected:
    void resizeEvent(QResizeEvent* event) override;

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
    void onNewSequence();
    void updateScrollBarRange();

    std::shared_ptr<AppState> appState_;

    QPushButton* playBtn_ = nullptr;
    QPushButton* stopBtn_ = nullptr;
    QPushButton* prevBtn_ = nullptr;
    QPushButton* nextBtn_ = nullptr;
    QSpinBox* fpsSpin_ = nullptr;
    QLabel* frameLabel_ = nullptr;
    QPushButton* newSeqBtn_ = nullptr;

    RulerWidget* rulerWidget_ = nullptr;
    QScrollArea* scrollArea_ = nullptr;
    QWidget* tracksContainer_ = nullptr;
    QVBoxLayout* tracksLayout_ = nullptr;
    QScrollBar* hScrollBar_ = nullptr;

    std::vector<SequenceTrackWidget*> trackWidgets_;

    QTimer* playbackTimer_ = nullptr;

    int currentFrame_ = 0;
    int totalFrames_ = 24;
    int fps_ = 24;
    bool playing_ = false;

    int scrollOffset_ = 0;
};
} // namespace fap
