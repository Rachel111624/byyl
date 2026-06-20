# L26 Compiler — Makefile
# 【参考 ExpressionProject tasks.txt】对标构建命令
# 使用 MinGW g++，Windows 环境

CXX      = g++
CXXFLAGS = -g -std=c++11 -Wall
TARGET   = l26compiler.exe
SRCS     = Scanner.cpp Parser.cpp main.cpp

all: generate compile

# 步骤 1：用 Coco/R 从 ATG 文法生成 Scanner.cpp / Parser.cpp
generate:
	coco.exe L26.atg -namespace L26Project

# 步骤 2：编译生成可执行文件
compile:
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET)

# 一键：生成 + 编译
build: generate compile

# 清理生成文件
clean:
	del /Q Scanner.cpp Parser.cpp Scanner.h Parser.h $(TARGET) 2>nul || true

.PHONY: all generate compile build clean
