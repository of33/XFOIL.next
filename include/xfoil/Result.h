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

// File: include/xfoil/Result.h
// Public C++20 result and error contract. Expected failures are returned
// as values; no std::expected is required. The type pattern follows the
// internal core result but exposes no internal core or session type.

#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

namespace xfoil {

// General, non-domain-specific error classes.
enum class ErrorCategory {
  invalidArgument,
  invalidState,
  capacityExceeded,
  nonConvergence,
  numericalFailure,
  unsupported,
  cancelled,
  internal
};

// Structured, value-semantic error object.
struct Error {
  ErrorCategory category{ErrorCategory::internal};
  std::string code;
  std::string message;
  std::string context;

  [[nodiscard]]
  static Error make(ErrorCategory category, std::string code,
                    std::string message, std::string context = {}) {
    return Error{category, std::move(code), std::move(message),
                 std::move(context)};
  }
};

// Value-semantic result holding exactly one of value or error.
// Accessing value() in the error state or error() in the success state
// is a programmer error and throws std::logic_error.
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

  [[nodiscard]]
  const T& value() const & {
    requireValue();
    return std::get<0>(storage_);
  }

  [[nodiscard]]
  T& value() & {
    requireValue();
    return std::get<0>(storage_);
  }

  [[nodiscard]]
  T&& value() && {
    requireValue();
    return std::get<0>(std::move(storage_));
  }

  [[nodiscard]]
  const Error& error() const {
    requireError();
    return std::get<1>(storage_);
  }

 private:
  struct in_place_ok_t {};
  struct in_place_error_t {};

  explicit Result(in_place_ok_t, T value)
      : storage_(std::in_place_index<0>, std::move(value)) {}

  explicit Result(in_place_error_t, Error error)
      : storage_(std::in_place_index<1>, std::move(error)) {}

  void requireValue() const {
    if (isErr()) {
      throw std::logic_error(
          "xfoil::Result: access to value in the error state");
    }
  }

  void requireError() const {
    if (isOk()) {
      throw std::logic_error(
          "xfoil::Result: access to error in the success state");
    }
  }

  std::variant<T, Error> storage_;
};

// Result<void>: no value, only success or error state.
template <>
class Result<void> {
 public:
  [[nodiscard]]
  static Result ok() {
    return Result{};
  }

  [[nodiscard]]
  static Result fail(Error error) {
    return Result(std::move(error));
  }

  [[nodiscard]]
  bool isOk() const noexcept {
    return !error_.has_value();
  }

  [[nodiscard]]
  bool isErr() const noexcept {
    return error_.has_value();
  }

  [[nodiscard]]
  const Error& error() const {
    if (isOk()) {
      throw std::logic_error(
          "xfoil::Result: access to error in the success state");
    }
    return *error_;
  }

 private:
  Result() = default;

  explicit Result(Error error) : error_(std::move(error)) {}

  std::optional<Error> error_;
};

}  // namespace xfoil

// End: include/xfoil/Result.h
