#C-Compiler-Class-Project-2025-12

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
Requirement	Status	Note
Lexical Analyzer	Complete	Supports full C subset
Syntax Analyzer	Complete	Recursive descent method
Expression Translation	Complete	All operators implemented
Assignment Statements	Complete	Supports various types
Conditional Branching	Complete	if-else support
while/for Loops	Complete	Full loop control support
Function Calls	Complete	Parameter passing & Return
Declarations	Complete	Variable/Function declaration
Extension: VM	✅ Complete	Full stack-based VM execution
Extension: Optimization	✅ Partial	Constant Folding implemented
Extension: Error Handling	✅ Basic	Syntax & Semantic error reporting
