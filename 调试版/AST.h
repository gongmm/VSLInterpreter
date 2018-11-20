#pragma once
#pragma once
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using namespace llvm;
using namespace llvm::sys;

struct SourceLocation {
    int Line;
    int Col;
};

static SourceLocation CurLoc;
static SourceLocation LexLoc = {1, 0};


raw_ostream &debugIndent(raw_ostream &O, int size) {
    return O << std::string(size, ' ');
}

/// ExprAST - Base class for all expression nodes.
class ExprAST {
  SourceLocation Loc;
public:
  ExprAST(SourceLocation Loc = CurLoc) : Loc(Loc) {}
  virtual ~ExprAST() = default;

  virtual Value *codegen() = 0;
  int getLine() const { return Loc.Line; }
  int getCol() const { return Loc.Col; }
  virtual raw_ostream &dump(raw_ostream &out, int ind) {
        return out << ':' << getLine() << ':' << getCol() << '\n';
    }
};

/// NumberExprAST - Expression class for numeric literals like "1.0".
class NumberExprAST : public ExprAST {
  double Val;

public:
  NumberExprAST(double Val) : Val(Val) {}
  raw_ostream &dump(raw_ostream &out, int ind) override {
        return ExprAST::dump(out << Val, ind);
    }
  Value *codegen() override;
};

/// VariableExprAST - Expression class for referencing a variable, like "a".
class VariableExprAST : public ExprAST {
  std::string Name;

public:
  VariableExprAST(SourceLocation Loc, const std::string &Name)
    : ExprAST(Loc), Name(Name) {}
    const std::string &getName() const { return Name; }
  VariableExprAST(const std::string &Name) : Name(Name) {}

  std::string getName() {
	  return Name;
  }

  Value *codegen() override;
  raw_ostream &dump(raw_ostream &out, int ind) override {
        return ExprAST::dump(out << Name, ind);
    }
};

/// BinaryExprAST - Expression class for a binary operator.
class BinaryExprAST : public ExprAST {
  char Op;
  std::unique_ptr<ExprAST> LHS, RHS;

public:
  BinaryExprAST(SourceLocation Loc,char Op, std::unique_ptr<ExprAST> LHS,
                std::unique_ptr<ExprAST> RHS)
      : ExprAST(Loc),Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}

  Value *codegen() override;
  raw_ostream &dump(raw_ostream &out, int ind) override {
        ExprAST::dump(out << "binary" << Op, ind);
        LHS->dump(debugIndent(out, ind) << "LHS:", ind + 1);
        RHS->dump(debugIndent(out, ind) << "RHS:", ind + 1);
        return out;
    }
};

/// UnaryExprAST - Expression class for a unary operator.
class UnaryExprAST : public ExprAST {
  char Opcode;                      // 存储操作符
  std::unique_ptr<ExprAST> Operand; //存储操作符对应处理的数

public:
  UnaryExprAST(char Opcode, std::unique_ptr<ExprAST> Operand)
      : Opcode(Opcode), Operand(std::move(Operand)) {}

  Value *codegen() override;
  raw_ostream &dump(raw_ostream &out, int ind) override {
        ExprAST::dump(out << "unary" << Opcode, ind);
        Operand->dump(out, ind + 1);
        return out;
    }
};

/// CallExprAST - Expression class for function calls.
class CallExprAST : public ExprAST {
  std::string Callee;
  std::vector<std::unique_ptr<ExprAST>> Args;

public:
  CallExprAST(SourceLocation Loc,const std::string &Callee,
              std::vector<std::unique_ptr<ExprAST>> Args)
      : ExprAST(Loc), Callee(Callee), Args(std::move(Args)) {}

  Value *codegen() override;
  raw_ostream &dump(raw_ostream &out, int ind) override {
        ExprAST::dump(out << "call " << Callee, ind);
        for (const auto &Arg : Args)
            Arg->dump(debugIndent(out, ind + 1), ind + 1);
        return out;
    }
};
class TextExprAST : public ExprAST {
	std::string Text;

public:
	TextExprAST(const std::string &Text)
		: Text(Text) {}

	Value *codegen() override;
    raw_ostream &dump(raw_ostream &out, int ind) override {
        return ExprAST::dump(out<<"text "<<Text, ind);
    }
};

class StatAST{
    SourceLocation Loc;
public:
    StatAST(SourceLocation Loc = CurLoc) : Loc(Loc) {}
    virtual ~StatAST()= default;
    
