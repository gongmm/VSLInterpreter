#pragma once
#include "Global.h"
/*******************
 *                  *
 ** 处理function **
 *                  *
 ********************/
/// prototype
///   ::= id '(' id* ')'
std::unique_ptr<PrototypeAST> ParsePrototype() {
  std::string FnName;

  unsigned Kind = 0; // 0 为函数, 1 为单目操作符, 2 为双目操作符.
  unsigned BinaryPrecedence = 30; //存储操作符优先级

  switch (CurTok) {
  default:
    return LogErrorP("Expected function name in prototype");
  case VARIABLE: // 为函数的情况
    FnName = IdentifierStr;
    Kind = 0;
    getNextToken();
    break;
  case UNARY: // 为单目操作符的情况
    getNextToken();
    if (!isascii(CurTok))
      return LogErrorP("Expected unary operator");
    FnName = "unary";
    FnName += (char)CurTok; // 函数名称为”unary”+操作符字节
    Kind = 1;
    getNextToken();
    break;
  case BINARY: // 为双目操作符的情况
    getNextToken();
    if (!isascii(CurTok))
      return LogErrorP("Expected binary operator");
    FnName = "binary";
    FnName += (char)CurTok; // 函数名称为”binary”+操作符字节
    Kind = 2;
    getNextToken();

    //读取优先级数字
    if (CurTok == INTEGER) {
      if (NumVal < 1 || NumVal > 100)
        return LogErrorP("Invalid precedence: must be 1..100");
      BinaryPrecedence = (unsigned)NumVal;
      getNextToken();
    }
    break;
  }
  // 处理函数或操作符参数
  if (CurTok != '(')
    return LogErrorP("Expected '(' in prototype");

  std::vector<std::string> ArgNames;
  while (getNextToken() == VARIABLE)
    ArgNames.push_back(IdentifierStr);
  if (CurTok != ')')
    return LogErrorP("Expected ')' in prototype");

  // success.
  getNextToken(); // eat ')'.

  // 判断参数个数是否与操作符类型匹配
  if (Kind && ArgNames.size() != Kind)
    return LogErrorP("Invalid number of operands for operator");

  return llvm::make_unique<PrototypeAST>(FnName, std::move(ArgNames), Kind != 0,
                                         BinaryPrecedence);
}

/// definition ::= 'def' prototype expression
std::unique_ptr<FunctionAST> ParseDefinition() {
  outputToTxt("FUNCTION\n");
  indent = 1;
  getNextToken(); // eat FUNC
  auto Proto = ParsePrototype();
  if (!Proto)
    return nullptr;

  if (auto S = ParseStatement())
    return llvm::make_unique<FunctionAST>(std::move(Proto), std::move(S));

   return nullptr;
}

void HandleDefinition() {
  if (auto FnAST = ParseDefinition()) {
    //fprintf(stderr, "Parsed a function definition.\n");
    /*outputToTxt("FUNCTION.");*/
	  if (auto *FnIR = FnAST->codegen()) {
		  fprintf(stderr, "Read function definition:");
		  FnIR->print(errs());
		  fprintf(stderr, "\n");
		  //将函数生成code后初始化Module和PassManager
		  TheJIT->addModule(std::move(TheModule));
		  InitializeModuleAndPassManager();
		  if (hasMainFunction&&MainLackOfProtos.size() == 0) {
			  processMain();
			  hasMainFunction = false;
		  }
	  }
  } else {
    // Skip token for error recovery.
//    getNextToken();
  }
}