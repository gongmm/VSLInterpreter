#pragma once
#include "Global.h"
//bool isMain = false;
std::map<std::string, std::unique_ptr<PrototypeAST>> MainLackOfProtos;
bool hasMainFunction=false;
Function *getLackFunction(std::string Name) {
	// First, see if the function has already been added to the current module.
	if (auto *F = TheModule->getFunction(Name))
		return F;
	// If not, check whether we can codegen the declaration from some existing
	// prototype.
	auto FI = MainLackOfProtos.find(Name);
	if (FI != MainLackOfProtos.end())
		return FI->second->codegen();

	// If no existing prototype exists, return null.
	return nullptr;
}
 void processMain(){
			 // JIT the module containing the anonymous expression, keeping a handle so
			 // we can free it later.
			// auto H = TheJIT->addModule(std::move(TheModule));
			 //InitializeModuleAndPassManager();

			 // Search the JIT for the __anon_expr symbol.
			//ÔÝÊ±×¢ÊÍ
			 // auto ExprSymbol = TheJIT->findSymbol("main");
			 //ÔÝÊ±×¢ÊÍ
			// assert(ExprSymbol && "Function not found");

			 // Get the symbol's address and cast it to the right type (takes no
			 // arguments, returns a double) so we can call it as a native function.
			 //ÔÝÊ±×¢ÊÍ
			 //double(*FP)() = (double(*)())(intptr_t)cantFail(ExprSymbol.getAddress());
			 //ÔÝÊ±×¢ÊÍ
			 //fprintf(stderr, "Evaluated to %f\n", FP());

			 // Delete the anonymous expression module from the JIT.
			 //TheJIT->removeModule(H);
}