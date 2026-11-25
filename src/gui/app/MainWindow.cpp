// XFOIL.next
//
// Copyright (C) 2026 Olaf Frommann
// https://github.com/of33/XFOIL.next
//
// XFOIL.next is based on XFOIL 6.99 by Mark Drela and
// Harold Youngren.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//
// SPDX-License-Identifier: GPL-3.0-or-later

// File: src/gui/app/MainWindow.cpp
// Thin standalone shell: one XFoilWidget, app actions, dialogs, settings,
// recent files and deferred close. No session or solver logic.

#include "app/MainWindow.h"

#include <filesystem>
#include <string>
#include <utility>

#include <QAction>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QTimer>

#include "app/GuiFileAdapters.h"
#include "app/ProductInfo.h"
#include "xfoil/PolarData.h"
#include "qcustomplot.h"
#include "xfoil/gui/XFoilWidget.h"

namespace xfoil::gui::app {
namespace {

QString ensureSuffix(const QString& path, const QString& suffix) {
  return path.endsWith(suffix, Qt::CaseInsensitive) ? path : path + suffix;
}

QString safeAirfoilBaseName(const QString& name) {
  QString base = QFileInfo(name).fileName();
  if (base.isEmpty() || base == QStringLiteral(".") ||
      base == QStringLiteral("..")) {
    return QStringLiteral("airfoil");
  }
  return base;
}

QString productName() {
  return QString::fromLatin1(xfoil::app::product::name.data(),
                             static_cast<int>(
                                 xfoil::app::product::name.size()));
}

QString productVersion() {
  return QString::fromLatin1(
      xfoil::app::product::version.data(),
      static_cast<int>(xfoil::app::product::version.size()));
}

QString productLicense() {
  return QString::fromLatin1(
      xfoil::app::product::licenseId.data(),
      static_cast<int>(xfoil::app::product::licenseId.size()));
}

QString productUrl() {
  return QString::fromLatin1(
      xfoil::app::product::projectUrl.data(),
      static_cast<int>(xfoil::app::product::projectUrl.size()));
}

QString openFilter() {
  return QStringLiteral("Airfoil files (*.dat *.DAT);;All files (*)");
}

QString airfoilSaveFilter() {
  return QStringLiteral("Airfoil files (*.dat);;All files (*)");
}

QString dxfSaveFilter() {
  return QStringLiteral("DXF R12 (*.dxf);;All files (*)");
}

QString polarOpenFilter() {
  return QStringLiteral("Polar files (*.pol);;All files (*)");
}

QString polarSaveFilter() {
  return QStringLiteral("Polar files (*.pol);;All files (*)");
}

QString cpSaveFilter() {
  return QStringLiteral("Pressure text (*.txt);;All files (*)");
}

QString pngSaveFilter() {
  return QStringLiteral("PNG image (*.png);;All files (*)");
}

QString pdfSaveFilter() {
  return QStringLiteral("PDF document (*.pdf);;All files (*)");
}

QString svgSaveFilter() {
  return QStringLiteral("SVG vector image (*.svg);;All files (*)");
}

std::filesystem::path toPath(const QString& path) {
  return std::filesystem::path(path.toStdString());
}

}  // namespace

MainWindow::MainWindow(QWidget* parent)
    : MainWindow(Dependencies{}, parent) {}

MainWindow::MainWindow(Dependencies dependencies, QWidget* parent)
    : QMainWindow(parent), dependencies_(std::move(dependencies)) {
  if (!dependencies_.chooseFile) {
    dependencies_.chooseFile =
        [this](FileDialogMode mode, const QString& title,
               const QString& filter, const QString& suggested)
        -> std::optional<QString> {
      QString selected;
      if (mode == FileDialogMode::open) {
        selected = QFileDialog::getOpenFileName(this, title, suggested,
                                                 filter);
      } else {
        selected = QFileDialog::getSaveFileName(this, title, suggested,
                                                 filter);
      }
      if (selected.isEmpty()) {
        return std::nullopt;
      }
      return selected;
    };
  }
  if (!dependencies_.askText) {
    dependencies_.askText = [this](const QString& title,
                                   const QString& label,
                                   const QString& initial)
        -> std::optional<QString> {
      bool accepted = false;
      const QString value = QInputDialog::getText(
          this, title, label, QLineEdit::Normal, initial, &accepted);
      if (!accepted) {
        return std::nullopt;
      }
      return value;
    };
  }
  if (!dependencies_.askInt) {
    dependencies_.askInt = [this](const QString& title,
                                  const QString& label, int initial,
                                  int minimum, int maximum)
        -> std::optional<int> {
      bool accepted = false;
      const int value = QInputDialog::getInt(
          this, title, label, initial, minimum, maximum, 1, &accepted);
      if (!accepted) {
        return std::nullopt;
      }
      return value;
    };
  }
  if (!dependencies_.confirm) {
    dependencies_.confirm = [this](const QString& title,
                                   const QString& text) {
      return QMessageBox::question(
                 this, title, text,
                 QMessageBox::Yes | QMessageBox::No,
                 QMessageBox::No) == QMessageBox::Yes;
    };
  }
  if (!dependencies_.showError) {
    dependencies_.showError = [this](const QString& title,
                                     const QString& text) {
      QMessageBox::critical(this, title, text);
    };
  }
  if (!dependencies_.showInfo) {
    dependencies_.showInfo = [this](const QString& title,
                                    const QString& text) {
      QMessageBox reference(QMessageBox::Information, title, text,
                            QMessageBox::Ok, this);
      reference.ensurePolished();
      const int baseWidth = reference.sizeHint().width();
      QDialog dialog(this);
      dialog.setObjectName(QStringLiteral("aboutDialog"));
      dialog.setWindowTitle(title);
      auto* layout = new QVBoxLayout(&dialog);
      auto* label = new QLabel(text, &dialog);
      label->setObjectName(QStringLiteral("aboutTextLabel"));
      label->setTextInteractionFlags(Qt::TextBrowserInteraction);
      layout->addWidget(label);
      auto* buttons =
          new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
      QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog,
                       &QDialog::accept);
      layout->addWidget(buttons);
      dialog.ensurePolished();
      dialog.setProperty("aboutBaseWidth", baseWidth);
      const int targetWidth =
          qRound(static_cast<double>(baseWidth) * 1.2);
      dialog.setMinimumWidth(targetWidth);
      dialog.resize(targetWidth, dialog.sizeHint().height());
      dialog.exec();
    };
  }

