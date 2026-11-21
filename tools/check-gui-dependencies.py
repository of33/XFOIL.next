#!/usr/bin/env python3
# XFOIL.next
#
# Copyright (C) 2026 Olaf Frommann
# https://github.com/of33/XFOIL.next
#
# XFOIL.next is based on XFOIL 6.99 by Mark Drela and
# Harold Youngren.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.
#
# SPDX-License-Identifier: GPL-3.0-or-later

"""Non-mutating GUI dependency, boundary and vendoring gate.

Checks:
  - public GUI headers use only the permitted Qt/public-include surface;
  - GUI implementation sources contain no session, solver, CLI, QProcess,
    QSettings, QFileDialog or file-I/O dependency;
  - the vendored QCustomPlot directory contains the expected files;
  - qcustomplot.h, qcustomplot.cpp and PATCHES.md match the recorded
    hashes, and every SHA256SUMS entry verifies;
  - the vendored license is the full GPL text;
  - no internal source path leaks into the vendored record.

Exit: 0 = OK, 1 = violation, 2 = invocation/setup error.
"""

import argparse
import hashlib
import os
import re
import sys

PUBLIC_HEADER_DIR = "include/xfoil/gui"
SOURCE_DIR = "src/gui"
APP_SOURCE_DIR = "src/gui/app"
QCP_DIR = "src/gui/3rdparty/qcustomplot-2.1.1"

QCP_REQUIRED = [
    "LICENSE",
    "PATCHES.md",
    "README.md",
    "SHA256SUMS",
    "qcustomplot.cpp",
    "qcustomplot.h",
]

QCP_EXPECTED = {
    "qcustomplot.h":
        "920646e8db6406a568bed52a121db6498a2c507a687400f3a559e3d840f1888c",
    "qcustomplot.cpp":
        "fd951300bb0911bbca53141e6b2959482cd951583fd7d3227718322d4dac3d95",
    "PATCHES.md":
        "e666cdffabcb78bcd097490bda6536807e8d563baab733bd4e2b71a86cbf4af2",
    "LICENSE":
        "3972dc9744f6499f0f9b2dbf76696f2ae7ad8af9b23dde66d6af86c9dfb36986",
}

PUBLIC_ALLOWED_EXACT = {
    "memory",
    "utility",
    "QWidget",
    "QObject",
    "QString",
    "QStringView",
    "QSize",
    "QPoint",
    "QPointF",
    "QRect",
    "QRectF",
    "QMetaObject",
    "QMetaType",
    "QtGlobal",
    "optional",
    "string_view",
    "xfoil/AnalysisConfig.h",
    "xfoil/AnalysisData.h",
    "xfoil/AnalysisRequest.h",
    "xfoil/Progress.h",
    "xfoil/Result.h",
    "xfoil/SequenceResult.h",
    "xfoil/Session.h",
    "xfoil/SessionOptions.h",
    "xfoil/geometry/AirfoilGeometry.h",
}
PUBLIC_ALLOWED_PREFIXES = (
    "QtCore/",
    "QtGui/",
    "QtWidgets/",
    "xfoil/gui/",
)
PUBLIC_FORBIDDEN_PREFIXES = (
    "xfoil/session/",
    "xfoil/core/",
    "xfoil/io/",
    "xfoil/XFoil.h",
    "xfoil/XFoilSession.h",
    "src/",
    "app/",
)

SOURCE_FORBIDDEN_TEXT = (
    "AirfoilCenter",
    "QProcess",
    "QSettings",
    "QFileDialog",
    "QFile",
    "QDir",
    "QApplication",
    "QMainWindow",
    "CommandInterpreter",
    "SessionOps",
    "xfoil/session/",
    "xfoil/core/",
    "xfoil/io/",
    "xfoil/XFoil.h",
    "xfoil/Session.h",
    "xfoil/XFoilSession.h",
    "src/app/",
    "std::filesystem",
    "std::fstream",
    "std::ifstream",
    "std::ofstream",
)

APP_ALLOWED_INCLUDE_EXACT = {
    "qcustomplot.h",
    "xfoil/AnalysisData.h",
    "xfoil/Diagnostic.h",
    "xfoil/Result.h",
    "xfoil/SkinFriction.h",
    "xfoil/Types.h",
}
APP_ALLOWED_INCLUDE_PREFIXES = (
    "app/",
    "xfoil/gui/",
    "xfoil/io/",
    "xfoil/geometry/",
)
APP_ALLOWED_STANDARD_INCLUDES = {
    "algorithm",
    "clocale",
    "cstddef",
    "filesystem",
    "functional",
    "memory",
    "optional",
    "span",
    "string",
    "string_view",
    "system_error",
    "utility",
    "vector",
}
APP_FORBIDDEN_TEXT = (
    "AirfoilCenter",
    "QProcess",
    "CommandInterpreter",
    "SessionOps",
    "xfoil::Session",
    "xfoil/session/",
    "xfoil/core/",
    "xfoil/XFoil.h",
    "xfoil/XFoilSession.h",
    "src/app/",
)

