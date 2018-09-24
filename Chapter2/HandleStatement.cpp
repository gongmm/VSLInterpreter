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
static std::unique_ptr<StatAST> ParseAssignStat();
static std::unique_ptr<StatAST> ParseReturnStat();
static std::unique_ptr<StatAST> ParsePrintStat();
static std::unique_ptr<StatAST> ParseIfStat();
static std::unique_ptr<StatAST> ParseWhileStat();
static std::unique_ptr<StatAST> ParseBlockStat();
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
		return llvm::make_unique<StatAST>();
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
		return LogErrorS("unknown token when expecting an statement");
	}
}
//Assignment Statement
static std::unique_ptr<StatAST> ParseAssignStat() {
	std::string Name = IdentifierStr;
	getNextToken();
	if (CurTok != ASSIGN_SYMBOL)
		return LogErrorS("Expected := in assignment statement");
	return llvm::make_unique<AssignStatAST>(Name, ParseExpression());
}
//Return Statement
static std::unique_ptr<StatAST> ParseReturnStat() {
	return llvm::make_unique<ReturnStatAST>(ParseExpression());
}
//Print Statement
static std::unique_ptr<StatAST> ParsePrintStat() {
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
static std::unique_ptr<StatAST> ParseIfStat() {
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
		return llvm::make_unique<IfStatAST>(IfCondition, ThenStat);
	}
	getNextToken();
	std::unique_ptr<StatAST>ElseStat = ParseStatement();
	if (CurTok != FI) {
		return LogErrorS("Expected FI in If statement");
	}
	getNextToken();
	return llvm::make_unique<IfStatAST>(IfCondition, ThenStat, ElseStat);
}
//While Statement
static std::unique_ptr<StatAST> ParseWhileStat() {
	auto WhileCondition = std::move(ParseExpression());
	if (CurTok != DO)
		return LogErrorS("Expected DO in While statement");
	getNextToken();
	auto DoStat = std::move(ParseStatement());
	if (CurTok != DONE)
		return LogErrorS("Expected DONE in While statement");
	getNextToken();
	return llvm::make_unique<WhileStatAST>(WhileCondition, DoStat);
}
//Block Statement
static std::unique_ptr<StatAST> ParseBlockStat() {
	std::vector<VariableExprAST>variables;
	std::vector<StatAST>statements;
	if (CurTok != VAR) {
		return LogErrorS("Expected VAR in Block statement");
	}
	do {
		getNextToken();
		do {
			VariableExprAST* ptr = dynamic_cast<VariableExprAST*>(ParseIdentifierExpr().release());
			variables.push_back(*ptr);
			if (CurTok != ',')
				break;
			getNextToken();
		} while (CurTok == VARIABLE);
	} while (CurTok == VAR);
	do {
		ParseStatement();
	} while (CurTok != '}');
	getNextToken();
	return  llvm::make_unique<BlockStatAST>(variables, statements);
}