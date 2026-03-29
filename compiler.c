//**************************************************************************************************** */
//*********************************************编译器代码***********************************************
//**************************************************************************************************** */
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define int long long // 兼容 64 位地址

// ======================= 前向声明 =======================
struct ASTNode;
typedef struct ASTNode ASTNode;

// 基础函数
void next(void);
int match(int tk);
void next1(void);
void flush_assembly(void);

// 语法分析函数
void program(void);
void global_declaration(void);
void function_declaration(void);
void function_body(void);
void statement(void);
ASTNode* expression(int level);
void enum_declaration(void);
void function_parameter(void);

// AST, TAC, VM, Utils
void print_token_stream(void);
void print_symbol_table(void);
void build_ast(void);
void print_ast(void);
void free_ast(ASTNode *node);
ASTNode *create_ast_node(int type, int line);
void set_node_name(ASTNode *node, const char *str);
void add_child(ASTNode *parent, ASTNode *child);
void add_statement_to_current(ASTNode *stmt);
char *get_identifier_name(int *id);
int eval(void);

// 优化相关
void optimize_tac(void);
int is_number(char *s);

// 全局变量
int debug;    // 调试
int assembly; // 汇编输出
int tac_mode; // TAC 输出
int optimize; // 优化开关

int token; // 当前 token

// 虚拟机指令集
enum { LEA ,IMM ,JMP ,CALL,JZ  ,JNZ ,ENT ,ADJ ,LEV ,LI  ,LC  ,SI  ,SC  ,PUSH,
       OR  ,XOR ,AND ,EQ  ,NE  ,LT  ,GT  ,LE  ,GE  ,SHL ,SHR ,ADD ,SUB ,MUL ,DIV ,MOD ,
       OPEN,READ,CLOS,PRTF,MALC,MSET,MCMP,EXIT };

// TAC 操作码
enum { 
    TAC_ADD, TAC_SUB, TAC_MUL, TAC_DIV, TAC_MOD,
    TAC_EQ, TAC_NE, TAC_LT, TAC_GT, TAC_LE, TAC_GE,
    TAC_ASSIGN, TAC_IFZ, TAC_GOTO, TAC_LABEL,
    TAC_PARAM, TAC_CALL, TAC_RETURN, TAC_FUNC, TAC_ENDFUNC
};

// Token 类型
enum {
  Num = 128, Fun, Sys, Glo, Loc, Id,
  Char, Else, Enum, For, If, Int, Return, Sizeof, While,
  Assign, Cond, Lor, Lan, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge, Shl, Shr, Add, Sub, Mul, Div, Mod, Inc, Dec, Brak
};

// 标识符字段
enum {Token, Hash, Name, Type, Class, Value, BType, BClass, BValue, IdSize};

// 类型
enum { CHAR, INT, PTR };
enum {Global, Local};

// AST 节点类型
typedef enum {
    AST_PROGRAM, AST_FUNCTION_DECL, AST_VARIABLE_DECL, AST_PARAM_DECL,
    AST_COMPOUND_STMT, AST_EXPR_STMT, AST_IF_STMT, AST_WHILE_STMT, AST_FOR_STMT, AST_RETURN_STMT,
    AST_ASSIGN_EXPR, AST_BINARY_EXPR, AST_UNARY_EXPR, AST_CALL_EXPR,
    AST_IDENTIFIER, AST_CONSTANT, AST_STRING,
    AST_TYPE, AST_ARG_LIST, AST_PARAM_LIST, AST_DECL_LIST
} ASTNodeType;

struct ASTNode {
    ASTNodeType type;
    int line_no;
    char *name;
    int value;
    int data_type;
    struct ASTNode *left;
    struct ASTNode *right;
    struct ASTNode *child;
    struct ASTNode *next;
    struct ASTNode *else_stmt; 
};

// TAC 结构体
typedef struct TACInstruction {
    int op;
    char *res;
    char *arg1;
    char *arg2;
    struct TACInstruction *next;
} TACInstruction;

// 全局状态变量
ASTNode *ast_root = NULL;
ASTNode *current_function = NULL;
ASTNode *current_compound = NULL;
int ast_building_mode = 0;

TACInstruction *tac_head = NULL;
TACInstruction *tac_tail = NULL;
int temp_count = 0;
int label_count = 0;

int *text, *old_text, *stack;
char *data;
int *idmain;
char *src, *old_src;
int poolsize;
int *pc, *bp, *sp, ax, cycle;
int *current_id, *symbols, line, token_val;
int basetype, expr_type, index_of_bp;

// ===================== 辅助函数 =====================

char* new_temp() {
    char buf[16]; sprintf(buf, "t%d", temp_count++); return strdup(buf);
}
char* new_label() {
    char buf[16]; sprintf(buf, "L%d", label_count++); return strdup(buf);
}

void emit_tac(int op, char* res, char* arg1, char* arg2) {
    TACInstruction *tac = (TACInstruction*)malloc(sizeof(TACInstruction));
    tac->op = op;
    tac->res = res ? strdup(res) : NULL;
    tac->arg1 = arg1 ? strdup(arg1) : NULL;
    tac->arg2 = arg2 ? strdup(arg2) : NULL;
    tac->next = NULL;
    if (!tac_head) tac_head = tac; else tac_tail->next = tac;
    tac_tail = tac;
}

// ===================== TAC 优化 (常量折叠) =====================
#define MAX_CONSTS 1024
struct { char *name; int val; int active; } const_table[MAX_CONSTS];

void clear_const_table() { for (int i=0; i<MAX_CONSTS; i++) const_table[i].active = 0; }

void set_const(char *name, int val) {
    if (!name) return;
    for (int i=0; i<MAX_CONSTS; i++) if (const_table[i].active && !strcmp(const_table[i].name, name)) { const_table[i].val = val; return; }
    for (int i=0; i<MAX_CONSTS; i++) if (!const_table[i].active) { const_table[i].name = name; const_table[i].val = val; const_table[i].active = 1; return; }
}

int get_const(char *name, int *val) {
    if (!name) return 0;
    if (is_number(name)) { *val = atoi(name); return 1; }
    for (int i=0; i<MAX_CONSTS; i++) if (const_table[i].active && !strcmp(const_table[i].name, name)) { *val = const_table[i].val; return 1; }
    return 0;
}

int is_number(char *s) {
    if (!s || *s == '\0') return 0;
    if (*s == '-') s++;
    while (*s) { if (*s < '0' || *s > '9') return 0; s++; }
    return 1;
}

