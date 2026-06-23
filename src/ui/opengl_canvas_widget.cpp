#include "ui/opengl_canvas_widget.hpp"

#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QTabletEvent>
#include <QtMath>

#include "core/document.hpp"
#include "core/layer.hpp"
#include "core/types.hpp"
#include "engine/brush/brush_engine.hpp"

namespace fap {

static const char* kCompositeVertSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
uniform mat4 uTransform;
uniform mat4 uLayerTransform;
out vec2 vTexCoord;
void main() {
    gl_Position = uTransform * uLayerTransform * vec4(aPos, 0.0, 1.0);
    vTexCoord = aTexCoord;
}
)";

static const char* kCompositeFragSrc = R"(
#version 330 core
in vec2 vTexCoord;
uniform sampler2D uTexture;
uniform float uOpacity;
uniform int uBlendMode;
uniform vec3 uTint;
uniform float uTintStrength;
out vec4 fragColor;

float blendChannel(float src, float dst, int mode) {
    switch (mode) {
        case 0:  return src;
        case 1:  return src * dst;
        case 2:  return 1.0 - (1.0 - src) * (1.0 - dst);
        case 3:  return dst < 0.5 ? 2.0*src*dst : 1.0-2.0*(1.0-src)*(1.0-dst);
        case 4:  return min(1.0, src + dst);
        case 5:  return max(0.0, dst - src);
        case 6:  return min(src, dst);
        case 7:  return max(src, dst);
        case 8:  return src > 0.0 ? 1.0 - min(1.0,(1.0-dst)/src) : 0.0;
        case 9:  return src < 1.0 ? min(1.0,dst/(1.0-src)) : 1.0;
        case 10: {
            if (src <= 0.5) return dst-(1.0-2.0*src)*dst*(1.0-dst);
            if (dst <= 0.25) return dst+(2.0*src-1.0)*(((16.0*dst-12.0)*dst+4.0)*dst-dst);
            return dst+(2.0*src-1.0)*(sqrt(dst)-dst);
        }
        case 11: return src < 0.5 ? 2.0*src*dst : 1.0-2.0*(1.0-src)*(1.0-dst);
        default: return src;
    }
}

void main() {
    vec4 texColor = texture(uTexture, vTexCoord);
    if (texColor.a < 0.001) discard;
    vec3 src = texColor.rgb;
    float srcAlpha = texColor.a * uOpacity;
    fragColor.rgb = mix(src, src * uTint, uTintStrength);
    fragColor.a = srcAlpha;
}
)";

static const char* kGridVertSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
uniform mat4 uTransform;
void main() {
    gl_Position = uTransform * vec4(aPos, 0.0, 1.0);
}
)";

static const char* kGridFragSrc = R"(
#version 330 core
out vec4 fragColor;
void main() {
    fragColor = vec4(0.3, 0.3, 0.35, 0.3);
}
)";

OpenGLCanvasWidget::OpenGLCanvasWidget(QWidget* parent)
    : QOpenGLWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setTabletTracking(true);
}

OpenGLCanvasWidget::~OpenGLCanvasWidget() {
    makeCurrent();
    layer_textures_.clear();
    texture_epochs_.clear();
    composite_shader_.reset();
    grid_shader_.reset();
    doneCurrent();
}

void OpenGLCanvasWidget::setDocumentAndBrush(Document* doc, BrushEngine* brush) {
    document_ = doc;
    brush_engine_ = brush;
    fit();
    update();
}

void OpenGLCanvasWidget::setTool(int toolIdx) { active_tool_ = toolIdx; }
void OpenGLCanvasWidget::setColor(const QColor& color) { brush_color_ = color; }

void OpenGLCanvasWidget::setCurrentFrame(int frame) {
    current_frame_ = frame;
    update();
}

void OpenGLCanvasWidget::setCurrentLayer(int layerIdx) {
    current_layer_idx_ = layerIdx;
    update();
}

void OpenGLCanvasWidget::setTotalFrames(int count) {
    total_frames_ = count;
}

