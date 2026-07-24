#ifndef POINTCLOUDDIALOG_H
#define POINTCLOUDDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QStackedWidget>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QFormLayout>
#include <QVBoxLayout>

// -----------------------------------------------------------------------
// PointCloudCreateDialog — modal dialog for creating point clouds
// with method selection, dynamic parameter fields, seed control,
// and a live Apply button for preview without closing.
//
// Supports:
//   2D: Bounding Box (count, xMin, xMax, yMin, yMax)
//       Circle       (count, cx, cy, radius)
//   3D: Bounding Box (count, xMin, xMax, yMin, yMax, zMin, zMax)
//       Sphere       (count, cx, cy, cz, radius)
// -----------------------------------------------------------------------
class PointCloudCreateDialog : public QDialog
{
    Q_OBJECT

public:
    enum Mode { Create2D, Create3D };

    /// @param mode  Create2D or Create3D
    /// @param parent  Parent widget
    explicit PointCloudCreateDialog(Mode mode, QWidget* parent = nullptr);

    // ---- Result accessors (call after exec() returns Accepted) ----------

    /// Selected method index: 0 = Bounding Box, 1 = Circle/Sphere
    int methodIndex() const;

    /// Number of points (shared by all methods)
    int count() const;

    /// Random seed (0 = use random device)
    int seed() const;

    // Bounding box parameters (both 2D and 3D)
    float xMin() const;
    float xMax() const;
    float yMin() const;
    float yMax() const;
    float zMin() const;  // 3D only
    float zMax() const;  // 3D only

    // Circle / Sphere parameters
    float cx() const;
    float cy() const;
    float cz() const;     // 3D only
    float radius() const;

signals:
    /// Emitted when the Apply button is clicked (Create2D mode).
    void applyRequested2D(int count, int method, int seed,
                          float xMin, float xMax,
                          float yMin, float yMax,
                          float cx, float cy, float radius);

    /// Emitted when the Apply button is clicked (Create3D mode).
    void applyRequested3D(int count, int method, int seed,
                          float xMin, float xMax,
                          float yMin, float yMax,
                          float zMin, float zMax,
                          float cx, float cy, float cz,
                          float radius);

private slots:
    void onMethodChanged(int index);
    void onApply();

private:
    void buildUI();
    QWidget* createBBPanel();    // bounding box parameter panel
    QWidget* createCirclePanel();  // circle / sphere parameter panel

    Mode mode_;
    QComboBox* method_combo_ = nullptr;
    QStackedWidget* param_stack_ = nullptr;

    // Shared: count + seed
    QSpinBox* count_spin_ = nullptr;
    QSpinBox* seed_spin_ = nullptr;

    // Buttons
    QPushButton* apply_btn_ = nullptr;

    // Bounding box parameters
    QDoubleSpinBox* xmin_spin_ = nullptr;
    QDoubleSpinBox* xmax_spin_ = nullptr;
    QDoubleSpinBox* ymin_spin_ = nullptr;
    QDoubleSpinBox* ymax_spin_ = nullptr;
    QDoubleSpinBox* zmin_spin_ = nullptr;
    QDoubleSpinBox* zmax_spin_ = nullptr;

    // Circle / Sphere parameters
    QDoubleSpinBox* cx_spin_ = nullptr;
    QDoubleSpinBox* cy_spin_ = nullptr;
    QDoubleSpinBox* cz_spin_ = nullptr;
    QDoubleSpinBox* radius_spin_ = nullptr;
};

#endif // POINTCLOUDDIALOG_H
