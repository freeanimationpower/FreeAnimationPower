#include "property_editor.hpp"

#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QFrame>

namespace fap {

PropertyEditor::PropertyEditor(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    auto* canvasGroup = new QGroupBox("Canvas Properties");
    auto* canvasLayout = new QVBoxLayout(canvasGroup);
    canvasLayout->setSpacing(4);

    auto makeRow = [](const QString& label, QSpinBox*& spin, int min, int max, int def) {
        auto* row = new QHBoxLayout();
        auto* lbl = new QLabel(label);
        lbl->setStyleSheet("color: #c0c0c0; font-size: 11px;");
        lbl->setFixedWidth(44);
        spin = new QSpinBox();
        spin->setRange(min, max);
        spin->setValue(def);
        spin->setStyleSheet(
            "QSpinBox { background: #3e3e42; color: #e0e0e0; border: 1px solid #555; "
            "border-radius: 3px; padding: 4px; }"
            "QSpinBox:focus { border-color: #007acc; }");
        row->addWidget(lbl);
        row->addWidget(spin, 1);
        return row;
    };

    canvasLayout->addLayout(makeRow("Width", width_spin_, 1, 8192, 1920));
    canvasLayout->addLayout(makeRow("Height", height_spin_, 1, 8192, 1080));
    canvasLayout->addLayout(makeRow("FPS", fps_spin_, 1, 120, 24));

    auto* applyBtn = new QPushButton("Apply Changes");
    applyBtn->setStyleSheet(
        "QPushButton { background: #007acc; color: #fff; border: none; "
        "border-radius: 3px; padding: 8px; font-weight: bold; }"
        "QPushButton:hover { background: #e0883a; }"
        "QPushButton:pressed { background: #b06820; }");
    QObject::connect(applyBtn, &QPushButton::clicked, [this]() {
        emit canvasSizeChanged(width_spin_->value(), height_spin_->value());
        emit fpsChanged(fps_spin_->value());
    });
    canvasLayout->addWidget(applyBtn);

    layout->addWidget(canvasGroup);

    auto* infoFrame = new QFrame();
    infoFrame->setStyleSheet(
        "QFrame { background: #333337; border: 1px solid #3e3e42; border-radius: 3px; }");
    auto* infoLayout = new QVBoxLayout(infoFrame);
    infoLayout->setSpacing(2);

    auto infoLabel = [&](const QString& name) {
        auto* lbl = new QLabel(name);
        lbl->setStyleSheet("color: #888; font-size: 11px;");
        infoLayout->addWidget(lbl);
        return lbl;
    };

    memory_label_ = infoLabel("Memory: --");
    storage_label_ = infoLabel("Storage: --");

    layout->addWidget(infoFrame);
    layout->addStretch();
}

void PropertyEditor::setValues(int width, int height, int fps) {
    width_spin_->setValue(width);
    height_spin_->setValue(height);
    fps_spin_->setValue(fps);
}

} // namespace fap
#include "property_editor.moc"
