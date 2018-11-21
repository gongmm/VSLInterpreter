#pragma once
#include "Global.h"
#include "llvm/IR/ValueSymbolTable.h"


using namespace llvm;
using namespace llvm::sys;
//===----------------------------------------------------------------------===//
// Code Generation
//===----------------------------------------------------------------------===//

//LLVMContext TheContext;
//IRBuilder<> Builder(TheContext);
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

/// CreateEntryBlockAlloca - Create an alloca instruction in the entry block of
/// the function.  This is used for mutable variables etc.
static AllocaInst *CreateEntryBlockAlloca(Function *TheFunction,
	const std::string &VarName) {
	IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
		TheFunction->getEntryBlock().begin());
	return TmpB.CreateAlloca(Type::getDoubleTy(TheContext), nullptr, VarName.c_str());
}


//===----------------------------------------------------------------------===//
// Debug Info Support
//===----------------------------------------------------------------------===//

std::unique_ptr<DIBuilder> DBuilder;

DIType *DebugInfo::getDoubleTy() {
    if (DblTy)
        return DblTy;
    
    DblTy = DBuilder->createBasicType("double", 64, dwarf::DW_ATE_float);
    return DblTy;
}



void DebugInfo::emitLocation(ExprAST *AST) {
    if (!AST)
        return Builder.SetCurrentDebugLocation(DebugLoc());
    DIScope *Scope;
    if (LexicalBlocks.empty())
        Scope = TheCU;
    else
        Scope = LexicalBlocks.back();
    Builder.SetCurrentDebugLocation(
                                    DebugLoc::get(AST->getLine(), AST->getCol(), Scope));
}

void DebugInfo::emitLocation(StatAST *AST){
    if (!AST)
        return Builder.SetCurrentDebugLocation(DebugLoc());
    DIScope *Scope;
    if (LexicalBlocks.empty())
        Scope = TheCU;
    else
        Scope = LexicalBlocks.back();
    Builder.SetCurrentDebugLocation(
                                    DebugLoc::get(AST->getLine(), AST->getCol(), Scope));
}


static DISubroutineType *CreateFunctionType(unsigned NumArgs, DIFile *Unit) {
    SmallVector<Metadata *, 8> EltTys;
    DIType *DblTy = KSDbgInfo.getDoubleTy();
    
    // Add the result type.
    EltTys.push_back(DblTy);
    
    for (unsigned i = 0, e = NumArgs; i != e; ++i)
        EltTys.push_back(DblTy);
    
    return DBuilder->createSubroutineType(DBuilder->getOrCreateTypeArray(EltTys));
}

Value *NumberExprAST::codegen() {
	//return ConstantInt::get(Builder.getInt32Ty(), this->Val, true);
    KSDbgInfo.emitLocation(this);
	return ConstantFP::get(TheContext, APFloat(Val));
}

Value *VariableExprAST::codegen() {
	// Look this variable up in the function.
	Value *V = NamedValues[Name];
	if (!V)
		return LogErrorV("Unknown variable name");
    KSDbgInfo.emitLocation(this);
	// Load the value.
	return Builder.CreateLoad(V, Name.c_str());
}


Value * TextExprAST::codegen()
{
	//return Constant::get(TheContext, StringRef(Text));
    KSDbgInfo.emitLocation(this);
	return nullptr;
	
}

Value *VarExprAST::codegen() {
	std::vector<AllocaInst *> OldBindings;

	Function *TheFunction = Builder.GetInsertBlock()->getParent();

	//  register all variables and initialize them
	for (unsigned i = 0, e = VarNames.size(); i != e; ++i) {
		const std::string &VarName = VarNames[i].first;
		ExprAST *Init = VarNames[i].second.get();

		// 在将变量添加到作用于前获得初始化表达式，防止初始化表达式中使用变量本身
		Value *InitVal;
		if (Init) {
			InitVal = Init->codegen();
			if (!InitVal)
				return nullptr;
		}
		else { // 如果没有指定, 赋值为 0.0.
			InitVal = ConstantFP::get(TheContext, APFloat(0.0));
		}
		// 创建 alloca
		AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);
		Builder.CreateStore(InitVal, Alloca);

		// 将该变量的先前值存入OldBindings中，以便在该作用域结束后恢复
		OldBindings.push_back(NamedValues[VarName]);

		// 记录此次绑定的值
		NamedValues[VarName] = Alloca;
	}
    KSDbgInfo.emitLocation(this);
	// 生成body部分的代码, 现在所有定义的变量均在作用域中
	Value *BodyVal = Body->codegen();
	if (!BodyVal)
		return nullptr;

	// 删除当前作用域中的所有的变量
	for (unsigned i = 0, e = VarNames.size(); i != e; ++i)
		// 恢复原来的值
		NamedValues[VarNames[i].first] = OldBindings[i];

	// 返回Body部分的计算结果
	return BodyVal;
}


