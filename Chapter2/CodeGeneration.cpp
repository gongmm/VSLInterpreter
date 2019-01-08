#pragma once
#include "DebugInfo.h"
#include "llvm/IR/ValueSymbolTable.h"


using namespace llvm;
using namespace llvm::orc;
//===----------------------------------------------------------------------===//
// Code Generation
//===----------------------------------------------------------------------===//

LLVMContext TheContext;
IRBuilder<> Builder(TheContext);
//extern std::unique_ptr<Module> TheModule;
//std::map<std::string, Value *> NamedValues;

extern DebugInfo KSDbgInfo;

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
	return TmpB.CreateAlloca(Type::getInt32Ty(TheContext), nullptr, VarName.c_str());
}


//===----------------------------------------------------------------------===//
// Debug Info Support
//===----------------------------------------------------------------------===//

std::unique_ptr<DIBuilder> DBuilder;


static DISubroutineType *CreateFunctionType(unsigned NumArgs, DIFile *Unit) {
    SmallVector<Metadata *, 8> EltTys;
    DIType *DblTy = KSDbgInfo.getIntTy();
    
    // Add the result type.
    EltTys.push_back(DblTy);
    
    for (unsigned i = 0, e = NumArgs; i != e; ++i)
        EltTys.push_back(DblTy);
    
    return DBuilder->createSubroutineType(DBuilder->getOrCreateTypeArray(EltTys));
}

Value *NumberExprAST::codegen() {
	//return ConstantInt::get(Builder.getInt32Ty(), this->Val, true);
    KSDbgInfo.emitLocation(this);
	return ConstantInt::get(TheContext, APInt(32, Val, true));
	//return ConstantFP::get(TheContext, APFloat(Val));
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

		// �ڽ�������ӵ�������ǰ��ó�ʼ�����ʽ����ֹ��ʼ�����ʽ��ʹ�ñ�������
		Value *InitVal;
		if (Init) {
			InitVal = Init->codegen();
			if (!InitVal)
				return nullptr;
		}
		else { // ���û��ָ��, ��ֵΪ 0.0.
			InitVal = ConstantInt::get(TheContext, APInt(32,0));
			//InitVal = ConstantFP::get(TheContext, APFloat(0.0));
		}
		// ���� alloca
		AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);
		Builder.CreateStore(InitVal, Alloca);

		// ���ñ�������ǰֵ����OldBindings�У��Ա��ڸ������������ָ�
		OldBindings.push_back(NamedValues[VarName]);

		// ��¼�˴ΰ󶨵�ֵ
		NamedValues[VarName] = Alloca;
	}
    KSDbgInfo.emitLocation(this);
	// ����body���ֵĴ���, �������ж���ı���������������
	Value *BodyVal = Body->codegen();
	if (!BodyVal)
		return nullptr;

	// ɾ����ǰ�������е����еı���
	for (unsigned i = 0, e = VarNames.size(); i != e; ++i)
		// �ָ�ԭ����ֵ
		NamedValues[VarNames[i].first] = OldBindings[i];

	// ����Body���ֵļ�����
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
      "unop"); //���ò�������Ӧ���������غ������������ֵ����ɵ�Ŀ�������Ա��ʽ������
}