    virtual Value *codegen() = 0;
    int getLine() const { return Loc.Line; }
    int getCol() const { return Loc.Col; }
    virtual raw_ostream &dump(raw_ostream &out, int ind) {
        return out << ':' << getLine() << ':' << getCol() << '\n';
    }
};

/// PrototypeAST - This class represents the "prototype" for a function,
/// which captures its name, and its argument names (thus implicitly the number
/// of arguments the function takes).
class PrototypeAST {
  std::string Name;
  std::vector<std::string> Args;
  bool IsOperator; //是否是一个操作符
  unsigned Precedence; //当该原型为一个双目操作符时，该属性存储其优先级
  int Line;
public:
  PrototypeAST(SourceLocation Loc, const std::string &Name, std::vector<std::string> Args,bool IsOperator=false, unsigned Precedence = 0)
      : Name(Name), Args(std::move(Args)), IsOperator(IsOperator), Precedence(Precedence), Line(Loc.Line) {}

  Function *codegen();
  const std::string &getName() const { return Name; }
   std::vector<std::string> getArgs()const { return Args; }
   void setArgs(std::vector<std::string> args) {
	   std::vector<std::string> V(args);
	   Args = std::move(V);
   }

  bool isUnaryOp() const {
    return IsOperator && Args.size() == 1;
  } //判定是否为单目操作符
  bool isBinaryOp() const {
    return IsOperator && Args.size() == 2;
  } //判断是否为双目操作符

  char getOperatorName() const { //若为操作符函数获得操作符的字符
    assert(isUnaryOp() || isBinaryOp());
    return Name[Name.size() - 1];
  }

  unsigned getBinaryPrecedence() const { return Precedence; }
  int getLine() const { return Line; }
  
};

/// FunctionAST - This class represents a function definition itself.
class FunctionAST {
  std::unique_ptr<PrototypeAST> Proto;
  std::unique_ptr<StatAST> Body;

public:
  FunctionAST(std::unique_ptr<PrototypeAST> Proto,
              std::unique_ptr<StatAST> Body)
      : Proto(std::move(Proto)), Body(std::move(Body)) {}

  Function *codegen();
  raw_ostream &dump(raw_ostream &out, int ind) {
        debugIndent(out, ind) << "FunctionAST\n";
        ++ind;
        debugIndent(out, ind) << "Body:";
        return Body ? Body->dump(out, ind) : out << "null\n";
    }
};

//statements' AST
/// NumberExprAST - Expression class for numeric literals like "1.0".
class AssignStatAST : public StatAST {
	std::string Name;
	std::unique_ptr<ExprAST> Val;
public:
	AssignStatAST(SourceLocation Loc, const std::string &Name, std::unique_ptr<ExprAST> Val) : StatAST(Loc), Name(Name), Val(std::move(Val)) {}

	Value *codegen() override;
    raw_ostream &dump(raw_ostream &out, int ind) override {
        StatAST::dump(out<<"assign "<<Name, ind);
        Val->dump(out,ind+1);
        return out;
    }
};
class ReturnStatAST : public StatAST {
	std::unique_ptr<ExprAST> Body;
public:
	ReturnStatAST(SourceLocation Loc, std::unique_ptr<ExprAST> Body) : StatAST(Loc),Body(std::move(Body)) {}

	Value *codegen() override;
    raw_ostream &dump(raw_ostream &out, int ind) override {
        StatAST::dump(out<<"return", ind);
        Body->dump(debugIndent(out, ind) <<"Body: ", ind+1);
        return out;
    }
};
class PrintStatAST : public StatAST {
	std::vector<std::unique_ptr<ExprAST>> Texts;
public:
	PrintStatAST(SourceLocation Loc,std::vector<std::unique_ptr<ExprAST>> Texts) : StatAST(Loc), Texts(std::move(Texts)) {}

