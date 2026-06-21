/* ============================================================
   main.cpp  —  L26 编译器主程序入口
   使用 Coco/R 生成的 Scanner + Parser，集成 VM 执行
   支持 --json 模式：输出结构化 JSON 供前端调用
   ============================================================ */
#include <iostream>
#include <cstdio>
#include <cstring>
#include "Parser.h"
#include "Scanner.h"
#include "PCode.h"
#include "VM.h"

using namespace std;
using namespace L26Project;

static string ws2s(const wchar_t *w) {
    if (!w) return "";
    char buf[256];
    int n = (int)wcslen(w);
    for (int i = 0; i < n && i < 255; ++i) buf[i] = (char)w[i];
    buf[n < 255 ? n : 255] = '\0';
    return string(buf);
}

/* wchar_t (UTF-16LE on Windows) → UTF-8 */
static string wchar_to_utf8(const wstring &ws) {
    string r;
    for (wchar_t wc : ws) {
        if (wc < 0x80) {
            r += (char)wc;
        } else if (wc < 0x800) {
            r += (char)(0xC0 | (wc >> 6));
            r += (char)(0x80 | (wc & 0x3F));
        } else {
            r += (char)(0xE0 | (wc >> 12));
            r += (char)(0x80 | ((wc >> 6) & 0x3F));
            r += (char)(0x80 | (wc & 0x3F));
        }
    }
    return r;
}

static string jsonEscape(const string &s) {
    string r;
    for (char c : s) {
        switch (c) {
            case '"':  r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            case '\n': r += "\\n";  break;
            case '\r': r += "\\r";  break;
            case '\t': r += "\\t";  break;
            default:   r += c;
        }
    }
    return r;
}

static const char* tokenKindName(int kind) {
    // 编号从 Parser.cpp SynErr 提取，与 Parser.cpp 中 IF() 谓词使用的编号一致
    switch (kind) {
        case 0:  return "EOF";
        case 1:  return "ident";
        case 2:  return "number";
        case 3:  return "{";
        case 4:  return "}";
        case 5:  return ";";
        case 6:  return "int";
        case 7:  return "bool";
        case 8:  return "set";
        case 9:  return "=";
        case 10: return "if";
        case 11: return "(";
        case 12: return ")";
        case 13: return "else";
        case 14: return "while";
        case 15: return "write";
        case 16: return "read";
        case 17: return "add";
        case 18: return "remove";
        case 19: return "<";
        case 20: return "<=";
        case 21: return ">";
        case 22: return ">=";
        case 23: return "==";
        case 24: return "!=";
        case 25: return "in";
        case 26: return "union";
        case 27: return "inter";
        case 28: return "||";
        case 29: return "&&";
        case 30: return "true";
        case 31: return "false";
        case 32: return "!";
        case 33: return "isempty";
        case 34: return "+";
        case 35: return "-";
        case 36: return "*";
        case 37: return "/";
        case 38: return ",";
        default: return "?";
    }
}

