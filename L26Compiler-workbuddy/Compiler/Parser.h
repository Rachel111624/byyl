

#if !defined(L26Project_COCO_PARSER_H__)
#define L26Project_COCO_PARSER_H__



#include <string>
#include <vector>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "SymTab.h"
#include "PCode.h"
#include "Scanner.h"

namespace L26Project {


struct ErrorEntry {
	int line, col;
	std::wstring msg;
};

class Errors {
public:
	int count;			// number of errors detected
	bool silent;                    // suppress wprintf output (for JSON mode)
	std::vector<ErrorEntry> errorMsgs;  // stored errors for JSON mode

	Errors();
	void SynErr(int line, int col, int n);
	void Error(int line, int col, const wchar_t *s);
	void Warning(int line, int col, const wchar_t *s);
	void Warning(const wchar_t *s);
	void Exception(const wchar_t *s);

}; // Errors

class Parser {
private:
	enum {
		_EOF=0,
		_ident=1,
		_number=2
	};
	int maxT;

	Token *dummyToken;
	int errDist;
	int minErrDist;

	void SynErr(int n);
	void Get();
	void Expect(int n);
	bool StartOf(int s);
	void ExpectWeak(int n, int follow);
	bool WeakSeparator(int n, int syFol, int repFol);

public:
	Scanner *scanner;
	Errors  *errors;

	Token *t;			// last recognized token
	Token *la;			// lookahead token

SymTab   symTab;
    /* P-Code 生成器（三字段 f l a 格式）*/
    PCodeGen pcode;

    /* Coco/R C++ 版专有：由构造函数自动调用 */
    void Init() {
        symTab.Init();
        pcode.Init();
    }

    /* 语义错误辅助 */
    void SemError(const wchar_t *msg) { SemErr(msg); }

    /* wchar_t* -> std::string */
    std::string ws2s(const wchar_t *w) {
        if (!w) return "";
        char buf[256];
        int n = (int)wcslen(w);
        for (int i = 0; i < n && i < 255; ++i) buf[i] = (char)w[i];
        buf[n < 255 ? n : 255] = '\0';
        return std::string(buf);
    }

    /* 计算层次差：当前层(curLevel) - 定义层(defLevel) */
    int LevelDiff(int defLevel) {
        return symTab.curLevel - defLevel;
    }

    /* ==========================================================
       真实 token 编号 (从 Parser.cpp 生成代码提取)：
        1=_ident 2=_number  3="{"   4="}"   5=";"   6="int"
        7="bool" 8="set"     9="="  10="if" 11="("  12=")"
       13="else" 14="while" 15="write" 16="read" 17="add" 18="remove"
       19="<"  20="<="      21=">"   22=">="  23="=="  24="!="
       25="in"
       26="||"  27="&&"     28="true" 29="false" 30="!"
       31="+"   32="-"      33="*"   34="/"   35=","   36="union"
       37="inter" 38="isempty"
       ========================================================== */

    /* 前瞻谓词：是否是集合表达式起始
         "{"          → 集合字面量
         ident 且 la->next 是 "union"(35) 或 "inter"(36)
                      → 集合运算（s union t）
     */
    bool IsSetExprStart() {
        if (la->kind == 3) return true;
        if (la->kind == _ident && la->next != NULL) {
            int nk = la->next->kind;
            if (nk == 36 || nk == 37) return true;
        }
        return false;
    }

    /* 前瞻谓词：AExpr 是否能以此 token 开头
        只有纯粹的算术表达式开头才会返回 true
        如果 la 是 ident 且 la->next 使表达式变为 bool/set，
        则不算纯 AExpr 开头，应走 BExpr/SetExpr 分支
        正确 FIRST(AExpr) = { _ident(1), _number(2), "-"(26), "(" (11) }
     */
    bool IsAExprStart() {
        int k = la->kind;
        if (!(k == _ident || k == _number || k == 32 /* "-" */ || k == 11 /* "(" */))
            return false;
        /* 检查 la->next：若使表达式变为 bool/set，则不算纯 AExpr */
        if (la->next != NULL) {
            int nk = la->next->kind;
            /* "in"(37) "union"(35) "inter"(36) "&&"(30) "||"(29) */
            if (nk == 25 || nk == 36 || nk == 37 || nk == 27 || nk == 26)
                return false;
        }
        return true;
    }

    /* 前瞻谓词：是否是 bool 表达式起始
         "true"(31) / "false"(32) / "!"(33) → 直接返回 true
         AExpr 且 la->next 是关系运算符或 "in"   → 返回 bool
     */
    bool IsBExprOnly() {
        int k = la->kind;
        /* true / false / ! */
        if (k == 28 || k == 29 || k == 30) return true;
        /* AExpr 后接关系运算符或 "in" → 关系表达式/成员测试 */
        if (k == _ident || k == _number || k == 32 || k == 11) {
            if (la->next != NULL) {
                int nk = la->next->kind;
                /* 关系运算符: 19-24, 或 "in": 25 */
                if ((nk >= 19 && nk <= 24) || nk == 25) return true;
            }
        }
        return false;
    }

    /* 前瞻谓词：是否是集合成员测试起始
         "isempty"(38) 直接匹配
         AExpr "in" ident → la 能启动 AExpr 且 la->next 是 "in"(25)
     */
    bool IsSetTest() {
        if (la->kind == 38) return true;
        int k = la->kind;
        if (k == _ident || k == _number || k == 32 || k == 11) {
            if (la->next != NULL && la->next->kind == 25) return true;
        }
        return false;
    }

/*==============================================================
  CHARACTERS
==============================================================*/


	Parser(Scanner *scanner);
	~Parser();
	void SemErr(const wchar_t* msg);

	void L26();
	void Block();
	void Decls(int &cnt);
	void Stmts();
	void Decl(int &cnt);
	void Type(std::string &typName);
	void Stmt();
	void AssignStmt();
	void IfStmt();
	void WhileStmt();
	void IOStmt();
	void SetOpStmt();
	void Expr(std::string &typ);
	void BExpr();
	void AExpr(std::string &typ);
	void SetExpr(std::string &typ);
	void BTerm();
	void BFactor();
	void ATerm(std::string &typ);
	void AFactor(std::string &typ);

	void Parse();

}; // end Parser

} // namespace


#endif

