#pragma once

class Failure {};

class Recover {
public:
  virtual ~Recover() = default;

protected:
  [[noreturn]] static void fail() { throw Failure{}; }
};