	Value *codegen() override;
    raw_ostream &dump(raw_ostream &out, int ind) override {
        StatAST::dump(out<<"print ", ind);
        for (const auto &Text : Texts)
            Text->dump(debugIndent(out, ind + 1), ind + 1);
        return out;
    }
};
class ContinueStatAST : public StatAST {
	Value *codegen() override;
    raw_ostream &dump(raw_ostream &out, int ind) override {
        StatAST::dump(out<<"continue ", ind);
        return out;
    }
};
class IfStatAST : public StatAST {
    //condition's value
    std::string VarName;
	std::unique_ptr<ExprAST> IfCondition;
	std::unique_ptr<StatAST> ThenStat;
	std::unique_ptr<StatAST> ElseStat;
public:
	IfStatAST(SourceLocation Loc,std::unique_ptr<ExprAST> IfCondition, std::unique_ptr<StatAST> ThenStat, std::unique_ptr<StatAST> ElseStat)
		: StatAST(Loc), IfCondition(std::move(IfCondition)), ThenStat(std::move(ThenStat)), ElseStat(std::move(ElseStat)) {}
	IfStatAST(SourceLocation Loc,std::unique_ptr<ExprAST> IfCondition, std::unique_ptr<StatAST> ThenStat) : StatAST(Loc), IfCondition(std::move(IfCondition)),
		ThenStat(std::move(ThenStat)), ElseStat(nullptr) {}

	Value *codegen() override;
    raw_ostream &dump(raw_ostream &out, int ind) override {
        StatAST::dump(out<<"if "<<VarName, ind);
        IfCondition->dump(debugIndent(out, ind) << "Cond:", ind + 1);
        ThenStat->dump(debugIndent(out, ind) << "Then:", ind + 1);
        ElseStat->dump(debugIndent(out, ind) << "Else:", ind + 1);
        return out;
    }
};
class WhileStatAST : public StatAST {
    //condition's value
    std::string VarName;
	std::unique_ptr<ExprAST> WhileCondition;
	std::unique_ptr<StatAST> DoStat;
public:
	WhileStatAST(SourceLocation Loc, std::unique_ptr<ExprAST> WhileCondition, std::unique_ptr<StatAST> DoStat)
		:StatAST(Loc), WhileCondition(std::move(WhileCondition)), DoStat(std::move(DoStat)) {}

	Value *codegen() override;
    raw_ostream &dump(raw_ostream &out, int ind) override {
        StatAST::dump(out<<"while "<<VarName, ind);
        WhileCondition->dump(debugIndent(out, ind) << "WhileCond:", ind + 1);
        DoStat->dump(debugIndent(out, ind) << "DoStat:", ind + 1);
        return out;
    }
};
class BlockStatAST : public StatAST {
	std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames;
	std::vector<std::unique_ptr<ExprAST>> Variables;
	std::vector<std::unique_ptr<StatAST>> Statements;
	std::map<std::string, llvm::Value*> locals;
public:
	BlockStatAST(
		SourceLocation Loc, std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames,
		std::vector<std::unique_ptr<StatAST>>)
		: StatAST(Loc), VarNames(std::move(VarNames)), Statements(std::move(Statements)) {}

	BlockStatAST(SourceLocation Loc, std::vector<std::unique_ptr<ExprAST>> Variables, std::vector<std::unique_ptr<StatAST>> Statements)
		: StatAST(Loc), Variables(std::move(Variables)), Statements(std::move(Statements)) {}

	Value *codegen() override;
    raw_ostream &dump(raw_ostream &out, int ind) override {
        StatAST::dump(out<<"block ", ind);
        for (auto &VarName : VarNames){
            StatAST::dump(out<<VarName.first, ind);
            VarName.second->dump(debugIndent(out, ind + 1), ind + 1);
        }
        for (const auto &Variable : Variables)
            Variable->dump(debugIndent(out, ind + 1), ind + 1);
        
        for (const auto &statement : Statements)
            statement->dump(debugIndent(out, ind + 1), ind + 1);
       
        for (auto &local : locals){
            StatAST::dump(out<<local.first<<" "<<local.second, ind);
        }
        return out;
    }
};


/// VarExprAST - Expression class for var
class VarExprAST : public StatAST {
	std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames;
	std::unique_ptr<StatAST> Body;

public:
	VarExprAST(SourceLocation Loc, std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames,
		std::unique_ptr<StatAST> Body)
		: StatAST(Loc),VarNames(std::move(VarNames)), Body(std::move(Body)) {}

	Value *codegen() override;
    raw_ostream &dump(raw_ostream &out, int ind) override {
        StatAST::dump(out<<"var ", ind);
        for (auto &VarName : VarNames){
            StatAST::dump(out<<VarName.first, ind);
            VarName.second->dump(debugIndent(out, ind + 1), ind + 1);
        }
        Body->dump(debugIndent(out, ind+1), ind+1);
        return out;
    }
};