SESSION_ALLOWED_SOURCES = {
    "src/gui/backend/GuiSessionBackend.cpp",
    "src/gui/backend/GuiSessionBackend.h",
    "src/gui/model/GuiDefinitionModel.h",
    "src/gui/model/GuiRunSpec.h",
    "src/gui/worker/GuiAnalysisWorker.cpp",
    "src/gui/worker/GuiAnalysisWorker.h",
}

INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]')
INTERNAL_PATH_RE = re.compile(
    r"(?:/local/|/home/|Eigene_Software|AirfoilCenter_refactored|C:\\\\Users)")
ABSOLUTE_INCLUDE_RE = re.compile(r"^(?:/|[A-Za-z]:[\\/])")


def read_text(path):
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        return handle.read()


def sha256_file(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 16), b""):
            digest.update(chunk)
    return digest.hexdigest()


def includes(text):
    result = []
    for line in text.splitlines():
        match = INCLUDE_RE.match(line)
        if match:
            result.append(match.group(1).replace("\\", "/"))
    return result


def check_public_headers(root, failures):
    directory = os.path.join(root, PUBLIC_HEADER_DIR)
    if not os.path.isdir(directory):
        failures.append("missing public GUI directory: %s" % PUBLIC_HEADER_DIR)
        return
    for name in sorted(os.listdir(directory)):
        if not name.endswith((".h", ".hpp")):
            continue
        rel = PUBLIC_HEADER_DIR + "/" + name
        text = read_text(os.path.join(root, rel))
        for target in includes(text):
            if target.startswith(PUBLIC_FORBIDDEN_PREFIXES):
                failures.append("%s: forbidden public include: %s" % (rel, target))
                continue
            if ABSOLUTE_INCLUDE_RE.match(target):
                failures.append("%s: absolute public include: %s" % (rel, target))
                continue
            if target.startswith(PUBLIC_ALLOWED_PREFIXES):
                continue
            if target in PUBLIC_ALLOWED_EXACT:
                continue
            failures.append("%s: unexpected public include: %s" % (rel, target))
        if INTERNAL_PATH_RE.search(text):
            failures.append("%s: internal source path in public header" % rel)


def check_sources(root, failures):
    directory = os.path.join(root, SOURCE_DIR)
    if not os.path.isdir(directory):
        failures.append("missing GUI source directory: %s" % SOURCE_DIR)
        return
    for dirpath, dirnames, filenames in os.walk(directory):
        rel_dir = os.path.relpath(dirpath, root).replace(os.sep, "/")
        if rel_dir.startswith(QCP_DIR):
            dirnames[:] = []
            continue
        if rel_dir == APP_SOURCE_DIR or rel_dir.startswith(
                APP_SOURCE_DIR + "/"):
            dirnames[:] = []
            continue
        dirnames[:] = sorted(
            name for name in dirnames
            if not (rel_dir + "/" + name).startswith(QCP_DIR)
        )
        for name in sorted(filenames):
            if not name.endswith((".cpp", ".cc", ".h", ".hpp")):
                continue
            rel = rel_dir + "/" + name
            text = read_text(os.path.join(root, rel))
            for token in SOURCE_FORBIDDEN_TEXT:
                if token not in text:
                    continue
                if token == "xfoil/Session.h" and rel in SESSION_ALLOWED_SOURCES:
                    continue
                failures.append("%s: forbidden GUI source token: %s"
                                % (rel, token))
            for target in includes(text):
                if ABSOLUTE_INCLUDE_RE.match(target):
                    failures.append("%s: absolute source include: %s"
                                    % (rel, target))
                if target.startswith(("xfoil/session/", "xfoil/core/",
                                      "xfoil/io/", "src/app/")):
                    failures.append("%s: forbidden source include: %s"
                                    % (rel, target))
            if INTERNAL_PATH_RE.search(text):
                failures.append("%s: internal source path in GUI source" % rel)


