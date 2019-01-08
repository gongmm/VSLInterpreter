statement语法分析设计及实现

将VSL语法中赋值语句，循环语句，if-else语句，continue语句，return返回语句，print语句，block语句，都划分为Statement，继承与同一个StatAST基类

在VSL的语法中的使用规则示例如下：

循环语句：

```
WHILE 11-i
DO{
PRINT i
}
DONE
```

print语句：

```
PRINT “123”，i+1，“456”
```

条件分支语句：

```
IF i-1
THEN
PRINT 1
ELSE
PRINT 2
FI
```

continue语句（用于循环语句中跳过本次迭代剩余代码，直接进入下一次迭代）：

```
CONTINUE
```

return语句：

```
RETURN
```

block语句：

```
{
    PRINT 1+2
    IF i+1
    THEN
    PRINT i
    ELSE
    PRINT 1
    RETURN 10
}
```

各个statement数据结构成员如下：

```c++
//赋值语句
class AssignStatAST : public StatAST {
	std::string Name;//被赋值的变量名
	std::unique_ptr<ExprAST> Val;//变量被赋予的值
public:
	AssignStatAST(SourceLocation Loc, const std::string &Name, std::unique_ptr<ExprAST> Val) : StatAST(Loc), Name(Name), Val(std::move(Val)) {}

	Value *codegen() override;
    raw_ostream &dump(raw_ostream &out, int ind) override {
        StatAST::dump(out<<"assign "<<Name, ind);
        Val->dump(out,ind+1);
        return out;
    }
};
//返回语句
class ReturnStatAST : public StatAST {
	std::unique_ptr<ExprAST> Body;//返回的值
public:
	ReturnStatAST(SourceLocation Loc, std::unique_ptr<ExprAST> Body) : StatAST(Loc),Body(std::move(Body)) {}

	Value *codegen() override;
    raw_ostream &dump(raw_ostream &out, int ind) override {
        StatAST::dump(out<<"return", ind);
        Body->dump(debugIndent(out, ind) <<"Body: ", ind+1);
        return out;
    }
};
//打印语句
class PrintStatAST : public StatAST {
	std::vector<std::unique_ptr<ExprAST>> Texts;//打印的所有字符串组成的链表
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
//continue语句
class ContinueStatAST : public StatAST {
	Value *codegen() override;
	raw_ostream &dump(raw_ostream &out, int ind) override {
		StatAST::dump(out << "continue ", ind);
		return out;
	}
public:
	void addParent(Bag* bag) {
		this->parent = bag;
	}
};
//条件分支语句
class IfStatAST : public StatAST {
    //condition's value
    //std::string VarName;
	std::unique_ptr<ExprAST> IfCondition;//条件判断值
	std::unique_ptr<StatAST> ThenStat;//条件判断为真执行语句
	std::unique_ptr<StatAST> ElseStat;//条件判断为假执行语句
public:
	void addParent(Bag* bag) {
		if (ThenStat != nullptr)
			ThenStat->addParent(bag) ;
		if (ElseStat != nullptr)
			ElseStat->addParent( bag);
	}
	IfStatAST(SourceLocation Loc,std::unique_ptr<ExprAST> IfCondition, std::unique_ptr<StatAST> ThenStat, std::unique_ptr<StatAST> ElseStat)
		: StatAST(Loc), IfCondition(std::move(IfCondition)), ThenStat(std::move(ThenStat)), ElseStat(std::move(ElseStat)) {}
	IfStatAST(SourceLocation Loc,std::unique_ptr<ExprAST> IfCondition, std::unique_ptr<StatAST> ThenStat) : StatAST(Loc), IfCondition(std::move(IfCondition)),
		ThenStat(std::move(ThenStat)), ElseStat(nullptr) {}

	Value *codegen() override;
    raw_ostream &dump(raw_ostream &out, int ind) override {
        //StatAST::dump(out<<"if "<<VarName, ind);
        IfCondition->dump(debugIndent(out, ind) << "Cond:", ind + 1);
        ThenStat->dump(debugIndent(out, ind) << "Then:", ind + 1);
        ElseStat->dump(debugIndent(out, ind) << "Else:", ind + 1);
        return out;
    }
};
//循环语句
class WhileStatAST : public StatAST {
    //condition's value
	std::unique_ptr<ExprAST> WhileCondition;//循环判断条件
	std::unique_ptr<StatAST> DoStat;//循环的语句
public:
	WhileStatAST(SourceLocation Loc, std::unique_ptr<ExprAST> WhileCondition, std::unique_ptr<StatAST> DoStat)
		:StatAST(Loc), WhileCondition(std::move(WhileCondition)), DoStat(std::move(DoStat)) {}

	Value *codegen() override;
    raw_ostream &dump(raw_ostream &out, int ind) override {
        //StatAST::dump(out<<"while "<<VarName, ind);
        WhileCondition->dump(debugIndent(out, ind) << "WhileCond:", ind + 1);
        DoStat->dump(debugIndent(out, ind) << "DoStat:", ind + 1);
        return out;
    }
};
//block语句
class BlockStatAST : public StatAST {
	std::vector<std::string> Variables;//声明的变量
	std::vector<std::unique_ptr<StatAST>> Statements;//block内所有语句链接成的链表
public:
	void addParent(Bag* bag) {
		for (int i = 0; i < Statements.size();i++) {
			Statements[i]->addParent(bag);
		}
	}
	BlockStatAST(SourceLocation Loc, std::vector<std::string> Variables, std::vector<std::unique_ptr<StatAST>> Statements)
		: StatAST(Loc), Variables(std::move(Variables)), Statements(std::move(Statements)) {}

	Value *codegen() override;
    raw_ostream &dump(raw_ostream &out, int ind) override {
        StatAST::dump(out<<"block ", ind);
        
        for (const auto &statement : Statements)
            statement->dump(debugIndent(out, ind + 1), ind + 1);
       
        return out;
    }
};

```

