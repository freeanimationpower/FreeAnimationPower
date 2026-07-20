#pragma once

#include <QtWidgets/QWidget>
#include <QtWidgets/QSlider>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLineEdit>
#include <QtCore/QString>
#include <memory>

#include "engine/animation/video_decoder.hpp"

namespace fap {

class AppState;
class TimelinePanelV2;

class VideoTrackWidget : public QWidget {
    Q_OBJECT
public:
    VideoTrackWidget(const QString& filepath, int trackIndex,
                     std::shared_ptr<AppState> state,
                     TimelinePanelV2* panel, QWidget* parent,
                     const VideoMetadata& meta);
    ~VideoTrackWidget() override;

    const QString& filepath() const { return filepath_; }
    const QString& displayName() const { return displayName_; }
    bool isMuted() const { return muted_; }
    int volume() const { return volume_; }
    float opacity() const { return opacity_; }
    int trackIndex() const { return trackIndex_; }
    int totalFrames() const { return totalFrames_; }
    int videoWidth() const { return videoWidth_; }
    int videoHeight() const { return videoHeight_; }
    double videoFps() const { return videoFps_; }

    void setMuted(bool m);
    void setVolume(int v);
    void setOpacity(float o);
    void setTrackIndex(int i) { trackIndex_ = i; }

    VideoFrameCache& frameCache() { return frameCache_; }
    QImage currentFrame(int frameIdx);

    void positionHeader();

signals:
    void moveUpRequested();
    void moveDownRequested();
    void opacityChanged();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    struct ThumbnailEntry {
        int frameIdx;
        QImage image;
    };

    QString filepath_;
    QString displayName_;
    int trackIndex_ = 0;
    bool muted_ = false;
    int volume_ = 80;
    float opacity_ = 1.0f;
    int videoWidth_ = 0, videoHeight_ = 0;
    double videoFps_ = 24.0;
    int totalFrames_ = 0;

    VideoFrameCache frameCache_;
    std::vector<ThumbnailEntry> thumbnails_;
    void decodeThumbnails();

    QLineEdit* nameEdit_ = nullptr;
    QPushButton* muteBtn_ = nullptr;
    QPushButton* upBtn_ = nullptr;
    QPushButton* downBtn_ = nullptr;
    QPushButton* delBtn_ = nullptr;
    QSlider* opacitySlider_ = nullptr;
    QLabel* opacityLabel_ = nullptr;
    QSlider* volumeSlider_ = nullptr;
    QLabel* volumeLabel_ = nullptr;

    std::shared_ptr<AppState> appState_;
    TimelinePanelV2* panel_ = nullptr;

    static constexpr int kHeaderLeft = 0;
    void drawThumbnailStrip(QPainter& p);
};

} // namespace fap
