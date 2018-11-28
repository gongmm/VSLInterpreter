#pragma once
#ifndef DEBUGINFO
#define DEBUGINFO

#include "Global.h"
struct DebugInfo {
	DICompileUnit *TheCU;
	DIType *DblTy;
	std::vector<DIScope *> LexicalBlocks;

	void emitLocation(ExprAST *ast) {
		if (!ast)
			return Builder.SetCurrentDebugLocation(DebugLoc());
		DIScope *Scope;
		if (LexicalBlocks.empty())
			Scope = TheCU;
		else
			Scope = LexicalBlocks.back();
		Builder.SetCurrentDebugLocation(
			DebugLoc::get(ast->getLine(), ast->getCol(), Scope));
	}

	void emitLocation(StatAST *ast) {
		if (!ast)
			return Builder.SetCurrentDebugLocation(DebugLoc());
		DIScope *Scope;
		if (LexicalBlocks.empty())
			Scope = TheCU;
		else
			Scope = LexicalBlocks.back();
		Builder.SetCurrentDebugLocation(
			DebugLoc::get(ast->getLine(), ast->getCol(), Scope));
	}

	DIType *getDoubleTy() {
		if (DblTy)
			return DblTy;

		DblTy = DBuilder->createBasicType("double", 64, dwarf::DW_ATE_float);
		return DblTy;
	}
};

extern DebugInfo KSDbgInfo;

#endif // !DebugInfo
