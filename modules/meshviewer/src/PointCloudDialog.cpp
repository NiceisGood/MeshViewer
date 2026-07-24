#include "PointCloudDialog.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QFrame>

// =======================================================================
//  Construction
// =======================================================================

PointCloudCreateDialog::PointCloudCreateDialog(Mode mode, QWidget* parent)
    : QDialog(parent)
    , mode_(mode)
{
    setWindowTitle(mode == Create2D
        ? QStringLiteral("Create 2D Point Cloud")
        : QStringLiteral("Create 3D Point Cloud"));
    setMinimumWidth(380);

    buildUI();
}

// =======================================================================
//  UI construction
// =======================================================================

void PointCloudCreateDialog::buildUI()
{
    QVBoxLayout* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(12, 12, 12, 12);
    main_layout->setSpacing(8);

    // ── Method selector ──
    {
        QHBoxLayout* row = new QHBoxLayout;
        QLabel* lbl = new QLabel(QStringLiteral("Method:"));
        lbl->setFixedWidth(70);
        row->addWidget(lbl);

        method_combo_ = new QComboBox;
        if (mode_ == Create2D) {
            method_combo_->addItem(QStringLiteral("Bounding Box"));
            method_combo_->addItem(QStringLiteral("Circle"));
        } else {
            method_combo_->addItem(QStringLiteral("Bounding Box"));
            method_combo_->addItem(QStringLiteral("Sphere"));
        }
        method_combo_->setCurrentIndex(0);
        method_combo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        row->addWidget(method_combo_);
        main_layout->addLayout(row);
    }

    // ── Count (shared, always visible) ──
    {
        QHBoxLayout* row = new QHBoxLayout;
        QLabel* lbl = new QLabel(QStringLiteral("Count:"));
        lbl->setFixedWidth(70);
        row->addWidget(lbl);

        count_spin_ = new QSpinBox;
        count_spin_->setRange(50, 100000);
        count_spin_->setValue(mode_ == Create2D ? 200 : 500);
        count_spin_->setSingleStep(50);
        count_spin_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        row->addWidget(count_spin_);
        main_layout->addLayout(row);
    }

    // ── Parameter stack (method-specific, inside a frame) ──
    param_stack_ = new QStackedWidget;
    param_stack_->addWidget(createBBPanel());      // index 0
    param_stack_->addWidget(createCirclePanel());   // index 1
    param_stack_->setCurrentIndex(0);
    // Set consistent min height so switching doesn't resize the dialog
    param_stack_->setMinimumHeight(200);
    main_layout->addWidget(param_stack_);

    // ── Button box ──
    QDialogButtonBox* btn_box = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(btn_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btn_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    main_layout->addWidget(btn_box);

    // ── Connections ──
    connect(method_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PointCloudCreateDialog::onMethodChanged);
}

QWidget* PointCloudCreateDialog::createBBPanel()
{
    QWidget* panel = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);

    QGroupBox* box = new QGroupBox(QStringLiteral("Bounding Box Range"));
    QFormLayout* form = new QFormLayout(box);
    form->setContentsMargins(10, 16, 10, 10);
    form->setSpacing(6);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto make_spin = [](double val, QDoubleSpinBox*& ptr) {
        ptr = new QDoubleSpinBox;
        ptr->setRange(-1e6, 1e6);
        ptr->setValue(val);
        ptr->setSingleStep(0.1);
        ptr->setDecimals(4);
        ptr->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    };

    make_spin(-1.0, xmin_spin_);
    make_spin( 1.0, xmax_spin_);
    make_spin(-1.0, ymin_spin_);
    make_spin( 1.0, ymax_spin_);
    form->addRow(QStringLiteral("X Min:"), xmin_spin_);
    form->addRow(QStringLiteral("X Max:"), xmax_spin_);
    form->addRow(QStringLiteral("Y Min:"), ymin_spin_);
    form->addRow(QStringLiteral("Y Max:"), ymax_spin_);

    if (mode_ == Create3D) {
        make_spin(-1.0, zmin_spin_);
        make_spin( 1.0, zmax_spin_);
        form->addRow(QStringLiteral("Z Min:"), zmin_spin_);
        form->addRow(QStringLiteral("Z Max:"), zmax_spin_);
    }

    layout->addWidget(box);
    layout->addStretch(1);
    return panel;
}

QWidget* PointCloudCreateDialog::createCirclePanel()
{
    QWidget* panel = new QWidget;
    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);

    QString title = (mode_ == Create2D) ? QStringLiteral("Circle") : QStringLiteral("Sphere");
    QGroupBox* box = new QGroupBox(title);
    QFormLayout* form = new QFormLayout(box);
    form->setContentsMargins(10, 16, 10, 10);
    form->setSpacing(6);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto make_spin = [](double val, QDoubleSpinBox*& ptr) {
        ptr = new QDoubleSpinBox;
        ptr->setRange(-1e6, 1e6);
        ptr->setValue(val);
        ptr->setSingleStep(0.1);
        ptr->setDecimals(4);
        ptr->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    };

    make_spin(0.0, cx_spin_);
    make_spin(0.0, cy_spin_);
    form->addRow(QStringLiteral("Center X:"), cx_spin_);
    form->addRow(QStringLiteral("Center Y:"), cy_spin_);

    if (mode_ == Create3D) {
        make_spin(0.0, cz_spin_);
        form->addRow(QStringLiteral("Center Z:"), cz_spin_);
    }

    double default_radius = (mode_ == Create2D) ? 2.0 : 1.5;
    make_spin(default_radius, radius_spin_);
    radius_spin_->setRange(0.001, 1e6);
    form->addRow(QStringLiteral("Radius:"), radius_spin_);

    layout->addWidget(box);
    layout->addStretch(1);
    return panel;
}

// =======================================================================
//  Slots
// =======================================================================

void PointCloudCreateDialog::onMethodChanged(int index)
{
    param_stack_->setCurrentIndex(index);
}

// =======================================================================
//  Result accessors
// =======================================================================

int PointCloudCreateDialog::methodIndex() const
{
    return method_combo_->currentIndex();
}

int PointCloudCreateDialog::count() const
{
    return count_spin_->value();
}

float PointCloudCreateDialog::xMin() const { return static_cast<float>(xmin_spin_->value()); }
float PointCloudCreateDialog::xMax() const { return static_cast<float>(xmax_spin_->value()); }
float PointCloudCreateDialog::yMin() const { return static_cast<float>(ymin_spin_->value()); }
float PointCloudCreateDialog::yMax() const { return static_cast<float>(ymax_spin_->value()); }
float PointCloudCreateDialog::zMin() const { return zmin_spin_ ? static_cast<float>(zmin_spin_->value()) : 0.0f; }
float PointCloudCreateDialog::zMax() const { return zmax_spin_ ? static_cast<float>(zmax_spin_->value()) : 0.0f; }

float PointCloudCreateDialog::cx() const { return static_cast<float>(cx_spin_->value()); }
float PointCloudCreateDialog::cy() const { return static_cast<float>(cy_spin_->value()); }
float PointCloudCreateDialog::cz() const { return cz_spin_ ? static_cast<float>(cz_spin_->value()) : 0.0f; }
float PointCloudCreateDialog::radius() const { return static_cast<float>(radius_spin_->value()); }
