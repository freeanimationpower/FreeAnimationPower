#pragma once

#include <QWidget>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <vector>
#include <memory>

class QLineEdit;
class QPushButton;
class QSlider;
class QLabel;

namespace fap {

class AppState;
class TimelinePanelV2;

class AudioTrackWidget : public QWidget {
    Q_OBJECT
public:
    AudioTrackWidget(const QString& filepath, int index,
                     std::shared_ptr<AppState> appState,
                     TimelinePanelV2* panel,
                     QWidget* parent = nullptr);
    ~AudioTrackWidget() override;

    void positionHeader();
    void syncToFrame(int frame, int fps, bool playing);

    const QString& filepath() const { return filepath_; }
    int trackIndex() const { return index_; }
    void setTrackIndex(int i) { index_ = i; }
    QMediaPlayer* player() { return player_; }

signals:
    void moveUpRequested();
    void moveDownRequested();

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private:
    void decodeAudio();
    void drawWaveform(QPainter& p);
    void updateMuteStyle();

    QString filepath_;
    QString displayName_;
    int index_;
    std::vector<float> waveformPicks_;
    bool decoded_ = false;
    qint64 durationMs_ = 0;
    bool muted_ = false;
    bool decodeError_ = false;

    QLineEdit* nameEdit_ = nullptr;
    QPushButton* upBtn_ = nullptr;
    QPushButton* downBtn_ = nullptr;
    QPushButton* muteBtn_ = nullptr;
    QPushButton* delBtn_ = nullptr;
    QSlider* volumeSlider_ = nullptr;
    QLabel* volumeLabel_ = nullptr;
    QMediaPlayer* player_ = nullptr;
    QAudioOutput* audioOutput_ = nullptr;

    std::shared_ptr<AppState> appState_;
    TimelinePanelV2* panel_;
};

} // namespace fap
