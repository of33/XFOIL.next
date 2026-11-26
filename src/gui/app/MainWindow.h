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

// File: src/gui/app/MainWindow.h
// Thin standalone application shell. The workflowTabs host is the central
// widget; its single Analysis page owns the XFoilAnalysisWidget. File dialogs,
// settings and recent files remain app responsibilities.

#pragma once

#include <functional>
#include <memory>
#include <optional>

#include <QList>
#include <QMainWindow>
#include <QString>
#include <QStringList>

class QAction;
class QCloseEvent;
class QMenu;
class QSettings;
class QTabWidget;

namespace xfoil::gui {
class XFoilAnalysisWidget;
}

namespace xfoil::gui::app {

class MainWindow : public QMainWindow {
 public:
  enum class FileDialogMode { open, save };

  struct Dependencies {
    std::function<std::optional<QString>(
        FileDialogMode mode, const QString& title, const QString& filter,
        const QString& suggested)>
        chooseFile;
    std::function<std::optional<QString>(
        const QString& title, const QString& label,
        const QString& initial)>
        askText;
    std::function<std::optional<int>(
        const QString& title, const QString& label, int initial,
        int minimum, int maximum)>
        askInt;
    std::function<bool(const QString& title, const QString& text)> confirm;
    std::function<void(const QString& title, const QString& text)> showError;
    std::function<void(const QString& title, const QString& text)> showInfo;
    QString settingsFile;
  };

  explicit MainWindow(QWidget* parent = nullptr);
  explicit MainWindow(Dependencies dependencies, QWidget* parent = nullptr);
  ~MainWindow() override;

  MainWindow(const MainWindow&) = delete;
  MainWindow& operator=(const MainWindow&) = delete;

 protected:
  void closeEvent(QCloseEvent* event) override;

 private:
  void createActions();
  void createMenus();
  void loadSettings();
  void saveSettings();
  void updateAppActions();
  void updateRecentMenu();
  void openFileDialog();
  void openPath(const QString& path);
  void saveAirfoilDialog();
  void exportDxfDialog();
  void exportAirfoilPngDialog();
  void exportAirfoilPdfDialog();
  void exportAirfoilSvgDialog();
  void importPolarDialog();
  void exportCurrentPolarDialog();
  void exportComparisonPolarDialog();
  void exportCpDialog();
  void exportPlotPngDialog();
  void exportPlotPdfDialog();
  void exportPlotSvgDialog();
  void addRecentFile(const QString& path);
  void removeRecentFile(const QString& path);
  void showAbout();
  void schedulePendingCloseCheck();

  [[nodiscard]]
  std::optional<QString> chooseFile(
      FileDialogMode mode, const QString& title, const QString& filter,
      const QString& suggested) const;
  [[nodiscard]]
  std::optional<QString> askText(const QString& title,
                                 const QString& label,
                                 const QString& initial) const;
  [[nodiscard]]
  std::optional<int> askInt(const QString& title, const QString& label,
                            int initial, int minimum, int maximum) const;
  [[nodiscard]]
  bool confirm(const QString& title, const QString& text) const;
  void showError(const QString& title, const QString& text) const;
  void showInfo(const QString& title, const QString& text) const;

  Dependencies dependencies_;
  QTabWidget* workflowTabs_{};
  XFoilAnalysisWidget* widget_{};
  QAction* loadAirfoilAction_{};
  QAction* saveAirfoilAction_{};
  QAction* exportDxfAction_{};
  QAction* exportAirfoilPngAction_{};
  QAction* exportAirfoilPdfAction_{};
  QAction* exportAirfoilSvgAction_{};
  QAction* importPolarAction_{};
  QAction* exportCurrentPolarAction_{};
  QAction* exportComparisonPolarAction_{};
  QAction* exportCpAction_{};
  QAction* exportPlotPngAction_{};
  QAction* exportPlotPdfAction_{};
  QAction* exportPlotSvgAction_{};
  QAction* exitAction_{};
  QAction* aboutAction_{};
  QMenu* exportAirfoilMenu_{};
  QMenu* exportPlotMenu_{};
  QMenu* recentFilesMenu_{};
  QMenu* fileMenu_{};
  QMenu* helpMenu_{};
  QList<QAction*> recentActions_;
  QStringList recentFiles_;
  std::unique_ptr<QSettings> settings_;
  bool pendingClose_{false};
};

}  // namespace xfoil::gui::app

// End: src/gui/app/MainWindow.h
