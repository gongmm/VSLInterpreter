#ifndef  GLOBAL
#define GLOBAL
#include "AST.h"
//#include "../include/KaleidoscopeJIT.h"
#include <map>

using namespace llvm;
//using namespace llvm::orc;
using namespace llvm::sys;

/********************************
*                               *
* 存放全局使用的变量            *
* 以及提供给别的板块使用的函数  *
*                               *
*********************************/
//===----------------------------------------------------------------------===//
// Lexer
//===----------------------------------------------------------------------===//

extern std::string IdentifierStr;
extern double NumVal;
extern std::string Text;
enum Token {
	VARIABLE = -1,
	INTEGER = -2,
	TEXT = -3,
	ASSIGN_SYMBOL = -4,
	FUNC = -5,
	PRINT = -6,
	RETURN = -7,
	CONTINUE = -8,
	IF = -9,
	THEN = -10,
	ELSE = -11,
	FI = -12,
	WHILE = -13,
	DO = -14,
	DONE = -15,
	VAR = -16,
	TOKEOF = -17,
	BINARY = -18,
	UNARY = -19
};
std::string getTokName(int Tok) {
    switch (Tok) {
        case TOKEOF:
            return "TOKEOF";
        case TEXT:
            return "TEXT";
        case ASSIGN_SYMBOL:
            return "ASSIGN_SYMBOL";
        case FUNC:
            return "FUNC";
        case INTEGER:
            return "INTEGER";
        case IF:
            return "IF";
        case THEN:
            return "THEN";
        case ELSE:
            return "ELSE";
        case WHILE:
            return "WHILE";
        case FI:
            return "FI";
        case BINARY:
            return "BINARY";
        case UNARY:
            return "UNARY";
        case DONE:
            return "DONE";
        case VARIABLE:
            return "VARIABLE";
        case CONTINUE:
            return "CONTINUE";
        case RETURN:
            return "RETURN";
    }
    return std::string(1, (char)Tok);
}

static LLVMContext TheContext;
static IRBuilder<> Builder(TheContext);

struct DebugInfo {
    DICompileUnit *TheCU;
    DIType *DblTy;
    std::vector<DIScope *> LexicalBlocks;
    
    void emitLocation(ExprAST *AST);
    void emitLocation(StatAST *AST);
    DIType *getDoubleTy();
} KSDbgInfo;

//===----------------------------------------------------------------------===//
// Debug Info Support
//===----------------------------------------------------------------------===//

static std::unique_ptr<DIBuilder> DBuilder;

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


static int advance() {
    int LastChar = getchar();
    
    if (LastChar == '\n' || LastChar == '\r') {
        LexLoc.Line++;
        LexLoc.Col = 0;
    } else
        LexLoc.Col++;
    return LastChar;
}


extern bool recWhitespace(int LastChar);
extern int recKeyword();
extern int gettok();

//===----------------------------------------------------------------------===//
// Parser
//===----------------------------------------------------------------------===//

/// CurTok/getNextToken - Provide a simple token buffer.  CurTok is the current
/// token the parser is looking at.  getNextToken reads another token from the
/// lexer and updates CurTok with its results.
extern int CurTok;
extern int getNextToken();

/// BinopPrecedence - This holds the precedence for each binary operator that is
/// defined.
extern std::map<char, int> BinopPrecedence;

/// 输出至文件
void outputToTxt(std::string str);
/// 缩进指示
extern int indent;

extern std::unique_ptr<ExprAST> LogError(const char *Str);
extern std::unique_ptr<PrototypeAST> LogErrorP(const char *Str);
//新增报错函数
std::unique_ptr<StatAST> LogErrorS(const char *Str);

extern std::unique_ptr<ExprAST> ParseExpression();
//statement函数定义
extern std::unique_ptr<StatAST> ParseStatement();
//statement函数内要用更改
std::unique_ptr<ExprAST> ParseIdentifierExpr();
extern void HandleDefinition();

//===----------------------------------------------------------------------===//
// Code Generation
//===----------------------------------------------------------------------===//

extern LLVMContext TheContext;
extern std::unique_ptr<Module> TheModule;
extern std::map<std::string, AllocaInst *> NamedValues;
//extern std::map<std::string, Value *> NamedValues;






//===----------------------------------------------------------------------===//
// JIT & Optimizer Support
//===----------------------------------------------------------------------===//
//extern std::unique_ptr<legacy::FunctionPassManager> TheFPM;
//extern std::unique_ptr<KaleidoscopeJIT> TheJIT;
extern std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;
//optimize
extern void InitializeModule();
Function *getFunction(std::string Name);
//support main()
extern bool isMain;
extern std::map<std::string, std::unique_ptr<PrototypeAST>> MainLackOfProtos;
extern bool hasMainFunction;
Function *getLackFunction(std::string Name);
extern void processMain();
//===----------------------------------------------------------------------===//
// "Library" functions that can be "extern'd" from user code.
//===----------------------------------------------------------------------===//
//添加print调用
/*#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

/// putchard - putchar that takes a double and returns 0.
extern "C" DLLEXPORT double putchard(double X) {
	fputc((char)X, stderr);
	return 0;
}

/// printd - printf that takes a double prints it as "%f\n", returning 0.
extern "C" DLLEXPORT double printd(double X) {
	fprintf(stderr, "%f\n", X);
	return 0;
}
*/

#endif // ! GLOBAL