Value *UnaryExprAST::codegen() {
  Value *OperandV = Operand->codegen();
  if (!OperandV)
    return nullptr;

  Function *F = getFunction(std::string("unary") + Opcode);
  if (!F)
    return LogErrorV("Unknown unary operator");
  KSDbgInfo.emitLocation(this);
  return Builder.CreateCall(
      F, OperandV,
      "unop"); //调用操作符对应函数，返回函数处理过的数值，完成单目操作符对表达式的运算
}

Value *BinaryExprAST::codegen() {
    KSDbgInfo.emitLocation(this);
	// '=' special handle, LHS isn't taken as expr
	if (Op == '=') {
		// 赋值操作中我们将LHS当作标识符
		// 假定不使用运行时类型识别（RTTI）这是LLVM的默认生成方式 
		// =如果希望进行运行时类型识别，可以使用 dynamic_cast 来进行动态错误检查
		VariableExprAST *LHSE = static_cast<VariableExprAST *>(LHS.get());
		if (!LHSE)
			return LogErrorV("destination of '=' must be a variable");
		// 生成 RHS 部分代码
		Value *Val = RHS->codegen();
		if (!Val)
			return nullptr;

		// 寻找变量名
		Value *Variable = NamedValues[LHSE->getName()];
		if (!Variable)
			return LogErrorV("Unknown variable name");

		Builder.CreateStore(Val, Variable);
		return Val;
	}

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
		//return LogErrorV("invalid binary operator");
        //若为新增操作符，跳出到下面执行
		break;
	}
	// 跳转并执行新增操作符对应的函数
	Function *F = getFunction(std::string("binary") + Op);
    assert(F && "binary operator not found!");

    Value *Ops[2] = {L, R};
    // 调用对应函数
    return Builder.CreateCall(F, Ops, "binop");
}

