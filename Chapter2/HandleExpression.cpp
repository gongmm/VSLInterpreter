#pragma once
#include "Global.h"
/*******************
*                  *
** ����expression **
*                  *
********************/
/// GetTokPrecedence - Get the precedence of the pending binary operator token.
int GetTokPrecedence() {
	if (!isascii(CurTok))
		return -1;

	// Make sure it's a declared binop.
	int TokPrec = BinopPrecedence[CurTok];
	if (TokPrec <= 0)
		return -1;
	return TokPrec;
}

//���ߺ���
extern void print(std::string str);

//std::unique_ptr<ExprAST> ParseExpression();

/// numberexpr ::= number
std::unique_ptr<ExprAST> ParseNumberExpr() {
	auto Result = llvm::make_unique<NumberExprAST>(NumVal);
	getNextToken(); // consume the number
    print("number-expression\n");
	return std::move(Result);
}

/// parenexpr ::= '(' expression ')'
std::unique_ptr<ExprAST> ParseParenExpr() {
	getNextToken(); // eat (.
	auto V = ParseExpression();
	if (!V)
		return nullptr;

	if (CurTok != ')')
		return LogError("expected ')'");
	getNextToken(); // eat ).
	print("paren-expression\n");
	return V;
}

/// minusexpr ::= '-' expression
std::unique_ptr<ExprAST> ParseMinusExpr() {
  
  int BinOp = CurTok;

  getNextToken(); // eat -.

  auto V = ParseExpression();
  
  if (!V)
    return nullptr;

  auto LHS = llvm::make_unique<NumberExprAST>(0);
  
  print("minus-expression\n");
  return llvm::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(V));
  
}

/// identifierexpr
///   ::= identifier
///   ::= identifier '(' expression* ')'
std::unique_ptr<ExprAST> ParseIdentifierExpr() {
	std::string IdName = IdentifierStr;

	getNextToken(); // eat identifier.

	if (CurTok != '('){ // Simple variable ref.
		print("varible-reference-expression\n");
		return llvm::make_unique<VariableExprAST>(IdName);
	}
	// Call.
	getNextToken(); // eat (
	std::vector<std::unique_ptr<ExprAST>> Args;
	if (CurTok != ')') {
		while (true) {
			if (auto Arg = ParseExpression())
				Args.push_back(std::move(Arg));
			else
				return nullptr;

			if (CurTok == ')')
				break;

			if (CurTok != ',')
				return LogError("Expected ')' or ',' in argument list");
			getNextToken();
		}
	}

	// Eat the ')'.
	getNextToken();
	print("call-expression\n");
	return llvm::make_unique<CallExprAST>(IdName, std::move(Args));
}



/// varexpr ::= 'var' identifier ('=' expression)
static std::unique_ptr<ExprAST> ParseVarExpr() {
	getNextToken(); // eat the var.

	/**
	* ������������ʼ�����ִ���
	*/
	std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames;

	// ������Ҫһ��������
	if (CurTok != VARIABLE)
		return LogError("expected identifier after var");

	while (true) {
		std::string Name = IdentifierStr;
		getNextToken(); // eat identifier.

		// ��ȡ���ܴ��ڵĳ�ʼ�����ʽ
		std::unique_ptr<ExprAST> Init = nullptr;
		if (CurTok == '=') {
			getNextToken(); // eat the '='.

			Init = ParseExpression();
			if (!Init)
				return nullptr;
		}

		VarNames.push_back(std::make_pair(Name, std::move(Init)));

		// �����������ֽ������˳�ѭ��
		if (CurTok != ',')
			break;
		getNextToken(); // eat the ','.

		if (CurTok != VARIABLE)
			return LogError("expected identifier list after var");
	}


	/**
	* ����Body���ִ���
	*/
	auto Body = ParseExpression();
	if (!Body)
		return nullptr;

	return llvm::make_unique<VarExprAST>(std::move(VarNames), std::move(Body));
}


/// primary
///   ::= identifierexpr
///   ::= numberexpr
///   ::= parenexpr
std::unique_ptr<ExprAST> ParsePrimary() {
	switch (CurTok) {
	default:
		return LogError("unknown token when expecting an expression");
	case VARIABLE:
		return ParseIdentifierExpr();
	case INTEGER:
		return ParseNumberExpr();
	case VAR:
		return ParseVarExpr();
	case '(':
		return ParseParenExpr();
    case '-':
        return ParseMinusExpr();
	}

}


/// ��Ŀ�����
std::unique_ptr<ExprAST> ParseUnary() {
  // ��������е�Ŀ�����������䰴��ͨ����������
  if (!isascii(CurTok) || CurTok == '(' || CurTok == ',')
    return ParsePrimary();

  //������е�Ŀ��������ȡ����������ʣ�µİ�����Ϊ���е�Ŀ����������ݹ鴦��
  int Opc = CurTok;
  getNextToken();
  if (auto Operand = ParseUnary())
    return llvm::make_unique<UnaryExprAST>(Opc, std::move(Operand));
  return nullptr;
}


/// binoprhs
///   ::= ('+' primary)*
std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec,
	std::unique_ptr<ExprAST> LHS) {
	// If this is a binop, find its precedence.
	while (true) {
		int TokPrec = GetTokPrecedence();

		// If this is a binop that binds at least as tightly as the current binop,
		// consume it, otherwise we are done.
		if (TokPrec < ExprPrec)
			return LHS;

		// Okay, we know this is a binop.
		int BinOp = CurTok;
		getNextToken(); // eat binop

		// Parse the primary expression after the binary operator.
		//auto RHS = ParsePrimary();
        auto RHS = ParseUnary();
		if (!RHS)
			return nullptr;

		// If BinOp binds less tightly with RHS than the operator after RHS, let
		// the pending operator take RHS as its LHS.
		int NextPrec = GetTokPrecedence();
		if (TokPrec < NextPrec) {
			RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
			if (!RHS)
				return nullptr;
		}

		// Merge LHS/RHS.
		LHS =
			llvm::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));
	}
}


/// expression
///   ::= primary binoprhs
///
std::unique_ptr<ExprAST> ParseExpression() {
	//auto LHS = ParsePrimary();
	auto LHS = ParseUnary();
	if (!LHS)
		return nullptr;
	print("binary-expression\n");
	return ParseBinOpRHS(0, std::move(LHS));
}