void optimize_tac() {
    TACInstruction *curr = tac_head;
    clear_const_table();
    while (curr) {
        int v1, v2, res_val, optimized = 0;
        if (curr->op == TAC_LABEL || curr->op == TAC_FUNC || curr->op == TAC_IFZ || curr->op == TAC_GOTO || curr->op == TAC_CALL) clear_const_table();
        switch (curr->op) {
            case TAC_ASSIGN:
                if (get_const(curr->arg1, &v1)) {
                    set_const(curr->res, v1);
                    if (!is_number(curr->arg1)) { char buf[32]; sprintf(buf, "%lld", v1); curr->arg1 = strdup(buf); }
                }
                break;
            case TAC_ADD: case TAC_SUB: case TAC_MUL: case TAC_DIV: case TAC_MOD:
                if (get_const(curr->arg1, &v1) && get_const(curr->arg2, &v2)) {
                    if (curr->op == TAC_ADD) res_val = v1 + v2;
                    else if (curr->op == TAC_SUB) res_val = v1 - v2;
                    else if (curr->op == TAC_MUL) res_val = v1 * v2;
                    else if (curr->op == TAC_DIV) { if(v2!=0) res_val = v1/v2; else optimized=0; }
                    else if (curr->op == TAC_MOD) { if(v2!=0) res_val = v1%v2; else optimized=0; }
                    if (v2 != 0 || (curr->op!=TAC_DIV && curr->op!=TAC_MOD)) optimized = 1;
                }
                break;
        }
        if (optimized) {
            curr->op = TAC_ASSIGN;
            char buf[32]; sprintf(buf, "%lld", res_val);
            curr->arg1 = strdup(buf); curr->arg2 = NULL;
            set_const(curr->res, res_val);
        }
        curr = curr->next;
    }
}

// ===================== 汇编打印 =====================
void flush_assembly() {
    if (!assembly) return;
    while (old_text < text) {
        int op = *++old_text;
        printf("    ");
        if (op == LEA)      printf("addi a0, fp, %lld", *++old_text);
        else if (op == IMM) printf("li   a0, %lld", *++old_text);
        else if (op == JMP) printf("j    %p", (void*)*++old_text);
        else if (op == CALL)printf("call %p", (void*)*++old_text);
        else if (op == JZ)  printf("beqz a0, %p", (void*)*++old_text);
        else if (op == JNZ) printf("bnez a0, %p", (void*)*++old_text);
        else if (op == ENT) printf("addi sp, sp, -%lld", *++old_text * 8);
        else if (op == ADJ) printf("addi sp, sp, %lld", *++old_text * 8);
        else if (op == LEV) printf("ret");
        else if (op == LI)  printf("lw   a0, 0(a0)");
        else if (op == LC)  printf("lb   a0, 0(a0)");
        else if (op == SI)  printf("sw   a0, (t0)");
        else if (op == SC)  printf("sb   a0, (t0)");
        else if (op == PUSH)printf("addi sp, sp, -8\n    sd   a0, 0(sp)");
        else if (op == OR)  printf("or   a0, t0, a0");
        else if (op == XOR) printf("xor  a0, t0, a0");
        else if (op == AND) printf("and  a0, t0, a0");
        else if (op == EQ)  printf("seqz a0, t0, a0");
        else if (op == NE)  printf("snez a0, t0, a0");
        else if (op == LT)  printf("slt  a0, t0, a0");
        else if (op == GT)  printf("sgt  a0, t0, a0");
        else if (op == LE)  printf("sle  a0, t0, a0");
        else if (op == GE)  printf("sge  a0, t0, a0");
        else if (op == SHL) printf("sll  a0, t0, a0");
        else if (op == SHR) printf("srl  a0, t0, a0");
        else if (op == ADD) printf("add  a0, t0, a0");
        else if (op == SUB) printf("sub  a0, t0, a0");
        else if (op == MUL) printf("mul  a0, t0, a0");
        else if (op == DIV) printf("div  a0, t0, a0");
        else if (op == MOD) printf("rem  a0, t0, a0");
        else if (op == OPEN) printf("ecall (open)");
        else if (op == READ) printf("ecall (read)");
        else if (op == CLOS) printf("ecall (close)");
        else if (op == PRTF) printf("ecall (printf)");
        else if (op == MALC) printf("ecall (malloc)");
        else if (op == MSET) printf("ecall (memset)");
        else if (op == MCMP) printf("ecall (memcmp)");
        else if (op == EXIT) printf("ecall (exit)");
        else printf("unknown_op %lld", op);
        printf("\n");
    }
}

// ===================== 词法分析 =====================
void next() {
    char *last_pos; int hash;
    while (token = *src) {
        ++src;
        if (token == '\n') {
            if (assembly) { printf("%d: %.*s", line, src-old_src, old_src); old_src = src; flush_assembly(); }
            ++line;
        }
        else if (token == '#') { while (*src != 0 && *src != '\n') src++; }
        else if ((token >= 'a' && token <= 'z') || (token >= 'A' && token <= 'Z') || (token == '_')) {
            last_pos = src - 1; hash = token;
            while ((*src >= 'a' && *src <= 'z') || (*src >= 'A' && *src <= 'Z') || (*src >= '0' && *src <= '9') || (*src == '_')) {
                hash = hash * 147 + *src; src++;
            }
            current_id = symbols;
            while (current_id[Token]) {
                if (current_id[Hash] == hash && !memcmp((char *)current_id[Name], last_pos, src - last_pos)) { token = current_id[Token]; return; }
                current_id = current_id + IdSize;
            }
            current_id[Name] = (int)last_pos; current_id[Hash] = hash; token = current_id[Token] = Id; return;
        }
        else if (token >= '0' && token <= '9') {
            token_val = token - '0';
            if (token_val > 0) { while (*src >= '0' && *src <= '9') token_val = token_val*10 + *src++ - '0'; } 
            else {
                if (*src == 'x' || *src == 'X') { token = *++src; while ((token >= '0' && token <= '9') || (token >= 'a' && token <= 'f') || (token >= 'A' && token <= 'F')) { token_val = token_val * 16 + (token & 15) + (token >= 'A' ? 9 : 0); token = *++src; } } 
                else { while (*src >= '0' && *src <= '7') token_val = token_val*8 + *src++ - '0'; }
            }
            token = Num; return;
        }
        else if (token == '/') {
            if (*src == '/') { while (*src != 0 && *src != '\n') ++src; }
            else if (*src == '*') { src++; while (*src != 0) { if (*src == '*' && *(src + 1) == '/') { src += 2; break; } if (*src == '\n') ++line; ++src; } }
            else { token = Div; return; }
        }
        else if (token == '"' || token == '\'') {
            last_pos = data;
            while (*src != 0 && *src != token) {
                token_val = *src++;
                if (token_val == '\\') { token_val = *src++; if (token_val == 'n') token_val = '\n'; }
                if (token == '"') *data++ = token_val;
            }
            src++;
            if (token == '"') token_val = (int)last_pos; else token = Num;
            return;
        }
        else if (token == '=') { if (*src == '=') { src++; token = Eq; } else token = Assign; return; }
        else if (token == '+') { if (*src == '+') { src++; token = Inc; } else token = Add; return; }
        else if (token == '-') { if (*src == '-') { src++; token = Dec; } else token = Sub; return; }
        else if (token == '!') { if (*src == '=') { src++; token = Ne; } else token = '!'; return; } 
        else if (token == '<') { if (*src == '=') { src++; token = Le; } else if (*src == '<') { src++; token = Shl; } else token = Lt; return; }
        else if (token == '>') { if (*src == '=') { src++; token = Ge; } else if (*src == '>') { src++; token = Shr; } else token = Gt; return; }
        else if (token == '|') { if (*src == '|') { src++; token = Lor; } else token = Or; return; }
        else if (token == '&') { if (*src == '&') { src++; token = Lan; } else token = And; return; }
        else if (token == '^') { token = Xor; return; } else if (token == '%') { token = Mod; return; }
        else if (token == '*') { token = Mul; return; } else if (token == '[') { token = Brak; return; }
        else if (token == '?') { token = Cond; return; }
        else if (token == '~' || token == ';' || token == '{' || token == '}' || token == '(' || token == ')' || token == ']' || token == ',' || token == ':') { return; }
    }
}