void OpenGLCanvasWidget::setOnionEnabled(bool enabled) {
    onion_enabled_ = enabled;
    update();
}

void OpenGLCanvasWidget::setOnionSettings(int prev, int next, int opacity) {
    onion_prev_ = prev;
    onion_next_ = next;
    onion_opacity_ = opacity;
    update();
}

void OpenGLCanvasWidget::fit() {
    if (!document_) return;
    zoom_ = 1.0f;
    pan_ = QPointF(0, 0);
    rotation_deg_ = 0.0f;
    flip_h_ = false;
    update();
}

void OpenGLCanvasWidget::flipH() {
    flip_h_ = !flip_h_;
    update();
}

void OpenGLCanvasWidget::rotateCanvas() {
    rotation_deg_ += 15.0f;
    if (rotation_deg_ >= 360.0f) rotation_deg_ -= 360.0f;
    update();
}

void OpenGLCanvasWidget::toggleGrid() {
    grid_visible_ = !grid_visible_;
    update();
}

QImage OpenGLCanvasWidget::grabFrameImage() {
    return grab().toImage();
}

void OpenGLCanvasWidget::initializeGL() {
    initializeOpenGLFunctions();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.059f, 0.067f, 0.082f, 1.0f);
    compileShaders();
}

void OpenGLCanvasWidget::compileShaders() {
    composite_shader_ = std::make_unique<QOpenGLShaderProgram>();
    composite_shader_->addShaderFromSourceCode(QOpenGLShader::Vertex, kCompositeVertSrc);
    composite_shader_->addShaderFromSourceCode(QOpenGLShader::Fragment, kCompositeFragSrc);
    composite_shader_->link();
    composite_shader_->bind();
    composite_shader_->release();

    grid_shader_ = std::make_unique<QOpenGLShaderProgram>();
    grid_shader_->addShaderFromSourceCode(QOpenGLShader::Vertex, kGridVertSrc);
    grid_shader_->addShaderFromSourceCode(QOpenGLShader::Fragment, kGridFragSrc);
    grid_shader_->link();
}

void OpenGLCanvasWidget::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
}

void OpenGLCanvasWidget::updateViewportMatrix() {
    viewport_matrix_.setToIdentity();

    float canvasW = document_ ? static_cast<float>(document_->width()) : 1920.0f;
    float canvasH = document_ ? static_cast<float>(document_->height()) : 1080.0f;
    float aspectWidget = static_cast<float>(width()) / static_cast<float>(std::max(1, height()));
    float aspectCanvas = canvasW / canvasH;

    float sx, sy;
    if (aspectWidget > aspectCanvas) {
        sy = 2.0f * zoom_;
        sx = sy * aspectCanvas / aspectWidget;
    } else {
        sx = 2.0f * zoom_;
        sy = sx * aspectWidget / aspectCanvas;
    }

    viewport_matrix_.translate(-1.0f, -1.0f, 0.0f);
    viewport_matrix_.scale(sx, sy, 1.0f);

    if (flip_h_) {
        viewport_matrix_.scale(-1.0f / canvasW, 1.0f / canvasH, 1.0f);
    } else {
        viewport_matrix_.scale(1.0f / canvasW, 1.0f / canvasH, 1.0f);
    }

    if (std::abs(rotation_deg_) > 0.001f) {
        viewport_matrix_.translate(canvasW * 0.5f, canvasH * 0.5f, 0.0f);
        viewport_matrix_.rotate(rotation_deg_, 0.0f, 0.0f, 1.0f);
        viewport_matrix_.translate(-canvasW * 0.5f, -canvasH * 0.5f, 0.0f);
    }

    viewport_matrix_.translate(static_cast<float>(pan_.x()), static_cast<float>(pan_.y()), 0.0f);
}

