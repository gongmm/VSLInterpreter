#pragma once
/*#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>*/

//修改
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "../include/KaleidoscopeJIT.h"
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace llvm;
using namespace llvm::orc;

/// ExprAST - Base class for all expression nodes.
class ExprAST {
public:
  virtual ~ExprAST() = default;

  virtual Value *codegen() = 0;
};

/// NumberExprAST - Expression class for numeric literals like "1.0".
class NumberExprAST : public ExprAST {
  double Val;

public:
  NumberExprAST(double Val) : Val(Val) {}

  Value *codegen() override;
};

/// VariableExprAST - Expression class for referencing a variable, like "a".
class VariableExprAST : public ExprAST {
  std::string Name;

public:
  VariableExprAST(const std::string &Name) : Name(Name) {}

  std::string getName() {
	  return Name;
  }

  Value *codegen() override;
};

/// BinaryExprAST - Expression class for a binary operator.
class BinaryExprAST : public ExprAST {
  char Op;
  std::unique_ptr<ExprAST> LHS, RHS;

public:
  BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS,
                std::unique_ptr<ExprAST> RHS)
      : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}

  Value *codegen() override;
};

/// CallExprAST - Expression class for function calls.
class CallExprAST : public ExprAST {
  std::string Callee;
  std::vector<std::unique_ptr<ExprAST>> Args;

public:
  CallExprAST(const std::string &Callee,
              std::vector<std::unique_ptr<ExprAST>> Args)
      : Callee(Callee), Args(std::move(Args)) {}

  Value *codegen() override;
};
class TextExprAST : public ExprAST {
	std::string Text;

public:
	TextExprAST(const std::string &Text)
		: Text(Text) {}

	Value *codegen() override;
};
/// PrototypeAST - This class represents the "prototype" for a function,
/// which captures its name, and its argument names (thus implicitly the number
/// of arguments the function takes).
class PrototypeAST {
  std::string Name;
  std::vector<std::string> Args;

public:
  PrototypeAST(const std::string &Name, std::vector<std::string> Args)
      : Name(Name), Args(std::move(Args)) {}

  Function *codegen();

  const std::string &getName() const { return Name; }
   std::vector<std::string> getArgs() const { return Args; }
   void setArgs(std::vector<std::string> args) {
	   std::vector<std::string> V(args);
	   Args = std::move(V);
   }
  
};
class StatAST;
/// FunctionAST - This class represents a function definition itself.
class FunctionAST {
  std::unique_ptr<PrototypeAST> Proto;
  std::unique_ptr<StatAST> Body;

public:
  FunctionAST(std::unique_ptr<PrototypeAST> Proto,
              std::unique_ptr<StatAST> Body)
      : Proto(std::move(Proto)), Body(std::move(Body)) {}

  Function *codegen();
};

/// StatementAST
//新定义Statement新添加结构定义
class StatAST {
public:
	virtual ~StatAST() = default;

	virtual Value *codegen() = 0;
};

/// NumberExprAST - Expression class for numeric literals like "1.0".
class AssignStatAST : public StatAST {
	std::string Name;
	std::unique_ptr<ExprAST> Val;
public:
	AssignStatAST(const std::string &Name, std::unique_ptr<ExprAST> Val) : Name(Name), Val(std::move(Val)) {}

	Value *codegen() override;
};
class ReturnStatAST : public StatAST {
	std::unique_ptr<ExprAST> Body;
public:
	ReturnStatAST(std::unique_ptr<ExprAST> Body) : Body(std::move(Body)) {}

	Value *codegen() override;
};
class PrintStatAST : public StatAST {
	std::vector<std::unique_ptr<ExprAST>> Texts;
public:
	PrintStatAST(std::vector<std::unique_ptr<ExprAST>> Texts) : Texts(std::move(Texts)) {}

	Value *codegen() override;
};
class ContinueStatAST : public StatAST {
	Value *codegen() override;
};
class IfStatAST : public StatAST {
	std::unique_ptr<ExprAST> IfCondition;
	std::unique_ptr<StatAST> ThenStat;
	std::unique_ptr<StatAST> ElseStat;
public:
	IfStatAST(std::unique_ptr<ExprAST> IfCondition, std::unique_ptr<StatAST> ThenStat, std::unique_ptr<StatAST> ElseStat)
		: IfCondition(std::move(IfCondition)), ThenStat(std::move(ThenStat)), ElseStat(std::move(ElseStat)) {}
	IfStatAST(std::unique_ptr<ExprAST> IfCondition, std::unique_ptr<StatAST> ThenStat) : IfCondition(std::move(IfCondition)),
		ThenStat(std::move(ThenStat)), ElseStat(nullptr) {}

	Value *codegen() override;
};
class WhileStatAST : public StatAST {
	std::unique_ptr<ExprAST> WhileCondition;
	std::unique_ptr<StatAST> DoStat;
public:
	WhileStatAST(std::unique_ptr<ExprAST> WhileCondition, std::unique_ptr<StatAST> DoStat)
		: WhileCondition(std::move(WhileCondition)), DoStat(std::move(DoStat)) {}

	Value *codegen() override;
};
class BlockStatAST : public StatAST {
	std::vector<std::unique_ptr<ExprAST>> Variables;
	std::vector<std::unique_ptr<StatAST>> Statements;
	std::map<std::string, llvm::Value*> locals;
public:
	BlockStatAST(std::vector<std::unique_ptr<ExprAST>> Variables, std::vector<std::unique_ptr<StatAST>> Statements)
		: Variables(std::move(Variables)), Statements(std::move(Statements)) {}

	Value *codegen() override;
};

