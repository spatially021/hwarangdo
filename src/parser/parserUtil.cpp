#include "hrd/AST/ASTNode.h"
#include "hrd/AST/Expr.h"
#include "hrd/Parser.h"
#include "hrd/Token.h"
#include "hrd/diagnostic/Diagnostic.h"
#include "hrd/util/Error.h"
#include <cstddef>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <utility>

bool Parser::isAtEnd() const {
  return current >= tokens.size() || peek().kind == TKind::END;
}

const Token &Parser::consume(TKind kind, DiagnosticCode code,
                             const string &message) {
  if (check(kind))
    return advance();
  auto dia = engine.makeDiagnostic(code);
  dia.labels = {
      {peek().span, message, true},
  };
  engine.emit(dia);
  auto &token = stream.makeSyntheticToken();

  recover.recover();
  return token;
}

bool Parser::match(std::initializer_list<TKind> kinds) {
  for (auto kind : kinds) {
    if (check(kind)) {
      advance();
      return true;
    }
  }
  return false;
}

bool Parser::check(std::initializer_list<TKind> kinds, size_t step) const {
  for (auto kind : kinds) {
    if (check(kind, step)) {
      return true;
    }
  }
  return false;
}

bool Parser::check(TKind kind, size_t step) const {
  if (isAtEnd())
    return false;
  return following(step).kind == kind;
}

const Token &Parser::advance() {
  if (!isAtEnd())
    current++;
  return previous();
}

const Token &Parser::peek() const {
  if (current >= tokens.size()) {
    throw runtime_error("peek out of range");
  }

  return tokens[current];
}

const Token &Parser::previous() const {
  if (current == 0)
    throw runtime_error("No previous token");
  return tokens[current - 1];
}

const Token &Parser::following(size_t step) const {
  if (current + step < tokens.size()) {
    return tokens[current + step];
  }
  throw runtime_error("No following token");
}

bool Parser::isAccessModifier(const size_t offset) const {
  auto k = following(offset).kind;
  return k == TKind::PUBLIC || k == TKind::PRIVATE || k == TKind::PROTECTED;
}

AModifier Parser::AModifierConvertor(Token t) {
  AModifier modi = AModifier::PUBLIC;
  if (t.kind == TKind::PUBLIC)
    modi = AModifier::PUBLIC;
  else if (t.kind == TKind::PRIVATE)
    modi = AModifier::PRIVATE;
  else if (t.kind == TKind::PROTECTED)
    modi = AModifier::PROTECTED;
  return modi;
}

bool Parser::isTypeToken(TKind k) const {
  switch (k) {
  case TKind::INT:
  case TKind::FLOAT:
  case TKind::CHAR:
  case TKind::STRING:
  case TKind::FIXED:
  case TKind::BOOL:
  case TKind::IDENTIFIER:
    return true;
  default:
    return false;
  }
}

bool Parser::isType() {
  if (isAccessModifier()) {
    if (isTypeToken(following().kind)) {
      return true;
    }
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P026);
    dia.labels = {
        {peek().span, "expected type name here", true},
    };
    engine.emit(dia);
    recover.recover();
    return false;
  }
  return isTypeToken(peek().kind);
}

bool Parser::isInit() {
  if (isAccessModifier()) {
    return check(TKind::INIT, 1) && check(TKind::LEFT_PAREN, 2);
  }
  return check(TKind::INIT) && check(TKind::LEFT_PAREN, 1);
}

bool Parser::isFunc() {
  size_t offset = 0;

  while (true) {
    if (isAccessModifier(offset) || check(TKind::STATIC, offset)) {
      ++offset;
      continue;
    }

    if (check(TKind::EXTERN, offset)) {
      ++offset;

      if (following(offset).kind == TKind::LEFT_PAREN) {
        int depth = 0;

        do {
          auto kind = following(offset).kind;

          if (kind == TKind::LEFT_PAREN) {
            ++depth;
          } else if (kind == TKind::RIGHT_PAREN) {
            --depth;
          }

          ++offset;
        } while (depth > 0 && following(offset).kind != TKind::END);
      }

      continue;
    }

    break;
  }

  auto kind = following(offset).kind;

  if (!(isTypeToken(kind) || kind == TKind::VOID)) {
    if (offset != 0) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P026);
      dia.labels = {
          {following(offset).span, "expected type name here", true},
      };

      engine.emit(dia);
      recover.recover();
    }

    return false;
  }

  ++offset;

  if (following(offset).kind != TKind::IDENTIFIER) {
    return false;
  }

  ++offset;

  // Skip method generic parameter list.
  if (following(offset).kind == TKind::LESS) {
    int depth = 0;

    do {
      auto genericKind = following(offset).kind;

      if (genericKind == TKind::LESS) {
        ++depth;
      } else if (genericKind == TKind::GREATER) {
        --depth;
      }

      ++offset;
    } while (depth > 0 && following(offset).kind != TKind::END);

    if (depth != 0) {
      return false;
    }
  }

  return following(offset).kind == TKind::LEFT_PAREN;
}

