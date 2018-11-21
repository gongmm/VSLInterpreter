#pragma once
#include "Global.h"
//#include "llvm/Transforms/InstCombine/InstCombine.h"
void InitializeModule() {
	// Open a new module.
	TheModule = llvm::make_unique<Module>("my cool jit", TheContext);
	//TheModule->setDataLayout(TheJIT->getTargetMachine().createDataLayout());

	// Create a new pass manager attached to it.
	//TheFPM = llvm::make_unique<legacy::FunctionPassManager>(TheModule.get());
    
//    TheFPM->add(new DataLayoutPass());
//
//    TheFPM->add(createBasicAliasAnalysisPass());
//
//    TheFPM->add(createPromoteMemoryToRegisterPass());

	// Do simple "peephole" optimizations and bit-twiddling optzns.
//	TheFPM->add(createInstructionCombiningPass());
	// Reassociate expressions.
//	TheFPM->add(createReassociatePass());
	// Eliminate Common SubExpressions.
//	TheFPM->add(createGVNPass());
	// Simplify the control flow graph (deleting unreachable blocks, etc).
//	TheFPM->add(createCFGSimplificationPass());

	//TheFPM->doInitialization();
}
Function *getFunction(std::string Name) {
	// First, see if the function has already been added to the current module.
	if (auto *F = TheModule->getFunction(Name))
		return F;

	// If not, check whether we can codegen the declaration from some existing
	// prototype.
	auto FI = FunctionProtos.find(Name);
	if (FI != FunctionProtos.end())
		return FI->second->codegen();

	// If no existing prototype exists, return null.
	return nullptr;
}
