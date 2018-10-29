#include "Global.h"

#include <fstream>

using namespace llvm;



/// top ::= definition | external | expression | ';'
void MainLoop() {
  while (1) {
    /*fprintf(stderr, "ready> ");*/
    switch (CurTok) {
    case TOKEOF:
		if (hasMainFunction)
			processMain();
      return;
    case FUNC:
      HandleDefinition();
      break;
    default:
      LogErrorP("Expected 'FUNC' ");
      return;
    }
  }
}

//===----------------------------------------------------------------------===//
// Main driver code.
//===----------------------------------------------------------------------===//

int main() {
	//��ʼ��
	InitializeNativeTarget();
	InitializeNativeTargetAsmPrinter();
	InitializeNativeTargetAsmParser();
  // Install standard binary operators.
  // 1 is lowest precedence.
  BinopPrecedence['<'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40; // highest.
  BinopPrecedence['/'] = 40; // highest.


  // Prime the first token.
  fprintf(stderr, "ready> ");
  getNextToken();
  //��ʼ��TheJIT���Ż���
  TheJIT = llvm::make_unique<KaleidoscopeJIT>();

  InitializeModuleAndPassManager();
  // Make the module, which holds all the code.
  // TheModule = llvm::make_unique<Module>("my cool jit", TheContext);

  // Run the main "interpreter loop" now.
  MainLoop();

  // Print out all of the generated code.
  TheModule->print(errs(), nullptr);

  return 0;
}
