#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLTexture>
#include <QMatrix4x4>
#include <QColor>
#include <QImage>
#include <QPointF>
#include <memory>
#include <vector>
#include <unordered_map>

namespace fap {

class Document;
class BrushEngine;
class RasterLayer;
class Layer;
class GroupLayer;
using LayerUid = uint64_t;

class OpenGLCanvasWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
public:
    explicit OpenGLCanvasWidget(QWidget* parent = nullptr);
    ~OpenGLCanvasWidget() override;

    void setDocumentAndBrush(Document* doc, BrushEngine* brush);
    void setTool(int toolIdx);
    void setColor(const QColor& color);
    void setCurrentFrame(int frame);
    void setCurrentLayer(int layerIdx);
    void setTotalFrames(int count);
    void setOnionEnabled(bool enabled);
    void setOnionSettings(int prev, int next, int opacity);

    void fit();
    void flipH();
    void rotateCanvas();
    void toggleGrid();

    QImage grabFrameImage();

    static constexpr float kMinZoom = 0.02f;
    static constexpr float kMaxZoom = 64.0f;

signals:
    void canvasUpdated();
    void colorPicked(QColor color);
    void statusMessage(const QString& msg);
    void toolChangedByKey(int tool);
    void togglePlayPause();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void tabletEvent(QTabletEvent* event) override;

private:
    void compileShaders();
    void uploadLayerTexture(LayerUid uid, const RasterLayer& raster);
    void renderLayers();
    void renderOnionSkin();
    void renderGrid();

    QPointF screenToWorld(QPointF screen) const;
    void updateViewportMatrix();

    Document* document_ = nullptr;
    BrushEngine* brush_engine_ = nullptr;

    int current_frame_ = 0;
    int current_layer_idx_ = 0;
    int total_frames_ = 24;

    bool onion_enabled_ = true;
    int onion_prev_ = 3;
    int onion_next_ = 1;
    int onion_opacity_ = 35;

    bool grid_visible_ = false;
    bool flip_h_ = false;
    float rotation_deg_ = 0.0f;

    float zoom_ = 1.0f;
    QPointF pan_ = {0, 0};

    QColor brush_color_ = Qt::black;
    int active_tool_ = 0;
    QPointF pan_start_ = {0, 0};

    std::unique_ptr<QOpenGLShaderProgram> composite_shader_;
    std::unique_ptr<QOpenGLShaderProgram> grid_shader_;
    QMatrix4x4 viewport_matrix_;

    std::unordered_map<LayerUid, std::unique_ptr<QOpenGLTexture>> layer_textures_;
    std::unordered_map<LayerUid, uint64_t> texture_epochs_;
    std::unordered_map<LayerUid, int> texture_widths_;
    std::unordered_map<LayerUid, int> texture_heights_;
};

} // namespace fap
