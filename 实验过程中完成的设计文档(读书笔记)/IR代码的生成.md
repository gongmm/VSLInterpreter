# 中间代码生成

by 王子昂 蒋颖

### 代码生成的准备

1. 在每个AST类中定义虚拟代码生成（codegen）方法，用于输出该AST节点的IR及其依赖的所有内容，并返回一个LLVM Value对象。

```c
/// ExprAST - Base class for all expression nodes.
class ExprAST {
public:
  virtual ~ExprAST() {}
  virtual Value *codegen() = 0;
};

/// NumberExprAST - Expression class for numeric literals like "1.0".
class NumberExprAST : public ExprAST {
  double Val;

public:
  NumberExprAST(double Val) : Val(Val) {}
  virtual Value *codegen();
};
...
```

* Value是一个类，表示LLVM中的“静态单一分配（SSA）寄存器，它的值在相关指令执行时计算的，并且在指令重新执行之前它不会获得新值。SSA值是没有办法“改变”的。

2. LogError方法，用于报告在代码生成期间发现的错误。

```c
//静态变量将在代码生成期间使用。TheContext是一个不透明的对象，拥有许多核心LLVM数据结构，例如类型和常量值表，用一个实例传递到需要它的API中。
static LLVMContext TheContext;

// Builder是一个辅助对象，用来生成LLVM指令。Builder能够监测插入指令的当前位置，并具有创建新指令的方法。
static IRBuilder<> Builder(TheContext);

//包含函数和全局变量的LLVM结构。它是LLVM IR用于包含代码的顶级结构。它将拥有我们生成的所有中间代码的内存，这就是codegen（）方法返回未加工的Value*而不是unique_ptr 的原因。
static std::unique_ptr<Module> TheModule;

//记录定义于当前作用域内的变量及与之相对应的LLVM表示(相当于符号表) 
//如函数的参数
static std::map<std::string, Value *> NamedValues;

Value *ErrorV(const char *Str) { Error(Str); return 0; }
```

在生成代码之前必须先设置好`Builder`对象，指明写入代码的位置。

### 表达式代码生成

#### 数值常量 ####

```c
Value *NumberExprAST::Codegen() {
  //新建并返回了一个ConstantFP对象
  return ConstantFP::get(getGlobalContext(), APFloat(Val));
}
```

* 数字常量用`ConstantFP`类表示，它在内部保存`APFloat`中的数值。
* `APFloat`可以放置任意精度的浮点常数

* 常量都是唯一的并且共享。API使用`foo :: get(...)`而不是`new foo(...)`或`foo :: Create(...)`

#### 变量 ####

```c
Value *VariableExprAST::codegen() {
 //确认给定的变量名是否存在于映射表中
  Value *V = NamedValues[Name];
  return V ? V : ErrorV("Unknown variable name");
}
```

#### 二元运算 ####

```c
Value *BinaryExprAST::Codegen() {
  //处理表达式的左侧
  Value *L = LHS->Codegen();
  //再处理表达式的右侧
  Value *R = RHS->Codegen();
  
  if (L == 0 || R == 0) return 0;
  //计算整个二元表达式的值  
  switch (Op) {
  	 case '+': 
          return Builder.CreateFAdd(L, R, "addtmp");
 	 case '-': 
          return Builder.CreateFSub(L, R, "subtmp");
 	 case '*': 
          return Builder.CreateFMul(L, R, "multmp");
     case '/': 
          return Builder.CreateFDiv(L, R, "divtmp");
 	 case '<':
    	  L = Builder.CreateFCmpULT(L, R, "cmptmp");
          // 将输入的整数视作无符号数，并将之转换成浮点数
    	  // 将布尔值 0/1 转换为浮点数 0.0 或 1.0
   		  return Builder.CreateUIToFP(L, Type::getDoubleTy(getGlobalContext()),
                                "booltmp");
  	  default: return ErrorV("invalid binary operator");
  }
}
```

* `IRBuilder`知道在哪里插入新创建的指令，我们所要做的就是指定要创建的指令（例如使用`CreateFAdd`），使用哪些操作数（此处为L和R），并且我们可以选择为生成的指令起名。这个名字只是一个提示，是可选的，只是让我们阅读中间代码更方便。

* LLVM指令受严格规则的约束，例如，add指令的Left和Right运算符必须具有相同的类型，add的结果类型必须与操作数类型匹配。

* `fcmp`指令始终返回 1 bit 的整数，把`fcmp`指令与`uitofp`指令结合起来，将输入视为无符号值将其输入整数转换为浮点值。

* 若使用`sitofp`指令，$<$ 运算符将返回0.0或-1.0。