语法分析生成statement语法结构

ParseStatement函数用于生成一个当前的statement，并返回该statement

先判断当前statement类型，进入对应的ParseStat函数，取得对应statement再返回

```c++
std::unique_ptr<StatAST> ParseStatement() {
	switch (CurTok) {
	case VARIABLE:
		return ParseAssignStat();//赋值语句
	case RETURN:
		getNextToken();
		return ParseReturnStat();//返回语句
	case PRINT:
		getNextToken();
		return ParsePrintStat();//打印语句
	case CONTINUE:
		getNextToken();
		return llvm::make_unique<ContinueStatAST>();//continue语句
	case IF:
		getNextToken();
		return ParseIfStat();//条件分支语句
	case WHILE:
		getNextToken();
		return ParseWhileStat();//循环语句
	case VAR:
		return ParseVarStat();//遇到变量声明语句
	case '{':
		getNextToken();
		return ParseBlockStat();//block语句
	default:
		if (CurTok == EOF)
			return nullptr;//遇到文件结束符
		getNextToken();
		return LogErrorS("unknown token when expecting an statement");
	}
}
```

获得特定statement的具体实现：

```c++
//变量声明语句
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

//Assignment Statement赋值语句
std::unique_ptr<StatAST> ParseAssignStat() {
    SourceLocation AssignLoc = CurLoc;
	int indentBefore=indent;
	std::string Name = IdentifierStr;
	getNextToken();
    //没有:=号，报错
	if (CurTok != ASSIGN_SYMBOL)
		return LogErrorS("Expected := in assignment statement");
	getNextToken();
	auto Result= llvm::make_unique<AssignStatAST>(AssignLoc,Name, std::move(ParseExpression()));//生成assignment对象
	return std::move(Result);
}
//Return Statement返回语句
std::unique_ptr<StatAST> ParseReturnStat() {
     SourceLocation ReturnLoc = CurLoc;
	auto Result = llvm::make_unique<ReturnStatAST>(ReturnLoc, std::move(ParseExpression()));
	return std::move(Result);
}
//Print Statement打印语句
std::unique_ptr<StatAST> ParsePrintStat() {
    SourceLocation PrintLoc = CurLoc;
	std::vector<std::unique_ptr<ExprAST>> Texts;
	if (CurTok == TEXT) {
		Texts.push_back(llvm::make_unique<TextExprAST>(Text));
		getNextToken();
	}
	else {
		Texts.push_back(std::move(ParseExpression()));
	}
    //可能有多个text字符串
	while (CurTok == ','){
		getNextToken();//去掉','
		 if (CurTok == TEXT) {
			 Texts.push_back(llvm::make_unique<TextExprAST>(Text));
			 getNextToken();
		 }
		 else {
			 Texts.push_back(std::move(ParseExpression()));
		 }
	}
	auto Result = llvm::make_unique<PrintStatAST>(PrintLoc,std::move(Texts));
	return std::move(Result);
}
//If Statement条件分支语句
std::unique_ptr<StatAST> ParseIfStat() {
    SourceLocation IfLoc = CurLoc;
   
	auto IfCondition = std::move(ParseExpression());
	if (CurTok != THEN) {//没有THEN，报错
		return LogErrorS("Expected THEN in If statement");
	}
	getNextToken();
	auto ThenStat = std::move(ParseStatement());
	if (CurTok != ELSE) {//判断有无else情况
		if (CurTok != FI) {
			indent = indentBefore;
			return LogErrorS("Expected FI in If statement");
		}
		getNextToken();
		return llvm::make_unique<IfStatAST>(IfLoc, std::move(IfCondition), std::move(ThenStat));
	}
	print("ELSE statement\n");
	getNextToken();
	std::unique_ptr<StatAST>ElseStat = ParseStatement();
	if (CurTok != FI) {//没有结尾FI符，报错
		return LogErrorS("Expected FI in If statement");
	}
	getNextToken();
	return llvm::make_unique<IfStatAST>(IfLoc, std::move(IfCondition), std::move(ThenStat), std::move(ElseStat));
}
//While Statement循环语句
std::unique_ptr<StatAST> ParseWhileStat() {
     SourceLocation WhileLoc = CurLoc;
	auto WhileCondition = std::move(ParseExpression());//获得循环条件判断条件
	if (CurTok != DO) {
		indent = indentBefore;
		return LogErrorS("Expected DO in While statement");
	}
	getNextToken();
	auto DoStat = std::move(ParseStatement());
	if (CurTok != DONE) {
		indent = indentBefore;
		return LogErrorS("Expected DONE in While statement");
	}
	getNextToken();
	Bag * bag = new Bag();
	DoStat->addParent(bag);
	std::unique_ptr<WhileStatAST> whilestate = llvm::make_unique<WhileStatAST>(WhileLoc, std::move(WhileCondition), std::move(DoStat));
	whilestate->parent = bag;
	return std::move(whilestate);
}
//Block Statement
std::unique_ptr<StatAST> ParseBlockStat() {
    SourceLocation BlockLoc = CurLoc;
	std::vector<std::string> variables;
	std::vector<std::unique_ptr<StatAST>> statements;
	while (CurTok == VAR){
		getNextToken();
		// 至少需要一个变量名
		if (CurTok != VARIABLE)
			return LogErrorS("expected identifier after var");
		do {
			std::string Name = IdentifierStr;
			
			variables.push_back(Name);

		
			if (CurTok != ',')
				break;
			getNextToken();
		} while (CurTok == VARIABLE);
	}
	do {
		if (CurTok == '}') {
			indent = indentBefore;
			getNextToken();
			return LogErrorS("Expected statement in block statement");
		}
		if (auto stat = ParseStatement())
			statements.push_back(std::move(stat));
		
		
	} while (CurTok != '}'&&CurTok!=TOKEOF);
	indent = indentBefore;
	getNextToken();
	return  llvm::make_unique<BlockStatAST>(BlockLoc,std::move(variables), std::move(statements));
}

```

