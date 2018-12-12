#pragma once
#include "DebugInfo.h"
#include <fstream>

using namespace llvm;
using namespace llvm::orc;
// using namespace llvm::sys;

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

// extern std::unique_ptr<DIBuilder> DBuilder;
extern DebugInfo KSDbgInfo;

int main() {
  //初始化
  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();
  // Install standard binary operators.
  // 1 是最小的优先级
  BinopPrecedence['='] = 2;
  BinopPrecedence['<'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40; // highest.
  BinopPrecedence['/'] = 40; // highest.

  // Prime the first token.
  // fprintf(stderr, "ready> ");
  getNextToken();
  //初始化TheJIT和优化器
  TheJIT = llvm::make_unique<KaleidoscopeJIT>();

  InitializeModule();
  // Make the module, which holds all the code.
  // TheModule = llvm::make_unique<Module>("my cool jit", TheContext);
  // Add the current debug info version into the module.
  TheModule->addModuleFlag(Module::Warning, "Debug Info Version",
                           DEBUG_METADATA_VERSION);
  // Darwin only supports dwarf2.
  if (Triple(sys::getProcessTriple()).isOSDarwin())
    TheModule->addModuleFlag(llvm::Module::Warning, "Dwarf Version", 2);

  // Construct the DIBuilder, we do this here because we need the module.
  DBuilder = llvm::make_unique<DIBuilder>(*TheModule);

  // Create the compile unit for the module.
  // Currently down as "fib.ks" as a filename since we're redirecting stdin
  // but we'd like actual source locations.
  KSDbgInfo.TheCU = DBuilder->createCompileUnit(
      dwarf::DW_LANG_C, DBuilder->createFile("fib", "."), "VSL Compiler", 0, "",
      0);
  // Run the main "interpreter loop" now.
  MainLoop();

  // Finalize the debug info.
  DBuilder->finalize();

  // Print out all of the generated code.
  TheModule->print(errs(), nullptr);

  // Initialize the target registry etc.
  //  InitializeAllTargetInfos();
  //  InitializeAllTargets();
  //  InitializeAllTargetMCs();
  //  InitializeAllAsmParsers();
  //  InitializeAllAsmPrinters();
  //
  //  auto TargetTriple = sys::getDefaultTargetTriple();
  //  TheModule->setTargetTriple(TargetTriple);
  //
  //  std::string Error;
  //  auto Target = TargetRegistry::lookupTarget(TargetTriple, Error);
  //
  //  // Print an error and exit if we couldn't find the requested target.
  //  // This generally occurs if we've forgotten to initialise the
  //  // TargetRegistry or we have a bogus target triple.
  //  if (!Target) {
  //      errs() << Error;
  //      return 1;
  //  }
  //
  //  auto CPU = "generic";
  //  auto Features = "";
  //
  //  TargetOptions opt;
  //  auto RM = Optional<Reloc::Model>();
  //  auto TheTargetMachine =
  //      Target->createTargetMachine(TargetTriple, CPU, Features, opt, RM);
  //
  //  TheModule->setDataLayout(TheTargetMachine->createDataLayout());
  //
  //  auto Filename = "output.o";
  //  std::error_code EC;
  //  raw_fd_ostream dest(Filename, EC, sys::fs::F_None);
  //
  //  if (EC) {
  //      errs() << "Could not open file: " << EC.message();
  //      return 1;
  //  }
  //
  //  legacy::PassManager pass;
  //  auto FileType = TargetMachine::CGFT_ObjectFile;
  //
  //  if (TheTargetMachine->addPassesToEmitFile(pass, dest, nullptr, FileType))
  //  {
  //      errs() << "TheTargetMachine can't emit a file of this type";
  //      return 1;
  //  }
  //
  //  pass.run(*TheModule);
  //  dest.flush();
  //
  //  outs() << "Wrote " << Filename << "\n";
  return 0;
}