int main(int argc, char* argv[]) {
    bool jsonMode = false;
    char path[256] = {0};

    /* 解析命令行参数 */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0) {
            jsonMode = true;
        } else {
            strncpy(path, argv[i], 255);
            path[255] = '\0';
        }
    }

    if (path[0] == '\0') {
        cout << "请输入 L26 源文件路径 (例如: test1.l26): ";
        cin.getline(path, 256);
    }

    if (jsonMode) {
        /* ==========================================
           JSON 模式：输出结构化 JSON 到 stdout
           ========================================== */

        /* 第一遍：收集全部 Token */
        wchar_t *wPath = coco_string_create(path);
        Scanner *scan1 = new Scanner(wPath);
        if (scan1->buffer == NULL) {
            printf("{\"tokens\":[],\"pcode\":[],\"errors\":[{\"line\":0,\"col\":0,\"msg\":\"无法找到文件\"}]}\n");
            coco_string_delete(wPath);
            delete scan1;
            return 1;
        }

        vector<string> tokenJsons;
        vector<int> tokenLines;
        Token *tok;
        while (true) {
            tok = scan1->Scan();
            if (tok->kind == 0) break; /* EOF */
            char buf[512];
            snprintf(buf, sizeof(buf),
                "{\"type\":\"%s\",\"val\":\"%s\",\"line\":%d,\"col\":%d}",
                tokenKindName(tok->kind),
                jsonEscape(ws2s(tok->val)).c_str(),
                tok->line, tok->col);
            tokenJsons.push_back(string(buf));
            tokenLines.push_back(tok->line);
        }
        delete scan1;

        /* 第二遍：语法/语义分析，收集 P-Code */
        Scanner *scan2 = new Scanner(wPath);
        Parser *parser = new Parser(scan2);
        parser->errors->silent = true;  /* JSON 模式：抑制 wprintf 输出 */
        parser->Parse();
        parser->pcode.EmitRET();

        /* 收集错误 */
        vector<string> errorJsons;
        for (const auto &e : parser->errors->errorMsgs) {
            string utf8 = wchar_to_utf8(e.msg);
            char buf[768];
            snprintf(buf, sizeof(buf),
                "{\"line\":%d,\"col\":%d,\"msg\":\"%s\"}",
                e.line, e.col, jsonEscape(utf8).c_str());
            errorJsons.push_back(string(buf));
        }

        /* 收集 P-Code，附带估算的源码行号 */
        vector<string> pcodeJsons;
        int nTok = (int)tokenLines.size();
        int nPc  = (int)parser->pcode.code.size();
        for (int i = 0; i < nPc; i++) {
            const auto &ins = parser->pcode.code[i];
            /* 按比例估算该指令对应的 token 行号 */
            int estLine = 0;
            if (nTok > 0) {
                int ti = (int)((long long)i * nTok / nPc);
                if (ti >= nTok) ti = nTok - 1;
                estLine = tokenLines[ti];
            }
            char buf[256];
            snprintf(buf, sizeof(buf),
                "{\"f\":%d,\"l\":%d,\"a\":%d,\"line\":%d}",
                (int)ins.f, ins.l, ins.a, estLine);
            pcodeJsons.push_back(string(buf));
        }

        /* 输出 JSON */
        printf("{");
        /* tokens */
        printf("\"tokens\":[");
        for (size_t i = 0; i < tokenJsons.size(); i++) {
            if (i > 0) printf(",");
            printf("%s", tokenJsons[i].c_str());
        }
        printf("],");
        /* pcode */
        printf("\"pcode\":[");
        for (size_t i = 0; i < pcodeJsons.size(); i++) {
            if (i > 0) printf(",");
            printf("%s", pcodeJsons[i].c_str());
        }
        printf("],");
        /* errors */
        printf("\"errors\":[");
        for (size_t i = 0; i < errorJsons.size(); i++) {
            if (i > 0) printf(",");
            printf("%s", errorJsons[i].c_str());
        }
        printf("]}\n");

        delete parser;
        delete scan2;
        coco_string_delete(wPath);
        return 0;
    }

    /* ==========================================
       普通交互模式（原有逻辑）
       ========================================== */
    system("chcp 65001");   /* 解决中文乱码 */

    wchar_t *wPath = coco_string_create(path);
    Scanner *scanner = new Scanner(wPath);

    if (scanner->buffer == NULL) {
        cout << "无法找到文件！" << endl;
        coco_string_delete(wPath);
        return 1;
    }

    Parser *parser = new Parser(scanner);
    parser->Parse();

    if (parser->errors->count == 0) {
        cout << ">>> 语法/语义检查通过！" << endl;
    } else {
        cout << ">>> 发现 " << parser->errors->count << " 个错误，终止执行。" << endl;
        delete parser;
        delete scanner;
        coco_string_delete(wPath);
        return 1;
    }

    parser->pcode.EmitRET();
    parser->pcode.PrintListing();

    cout << "是否执行虚拟机？(y/n): ";
    char ch;
    cin >> ch;
    if (ch == 'y' || ch == 'Y') {
        VM vm(parser->pcode.code);
        vm.Run();
        cout << ">>> 执行完毕。" << endl;
    }

    delete parser;
    delete scanner;
    coco_string_delete(wPath);

    return 0;
}