void next1() { next(); }

//代码报错信息提示
int match(int tk) {
    if (token == tk) { next(); return 1; }
    else { printf("Line %d: expected token: %d\n", line, tk); exit(-1); }
}

ASTNode *create_ast_node(int type, int line) {
    ASTNode *node = (ASTNode *)malloc(sizeof(ASTNode));
    memset(node, 0, sizeof(ASTNode)); node->type = (ASTNodeType)type; node->line_no = line; return node;
}
void set_node_name(ASTNode *node, const char *str) { if (node && str) node->name = strdup(str); }
void add_child(ASTNode *parent, ASTNode *child) {
    if (!parent || !child) return;
    if (!parent->child) parent->child = child;
    else { ASTNode *last = parent->child; while (last->next) last = last->next; last->next = child; }
}
void add_statement_to_current(ASTNode *stmt) {
    if (!current_compound || !stmt) return; add_child(current_compound, stmt);
}
char *get_identifier_name(int *id) {
    if (!id || !id[Name]) return "unknown";
    static char buf[256]; char *p = (char *)id[Name]; int i = 0;
    while(i < 255 && ((p[i]>='a'&&p[i]<='z')||(p[i]>='A'&&p[i]<='Z')||(p[i]>='0'&&p[i]<='9')||p[i]=='_')) { buf[i] = p[i]; i++; }
    buf[i] = 0; return strdup(buf);
}

// ===================== 语法分析 =====================

void statement() {
    int *a, *b;
    ASTNode *stmt_node = NULL;

    if (token == If) {
        ASTNode *if_node = NULL;
        if (ast_building_mode) if_node = create_ast_node(AST_IF_STMT, line);
        match(If); match('('); ASTNode *cond = expression(Assign); match(')');
        if (ast_building_mode && if_node) if_node->left = cond;
        *++text = JZ; b = ++text;
        ASTNode *saved = current_compound;
        if (ast_building_mode && if_node) { ASTNode *body = create_ast_node(AST_COMPOUND_STMT, line); if_node->child = body; current_compound = body; }
        statement();
        if (ast_building_mode) current_compound = saved;
        if (token == Else) {
            if (ast_building_mode && if_node) { ASTNode *el = create_ast_node(AST_COMPOUND_STMT, line); if_node->else_stmt = el; current_compound = el; }
            match(Else); *b = (int)(text + 3); *++text = JMP; b = ++text;
            statement();
            if (ast_building_mode) current_compound = saved;
        }
        *b = (int)(text + 1);
        if (ast_building_mode && if_node) add_statement_to_current(if_node);
    }
    else if (token == While) {
        ASTNode *wn = NULL; if (ast_building_mode) wn = create_ast_node(AST_WHILE_STMT, line);
        match(While); a = text + 1; match('('); ASTNode *cond = expression(Assign); match(')');
        if (ast_building_mode && wn) wn->left = cond;
        *++text = JZ; b = ++text;
        ASTNode *saved = current_compound;
        if (ast_building_mode && wn) { ASTNode *body = create_ast_node(AST_COMPOUND_STMT, line); wn->child = body; current_compound = body; }
        statement();
        if (ast_building_mode) current_compound = saved;
        *++text = JMP; *++text = (int)a; *b = (int)(text + 1);
        if (ast_building_mode && wn) add_statement_to_current(wn);
    }
    else if (token == For) {
        ASTNode *fn = NULL; if (ast_building_mode) fn = create_ast_node(AST_FOR_STMT, line);
        match(For); match('(');
        if (token != ';') { ASTNode *init = expression(Assign); if (ast_building_mode && fn) fn->left = init; }
        match(';');
        a = text + 1; 
        if (token != ';') { ASTNode *cond = expression(Assign); if (ast_building_mode && fn) fn->right = cond; }
        match(';');
        *++text = JZ; b = ++text;
        *++text = JMP; int *c = ++text;
        int *d = text + 1;
        if (token != ')') { ASTNode *inc = expression(Assign); if (ast_building_mode && fn) fn->else_stmt = inc; }
        match(')');
        *++text = JMP; *++text = (int)a;
        *c = (int)(text + 1);
        ASTNode *saved = current_compound;
        if (ast_building_mode && fn) { ASTNode *body = create_ast_node(AST_COMPOUND_STMT, line); fn->child = body; current_compound = body; }
        statement();
        if (ast_building_mode) current_compound = saved;
        *++text = JMP; *++text = (int)d;
        *b = (int)(text + 1);
        if (ast_building_mode && fn) add_statement_to_current(fn);
    }
    else if (token == '{') {
        ASTNode *cn = NULL, *saved = current_compound;
        if (ast_building_mode) { cn = create_ast_node(AST_COMPOUND_STMT, line); current_compound = cn; }
        match('{'); while (token != '}') statement(); match('}');
        if (ast_building_mode) { current_compound = saved; if (saved && cn) add_statement_to_current(cn); }
    }
    else if (token == Return) {
        ASTNode *rn = NULL; if (ast_building_mode) rn = create_ast_node(AST_RETURN_STMT, line);
        match(Return);
        if (token != ';') { ASTNode *val = expression(Assign); if (ast_building_mode && rn) rn->child = val; }
        match(';'); *++text = LEV;
        if (ast_building_mode && rn) add_statement_to_current(rn);
    }
    else if (token == ';') { match(';'); }
    else {
        ASTNode *stmt = NULL; if (ast_building_mode) stmt = create_ast_node(AST_EXPR_STMT, line);
        ASTNode *expr = expression(Assign); match(';');
        if (ast_building_mode && stmt) { stmt->child = expr; add_statement_to_current(stmt); }
    }
}