void OpenGLCanvasWidget::uploadLayerTexture(LayerUid uid, const RasterLayer& raster) {
    auto it = layer_textures_.find(uid);
    if (it == layer_textures_.end()) {
        auto tex = std::make_unique<QOpenGLTexture>(QOpenGLTexture::Target2D);
        tex->setMinificationFilter(QOpenGLTexture::Linear);
        tex->setMagnificationFilter(QOpenGLTexture::Linear);
        tex->setWrapMode(QOpenGLTexture::ClampToEdge);
        it = layer_textures_.emplace(uid, std::move(tex)).first;
        texture_epochs_[uid] = 0;
    }

    auto& tex = it->second;
    uint64_t currentEpoch = raster.bufferEpoch();

    if (texture_epochs_[uid] != currentEpoch ||
        texture_widths_[uid] != raster.width() ||
        texture_heights_[uid] != raster.height()) {
        tex->destroy();
        tex->setFormat(QOpenGLTexture::RGBA8_UNorm);
        tex->setSize(raster.width(), raster.height());
        tex->allocateStorage();
        tex->setData(QOpenGLTexture::RGBA, QOpenGLTexture::UInt8,
                     raster.pixelData());
        texture_epochs_[uid] = currentEpoch;
        texture_widths_[uid] = raster.width();
        texture_heights_[uid] = raster.height();
    }
}

void OpenGLCanvasWidget::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT);
    if (!document_) return;
    updateViewportMatrix();
    renderLayers();
    renderOnionSkin();
    if (grid_visible_) renderGrid();
    emit canvasUpdated();
}