TypeNode::Ptr Parser::typeNodeConvertor(Token ty, Token size) {
  TypeNode::Ptr node;
  if (ty.kind == TKind::IDENTIFIER) {
    node = make_shared<IdentifierTypeNode>(ty, ty.text);
  } else if (ty.kind == TKind::HANDLE) {
    return nullptr;
  } else {
    switch (ty.kind) {
    case TKind::INT:
      node = make_shared<BuiltinTypeNode>(ty, BuiltinTypeNode::Category::Int,
                                          size);
      break;
    case TKind::FLOAT:
      node = make_shared<BuiltinTypeNode>(ty, BuiltinTypeNode::Category::Float,
                                          size);
      break;
    case TKind::FIXED:
      node = make_shared<BuiltinTypeNode>(ty, BuiltinTypeNode::Category::Fixed,
                                          size);
      break;
    case TKind::BOOL:
      node = make_shared<BuiltinTypeNode>(ty, BuiltinTypeNode::Category::Bool,
                                          size);
      break;
    case TKind::CHAR:
      node = make_shared<BuiltinTypeNode>(ty, BuiltinTypeNode::Category::CHAR,
                                          size);
      break;
    case TKind::STRING:
      node = make_shared<BuiltinTypeNode>(ty, BuiltinTypeNode::Category::STRING,
                                          size);
      break;
    case TKind::VOID:
      node = make_shared<BuiltinTypeNode>(ty, BuiltinTypeNode::Category::Void);
      break;
    case TKind::SIZE:
      node = make_shared<BuiltinTypeNode>(ty);
      // case TKind::FUNC:
      //   node = make_shared<BuiltinTypeNode>(ty,
      //   BuiltinTypeNode::Category::FUNC); break;
      break;
    default: {
      Error::internal(ty, "unexpect type kind : " + ty.text);
    }
    }
  }
  return node;
}

bool Parser::isAssign() const {
  return check({TKind::EQUAL, TKind::PLUS_EQUAL, TKind::MINUS_EQUAL,
                TKind::STAR_EQUAL, TKind::DOUBLE_STAR_EQUAL, TKind::SLASH_EQUAL,
                TKind::PERCENT_EQUAL, TKind::AMPERSAND_EQAUL, TKind::PIPE_EQUAL,
                TKind::CARET_EQUAL, TKind::DOUBLE_ANGLEBUCKET_EQAUL,
                TKind::DOUBLE_RIGHT_ANGLE_BUCKET_EQUAL});
}

bool Parser::isAssginable(Expr::Ptr p) const {
  switch (p->kind) {
  case NKind::NAME_EXPR:
  case NKind::MEMBER_EXPR:
  case NKind::ARRAY_ACCESS_EXPR:
    return true;
  default:
    return false;
  }
}

TypeNode::Ptr Parser::parseType() {
  Token ty = advance(); // 자료형/객체명

  Token size = {};

  if (check(TKind::COLON)) {
    if (ty.kind == TKind::IDENTIFIER) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P027);
      dia.labels = {
          {peek().span, "invalid use of ':'", true},
      };
      engine.emit(dia);
      recover.recover();
    }

    advance(); // :
    size = consume(TKind::SIZE, DiagnosticCode::HRD_P057,
                   "expected type width specifier after ':'");
    assert(size.text != "");

    switch (ty.kind) {
    case TKind::INT:
      if (!(size.text[0] == 'i' || size.text[0] == 'u')) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P028);
        dia.labels = {
            {peek().span, "this type does not support the specified width",
             true},
        };
        engine.emit(dia);
        recover.recover();
      }
      break;
    case TKind::FLOAT:
      if (size.text[0] != 'f') {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P028);
        dia.labels = {
            {peek().span, "this type does not support the specified width",
             true},
        };
        engine.emit(dia);
        recover.recover();
      }

      break;
    case TKind::FIXED:
      break;
    case TKind::CHAR:
      if (size.text[0] != 'c') {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P028);
        dia.labels = {
            {peek().span, "this type does not support the specified width",
             true},
        };
        engine.emit(dia);
        recover.recover();
      }
      break;
    case TKind::STRING:
      if (size.text[0] != 's') {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P028);
        dia.labels = {
            {peek().span, "this type does not support the specified width",
             true},
        };
        engine.emit(dia);
        recover.recover();
      }
      break;
    default:
      Error::internal(size.span, "unreachable parseType");
    }
  }

  TypeNode::Ptr type = typeNodeConvertor(ty, size);

  if (check(TKind::LESS)) {
    type = parseGenericType(ty);
  }

  std::vector<std::pair<Token, Expr::Ptr>> dims;
  while (check(TKind::LEFT_BRACKET)) {
    Token bracket = advance();
    Expr::Ptr sizeExpr = expression();
    consume(TKind::RIGHT_BRACKET, DiagnosticCode::HRD_P053,
            "expected ']' after array size");
    dims.push_back({bracket, std::move(sizeExpr)});
  }

  for (auto it = dims.rbegin(); it != dims.rend(); ++it) {
    type = make_shared<ArrayTypeNode>(it->first, type, std::move(it->second));
  }

  return type;
}