ASTNode* expression(int level) {
    ASTNode *node = NULL;
    int *id; int tmp; int *addr;
    
    if (!token) { printf("Line %d: unexpected EOF\n", line); exit(-1); }
    
    if (token == Num) {
        if (ast_building_mode) { node = create_ast_node(AST_CONSTANT, line); node->value = token_val; }
        match(Num); *++text = IMM; *++text = token_val; expr_type = INT;
    }
    else if (token == '"') {
        if (ast_building_mode) { node = create_ast_node(AST_STRING, line); set_node_name(node, (char*)token_val); }
        *++text = IMM; *++text = token_val; match('"'); while(token=='"') match('"');
        data = (char *)(((int)data + sizeof(int)) & (-sizeof(int))); expr_type = PTR;
    }
    else if (token == Sizeof) {
        if (ast_building_mode) { node = create_ast_node(AST_UNARY_EXPR, line); set_node_name(node, "sizeof"); }
        match(Sizeof); match('('); expr_type = INT;
        if (token == Int) match(Int); else if (token == Char) { match(Char); expr_type = CHAR; }
        while (token == Mul) { match(Mul); expr_type += PTR; }
        match(')'); *++text = IMM; *++text = (expr_type == CHAR) ? 1 : sizeof(int); expr_type = INT;
    }
    else if (token == Id) {
        if (ast_building_mode) { node = create_ast_node(AST_IDENTIFIER, line); set_node_name(node, get_identifier_name(current_id)); }
        match(Id); id = current_id;
        if (token == '(') {
            if (ast_building_mode) { ASTNode *call = create_ast_node(AST_CALL_EXPR, line); call->left = node; node = call; }
            match('('); tmp = 0;
            while (token != ')') {
                ASTNode *arg = expression(Assign);
                if (ast_building_mode && node) { if (!node->right) node->right = arg; else { ASTNode *t = node->right; while(t->next) t=t->next; t->next = arg; } }
                *++text = PUSH; tmp++; if (token == ',') match(',');
            }
            match(')');
            if (id[Class]==Sys) *++text = id[Value]; else if (id[Class]==Fun) { *++text = CALL; *++text = id[Value]; }
            else { printf("Line %d: bad call\n", line); exit(-1); }
            if (tmp > 0) { *++text = ADJ; *++text = tmp; }
            expr_type = id[Type];
        } else if (id[Class] == Num) {
            if (ast_building_mode) { node->type = AST_CONSTANT; node->value = id[Value]; }
            *++text = IMM; *++text = id[Value]; expr_type = INT;
        } else {
            if (id[Class] == Loc) { *++text = LEA; *++text = index_of_bp - id[Value]; }
            else if (id[Class] == Glo) { *++text = IMM; *++text = id[Value]; }
            else { printf("Line %d: undefined variable\n", line); exit(-1); }
            expr_type = id[Type]; *++text = (expr_type == CHAR) ? LC : LI;
        }
    }
    else if (token == '(') {
        match('(');
        if (token == Int || token == Char) {
            tmp = (token == Char) ? CHAR : INT; match(token); while (token == Mul) { match(Mul); tmp += PTR; } match(')');
            ASTNode *inner = expression(Inc);
            if (ast_building_mode) { node = create_ast_node(AST_UNARY_EXPR, line); set_node_name(node, "cast"); node->child = inner; }
            expr_type = tmp;
        } else { node = expression(Assign); match(')'); }
    }
    else if (token == Mul) {
        match(Mul); ASTNode *inner = expression(Inc);
        if (ast_building_mode) { node = create_ast_node(AST_UNARY_EXPR, line); set_node_name(node, "*"); node->child = inner; }
        if (expr_type >= PTR) expr_type -= PTR; else { printf("Line %d: bad deref\n", line); exit(-1); }
        *++text = (expr_type == CHAR) ? LC : LI;
    }
    else if (token == And) {
        match(And); ASTNode *inner = expression(Inc);
        if (ast_building_mode) { node = create_ast_node(AST_UNARY_EXPR, line); set_node_name(node, "&"); node->child = inner; }
        if (*text == LC || *text == LI) text--; else { printf("Line %d: bad &\n", line); exit(-1); }
        expr_type += PTR;
    }
    else if (token == '!') {
        match('!'); ASTNode *inner = expression(Inc);
        if (ast_building_mode) { node = create_ast_node(AST_UNARY_EXPR, line); set_node_name(node, "!"); node->child = inner; }
        *++text = PUSH; *++text = IMM; *++text = 0; *++text = EQ; expr_type = INT;
    }
    else if (token == '~') {
        match('~'); ASTNode *inner = expression(Inc);
        if (ast_building_mode) { node = create_ast_node(AST_UNARY_EXPR, line); set_node_name(node, "~"); node->child = inner; }
        *++text = PUSH; *++text = IMM; *++text = -1; *++text = XOR; expr_type = INT;
    }
    else if (token == Add) { match(Add); node = expression(Inc); expr_type = INT; }
    else if (token == Sub) {
        match(Sub);
        if (token == Num) {
            if (ast_building_mode) { node = create_ast_node(AST_CONSTANT, line); node->value = -token_val; }
            *++text = IMM; *++text = -token_val; match(Num);
        } else {
            *++text = IMM; *++text = -1; *++text = PUSH; ASTNode *inner = expression(Inc);
            if (ast_building_mode) { node = create_ast_node(AST_UNARY_EXPR, line); set_node_name(node, "-"); node->child = inner; }
            *++text = MUL;
        }
        expr_type = INT;
    }
    else if (token == Inc || token == Dec) {
        tmp = token; match(token); ASTNode *inner = expression(Inc);
        if (ast_building_mode) { node = create_ast_node(AST_UNARY_EXPR, line); set_node_name(node, tmp==Inc?"++pre":"--pre"); node->child = inner; }
        if (*text==LC) { *text=PUSH; *++text=LC; } else if (*text==LI) { *text=PUSH; *++text=LI; } else { printf("%d: bad lval\n", line); exit(-1); }
        *++text = PUSH; *++text = IMM; *++text = (expr_type > PTR) ? sizeof(int) : sizeof(char);
        *++text = (tmp == Inc) ? ADD : SUB; *++text = (expr_type == CHAR) ? SC : SI;
    }

    while (token >= level) {
        tmp = expr_type;
        if (token == Assign) {
            match(Assign); if (*text==LC || *text==LI) *text=PUSH; else { printf("Line %d: bad assign\n", line); exit(-1); }
            ASTNode *rhs = expression(Assign);
            if (ast_building_mode) { ASTNode *n = create_ast_node(AST_ASSIGN_EXPR, line); n->left=node; n->right=rhs; node=n; }
            expr_type = tmp; *++text = (expr_type == CHAR) ? SC : SI;
        }
        else if (token == Cond) {
            match(Cond); *++text=JZ; addr=++text; ASTNode *tb = expression(Assign);
            match(':'); *addr=(int)(text+3); *++text=JMP; addr=++text; ASTNode *fb = expression(Cond);
            *addr=(int)(text+1);
            if (ast_building_mode) { ASTNode *n = create_ast_node(AST_IF_STMT, line); set_node_name(n,"?:"); n->left=node; n->child=tb; n->else_stmt=fb; node=n; }
        }
        else if (token == Lor) { match(Lor); *++text=JNZ; addr=++text; ASTNode *rhs=expression(Lan); *addr=(int)(text+1); expr_type=INT; 
            if (ast_building_mode) { ASTNode *n=create_ast_node(AST_BINARY_EXPR, line); set_node_name(n,"||"); n->left=node; n->right=rhs; node=n; } }
        else if (token == Lan) { match(Lan); *++text=JZ; addr=++text; ASTNode *rhs=expression(Or); *addr=(int)(text+1); expr_type=INT;
            if (ast_building_mode) { ASTNode *n=create_ast_node(AST_BINARY_EXPR, line); set_node_name(n,"&&"); n->left=node; n->right=rhs; node=n; } }
        else if (token == Or) { match(Or); *++text=PUSH; ASTNode *rhs=expression(Xor); *++text=OR; expr_type=INT;
            if (ast_building_mode) { ASTNode *n=create_ast_node(AST_BINARY_EXPR, line); set_node_name(n,"|"); n->left=node; n->right=rhs; node=n; } }
        else if (token == Xor) { match(Xor); *++text=PUSH; ASTNode *rhs=expression(And); *++text=XOR; expr_type=INT;
            if (ast_building_mode) { ASTNode *n=create_ast_node(AST_BINARY_EXPR, line); set_node_name(n,"^"); n->left=node; n->right=rhs; node=n; } }
        else if (token == And) { match(And); *++text=PUSH; ASTNode *rhs=expression(Eq); *++text=AND; expr_type=INT;
            if (ast_building_mode) { ASTNode *n=create_ast_node(AST_BINARY_EXPR, line); set_node_name(n,"&"); n->left=node; n->right=rhs; node=n; } }
        else if (token == Eq) { match(Eq); *++text=PUSH; ASTNode *rhs=expression(Ne); *++text=EQ; expr_type=INT;
            if (ast_building_mode) { ASTNode *n=create_ast_node(AST_BINARY_EXPR, line); set_node_name(n,"=="); n->left=node; n->right=rhs; node=n; } }
        else if (token == Ne) { match(Ne); *++text=PUSH; ASTNode *rhs=expression(Lt); *++text=NE; expr_type=INT;
            if (ast_building_mode) { ASTNode *n=create_ast_node(AST_BINARY_EXPR, line); set_node_name(n,"!="); n->left=node; n->right=rhs; node=n; } }
        else if (token == Lt) { match(Lt); *++text=PUSH; ASTNode *rhs=expression(Shl); *++text=LT; expr_type=INT;
            if (ast_building_mode) { ASTNode *n=create_ast_node(AST_BINARY_EXPR, line); set_node_name(n,"<"); n->left=node; n->right=rhs; node=n; } }
        else if (token == Gt) { match(Gt); *++text=PUSH; ASTNode *rhs=expression(Shl); *++text=GT; expr_type=INT;
            if (ast_building_mode) { ASTNode *n=create_ast_node(AST_BINARY_EXPR, line); set_node_name(n,">"); n->left=node; n->right=rhs; node=n; } }
        else if (token == Le) { match(Le); *++text=PUSH; ASTNode *rhs=expression(Shl); *++text=LE; expr_type=INT;
            if (ast_building_mode) { ASTNode *n=create_ast_node(AST_BINARY_EXPR, line); set_node_name(n,"<="); n->left=node; n->right=rhs; node=n; } }
        else if (token == Ge) { match(Ge); *++text=PUSH; ASTNode *rhs=expression(Shl); *++text=GE; expr_type=INT;
            if (ast_building_mode) { ASTNode *n=create_ast_node(AST_BINARY_EXPR, line); set_node_name(n,">="); n->left=node; n->right=rhs; node=n; } }
        else if (token == Shl) { match(Shl); *++text=PUSH; ASTNode *rhs=expression(Add); *++text=SHL; expr_type=INT;
            if (ast_building_mode) { ASTNode *n=create_ast_node(AST_BINARY_EXPR, line); set_node_name(n,"<<"); n->left=node; n->right=rhs; node=n; } }
        else if (token == Shr) { match(Shr); *++text=PUSH; ASTNode *rhs=expression(Add); *++text=SHR; expr_type=INT;
            if (ast_building_mode) { ASTNode *n=create_ast_node(AST_BINARY_EXPR, line); set_node_name(n,">>"); n->left=node; n->right=rhs; node=n; } }
        else if (token == Add) { match(Add); *++text=PUSH; ASTNode *rhs=expression(Mul); 
            if (ast_building_mode) { ASTNode *n=create_ast_node(AST_BINARY_EXPR, line); set_node_name(n,"+"); n->left=node; n->right=rhs; node=n; }
            expr_type=tmp;
            if (expr_type > PTR) { *++text=PUSH; *++text=IMM; *++text=sizeof(int); *++text=MUL; } *++text=ADD; 
        }
        else if (token == Sub) { match(Sub); *++text=PUSH; ASTNode *rhs=expression(Mul);
            if (ast_building_mode) { ASTNode *n=create_ast_node(AST_BINARY_EXPR, line); set_node_name(n,"-"); n->left=node; n->right=rhs; node=n; }
            if (tmp > PTR && tmp == expr_type) { *++text=SUB; *++text=PUSH; *++text=IMM; *++text=sizeof(int); *++text=DIV; expr_type=INT; }
            else if (tmp > PTR) { *++text=PUSH; *++text=IMM; *++text=sizeof(int); *++text=MUL; *++text=SUB; expr_type=tmp; }
            else { *++text=SUB; expr_type=tmp; }
        }
        else if (token == Mul) { match(Mul); *++text=PUSH; ASTNode *rhs=expression(Inc); *++text=MUL; expr_type=tmp;
            if (ast_building_mode) { ASTNode *n=create_ast_node(AST_BINARY_EXPR, line); set_node_name(n,"*"); n->left=node; n->right=rhs; node=n; } }
        else if (token == Div) { match(Div); *++text=PUSH; ASTNode *rhs=expression(Inc); *++text=DIV; expr_type=tmp;
            if (ast_building_mode) { ASTNode *n=create_ast_node(AST_BINARY_EXPR, line); set_node_name(n,"/"); n->left=node; n->right=rhs; node=n; } }
        else if (token == Mod) { match(Mod); *++text=PUSH; ASTNode *rhs=expression(Inc); *++text=MOD; expr_type=tmp;
            if (ast_building_mode) { ASTNode *n=create_ast_node(AST_BINARY_EXPR, line); set_node_name(n,"%%"); n->left=node; n->right=rhs; node=n; } }
        else if (token == Inc || token == Dec) {
            if (*text==LI) { *text=PUSH; *++text=LI; } else if (*text==LC) { *text=PUSH; *++text=LC; } else { printf("%d: bad val\n", line); exit(-1); }
            if (ast_building_mode) { ASTNode *n=create_ast_node(AST_UNARY_EXPR, line); set_node_name(n, token==Inc?"post++":"post--"); n->child=node; node=n; }
            *++text=PUSH; *++text=IMM; *++text=(expr_type > PTR) ? sizeof(int) : sizeof(char);
            *++text=(token==Inc)?ADD:SUB; *++text=(expr_type==CHAR)?SC:SI;
            *++text=PUSH; *++text=IMM; *++text=(expr_type > PTR) ? sizeof(int) : sizeof(char);
            *++text=(token==Inc)?SUB:ADD; match(token);
        }
        else if (token == Brak) { match(Brak); *++text=PUSH; ASTNode *idx=expression(Assign); match(']');
            if (ast_building_mode) { ASTNode *n=create_ast_node(AST_BINARY_EXPR, line); set_node_name(n,"[]"); n->left=node; n->right=idx; node=n; }
            if (tmp > PTR) { *++text=PUSH; *++text=IMM; *++text=sizeof(int); *++text=MUL; } else if (tmp < PTR) { printf("%d: ptr expected\n", line); exit(-1); }
            expr_type = tmp - PTR; *++text=ADD; *++text=(expr_type==CHAR)?LC:LI;
        }
        else { printf("Line %d: err token=%d\n", line, token); exit(-1); }
    }
    return node;
}

