#pragma once

#include <string>

enum class ErrorKind {
  RECOVERABLE,
  FATEL,
};
struct ErrorPayload {

  ErrorKind kind;
  std::string message = "";
};