  if (dependencies_.settingsFile.isEmpty()) {
    settings_ = std::make_unique<QSettings>();
  } else {
    settings_ = std::make_unique<QSettings>(dependencies_.settingsFile,
                                             QSettings::IniFormat);
  }

  setWindowTitle(productName() + QStringLiteral(" ") + productVersion());
  setMinimumSize(1280, 720);
  resize(1280, 720);
  widget_ = new XFoilWidget(this);
  setCentralWidget(widget_);

  connect(widget_, &XFoilWidget::airfoilImportRequested, this,
          [this]() { openFileDialog(); });

  createActions();
  createMenus();
  statusBar()->showMessage(QStringLiteral("Ready"));

  connect(widget_, &XFoilWidget::geometryChanged, this,
          [this]() { updateAppActions(); });
  connect(widget_, &XFoilWidget::panelingChanged, this,
          [this]() { updateAppActions(); });
  connect(widget_, &XFoilWidget::operatingPointReady, this,
          [this](const OperatingPointData&) { updateAppActions(); });
  connect(widget_, &XFoilWidget::sequenceReady, this,
          [this](const DetailedSequenceResult&) { updateAppActions(); });
  connect(widget_, &XFoilWidget::comparisonPolarsChanged, this,
          [this]() { updateAppActions(); });
  connect(widget_, &XFoilWidget::activePlotChanged, this,
          [this]() { updateAppActions(); });
  connect(widget_, &XFoilWidget::resultSelectionChanged, this,
          [this]() { updateAppActions(); });
  connect(widget_, &XFoilWidget::stateChanged, this,
          [this](XFoilGuiState) {
            updateAppActions();
            if (pendingClose_) {
              schedulePendingCloseCheck();
            }
          });
  connect(widget_, &XFoilWidget::analysisCancelled, this, [this]() {
    if (pendingClose_) {
      schedulePendingCloseCheck();
    }
  });
  connect(widget_, &XFoilWidget::analysisFailed, this,
          [this](const Error&) {
            if (pendingClose_) {
              schedulePendingCloseCheck();
            }
          });