void function_parameter() {
    int type; int params = 0;
    while (token != ')') {
        type = INT; if (token == Int) match(Int); else if (token == Char) { type = CHAR; match(Char); }
        while (token == Mul) { match(Mul); type = type + PTR; }
        if (token != Id) { printf("Line %d: bad param decl\n", line); exit(-1); }
        if (current_id[Class] == Loc) { printf("Line %d: dup param\n", line); exit(-1); }
        match(Id);
        current_id[BClass] = current_id[Class]; current_id[Class] = Loc;
        current_id[BType] = current_id[Type]; current_id[Type] = type;
        current_id[BValue] = current_id[Value]; current_id[Value] = params++;
        if (token == ',') match(',');
    }
    index_of_bp = params+1;
}

void function_body() {
    int pos_local = index_of_bp; int type;
    while (token == Int || token == Char) {
        basetype = (token == Int) ? INT : CHAR; match(token);
        while (token != ';') {
            type = basetype; while (token == Mul) { match(Mul); type = type + PTR; }
            if (token != Id) { printf("Line %d: bad local\n", line); exit(-1); }
            match(Id);
            current_id[BClass] = current_id[Class]; current_id[Class] = Loc;
            current_id[BType] = current_id[Type]; current_id[Type] = type;
            current_id[BValue] = current_id[Value]; current_id[Value] = ++pos_local;
            if (token == ',') match(',');
        }
        match(';');
    }
    *++text = ENT; *++text = pos_local - index_of_bp;
    while (token != '}') statement();
    *++text = LEV;
}