TypeNode::Ptr Parser::parseGenericType(Token base) {
  advance(); // <

  if (check(TKind::GREATER) || check(TKind::DOUBLE_RIGHT_ANGLE_BUCKET)) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P050);
    dia.labels = {
        {peek().span, "expected generic type argument", true},
    };
    engine.emit(dia);
    recover.recover();
  }

  vector<TypeNode::Ptr> args;
  args.push_back(parseType());

  while (check(TKind::COMMA)) {
    advance();

    if (check(TKind::GREATER) || check(TKind::DOUBLE_RIGHT_ANGLE_BUCKET)) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P050);
      dia.labels = {
          {peek().span, "expected type argument after ','", true},
      };
      engine.emit(dia);
      recover.recover();
    }

    args.push_back(parseType());
  }

  if (pendingGenericClose > 0) {
    --pendingGenericClose;
  } else if (check(TKind::GREATER)) {
    advance();
  } else if (check(TKind::DOUBLE_RIGHT_ANGLE_BUCKET)) {
    advance();
    pendingGenericClose = 1;
  } else {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P050);
    dia.labels = {
        {peek().span, "expected '>' after generic type arguments", true},
    };
    engine.emit(dia);
    recover.recover();
  }

  return make_shared<GenericTypeNode>(base, base.text, std::move(args));
}

TypeName Parser::parseTypeName(DiagnosticCode nameDiag, const string &message) {
  TypeName result;

  result.name = consume(TKind::IDENTIFIER, nameDiag, message);

  if (!check(TKind::LESS))
    return result;

  advance(); // <

  result.genericParams.push_back(
      GenericParamDecl{consume(TKind::IDENTIFIER, DiagnosticCode::HRD_P041,
                               "expected generic parameter name")});

  while (check(TKind::COMMA)) {
    advance();

    result.genericParams.push_back(
        GenericParamDecl{consume(TKind::IDENTIFIER, DiagnosticCode::HRD_P041,
                                 "expected generic parameter name")});
  }

  consume(TKind::GREATER, DiagnosticCode::HRD_P050,
          "expected '>' after generic parameters");

  return result;
}

void Parser::notFunc(DeclPrefix prefix) {
  if (prefix.isFrame) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P029);
    dia.labels = {
        {previous().span, "'frame' is only allowed on function declarations",
         true},
    };
    engine.emit(dia);
    recover.recover();
  }

  if (prefix.isOverride) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P030);
    dia.labels = {
        {previous().span, "'override' is only allowed on function declarations",
         true},
    };
    engine.emit(dia);
    recover.recover();
  }

  if (prefix.isAsync) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P031);
    dia.labels = {
        {previous().span, "'async' is only allowed on function declarations",
         true},
    };
    engine.emit(dia);
    recover.recover();
  }

  if (prefix.isExtern) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P072);
    dia.labels = {
        {previous().span, "'extern' is only allowed on function declarations",
         true},
    };
    engine.emit(dia);
    recover.recover();
  }
  if (prefix.isStatic) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P074);
    dia.labels = {
        {previous().span, "'static' is only allowed on function declarations",
         true},
    };
    engine.emit(dia);
    recover.recover();
  }
}

