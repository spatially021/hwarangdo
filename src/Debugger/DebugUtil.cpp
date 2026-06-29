#include "hrd/Debugger/DebuggerUtil.h"
#include <iostream>
using namespace std;

void DebugUtil::debugLiteral(const ResolvedLit &lit) {
  if (lit.isBool()) {
    cout << (lit.asBool() ? "true" : "false");
    return;
  }

  if (lit.isInt()) {
    std::string s;
    llvm::raw_string_ostream os(s);
    lit.asInt().value.print(os, true);
    cout << os.str();
    return;
  }

  if (lit.isFloat()) {
    llvm::SmallVector<char, 32> buffer;
    lit.asFloat().value.toString(buffer);
    cout << std::string(buffer.begin(), buffer.end());
    return;
  }

  if (lit.isChar()) {
    cout << "U+";
    cout << std::hex << std::uppercase << lit.asChar().codePoint;
    cout << std::dec << std::nouppercase;
    return;
  }

  if (lit.isString()) {
    cout << "\"";

    for (uint32_t cp : lit.asString().codePoints) {
      if (cp >= 32 && cp <= 126 && cp != '"' && cp != '\\') {
        cout << static_cast<char>(cp);
      } else {
        cout << "\\u{";
        cout << std::hex << std::uppercase << cp;
        cout << std::dec << std::nouppercase;
        cout << "}";
      }
    }

    cout << "\"";
    return;
  }

  cout << "<unknown-lit>";
}