Value *BinaryExprAST::codegen() {
    KSDbgInfo.emitLocation(this);
	// '=' special handle, LHS isn't taken as expr
	if (Op == '=') {
		// ��ֵ���������ǽ�LHS������ʶ��
		// �ٶ���ʹ������ʱ����ʶ��RTTI������LLVM��Ĭ�����ɷ�ʽ 
		// =���ϣ����������ʱ����ʶ�𣬿���ʹ�� dynamic_cast �����ж�̬������
		VariableExprAST *LHSE = static_cast<VariableExprAST *>(LHS.get());
		if (!LHSE)
			return LogErrorV("destination of '=' must be a variable");
		// ���� RHS ���ִ���
		Value *Val = RHS->codegen();
		if (!Val)
			return nullptr;

		// Ѱ�ұ�����
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
		return Builder.CreateAdd(L, R, "addtmp");
	case '-':
		return Builder.CreateSub(L, R, "subtmp");
	case '*':
		return Builder.CreateMul(L, R, "multmp");
	case '/':
		return Builder.CreateExactSDiv(L, R, "divtmp");
	case '<':
		L = Builder.CreateICmpULT(L, R, "cmptmp");
		// Convert bool 0/1 to double 0.0 or 1.0
		return Builder.CreateUIToFP(L, Type::getInt32Ty(TheContext), "booltmp");
	default:
		//return LogErrorV("invalid binary operator");
        //��Ϊ����������������������ִ��
		break;
	}
	// ��ת��ִ��������������Ӧ�ĺ���
	Function *F = getFunction(std::string("binary") + Op);
    assert(F && "binary operator not found!");

    Value *Ops[2] = {L, R};
    // ���ö�Ӧ����
    return Builder.CreateCall(F, Ops, "binop");
}