void Parser::notVar(DeclPrefix prefix) {
  if (prefix.isConst) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P015);
    dia.labels = {
        {previous().span, "'const' is not allowed on this declaration", true},
    };
    engine.emit(dia);
    recover.recover();
  }

  if (prefix.isRoot) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P016);
    dia.labels = {
        {previous().span, "'root' is not allowed on this declaration", true},
    };
    engine.emit(dia);
    recover.recover();
  }
}

Expr::Ptr Parser::parseCaseValue() {
  auto value = postfix();
  Expr::Ptr arg = nullptr;
  if (auto call = dynamic_cast<CallExpr *>(value.get())) {
    if (call->arguments.empty()) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P032);
      dia.labels = {
          {previous().span,
           "payload case selector must bind exactly one variable", true},
      };
      engine.emit(dia);
      recover.recover();
    }

    if (call->arguments.size() != 1) {
      auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P032);
      dia.labels = {
          {previous().span,
           "payload case selector must bind exactly one variable", true},
      };
      engine.emit(dia);
      recover.recover();
    }
    arg = call->arguments[0];
  }
  return make_shared<CaseValueExpr>(value->span, value, arg);
}

bool Parser::looksLikeDecl() {
  size_t pos = current;

  auto at = [&](size_t index) -> const Token & {
    if (index >= tokens.size()) {
      return tokens.back();
    }

    return tokens[index];
  };

  // statement()에서 호출되는 시점에는 IDENTIFIER임.
  ++pos;

  // Generic: Type<A>, Type<A<B>>, ...
  if (at(pos).kind == TKind::LESS) {
    size_t depth = 1;
    ++pos;

    while (depth > 0) {
      switch (at(pos).kind) {
      case TKind::LESS:
        ++depth;
        break;

      case TKind::GREATER:
        --depth;
        break;

      case TKind::END:
        return false;

      default:
        break;
      }

      ++pos;
    }
  }

  // Array: Type[], Type[10], Type<T>[], ...
  while (at(pos).kind == TKind::LEFT_BRACKET) {
    size_t depth = 1;
    ++pos;

    while (depth > 0) {
      switch (at(pos).kind) {
      case TKind::LEFT_BRACKET:
        ++depth;
        break;

      case TKind::RIGHT_BRACKET:
        --depth;
        break;

      case TKind::END:
        return false;

      default:
        break;
      }

      ++pos;
    }
  }

  return at(pos).kind == TKind::IDENTIFIER;
}

bool Parser::isGenericArgs() {
  if (!check(TKind::LESS)) {
    return false;
  }

  size_t offset = 0;
  size_t depth = 0;

  while (true) {
    if (check(TKind::END, offset)) {
      return false;
    }

    if (check(TKind::LESS, offset)) {
      ++depth;

    } else if (check(TKind::GREATER, offset)) {
      if (depth == 0) {
        return false;
      }

      --depth;

      if (depth == 0) {
        return true;
      }

    } else if (check(TKind::DOUBLE_RIGHT_ANGLE_BUCKET, offset)) {
      if (depth < 2) {
        return false;
      }

      depth -= 2;

      if (depth == 0) {
        return true;
      }
    }

    ++offset;
  }
}

std::vector<TypeNode::Ptr> Parser::parseGenericArgs() {
  std::vector<TypeNode::Ptr> args;

  consume(TKind::LESS, DiagnosticCode::HRD_P041,
          "expected '<' before generic arguments");

  if (check(TKind::GREATER)) {
    auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P041);
    dia.labels = {
        {peek().span, "generic argument list cannot be empty", true},
    };
    engine.emit(dia);
    recover.recover();
  }

  while (!isAtEnd()) {
    args.push_back(parseType());

    if (check(TKind::COMMA)) {
      advance();

      if (check(TKind::GREATER)) {
        auto dia = engine.makeDiagnostic(DiagnosticCode::HRD_P041);
        dia.labels = {
            {peek().span, "expected type after ','", true},
        };
        engine.emit(dia);
        recover.recover();
      }

      continue;
    }

    break;
  }

  consume(TKind::GREATER, DiagnosticCode::HRD_P041,
          "expected '>' after generic arguments");

  return args;
}

std::vector<Expr::Ptr> Parser::parseCallArgs() {
  consume(TKind::LEFT_PAREN, DiagnosticCode::HRD_P046, "expected '('");

  std::vector<Expr::Ptr> args;

  if (!check(TKind::RIGHT_PAREN)) {
    do {
      args.push_back(ternary());
    } while (match({TKind::COMMA}));
  }

  consume(TKind::RIGHT_PAREN, DiagnosticCode::HRD_P047,
          "expected ')' to close argument list");

  return args;
}