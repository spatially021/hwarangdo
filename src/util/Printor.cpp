#include "hrd/util/Printor.h"
#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <iostream>

void Printor::printStackTrace() {
  const int maxFrames = 20;
  void *array[maxFrames];
  int size = backtrace(array, maxFrames);

  std::cerr << "Stack trace (" << size << " frames):\n";

  for (int i = 1; i < size; ++i) {
    Dl_info info;
    if (dladdr(array[i], &info)) {
      const char *symname = info.dli_sname;

      int status = 0;
      char *demangled = abi::__cxa_demangle(symname, nullptr, nullptr, &status);
      std::cerr << "#" << i << " ";
      if (status == 0 && demangled) {
        std::cerr << demangled;
        free(demangled);
      } else {
        std::cerr << symname;
      }

      // 라이브러리/실행파일 정보
      std::cerr << " at " << info.dli_fname;

      // 포인터 주소
      std::cerr << " [" << array[i] << "]\n";
    } else {
      // dladdr 실패시 포인터만 출력
      std::cerr << "#" << i << " " << array[i] << "\n";
    }
  }
}