  loadSettings();
  updateAppActions();
}

MainWindow::~MainWindow() {
  saveSettings();
}

void MainWindow::createActions() {
  loadAirfoilAction_ = new QAction(QStringLiteral("&Load Airfoil..."), this);
  loadAirfoilAction_->setObjectName(QStringLiteral("loadAirfoilAction"));
  loadAirfoilAction_->setShortcut(QKeySequence::Open);
  connect(loadAirfoilAction_, &QAction::triggered, this,
          [this](bool) { openFileDialog(); });

  saveAirfoilAction_ = new QAction(QStringLiteral("&Save Airfoil..."), this);
  saveAirfoilAction_->setObjectName(QStringLiteral("saveAirfoilAction"));
  saveAirfoilAction_->setShortcut(QKeySequence::Save);
  connect(saveAirfoilAction_, &QAction::triggered, this,
          [this](bool) { saveAirfoilDialog(); });

  exportDxfAction_ = new QAction(QStringLiteral("Export &DXF..."), this);
  exportDxfAction_->setObjectName(QStringLiteral("exportDxfAction"));
  connect(exportDxfAction_, &QAction::triggered, this,
          [this](bool) { exportDxfDialog(); });

  importPolarAction_ = new QAction(QStringLiteral("&Import Polar..."), this);
  importPolarAction_->setObjectName(QStringLiteral("importPolarAction"));
  connect(importPolarAction_, &QAction::triggered, this,
          [this](bool) { importPolarDialog(); });

  exportCurrentPolarAction_ =
      new QAction(QStringLiteral("Export &Current Polar..."), this);
  exportCurrentPolarAction_->setObjectName(
      QStringLiteral("exportCurrentPolarAction"));
  connect(exportCurrentPolarAction_, &QAction::triggered, this,
          [this](bool) { exportCurrentPolarDialog(); });

  exportComparisonPolarAction_ =
      new QAction(QStringLiteral("Export &Comparison Polar..."), this);
  exportComparisonPolarAction_->setObjectName(
      QStringLiteral("exportComparisonPolarAction"));
  connect(exportComparisonPolarAction_, &QAction::triggered, this,
          [this](bool) { exportComparisonPolarDialog(); });

  exportCpAction_ = new QAction(QStringLiteral("Export &Pressure (Cp)..."), this);
  exportCpAction_->setObjectName(QStringLiteral("exportCpAction"));
  connect(exportCpAction_, &QAction::triggered, this,
          [this](bool) { exportCpDialog(); });

  exportPlotPngAction_ =
      new QAction(QStringLiteral("Export Active Plot as &PNG..."), this);
  exportPlotPngAction_->setObjectName(QStringLiteral("exportPlotPngAction"));
  connect(exportPlotPngAction_, &QAction::triggered, this,
          [this](bool) { exportPlotPngDialog(); });

  exportPlotPdfAction_ =
      new QAction(QStringLiteral("Export Active Plot as P&DF..."), this);
  exportPlotPdfAction_->setObjectName(QStringLiteral("exportPlotPdfAction"));
  connect(exportPlotPdfAction_, &QAction::triggered, this,
          [this](bool) { exportPlotPdfDialog(); });

  exportPlotSvgAction_ =
      new QAction(QStringLiteral("Export Active Plot as S&VG..."), this);
  exportPlotSvgAction_->setObjectName(QStringLiteral("exportPlotSvgAction"));
  connect(exportPlotSvgAction_, &QAction::triggered, this,
          [this](bool) { exportPlotSvgDialog(); });

  exitAction_ = new QAction(QStringLiteral("E&xit"), this);
  exitAction_->setObjectName(QStringLiteral("exitAction"));
  exitAction_->setShortcut(QKeySequence::Quit);
  connect(exitAction_, &QAction::triggered, this,
          [this](bool) { close(); });

  aboutAction_ = new QAction(QStringLiteral("&About XFOIL.next"), this);
  aboutAction_->setObjectName(QStringLiteral("aboutAction"));
  connect(aboutAction_, &QAction::triggered, this,
          [this](bool) { showAbout(); });
}

