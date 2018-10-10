# JIT和优化支持设计文档

- 目标：添加JIT编译器支持以及优化支持

## 折叠琐碎的常量

- 我们不需要在AST中实现该支持，因为在LLVM中，当调用生成LLVM IR的函数时，都会通过LLVM IR builder，这个builder会自动检查是否有可折叠的常量。如果有，它会折叠该常量并返回该常量而非创建一条指令。
- 所以在生成代码时，应都使用`IRBuilder`.



## LLVM 优化通道

### 单个函数优化

- 我们需要建立一个 `FunctionPassManager` （FPM）来管理LLVM优化。

1. 我们需要一个函数来创建和初始化Module和通道管理器。

```c
void InitializeModuleAndPassManager(void) {
  // 打开一个新的Module（TheModule为全局变量）
  TheModule = llvm::make_unique<Module>("my cool jit", TheContext);

  // 创建一个新的通道管理器，并绑定于Module（TheFPM为全局变量）
  TheFPM = llvm::make_unique<FunctionPassManager>(TheModule.get());

  // 以下为添加优化通道
  // 添加简单的"peephole"优化和bit-twiddling优化（直接操作数据字节）
  TheFPM->add(createInstructionCombiningPass());
  // 重新关联表达式
  TheFPM->add(createReassociatePass());
  // 消除常见子表达式
  TheFPM->add(createGVNPass());
  // 简化控制流图
  TheFPM->add(createCFGSimplificationPass());

  TheFPM->doInitialization();
}
```

​	这些优化通道会重新组织表达式，当出现相同的子字符串时，会重用子字符串而不会重新申请新的子字符串。

​	可以翻阅https://llvm.org/docs/Passes.html 查看已定义的Pass。

2. 当一个新的function被创建后（在 `FunctionAST::codegen()`），但又还未返回前，运行该FPM：

   ```C
   if (Value *RetVal = Body->codegen()) {
     Builder.CreateRet(RetVal);
     verifyFunction(*TheFunction);
   
     // 优化function
     TheFPM->run(*TheFunction);
   
     return TheFunction;
   }
   ```



## 添加JIT编译器

1. 为创建本机目标代码准备环境，声明和初始化JIT：

   ```c
   // 声明全局变量JIT
   std::unique_ptr<KaleidoscopeJIT> TheJIT;
   ...
   int main() {
     // 准备环境
     InitializeNativeTarget();
     InitializeNativeTargetAsmPrinter();
     InitializeNativeTargetAsmParser();
   
     BinopPrecedence['<'] = 10;
     BinopPrecedence['+'] = 20;
     BinopPrecedence['-'] = 20;
     BinopPrecedence['*'] = 40;
   
     fprintf(stderr, "ready> ");
     getNextToken();
   
     // 初始化JIT
     TheJIT = llvm::make_unique<KaleidoscopeJIT>();
   
     MainLoop();
   
     return 0;
   }
   ```

2. 为JIT创建数据层

   ```C
   void InitializeModuleAndPassManager(void) {
     // 打开新的module
     TheModule = llvm::make_unique<Module>("my cool jit", TheContext);
     // 创建数据层
     TheModule->setDataLayout(TheJIT->getTargetMachine().createDataLayout());
   
     // 创建新的FPM，并绑定module
     TheFPM = llvm::make_unique<FunctionPassManager>(TheModule.get());
     ...
   ```

3. 在新生成的module中重新生成函数声明：

   ```C
   // 创建全局变量FunctionProtos，用以保存每个函数最近的原型
   std::unique_ptr<KaleidoscopeJIT> TheJIT;
   
   ...
   
   /*
   	取代TheModule->getFunction()，直接搜索TheModule来查找已存在的函数声明，若没有找到，则从FunctionProtos中生成新的声明。每次函数调用时使用getFunction()使每次返回的函数都是最近一次声明的该名称的函数。
   */
   Function *getFunction(std::string Name) {
     // 首先, 查看是否已将该函数添加到当前模块中，是则直接返回
     if (auto *F = TheModule->getFunction(Name))
       return F;
   
     // 若没有，则查看我们是否可以从已存在的原型中生成声明代码
     auto FI = FunctionProtos.find(Name);
     if (FI != FunctionProtos.end())
       return FI->second->codegen();
   
     // 如果现有原型不存在, 则返回 null
     return nullptr;
   }
   
   ...
   
   // 取代TheModule->getFunction()
   Value *CallExprAST::codegen() {
     // 在全局模块表中查找名称
     Function *CalleeF = getFunction(Callee);
   
   ...
   
   // 首先更新FunctionProtos，再调用getFunction()
   Function *FunctionAST::codegen() {
     // 将原型的所有转移到FunctionProtos的映射，但保持其引用，以便后续使用
     auto &P = *Proto;
     FunctionProtos[Proto->getName()] = std::move(Proto);
     // FunctionProtos存储最近一次该名称的函数原型，
     // 因此新的函数声明需要在FunctionProtos中更新它
     Function *TheFunction = getFunction(P.getName());
     if (!TheFunction)
       return nullptr;
       
      ...
   }
   
   // 更新
   static void HandleDefinition() {
     if (auto FnAST = ParseDefinition()) {
       if (auto *FnIR = FnAST->codegen()) {
         fprintf(stderr, "Read function definition:");
         FnIR->print(errs());
         fprintf(stderr, "\n");
         // 声明完成一个函数后，使用JIT的addModule方法将TheModule即模块内新生成的函数
         // 传递到JIT中，生成模块中的函数的代码
         TheJIT->addModule(std::move(TheModule));
         // 在添加完后，初始化模块
         InitializeModuleAndPassManager();
       }
     } else {
       // Skip token for error recovery.
        getNextToken();
     }
   }
   ```







