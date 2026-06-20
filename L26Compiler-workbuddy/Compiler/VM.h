/* ============================================================
   VM.h  —  类 P-Code 虚拟机（L26 运行时执行引擎）
   栈式机模型：s[]栈 + 四个寄存器 p/b/t
   对标 Pcode.md §一、§四
   ============================================================ */
#ifndef L26_VM_H__
#define L26_VM_H__

#include <vector>
#include <set>
#include <string>
#include <cstdio>
#include <cstdlib>
#include "PCode.h"

namespace L26Project {

#define STACKSIZE     2048
#define CXMAX         1024
#define MAX_SET_SIZE  200   /* PDF 要求：集合元素上限 200 */

/* 集合辅助池（s[] 只存 int，set 类型额外存在此处） */
static std::set<int> g_setPool[STACKSIZE];

class VM {
public:
    std::vector<PInstr>  &code;

    int   s[STACKSIZE];
    int   p;
    int   b;
    int   t;
    bool  halted;
    int   pendingSetCnt;

    std::vector<std::string> outputLog;

    explicit VM(std::vector<PInstr> &c)
        : code(c), p(0), b(0), t(-1), halted(false), pendingSetCnt(0) {}

    /* 按层次差 l 求基址（沿静态链向上 l 层）*/
    int Base(int l, int cur_b) const {
        int b1 = cur_b;
        while (l > 0) { b1 = s[b1]; l--; }
        return b1;
    }

    std::string SetToStr(const std::set<int> &st) const {
        std::string r = "{";
        bool first = true;
        for (int x : st) {
            if (!first) r += ", ";
            r += std::to_string(x);
            first = false;
        }
        return r + "}";
    }

    /* -------- 主执行入口 -------- */
    void Run() {
        p = 0; b = 0; t = 0;
        s[0] = 0; s[1] = 0; s[2] = 0;
        halted = false;
        outputLog.clear();

        while (!halted && p < (int)code.size()) {
            PInstr ins = code[p];
            p++;
            ExecInstr(ins);
        }
    }

    /* -------- 单步执行 -------- */
    void Step() {
        if (halted || p < 0 || p >= (int)code.size()) {
            halted = true;
            return;
        }
        PInstr ins = code[p];
        p++;
        ExecInstr(ins);
    }

private:
    void ExecInstr(const PInstr &ins) {
        switch (ins.f) {
        case F_LIT:
            t++; s[t] = ins.a;
            break;
        case F_OPR:
            ExecOPR(ins);
            break;
        case F_LOD:
            t++; s[t] = s[Base(ins.l, b) + ins.a];
            break;
        case F_STO:
            s[Base(ins.l, b) + ins.a] = s[t]; t--;
            break;
        case F_CAL:
            s[t+1] = Base(ins.l, b);
            s[t+2] = b;
            s[t+3] = p;
            b = t+1;
            p = ins.a;
            break;
        case F_INT:
            t += ins.a;
            break;
        case F_JMP:
            p = ins.a;
            break;
        case F_JPC:
            if (s[t] == 0) p = ins.a;
            t--;
            break;
        }
    }