Value *CallExprAST::codegen() {
    KSDbgInfo.emitLocation(this);
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
		Function::Create(FT, Function::ExternalLinkage, Name, TheModule.get());

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
	//check if main already exists
	Function *TheFunction;
	std::unique_ptr<PrototypeAST>temp;
	if (hasMainFunction) {
		temp = std::move(MainLackOfProtos[Proto->getName()]);
	}
	if (hasMainFunction&&temp != nullptr) {
		auto args = temp->getArgs();
		auto Args = P.getArgs();
		if (args.size() != Args.size()) {
			//args inconsistency
			return LogErrorF("main function's arg_size is inconsistent");
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
	//main-------------------------------------------
	else {
		FunctionProtos[Proto->getName()] = std::move(Proto);
		TheFunction = getFunction(P.getName());
		if (!TheFunction)
			return nullptr;
	}
//old edition
//if op add it and its precedence to binopprecedence
	if (P.isBinaryOp())
          BinopPrecedence[P.getOperatorName()] = P.getBinaryPrecedence();

	// Create a new basic block to start insertion into.
	BasicBlock *BB = BasicBlock::Create(TheContext, "entry", TheFunction);
	Builder.SetInsertPoint(BB);

    // Create a subprogram DIE for this function.
    DIFile *Unit = DBuilder->createFile(KSDbgInfo.TheCU->getFilename(),
                                        KSDbgInfo.TheCU->getDirectory());
    DIScope *FContext = Unit;
   // unsigned LineNo = P.getLine();
    //unsigned ScopeLine = LineNo;
   // DISubprogram *SP = DBuilder->createFunction(
//                                                FContext, P.getName(), StringRef(), Unit, LineNo,
//                                                CreateFunctionType(TheFunction->arg_size(), Unit),
//                                                false /* internal linkage */, true /* definition */, ScopeLine,
//                                                DINode::FlagPrototyped, false);
    //TheFunction->setSubprogram(SP);
    
    // Push the current scope.
   // KSDbgInfo.LexicalBlocks.push_back(SP);
    
    // Unset the location for the prologue emission (leading instructions with no
    // location in a function are considered part of the prologue and the debugger
    // will run past them when breaking on a function)
    ExprAST* exprnull=nullptr;
    KSDbgInfo.emitLocation(exprnull);
	// Record the function arguments in the NamedValues map.
	NamedValues.clear();
    unsigned ArgIdx = 0;
	for (auto &Arg : TheFunction->args()) {
		// Create an alloca for this variable.
		AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, Arg.getName());
        // Create a debug descriptor for the variable.
      //  DILocalVariable *D = DBuilder->createParameterVariable(
       //                                                        SP, Arg.getName(), ++ArgIdx, Unit, LineNo, KSDbgInfo.getDoubleTy(),
//                                                               true);
//
//        DBuilder->insertDeclare(Alloca, D, DBuilder->createExpression(),
//                                DebugLoc::get(LineNo, 0, SP),
//                                Builder.GetInsertBlock());

		// Store the initial value into the alloca.
		Builder.CreateStore(&Arg, Alloca);

		// Add arguments to variable symbol table.
		NamedValues[Arg.getName()] = Alloca;
	}
    KSDbgInfo.emitLocation(Body.get());
    
	if (Value *RetVal = Body->codegen()) {
		// Finish off the function.
		Builder.CreateRet(RetVal);
        
        // Pop off the lexical block for the function.
        KSDbgInfo.LexicalBlocks.pop_back();

		// Validate the generated code, checking for consistency.
		verifyFunction(*TheFunction);

		// Run the optimizer on the function.
		//TheFPM->run(*TheFunction);

		//更新isMain值
		if (P.getName() == "main") {
			isMain = false;
			hasMainFunction = true;
		}

		return TheFunction;
	}

	// Error reading body, remove function.
	TheFunction->eraseFromParent();
    if (P.isBinaryOp())
        BinopPrecedence.erase(Proto->getOperatorName());
    
    // Pop off the lexical block for the function since we added it
    // unconditionally.
    KSDbgInfo.LexicalBlocks.pop_back();
	return nullptr;
}



Value * AssignStatAST::codegen()
{

	// *TheFunction = Builder.GetInsertBlock()->getParent();
	// Create a new basic block to start insertion into.
	//BasicBlock *BB = BasicBlock::Create(TheContext, "entry", TheFunction);
	/* ValueSymbolTable* st = BB->getValueSymbolTable();

	Value* v = st->lookup(Name);
	if (v == NULL || v->hasName() == false) {
		errs() << "undeclared variable " << Name << "\n";
		return NULL;
	}
	*/

    KSDbgInfo.emitLocation(this);
	// Look up the name.
	AllocaInst *Alloca = NamedValues[Name];
	
	Value * result=nullptr;
	if(Val)
		result = Val->codegen();

	if (!result)
		return nullptr;

	//Value* load = new LoadInst(result, "", false, BB);
	Builder.CreateStore(result, Alloca);
	return result;
	

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
    KSDbgInfo.emitLocation(this);
	Value * val = Body->codegen();
	return val;
}

Value * PrintStatAST::codegen()
{
    KSDbgInfo.emitLocation(this);
	return nullptr;
}

Value * IfStatAST::codegen()
{
    
    // Get the current Function object that is being built
    Function *TheFunction = Builder.GetInsertBlock()->getParent();

      // Create an alloca for the variable in the entry block.
    AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);
    
    KSDbgInfo.emitLocation(this);
    
    Value *OldVal=Builder.CreateLoad(NamedValues[VarName]);
    NamedValues[VarName]=Alloca;

	Value *CondV = IfCondition -> codegen();
	if (!CondV)
		return nullptr;
	// Convert condition to a bool by comparing non-equal to 0.0.
	CondV = Builder.CreateFCmpONE(CondV, ConstantFP::get(TheContext, APFloat(0.0)), "ifcond");

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
    
    Builder.CreateStore(ThenV, NamedValues[VarName]);
    
    Builder.CreateBr(MergeBB);
    
    Value *ElseV=nullptr;
    
    if (ElseStat) {
        // Emit else block.
        TheFunction->getBasicBlockList().push_back(ElseBB);
        Builder.SetInsertPoint(ElseBB);
        
        ElseV = ElseStat->codegen();
        if (!ElseV)
            return nullptr;
        
        Builder.CreateStore(ElseV, NamedValues[VarName]);
        Builder.CreateBr(MergeBB);
        
    }
    
    TheFunction->getBasicBlockList().push_back(MergeBB);
    Builder.SetInsertPoint(MergeBB);
    
    Value *result=Builder.CreateLoad(NamedValues[VarName]);
    
    // Restore the unshadowed variable.t
    if (OldVal)
        Builder.CreateStore(OldVal, NamedValues[VarName]);
    else
        NamedValues.erase(VarName);
   
    return result;
}