Value *CallExprAST::codegen() {
    KSDbgInfo.emitLocation(this);
	//�޸�ǰ
	/*// Look up the name in the global module table.
	Function *CalleeF = TheModule->getFunction(Callee);*/
	//�޸ĺ�
	// Look up the name in the global module table.
	Function *CalleeF = getFunction(Callee);
	//if (isMain&&CalleeF==nullptr) {
	if (CalleeF == nullptr) {
		std::vector<std::string> ArgNames;
		//��ʱ�洢���ƣ�������Ϊ����ֵ����������������ʱ�ټ�
		for (int i = 0; i < Args.size();i++) {
			ArgNames.push_back("temp"+i);
		}
		MainLackOfProtos[Callee]= llvm::make_unique<PrototypeAST>(Callee, std::move(ArgNames));
		
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
	// Ѱ���Ƿ����Ѿ����ڵĺ���
	Function *TheFunction = TheModule->getFunction(Name);

	if (TheFunction)
		return (Function*)LogErrorV("Prototype already exist.");

	// Make the function type:  int(int,int) etc.
	std::vector<Type*> Integers(Args.size(), Type::getInt32Ty(TheContext));
	FunctionType *FT =
		FunctionType::get(Type::getInt32Ty(TheContext), Integers, false);

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
	//��Ӷ�ȫ�ֺ���ԭ�ͱ�FunctionProtos���޸ģ��޸�getFunction�ķ�ʽ
	auto &P = *Proto;

	//��ӹ���main���߼�-------------------------------------------
	//	if (P.getName() == "main")
	//		isMain = true;
	//check if main already exists
	Function *TheFunction;
	std::unique_ptr<PrototypeAST>temp;
	//if (hasMainFunction) {
	temp = std::move(MainLackOfProtos[Proto->getName()]);
	//}
	//if (hasMainFunction&&temp != nullptr) {
	if (temp != nullptr) {
		auto args = temp->getArgs();
		auto Args = P.getArgs();
		if (args.size() != Args.size()) {
			//args inconsistency
			return LogErrorF("main function's arg_size is inconsistent");
		}
		//P.setArgs(args);
		temp->setArgs(Args);
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
    unsigned LineNo = P.getLine();
    unsigned ScopeLine = LineNo;
    DISubprogram *SP = DBuilder->createFunction(
                                                FContext, P.getName(), StringRef(), Unit, LineNo,
                                                CreateFunctionType(TheFunction->arg_size(), Unit),
                                                false /* internal linkage */, true /* definition */, ScopeLine,
                                                DINode::FlagPrototyped, false);
    TheFunction->setSubprogram(SP);
    
    // Push the current scope.
    KSDbgInfo.LexicalBlocks.push_back(SP);
    
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
        DILocalVariable *D = DBuilder->createParameterVariable(
                                                               SP, Arg.getName(), ++ArgIdx, Unit, LineNo, KSDbgInfo.getIntTy(),
                                                               true);

        DBuilder->insertDeclare(Alloca, D, DBuilder->createExpression(),
                                DebugLoc::get(LineNo, 0, SP),
                                Builder.GetInsertBlock());

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
		TheFPM->run(*TheFunction);

		//����isMainֵ
		if (P.getName() == "main") {
			//	isMain = false;
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
    KSDbgInfo.emitLocation(this);
	Value * val = Body->codegen();
	return val;
}

Value * PrintStatAST::codegen()
{
	KSDbgInfo.emitLocation(this);
	//std::vector<std::unique_ptr<ExprAST>> Texts;
	for (int i = 0; i < Texts.size(); i++) {
		auto Exp = std::move(Texts[i]);
		ExprAST* ptr = dynamic_cast<ExprAST*>(Exp.get());
		if (ptr != nullptr) {
			Exp.release();
		}
//        const char* typeName = typeid(*ptr).name();
//        const char* className = typeid(TextExprAST).name();
//        if (strcmp(typeName, className) == 0) {
//            std::unique_ptr<TextExprAST> text;
//            text.reset((TextExprAST*)ptr);
//            for (int j = 0; j < text->getText().size(); j++) {
//                char t1 = text->getText().at(j);
			/*	if (t1 == '\\') {
					j++;
					if (j >= text->getText().size())
						return LogErrorV("input String is not right!");
					t1 = text->getText().at(j);
					if (t1 == 'n') {
						t1 = '\n';
					}
					else if (t1 == '\\') {
						t1 = '\\';
					}
					else if (t1 == '\"') {
						t1 = '\"';
					}
				}*/
//                Function *CalleeF = getFunction("putchard");
//                if (!CalleeF)
//                    return LogErrorV("Unknown function referenced");
//                std::vector<Value *> ArgsV;
//                ArgsV.push_back(ConstantInt::get(TheContext, APInt(32,(int)(t1))));
//                //ArgsV.push_back(ConstantFP::get(TheContext, APFloat((double)(t1))));
//                Builder.CreateCall(CalleeF, ArgsV, "calltmp");
//            }
//        }
//        else {
			std::unique_ptr<ExprAST> temp;
			temp.reset(ptr);
			Function *CalleeF = getFunction("printd");
			if (!CalleeF)
				return LogErrorV("Unknown function referenced");
			std::vector<Value *> ArgsV;
			Value* value = temp->codegen();
			ArgsV.push_back(value);
			Builder.CreateCall(CalleeF, ArgsV, "calltmp");
//        }
	}
	return ConstantInt::get(TheContext, APInt(32,(int)(0)));
}

Value * IfStatAST::codegen()
{
	KSDbgInfo.emitLocation(this);

	Value *CondV = IfCondition->codegen();
	if (!CondV)
		return nullptr;

	// Convert condition to a bool by comparing non-equal to 0.0.
	CondV = Builder.CreateICmpNE(CondV, Builder.getInt32(0), "ifcond");
	//CondV = Builder.CreateFCmpONE(CondV, ConstantInt::get(TheContext, APInt(32,0)), "ifcond");

	Function *TheFunction = Builder.GetInsertBlock()->getParent();

	// Create blocks for the then and else cases.  Insert the 'then' block at the
	// end of the function.
	BasicBlock *ThenBB = BasicBlock::Create(TheContext, "then", TheFunction);
	BasicBlock *ElseBB = ElseBB = BasicBlock::Create(TheContext, "else");
	BasicBlock *MergeBB = BasicBlock::Create(TheContext, "ifcont");
	if (ElseStat != nullptr) {
		Builder.CreateCondBr(CondV, ThenBB, ElseBB);
	}
	else {
		Builder.CreateCondBr(CondV, ThenBB, MergeBB);
	}
	

	// Emit then value.
	Builder.SetInsertPoint(ThenBB);

	Value *ThenV = ThenStat->codegen();
	if (!ThenV)
		return nullptr;

	Builder.CreateBr(MergeBB);
	// Codegen of 'Then' can change the current block, update ThenBB for the PHI.
	ThenBB = Builder.GetInsertBlock();
    
	// Emit else block.
	TheFunction->getBasicBlockList().push_back(ElseBB);
	Builder.SetInsertPoint(ElseBB);
	Value *ElseV;
	if (ElseStat != nullptr) {
		ElseV = ElseStat->codegen();
		if (!ElseV)
			return nullptr;
		Builder.CreateBr(MergeBB);

	}
	ElseBB = Builder.GetInsertBlock();
	// Emit merge block.
	TheFunction->getBasicBlockList().push_back(MergeBB);
	Builder.SetInsertPoint(MergeBB);
	PHINode *PN = Builder.CreatePHI(Type::getInt32Ty(TheContext), 2, "iftmp");

	PN->addIncoming(ThenV, ThenBB);
	if (ElseStat != nullptr) {
		PN->addIncoming(ElseV, ElseBB);
	}
	else {
		PN->addIncoming(Constant::getNullValue(Type::getInt32Ty(TheContext)), ElseBB);
	}

	return PN;
	//if (ElseStat != nullptr) {
	//	Builder.CreateCondBr(CondV, ThenBB, ElseBB);

	//	// Emit then value.
	//	Builder.SetInsertPoint(ThenBB);

	//	Value *ThenV = ThenStat->codegen();
	//	if (!ThenV)
	//		return nullptr;

	//	Builder.CreateBr(MergeBB);
	//	// Codegen of 'Then' can change the current block, update ThenBB for the PHI.
	//	ThenBB = Builder.GetInsertBlock();
	//	// Emit else block.
	//	TheFunction->getBasicBlockList().push_back(ElseBB);
	//	Builder.SetInsertPoint(ElseBB);
	//	ElseV = ElseStat->codegen();
	//	if (!ElseV)
	//		return nullptr;
	//	Builder.CreateBr(MergeBB);
	//	ElseBB = Builder.GetInsertBlock();
	//	// Emit merge block.
	//	TheFunction->getBasicBlockList().push_back(MergeBB);
	//	Builder.SetInsertPoint(MergeBB);
	//	PHINode *PN = Builder.CreatePHI(Type::getInt32Ty(TheContext), 2, "iftmp");

	//	PN->addIncoming(ThenV, ThenBB);
	//	PN->addIncoming(ElseV, ElseBB);
	//	
	//	return PN;
	//}
	//else {
	//	Builder.CreateCondBr(CondV, ThenBB, MergeBB);
	//	// Emit then value.
	//	Builder.SetInsertPoint(ThenBB);

	//	Value *ThenV = ThenStat->codegen();
	//	if (!ThenV)
	//		return nullptr;
	//	Builder.CreateBr(MergeBB);
	//	// Codegen of 'Then' can change the current block, update ThenBB for the PHI.
	//	ThenBB = Builder.GetInsertBlock();
	//	// Emit else block.
	//	TheFunction->getBasicBlockList().push_back(ElseBB);
	//	Builder.SetInsertPoint(ElseBB);
	//	
	//	ElseBB = Builder.GetInsertBlock();
	//	// Emit merge block.
	//	TheFunction->getBasicBlockList().push_back(MergeBB);
	//	Builder.SetInsertPoint(MergeBB);
	//	PHINode *PN = Builder.CreatePHI(Type::getInt32Ty(TheContext), 2, "iftmp");

	//	PN->addIncoming(ThenV, ThenBB);
	//	PN->addIncoming(Constant::getNullValue(Type::getInt32Ty(TheContext)), ElseBB);
	//	return PN;
	//}
	
}


Value * WhileStatAST::codegen()
{
	KSDbgInfo.emitLocation(this);
	
	//����ѭ����������
	Value *Condition = WhileCondition->codegen();
	if (!Condition)
		return nullptr;
    
	// ��ȡ���ڹ����ĵ�ǰFunction����
	Function *TheFunction = Builder.GetInsertBlock()->getParent();
    

	// create loop block
	BasicBlock *LoopBB = BasicBlock::Create(TheContext, "loop", TheFunction);
	// create after block
	BasicBlock *AfterBB = BasicBlock::Create(TheContext, "afterloop", TheFunction);
    
	// ��0�Ƚ�
	Condition = Builder.CreateICmpNE(Condition, Builder.getInt32(0), "whilecond");
	//Condition = Builder.CreateFCmpONE(Condition, ConstantInt::get(TheContext, APInt(32,0)), "whilecond");
	// branch base on startcond
	Builder.CreateCondBr(Condition, LoopBB, AfterBB);
	parent->loop = LoopBB;
	parent->after = AfterBB;
	
	// insert LoopBB.
	Builder.SetInsertPoint(LoopBB);
	
	parent->loop = LoopBB;
	parent->after = AfterBB;
	// Do statement �м��������
	if (!DoStat->codegen())
		return nullptr;


	//����ѭ����������
	Condition = WhileCondition->codegen();
	if (!Condition)
		return nullptr;

	Condition = Builder.CreateICmpNE(Condition, Builder.getInt32(0), "whilecond");
    //Condition=Builder.CreateFCmpONE(Condition, ConstantInt::get(TheContext, APInt(32,0)), "whilecond");

    // branch base on endcond
    Builder.CreateCondBr(Condition, LoopBB, AfterBB);


	// code afterwards added to afterbb
	Builder.SetInsertPoint(AfterBB);

	// whileѭ���Ĵ����������Ƿ���0.0
	return Constant::getNullValue(Type::getInt32Ty(TheContext));
}


Value * BlockStatAST::codegen()
{
    KSDbgInfo.emitLocation(this);
	std::vector<AllocaInst *> OldBindings;

	Function *TheFunction = Builder.GetInsertBlock()->getParent();

	// ע�����еı���
	for (unsigned i = 0, e = Variables.size(); i != e; ++i) {
		const std::string &VarName = Variables[i];
		

		// �ڽ�������ӵ�������ǰ��ó�ʼ�����ʽ����ֹ��ʼ�����ʽ��ʹ�ñ�������
		Value* InitVal = ConstantInt::get(TheContext, APInt(32,0));
		// ���û��ָ��, ��ֵΪ 0.0.
		
		// ���� alloca
		AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);
		Builder.CreateStore(InitVal, Alloca);

		// ���ñ�������ǰֵ����OldBindings�У��Ա��ڸ������������ָ�
		OldBindings.push_back(NamedValues[VarName]);

		// ��¼�˴ΰ󶨵�ֵ
		NamedValues[VarName] = Alloca;
	}

	// ����body���ֵĴ���, �������ж���ı���������������
	Value *ret = 0;
	for (unsigned i = 0, e = Statements.size(); i != e; ++i) {
		ret = Statements[i]->codegen();

	}
	if (!ret)
		return nullptr;

	// ɾ����ǰ�������е����еı���
	for (unsigned i = 0, e = Variables.size(); i != e; ++i)
		// �ָ�ԭ����ֵ
		NamedValues[Variables[i]] = OldBindings[i];

	// ����Body���ֵļ�����
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
	//parent->con = Builder.CreateFCmpONE(ConstantFP::get(TheContext, APFloat(1.0)), ConstantFP::get(TheContext, APFloat(0.0)), "whilecond");
	//Builder.CreateCondBr(ConstantFP::get(TheContext, APFloat(1.0)), parent->loop, parent->after);
	Builder.CreateBr(parent->loop);
	return ConstantInt::get(TheContext, APInt(32,1));
}