    void ExecOPR(const PInstr &ins) {
        int a = ins.a;
        switch (a) {

        case OPR_RET:
            t = b - 1;
            p = s[b + 2];
            b = s[b + 1];
            if (p == 0) halted = true;
            break;

        case OPR_NEG:  s[t] = -s[t]; break;
        case OPR_ODD:  s[t] = s[t] & 1; break;
        case OPR_NOT:  s[t] = s[t] ? 0 : 1; break;

        case OPR_ADD: t--; s[t] = s[t] + s[t+1]; break;
        case OPR_SUB: t--; s[t] = s[t] - s[t+1]; break;
        case OPR_MUL: t--; s[t] = s[t] * s[t+1]; break;
        case OPR_DIV:
            if (s[t] == 0) { fprintf(stderr, "除以零错误\n"); halted = true; break; }
            t--; s[t] = s[t] / s[t+1];
            break;

        case OPR_EQ: t--; s[t] = (s[t] == s[t+1]) ? 1 : 0; break;
        case OPR_NE: t--; s[t] = (s[t] != s[t+1]) ? 1 : 0; break;
        case OPR_LT: t--; s[t] = (s[t] <  s[t+1]) ? 1 : 0; break;
        case OPR_GE: t--; s[t] = (s[t] >= s[t+1]) ? 1 : 0; break;
        case OPR_GT: t--; s[t] = (s[t] >  s[t+1]) ? 1 : 0; break;
        case OPR_LE: t--; s[t] = (s[t] <= s[t+1]) ? 1 : 0; break;

        case OPR_AND: t--; s[t] = (s[t] && s[t+1]) ? 1 : 0; break;
        case OPR_OR:  t--; s[t] = (s[t] || s[t+1]) ? 1 : 0; break;

        case OPR_WRITE: {
            char buf[64]; sprintf(buf, "%d", s[t]);
            printf("%s", buf); outputLog.push_back(buf); t--;
            break;
        }
        case OPR_WRITE_BOOL: {
            const char *bs = s[t] ? "true" : "false";
            printf("%s", bs); outputLog.push_back(bs); t--;
            break;
        }
        case OPR_WRITE_SET: {
            std::string str = SetToStr(g_setPool[t]);
            printf("%s", str.c_str()); outputLog.push_back(str); t--;
            break;
        }
        case OPR_WRITELN:
            printf("\n"); outputLog.push_back("\n");
            break;
        case OPR_READ: {
            int val = 0;
            printf("请输入整数: "); scanf("%d", &val);
            t++; s[t] = val;
            break;
        }
        case OPR_READBOOL: {
            int val = 0;
            printf("请输入 bool(0/1): "); scanf("%d", &val);
            t++; s[t] = val ? 1 : 0;
            break;
        }

        /* 集合操作 */
        case OPR_SET_MK: {
            int cnt = ins.a;   /* EmitSetMake 已将 a 设置为元素个数 */
            if (cnt > MAX_SET_SIZE) {
                fprintf(stderr, "运行时错误: 集合字面量元素数 %d 超过上限 %d\n", cnt, MAX_SET_SIZE);
                halted = true; break;
            }
            std::set<int> ns;
            for (int k = 0; k < cnt; k++) { ns.insert(s[t]); t--; }
            t++;
            s[t] = t;
            g_setPool[t] = ns;
            break;
        }
        case OPR_SET_UNION: {
            std::set<int> res = g_setPool[t-1];
            for (int x : g_setPool[t]) res.insert(x);
            if ((int)res.size() > MAX_SET_SIZE) {
                fprintf(stderr, "运行时错误: 并集结果大小 %d 超过上限 %d\n", (int)res.size(), MAX_SET_SIZE);
                halted = true; break;
            }
            t--;
            s[t] = t;
            g_setPool[t] = res;
            break;
        }
        case OPR_SET_INTER: {
            std::set<int> res;
            for (int x : g_setPool[t-1]) if (g_setPool[t].count(x)) res.insert(x);
            t--;
            s[t] = t;
            g_setPool[t] = res;
            break;
        }
        case OPR_SET_IN: {
            int elem = s[t-1];
            int found = g_setPool[t].count(elem) ? 1 : 0;
            t--;
            s[t] = found;
            break;
        }
        case OPR_SET_ISEMPTY:
            s[t] = g_setPool[t].empty() ? 1 : 0;
            break;
        case OPR_SET_ADD: {
            int elem = s[t];
            if ((int)g_setPool[t-1].size() >= MAX_SET_SIZE) {
                fprintf(stderr, "运行时错误: add 时集合已达上限 %d\n", MAX_SET_SIZE);
                halted = true; break;
            }
            g_setPool[t-1].insert(elem);
            t--;
            break;
        }
        case OPR_SET_REMOVE: {
            int elem = s[t];
            g_setPool[t-1].erase(elem);
            t--;
            break;
        }
        case OPR_SET_EQ: {
            int eq = (g_setPool[t-1] == g_setPool[t]) ? 1 : 0;
            t--;
            s[t] = eq;
            break;
        }

        case OPR_SET_NE: {
            int ne = (g_setPool[t-1] != g_setPool[t]) ? 1 : 0;
            t--;
            s[t] = ne;
            break;
        }

        default:
            fprintf(stderr, "未知 OPR 子码: %d\n", a);
            halted = true;
            break;
        }
    }

}; /* class VM */

} /* namespace L26Project */

#endif /* L26_VM_H__ */
