# C-Compiler-Class-Project-2025-12

ENGLISH version


A lightweight C compiler implementation featuring a complete pipeline from lexical analysis to intermediate code optimization and virtual machine execution.

🛠 Compilation Workflow

Source Code → next() [Lexical Analysis] → Token Stream → program()/statement()/expression() [Syntax Analysis]
→ AST Nodes → tac_visit() [TAC Generation] → TAC Instructions → optimize_tac() [Optimization]
→ eval() [Execution / VM] OR flush_assembly() [Assembly Output]

✨ Features
1. Lexical Analyzer (Lexer) ✅

The next() function is a fully-featured lexical analyzer supporting:

Identifiers & Constants: Supports variable names, numeric constants (Decimal, Octal, Hexadecimal), and string literals.

Operators & Delimiters: + - * / % = == != < <= > >= & && | || ^ ! ~ ? : ; , ( ) { } [ ]

Preprocessing: Automatically skips lines starting with #.

Comments: Full support for single-line (//) and multi-line (/* */) comments.

Keywords: char, else, enum, for, if, int, return, sizeof, while.

System Calls: Built-in recognition for open, read, close, printf, malloc, memset, memcmp, exit.

2. Syntax Analyzer (Parser) ✅

Implements Recursive Descent Parsing:

Structure: Core logic handled by program(), global_declaration(), function_declaration(), statement(), and expression().

AST Construction: Generates a comprehensive Abstract Syntax Tree with distinct node types for all supported C constructs.

3. Syntax-Directed Translation & TAC Generation ✅

The tac_visit() function transforms the AST into Three-Address Code (TAC).

Supported Translations:

✅ Assignments and complex expressions (all operators)

✅ Control Flow: if-else branching, while loops, and for loops.

✅ Function calls and return statements.

✅ Compound statement blocks.

Symbol Table: A robust symbol management system for tracking scopes and variable metadata.

4. Advanced Extensions ✅

Virtual Machine (VM): A complete stack-based VM (eval()) with a 37-instruction set to execute the generated code directly.

Code Optimization: TAC-level Constant Folding optimization via optimize_tac() (enabled with -o).

Error Reporting:

Syntax error checking in match() with line numbers and error types.

Semantic checks for undefined variables and type mismatches.

Assembly Generation: flush_assembly() generates pseudo-assembly code in a RISC-V inspired style.

🚀 Usage & Parameters

Run the compiler with the following flags to inspect different stages:

Flag	Description

-t	Tokens: Display the lexical analysis token stream.

-m	Symbol Table: Display the generated symbol table.

-a	AST: Display the Abstract Syntax Tree structure.

-c	TAC: Display the raw Three-Address Code.

-o	Optimization: Display TAC after Constant Folding optimization.

-s	Assembly: Output RISC-V style assembly code.

(None)	Execute: Run the program directly using the built-in Virtual Machine.


📊 Project Evaluation Summary
Requirement	               Status	     Note

Lexical Analyzer	          Complete	     Supports full C subset

Syntax Analyzer	          Complete	     Recursive descent method

Expression Translation	     Complete	     All operators implemented

Assignment Statements	     Complete	     Supports various types

Conditional Branching	     Complete	     if-else support

while/for Loops	          Complete	     Full loop control support

Function Calls	               Complete	     Parameter passing & Return

Declarations	               Complete	     Variable/Function declaration

Extension: VM	✅            Complete	     Full stack-based VM execution

Extension: Optimization	✅  Partial	     Constant Folding implemented

Extension: Error Handling✅   Basic	     Syntax & Semantic error reporting


中文版

源代码 → next() [词法分析] → token流 → program()/statement()/expression() [语法分析] 
     → AST节点 → tac_visit() [TAC生成] → TAC指令 → optimize_tac() [优化]
     → eval() [执行] 或 flush_assembly() [输出汇编]


已实现的基本要求：
一、词法分析器 ✅
完全实现：next() 函数是完整的词法分析器

支持：标识符、数字常量（十进制、八进制、十六进制）、字符串常量

支持运算符和分隔符：+ - * / % = == != < <= > >= & && | || ^ ! ~ ? : ; , ( ) { } [ ]

支持预处理指令（跳过 # 开头的行）

支持单行注释 // 和多行注释 

关键字识别：char else enum for if int return sizeof while

系统调用识别：open read close printf malloc memset memcmp exit

二、语法分析器 ✅
自顶向下递归子程序法：使用递归下降分析

主要函数：program(), global_declaration(), function_declaration(), statement(), expression()

支持完整的语法分析流程

三、语法制导翻译和中间代码生成 ✅
AST 构建：完整构建抽象语法树（AST 节点类型齐全）

三地址码（TAC）生成：tac_visit() 函数将 AST 转换为 TAC


支持语句翻译：

          ✅ 赋值语句

          ✅ 表达式（各种运算符）

          ✅ if 条件分支语句

          ✅ while 循环语句

          ✅ for 循环语句

          ✅ 函数调用

          ✅ 返回语句

          ✅ 复合语句块

符号表管理：完整的符号表系统

✅ 已实现的扩展要求：

1. 目标代码运行（虚拟机）✅
完整的栈式虚拟机：eval() 函数实现了虚拟机指令集

支持 37 条虚拟机指令

可以执行生成的中间代码

支持系统调用

2. 中间代码优化 ✅
常量折叠优化：optimize_tac() 函数实现了 TAC 级别的常量折叠

优化开关：通过 -o 参数启用

3. 代码错误提醒 ✅
语法错误检查：match() 函数提供错误位置和类型

语义错误检查：变量未定义、类型错误等

错误信息包含行号

✅ 额外实现的功能：
4. 多种输出模式
-t：显示词法分析结果（token 流）

-m：显示符号表

-a：显示抽象语法树（AST）

-c：显示三地址码（TAC）

-s：显示汇编代码（伪 RISC-V 风格）

-o：显示优化后的三地址码

5. 汇编代码生成
flush_assembly() 函数输出类 RISC-V 的汇编指令

📊 总结评估：
要求	实现状态	备注
词法分析器	 完全实现	     支持完整 C 子集
语法分析器	 完全实现	     递归子程序法
表达式翻译	 完全实现	     所有运算符
赋值语句	      完全实现	     支持各种赋值
条件分支	      完全实现	     if-else
while 循环	 完全实现	     完整支持
for 循环	      完全实现	     完整支持
函数调用	      完全实现	     参数传递
说明语句	      完全实现	     变量/函数声明
扩展：虚拟机	✅ 完全实现	完整栈式 VM
扩展：代码优化	✅ 部分实现	常量折叠
扩展：错误提醒	✅ 基本实现	语法/语义错误

传参说明
-s 汇编 
-t token 
-m 符号表 
-a AST 
-c TAC 
-o TAC_after_Constant_Folding_Optimization
不传参-虚拟机执行

*/