#### 函数调用 ####

```c
Value *CallExprAST::codegen() {
  // Module的符号表中查找函数名
  Function *CalleeF = TheModule->getFunction(Callee);
  if (!CalleeF)
     return ErrorV("Unknown function referenced");

 // 判断参数个数，看找到的函数与用户指定的函数是否匹配
  if (CalleeF->arg_size() != Args.size())
     return ErrorV("Incorrect # arguments passed");

  // 递归地生成传入的各个参数的代码
  std::vector<Value *> ArgsV;
  for (unsigned i = 0, e = Args.size(); i != e; ++i) {
    ArgsV.push_back(Args[i]->codegen());
    if (!ArgsV.back())
      return 0;
  }
// 创建call指令
  return Builder.CreateCall(CalleeF, ArgsV, "calltmp");
}
```

* Module是容纳即时编译的函数的容器，通过为每个函数指定与用户指定的名称相同的名称，我们可以使用LLVM symbol table来解析我们的函数名称。

* LLVM默认使用原生的C调用惯例，允许调用标准库函数，如“sin”和“cos”，而不需要额外的工作。

### 函数代码生成

#### 原型 ####

```c
Function *PrototypeAST::Codegen() {
  // 创建函数类型:  double(double,double) 等.
  std::vector<Type*> Doubles(Args.size(),
                             Type::getDoubleTy(getGlobalContext()));
  // 创建出一个参数个数不可变（false指定），放回一个double的函数类型
  FunctionType *FT = FunctionType::get(Type::getDoubleTy(getGlobalContext()),
                                       Doubles, false);
  // 创建与该函数原型相对应的IR函数，包括类型、链接方式和函数名等信息。
  // 指定了该函数待插入的模块
  // “ExternalLinkage”表示该函数可能定义于当前模块之外，且可以被当前模块之外的函数调用
  // Name 是用户定义的，在“TheModule” 符号表中注册
  Function *F = Function::Create(FT, Function::ExternalLinkage, Name, TheModule);
  
  // 根据原型中的名字，为函数参数设置名字，并不是必要的
  // 这样做，允许后续代码直接引用其名称的参数，而不必在Prototype AST中查找它们
  unsigned Idx = 0;
  for (auto &Arg : F->args())
    Arg.setName(Args[Idx++]);
  return F;
}
```

* Prototype既用于函数体，也用于外部函数声明，但在VSL中不支持外部函数声明
* 此函数返回“Function”而不是“Value”，因为“函数原型”描述的是函数的对外接口，而不是表达式计算出的值

#### 函数定义 ####

```c
Function *FunctionAST::codegen() {
//在TheModule的符号表中搜索此函数的现有版本
  Function *TheFunction = TheModule->getFunction(Proto->getName());
    
//如果Module :: getFunction返回null，则不存在先前版本，因此我们将从Prototype中编译一个。
  if (!TheFunction)
    TheFunction = Proto->codegen();

  if (!TheFunction)
    return 0;

  if (!TheFunction->empty())
    return (Function*)ErrorV("Function cannot be redefined.");
```

**建立Builder**

```c
// 创建一个新的名为“entry”的基本块，插入到TheFunction
BasicBlock *BB = BasicBlock::Create(TheContext, "entry", TheFunction);
//将新指令插入到新基本块的末尾
Builder.SetInsertPoint(BB);

// 在NamedValues map中记录函数参数，以便它们可以被VariableExprAST节点访问
NamedValues.clear();
for (auto &Arg : TheFunction->args())
  NamedValues[Arg.getName()] = &Arg;
```

* LLVM中的基本块是定义控制流图的函数的重要部分。

```c
//设置了插入点并填充了NamedValues后，调用codegen()方法来获取函数的根表达式
if (Value *RetVal = Body->codegen()) {
  
  // 创建一个LLVM ret指令，完成该函数。
  Builder.CreateRet(RetVal);

  // 验证生成的代码，检查一致性。
  verifyFunction(*TheFunction);

  return TheFunction;
}
```

```c
 // 读取体时出错，删除函数。
  TheFunction->eraseFromParent();
  return 0;
}
```

* 使用eraseFromParent方法来允许用户重新定义之前错误输入的函数：如果我们没有删除它，它将存在于带有正文的符号表中，从而阻止将来重新定义。

```c
ready> 4+5;
//解析器将顶级表达式转换为我们的匿名函数。
Read top-level expression:
define double @0() {
entry:
  ret double 9.000000e+00
}
```

示例代码将对codegen的调用插入到“HandleDefinition”，“HandleExtern”等函数中，然后转储出LLVM IR。