void MainWindow::createMenus() {
  fileMenu_ = menuBar()->addMenu(QStringLiteral("&File"));
  fileMenu_->setObjectName(QStringLiteral("fileMenu"));
  fileMenu_->addAction(loadAirfoilAction_);
  fileMenu_->addAction(saveAirfoilAction_);
  fileMenu_->addAction(exportDxfAction_);
  fileMenu_->addSeparator();
  fileMenu_->addAction(importPolarAction_);
  fileMenu_->addAction(exportCurrentPolarAction_);
  fileMenu_->addAction(exportComparisonPolarAction_);
  fileMenu_->addSeparator();
  fileMenu_->addAction(exportCpAction_);
  fileMenu_->addAction(exportPlotPngAction_);
  fileMenu_->addAction(exportPlotPdfAction_);
  fileMenu_->addAction(exportPlotSvgAction_);
  recentFilesMenu_ = fileMenu_->addMenu(QStringLiteral("Recent Files"));
  recentFilesMenu_->setObjectName(QStringLiteral("recentFilesMenu"));
  fileMenu_->addSeparator();
  fileMenu_->addAction(exitAction_);

  helpMenu_ = menuBar()->addMenu(QStringLiteral("&Help"));
  helpMenu_->setObjectName(QStringLiteral("helpMenu"));
  helpMenu_->addAction(aboutAction_);

  updateRecentMenu();
}

void MainWindow::loadSettings() {
  if (!settings_) {
    return;
  }
  restoreGeometry(settings_->value(QStringLiteral("window.geometry")).toByteArray());
  restoreState(settings_->value(QStringLiteral("window.state")).toByteArray());
  const QStringList stored =
      settings_->value(QStringLiteral("recentFiles")).toStringList();
  recentFiles_.clear();
  for (const QString& path : stored) {
    if (path.isEmpty() || recentFiles_.size() >= 8) {
      continue;
    }
    const QString absolute = QFileInfo(path).absoluteFilePath();
    if (!recentFiles_.contains(absolute)) {
      recentFiles_.append(absolute);
    }
  }
  updateRecentMenu();
}

void MainWindow::saveSettings() {
  if (!settings_) {
    return;
  }
  settings_->setValue(QStringLiteral("window.geometry"), saveGeometry());
  settings_->setValue(QStringLiteral("window.state"), saveState());
  settings_->setValue(QStringLiteral("recentFiles"), recentFiles_);
  settings_->sync();
}

void MainWindow::updateAppActions() {
  saveAirfoilAction_->setEnabled(widget_ != nullptr &&
                                 widget_->geometry().has_value());
  exportDxfAction_->setEnabled(widget_ != nullptr &&
                               widget_->panelGeometry().has_value());
  importPolarAction_->setEnabled(widget_ != nullptr);
  exportCurrentPolarAction_->setEnabled(
      widget_ != nullptr && widget_->currentPolar().has_value());
  exportComparisonPolarAction_->setEnabled(
      widget_ != nullptr && widget_->selectedComparisonPolar().has_value());
  exportCpAction_->setEnabled(widget_ != nullptr &&
                              widget_->hasCurrentCp());
  const bool activePlot = widget_ != nullptr && widget_->hasActivePlot();
  exportPlotPngAction_->setEnabled(activePlot);
  exportPlotPdfAction_->setEnabled(activePlot);
  exportPlotSvgAction_->setEnabled(activePlot);
}

