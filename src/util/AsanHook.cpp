#include "hrd/util/Error.h"
#include <cstdlib>
#include <stdexcept>

extern "C" void __asan_on_error() {
  // ASan 내부에서 이미 stderr에 일부 출력했을 수 있으므로
  // 우리는 "통합 포맷"만 책임진다.

  Error::fatal(Error::ErrorCategory::Sanitizer,
               "AddressSanitizer detected memory error");

  // Error 시스템으로 통합
  // (throw는 abort 전에 의미 없을 수 있으므로 메시지 목적)
  try {
    throw std::runtime_error("AddressSanitizer detected memory error");
  } catch (...) {
    // ASan은 결국 abort() 하므로 여기서 종료
  }
}
