/* ============================================================
   PCode.h  —  类 P-Code 指令集 & 代码生成器
   指令格式：f  l  a
     f：操作码（fct 枚举）
     l：层次差（0 表示无层次概念）
     a：地址 / 常量 / 运算子码
   注：枚举成员避免使用 CAL/INT，防止与 Windows SDK 宏冲突
   ============================================================ */
#ifndef L26_PCODE_H__
#define L26_PCODE_H__

#include <vector>
#include <string>
#include <cstdio>

/* ----------------------------------------------------------
   操作码枚举（对标 Pcode.md §六）
   前缀 F_ 避免与 Windows SDK 中 CAL/INT 等宏冲突
---------------------------------------------------------- */
enum fct {
    F_LIT,  /* LIT 0 a  — 常数 a 压栈                     */
    F_OPR,  /* OPR 0 a  — 运算指令，a 为子码               */
    F_LOD,  /* LOD l a  — 取变量值压栈（层差l，偏移a）     */
    F_STO,  /* STO l a  — 存变量值（层差l，偏移a）         */
    F_CAL,  /* CAL l a  — 调用过程（层差l，入口地址a）     */
    F_INT,  /* INT 0 a  — 开辟栈空间（局部变量数+3）       */
    F_JMP,  /* JMP 0 a  — 无条件跳转到地址 a               */
    F_JPC   /* JPC 0 a  — 栈顶==0 则跳转到 a，T--          */
};

/* Listing 输出时还原助记符名称（不含 F_ 前缀） */
static const char *FctName(fct f) {
    switch (f) {
        case F_LIT: return "LIT";
        case F_OPR: return "OPR";
        case F_LOD: return "LOD";
        case F_STO: return "STO";
        case F_CAL: return "CAL";
        case F_INT: return "INT";
        case F_JMP: return "JMP";
        case F_JPC: return "JPC";
        default:    return "???";
    }
}

/* ----------------------------------------------------------
   OPR 指令子码（a 字段）
---------------------------------------------------------- */
enum OprCode {
    OPR_RET     = 0,
    OPR_NEG     = 1,
    OPR_ADD     = 2,
    OPR_SUB     = 3,
    OPR_MUL     = 4,
    OPR_DIV     = 5,
    OPR_ODD     = 6,
    OPR_EQ      = 8,
    OPR_NE      = 9,
    OPR_LT      = 10,
    OPR_GE      = 11,
    OPR_GT      = 12,
    OPR_LE      = 13,
    OPR_WRITE   = 14,
    OPR_WRITELN = 15,
    OPR_READ    = 16,
    OPR_AND     = 18,
    OPR_OR      = 19,
    OPR_NOT     = 20,
    OPR_READBOOL   = 116,
    OPR_SET_MK     = 201,
    OPR_SET_UNION  = 202,
    OPR_SET_INTER  = 203,
    OPR_SET_IN     = 204,
    OPR_SET_ISEMPTY= 205,
    OPR_SET_ADD    = 206,
    OPR_SET_REMOVE = 207,
    OPR_SET_EQ     = 208,
    OPR_WRITE_BOOL = 209,
    OPR_WRITE_SET  = 210,
    OPR_SET_NE     = 211
};

/* ----------------------------------------------------------
   单条 P-Code 指令（三字段格式）
---------------------------------------------------------- */
struct PInstr {
    fct f;
    int l;
    int a;
    PInstr() : f(F_LIT), l(0), a(0) {}
    PInstr(fct f_, int l_, int a_) : f(f_), l(l_), a(a_) {}
};

/* ----------------------------------------------------------
   PCodeGen  —  代码生成器
---------------------------------------------------------- */
class PCodeGen {
public:
    std::vector<PInstr> code;

    void Init() { code.clear(); }

    int CurrentAddr() const { return (int)code.size(); }

    int Emit(fct f, int l, int a) {
        code.push_back(PInstr(f, l, a));
        return (int)code.size() - 1;
    }

    void PatchJump(int instrIdx) {
        code[instrIdx].a = CurrentAddr();
    }

    void EmitLIT(int val)            { Emit(F_LIT, 0, val); }
    void EmitLOD(int level, int off) { Emit(F_LOD, level, off); }
    void EmitSTO(int level, int off) { Emit(F_STO, level, off); }
    void EmitCAL(int level, int addr){ Emit(F_CAL, level, addr); }
    void EmitINT(int size)           { Emit(F_INT, 0, size); }
    void EmitRET()                   { Emit(F_OPR, 0, OPR_RET); }

    void EmitNEG() { Emit(F_OPR, 0, OPR_NEG); }
    void EmitBinOp(char op) {
        switch (op) {
            case '+': Emit(F_OPR, 0, OPR_ADD); break;
            case '-': Emit(F_OPR, 0, OPR_SUB); break;
            case '*': Emit(F_OPR, 0, OPR_MUL); break;
            case '/': Emit(F_OPR, 0, OPR_DIV); break;
        }
    }

    void EmitRelOp(int relOp) {
        /* relOp: 1=<  2=<=  3=>  4=>=  5===  6=!= */
        static const int tbl[] = { 0,
            OPR_LT, OPR_LE, OPR_GT, OPR_GE, OPR_EQ, OPR_NE };
        Emit(F_OPR, 0, tbl[relOp]);
    }

    void EmitBoolOp(int kind) { Emit(F_OPR, 0, kind==1 ? OPR_AND : OPR_OR); }
    void EmitBoolNot()        { Emit(F_OPR, 0, OPR_NOT); }

    int EmitJMP(int addr) { return Emit(F_JMP, 0, addr); }
    int EmitJPC(int addr) { return Emit(F_JPC, 0, addr); }

    void EmitWRITE(const std::string &typ) {
        if      (typ == "bool") Emit(F_OPR, 0, OPR_WRITE_BOOL);
        else if (typ == "set")  Emit(F_OPR, 0, OPR_WRITE_SET);
        else                    Emit(F_OPR, 0, OPR_WRITE);
    }
    void EmitWRITELN() { Emit(F_OPR, 0, OPR_WRITELN); }
    void EmitREAD()    { Emit(F_OPR, 0, OPR_READ); }

    void EmitSetMake(int cnt) {
        int idx = Emit(F_OPR, 0, OPR_SET_MK);
        code[idx].a = cnt;
    }
    void EmitSetUnion()   { Emit(F_OPR, 0, OPR_SET_UNION); }
    void EmitSetInter()   { Emit(F_OPR, 0, OPR_SET_INTER); }
    void EmitSetIn()      { Emit(F_OPR, 0, OPR_SET_IN); }
    void EmitSetIsEmpty() { Emit(F_OPR, 0, OPR_SET_ISEMPTY); }
    void EmitSetAdd()     { Emit(F_OPR, 0, OPR_SET_ADD); }
    void EmitSetRemove()  { Emit(F_OPR, 0, OPR_SET_REMOVE); }
    void EmitSetEQ()      { Emit(F_OPR, 0, OPR_SET_EQ); }
    void EmitSetNE()      { Emit(F_OPR, 0, OPR_SET_NE); }

    void PrintListing() const {
        printf("\n===== P-Code Listing =====\n");
        for (int i = 0; i < (int)code.size(); ++i) {
            const PInstr &ins = code[i];
            printf("(%3d)  %-4s %2d  %d\n",
                   i, FctName(ins.f), ins.l, ins.a);
        }
        printf("==========================\n\n");
    }
};

#endif /* L26_PCODE_H__ */