void MainWindow::updateRecentMenu() {
  if (recentFilesMenu_ == nullptr) {
    return;
  }
  recentFilesMenu_->clear();
  recentActions_.clear();
  if (recentFiles_.isEmpty()) {
    QAction* empty = recentFilesMenu_->addAction(
        QStringLiteral("No recent files"));
    empty->setEnabled(false);
    return;
  }
  for (int index = 0; index < recentFiles_.size(); ++index) {
    const QString path = recentFiles_.at(index);
    QAction* action = recentFilesMenu_->addAction(
        QFileInfo(path).fileName());
    action->setObjectName(QStringLiteral("recentFile%1Action").arg(index));
    action->setToolTip(path);
    action->setData(path);
    connect(action, &QAction::triggered, this,
            [this, path](bool) { openPath(path); });
    recentActions_.push_back(action);
  }
}

void MainWindow::addRecentFile(const QString& path) {
  const QString absolute = QFileInfo(path).absoluteFilePath();
  recentFiles_.removeAll(absolute);
  recentFiles_.prepend(absolute);
  while (recentFiles_.size() > 8) {
    recentFiles_.removeLast();
  }
  updateRecentMenu();
  if (settings_) {
    settings_->setValue(QStringLiteral("recentFiles"), recentFiles_);
    settings_->sync();
  }
}

void MainWindow::removeRecentFile(const QString& path) {
  const QString absolute = QFileInfo(path).absoluteFilePath();
  if (recentFiles_.removeAll(absolute) > 0) {
    updateRecentMenu();
    if (settings_) {
      settings_->setValue(QStringLiteral("recentFiles"), recentFiles_);
      settings_->sync();
    }
  }
}

void MainWindow::openFileDialog() {
  const std::optional<QString> path =
      chooseFile(FileDialogMode::open, QStringLiteral("Load Airfoil"),
                 openFilter(), QString());
  if (!path.has_value()) {
    widget_->cancelAirfoilImportRequest();
    return;
  }
  openPath(*path);
}

void MainWindow::openPath(const QString& path) {
  int selectedElement = 0;
  QString suppliedName;
  AirfoilFileReadResult read;
  QString suggestedName = QFileInfo(path).completeBaseName();

  for (;;) {
    read = readAirfoilFile(toPath(path), selectedElement,
                           suppliedName.toStdString());
    if (read.status == AirfoilFileReadStatus::elementSelectionRequired) {
      const std::optional<int> selected = askInt(
          QStringLiteral("Load Airfoil"),
          QStringLiteral("Enter element number:"),
          selectedElement > 0 ? selectedElement : 1, 1, 100000);
      if (!selected.has_value()) {
        widget_->cancelAirfoilImportRequest();
        return;
      }
      selectedElement = *selected;
      continue;
    }
    if (read.status == AirfoilFileReadStatus::nameRequired) {
      const std::optional<QString> name = askText(
          QStringLiteral("Load Airfoil"),
          QStringLiteral("Enter profile name:"), suggestedName);
      if (!name.has_value()) {
        widget_->cancelAirfoilImportRequest();
        return;
      }
      suppliedName = name->isEmpty() ? suggestedName : *name;
      continue;
    }
    break;
  }

  if (read.status != AirfoilFileReadStatus::success) {
    showError(QStringLiteral("Load Airfoil"),
              QString::fromStdString(read.message));
    widget_->cancelAirfoilImportRequest();
    removeRecentFile(path);
    return;
  }

  const Result<void> set =
      widget_->applyImportedGeometry(read.geometry, read.clockwiseReversed);
  if (set.isErr()) {
    showError(QStringLiteral("Load Airfoil"),
              QString::fromStdString(set.error().message));
    widget_->cancelAirfoilImportRequest();
    return;
  }

  addRecentFile(path);
  statusBar()->showMessage(
      QStringLiteral("Loaded %1").arg(QFileInfo(path).fileName()));
  updateAppActions();
}

