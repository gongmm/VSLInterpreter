#ifndef  GLOBAL
#define GLOBAL
#include "AST.h"
#include <map>
/********************************
*                               *
* ���ȫ��ʹ�õı���            *
* �Լ��ṩ����İ��ʹ�õĺ���  *
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
	TOKEOF = -17
};

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

/// ������ļ�
void outputToTxt(std::string str);
/// ����ָʾ
extern int indent;

extern std::unique_ptr<ExprAST> LogError(const char *Str);
extern std::unique_ptr<PrototypeAST> LogErrorP(const char *Str);
//����������
std::unique_ptr<StatAST> LogErrorS(const char *Str);

extern std::unique_ptr<ExprAST> ParseExpression();
//statement��������
extern std::unique_ptr<StatAST> ParseStatement();
//statement������Ҫ�ø���
std::unique_ptr<ExprAST> ParseIdentifierExpr();
extern void HandleDefinition();
#endif // ! GLOBAL
