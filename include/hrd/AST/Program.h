#pragma once

#include "hrd/AST/Decl.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>
// 단일 소스 파일 단위의 AST 컨테이너를 나타낸다.
// 파일 경로와 해당 파일에 포함된 선언 목록을 보유한다.
// decls의 소유권은 SourceFile이 가지며 파싱 결과의 루트 단위로 사용된다.
class SourceFile {
public:
  std::string path;
  std::vector<Decl::Ptr> decls;
  SourceFile(const string &p, std::vector<Decl::Ptr> d)
      : path(p), decls(std::move(d)) {}
};

// 전체 프로그램 단위의 AST 컨테이너를 나타낸다.
// 여러 SourceFile을 묶어 컴파일 단위 전체를 구성한다.
// 각 소스는 shared_ptr로 관리되어 여러 단계에서 공유된다.
struct Program {
  std::vector<shared_ptr<SourceFile>> sources;
};