void global_declaration() {
    int type; basetype = INT;
    if (token == Enum) {
        match(Enum); if (token != '{') match(Id); if (token == '{') { 
            match('{'); int i=0; while(token!='}') { if(token!=Id) exit(-1); next(); if(token==Assign){next(); i=token_val; next();} current_id[Class]=Num; current_id[Type]=INT; current_id[Value]=i++; if(token==',') next(); } match('}'); 
        } match(';'); return;
    }
    if (token == Int) { match(Int); basetype = INT; } else if (token == Char) { match(Char); basetype = CHAR; }
    while (token != ';' && token != '}') {
        type = basetype; while (token == Mul) { match(Mul); type = type + PTR; }
        if (token != Id) { printf("Line %d: bad global\n", line); exit(-1); }
        if (current_id[Class]) { printf("Line %d: duplicate global\n", line); exit(-1); }
        match(Id); current_id[Type] = type;
        if (token == '(') {
            current_id[Class] = Fun; current_id[Value] = (int)(text + 1);
            if (ast_building_mode) {
                ASTNode *fn = create_ast_node(AST_FUNCTION_DECL, line);
                set_node_name(fn, get_identifier_name(current_id)); fn->data_type = type;
                add_child(ast_root, fn); current_function = fn;
            }
            match('('); function_parameter(); match(')'); match('{');
            if (ast_building_mode && current_function) { ASTNode *b = create_ast_node(AST_COMPOUND_STMT, line); current_function->child = b; current_compound = b; }
            function_body();
            if (ast_building_mode) { current_compound = NULL; current_function = NULL; }
            current_id = symbols;
            while (current_id[Token]) { if (current_id[Class]==Loc) { current_id[Class]=current_id[BClass]; current_id[Type]=current_id[BType]; current_id[Value]=current_id[BValue]; } current_id += IdSize; }
        } else {
            current_id[Class] = Glo; current_id[Value] = (int)data; data = data + sizeof(int);
            if (ast_building_mode) {
                ASTNode *vn = create_ast_node(AST_VARIABLE_DECL, line);
                set_node_name(vn, get_identifier_name(current_id)); vn->data_type = type;
                add_child(ast_root, vn);
            }
        }
        if (token == ',') match(',');
    }
    next();
}

void program() {
    next(); while (token > 0) global_declaration();
}

// ===================== TAC =====================

char* tac_visit(ASTNode *node) {
    if (!node) return NULL;
    switch(node->type) {
        case AST_FUNCTION_DECL: {
            emit_tac(TAC_FUNC, node->name, NULL, NULL);
            tac_visit(node->child);
            emit_tac(TAC_ENDFUNC, NULL, NULL, NULL);
            return NULL;
        }
        case AST_COMPOUND_STMT: {
            ASTNode *stmt = node->child;
            while (stmt) { tac_visit(stmt); stmt = stmt->next; }
            return NULL;
        }
        case AST_EXPR_STMT: return tac_visit(node->child);
        case AST_RETURN_STMT: {
            char *ret = NULL; if (node->child) ret = tac_visit(node->child);
            emit_tac(TAC_RETURN, ret, NULL, NULL); return NULL;
        }
        case AST_ASSIGN_EXPR: {
            char *rhs = tac_visit(node->right); char *lhs = node->left->name;
            emit_tac(TAC_ASSIGN, lhs, rhs, NULL); return lhs;
        }
        case AST_BINARY_EXPR: {
            char *t1 = tac_visit(node->left); char *t2 = tac_visit(node->right);
            char *res = new_temp();
            int op = -1;
            if (!strcmp(node->name, "+")) op = TAC_ADD;
            else if (!strcmp(node->name, "-")) op = TAC_SUB;
            else if (!strcmp(node->name, "*")) op = TAC_MUL;
            else if (!strcmp(node->name, "/")) op = TAC_DIV;
            else if (!strcmp(node->name, "%%")) op = TAC_MOD;
            else if (!strcmp(node->name, "==")) op = TAC_EQ;
            else if (!strcmp(node->name, "!=")) op = TAC_NE;
            else if (!strcmp(node->name, "<")) op = TAC_LT;
            else if (!strcmp(node->name, ">")) op = TAC_GT;
            else if (!strcmp(node->name, "<=")) op = TAC_LE;
            else if (!strcmp(node->name, ">=")) op = TAC_GE;
            if (op != -1) emit_tac(op, res, t1, t2);
            return res;
        }
        case AST_UNARY_EXPR: return tac_visit(node->child);
        case AST_IDENTIFIER: return node->name;
        case AST_CONSTANT: { char buf[32]; sprintf(buf, "%lld", node->value); return strdup(buf); }
        case AST_STRING: return node->name;
        case AST_IF_STMT: {
            char *Lf = new_label(); char *Le = node->else_stmt ? new_label() : Lf;
            char *cond = tac_visit(node->left);
            emit_tac(TAC_IFZ, cond, Lf, NULL);
            tac_visit(node->child);
            if (node->else_stmt) emit_tac(TAC_GOTO, Le, NULL, NULL);
            emit_tac(TAC_LABEL, Lf, NULL, NULL);
            if (node->else_stmt) { tac_visit(node->else_stmt); emit_tac(TAC_LABEL, Le, NULL, NULL); }
            return NULL;
        }
        case AST_WHILE_STMT: {
            char *Ls = new_label(), *Le = new_label();
            emit_tac(TAC_LABEL, Ls, NULL, NULL);
            char *cond = tac_visit(node->left);
            emit_tac(TAC_IFZ, cond, Le, NULL);
            tac_visit(node->child);
            emit_tac(TAC_GOTO, Ls, NULL, NULL);
            emit_tac(TAC_LABEL, Le, NULL, NULL);
            return NULL;
        }
        case AST_FOR_STMT: {
            char *Ls = new_label(), *Le = new_label();
            if (node->left) tac_visit(node->left);
            emit_tac(TAC_LABEL, Ls, NULL, NULL);
            if (node->right) { char *cond = tac_visit(node->right); emit_tac(TAC_IFZ, cond, Le, NULL); }
            tac_visit(node->child);
            if (node->else_stmt) tac_visit(node->else_stmt);
            emit_tac(TAC_GOTO, Ls, NULL, NULL);
            emit_tac(TAC_LABEL, Le, NULL, NULL);
            return NULL;
        }
        case AST_CALL_EXPR: {
            int ac = 0; ASTNode *arg = node->right; char *at[20];
            while(arg && ac < 20) { at[ac++] = tac_visit(arg); arg = arg->next; }
            for(int i=0; i<ac; i++) emit_tac(TAC_PARAM, at[i], NULL, NULL);
            char *res = new_temp(); char buf[16]; sprintf(buf, "%lld", ac);
            emit_tac(TAC_CALL, res, node->left->name, buf);
            return res;
        }
        default: return NULL;
    }
}


