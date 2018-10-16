#pragma once
#include "Global.h"
#include "llvm/IR/ValueSymbolTable.h"


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
	//�޸�ǰ
	/*// Look up the name in the global module table.
	Function *CalleeF = TheModule->getFunction(Callee);*/
	//�޸ĺ�
	// Look up the name in the global module table.
	Function *CalleeF = getFunction(Callee);

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
	// Ѱ���Ƿ����Ѿ����ڵĺ���
	Function *TheFunction = TheModule->getFunction(Name);

	if (TheFunction)
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
	//���Ӷ�ȫ�ֺ���ԭ�ͱ�FunctionProtos���޸ģ��޸�getFunction�ķ�ʽ
	auto &P = *Proto;
	FunctionProtos[Proto->getName()] = std::move(Proto);
	Function *TheFunction = getFunction(P.getName());
	if (!TheFunction)
		return nullptr;
//��ǰ�İ汾	
/*	Function *TheFunction = Proto->codegen();

	if (!TheFunction)
		return nullptr;*/

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

		// Run the optimizer on the function.
		TheFPM->run(*TheFunction);

		return TheFunction;
	}

	// Error reading body, remove function.
	TheFunction->eraseFromParent();
	return nullptr;
}



Value * AssignStatAST::codegen()
{
	Function *TheFunction = Builder.GetInsertBlock()->getParent();
	// Create a new basic block to start insertion into.
	BasicBlock *BB = BasicBlock::Create(TheContext, "entry", TheFunction);
	ValueSymbolTable* st = BB->getValueSymbolTable();

	Value* v = st->lookup(Name);
	if (v == NULL || v->hasName() == false) {
		errs() << "undeclared variable " << Name << "\n";
		return NULL;
	}
	
	Value * result = this->Val->codegen();

	

	Value* load = new LoadInst(result, "", false, BB);
	return load;
	

	// �� NamedValues map ��Ѱ�Ҹñ���.
	//Value *V = NamedValues[Name];
	//if (!V)
		//return LogErrorV("Unknown variable name");
	// ��ֵ
	//NamedValues[Name] = result;

	//return Builder.CreateStore(result, TheModule->getGlobalVariable(this->Name));
	
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
	//����ѭ����������
	Value *EndCond = WhileCondition->codegen();
	if (!EndCond)
		return nullptr;

	// ���������ı���ʽ��ֵ������бȽ�, �Ӷ�����ֵ��Ϊ1λ (bool) ֵ��ȡ����ȷ��ѭ���Ƿ�Ӧ���˳�
	EndCond = Builder.CreateFCmpONE(EndCond, ConstantFP::get(TheContext, APFloat(0.0)), "whilecond");

	// ��ȡ���ڹ����ĵ�ǰFunction����
	Function *TheFunction = Builder.GetInsertBlock()->getParent();
	// ��������Phi�ڵ�
	BasicBlock *PreheaderBB = Builder.GetInsertBlock();
	// ����ѭ���������
	BasicBlock *LoopBB = BasicBlock::Create(TheContext, "loop", TheFunction);

	// ������ǰ�鵽ѭ�����һ����������֧
	Builder.CreateBr(LoopBB);

	// ��������л��� LoopBB.
	Builder.SetInsertPoint(LoopBB);
	// ����PHI�ڵ�
	PHINode *Variable = Builder.CreatePHI(Type::getDoubleTy(TheContext), 2, "loopend");
	// ����ʼ�ı���ʽ��ֵ����PHI�ڵ㣬ĿǰPreheader��������
	Variable->addIncoming(EndCond, PreheaderBB);
	// ����ѭ����Statement���ֵĴ���
	if (!DoStat->codegen())
		return nullptr;

	//����ѭ����������
	EndCond = WhileCondition->codegen();
	if (!EndCond)
		return nullptr;

	// ���������ı���ʽ��ֵ������бȽ�, �Ӷ�����ֵ��Ϊ1λ (bool) ֵ��ȡ����ȷ��ѭ���Ƿ�Ӧ���˳�
	EndCond = Builder.CreateFCmpONE(EndCond, ConstantFP::get(TheContext, APFloat(0.0)), "whilecond");

	// ��ס������
	BasicBlock *LoopEndBB = Builder.GetInsertBlock();
	// ������ѭ���˳��������飬������
	BasicBlock *AfterBB = BasicBlock::Create(TheContext, "afterloop", TheFunction);

	// ����ѭ��������������������֧
	Builder.CreateCondBr(EndCond, LoopBB, AfterBB);

	// �κ�֮��Ĵ���ᱻ���뵽AfterBB�У����Խ���������õ�AfterBB
	Builder.SetInsertPoint(AfterBB);

	// ΪPHI�ڵ�������ֵ
	Variable->addIncoming(EndCond, LoopEndBB);


	// whileѭ���Ĵ����������Ƿ���0.0
	return Constant::getNullValue(Type::getDoubleTy(TheContext));
}


Value * BlockStatAST::codegen()
{
	// delcaration
	Function *TheFunction = Builder.GetInsertBlock()->getParent();
	// Create a new basic block to start insertion into.
	BasicBlock *BB = BasicBlock::Create(TheContext, "entry", TheFunction);
	ValueSymbolTable* st = BB->getValueSymbolTable();
	Builder.SetInsertPoint(BB);
	//locals.insert(std::make_pair("",0));
	//Value* v = st->lookup(Name);
	
	unsigned start = NamedValues.size();
	unsigned e = Variables.size();
	unsigned end = start + e;
	for (unsigned i = 0; i != e; ++i)
	{
	
		//for (auto &Arg : Variables)
			//NamedValues[Arg.getName()] = &Arg;
		//llvm::Value *ret = Variables[i]->codegen();
		//NamedValues.put(Variables.,0);
		
	}
	

	// statements code generation
	
	Value *ret;
	for (unsigned i = 0, e = Statements.size(); i != e; ++i) {
		ret = Statements[i]->codegen();
		
	}


	// clear variable
	for (unsigned i = start; i != end; ++i)
	{
		//std::unique_ptr<VariableExprAST> result = Variables[i];
		//NamedValues.erase();


	}
	

	return nullptr;
}

Value * ContinueStatAST::codegen()
{
	return nullptr;
}