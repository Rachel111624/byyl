

#include <wchar.h>
#include "Parser.h"
#include "Scanner.h"


namespace L26Project {


void Parser::SynErr(int n) {
	if (errDist >= minErrDist) errors->SynErr(la->line, la->col, n);
	errDist = 0;
}

void Parser::SemErr(const wchar_t* msg) {
	if (errDist >= minErrDist) errors->Error(t->line, t->col, msg);
	errDist = 0;
}

void Parser::Get() {
	for (;;) {
		t = la;
		la = scanner->Scan();
		if (la->kind <= maxT) { ++errDist; break; }

		if (dummyToken != t) {
			dummyToken->kind = t->kind;
			dummyToken->pos = t->pos;
			dummyToken->col = t->col;
			dummyToken->line = t->line;
			dummyToken->next = NULL;
			coco_string_delete(dummyToken->val);
			dummyToken->val = coco_string_create(t->val);
			t = dummyToken;
		}
		la = t;
	}
}

void Parser::Expect(int n) {
	if (la->kind==n) Get(); else { SynErr(n); }
}

void Parser::ExpectWeak(int n, int follow) {
	if (la->kind == n) Get();
	else {
		SynErr(n);
		while (!StartOf(follow)) Get();
	}
}

bool Parser::WeakSeparator(int n, int syFol, int repFol) {
	if (la->kind == n) {Get(); return true;}
	else if (StartOf(repFol)) {return false;}
	else {
		SynErr(n);
		while (!(StartOf(syFol) || StartOf(repFol) || StartOf(0))) {
			Get();
		}
		return StartOf(syFol);
	}
}

void Parser::L26() {
		Block();
		pcode.EmitRET(); 
}

void Parser::Block() {
		int intAddr; int declCnt = 0; 
		Expect(3 /* "{" */);
		symTab.EnterScope();
		intAddr = pcode.Emit(F_INT, 0, 0);
		
		Decls(declCnt);
		pcode.code[intAddr].a = declCnt + 3; 
		Stmts();
		Expect(4 /* "}" */);
		symTab.LeaveScope(); 
}

void Parser::Decls(int &cnt) {
		while (la->kind == 6 /* "int" */ || la->kind == 7 /* "bool" */ || la->kind == 8 /* "set" */) {
			Decl(cnt);
		}
}

void Parser::Stmts() {
		while (StartOf(1)) {
			Stmt();
		}
}

void Parser::Decl(int &cnt) {
		std::string typName; std::string varName; 
		Type(typName);
		Expect(_ident);
		varName = ws2s(t->val);
		if (!symTab.Declare(varName, typName))
		   SemError(L"变量重复声明");
		cnt++;
		
		Expect(5 /* ";" */);
}

void Parser::Type(std::string &typName) {
		if (la->kind == 6 /* "int" */) {
			Get();
			typName = "int";  
		} else if (la->kind == 7 /* "bool" */) {
			Get();
			typName = "bool"; 
		} else if (la->kind == 8 /* "set" */) {
			Get();
			typName = "set"; 
		} else SynErr(40);
}

void Parser::Stmt() {
		switch (la->kind) {
		case _ident: {
			AssignStmt();
			break;
		}
		case 10 /* "if" */: {
			IfStmt();
			break;
		}
		case 14 /* "while" */: {
			WhileStmt();
			break;
		}
		case 15 /* "write" */: case 16 /* "read" */: {
			IOStmt();
			break;
		}
		case 3 /* "{" */: {
			Block();
			break;
		}
		case 17 /* "add" */: case 18 /* "remove" */: {
			SetOpStmt();
			break;
		}
		default: SynErr(41); break;
		}
}

void Parser::AssignStmt() {
		std::string varName; std::string exprType;
		SymEntry *e = NULL; 
		Expect(_ident);
		varName = ws2s(t->val);
		e = symTab.Lookup(varName);
		if (!e) SemError(L"未声明的变量");
		
		Expect(9 /* "=" */);
		Expr(exprType);
		if (e) {
		   if (e->typName != exprType)
		       SemError(L"赋值类型不匹配");
		   pcode.EmitSTO(LevelDiff(e->level),
		                 e->offset + 3);
		}
		
		Expect(5 /* ";" */);
}

void Parser::IfStmt() {
		int labelElse, labelEnd; 
		Expect(10 /* "if" */);
		Expect(11 /* "(" */);
		BExpr();
		Expect(12 /* ")" */);
		labelElse = pcode.EmitJPC(-1); 
		Stmt();
		labelEnd = pcode.EmitJMP(-1);
		pcode.PatchJump(labelElse);
		
		if (la->kind == 13 /* "else" */) {
			Get();
			Stmt();
		}
		pcode.PatchJump(labelEnd); 
}

void Parser::WhileStmt() {
		int labelTop, labelEnd; 
		Expect(14 /* "while" */);
		labelTop = pcode.CurrentAddr(); 
		Expect(11 /* "(" */);
		BExpr();
		Expect(12 /* ")" */);
		labelEnd = pcode.EmitJPC(-1); 
		Stmt();
		pcode.EmitJMP(labelTop);
		pcode.PatchJump(labelEnd);
		
}

void Parser::IOStmt() {
		std::string exprType; 
		if (la->kind == 15 /* "write" */) {
			Get();
			Expr(exprType);
			pcode.EmitWRITE(exprType);
			pcode.EmitWRITELN();
			
			Expect(5 /* ";" */);
		} else if (la->kind == 16 /* "read" */) {
			Get();
			Expect(_ident);
			std::string varName = ws2s(t->val);
			SymEntry *e = symTab.Lookup(varName);
			if (!e) SemError(L"未声明的变量");
			pcode.EmitREAD();
			if (e) pcode.EmitSTO(LevelDiff(e->level),
			                    e->offset + 3);
			
			Expect(5 /* ";" */);
		} else SynErr(42);
}

void Parser::SetOpStmt() {
		std::string varName; std::string dummy;
		SymEntry *e = NULL; 
		if (la->kind == 17 /* "add" */) {
			Get();
			Expect(_ident);
			varName = ws2s(t->val);
			e = symTab.Lookup(varName);
			if (!e || e->typName != "set")
			   SemError(L"add 目标必须是 set 类型");
			if (e) pcode.EmitLOD(LevelDiff(e->level),
			                     e->offset + 3);
			
			AExpr(dummy);
			pcode.EmitSetAdd();
			if (e) pcode.EmitSTO(LevelDiff(e->level),
			                     e->offset + 3);
			
			Expect(5 /* ";" */);
		} else if (la->kind == 18 /* "remove" */) {
			Get();
			Expect(_ident);
			varName = ws2s(t->val);
			e = symTab.Lookup(varName);
			if (!e || e->typName != "set")
			   SemError(L"remove 目标必须是 set 类型");
			if (e) pcode.EmitLOD(LevelDiff(e->level),
			                     e->offset + 3);
			
			AExpr(dummy);
			pcode.EmitSetRemove();
			if (e) pcode.EmitSTO(LevelDiff(e->level),
			                     e->offset + 3);
			
			Expect(5 /* ";" */);
		} else SynErr(43);
}

void Parser::Expr(std::string &typ) {
		std::string dummy; int relOp = 0; std::string prevTyp; 
		if (IsSetExprStart()) {
			SetExpr(typ);
		} else if (IsAExprStart()) {
			AExpr(typ);
			if (StartOf(2)) {
				switch (la->kind) {
				case 19 /* "<" */: {
					Get();
					relOp = 1; 
					break;
				}
				case 20 /* "<=" */: {
					Get();
					relOp = 2; 
					break;
				}
				case 21 /* ">" */: {
					Get();
					relOp = 3; 
					break;
				}
				case 22 /* ">=" */: {
					Get();
					relOp = 4; 
					break;
				}
				case 23 /* "==" */: {
					Get();
					relOp = 5; 
					break;
				}
				case 24 /* "!=" */: {
					Get();
					relOp = 6; 
					break;
				}
				}
				AExpr(dummy);
				prevTyp = typ; typ = "bool";
				if (prevTyp == "set") {
				   if (relOp == 5) pcode.EmitSetEQ();
				   else if (relOp == 6) pcode.EmitSetNE();
				   else SemError(L"集合只支持 == 与 !=");
				} else {
				   pcode.EmitRelOp(relOp);
				}
				
			}
			if (la->kind == 25 /* "in" */) {
				Get();
				Expect(_ident);
				typ = "bool";
				std::string setName = ws2s(t->val);
				SymEntry *se = symTab.Lookup(setName);
				if (!se || se->typName != "set")
				   SemError(L"in 右侧必须是 set");
				if (se) pcode.EmitLOD(LevelDiff(se->level), se->offset + 3);
				pcode.EmitSetIn();
				
			}
			if (la->kind == 26 /* "union" */ || la->kind == 27 /* "inter" */) {
				if (la->kind == 26 /* "union" */) {
					Get();
					Expect(_ident);
					typ = "set";
					std::string nb = ws2s(t->val);
					SymEntry *eb = symTab.Lookup(nb);
					if (!eb || eb->typName != "set")
					   SemError(L"union 右侧必须是 set");
					if (eb) pcode.EmitLOD(LevelDiff(eb->level), eb->offset + 3);
					pcode.EmitSetUnion();
					
				} else {
					Get();
					Expect(_ident);
					typ = "set";
					std::string nb = ws2s(t->val);
					SymEntry *eb = symTab.Lookup(nb);
					if (!eb || eb->typName != "set")
					   SemError(L"inter 右侧必须是 set");
					if (eb) pcode.EmitLOD(LevelDiff(eb->level), eb->offset + 3);
					pcode.EmitSetInter();
					
				}
			}
		} else if (StartOf(3)) {
			BExpr();
			typ = "bool"; 
		} else SynErr(44);
}

void Parser::BExpr() {
		BTerm();
		while (la->kind == 28 /* "||" */) {
			Get();
			BTerm();
			pcode.EmitBoolOp(2); 
		}
}

void Parser::AExpr(std::string &typ) {
		char op; typ = "int"; 
		ATerm(typ);
		while (la->kind == 34 /* "+" */ || la->kind == 35 /* "-" */) {
			if (la->kind == 34 /* "+" */) {
				Get();
				op = '+'; 
			} else {
				Get();
				op = '-'; 
			}
			ATerm(typ);
			pcode.EmitBinOp(op); 
		}
}

void Parser::SetExpr(std::string &typ) {
		typ = "set"; int elemCnt = 0;
		std::string na, nb, elemTyp;
		SymEntry *ea, *eb; 
		if (la->kind == 3 /* "{" */) {
			Get();
			if (StartOf(4)) {
				AExpr(elemTyp);
				elemCnt++; 
				while (la->kind == 38 /* "," */) {
					Get();
					AExpr(elemTyp);
					elemCnt++; 
				}
			}
			Expect(4 /* "}" */);
			pcode.EmitSetMake(elemCnt); 
		} else if (la->kind == _ident) {
			Get();
			na = ws2s(t->val);
			ea = symTab.Lookup(na);
			if (!ea || ea->typName != "set")
			   SemError(L"union/inter 左侧必须是 set");
			if (ea) pcode.EmitLOD(LevelDiff(ea->level),
			                     ea->offset + 3);
			
			if (la->kind == 26 /* "union" */) {
				Get();
				Expect(_ident);
				nb = ws2s(t->val);
				eb = symTab.Lookup(nb);
				if (!eb || eb->typName != "set")
				   SemError(L"union 右侧必须是 set");
				if (eb) pcode.EmitLOD(LevelDiff(eb->level),
				                     eb->offset + 3);
				pcode.EmitSetUnion();
				
			} else if (la->kind == 27 /* "inter" */) {
				Get();
				Expect(_ident);
				nb = ws2s(t->val);
				eb = symTab.Lookup(nb);
				if (!eb || eb->typName != "set")
				   SemError(L"inter 右侧必须是 set");
				if (eb) pcode.EmitLOD(LevelDiff(eb->level),
				                     eb->offset + 3);
				pcode.EmitSetInter();
				
			} else SynErr(45);
		} else SynErr(46);
}

void Parser::BTerm() {
		BFactor();
		while (la->kind == 29 /* "&&" */) {
			Get();
			BFactor();
			pcode.EmitBoolOp(1); 
		}
}

void Parser::BFactor() {
		std::string dummy; std::string prevTypBF; int relOp = 0; 
		if (la->kind == 30 /* "true" */) {
			Get();
			pcode.EmitLIT(1); 
		} else if (la->kind == 31 /* "false" */) {
			Get();
			pcode.EmitLIT(0); 
		} else if (la->kind == 32 /* "!" */) {
			Get();
			BFactor();
			pcode.EmitBoolNot(); 
		} else if (la->kind == 11 /* "(" */) {
			Get();
			BExpr();
			Expect(12 /* ")" */);
		} else if (la->kind == 33 /* "isempty" */) {
			Get();
			Expect(11 /* "(" */);
			Expect(_ident);
			std::string vn = ws2s(t->val);
			SymEntry *e = symTab.Lookup(vn);
			if (!e || e->typName != "set")
			   SemError(L"isempty 参数必须是 set");
			if (e) pcode.EmitLOD(LevelDiff(e->level), e->offset + 3);
			pcode.EmitSetIsEmpty();
			
			Expect(12 /* ")" */);
		} else if (StartOf(4)) {
			AExpr(dummy);
			if (StartOf(5)) {
				if (la->kind == 25 /* "in" */) {
					Get();
					Expect(_ident);
					std::string vn2 = ws2s(t->val);
					SymEntry *e2 = symTab.Lookup(vn2);
					if (!e2 || e2->typName != "set")
					   SemError(L"in 右侧必须是 set");
					if (e2) pcode.EmitLOD(LevelDiff(e2->level), e2->offset + 3);
					pcode.EmitSetIn();
					
				} else {
					switch (la->kind) {
					case 19 /* "<" */: {
						Get();
						relOp = 1; 
						break;
					}
					case 20 /* "<=" */: {
						Get();
						relOp = 2; 
						break;
					}
					case 21 /* ">" */: {
						Get();
						relOp = 3; 
						break;
					}
					case 22 /* ">=" */: {
						Get();
						relOp = 4; 
						break;
					}
					case 23 /* "==" */: {
						Get();
						relOp = 5; 
						break;
					}
					case 24 /* "!=" */: {
						Get();
						relOp = 6; 
						break;
					}
					}
					AExpr(dummy);
					prevTypBF = dummy;
					if (prevTypBF == "set") {
					   if (relOp == 5) pcode.EmitSetEQ();
					   else if (relOp == 6) pcode.EmitSetNE();
					   else SemError(L"集合只支持 == 与 !=");
					} else {
					   pcode.EmitRelOp(relOp);
					}
					
				}
			}
		} else SynErr(47);
}

void Parser::ATerm(std::string &typ) {
		char op; typ = "int"; 
		AFactor(typ);
		while (la->kind == 36 /* "*" */ || la->kind == 37 /* "/" */) {
			if (la->kind == 36 /* "*" */) {
				Get();
				op = '*'; 
			} else {
				Get();
				op = '/'; 
			}
			AFactor(typ);
			pcode.EmitBinOp(op); 
		}
}

void Parser::AFactor(std::string &typ) {
		typ = "int"; 
		if (la->kind == _ident) {
			Get();
			std::string vn = ws2s(t->val);
			SymEntry *e = symTab.Lookup(vn);
			if (!e) {
			   SemError(L"未声明的变量");
			} else {
			   typ = e->typName;
			   pcode.EmitLOD(LevelDiff(e->level),
			                 e->offset + 3);
			}
			
		} else if (la->kind == _number) {
			Get();
			int val = atoi(ws2s(t->val).c_str());
			pcode.EmitLIT(val);
			typ = "int";
			
		} else if (la->kind == 35 /* "-" */) {
			Get();
			AFactor(typ);
			pcode.EmitNEG(); 
		} else if (la->kind == 11 /* "(" */) {
			Get();
			AExpr(typ);
			Expect(12 /* ")" */);
		} else SynErr(48);
}




// If the user declared a method Init and a mehtod Destroy they should
// be called in the contructur and the destructor respctively.
//
// The following templates are used to recognize if the user declared
// the methods Init and Destroy.

template<typename T>
struct ParserInitExistsRecognizer {
	template<typename U, void (U::*)() = &U::Init>
	struct ExistsIfInitIsDefinedMarker{};

