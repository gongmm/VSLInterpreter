/*******************
*                  *
** 处理statement **
*                  *
********************/
#pragma once
#include"AST.h"
#include"Global.h"
//工具函数
void print(std::string str) {//带indent输出
	for (int i = 0; i < indent; i++) {
		outputToTxt("\t");
	}
	outputToTxt(str);
}
//新添加新定义函数关于Statement
//Statement
std::unique_ptr<StatAST> ParseAssignStat();
std::unique_ptr<StatAST> ParseReturnStat();
std::unique_ptr<StatAST> ParsePrintStat();
std::unique_ptr<StatAST> ParseIfStat();
std::unique_ptr<StatAST> ParseWhileStat();
std::unique_ptr<StatAST> ParseBlockStat();
std::unique_ptr<StatAST> ParseVarStat();
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

	case VAR:
		return ParseVarStat();
	case '{':
		getNextToken();
		return ParseBlockStat();
	default:
		if (CurTok == EOF)
			return nullptr;
		getNextToken();
		return LogErrorS("unknown token when expecting an statement");
	}
}

/// varexpr ::= 'var' identifier ('=' expression)
 std::unique_ptr<StatAST> ParseVarStat() {
	getNextToken(); // eat the var.

	/**
	* 变量声明、初始化部分代码
	*/
	std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames;

	// 至少需要一个变量名
	if (CurTok != VARIABLE)
		return LogErrorS("expected identifier after var");

	while (1) {
		std::string Name = IdentifierStr;
		getNextToken(); // eat identifier.

		// 读取可能存在的初始化表达式
		std::unique_ptr<ExprAST> Init = nullptr;
		if (CurTok == '=') {
			getNextToken(); // eat the '='.

			Init = ParseExpression();
			if (!Init)
				return nullptr;
		}

		VarNames.push_back(std::make_pair(Name, std::move(Init)));

		// 声明变量部分结束，退出循环
		if (CurTok != ',')
			break;
		getNextToken(); // eat the ','.

		if (CurTok != VARIABLE)
			return LogErrorS("expected identifier list after var");
	}


	/**
	* 处理Body部分代码
	*/
	auto Body = ParseStatement();
	if (!Body)
		return nullptr;

	return llvm::make_unique<VarExprAST>(std::move(VarNames), std::move(Body));
}

//Assignment Statement
std::unique_ptr<StatAST> ParseAssignStat() {
	int indentBefore=indent;
	print("assignment-stat\n");
	indent++;
	print("VARIABLE");
	std::string Name = IdentifierStr;
	getNextToken();
	outputToTxt(":=");
	if (CurTok != ASSIGN_SYMBOL)
		return LogErrorS("Expected := in assignment statement");
	getNextToken();
	outputToTxt("expression\n");
	indent++;
	auto Result= llvm::make_unique<AssignStatAST>(Name, std::move(ParseExpression()));
	indent =indentBefore;
	return std::move(Result);
}
//Return Statement
std::unique_ptr<StatAST> ParseReturnStat() {
	int indentBefore = indent;
	print("return-stat\n");
	indent++;
	print("expression\n");
	indent++;
	auto Result = llvm::make_unique<ReturnStatAST>(std::move(ParseExpression()));
	indent = indentBefore;
	return std::move(Result);
}
//Print Statement
std::unique_ptr<StatAST> ParsePrintStat() {
	print("print-stat\n");
	std::vector<std::unique_ptr<ExprAST>> Texts;
	/*if (CurTok != TEXT)
		return LogErrorS("Expected Text in Print statement");*/
	if (CurTok == TEXT) {
		Texts.push_back(llvm::make_unique<TextExprAST>(Text));
		getNextToken();
	}
	else {
		Texts.push_back(std::move(ParseExpression()));
	}
	while (CurTok == ','){
		getNextToken();
		 if (CurTok == TEXT) {
			 Texts.push_back(llvm::make_unique<TextExprAST>(Text));
			 getNextToken();
		 }
		 else {
			 Texts.push_back(std::move(ParseExpression()));
		 }
	}
	auto Result = llvm::make_unique<PrintStatAST>(std::move(Texts));
	return std::move(Result);
}
//If Statement
std::unique_ptr<StatAST> ParseIfStat() {
    SourceLocation IfLoc = CurLoc;
    
	int indentBefore = indent;
	print("if-stat\n");
	indent++;
	print("ifCondition expression\n");
	indent++;
	auto IfCondition = std::move(ParseExpression());
	indent--;
	if (CurTok != THEN) {
		return LogErrorS("Expected THEN in If statement");
	}
	getNextToken();
	print("THEN statement\n");
	indent++;
	auto ThenStat = std::move(ParseStatement());
	indent--;
	if (CurTok != ELSE) {
		if (CurTok != FI) {
			indent = indentBefore;
			return LogErrorS("Expected FI in If statement");
		}
		getNextToken();
		indent = indentBefore;
		return llvm::make_unique<IfStatAST>(IfLoc, std::move(IfCondition), std::move(ThenStat));
	}
	print("ELSE statement\n");
	getNextToken();
	indent++;
	std::unique_ptr<StatAST>ElseStat = ParseStatement();
	indent--;
	if (CurTok != FI) {
		indent = indentBefore;
		return LogErrorS("Expected FI in If statement");
	}
	print("FI keyword\n");
	indent = indentBefore;
	getNextToken();
	return llvm::make_unique<IfStatAST>(std::move(IfCondition), std::move(ThenStat), std::move(ElseStat));
}
//While Statement
std::unique_ptr<StatAST> ParseWhileStat() {
	int indentBefore = indent;
	print("while-stat\n");
	indent++;
	print("do-condition\n");
	indent++;
	auto WhileCondition = std::move(ParseExpression());
	indent--;
	if (CurTok != DO) {
		indent = indentBefore;
		return LogErrorS("Expected DO in While statement");
	}
	print("DO statement\n");
	getNextToken();
	indent++;
	auto DoStat = std::move(ParseStatement());
	indent--;
	if (CurTok != DONE) {
		indent = indentBefore;
		return LogErrorS("Expected DONE in While statement");
	}
	print("DONE keyword\n");
	indent = indentBefore;
	getNextToken();
	return llvm::make_unique<WhileStatAST>(std::move(WhileCondition), std::move(DoStat));
}
//Block Statement
std::unique_ptr<StatAST> ParseBlockStat() {
	int indentBefore = indent;
	print("block-stat\n");
	indent++;
	//std::vector<VariableExprAST>variables;
	std::vector<std::unique_ptr<ExprAST>> variables;
	std::vector<std::unique_ptr<StatAST>> statements;
	while (CurTok == VAR){
		print("declaration\n");
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
	}
	print("statement-list\n");
	do {
		if (CurTok == '}') {
			indent = indentBefore;
			getNextToken();
			return LogErrorS("Expected statement in block statement");
		}
		indent++;
		if (auto stat = ParseStatement())
			statements.push_back(std::move(stat));
		indent--;
		/*else
			return nullptr;*/
		//错返回空 要报错
	} while (CurTok != '}'&&CurTok!=TOKEOF);
	indent = indentBefore;
	getNextToken();
	return  llvm::make_unique<BlockStatAST>(std::move(variables), std::move(statements));
}