void MainWindow::saveAirfoilDialog() {
  const std::optional<geometry::AirfoilGeometry> geometry =
      widget_->geometry();
  if (!geometry.has_value()) {
    showError(QStringLiteral("Save Airfoil"),
              QStringLiteral("No airfoil geometry is available."));
    return;
  }

  QString suggested = safeAirfoilBaseName(
      QString::fromStdString(std::string(geometry->name())));
  const std::optional<QString> path = chooseFile(
      FileDialogMode::save, QStringLiteral("Save Airfoil"),
      airfoilSaveFilter(), suggested);
  if (!path.has_value()) {
    return;
  }
  const FileWriteResult written =
      saveAirfoilFile(toPath(*path), *geometry, true);
  if (!written.success) {
    showError(QStringLiteral("Save Airfoil"),
              QString::fromStdString(written.message));
    return;
  }
  statusBar()->showMessage(
      QStringLiteral("Saved %1").arg(QFileInfo(*path).fileName()));
}

void MainWindow::exportDxfDialog() {
  const std::optional<SurfaceGeometry> panel = widget_->panelGeometry();
  if (!panel.has_value()) {
    showError(QStringLiteral("Export DXF"),
              QStringLiteral("No panel geometry is available."));
    return;
  }
  QString suggested = safeAirfoilBaseName(QString::fromStdString(
      std::string(widget_->geometry().has_value()
                      ? widget_->geometry()->name()
                      : std::string_view{})));
  const std::optional<QString> path = chooseFile(
      FileDialogMode::save, QStringLiteral("Export DXF"), dxfSaveFilter(),
      suggested);
  if (!path.has_value()) {
    return;
  }
  const FileWriteResult written =
      exportDxfFile(toPath(*path), *panel, true);
  if (!written.success) {
    showError(QStringLiteral("Export DXF"),
              QString::fromStdString(written.message));
    return;
  }
  statusBar()->showMessage(
      QStringLiteral("Exported %1").arg(QFileInfo(*path).fileName()));
}

void MainWindow::importPolarDialog() {
  const std::optional<QString> path =
      chooseFile(FileDialogMode::open, QStringLiteral("Import Polar"),
                 polarOpenFilter(), QString());
  if (!path.has_value()) {
    return;
  }
  const Result<PolarData> polar = readPolarFile(toPath(*path));
  if (polar.isErr()) {
    showError(QStringLiteral("Import Polar"),
              QString::fromStdString(polar.error().message));
    return;
  }
  const Result<std::uint64_t> added =
      widget_->addComparisonPolar(polar.value());
  if (added.isErr()) {
    showError(QStringLiteral("Import Polar"),
              QString::fromStdString(added.error().message));
    return;
  }
  statusBar()->showMessage(
      QStringLiteral("Imported %1").arg(QFileInfo(*path).fileName()));
  updateAppActions();
}

void MainWindow::exportCurrentPolarDialog() {
  const std::optional<PolarData> polar = widget_->currentPolar();
  if (!polar.has_value()) {
    showError(QStringLiteral("Export Current Polar"),
              QStringLiteral("No current polar is available."));
    return;
  }
  QString suggested = QString::fromStdString(polar->metadata.name);
  if (suggested.isEmpty()) {
    suggested = QStringLiteral("polar");
  }
  suggested = ensureSuffix(safeAirfoilBaseName(suggested),
                           QStringLiteral(".pol"));
  const std::optional<QString> path = chooseFile(
      FileDialogMode::save, QStringLiteral("Export Current Polar"),
      polarSaveFilter(), suggested);
  if (!path.has_value()) {
    return;
  }
  const FileWriteResult written =
      savePolarFile(toPath(ensureSuffix(*path, QStringLiteral(".pol"))),
                    *polar, true);
  if (!written.success) {
    showError(QStringLiteral("Export Current Polar"),
              QString::fromStdString(written.message));
    return;
  }
  statusBar()->showMessage(
      QStringLiteral("Exported %1").arg(QFileInfo(*path).fileName()));
}