void OpenGLCanvasWidget::renderLayers() {
    if (!composite_shader_ || !composite_shader_->isLinked()) return;

    const GroupLayer* root = document_->peekRootLayerForFrame(current_frame_);
    if (!root) return;

    composite_shader_->bind();

    float quadVertices[] = {
        0.0f, 0.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        1.0f, 1.0f, 1.0f, 0.0f,
    };

    QOpenGLBuffer vbo(QOpenGLBuffer::VertexBuffer);
    vbo.create();
    vbo.bind();
    vbo.allocate(quadVertices, sizeof(quadVertices));

    QOpenGLVertexArrayObject vao;
    vao.create();
    vao.bind();

    composite_shader_->enableAttributeArray(0);
    composite_shader_->setAttributeBuffer(0, GL_FLOAT, 0, 2, 4 * sizeof(float));
    composite_shader_->enableAttributeArray(1);
    composite_shader_->setAttributeBuffer(1, GL_FLOAT, 2 * sizeof(float), 2, 4 * sizeof(float));

    composite_shader_->setUniformValue("uTransform", viewport_matrix_);
    composite_shader_->setUniformValue("uTint", QVector3D(1.0f, 1.0f, 1.0f));
    composite_shader_->setUniformValue("uTintStrength", 0.0f);

    int layerCount = static_cast<int>(root->layerCount());
    for (int i = 0; i < layerCount; ++i) {
        const Layer* layer = root->layerAt(i);
        if (!layer || !layer->visible()) continue;
        if (layer->type() != LayerType::Raster) continue;

        const auto* raster = static_cast<const RasterLayer*>(layer);
        uploadLayerTexture(layer->uid(), *raster);

        QMatrix4x4 layerXform;
        layerXform.setToIdentity();
        layerXform.translate(static_cast<float>(raster->originX()),
                             static_cast<float>(raster->originY()), 0.0f);
        layerXform.scale(static_cast<float>(raster->width()),
                         static_cast<float>(raster->height()), 1.0f);
        composite_shader_->setUniformValue("uLayerTransform", layerXform);
        composite_shader_->setUniformValue("uOpacity", raster->opacity());
        composite_shader_->setUniformValue("uBlendMode", static_cast<int>(raster->blendMode()));
        composite_shader_->setUniformValue("uTexture", 0);

        auto it = layer_textures_.find(layer->uid());
        if (it != layer_textures_.end()) {
            it->second->bind(0);
        }

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    vao.release();
    vbo.release();
    vbo.destroy();
    vao.destroy();
    composite_shader_->release();
}

void OpenGLCanvasWidget::renderOnionSkin() {
    if (!onion_enabled_ || !composite_shader_ || !composite_shader_->isLinked()) return;

    composite_shader_->bind();

    float quadVertices[] = {
        0.0f, 0.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        1.0f, 1.0f, 1.0f, 0.0f,
    };

    QOpenGLBuffer vbo(QOpenGLBuffer::VertexBuffer);
    vbo.create();
    vbo.bind();
    vbo.allocate(quadVertices, sizeof(quadVertices));

    QOpenGLVertexArrayObject vao;
    vao.create();
    vao.bind();

    composite_shader_->enableAttributeArray(0);
    composite_shader_->setAttributeBuffer(0, GL_FLOAT, 0, 2, 4 * sizeof(float));
    composite_shader_->enableAttributeArray(1);
    composite_shader_->setAttributeBuffer(1, GL_FLOAT, 2 * sizeof(float), 2, 4 * sizeof(float));

    composite_shader_->setUniformValue("uTransform", viewport_matrix_);
    composite_shader_->setUniformValue("uBlendMode", 0);
    composite_shader_->setUniformValue("uTexture", 0);

    float baseOpacity = onion_opacity_ / 100.0f;

    auto renderGhost = [this](int frameIdx, float alpha, float r, float g, float b) {
        const GroupLayer* root = document_->peekRootLayerForFrame(frameIdx);
        if (!root) return;

        int count = static_cast<int>(root->layerCount());
        for (int i = 0; i < count; ++i) {
            const Layer* layer = root->layerAt(i);
            if (!layer || !layer->visible()) continue;
            if (layer->type() != LayerType::Raster) continue;

            const auto* raster = static_cast<const RasterLayer*>(layer);
            uploadLayerTexture(layer->uid(), *raster);

            QMatrix4x4 layerXform;
            layerXform.setToIdentity();
            layerXform.translate(static_cast<float>(raster->originX()),
                                 static_cast<float>(raster->originY()), 0.0f);
            layerXform.scale(static_cast<float>(raster->width()),
                             static_cast<float>(raster->height()), 1.0f);
            composite_shader_->setUniformValue("uLayerTransform", layerXform);
            composite_shader_->setUniformValue("uOpacity", alpha);
            composite_shader_->setUniformValue("uTint", QVector3D(r, g, b));
            composite_shader_->setUniformValue("uTintStrength", 0.6f);

            auto it = layer_textures_.find(layer->uid());
            if (it != layer_textures_.end()) {
                it->second->bind(0);
            }

            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
    };

    for (int i = 0; i < onion_prev_; ++i) {
        int frame = current_frame_ - (i + 1);
        if (frame < 0) continue;
        float dist = (i + 1.0f) / (onion_prev_ + 1.0f);
        renderGhost(frame, baseOpacity * (1.0f - dist), 1.0f, 0.15f, 0.15f);
    }

    for (int i = 0; i < onion_next_; ++i) {
        int frame = current_frame_ + (i + 1);
        if (frame >= total_frames_) continue;
        float dist = (i + 1.0f) / (onion_next_ + 1.0f);
        renderGhost(frame, baseOpacity * (1.0f - dist), 0.15f, 0.4f, 1.0f);
    }

    vao.release();
    vbo.release();
    vbo.destroy();
    vao.destroy();
    composite_shader_->release();
}

void OpenGLCanvasWidget::renderGrid() {
    if (!grid_shader_ || !grid_shader_->isLinked() || !document_) return;

    grid_shader_->bind();
    grid_shader_->setUniformValue("uTransform", viewport_matrix_);

    int w = document_->width();
    int h = document_->height();
    int step = std::max(32, std::min(w, h) / 20);

    std::vector<float> lines;
    for (int x = 0; x <= w; x += step) {
        lines.push_back(static_cast<float>(x)); lines.push_back(0.0f);
        lines.push_back(static_cast<float>(x)); lines.push_back(static_cast<float>(h));
    }
    for (int y = 0; y <= h; y += step) {
        lines.push_back(0.0f); lines.push_back(static_cast<float>(y));
        lines.push_back(static_cast<float>(w)); lines.push_back(static_cast<float>(y));
    }

    QOpenGLBuffer vbo(QOpenGLBuffer::VertexBuffer);
    vbo.create();
    vbo.bind();
    vbo.allocate(lines.data(), static_cast<int>(lines.size() * sizeof(float)));

    QOpenGLVertexArrayObject vao;
    vao.create();
    vao.bind();
    grid_shader_->enableAttributeArray(0);
    grid_shader_->setAttributeBuffer(0, GL_FLOAT, 0, 2, 0);

    glLineWidth(1.0f);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(lines.size() / 2));

    vao.release();
    vbo.release();
    vbo.destroy();
    vao.destroy();
    grid_shader_->release();
}

QPointF OpenGLCanvasWidget::screenToWorld(QPointF screen) const {
    float ndcX = (2.0f * static_cast<float>(screen.x()) / static_cast<float>(width())) - 1.0f;
    float ndcY = 1.0f - (2.0f * static_cast<float>(screen.y()) / static_cast<float>(height()));

    bool invertible = false;
    QMatrix4x4 inv = viewport_matrix_.inverted(&invertible);
    if (!invertible) return screen;

    QVector4D w = inv * QVector4D(ndcX, ndcY, 0.0f, 1.0f);
    float canvasW = document_ ? static_cast<float>(document_->width()) : 1920.0f;
    float canvasH = document_ ? static_cast<float>(document_->height()) : 1080.0f;
    return QPointF(w.x() * canvasW, w.y() * canvasH);
}

void OpenGLCanvasWidget::mousePressEvent(QMouseEvent* event) {
    if (active_tool_ == 10) {
        setCursor(Qt::ClosedHandCursor);
        pan_start_ = event->position();
        return;
    }
    QPointF world = screenToWorld(event->position());
    emit statusMessage(QString("Press at %.0f, %.0f").arg(world.x()).arg(world.y()));
    update();
}

void OpenGLCanvasWidget::mouseMoveEvent(QMouseEvent* event) {
    if (active_tool_ == 10 && (event->buttons() & Qt::LeftButton)) {
        QPointF delta = event->position() - pan_start_;
        pan_ += delta / zoom_;
        pan_start_ = event->position();
        update();
        return;
    }
}

void OpenGLCanvasWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (active_tool_ == 10) setCursor(Qt::OpenHandCursor);
    update();
}

