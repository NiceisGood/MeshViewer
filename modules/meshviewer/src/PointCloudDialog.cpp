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
    setMinimumWidth(420);

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

    // ── Count ──
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

    // ── Seed ──
    {
        QHBoxLayout* row = new QHBoxLayout;
        QLabel* lbl = new QLabel(QStringLiteral("Seed:"));
        lbl->setFixedWidth(70);
        row->addWidget(lbl);

        seed_spin_ = new QSpinBox;
        seed_spin_->setRange(0, 999999);
        seed_spin_->setValue(0);
        seed_spin_->setSpecialValueText(QStringLiteral("Random"));
        seed_spin_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        row->addWidget(seed_spin_);

        QLabel* hint = new QLabel(QStringLiteral("(0 = random)"));
        hint->setStyleSheet(QStringLiteral("color: #888; font-size: 11px;"));
        row->addWidget(hint);

        main_layout->addLayout(row);
    }

    // ── Only Boundary ──
    {
        QHBoxLayout* row = new QHBoxLayout;
        only_boundary_check_ = new QCheckBox(QStringLiteral("Only on Boundary"));
        only_boundary_check_->setToolTip(
            QStringLiteral("Place points only on the outer boundary/surface instead of the interior"));
        row->addWidget(only_boundary_check_);
        row->addStretch(1);
        main_layout->addLayout(row);
    }

    // ── Parameter stack (method-specific) ──
    param_stack_ = new QStackedWidget;
    param_stack_->addWidget(createBBPanel());      // index 0
    param_stack_->addWidget(createCirclePanel());   // index 1
    param_stack_->setCurrentIndex(0);
    param_stack_->setMinimumHeight(220);
    main_layout->addWidget(param_stack_);

    // ── Action buttons ──
    {
        QHBoxLayout* btn_layout = new QHBoxLayout;
        btn_layout->setContentsMargins(0, 4, 0, 0);

        apply_btn_ = new QPushButton(QStringLiteral("&Apply"));
        apply_btn_->setToolTip(QStringLiteral("Generate/re-generate without closing the dialog"));
        apply_btn_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        btn_layout->addWidget(apply_btn_);

        btn_layout->addStretch(1);

        QDialogButtonBox* btn_box = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(btn_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(btn_box, &QDialogButtonBox::rejected, this, &QDialog::reject);
        btn_layout->addWidget(btn_box);

        main_layout->addLayout(btn_layout);
    }

    // ── Connections ──
    connect(method_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PointCloudCreateDialog::onMethodChanged);
    connect(apply_btn_, &QPushButton::clicked,
            this, &PointCloudCreateDialog::onApply);
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

    QString title = (mode_ == Create2D) ? QStringLiteral("Circle / Annulus")
                                        : QStringLiteral("Sphere / Shell");
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
    form->addRow(QStringLiteral("Outer Radius:"), radius_spin_);

    make_spin(0.0, radius_inner_spin_);
    radius_inner_spin_->setRange(0.0, 1e6);
    radius_inner_spin_->setSpecialValueText(QStringLiteral("None"));
    radius_inner_spin_->setToolTip(
        QStringLiteral("Inner radius for annular region (0 = full disc/sphere)"));
    form->addRow(QStringLiteral("Inner Radius:"), radius_inner_spin_);

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

void PointCloudCreateDialog::onApply()
{
    if (mode_ == Create2D) {
        emit applyRequested2D(
            count(), methodIndex(), seed(), onlyBoundary(),
            xMin(), xMax(), yMin(), yMax(),
            cx(), cy(),
            radius(), radiusInner());
    } else {
        emit applyRequested3D(
            count(), methodIndex(), seed(), onlyBoundary(),
            xMin(), xMax(), yMin(), yMax(), zMin(), zMax(),
            cx(), cy(), cz(),
            radius(), radiusInner());
    }
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

int PointCloudCreateDialog::seed() const
{
    return seed_spin_->value();
}

bool PointCloudCreateDialog::onlyBoundary() const
{
    return only_boundary_check_->isChecked();
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
float PointCloudCreateDialog::radiusInner() const { return static_cast<float>(radius_inner_spin_->value()); }
