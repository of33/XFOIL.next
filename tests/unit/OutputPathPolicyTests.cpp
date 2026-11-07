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

// File: tests/unit/OutputPathPolicyTests.cpp
// unit tests for the prefix transformation.
// Matrix: empty path, relative path with and without directory, absolute path,
// path ending in '/', nested directories, empty prefix as identity,
// and special characters from the allowed prefix character set.

#include "TestSupport.h"
#include "xfoil/io/OutputPathPolicy.h"

#include <string>

TEST(unit, OutputPathPolicyEmptyPathStaysEmpty) {
  const xfoil::io::OutputPathPolicy policy{"run01_"};
  CHECK_EQ(xfoil::io::applyOutputPathPolicy(policy, std::string{}),
           std::string{});
}

TEST(unit, OutputPathPolicyEmptyPrefixIsIdentity) {
  const xfoil::io::OutputPathPolicy policy{""};
  CHECK_EQ(xfoil::io::applyOutputPathPolicy(policy, "CLARK_Y.cp"),
           std::string{"CLARK_Y.cp"});
  CHECK_EQ(xfoil::io::applyOutputPathPolicy(policy, "sub/CLARK_Y.cp"),
           std::string{"sub/CLARK_Y.cp"});
  CHECK_EQ(xfoil::io::applyOutputPathPolicy(policy, "/tmp/x.cp"),
           std::string{"/tmp/x.cp"});
}

TEST(unit, OutputPathPolicyBareNameGetsPrefixDirectlyBeforeBase) {
  const xfoil::io::OutputPathPolicy policy{"candidate_001"};
  CHECK_EQ(xfoil::io::applyOutputPathPolicy(policy, "polar.pcf"),
           std::string{"candidate_001polar.pcf"});
}

TEST(unit, OutputPathPolicyRelativeDirectoryKeepsDirectoryPart) {
  const xfoil::io::OutputPathPolicy policy{"P1"};
  CHECK_EQ(xfoil::io::applyOutputPathPolicy(policy, "dir/name.ext"),
           std::string{"dir/P1name.ext"});
  CHECK_EQ(xfoil::io::applyOutputPathPolicy(policy, "a/b/c/name"),
           std::string{"a/b/c/P1name"});
  CHECK_EQ(xfoil::io::applyOutputPathPolicy(policy, "./x.cp"),
           std::string{"./P1x.cp"});
  CHECK_EQ(xfoil::io::applyOutputPathPolicy(policy, "../x.cp"),
           std::string{"../P1x.cp"});
}

TEST(unit, OutputPathPolicyAbsolutePathTransformedAtLastSeparator) {
  const xfoil::io::OutputPathPolicy policy{"PREFIX"};
  CHECK_EQ(xfoil::io::applyOutputPathPolicy(policy, "/tmp/x.cp"),
           std::string{"/tmp/PREFIXx.cp"});
  CHECK_EQ(xfoil::io::applyOutputPathPolicy(policy, "/x"),
           std::string{"/PREFIXx"});
}

TEST(unit, OutputPathPolicyTrailingSlashStaysUnchanged) {
  const xfoil::io::OutputPathPolicy policy{"P"};
  CHECK_EQ(xfoil::io::applyOutputPathPolicy(policy, "dir/"),
           std::string{"dir/"});
  CHECK_EQ(xfoil::io::applyOutputPathPolicy(policy, "/"),
           std::string{"/"});
}

TEST(unit, OutputPathPolicyExtensionNotMoved) {
  const xfoil::io::OutputPathPolicy policy{"pre"};
  CHECK_EQ(xfoil::io::applyOutputPathPolicy(policy, "name.ext"),
           std::string{"prename.ext"});
  CHECK_EQ(xfoil::io::applyOutputPathPolicy(policy, "name"),
           std::string{"prename"});
}

TEST(unit, OutputPathPolicyAllowedPrefixCharacters) {
  const xfoil::io::OutputPathPolicy policy{"aZ09._-x"};
  CHECK_EQ(xfoil::io::applyOutputPathPolicy(policy, "out.dat"),
           std::string{"aZ09._-xout.dat"});
}

TEST(unit, OutputPathPolicyOnlyLastSeparatorSplits) {
  // A prefix never contains '/' by character set, but splitting still
  // occurs at the last separator so the directory structure is preserved.
  const xfoil::io::OutputPathPolicy policy{"p"};
  CHECK_EQ(xfoil::io::applyOutputPathPolicy(policy, "a/b/c/d/name.cp"),
           std::string{"a/b/c/d/pname.cp"});
}

TEST(unit, OutputPathPolicyForwardSlashWindowsStylePaths) {
  // Platform-independent: '/' is a separator on every platform, so
  // forward-slash Windows-style paths are prefixed at the base name
  // everywhere.
  const xfoil::io::OutputPathPolicy policy{"A_"};
  CHECK_EQ(xfoil::io::applyOutputPathPolicy(policy, "C:/dir/foil.dat"),
           std::string{"C:/dir/A_foil.dat"});
  CHECK_EQ(xfoil::io::applyOutputPathPolicy(policy, "C:/foil.dat"),
           std::string{"C:/A_foil.dat"});
}

TEST(unit, OutputPathPolicyNativeSeparatorSemantics) {
  const xfoil::io::OutputPathPolicy policy{"A_"};
#ifdef _WIN32
  // Native Windows: backslash is a separator; drive-letter and UNC
  // paths are prefixed at the base name.
  CHECK_EQ(xfoil::io::applyOutputPathPolicy(policy, "C:\\dir\\foil.dat"),
           std::string{"C:\\dir\\A_foil.dat"});
  CHECK_EQ(xfoil::io::applyOutputPathPolicy(policy, "C:\\foil.dat"),
           std::string{"C:\\A_foil.dat"});
  CHECK_EQ(xfoil::io::applyOutputPathPolicy(policy, "\\\\server\\share\\f.dat"),
           std::string{"\\\\server\\share\\A_f.dat"});
  CHECK_EQ(xfoil::io::applyOutputPathPolicy(policy, "C:\\dir\\"),
           std::string{"C:\\dir\\"});
  CHECK_EQ(xfoil::io::applyOutputPathPolicy(policy, "dir\\sub\\x.cp"),
           std::string{"dir\\sub\\A_x.cp"});
#else
  // POSIX contract (unchanged): a backslash is a regular filename
  // character, so the whole token is the base name.
  CHECK_EQ(xfoil::io::applyOutputPathPolicy(policy, "C:\\dir\\foil.dat"),
           std::string{"A_C:\\dir\\foil.dat"});
  CHECK_EQ(xfoil::io::applyOutputPathPolicy(policy, "dir\\name.cp"),
           std::string{"A_dir\\name.cp"});
#endif
}

// End: tests/unit/OutputPathPolicyTests.cpp
