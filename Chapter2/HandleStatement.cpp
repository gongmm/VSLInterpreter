/*******************
*                  *
** 处理statement **
*                  *
********************/
#pragma once
#include"AST.h"
#include"Global.h"
//新添加新定义函数关于Statement
//Statement
std::unique_ptr<StatAST> ParseAssignStat();
std::unique_ptr<StatAST> ParseReturnStat();
std::unique_ptr<StatAST> ParsePrintStat();
std::unique_ptr<StatAST> ParseIfStat();
std::unique_ptr<StatAST> ParseWhileStat();
std::unique_ptr<StatAST> ParseBlockStat();
std::unique_ptr<StatAST> ParseStatement() {
	switch (CurTok) {
	case VARIABLE:
		return ParseAssignStat();
	case RETURN:
		getNextToken();
		return ParseReturnStat();
	case PRINT:
		getNextToken();
		return ParsePrintStat();
	case CONTINUE:
		getNextToken();
		return llvm::make_unique<ContinueStatAST>();//语义分析可能出现问题
	case IF:
		getNextToken();
		return ParseIfStat();
	case WHILE:
		getNextToken();
		return ParseWhileStat();
	case '{':
		getNextToken();
		return ParseBlockStat();
	default:
		getNextToken();
		return LogErrorS("unknown token when expecting an statement");
	}
}
//Assignment Statement
std::unique_ptr<StatAST> ParseAssignStat() {
	std::string Name = IdentifierStr;
	getNextToken();
	if (CurTok != ASSIGN_SYMBOL)
		return LogErrorS("Expected := in assignment statement");
	getNextToken();
	return llvm::make_unique<AssignStatAST>(Name, std::move(ParseExpression()));
}
//Return Statement
std::unique_ptr<StatAST> ParseReturnStat() {
	return llvm::make_unique<ReturnStatAST>(std::move(ParseExpression()));
}
//Print Statement
std::unique_ptr<StatAST> ParsePrintStat() {
	std::vector<std::string> Texts;
	if (CurTok != TEXT)
		return LogErrorS("Expected Text in Print statement");
	Texts.push_back(Text);
	getNextToken();
	while (CurTok == ',') {
		getNextToken();
		if (CurTok != TEXT)
			return LogErrorS("Expected Text in Print statement");
		Texts.push_back(Text);
		getNextToken();
	}
	return llvm::make_unique<PrintStatAST>(Texts);
}
//If Statement
std::unique_ptr<StatAST> ParseIfStat() {
	auto IfCondition = std::move(ParseExpression());
	if (CurTok != THEN) {
		return LogErrorS("Expected THEN in If statement");
	}
	getNextToken();
	auto ThenStat = std::move(ParseStatement());
	if (CurTok != ELSE) {
		if (CurTok != FI) {
			return LogErrorS("Expected FI in If statement");
		}
		getNextToken();
		return llvm::make_unique<IfStatAST>(std::move(IfCondition), std::move(ThenStat));
	}
	getNextToken();
	std::unique_ptr<StatAST>ElseStat = ParseStatement();
	if (CurTok != FI) {
		return LogErrorS("Expected FI in If statement");
	}
	getNextToken();
	return llvm::make_unique<IfStatAST>(std::move(IfCondition), std::move(ThenStat), std::move(ElseStat));
}
//While Statement
std::unique_ptr<StatAST> ParseWhileStat() {
	auto WhileCondition = std::move(ParseExpression());
	if (CurTok != DO)
		return LogErrorS("Expected DO in While statement");
	getNextToken();
	auto DoStat = std::move(ParseStatement());
	if (CurTok != DONE)
		return LogErrorS("Expected DONE in While statement");
	getNextToken();
	return llvm::make_unique<WhileStatAST>(std::move(WhileCondition), std::move(DoStat));
}
//Block Statement
std::unique_ptr<StatAST> ParseBlockStat() {
	//std::vector<VariableExprAST>variables;
	std::vector<std::unique_ptr<ExprAST>> variables;
	std::vector<std::unique_ptr<StatAST>> statements;
	if (CurTok != VAR) {
		return LogErrorS("Expected VAR in Block statement");
	}
	do {
		getNextToken();
		do {
			/*VariableExprAST* ptr = dynamic_cast<VariableExprAST*>(ParseIdentifierExpr().release());
			variables.push_back(*ptr);*/
			if (auto Arg = ParseIdentifierExpr())
				variables.push_back(std::move(Arg));
			/*else
				return nullptr;*/
			//若返回空 要报错
			if (CurTok != ',')
				break;
			getNextToken();
		} while (CurTok == VARIABLE);
	} while (CurTok == VAR);
	do {
		if (auto stat = ParseStatement())
			statements.push_back(std::move(stat));
		/*else
			return nullptr;*/
		//错返回空 要报错
	} while (CurTok != '}');
	getNextToken();
	return  llvm::make_unique<BlockStatAST>(std::move(variables), std::move(statements));
}