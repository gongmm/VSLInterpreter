#include "DebugInfo.h"

//===----------------------------------------------------------------------===//
// Debug Info Support
//===----------------------------------------------------------------------===//

DebugInfo KSDbgInfo;

//DIType *DebugInfo::getDoubleTy() {
//	if (DblTy)
//		return DblTy;
//
//	DblTy = DBuilder->createBasicType("double", 64, dwarf::DW_ATE_float);
//	return DblTy;
//}

//void DebugInfo::emitLocation(ExprAST *AST) {
//	if (!AST)
//		return Builder.SetCurrentDebugLocation(DebugLoc());
//	DIScope *Scope;
//	if (LexicalBlocks.empty())
//		Scope = TheCU;
//	else
//		Scope = LexicalBlocks.back();
//	Builder.SetCurrentDebugLocation(
//		DebugLoc::get(AST->getLine(), AST->getCol(), Scope));
//}

//void DebugInfo::emitLocation(StatAST *AST) {
//	if (!AST)
//		return Builder.SetCurrentDebugLocation(DebugLoc());
//	DIScope *Scope;
//	if (LexicalBlocks.empty())
//		Scope = TheCU;
//	else
//		Scope = LexicalBlocks.back();
//	Builder.SetCurrentDebugLocation(
//		DebugLoc::get(AST->getLine(), AST->getCol(), Scope));
//}