void OpenGLCanvasWidget::wheelEvent(QWheelEvent* event) {
    float factor = event->angleDelta().y() > 0 ? 1.12f : 1.0f / 1.12f;
    zoom_ = std::clamp(zoom_ * factor, kMinZoom, kMaxZoom);
    update();
}

void OpenGLCanvasWidget::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
    case Qt::Key_Space: emit togglePlayPause(); break;
    case Qt::Key_B: emit toolChangedByKey(0); break;
    case Qt::Key_E: emit toolChangedByKey(1); break;
    case Qt::Key_I: emit toolChangedByKey(2); break;
    case Qt::Key_G: emit toolChangedByKey(3); break;
    case Qt::Key_T: emit toolChangedByKey(4); break;
    case Qt::Key_L: emit toolChangedByKey(5); break;
    case Qt::Key_U: emit toolChangedByKey(6); break;
    case Qt::Key_Y: emit toolChangedByKey(7); break;
    case Qt::Key_M: emit toolChangedByKey(8); break;
    case Qt::Key_S: emit toolChangedByKey(9); break;
    case Qt::Key_H: emit toolChangedByKey(10); break;
    case Qt::Key_F: fit(); break;
    case Qt::Key_R: rotateCanvas(); break;
    case Qt::Key_Apostrophe: toggleGrid(); break;
    default: QOpenGLWidget::keyPressEvent(event);
    }
}

void OpenGLCanvasWidget::tabletEvent(QTabletEvent* event) {
    event->accept();
    switch (event->type()) {
    case QEvent::TabletPress:
        if (active_tool_ == 1) brush_color_ = Qt::white;
        update();
        break;
    case QEvent::TabletRelease:
        update();
        break;
    default:
        break;
    }
}

} // namespace fap