	struct InitIsMissingType {
		char dummy1;
	};
	
	struct InitExistsType {
		char dummy1; char dummy2;
	};

	// exists always
	template<typename U>
	static InitIsMissingType is_here(...);

	// exist only if ExistsIfInitIsDefinedMarker is defined
	template<typename U>
	static InitExistsType is_here(ExistsIfInitIsDefinedMarker<U>*);

	enum { InitExists = (sizeof(is_here<T>(NULL)) == sizeof(InitExistsType)) };
};

template<typename T>
struct ParserDestroyExistsRecognizer {
	template<typename U, void (U::*)() = &U::Destroy>
	struct ExistsIfDestroyIsDefinedMarker{};

	struct DestroyIsMissingType {
		char dummy1;
	};
	
	struct DestroyExistsType {
		char dummy1; char dummy2;
	};

	// exists always
	template<typename U>
	static DestroyIsMissingType is_here(...);

	// exist only if ExistsIfDestroyIsDefinedMarker is defined
	template<typename U>
	static DestroyExistsType is_here(ExistsIfDestroyIsDefinedMarker<U>*);

	enum { DestroyExists = (sizeof(is_here<T>(NULL)) == sizeof(DestroyExistsType)) };
};

// The folloing templates are used to call the Init and Destroy methods if they exist.

// Generic case of the ParserInitCaller, gets used if the Init method is missing
template<typename T, bool = ParserInitExistsRecognizer<T>::InitExists>
struct ParserInitCaller {
	static void CallInit(T *t) {
		// nothing to do
	}
};

// True case of the ParserInitCaller, gets used if the Init method exists
template<typename T>
struct ParserInitCaller<T, true> {
	static void CallInit(T *t) {
		t->Init();
	}
};

// Generic case of the ParserDestroyCaller, gets used if the Destroy method is missing
template<typename T, bool = ParserDestroyExistsRecognizer<T>::DestroyExists>
struct ParserDestroyCaller {
	static void CallDestroy(T *t) {
		// nothing to do
	}
};

// True case of the ParserDestroyCaller, gets used if the Destroy method exists
template<typename T>
struct ParserDestroyCaller<T, true> {
	static void CallDestroy(T *t) {
		t->Destroy();
	}
};

void Parser::Parse() {
	t = NULL;
	la = dummyToken = new Token();
	la->val = coco_string_create(L"Dummy Token");
	Get();
	L26();
	Expect(0);
}

Parser::Parser(Scanner *scanner) {
	maxT = 39;

	ParserInitCaller<Parser>::CallInit(this);
	dummyToken = NULL;
	t = la = NULL;
	minErrDist = 2;
	errDist = minErrDist;
	this->scanner = scanner;
	errors = new Errors();
}

bool Parser::StartOf(int s) {
	const bool T = true;
	const bool x = false;

	static bool set[6][41] = {
		{T,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x},
		{x,T,x,T, x,x,x,x, x,x,T,x, x,x,T,T, T,T,T,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x},
		{x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,T, T,T,T,T, T,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x},
		{x,T,T,x, x,x,x,x, x,x,x,T, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,T,T, T,T,x,T, x,x,x,x, x},
		{x,T,T,x, x,x,x,x, x,x,x,T, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,T, x,x,x,x, x},
		{x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,T, T,T,T,T, T,T,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x}
	};



	return set[s][la->kind];
}

Parser::~Parser() {
	ParserDestroyCaller<Parser>::CallDestroy(this);
	delete errors;
	delete dummyToken;
}

Errors::Errors() {
	count = 0; silent = false;
}

void Errors::SynErr(int line, int col, int n) {
	wchar_t* s;
	switch (n) {
			case 0: s = coco_string_create(L"EOF expected"); break;
			case 1: s = coco_string_create(L"ident expected"); break;
			case 2: s = coco_string_create(L"number expected"); break;
			case 3: s = coco_string_create(L"\"{\" expected"); break;
			case 4: s = coco_string_create(L"\"}\" expected"); break;
			case 5: s = coco_string_create(L"\";\" expected"); break;
			case 6: s = coco_string_create(L"\"int\" expected"); break;
			case 7: s = coco_string_create(L"\"bool\" expected"); break;
			case 8: s = coco_string_create(L"\"set\" expected"); break;
			case 9: s = coco_string_create(L"\"=\" expected"); break;
			case 10: s = coco_string_create(L"\"if\" expected"); break;
			case 11: s = coco_string_create(L"\"(\" expected"); break;
			case 12: s = coco_string_create(L"\")\" expected"); break;
			case 13: s = coco_string_create(L"\"else\" expected"); break;
			case 14: s = coco_string_create(L"\"while\" expected"); break;
			case 15: s = coco_string_create(L"\"write\" expected"); break;
			case 16: s = coco_string_create(L"\"read\" expected"); break;
			case 17: s = coco_string_create(L"\"add\" expected"); break;
			case 18: s = coco_string_create(L"\"remove\" expected"); break;
			case 19: s = coco_string_create(L"\"<\" expected"); break;
			case 20: s = coco_string_create(L"\"<=\" expected"); break;
			case 21: s = coco_string_create(L"\">\" expected"); break;
			case 22: s = coco_string_create(L"\">=\" expected"); break;
			case 23: s = coco_string_create(L"\"==\" expected"); break;
			case 24: s = coco_string_create(L"\"!=\" expected"); break;
			case 25: s = coco_string_create(L"\"in\" expected"); break;
			case 26: s = coco_string_create(L"\"union\" expected"); break;
			case 27: s = coco_string_create(L"\"inter\" expected"); break;
			case 28: s = coco_string_create(L"\"||\" expected"); break;
			case 29: s = coco_string_create(L"\"&&\" expected"); break;
			case 30: s = coco_string_create(L"\"true\" expected"); break;
			case 31: s = coco_string_create(L"\"false\" expected"); break;
			case 32: s = coco_string_create(L"\"!\" expected"); break;
			case 33: s = coco_string_create(L"\"isempty\" expected"); break;
			case 34: s = coco_string_create(L"\"+\" expected"); break;
			case 35: s = coco_string_create(L"\"-\" expected"); break;
			case 36: s = coco_string_create(L"\"*\" expected"); break;
			case 37: s = coco_string_create(L"\"/\" expected"); break;
			case 38: s = coco_string_create(L"\",\" expected"); break;
			case 39: s = coco_string_create(L"??? expected"); break;
			case 40: s = coco_string_create(L"invalid Type"); break;
			case 41: s = coco_string_create(L"invalid Stmt"); break;
			case 42: s = coco_string_create(L"invalid IOStmt"); break;
			case 43: s = coco_string_create(L"invalid SetOpStmt"); break;
			case 44: s = coco_string_create(L"invalid Expr"); break;
			case 45: s = coco_string_create(L"invalid SetExpr"); break;
			case 46: s = coco_string_create(L"invalid SetExpr"); break;
			case 47: s = coco_string_create(L"invalid BFactor"); break;
			case 48: s = coco_string_create(L"invalid AFactor"); break;

		default:
		{
			wchar_t format[20];
			coco_swprintf(format, 20, L"error %d", n);
			s = coco_string_create(format);
		}
		break;
	}
	if (!silent) wprintf(L"-- line %d col %d: %ls\n", line, col, s);
		errorMsgs.push_back({line, col, std::wstring(s)});
	coco_string_delete(s);
	count++;
}

void Errors::Error(int line, int col, const wchar_t *s) {
	if (!silent) wprintf(L"-- line %d col %d: %ls\n", line, col, s);
		errorMsgs.push_back({line, col, std::wstring(s)});
	count++;
}

void Errors::Warning(int line, int col, const wchar_t *s) {
	wprintf(L"-- line %d col %d: %ls\n", line, col, s);
}

void Errors::Warning(const wchar_t *s) {
	wprintf(L"%ls\n", s);
}

void Errors::Exception(const wchar_t* s) {
	wprintf(L"%ls", s); 
	exit(1);
}

} // namespace

