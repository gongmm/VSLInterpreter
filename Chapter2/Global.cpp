#pragma once
#include "Global.h"

std::string IdentifierStr;
double NumVal;
std::string Text;


//===-------------------
// Parser
//===--------------------
int CurTok = 0;
int getNextToken() { return CurTok = gettok(); }

/// BinopPrecedence - This holds the precedence for each binary operator that is
/// defined.
std::map<char, int> BinopPrecedence;

/// LogError* - These are little helper functions for error handling.
std::unique_ptr<ExprAST> LogError(const char *Str) {
	fprintf(stderr, "Error: %s\n", Str);
	return nullptr;
}
std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
	LogError(Str);
	return nullptr;
}
//新添加Statement报错函数
std::unique_ptr<StatAST> LogErrorS(const char *Str) {
	LogError(Str);
	return nullptr;
}