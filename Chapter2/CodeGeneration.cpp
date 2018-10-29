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

Function *LogErrorF(const char *Str) {
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
	//修改前
	/*// Look up the name in the global module table.
	Function *CalleeF = TheModule->getFunction(Callee);*/
	//修改后
	// Look up the name in the global module table.
	Function *CalleeF = getFunction(Callee);
	if (isMain&&CalleeF==nullptr) {
		std::vector<std::string> ArgNames;
		//暂时存储名称，名称名为错误值，到真正遇到函数时再加
		for (int i = 0; i < Args.size();i++) {
			ArgNames.push_back("temp"+i);
		}
		MainLackOfProtos[Callee]= llvm::make_unique<PrototypeAST>(Callee, std::move(ArgNames));
		//存在问题：若是下次在取该值名称不一致怎么办！！！
		CalleeF = getLackFunction(Callee);
	}
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
	//添加对全局函数原型表FunctionProtos的修改，修改getFunction的方式
	auto &P = *Proto;
	//添加关于main的逻辑-------------------------------------------
	if (P.getName() == "main")
		isMain = true;
	//先检查是否已经出现了Main函数
	Function *TheFunction;
	std::unique_ptr<PrototypeAST> temp;
	if (hasMainFunction) {
		temp = std::move(MainLackOfProtos[Proto->getName()]);
	}
	if (hasMainFunction&&temp != nullptr) {
		auto& args = temp->getArgs();
		auto& Args = P.getArgs();
		if (args.size() != Args.size()) {
			//参数不一致，报错返回
			return LogErrorF("main函数调用函数参数不一致");
		}
		P.setArgs(args);
		FunctionProtos[Proto->getName()] = std::move(temp);
		MainLackOfProtos.erase(Proto->getName());
		TheFunction = getFunction(P.getName());
		if (!TheFunction)
			return nullptr;
		unsigned Idx = 0;
		for (auto &Arg : TheFunction->args())
			Arg.setName(Args[Idx++]);
	}
	//添加关于main的逻辑-------------------------------------------
	else {
		FunctionProtos[Proto->getName()] = std::move(Proto);
		TheFunction = getFunction(P.getName());
		if (!TheFunction)
			return nullptr;
	}
//以前的版本	
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

		//更新isMain值
		if (P.getName() == "main") {
			isMain = false;
			hasMainFunction = true;
		}

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
	

	// 在 NamedValues map 中寻找该变量.
	//Value *V = NamedValues[Name];
	//if (!V)
		//return LogErrorV("Unknown variable name");
	// 赋值
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
	//处理循环控制条件
	Value *StartVal = WhileCondition->codegen();
	if (!StartVal)
		return nullptr;

	// 将该条件的表达式的值与零进行比较, 从而将真值作为1位 (bool) 值获取。以确定循环是否应该退出
	StartVal = Builder.CreateFCmpONE(StartVal, ConstantFP::get(TheContext, APFloat(0.0)), "whilecond");

	// 获取正在构建的当前Function对象
	Function *TheFunction = Builder.GetInsertBlock()->getParent();
	// 用来创建Phi节点
	BasicBlock *PreheaderBB = Builder.GetInsertBlock();
	// 创建循环体基本块
	BasicBlock *LoopBB = BasicBlock::Create(TheContext, "loop", TheFunction);

	// 创建当前块到循环体的一个无条件分支
	Builder.CreateBr(LoopBB);

	// 将插入点切换到 LoopBB.
	Builder.SetInsertPoint(LoopBB);
	// 创建PHI节点
	PHINode *Variable = Builder.CreatePHI(Type::getDoubleTy(TheContext), 2, "loopend");
	// 将初始的表达式的值传入PHI节点，目前Preheader还不存在
	Variable->addIncoming(StartVal, PreheaderBB);
	// 生成循环体Statement部分的代码
	if (!DoStat->codegen())
		return nullptr;

	//处理循环控制条件
	Value *EndCond = WhileCondition->codegen();
	if (!EndCond)
		return nullptr;

	// 将该条件的表达式的值与零进行比较, 从而将真值作为1位 (bool) 值获取。以确定循环是否应该退出
	EndCond = Builder.CreateFCmpONE(EndCond, ConstantFP::get(TheContext, APFloat(0.0)), "whilecond");

	// 记住结束块
	BasicBlock *LoopEndBB = Builder.GetInsertBlock();
	// 创建“循环退出”基本块，并插入
	BasicBlock *AfterBB = BasicBlock::Create(TheContext, "afterloop", TheFunction);

	// 根据循环控制条件创建条件分支
	Builder.CreateCondBr(EndCond, LoopBB, AfterBB);

	// 任何之后的代码会被插入到AfterBB中，所以将插入点设置到AfterBB
	Builder.SetInsertPoint(AfterBB);

	// 为PHI节点设置新值
	Variable->addIncoming(EndCond, LoopEndBB);


	// while循环的代码生成总是返回0.0
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