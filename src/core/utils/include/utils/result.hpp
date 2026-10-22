#pragma once

#include <cassert>
#include <concepts>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace mole {

struct Failure {
  std::string message;
};

/**
 * @brief A class that is to be used instead of throwing errors. If successfull it holds the passed down value,
 * otherwise it holds an error message.
 * @tparam T type of the value
 */
template <typename T>
class [[nodiscard]] Result {
 public:
  Result(T value) : data(std::in_place_index<0>, std::move(value)) {}

  Result(mole::Failure failure) : data(std::in_place_index<1>, std::move(failure.message)) {}

  template <typename U>
  requires std::convertible_to<U, T> && (!std::same_as<std::decay_t<U>, Result<T>>)
  Result(U&& value) : data(std::in_place_index<0>, T(std::forward<U>(value))) {}

  Result(Result&& other) noexcept = default;
  Result& operator=(Result&& other) noexcept = default;

  Result(const Result&) = default;
  Result& operator=(const Result&) = default;

  // Check the index directly instead of holds_alternative
  [[nodiscard]] inline bool is_ok() const noexcept { return data.index() == 0; }
  explicit operator bool() const noexcept { return is_ok(); }

  /**
   * @brief If the result is successful, returns the value, otherwise terminates the program.
   */
  [[nodiscard]] const T& value() const & noexcept {
    assert(is_ok() && "Error: tried to access value of a Result that is failed.");
    return std::get<0>(data); // Retrieve by index
  }

  /**
   * @brief If the result is successful, returns the value, otherwise terminates the program.
   */
  [[nodiscard]] T&& value() && noexcept {
    assert(is_ok() && "Error: tried to access value of a Result that is failed.");
    return std::get<0>(std::move(data)); // Retrieve by index
  }

  /**
   * @brief If the result is failed, returns the error message, otherwise terminates the program.
   */
  [[nodiscard]] const std::string& error() const {
    assert(!is_ok() && "Error: tried to access error of a Result that is successful.");
    return std::get<1>(data); // Retrieve by index
  }

 private:
  std::variant<T, std::string> data;
};

/**
 * @brief A specialization of Result for void.
 */
template <>
class [[nodiscard]] Result<void> {
 public:
  Result() = default;
  Result(Failure failure) : error_message(std::move(failure.message)), success(false) {}

  bool is_ok() const noexcept { return success; }
  explicit operator bool() const noexcept { return success; }

  const std::string& error() const noexcept { return error_message; }

 private:
  std::string error_message = "";
  bool success = true;
};

static inline Result<void> ok() { return Result<void>(); }
static inline Failure fail(std::string error_message) { return Failure{std::move(error_message)}; }
} // mole
