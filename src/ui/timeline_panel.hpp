#pragma once

#include <QWidget>
#include <QTimer>
#include <QImage>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QSlider>
#include <QScrollBar>
#include <vector>

class QPushButton;

namespace fap {

class TimelinePanel : public QWidget {
    Q_OBJECT
public:
    explicit TimelinePanel(QWidget* parent = nullptr);

    void setTotalFrames(int count);
    void setCurrentFrame(int frame);
    void setFPS(int fps);
    void setOnionSkin(int prev, int next);
    void setFrameThumbnail(int frame, const QImage& thumb);
    void purgeThumbnails();

    void togglePlayback();

    int currentFrame() const { return current_frame_; }

signals:
    void frameChanged(int frame);
    void frameCountChanged(int count);
    void frameInserted(int atFrame);
    void frameRemoved(int atFrame);
    void frameDuplicated(int sourceFrame, int newFrame);

protected:
    void timerEvent(QTimerEvent*) override;
    void paintEvent(QPaintEvent*) override;

private:
    static constexpr int kPanelW = 130;
    static constexpr int kCellW = 84;
    static constexpr int kCellH = 52;
    static constexpr int kHeaderH = 22;
    static constexpr int kThumbPad = 3;

    friend class GridView;
    class GridView : public QWidget {
    public:
        GridView(TimelinePanel* panel);
    protected:
        void paintEvent(QPaintEvent*) override;
        void mousePressEvent(QMouseEvent*) override;
    private:
        TimelinePanel* panel_;
    };

    int current_frame_ = 0;
    int total_frames_ = 24;
    int fps_ = 24;
    int onion_prev_ = 3;
    int onion_next_ = 1;
    int scroll_x_ = 0;
    bool playing_ = false;
    int play_timer_ = 0;

    QPushButton* play_btn_;
    QPushButton* prev_btn_;
    QPushButton* next_btn_;
    QPushButton* add_btn_;
    QPushButton* dup_btn_;
    QPushButton* del_btn_;
    QLabel* frame_label_;
    QLabel* fps_label_;
    QScrollBar* scroll_bar_;
    GridView* grid_view_;

    std::vector<QImage> thumbnails_;

    void addFrameAt(int frame);
    void removeFrameAt(int frame);
    void goToFirst();
    void goToLast();
};

} // namespace fap