//=================================三地址码打印===========================
void print_tac() {
    TACInstruction *curr = tac_head;
    printf("\n================ Three Address Code ================\n");
    while (curr) {
        if (curr->op == TAC_FUNC) printf("\n");
        int indent = (curr->op != TAC_LABEL && curr->op != TAC_FUNC && curr->op != TAC_ENDFUNC);
        if (indent) printf("    ");
        switch(curr->op) {
            case TAC_ADD: printf("%s = %s + %s\n", curr->res, curr->arg1, curr->arg2); break;
            case TAC_SUB: printf("%s = %s - %s\n", curr->res, curr->arg1, curr->arg2); break;
            case TAC_MUL: printf("%s = %s * %s\n", curr->res, curr->arg1, curr->arg2); break;
            case TAC_DIV: printf("%s = %s / %s\n", curr->res, curr->arg1, curr->arg2); break;
            case TAC_MOD: printf("%s = %s %% %s\n", curr->res, curr->arg1, curr->arg2); break;
            case TAC_EQ:  printf("%s = %s == %s\n", curr->res, curr->arg1, curr->arg2); break;
            case TAC_NE:  printf("%s = %s != %s\n", curr->res, curr->arg1, curr->arg2); break;
            case TAC_LT:  printf("%s = %s < %s\n", curr->res, curr->arg1, curr->arg2); break;
            case TAC_GT:  printf("%s = %s > %s\n", curr->res, curr->arg1, curr->arg2); break;
            case TAC_LE:  printf("%s = %s <= %s\n", curr->res, curr->arg1, curr->arg2); break;
            case TAC_GE:  printf("%s = %s >= %s\n", curr->res, curr->arg1, curr->arg2); break;
            case TAC_ASSIGN: printf("%s = %s\n", curr->res, curr->arg1); break;
            case TAC_IFZ: printf("ifz %s goto %s\n", curr->arg1, curr->res); break; // Swap for readability
            case TAC_GOTO: printf("goto %s\n", curr->res); break;
            case TAC_LABEL: printf("%s:\n", curr->res); break;
            case TAC_PARAM: printf("param %s\n", curr->res); break;
            case TAC_CALL: 
                if (curr->res && strlen(curr->res) > 0) printf("%s = call %s, %s\n", curr->res, curr->arg1, curr->arg2);
                else printf("call %s, %s\n", curr->arg1, curr->arg2);
                break;
            case TAC_RETURN: 
                if (curr->res) printf("return %s\n", curr->res); else printf("return\n");
                break;
            case TAC_FUNC: printf("%s:\n", curr->res); break;
            case TAC_ENDFUNC: printf("endfunc\n"); break;
        }
        curr = curr->next;
    }
    printf("====================================================\n");
}

// ===================== Print Utils =====================

void print_token_stream() {
    printf("============================TOKEN STREAM============================\n");
    printf("Line: Token type      Value/Name\n--------------------------------\n");
    next();
    while (token > 0) {
        printf("%4lld: ", line);
        if (token == Num) printf("Num             %lld", token_val);
        else if (token == Id) printf("Id              %s", get_identifier_name(current_id));
        else if (token >= 128) printf("Keyword         %lld", token);
        else printf("Char            '%c'", (char)token);
        printf("\n"); next();
    }
    printf("=====================================================================\n");
}

void print_symbol_table() {
    int *id = symbols;
    printf("\n================ Symbol Table ================\n");
    printf("Name       Type   Class  Value/Addr\n");
    printf("----------------------------------------------\n");
    
    while (id[Token]) {
        // 1. 过滤掉关键字。在 C4 中，关键字的 Token 值小于 Id
        // 如果你希望只看用户定义的变量和函数，加上这个判断
        if (id[Token] != Id) {
            id = id + IdSize;
            continue;
        }

        char *name = (char *)id[Name];
        if (name) {
             if (id[Class] == 0) {
                 id = id + IdSize;
                 continue;
             }
             // 截取标识符名称
             char buf[64]; int k = 0;
             while (name[k] && k < 63 && 
                   ((name[k] >= 'a' && name[k] <= 'z') || 
                    (name[k] >= 'A' && name[k] <= 'Z') || 
                    (name[k] >= '0' && name[k] <= '9') || 
                    name[k] == '_')) {
                 buf[k] = name[k]; k++;
             }
             buf[k] = '\0';
             
             printf("%-10s ", buf);
             
             // 打印类型
             if (id[Type] == INT) printf("INT    ");
             else if (id[Type] == CHAR) printf("CHAR   ");
             else if (id[Type] == 0) printf("-      "); // 未定义类型
             else printf("PTR    ");
             
             // 打印类别 (关键修改)
             if (id[Class] == Glo) printf("Global ");
             else if (id[Class] == Loc) printf("Local  ");
             else if (id[Class] == Fun) printf("Func   ");
             else if (id[Class] == Sys) printf("Sys    ");
             else printf("Unused "); // 处理 Class 为 0 的情况
             
             printf("%lld\n", id[Value]);
        }
        id = id + IdSize;
    }
    printf("==============================================\n");
}

// 树形 AST 打印
void print_ast_node(ASTNode *node, int depth) {
    if (!node) return;
    for (int i = 0; i < depth - 1; i++) printf("    ");
    if (depth > 0) printf("    ");
    
    const char *type_names[] = {
        "程序根节点", "函数声明", "变量声明", "参数声明",
        "复合语句块", "表达式语句", "IF条件判断", "WHILE循环", "FOR循环", "返回语句",
        "赋值操作", "二元运算", "一元运算", "函数调用",
        "标识符", "整数常量", "字符串", "类型", "参数表", "形参表", "声明表"
    };
    
    if (node->type >= 0 && node->type <= AST_DECL_LIST) printf("%s", type_names[node->type]);
    else printf("未知节点");
    
    if (node->name) printf(" [%s]", node->name);
    if (node->type == AST_CONSTANT) printf(" [值=%lld]", node->value);
    printf("\n");
    
    if (node->left) print_ast_node(node->left, depth + 1);
    if (node->right) print_ast_node(node->right, depth + 1);
    if (node->child) print_ast_node(node->child, depth + 1);
    
    if (node->else_stmt) {
        for (int i = 0; i < depth; i++) printf("    ");
        if (node->type == AST_FOR_STMT) printf("    [增量操作]\n");
        else printf("    [ELSE分支]\n");
        print_ast_node(node->else_stmt, depth + 1);
    }
    if (node->next) print_ast_node(node->next, depth);
}

