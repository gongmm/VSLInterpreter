#pragma once
#include "Global.h"
#include <fstream>
#include <iostream>
#include <string>
#include <stack>

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
static std::ofstream fout;
static bool isFirstOpenFile = true;
static std::stack<std::string> outputStack;
void outputToTxt(std::string str) {
  if (isFirstOpenFile) {
    fout.open("output.txt", std::ios::trunc);
	isFirstOpenFile = false;
  }else fout.open("output.txt", std::ios::app);
  if (fout.is_open()) {
    fout << str;
  }
  fout.close();
}

/// LogError* - These are little helper functions for error handling.
std::unique_ptr<ExprAST> LogError(const char *Str) {
   fprintf(stderr, "Error: %s\n", Str);
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