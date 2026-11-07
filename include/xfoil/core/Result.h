// XFOIL.next
//
// Copyright (C) 2026 Olaf Frommann
// https://github.com/of33/XFOIL.next
//
// XFOIL.next is based on XFOIL 6.99 by Mark Drela and
// Harold Youngren.
//
// This file contains a C++ implementation derived from XFOIL 6.99.
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

// File: include/xfoil/core/Result.h
// Minimal, non-domain-specific core error and result objects
//. No error propagation via global flags, stored
// output streams or magic integer codes. Domain-specific controlled errors
// are communicated via Result<T> / Status; bugs (e.g.
// dimension overflow) are programmer errors and throw.
//
// No dependency on app or CLI types.

#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

namespace xfoil::core {

// General, non-domain-specific error classes.
enum class ErrorCode {
  invalidArgument,
  sizeMismatch,
  outOfRange,
  capacityExceeded,
  invalidState,
  numericalFailure,
  nonConvergence,
  ioError,
  unavailable,
};

// Structured, value-semantic error object.
struct Error {
  ErrorCode code{ErrorCode::invalidArgument};
  std::string message;

  [[nodiscard]]
  static Error make(ErrorCode code, std::string message) {
    return Error{code, std::move(message)};
  }
};

// Value-semantic result holding exactly one of value or error.
// Accessing the value in the error state throws std::logic_error (no
// implicit loss of the error state).
template <class T>
class Result {
 public:
  using value_type = T;

  [[nodiscard]]
  static Result ok(T value) {
    return Result(in_place_ok_t{}, std::move(value));
  }

  [[nodiscard]]
  static Result fail(Error error) {
    return Result(in_place_error_t{}, std::move(error));
  }

  [[nodiscard]]
  bool isOk() const noexcept {
    return storage_.index() == 0;
  }

  [[nodiscard]]
  bool isErr() const noexcept {
    return storage_.index() == 1;
  }

  const T& value() const {
    if (isErr()) {
      throw std::logic_error(
          "xfoil::core::Result: access to value in the error state");
    }
    return std::get<0>(storage_);
  }

  T& value() {
    if (isErr()) {
      throw std::logic_error(
          "xfoil::core::Result: access to value in the error state");
    }
    return std::get<0>(storage_);
  }

  const Error& error() const {
    if (isOk()) {
      throw std::logic_error(
          "xfoil::core::Result: access to error in the success state");
    }
    return std::get<1>(storage_);
  }

 private:
  struct in_place_ok_t {};
  struct in_place_error_t {};

  explicit Result(in_place_ok_t, T value)
      : storage_(std::in_place_index<0>, std::move(value)) {}

  explicit Result(in_place_error_t, Error error)
      : storage_(std::in_place_index<1>, std::move(error)) {}

  std::variant<T, Error> storage_;
};

// Status = Result<void>: no value, only success/error state.
class Status {
 public:
  Status() = default;

  [[nodiscard]]
  static Status ok() noexcept {
    return Status{};
  }

  [[nodiscard]]
  static Status fail(Error error) {
    return Status(std::move(error));
  }

  [[nodiscard]]
  bool isOk() const noexcept {
    return !error_.has_value();
  }

  [[nodiscard]]
  bool isErr() const noexcept {
    return error_.has_value();
  }

  const Error& error() const {
    if (isOk()) {
      throw std::logic_error(
          "xfoil::core::Status: access to error in the success state");
    }
    return *error_;
  }

 private:
  explicit Status(Error error) : error_(std::move(error)) {}

  std::optional<Error> error_;
};

}  // namespace xfoil::core

// End: include/xfoil/core/Result.h
