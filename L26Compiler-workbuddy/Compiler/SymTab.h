/* ============================================================
   SymTab.h  —  符号表（支持嵌套作用域）
   ============================================================ */
#ifndef L26_SYMTAB_H__
#define L26_SYMTAB_H__

#include <string>
#include <vector>
#include <map>

/* ----------------------------------------------------------
   符号表条目
---------------------------------------------------------- */
struct SymEntry {
    std::string name;       /* 变量名                    */
    std::string typName;    /* "int" / "bool" / "set"  */
    int         level;      /* 作用域层次（0 = 最外层）  */
    int         offset;     /* 当前块内偏移              */
};

/* ----------------------------------------------------------
   符号表：栈式多级作用域
   每次 EnterScope 压入一个新块；LeaveScope 弹出
---------------------------------------------------------- */
class SymTab {
public:
    int curLevel;
    std::vector<int> offsetStack;
    std::vector< std::map<std::string, SymEntry> > scopes;

    void Init() {
        curLevel = -1;
        scopes.clear();
        offsetStack.clear();
    }

    void EnterScope() {
        curLevel++;
        scopes.push_back(std::map<std::string, SymEntry>());
        offsetStack.push_back(0);
    }

    void LeaveScope() {
        if (curLevel < 0) return;
        scopes.pop_back();
        offsetStack.pop_back();
        curLevel--;
    }

    bool Declare(const std::string &name, const std::string &typ) {
        if (curLevel < 0) return false;
        std::map<std::string, SymEntry> &top = scopes[curLevel];
        if (top.count(name)) return false;
        SymEntry e;
        e.name    = name;
        e.typName = typ;
        e.level   = curLevel;
        e.offset  = offsetStack[curLevel]++;
        top[name] = e;
        return true;
    }

    SymEntry *Lookup(const std::string &name) {
        for (int i = curLevel; i >= 0; --i) {
            std::map<std::string, SymEntry> &scope = scopes[i];
            if (scope.count(name)) return &scope[name];
        }
        return NULL;
    }

    int CurrentOffset() {
        if (curLevel < 0) return 0;
        return offsetStack[curLevel];
    }
};

#endif /* L26_SYMTAB_H__ */
