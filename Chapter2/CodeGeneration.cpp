#pragma once
#include "Global.h"


using namespace llvm;

//===----------------------------------------------------------------------===//
// Code Generation
//===----------------------------------------------------------------------===//

//LLVMContext TheContext;
static IRBuilder<> Builder(TheContext);
//extern std::unique_ptr<Module> TheModule;
//std::map<std::string, Value *> NamedValues;

Value *LogErrorV(const char *Str) {
	LogError(Str);
	return nullptr;
}

Value *NumberExprAST::codegen() {
	//return ConstantInt::get(Builder.getInt32Ty(), this->Val, true);
	return ConstantFP::get(TheContext, APFloat(Val));
}

Value *VariableExprAST::codegen() {
	// Look this variable up in the function.
	Value *V = NamedValues[Name];
	if (!V)
		return LogErrorV("Unknown variable name");
	return V;
}


Value * TextExprAST::codegen()
{
	//return Constant::get(TheContext, StringRef(Text));
	return nullptr;
	
}




Value *BinaryExprAST::codegen() {
	Value *L = LHS->codegen();
	Value *R = RHS->codegen();
	if (!L || !R)
		return nullptr;

	switch (Op) {
	case '+':
		return Builder.CreateFAdd(L, R, "addtmp");
	case '-':
		return Builder.CreateFSub(L, R, "subtmp");
	case '*':
		return Builder.CreateFMul(L, R, "multmp");
	case '/':
		return Builder.CreateFDiv(L, R, "divtmp");
	//case '<':
	//	L = Builder.CreateFCmpULT(L, R, "cmptmp");
	//	// Convert bool 0/1 to double 0.0 or 1.0
	//	return Builder.CreateUIToFP(L, Type::getDoubleTy(TheContext), "booltmp");
	default:
		return LogErrorV("invalid binary operator");
	}
}

Value *CallExprAST::codegen() {
	// Look up the name in the global module table.
	Function *CalleeF = TheModule->getFunction(Callee);
	if (!CalleeF)
		return LogErrorV("Unknown function referenced");

	// If argument mismatch error.
	if (CalleeF->arg_size() != Args.size())
		return LogErrorV("Incorrect # arguments passed");

	std::vector<Value *> ArgsV;
	for (unsigned i = 0, e = Args.size(); i != e; ++i) {
		ArgsV.push_back(Args[i]->codegen());
		if (!ArgsV.back())
			return nullptr;
	}

	return Builder.CreateCall(CalleeF, ArgsV, "calltmp");
}

Function *PrototypeAST::codegen() {
	// 寻找是否有已经存在的函数
	Function *TheFunction = TheModule->getFunction(Name);

	if (!TheFunction->empty())
		return (Function*)LogErrorV("Prototype already exist.");

	// Make the function type:  double(double,double) etc.
	std::vector<Type *> Doubles(Args.size(), Type::getDoubleTy(TheContext));
	FunctionType *FT =
		FunctionType::get(Type::getDoubleTy(TheContext), Doubles, false);

	// create function
	Function *F =
		Function::Create(FT, Function::InternalLinkage, Name, TheModule.get());

	// Set names for all arguments.
	unsigned Idx = 0;
	for (auto &Arg : F->args())
		Arg.setName(Args[Idx++]);

	return F;
}

Function *FunctionAST::codegen() {
	
	
	Function *TheFunction = TheFunction = Proto->codegen();

	if (!TheFunction)
		return nullptr;

	// Create a new basic block to start insertion into.
	BasicBlock *BB = BasicBlock::Create(TheContext, "entry", TheFunction);
	Builder.SetInsertPoint(BB);

	// Record the function arguments in the NamedValues map.
	NamedValues.clear();
	for (auto &Arg : TheFunction->args())
		NamedValues[Arg.getName()] = &Arg;

	if (Value *RetVal = Body->codegen()) {
		// Finish off the function.
		Builder.CreateRet(RetVal);

		// Validate the generated code, checking for consistency.
		verifyFunction(*TheFunction);

		return TheFunction;
	}

	// Error reading body, remove function.
	TheFunction->eraseFromParent();
	return nullptr;
}



Value * AssignStatAST::codegen()
{

	Value * result = this->Val->codegen();

	// 在 NamedValues map 中寻找该变量.
	Value *V = NamedValues[Name];
	if (!V)
		return LogErrorV("Unknown variable name");
	// 赋值
	NamedValues[Name] = result;


	return Builder.CreateStore(result, TheModule->getGlobalVariable(this->Name));
	
}

Value * ReturnStatAST::codegen()
{
	Value * val = Body->codegen();
	return val;
}

Value * PrintStatAST::codegen()
{
	return nullptr;
}

