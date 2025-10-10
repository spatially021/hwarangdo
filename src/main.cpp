#include <iostream>
#include "Lexer.h"
#include "Parser.h"

int main() {
    std::cout << "입력식: ";
    std::string input;
    std::getline(std::cin, input);

    try {
        Lexer lexer(input);
        std::vector<Token> toks = lexer.tokenize();

        Parser parser(toks);
        double result = parser.parseExpr();

        std::cout << "결과: " << result << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "에러: " << e.what() << std::endl;
    }
}
