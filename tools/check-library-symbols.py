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

# -*- coding: utf-8 -*-
"""Non-mutating symbol audit for the L7 library artifacts.

Checks the static archive and the shared library for forbidden unresolved
file/console/environment/process symbols, app/CLI implementation symbols,
and accidental internal exports. This is a static complement to the armed
runtime proof; it does not replace it.
"""

import argparse
import os
import re
import subprocess
import sys

FORBIDDEN_C_NAMES = {
    "open", "open64", "openat", "creat", "fopen", "freopen", "rename",
    "unlink", "remove", "chdir", "getcwd", "getenv", "setenv", "putenv",
    "unsetenv", "tmpfile", "mkstemp", "mkstemps", "system", "popen", "fork",
    "vfork", "execl", "execle", "execlp", "execv", "execve", "execvp",
    "execvpe", "abort", "exit", "_exit", "quick_exit", "terminate", "stat",
    "lstat", "access", "opendir", "readdir", "mkdir", "rmdir", "write",
    "read", "setlocale",
}
FORBIDDEN_DEMANGLED_PREFIXES = (
    "std::filesystem::",
    "std::basic_filebuf",
    "std::basic_fstream",
    "std::basic_ifstream",
    "std::basic_ofstream",
    "std::cout",
    "std::cerr",
    "std::cin",
    "std::exit(",
    "std::abort(",
    "std::terminate(",
    "std::remove(",
    "std::rename(",
)
APP_MARKERS = (
    "app::", "CommandInterpreter", "AirfoilWriter", "DatFileReader",
    "DumpFileAdapter", " T main",
)
PUBLIC_API_MARKERS = (
    "xfoil::analyze(", "xfoil::Session::Session(", "xfoil::Session::~Session(",
    "xfoil::validate(",
)


def run_nm(args):
    completed = subprocess.run(
        ["nm"] + args, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        text=True, check=False)
    if completed.returncode != 0:
        raise RuntimeError(completed.stderr.strip() or "nm failed")
    return completed.stdout.splitlines()


def undefined_symbols(path, dynamic):
    args = ["-C", "-u"]
    if dynamic:
        args.insert(0, "-D")
    lines = run_nm(args + [path])
    result = []
    for line in lines:
        text = line.strip()
        if not text or text.endswith(":"):
            continue
        parts = text.split()
        if len(parts) < 2:
            continue
        symbol = parts[-1]
        base = symbol.split("@", 1)[0]
        result.append((symbol, base, text))
    return result


def defined_symbols(path, dynamic):
    args = ["-C", "--defined-only"]
    if dynamic:
        args.insert(0, "-D")
    return [line.strip() for line in run_nm(args + [path]) if line.strip()]


def forbidden_name(base, text):
    if base in FORBIDDEN_C_NAMES:
        return True
    return any(text.startswith("U " + prefix) or prefix in text
               for prefix in FORBIDDEN_DEMANGLED_PREFIXES)


def check_artifact(path, dynamic, require_public):
    messages = []
    for symbol, base, text in undefined_symbols(path, dynamic):
        if forbidden_name(base, text):
            messages.append("forbidden unresolved symbol: %s" % symbol)
        if dynamic and "xfoil::" in text:
            messages.append("undefined project symbol: %s" % symbol)

    defined = defined_symbols(path, dynamic)
    joined = "\n".join(defined)
    for marker in APP_MARKERS:
        if marker in joined:
            messages.append("application/CLI symbol in library: %s" % marker)
    if require_public:
        for marker in PUBLIC_API_MARKERS:
            if marker not in joined:
                messages.append("missing public API symbol: %s" % marker)

    if dynamic:
        for line in defined:
            parts = line.split(None, 2)
            symbol = parts[2] if len(parts) >= 3 else line
            kind = parts[1] if len(parts) >= 2 else ""
            if ("xfoil::core::" in symbol or
                    "xfoil::session::" in symbol):
                weak_std_template = (kind in ("W", "V") and
                                     ("std::" in symbol or
                                      "__gnu_cxx::" in symbol))
                if not (symbol.startswith("std::") or weak_std_template):
                    messages.append(
                        "internal symbol exported from shared library: %s"
                        % symbol)
    return messages


def main(argv):
    parser = argparse.ArgumentParser(description="L7 library symbol audit")
    parser.add_argument("--static", required=True,
                        help="static archive to audit")
    parser.add_argument("--shared", required=True,
                        help="shared library to audit")
    args = parser.parse_args(argv)

    name = "library-symbol-check"
    failures = []
    for path, dynamic, require_public in (
            (args.static, False, False),
            (args.shared, True, True)):
        if not os.path.isfile(path):
            failures.append("missing artifact: %s" % path)
            continue
        try:
            failures.extend(check_artifact(path, dynamic, require_public))
        except RuntimeError as exc:
            failures.append("%s: %s" % (path, exc))

    for message in failures:
        sys.stdout.write("%s: %s\n" % (name, message))
    if failures:
        sys.stdout.write("Library symbol check: FAIL (%d finding(s)).\n"
                         % len(failures))
        return 1
    sys.stdout.write("Library symbol check: OK (static and shared).\n")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
