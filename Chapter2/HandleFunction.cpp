#pragma once
#include "Global.h"
/*******************
 *                  *
 ** ����function **
 *                  *
 ********************/
/// prototype
///   ::= id '(' id* ')'
std::unique_ptr<PrototypeAST> ParsePrototype() {
  std::string FnName;

  unsigned Kind = 0; // 0 Ϊ����, 1 Ϊ��Ŀ������, 2 Ϊ˫Ŀ������.
  unsigned BinaryPrecedence = 30; //�洢���������ȼ�

  switch (CurTok) {
  default:
    return LogErrorP("Expected function name in prototype");
  case VARIABLE: // Ϊ���������
    FnName = IdentifierStr;
    Kind = 0;
    getNextToken();
    break;
  case UNARY: // Ϊ��Ŀ�����������
    getNextToken();
    if (!isascii(CurTok))
      return LogErrorP("Expected unary operator");
    FnName = "unary";
    FnName += (char)CurTok; // ��������Ϊ��unary��+�������ֽ�
    Kind = 1;
    getNextToken();
    break;
  case BINARY: // Ϊ˫Ŀ�����������
    getNextToken();
    if (!isascii(CurTok))
      return LogErrorP("Expected binary operator");
    FnName = "binary";
    FnName += (char)CurTok; // ��������Ϊ��binary��+�������ֽ�
    Kind = 2;
    getNextToken();

    //��ȡ���ȼ�����
    if (CurTok == INTEGER) {
      if (NumVal < 1 || NumVal > 100)
        return LogErrorP("Invalid precedence: must be 1..100");
      BinaryPrecedence = (unsigned)NumVal;
      getNextToken();
    }
    break;
  }
  // �����������������
  if (CurTok != '(')
    return LogErrorP("Expected '(' in prototype");

  std::vector<std::string> ArgNames;
  while (getNextToken() == VARIABLE)
    ArgNames.push_back(IdentifierStr);
  if (CurTok != ')')
    return LogErrorP("Expected ')' in prototype");

  // success.
  getNextToken(); // eat ')'.

  // �жϲ��������Ƿ������������ƥ��
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
		  //����������code���ʼ��Module��PassManager
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