def check_app_sources(root, failures):
    directory = os.path.join(root, APP_SOURCE_DIR)
    if not os.path.isdir(directory):
        failures.append("missing GUI app directory: %s" % APP_SOURCE_DIR)
        return
    for dirpath, _dirnames, filenames in os.walk(directory):
        for name in sorted(filenames):
            if not name.endswith((".cpp", ".cc", ".h", ".hpp")):
                continue
            rel = os.path.relpath(os.path.join(dirpath, name), root)
            rel = rel.replace(os.sep, "/")
            text = read_text(os.path.join(root, rel))
            for token in APP_FORBIDDEN_TEXT:
                if token in text:
                    failures.append("%s: forbidden app source token: %s"
                                    % (rel, token))
            if "QApplication" in text and not rel.endswith("/main.cpp"):
                failures.append("%s: QApplication is only allowed in "
                                "src/gui/app/main.cpp" % rel)
            for target in includes(text):
                if ABSOLUTE_INCLUDE_RE.match(target):
                    failures.append("%s: absolute app include: %s"
                                    % (rel, target))
                    continue
                if target.startswith("Q"):
                    continue
                if target.startswith(APP_ALLOWED_INCLUDE_PREFIXES):
                    continue
                if target in APP_ALLOWED_INCLUDE_EXACT:
                    continue
                if target in APP_ALLOWED_STANDARD_INCLUDES:
                    continue
                failures.append("%s: unexpected app include: %s"
                                % (rel, target))
            if INTERNAL_PATH_RE.search(text):
                failures.append("%s: internal source path in app source"
                                % rel)


def check_qcustomplot(root, failures):
    directory = os.path.join(root, QCP_DIR)
    if not os.path.isdir(directory):
        failures.append("missing QCustomPlot directory: %s" % QCP_DIR)
        return

    for name in QCP_REQUIRED:
        rel = QCP_DIR + "/" + name
        if not os.path.isfile(os.path.join(root, rel)):
            failures.append("missing QCustomPlot file: %s" % rel)

    for name, expected in QCP_EXPECTED.items():
        rel = QCP_DIR + "/" + name
        full = os.path.join(root, rel)
        if not os.path.isfile(full):
            continue
        actual = sha256_file(full)
        if actual != expected:
            failures.append("%s: hash mismatch (expected %s, found %s)"
                            % (rel, expected, actual))

    sums_rel = QCP_DIR + "/SHA256SUMS"
    sums_path = os.path.join(root, sums_rel)
    if os.path.isfile(sums_path):
        entries = []
        for line_no, line in enumerate(read_text(sums_path).splitlines(), 1):
            if not line.strip():
                continue
            parts = line.split()
            if len(parts) != 2 or not re.fullmatch(r"[0-9a-f]{64}", parts[0]):
                failures.append("%s:%d: invalid SHA256SUMS line"
                                % (sums_rel, line_no))
                continue
            entries.append((parts[1], parts[0]))
        listed = [name for name, _ in entries]
        if listed != sorted(listed):
            failures.append("%s: entries are not sorted by path" % sums_rel)
        if set(listed) != set(QCP_REQUIRED) - {"SHA256SUMS"}:
            failures.append("%s: entry set does not match required files"
                            % sums_rel)
        for name, expected in entries:
            rel = QCP_DIR + "/" + name
            full = os.path.join(root, rel)
            if not os.path.isfile(full):
                failures.append("%s: listed file missing" % rel)
                continue
            actual = sha256_file(full)
            if actual != expected:
                failures.append("%s: SHA256SUMS mismatch" % rel)

    license_path = os.path.join(root, QCP_DIR, "LICENSE")
    if os.path.isfile(license_path):
        license_text = read_text(license_path)
        if "GNU GENERAL PUBLIC LICENSE" not in license_text:
            failures.append("QCustomPlot LICENSE: not the GPL text")
        if "Version 3" not in license_text:
            failures.append("QCustomPlot LICENSE: GPL version 3 not found")
        if len(license_text) < 30000:
            failures.append("QCustomPlot LICENSE: suspiciously short")

    cpp_path = os.path.join(root, QCP_DIR, "qcustomplot.cpp")
    if os.path.isfile(cpp_path):
        if "qcpZoneForTimeSpec" not in read_text(cpp_path):
            failures.append("QCustomPlot cpp: expected Qt compatibility patch "
                            "marker missing")

    for dirpath, _dirnames, filenames in os.walk(directory):
        for name in filenames:
            if not name.endswith((".md", ".txt", ".h", ".cpp")):
                continue
            rel = os.path.relpath(os.path.join(dirpath, name), root)
            rel = rel.replace(os.sep, "/")
            if INTERNAL_PATH_RE.search(read_text(os.path.join(root, rel))):
                failures.append("%s: internal source path in vendored record"
                                % rel)


def main(argv):
    parser = argparse.ArgumentParser(
        description="Check GUI G1 boundaries and QCustomPlot vendoring.")
    parser.add_argument("--root", default=".",
                        help="repository root (default: current dir)")
    args = parser.parse_args(argv[1:])

    root = os.path.abspath(args.root)
    if not os.path.isdir(root):
        print("ERROR: not a directory: %s" % root, file=sys.stderr)
        return 2

    failures = []
    check_public_headers(root, failures)
    check_sources(root, failures)
    check_app_sources(root, failures)
    check_qcustomplot(root, failures)

    if failures:
        for failure in failures:
            print("FAIL: %s" % failure)
        print("GUI dependency/vendoring gate: FAIL (%d finding(s))"
              % len(failures))
        return 1

    print("GUI dependency/vendoring gate: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