void print_ast() { 
    printf("\n================ AST ================\n"); 
    print_ast_node(ast_root, 0); 
    printf("===================================\n");
}
void free_ast(ASTNode *node) { if(!node)return; free_ast(node->left); free_ast(node->right); free_ast(node->child); free_ast(node->next); free(node); }

// ===================== VM 栈式虚拟机=====================
int eval() {
    int op, *tmp; cycle = 0;
    while (1) {
        cycle++; op = *pc++;
        if (op == IMM) ax = *pc++;
        else if (op == LC) ax = *(char *)ax;
        else if (op == LI) ax = *(int *)ax;
        else if (op == SC) ax = *(char *)*sp++ = ax;
        else if (op == SI) *(int *)*sp++ = ax;
        else if (op == PUSH) *--sp = ax;
        else if (op == JMP) pc = (int *)*pc;
        else if (op == JZ) pc = ax ? pc + 1 : (int *)*pc;
        else if (op == JNZ) pc = ax ? (int *)*pc : pc + 1;
        else if (op == CALL) { *--sp = (int)(pc+1); pc = (int *)*pc; }
        else if (op == ENT) { *--sp = (int)bp; bp = sp; sp = sp - *pc++; }
        else if (op == ADJ) sp = sp + *pc++;
        else if (op == LEV) { sp = bp; bp = (int *)*sp++; pc = (int *)*sp++; }
        else if (op == LEA) ax = (int)(bp + *pc++);
        else if (op == OR) ax = *sp++ | ax;
        else if (op == XOR) ax = *sp++ ^ ax;
        else if (op == AND) ax = *sp++ & ax;
        else if (op == EQ) ax = *sp++ == ax;
        else if (op == NE) ax = *sp++ != ax;
        else if (op == LT) ax = *sp++ < ax;
        else if (op == LE) ax = *sp++ <= ax;
        else if (op == GT) ax = *sp++ > ax;
        else if (op == GE) ax = *sp++ >= ax;
        else if (op == SHL) ax = *sp++ << ax;
        else if (op == SHR) ax = *sp++ >> ax;
        else if (op == ADD) ax = *sp++ + ax;
        else if (op == SUB) ax = *sp++ - ax;
        else if (op == MUL) ax = *sp++ * ax;
        else if (op == DIV) ax = *sp++ / ax;
        else if (op == MOD) ax = *sp++ % ax;
        else if (op == EXIT) { printf("exit(%lld)", *sp); return *sp; }
        else if (op == OPEN) { ax = open((char *)sp[1], sp[0]); }
        else if (op == CLOS) { ax = close(*sp); }
        else if (op == READ) { ax = read(sp[2], (char *)sp[1], *sp); }
        else if (op == PRTF) { tmp = sp + pc[1]; ax = printf((char *)tmp[-1], tmp[-2], tmp[-3], tmp[-4], tmp[-5], tmp[-6]); }
        else if (op == MALC) { ax = (int)malloc(*sp); }
        else if (op == MSET) { ax = (int)memset((char *)sp[2], sp[1], *sp); }
        else if (op == MCMP) { ax = memcmp((char *)sp[2], (char *)sp[1], *sp); }
        else { printf("unknown instruction:%lld\n", op); return -1; }
    }
}

// ===================== Main =====================
#undef int

int main(int argc, char **argv)
{
    #define int long long
    int i, fd;
    int *tmp; 
    int token_mode = 0, symbol_table_mode = 0, ast_mode = 0;

    argc--; argv++;
    debug = 0; assembly = 0; tac_mode = 0; optimize = 0;

    //传参选择
    while (argc > 0 && **argv == '-') {
        if ((*argv)[1] == 's') assembly = 1;
        else if ((*argv)[1] == 'd') debug = 1;
        else if ((*argv)[1] == 't') token_mode = 1;
        else if ((*argv)[1] == 'm') symbol_table_mode = 1;
        else if ((*argv)[1] == 'a') ast_mode = 1;
        else if ((*argv)[1] == 'c') tac_mode = 1; 
        else if ((*argv)[1] == 'o') { tac_mode = 1; optimize = 1; } 
        argc--; argv++;
    }

    if (argc < 1) { printf("usage: xc [-s] [-t] [-m] [-a] [-c] [-o] file ...\n"); return -1; }
    if ((fd = open(*argv, 0)) < 0) { printf("could not open(%s)\n", *argv); return -1; }

    poolsize = 256 * 1024;
    line = 1;

    if (!(text = old_text = malloc(poolsize))) { printf("malloc fail\n"); return -1; }
    if (!(data = malloc(poolsize))) { printf("malloc fail\n"); return -1; }
    if (!(stack = malloc(poolsize))) { printf("malloc fail\n"); return -1; }
    if (!(symbols = malloc(poolsize))) { printf("malloc fail\n"); return -1; }

    memset(text, 0, poolsize); memset(data, 0, poolsize); 
    memset(stack, 0, poolsize); memset(symbols, 0, poolsize);

    src = "char else enum for if int return sizeof while "
          "open read close printf malloc memset memcmp exit void main";
    i = Char;
    while (i <= While) { next(); current_id[Token] = i++; }
    i = OPEN;
    while (i <= EXIT) { next(); current_id[Class] = Sys; current_id[Type] = INT; current_id[Value] = i++; }
    next(); current_id[Token] = Char; next(); idmain = current_id;

    if (!(src = old_src = malloc(poolsize))) { printf("malloc fail\n"); return -1; }
    if ((i = read(fd, src, poolsize-1)) <= 0) { printf("read return %lld\n", i); return -1; }
    src[i] = 0; close(fd);
    
    char *src_copy = malloc(poolsize);
    memcpy(src_copy, src, poolsize);

    if (token_mode) {
        src = old_src = src_copy;
        print_token_stream();
    } else if (symbol_table_mode) {
        program();
        print_symbol_table();
    } else if (ast_mode) {
        ast_root = create_ast_node(AST_PROGRAM, 1);
        ast_building_mode = 1;
        program();
        print_ast();
        free_ast(ast_root);
    } else if (tac_mode) { 
        ast_root = create_ast_node(AST_PROGRAM, 1);
        ast_building_mode = 1; 
        program();
        ASTNode *child = ast_root->child;
        while(child) {
            tac_visit(child);
            child = child->next;
        }
        if (optimize) {
            printf("[INFO] Running Constant Folding Optimization...\n");
            optimize_tac();
        }
        print_tac();
        free_ast(ast_root);
    } else {
        program();
        if (assembly) {
            flush_assembly();
        } else {
            if (!(pc = (int *)idmain[Value])) { printf("main() not defined\n"); return -1; }
            int *exit_address = text; 
            *text++ = EXIT;
            *text++ = 0;
            sp = (int *)((int)stack + poolsize);
            *--sp = argc;
            *--sp = (int)argv;
            *--sp = (int)exit_address; 
            return eval();
        }
    }
    return 0;
}

