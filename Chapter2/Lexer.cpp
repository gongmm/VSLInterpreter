#ifndef LEXER
#define LEXER
#include "Global.h"
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <regex>

using namespace std;
//===----------------------------------------------------------------------===//
// Lexer
//===----------------------------------------------------------------------===//


/// �ж��Ƿ��ǿո�\t��\n
bool recWhitespace(int LastChar) {
	if (LastChar == ' ' || LastChar == '\t' || LastChar == '\n')
		return true;
	return false;
}
/// �ж�����һ����ʶ��
int recKeyword() {
	if (IdentifierStr == "FUNC")
		return FUNC;
	else if (IdentifierStr == "PRINT")
		return PRINT;
	else if (IdentifierStr == "RETURN")
		return RETURN;
	else if (IdentifierStr == "CONTINUE")
		return CONTINUE;
	else if (IdentifierStr == "IF")
		return IF;
	else if (IdentifierStr == "THEN")
		return THEN;
	else if (IdentifierStr == "ELSE")
		return ELSE;
	else if (IdentifierStr == "FI")
		return FI;
	else if (IdentifierStr == "WHILE")
		return WHILE;
	else if (IdentifierStr == "DO")
		return DO;
	else if (IdentifierStr == "DONE")
		return DONE;
	else if (IdentifierStr == "VAR")
		return VAR;
	return Token();
}
/// gettok - Return the next token from standard input.
int gettok() {
	static int LastChar = ' ';
	IdentifierStr = "";
	NumVal = 0;
	Text = "";
	//ʶ��ָ���������
	while (recWhitespace(LastChar)) {
		LastChar = getchar();
	}
	//ʶ��ע��
	if (LastChar == '/') {
		LastChar = getchar();
          if (LastChar == '/')
            while (LastChar != '\n')
              LastChar = getchar();
          else
            return '/';
	}
	//ʶ���ʶ��
	if (isalpha(LastChar)) {
		IdentifierStr += LastChar;
		while (isalnum(LastChar = getchar())) {
			IdentifierStr += LastChar;
		}
		int keyword = recKeyword();
		if (keyword)
			return keyword;
		return VARIABLE;
	}
	//ʶ������
	if (isdigit(LastChar)) {
		std::string NumStr;
		do {
			NumStr += LastChar;
			LastChar = getchar();
		} while (isdigit(LastChar) || LastChar == '.');

		std::regex re("(.+\..+){2,}");
		if (regex_match(NumStr, re)) {
			cout << "���Ƿ��ʣ�" << NumStr << endl;
			return 0;
		}
		NumVal = strtod(NumStr.c_str(), 0);
		return INTEGER;
	}
	//ʶ��text
	if (LastChar == '"') {
		LastChar = getchar();
		while (LastChar != '"') {
			Text += LastChar;
			LastChar = getchar();
		}
        LastChar = getchar();
		return TEXT;
	}
	//ʶ��ֵ����
	if (LastChar == ':') {
		LastChar = getchar();
          if (LastChar == '=') {
			  LastChar = getchar();
			  return ASSIGN_SYMBOL;
          }
			
	}
	if (LastChar == EOF)
		return TOKEOF;

	// ����ֻ�Ƿ����ַ���ascii��
	int ThisChar = LastChar;
	LastChar = getchar();
	return ThisChar;
}

#endif