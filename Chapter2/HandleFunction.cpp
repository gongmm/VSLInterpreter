#pragma once
#include "Global.h"
/*******************
 *                  *
 ** ¥¶¿Ìfunction **
 *                  *
 ********************/
/// prototype
///   ::= id '(' id* ')'
std::unique_ptr<PrototypeAST> ParsePrototype() {
  if (CurTok != VARIABLE)
    return LogErrorP("Expected function name in prototype");

  std::string FnName = IdentifierStr;
  getNextToken();

  if (CurTok != '(')
    return LogErrorP("Expected '(' in prototype");

  std::vector<std::string> ArgNames;
  getNextToken();
  while (CurTok == VARIABLE) {
	ArgNames.push_back(IdentifierStr);
    getNextToken(); 
	if (CurTok == ',')
      getNextToken();
  }

  if (CurTok != ')')
    return LogErrorP("Expected ')' in prototype");

  // success.
  getNextToken(); // eat ')'.

  return llvm::make_unique<PrototypeAST>(FnName, std::move(ArgNames));
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
	  }
  } else {
    // Skip token for error recovery.
//    getNextToken();
  }
}