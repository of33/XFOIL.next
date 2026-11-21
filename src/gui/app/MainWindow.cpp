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
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QKeySequence>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>

#include "app/GuiFileAdapters.h"
#include "app/ProductInfo.h"
#include "qcustomplot.h"
#include "xfoil/gui/XFoilWidget.h"

namespace xfoil::gui::app {
namespace {

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
      QMessageBox::information(this, title, text);
    };
  }

  if (dependencies_.settingsFile.isEmpty()) {
    settings_ = std::make_unique<QSettings>();
  } else {
    settings_ = std::make_unique<QSettings>(dependencies_.settingsFile,
                                             QSettings::IniFormat);
  }

  setWindowTitle(productName() + QStringLiteral(" ") + productVersion());
  resize(1280, 800);
  widget_ = new XFoilWidget(this);
  setCentralWidget(widget_);

  createActions();
  createMenusAndToolBar();
  statusBar()->showMessage(QStringLiteral("Ready"));

  connect(widget_, &XFoilWidget::geometryChanged, this,
          [this]() { updateAppActions(); });
  connect(widget_, &XFoilWidget::panelingChanged, this,
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
  openAction_ = new QAction(QStringLiteral("&Open..."), this);
  openAction_->setObjectName(QStringLiteral("openAction"));
  openAction_->setShortcut(QKeySequence::Open);
  connect(openAction_, &QAction::triggered, this,
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

void MainWindow::createMenusAndToolBar() {
  fileMenu_ = menuBar()->addMenu(QStringLiteral("&File"));
  fileMenu_->setObjectName(QStringLiteral("fileMenu"));
  fileMenu_->addAction(openAction_);
  fileMenu_->addAction(saveAirfoilAction_);
  fileMenu_->addAction(exportDxfAction_);
  recentFilesMenu_ = fileMenu_->addMenu(QStringLiteral("Recent Files"));
  recentFilesMenu_->setObjectName(QStringLiteral("recentFilesMenu"));
  fileMenu_->addSeparator();
  fileMenu_->addAction(exitAction_);

  QMenu* analysisMenu = menuBar()->addMenu(QStringLiteral("&Analysis"));
  analysisMenu->setObjectName(QStringLiteral("analysisMenu"));
  analysisMenu->addAction(widget_->analyzeAction());
  analysisMenu->addAction(widget_->cancelAction());

  helpMenu_ = menuBar()->addMenu(QStringLiteral("&Help"));
  helpMenu_->setObjectName(QStringLiteral("helpMenu"));
  helpMenu_->addAction(aboutAction_);

  QToolBar* toolBar = addToolBar(QStringLiteral("Main"));
  toolBar->setObjectName(QStringLiteral("mainToolBar"));
  toolBar->addAction(openAction_);
  toolBar->addAction(saveAirfoilAction_);
  toolBar->addAction(exportDxfAction_);
  toolBar->addSeparator();
  toolBar->addAction(widget_->analyzeAction());
  toolBar->addAction(widget_->cancelAction());
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
      chooseFile(FileDialogMode::open, QStringLiteral("Open Airfoil"),
                 openFilter(), QString());
  if (path.has_value()) {
    openPath(*path);
  }
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
          QStringLiteral("Open Airfoil"),
          QStringLiteral("Enter element number:"),
          selectedElement > 0 ? selectedElement : 1, 1, 100000);
      if (!selected.has_value()) {
        return;
      }
      selectedElement = *selected;
      continue;
    }
    if (read.status == AirfoilFileReadStatus::nameRequired) {
      const std::optional<QString> name = askText(
          QStringLiteral("Open Airfoil"),
          QStringLiteral("Enter profile name:"), suggestedName);
      if (!name.has_value()) {
        return;
      }
      suppliedName = name->isEmpty() ? suggestedName : *name;
      continue;
    }
    break;
  }

  if (read.status != AirfoilFileReadStatus::success) {
    showError(QStringLiteral("Open Airfoil"),
              QString::fromStdString(read.message));
    removeRecentFile(path);
    return;
  }

  const Result<void> set = widget_->setGeometry(read.geometry);
  if (set.isErr()) {
    showError(QStringLiteral("Open Airfoil"),
              QString::fromStdString(set.error().message));
    return;
  }

  const Result<SurfaceGeometry> panel =
      widget_->generatePanel(widget_->panelingOptions());
  if (panel.isErr()) {
    showError(QStringLiteral("Panel generation failed"),
              QString::fromStdString(panel.error().message));
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

  QString suggested = QString::fromStdString(std::string(geometry->name()));
  if (suggested.isEmpty()) {
    suggested = QStringLiteral("airfoil");
  }
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
  QString suggested = QString::fromStdString(
      std::string(widget_->geometry().has_value()
                      ? widget_->geometry()->name()
                      : std::string_view{}));
  if (suggested.isEmpty()) {
    suggested = QStringLiteral("airfoil");
  }
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

void MainWindow::showAbout() {
  const QString text =
      QStringLiteral(
          "%1 %2\n\n"
          "Based upon XFOIL Version 6.996\n\n"
          "Qt %3\n"
          "QCustomPlot %4\n\n"
          "%5\n"
          "%6")
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
