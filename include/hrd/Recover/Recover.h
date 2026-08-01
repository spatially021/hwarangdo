#pragma once

class Failure {};

class Recover {
public:
  virtual ~Recover() = default;
  virtual void recover() = 0;

protected:
  [[noreturn]] static void fail() { throw Failure{}; }
};