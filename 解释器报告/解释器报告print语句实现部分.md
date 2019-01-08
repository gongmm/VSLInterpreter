## Print语句实现

为了实现输出字符串的print语句，我们可以调用外部函数来帮助实现，其中，因为要即时编译，所以需要把被调用的函数引入内存中，代码如下：

```c++
#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

/// putchard - putchar that takes a double and returns 0.
extern "C" DLLEXPORT double putchard(double X) {
	fputc((char)X, stdout);//输出指定char字符
return 0;
}

extern "C" DLLEXPORT double printd(double X) {
	fprintf(stderr, "%d", (int)X);//输出指定double值
	return 0;
}
```

以上两个函数为引入内存的用于输出字符串的函数，相当于VSL用户可调用的库函数

同时需要手动将这两个函数添加进Module中，这样jit才能获得这两个“库函数”在内存中的地址来调用这两个函数。

```c++
 std::vector<std::string> ArgNames;
  ArgNames.push_back("char");
  auto Proto = llvm::make_unique<PrototypeAST>("putchard", std::move(ArgNames), false,
	  30);
  FunctionProtos["putchard"] = std::move(Proto);
  Function *TheFunction = getFunction("putchard");
  std::vector<std::string> ArgNames2;
  ArgNames.push_back("char");
  Proto = llvm::make_unique<PrototypeAST>("printd", std::move(ArgNames), false,
	  30);
  FunctionProtos["printd"] = std::move(Proto);
  TheFunction = getFunction("printd");
```

print语句实现就是不停的调用putchar和printd库函数来输出组成字符串的字符或者是输出算术表达式数值。

```c++
Value * PrintStatAST::codegen()
{
	KSDbgInfo.emitLocation(this);
	for (int i = 0; i < Texts.size(); i++) {
		auto Exp = std::move(Texts[i]);
        //去除unique指针类型中的ExprAST指针
		ExprAST* ptr = dynamic_cast<ExprAST*>(Exp.get());
		if (ptr != nullptr) {
			Exp.release();
		}
        //启用RTTI，利用运行时类型判断
		const char* typeName = typeid(*ptr).name();
		const char* className = typeid(TextExprAST).name();
        //判断该字符串需要输出的是字符还是数值
		if (strcmp(typeName, className) == 0) {
			std::unique_ptr<TextExprAST> text;
			text.reset((TextExprAST*)ptr);
            //若为字符串，则循环调用putchard函数，该字符串每个字符输出
			for (int j = 0; j < text->getText().size(); j++) {
				char t1 = text->getText().at(j);
				Function *CalleeF = getFunction("putchard");
				if (!CalleeF)
					return LogErrorV("Unknown function referenced");
				std::vector<Value *> ArgsV;
				ArgsV.push_back(ConstantFP::get(TheContext, APFloat((double)(t1))));
				Builder.CreateCall(CalleeF, ArgsV, "calltmp");
			}
		}
		else {
			std::unique_ptr<ExprAST> temp;
			temp.reset(ptr);
            //若为字符串，则调用printd函数，输出表达式值
			Function *CalleeF = getFunction("printd");
			if (!CalleeF)
				return LogErrorV("Unknown function referenced");
			std::vector<Value *> ArgsV;
			Value* value = temp->codegen();
			ArgsV.push_back(value);
			Builder.CreateCall(CalleeF, ArgsV, "calltmp");
		}
	}
	return ConstantFP::get(TheContext, APFloat((double)(0)));
}
```