void MainWindow::exportComparisonPolarDialog() {
  const std::optional<PolarData> polar = widget_->selectedComparisonPolar();
  if (!polar.has_value()) {
    showError(QStringLiteral("Export Comparison Polar"),
              QStringLiteral("No comparison polar is selected."));
    return;
  }
  QString suggested = QString::fromStdString(polar->metadata.name);
  if (suggested.isEmpty()) {
    suggested = QStringLiteral("polar");
  }
  suggested = ensureSuffix(safeAirfoilBaseName(suggested),
                           QStringLiteral(".pol"));
  const std::optional<QString> path = chooseFile(
      FileDialogMode::save, QStringLiteral("Export Comparison Polar"),
      polarSaveFilter(), suggested);
  if (!path.has_value()) {
    return;
  }
  const FileWriteResult written =
      savePolarFile(toPath(ensureSuffix(*path, QStringLiteral(".pol"))),
                    *polar, true);
  if (!written.success) {
    showError(QStringLiteral("Export Comparison Polar"),
              QString::fromStdString(written.message));
    return;
  }
  statusBar()->showMessage(
      QStringLiteral("Exported %1").arg(QFileInfo(*path).fileName()));
}

void MainWindow::exportCpDialog() {
  const std::optional<PressureExportData> data =
      widget_->selectedPressureExport();
  if (!data.has_value()) {
    showError(QStringLiteral("Export Pressure (Cp)"),
              QStringLiteral("No selected operating point has pressure "
                             "export data."));
    return;
  }
  const Result<std::string> encoded = encodePressureDumpText(*data);
  if (encoded.isErr()) {
    showError(QStringLiteral("Export Pressure (Cp)"),
              QString::fromStdString(encoded.error().message));
    return;
  }
  const QString suggested = ensureSuffix(
      safeAirfoilBaseName(QString::fromStdString(std::string(
          widget_->geometry().has_value() ? widget_->geometry()->name()
                                          : std::string_view{}))),
      QStringLiteral(".cp"));
  const std::optional<QString> path = chooseFile(
      FileDialogMode::save, QStringLiteral("Export Pressure (Cp)"),
      cpSaveFilter(), suggested);
  if (!path.has_value()) {
    return;
  }
  const FileWriteResult written =
      saveTextFile(toPath(ensureSuffix(*path, QStringLiteral(".cp"))),
                   encoded.value(), true);
  if (!written.success) {
    showError(QStringLiteral("Export Pressure (Cp)"),
              QString::fromStdString(written.message));
    return;
  }
  statusBar()->showMessage(
      QStringLiteral("Exported %1").arg(QFileInfo(*path).fileName()));
}

void MainWindow::exportPlotPngDialog() {
  const Result<std::string> encoded = widget_->activePlotPng();
  if (encoded.isErr()) {
    showError(QStringLiteral("Export PNG"),
              QString::fromStdString(encoded.error().message));
    return;
  }
  QString suggested = QStringLiteral("xfoil_plot.png");
  const std::optional<QString> path = chooseFile(
      FileDialogMode::save, QStringLiteral("Export Active Plot as PNG"),
      pngSaveFilter(), suggested);
  if (!path.has_value()) {
    return;
  }
  const FileWriteResult written =
      saveBinaryFile(toPath(*path), encoded.value(), true);
  if (!written.success) {
    showError(QStringLiteral("Export PNG"),
              QString::fromStdString(written.message));
    return;
  }
  statusBar()->showMessage(
      QStringLiteral("Exported %1").arg(QFileInfo(*path).fileName()));
}

