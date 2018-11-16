#pragma once
#include "Global.h"
//���������ʱע��
//#include <fstream>
//#include <iostream>
#include <string>

std::string IdentifierStr;
double NumVal;
std::string Text;
//===-------------------
// Parser
//===--------------------
int CurTok = 0;
int getNextToken() { return CurTok = gettok(); }

/// BinopPrecedence - This holds the precedence for each binary operator that is
/// defined.
std::map<char, int> BinopPrecedence;

/// ����ָʾ��
int indent = 0;

/// ������ļ�
//���������ʱע��
//static std::ofstream fout;
static bool isFirstOpenFile = true;
void outputToTxt(std::string str) {
	//����Ϊ���벻������ʱע��
 /* if (isFirstOpenFile) {
    fout.open("output.txt", std::ios::trunc);
	isFirstOpenFile = false;
  }else fout.open("output.txt", std::ios::app);
  if (fout.is_open()) {
    fout << str;
  }
  fout.close();*/
}
//���������ʱע��
//static std::ofstream errorFout;
static bool isErrorFirstOpenFile = true;
/// LogError* - These are little helper functions for error handling.
std::unique_ptr<ExprAST> LogError(const char *Str) {
	//����Ϊ���벻������ʱע��
 /* if (isErrorFirstOpenFile) {
    errorFout.open("error.txt", std::ios::trunc);
    isErrorFirstOpenFile = false;
  } else
    errorFout.open("error.txt", std::ios::app);
  if (errorFout.is_open()) {
    errorFout << Str;
  }
  errorFout.close();*/
  /*std::string str("Error: ");
  str.append(Str);
  outputToTxt(str);*/
  return nullptr;
}
std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
  LogError(Str);
  return nullptr;
}
//�����Statement������
std::unique_ptr<StatAST> LogErrorS(const char *Str) {
  LogError(Str);
  return nullptr;
}

//===----------------------------------------------------------------------===//
// Code Generation
//===----------------------------------------------------------------------===//

LLVMContext TheContext;
//IRBuilder<> Builder(TheContext);
std::unique_ptr<Module> TheModule = llvm::make_unique<Module>("my cool jit", TheContext);
//std::map<std::string, Value *> NamedValues;
std::map<std::string, AllocaInst *> NamedValues;

//std::unique_ptr<legacy::FunctionPassManager> TheFPM;
//std::unique_ptr<KaleidoscopeJIT> TheJIT;
std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;