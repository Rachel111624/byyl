/* ============================================================
   main.cpp  —  L26 编译器主程序入口
   使用 Coco/R 生成的 Scanner + Parser，集成 VM 执行
   ============================================================ */
#include <iostream>
#include "Parser.h"
#include "Scanner.h"
#include "PCode.h"
#include "VM.h"

using namespace std;
using namespace L26Project;

int main(int argc, char* argv[]) {
    system("chcp 65001");   /* 解决中文乱码 */

    char path[256];
    if (argc > 1) {
        /* 从命令行参数读取文件路径 */
        strncpy(path, argv[1], 255);
        path[255] = '\0';
    } else {
        cout << "请输入 L26 源文件路径 (例如: test1.l26): ";
        cin.getline(path, 256);
    }

    /* 将 char* 转换为 Coco/R 所需的 wchar_t* */
    wchar_t *wPath = coco_string_create(path);

    /* 初始化扫描器 */
    Scanner *scanner = new Scanner(wPath);

    /* 检查文件加载状态 */
    if (scanner->buffer == NULL) {
        cout << "无法找到文件！" << endl;
        coco_string_delete(wPath);
        return 1;
    }

    /* 初始化解析器并执行语法/语义分析 */
    Parser *parser = new Parser(scanner);
    parser->Parse();

    /* 输出解析结果 */
    if (parser->errors->count == 0) {
        cout << ">>> 语法/语义检查通过！" << endl;
    } else {
        cout << ">>> 发现 " << parser->errors->count << " 个错误，终止执行。" << endl;
        delete parser;
        delete scanner;
        coco_string_delete(wPath);
        return 1;
    }

    /* 输出 P-Code 清单 */
    parser->pcode.EmitRET();
    parser->pcode.PrintListing();

    /* 询问是否执行虚拟机 */
    cout << "是否执行虚拟机？(y/n): ";
    char ch;
    cin >> ch;
    if (ch == 'y' || ch == 'Y') {
        VM vm(parser->pcode.code);
        vm.Run();
        cout << ">>> 执行完毕。" << endl;
    }

    /* 清理内存 */
    delete parser;
    delete scanner;
    coco_string_delete(wPath);

    return 0;
}