void MainWindow::exportPlotPdfDialog() {
  const Result<std::string> encoded = widget_->activePlotPdf();
  if (encoded.isErr()) {
    showError(QStringLiteral("Export PDF"),
              QString::fromStdString(encoded.error().message));
    return;
  }
  QString suggested = QStringLiteral("xfoil_plot.pdf");
  const std::optional<QString> path = chooseFile(
      FileDialogMode::save, QStringLiteral("Export Active Plot as PDF"),
      pdfSaveFilter(), suggested);
  if (!path.has_value()) {
    return;
  }
  const FileWriteResult written =
      saveBinaryFile(toPath(*path), encoded.value(), true);
  if (!written.success) {
    showError(QStringLiteral("Export PDF"),
              QString::fromStdString(written.message));
    return;
  }
  statusBar()->showMessage(
      QStringLiteral("Exported %1").arg(QFileInfo(*path).fileName()));
}

void MainWindow::exportPlotSvgDialog() {
  const Result<std::string> encoded = widget_->activePlotSvg();
  if (encoded.isErr()) {
    showError(QStringLiteral("Export SVG"),
              QString::fromStdString(encoded.error().message));
    return;
  }
  QString suggested = QStringLiteral("xfoil_plot.svg");
  const std::optional<QString> path = chooseFile(
      FileDialogMode::save, QStringLiteral("Export Active Plot as SVG"),
      svgSaveFilter(), suggested);
  if (!path.has_value()) {
    return;
  }
  const FileWriteResult written =
      saveBinaryFile(toPath(*path), encoded.value(), true);
  if (!written.success) {
    showError(QStringLiteral("Export SVG"),
              QString::fromStdString(written.message));
    return;
  }
  statusBar()->showMessage(
      QStringLiteral("Exported %1").arg(QFileInfo(*path).fileName()));
}

void MainWindow::showAbout() {
  const QString text =
      QStringLiteral(
          "%1 %2\n\n"
          "Copyright (C) 2026 Olaf Frommann\n"
          "%6\n\n"
          "XFOIL.next is based on XFOIL 6.996\n"
          "by Mark Drela and Harold Youngren.\n\n"
          "License: %5\n\n"
          "Qt %3\n"
          "QCustomPlot %4")
          .arg(productName(), productVersion(),
               QStringLiteral(QT_VERSION_STR),
               QStringLiteral(QCUSTOMPLOT_VERSION_STR), productLicense(),
               productUrl());
  showInfo(QStringLiteral("About XFOIL.next"), text);
}

void MainWindow::closeEvent(QCloseEvent* event) {
  if (pendingClose_) {
    event->ignore();
    return;
  }
  if (widget_ != nullptr && widget_->isRunning()) {
    if (!confirm(QStringLiteral("Close XFOIL.next"),
                  QStringLiteral(
                      "Analysis is still running. Cancel and close?"))) {
      event->ignore();
      return;
    }
    pendingClose_ = true;
    widget_->cancel();
    event->ignore();
    return;
  }
  saveSettings();
  event->accept();
}

void MainWindow::schedulePendingCloseCheck() {
  QTimer::singleShot(20, this, [this]() {
    if (!pendingClose_) {
      return;
    }
    if (widget_ == nullptr || !widget_->isRunning()) {
      pendingClose_ = false;
      close();
      return;
    }
    schedulePendingCloseCheck();
  });
}

std::optional<QString> MainWindow::chooseFile(
    FileDialogMode mode, const QString& title, const QString& filter,
    const QString& suggested) const {
  return dependencies_.chooseFile(mode, title, filter, suggested);
}

std::optional<QString> MainWindow::askText(
    const QString& title, const QString& label,
    const QString& initial) const {
  return dependencies_.askText(title, label, initial);
}

std::optional<int> MainWindow::askInt(
    const QString& title, const QString& label, int initial, int minimum,
    int maximum) const {
  return dependencies_.askInt(title, label, initial, minimum, maximum);
}

bool MainWindow::confirm(const QString& title, const QString& text) const {
  return dependencies_.confirm(title, text);
}

void MainWindow::showError(const QString& title,
                           const QString& text) const {
  dependencies_.showError(title, text);
}

void MainWindow::showInfo(const QString& title, const QString& text) const {
  dependencies_.showInfo(title, text);
}

}  // namespace xfoil::gui::app

// End: src/gui/app/MainWindow.cpp