Value * WhileStatAST::codegen()
{
	KSDbgInfo.emitLocation(this);
	//处理循环控制条件
	Value *StartVal = WhileCondition->codegen();
	if (!StartVal)
		return nullptr;
    
	// 获取正在构建的当前Function对象
	Function *TheFunction = Builder.GetInsertBlock()->getParent();
    
    AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);
    
    Builder.CreateStore(StartVal, Alloca);
    
    
	//create loop block
	BasicBlock *LoopBB = BasicBlock::Create(TheContext, "loop", TheFunction);
    
    // create after block
    BasicBlock *AfterBB = BasicBlock::Create(TheContext, "afterloop", TheFunction);
    
    
    StartVal=Builder.CreateFCmpONE(Builder.CreateLoad(Alloca), ConstantFP::get(TheContext, APFloat(0.0)), "whilecond");

    // branch base on startcond
    Builder.CreateCondBr(StartVal, LoopBB, AfterBB);
    

    // insert LoopBB.
	Builder.SetInsertPoint(LoopBB);

    //save  NamedValues[VarName]
    Value *OldVal = Builder.CreateLoad(NamedValues[VarName]);
    NamedValues[VarName]=Alloca;
    
	if (!DoStat->codegen())
		return nullptr;

	Value *EndCond = WhileCondition->codegen();
	if (!EndCond)
		return nullptr;

    Builder.CreateStore(EndCond, Alloca);
    
    EndCond=Builder.CreateFCmpONE(Builder.CreateLoad(Alloca), ConstantFP::get(TheContext, APFloat(0.0)), "whilecond");

    Builder.CreateCondBr(EndCond, LoopBB, AfterBB);

	// code afterwards added to afterbb
	Builder.SetInsertPoint(AfterBB);

	// Restore the unshadowed variable.t
    if (OldVal)
        Builder.CreateStore(OldVal, Alloca);
    else
        NamedValues.erase(VarName);


	// while循环的代码生成总是返回0.0
	return Constant::getNullValue(Type::getDoubleTy(TheContext));
}


Value * BlockStatAST::codegen()
{
    KSDbgInfo.emitLocation(this);
	std::vector<AllocaInst *> OldBindings;

	Function *TheFunction = Builder.GetInsertBlock()->getParent();

	// 注册所有的变量
	for (unsigned i = 0, e = Variables.size(); i != e; ++i) {
		const std::string &VarName = Variables[i];
		

		// 在将变量添加到作用于前获得初始化表达式，防止初始化表达式中使用变量本身
		Value* InitVal = ConstantFP::get(TheContext, APFloat(0.0));
		// 如果没有指定, 赋值为 0.0.
		
		// 创建 alloca
		AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);
		Builder.CreateStore(InitVal, Alloca);

		// 将该变量的先前值存入OldBindings中，以便在该作用域结束后恢复
		OldBindings.push_back(NamedValues[VarName]);

		// 记录此次绑定的值
		NamedValues[VarName] = Alloca;
	}

	// 生成body部分的代码, 现在所有定义的变量均在作用域中
	Value *ret;
	for (unsigned i = 0, e = Statements.size(); i != e; ++i) {
		ret = Statements[i]->codegen();

	}
	if (!ret)
		return nullptr;

	// 删除当前作用域中的所有的变量
	for (unsigned i = 0, e = Variables.size(); i != e; ++i)
		// 恢复原来的值
		NamedValues[Variables[i]] = OldBindings[i];

	// 返回Body部分的计算结果
	return ret;
	

	/*

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
	*/
	
}

Value * ContinueStatAST::codegen()
{
    KSDbgInfo.emitLocation(this);
	return nullptr;
}