Value * IfStatAST::codegen()
{
	Value *CondV = IfCondition -> codegen();
	if (!CondV)
		return nullptr;

	// Convert condition to a bool by comparing non-equal to 0.0.
	CondV = Builder.CreateFCmpONE(CondV, ConstantFP::get(TheContext, APFloat(0.0)), "ifcond");

	// Get the current Function object that is being built
	Function *TheFunction = Builder.GetInsertBlock()->getParent();

	// Create blocks for the then and else cases.  
	// Insert the 'then' block at the end of the function.
	BasicBlock *ThenBB = BasicBlock::Create(TheContext, "then", TheFunction);
	BasicBlock *ElseBB = BasicBlock::Create(TheContext, "else");
	BasicBlock *MergeBB = BasicBlock::Create(TheContext, "ifcont");

	Builder.CreateCondBr(CondV, ThenBB, ElseBB);

	// Emit then value.
	Builder.SetInsertPoint(ThenBB);

	Value *ThenV = ThenStat -> codegen();
	if (!ThenV)
		return nullptr;

	Builder.CreateBr(MergeBB);
	// Codegen of 'Then' can change the current block, update ThenBB for the PHI.
	ThenBB = Builder.GetInsertBlock();

	PHINode *PN = Builder.CreatePHI(Type::getDoubleTy(TheContext), 2, "iftmp");

	if (!ElseStat) {
		// Emit else block.
		TheFunction->getBasicBlockList().push_back(ElseBB);
		Builder.SetInsertPoint(ElseBB);

		Value *ElseV = ElseStat->codegen();
		if (!ElseV)
			return nullptr;

		Builder.CreateBr(MergeBB);
		// codegen of 'Else' can change the current block, update ElseBB for the PHI.
		ElseBB = Builder.GetInsertBlock();

		PN->addIncoming(ElseV, ElseBB);
	}
	

	// Emit merge block.
	TheFunction->getBasicBlockList().push_back(MergeBB);
	Builder.SetInsertPoint(MergeBB);
	

	PN->addIncoming(ThenV, ThenBB);
	
	return PN;
}

Value * WhileStatAST::codegen()
{
	Value *EndCond = WhileCondition -> codegen();
	if (!EndCond)
		return nullptr;

	// Convert condition to a bool by comparing non-equal to 0.0.
	EndCond = Builder.CreateFCmpONE(EndCond, ConstantFP::get(TheContext, APFloat(0.0)), "whilecond");

	// Make the new basic block for the loop header, inserting after current block.
	Function *TheFunction = Builder.GetInsertBlock()->getParent();
	// BasicBlock *PreheaderBB = Builder.GetInsertBlock();
	BasicBlock *LoopBB = BasicBlock::Create(TheContext, "loop", TheFunction);

	// Insert an explicit fall through from the current block to the LoopBB.
	Builder.CreateBr(LoopBB);

	// Start insertion in LoopBB.
	Builder.SetInsertPoint(LoopBB);

	// Start the PHI node with an entry for Start.
	// PHINode *Variable = Builder.CreatePHI(Type::getDoubleTy(TheContext), 2, "whiletmp");
	// Variable->addIncoming(StartVal, PreheaderBB);

	// Within the loop, the variable is defined equal to the PHI node.  If it shadows an existing variable, we have to restore it, so save it now.
	// Value *OldVal = NamedValues[VarName];
	// NamedValues[VarName] = Variable;

	// Emit the body of the loop.  This, like any other expr, can change the
	// current BB.  Note that we ignore the value computed by the body, but don't
	// allow an error.
	if (!DoStat -> codegen())
		return nullptr;

	// Emit the step value.
	//Value *StepVal = nullptr;
	//if (Step) {
	//	StepVal = Step->codegen();
	//	if (!StepVal)
	//		return nullptr;
	//}
	//else {
	//	// If not specified, use 1.0.
	//	StepVal = ConstantFP::get(TheContext, APFloat(1.0));
	//}

	//Value *NextVar = Builder.CreateFAdd(Variable, StepVal, "nextvar");

	// Compute the end condition.
	/*Value *EndCond = End->codegen();
	if (!EndCond)
		return nullptr;*/

	// Convert condition to a bool by comparing non-equal to 0.0.
	EndCond = Builder.CreateFCmpONE( EndCond, ConstantFP::get(TheContext, APFloat(0.0)), "loopcond");

	// Create the "after loop" block and insert it.
	BasicBlock *LoopEndBB = Builder.GetInsertBlock();
	BasicBlock *AfterBB = BasicBlock::Create(TheContext, "afterloop", TheFunction);

	// Insert the conditional branch into the end of LoopEndBB.
	Builder.CreateCondBr(EndCond, LoopBB, AfterBB);

	// Any new code will be inserted in AfterBB.
	Builder.SetInsertPoint(AfterBB);

	// Add a new entry to the PHI node for the backedge.
	// Variable->addIncoming(NextVar, LoopEndBB);

	// Restore the unshadowed variable.
	/*if (OldVal)
		NamedValues[VarName] = OldVal;
	else
		NamedValues.erase(VarName);*/

	// for expr always returns 0.0.
	return Constant::getNullValue(Type::getDoubleTy(TheContext));
}


Value * BlockStatAST::codegen()
{
	return nullptr;
}

Value * ContinueStatAST::codegen()
{
	return nullptr;
}