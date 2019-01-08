添加JIT编译器支持以及优化支持，实现即时编译：

## 添加JIT编译器

1. 为创建本机目标代码准备环境，初始化目标，初始化JIT：

```c++
 //初始化TheJIT
  TheJIT = llvm::make_unique<KaleidoscopeJIT>();
 // 准备环境,初始化目标
  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();
```

选择目标：

不同目标的操作系统可能会有不同值，利用LLVM中的`sys::getDefaultTargetTriple`方法可以动态获得当前机器的操作系统。

```c++
auto TargetTriple = sys::getDefaultTargetTriple();
```

为了在特定机器上实现即时编译，我们需要为Module指定一个描述目标主机的 target triple 字符串（对应目标的操作系统），可以利用我们刚刚获得的对应当前机器操作系统的target triple 字符串。

```c++
 TheModule->setTargetTriple(TargetTriple);
```

根据目标的（当前操作系统的）数据布局（data layout），指定模块数据如何在内存中布局，为目标创建数据层

```c++
  TheModule->setDataLayout(TheTargetMachine->createDataLayout());
```

将Module加入到JIT中，实现即时编译，其中，句柄H用于操作完成后释放Module有关数据的内存空间。

```c++
auto H = TheJIT->addModule(std::move(TheModule));
if (hasMainFunction) {
    //找到main函数入口地址，若没有则报错
	  auto ExprSymbol = TheJIT->findSymbol("main");
	  assert(ExprSymbol && "Function not found");
    //调用main函数，输出结果
	  double(*FP)() = (double(*)())(intptr_t)cantFail(ExprSymbol.getAddress());
	  fprintf(stderr, "Evaluated to %f\n", FP());
  }
  else {
	  fprintf(stderr, "don't have main function!\n");
  }
  // Delete the anonymous expression module from the JIT.
  TheJIT->removeModule(H);
```

## 生成目标代码

利用之前获得的当前目标的target triple字符串获得对应体系结构的目标（target）

```c++
 std::string Error;
 auto Target = TargetRegistry::lookupTarget(TargetTriple, Error);
```

生成一个包含目标机器的完整描述的对象TargetMachine，帮助定位到我们想要的功能或特定CPU ，这里我们选择的是通用的CPU。

```c++
 auto CPU = "generic";
  auto Features = "";

  TargetOptions opt;
  auto RM = Optional<Reloc::Model>();
  auto TheTargetMachine =
	  Target->createTargetMachine(TargetTriple, CPU, Features, opt, RM);
```

之前实现即时编译JIT时已经指定了Module数据在内存中对应当前目标机器应有的布局。

所以我们可以直接利用Module生成目标代码。生成代码前，先定义代码将写入的文件的位置，我们设置为output.o文件。

```c++
  auto Filename = "output.o";
  std::error_code EC;
  raw_fd_ostream dest(Filename, EC, sys::fs::F_None);

  if (EC) {
	  errs() << "Could not open file: " << EC.message();
	  return 1;
  }
```

接下来，我们定义一个发出目标代码的pass，然后运行该pass，将目标代码写入文件，刷新

```c++
 legacy::PassManager pass;
  auto FileType = TargetMachine::CGFT_ObjectFile;

  if (TheTargetMachine->addPassesToEmitFile(pass, dest, FileType)) {
	  errs() << "TheTargetMachine can't emit a file of this type";
	  return 1;
  }

  pass.run(*TheModule);
  dest.flush();

```

## 添加优化pass

利用LLVM 优化通道，优化生成的代码

- 我们通过建立一个 `FunctionPassManager` （FPM）来管理LLVM优化。

  ```c++
   std::unique_ptr<legacy::FunctionPassManager> TheFPM;
  ```

- 利用函数InitializeModuleAndPassManager来创建和初始化Module和通道管理器并向FunctionPassManager内添加Pass来优化代码。

  ```c++
  void InitializeModule() {
  	// Open a new module.
  	TheModule = llvm::make_unique<Module>("my cool jit", TheContext);
  	TheModule->setDataLayout(TheJIT->getTargetMachine().createDataLayout());
  
  	// Create a new pass manager attached to it.
  	TheFPM = llvm::make_unique<legacy::FunctionPassManager>(TheModule.get());
  
  	// Do simple "peephole" optimizations and bit-twiddling optzns.
  	TheFPM->add(createInstructionCombiningPass());
  	// Reassociate expressions.
  	TheFPM->add(createReassociatePass());
  	// Eliminate Common SubExpressions.
  	TheFPM->add(createGVNPass());
  	// Simplify the control flow graph (deleting unreachable blocks, etc).
  	TheFPM->add(createCFGSimplificationPass());
  
  	TheFPM->doInitialization();
  }
  ```

- 当一个新的function被创建后（在 `FunctionAST::codegen()`内），但还未返回前，运行该FPM：

  ```c++
  TheFPM->run(*TheFunction);
  ```

  优化代码前后对比

  代码：

  ```
  FUNC main(){
  VAR i,r,a
  a:=1
  r:=10
  i:=1
  WHILE 11-i
  DO{
  r:=a*r
  i:=i+1
  }
  DONE
  }
  ```

  优化前：

  ![QQ图片20190108040513](C:\Users\郑晓欣\Desktop\QQ图片20190108040513.png)

  优化后：

  ![QQ浏览器截图20190107221235](C:\Users\郑晓欣\Desktop\QQ浏览器截图20190107221235.png)

优化后代码删除了函数中没有返回或是作为其他函数参数的局部变量的乘法操作语句，生成的IR代码变短