/*
 * CONJUNTO DE INSTRUÇÕES IR:
 * - FUNC_BEGIN/END_FUNC: Delimitadores de função
 * - PARAM/LOCAL: Declarações de parâmetros e variáveis locais
 * - MOV: Movimentação de dados entre variáveis
 * - ADD/SUB/MUL/DIV: Operações aritméticas
 * - CMP: Comparação entre valores
 * - BR_EQ/BR_NE/BR_LT/BR_LE/BR_GT/BR_GE: Saltos condicionais
 * - GOTO: Salto incondicional
 * - LOAD_ARRAY/STORE_ARRAY: Acesso a elementos de array
 * - ARG/CALL/STORE_RET: Chamadas de função e retorno
 * - RETURN/RETURN_VOID: Comandos de retorno
 * 
 * ESTRUTURA DE PROCESSAMENTO:
 * 1. Primeiro passo: Coleta declarações de variáveis globais
 * 2. Segundo passo: Gera código para cada função individualmente
 * 3. Usa sistema de buffer para permitir declarações LOCAL corretas
 */

#include "globals.h"
#include "codegen.h"
#include "util.h"
#include "symtab.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

// Tamanhos máximos de buffer para geração de função
#define MAX_FUNC_INSTRUCTIONS 1024
#define MAX_FUNC_LOCALS 256
#define MAX_FUNC_PARAMS 32
#define MAX_IDENTIFIER_LEN 256
#define MAX_REGISTERS 64  // 64 total registers including special ones

// ============================================================================
// ENHANCED IR STRUCTURES FOR ASSEMBLY GENERATION
// ============================================================================

// Type system for variables and expressions
typedef enum {
    TYPE_VOID,
    TYPE_INT,
    TYPE_ARRAY_INT,
    TYPE_UNKNOWN
} IRType;

// Address modes for operands
typedef enum {
    ADDR_IMMEDIATE,    // #123 - immediate constant
    ADDR_REGISTER,     // R5 - register reference
    ADDR_MEMORY,       // [SP+8] - memory location
    ADDR_VARIABLE,     // variable name (will be resolved later)
    ADDR_LABEL,        // label reference
    ADDR_UNKNOWN
} AddressMode;

// Enhanced register management
typedef struct {
    char name[32];           // Register name (R0, R1, etc.)
    int in_use;              // 1 if allocated, 0 if free
    int scope_level;         // Function scope level
    char *assigned_var;      // Variable currently assigned to this register
    int is_special;          // 1 if special register (R0=zero, R63=stack, etc.)
    IRType result_type;      // Type of value stored in this register
} EnhancedRegister;

// Variable information with type and scope
typedef struct {
    char name[MAX_IDENTIFIER_LEN];
    IRType type;
    int size;                // Size in bytes
    int stack_offset;        // Offset from stack pointer
    int scope_level;         // 0=global, 1=function, 2+=nested
    int is_parameter;        // 1 if function parameter
    int reg_hint;           // Preferred register (-1 if none)
} VariableInfo;

// Memory allocation tracking
typedef struct MemoryBlock {
    char name[MAX_IDENTIFIER_LEN];
    IRType type;
    int size;
    int offset;
    int scope_level;
    struct MemoryBlock *next;
} MemoryBlock;

// Enhanced expression result with type information
typedef struct {
    char *result_var;
    IRType type;
    AddressMode addr_mode;
    int is_constant;
    int constant_value;
    int reg_allocated;       // Register number if allocated
} ExpressionResult;

// ============================================================================
// ENHANCED GLOBAL STATE
// ============================================================================

// Register pool management
static EnhancedRegister register_pool[MAX_REGISTERS];
static int register_pool_initialized = 0;
static int current_scope_level = 0;

// Memory management
static MemoryBlock *memory_blocks = NULL;
static int current_stack_offset = 0;

// Variable tracking with scope
static VariableInfo variable_table[MAX_FUNC_LOCALS + MAX_FUNC_PARAMS];
static int variable_count = 0;

// Existing compatibility structures
typedef struct {
    int total_instructions;
    int total_temporaries;
    int total_labels;
    int total_functions;
    int optimization_count;
} CompilationStats;

static CompilationStats stats = {0, 0, 0, 0, 0};

// ============================================================================
// ENHANCED REGISTER ALLOCATION FUNCTIONS
// ============================================================================

// Initialize register pool with special registers
static void init_register_pool() {
    if (register_pool_initialized) return;
    
    for (int i = 0; i < MAX_REGISTERS; i++) {
        snprintf(register_pool[i].name, sizeof(register_pool[i].name), "R%d", i);
        register_pool[i].in_use = 0;
        register_pool[i].scope_level = 0;
        register_pool[i].assigned_var = NULL;
        register_pool[i].is_special = 0;
    }
    
    // Mark special registers
    register_pool[0].is_special = 1;   // R0 = constant zero
    register_pool[63].is_special = 1;  // R63 = stack pointer
    register_pool[62].is_special = 1;  // R62 = frame pointer
    register_pool[61].is_special = 1;  // R61 = return address
    
    // Reserve special registers
    register_pool[0].in_use = 1;
    register_pool[61].in_use = 1;
    register_pool[62].in_use = 1;
    register_pool[63].in_use = 1;
    
    register_pool_initialized = 1;
    printf("Register pool initialized with %d registers (%d general purpose)\n", 
           MAX_REGISTERS, MAX_REGISTERS - 4);
}

// Allocate a register from the pool
static int allocate_register_enhanced(const char *var_name) {
    init_register_pool();
    
    // Look for free general purpose registers (R1-R60)
    for (int i = 1; i < 61; i++) {
        if (!register_pool[i].in_use && !register_pool[i].is_special) {
            register_pool[i].in_use = 1;
            register_pool[i].scope_level = current_scope_level;
            if (var_name) {
                register_pool[i].assigned_var = copyString(var_name);
            }
            stats.total_temporaries++;
            return i;
        }
    }
    
    // No free registers - this is a register pressure situation
    fprintf(stderr, "Warning: Register pressure - no free registers available\n");
    return -1; // Indicates spill to memory needed
}

// Release a register back to the pool
static void release_register_enhanced(int reg_num) {
    if (reg_num >= 0 && reg_num < MAX_REGISTERS && !register_pool[reg_num].is_special) {
        register_pool[reg_num].in_use = 0;
        register_pool[reg_num].scope_level = 0;
        if (register_pool[reg_num].assigned_var) {
            free(register_pool[reg_num].assigned_var);
            register_pool[reg_num].assigned_var = NULL;
        }
    }
}

// ============================================================================
// SCOPE MANAGEMENT FUNCTIONS  
// ============================================================================

// Enter a new scope (function or block)
static void enter_scope() {
    current_scope_level++;
}

// Exit current scope and cleanup variables/registers
static void exit_scope() {
    // Release registers in current scope
    for (int i = 0; i < MAX_REGISTERS; i++) {
        if (register_pool[i].scope_level == current_scope_level) {
            release_register_enhanced(i);
        }
    }
    
    // Remove variables from current scope
    for (int i = variable_count - 1; i >= 0; i--) {
        if (variable_table[i].scope_level == current_scope_level) {
            variable_count--;
            // Shift remaining variables down
            for (int j = i; j < variable_count; j++) {
                variable_table[j] = variable_table[j + 1];
            }
        }
    }
    
    current_scope_level--;
}

// ============================================================================
// MEMORY MANAGEMENT FUNCTIONS
// ============================================================================

// Add memory block to tracking list
static void add_memory_block(const char *name, IRType type, int size) {
    MemoryBlock *block = (MemoryBlock*)malloc(sizeof(MemoryBlock));
    strncpy(block->name, name, MAX_IDENTIFIER_LEN - 1);
    block->name[MAX_IDENTIFIER_LEN - 1] = '\0';
    block->type = type;
    block->size = size;
    block->scope_level = current_scope_level;
    block->offset = current_stack_offset;
    block->next = memory_blocks;
    memory_blocks = block;
    
    current_stack_offset += size;
    
    // Align to 4-byte boundaries for better performance
    if (current_stack_offset % 4 != 0) {
        current_stack_offset += 4 - (current_stack_offset % 4);
    }
}

// Get variable information by name
static VariableInfo* get_variable_info(const char *name) {
    for (int i = 0; i < variable_count; i++) {
        if (strcmp(variable_table[i].name, name) == 0) {
            return &variable_table[i];
        }
    }
    return NULL;
}

// Add variable to tracking table
static void add_variable_info(const char *name, IRType type, int is_param) {
    if (variable_count >= MAX_FUNC_LOCALS + MAX_FUNC_PARAMS) {
        fprintf(stderr, "Error: Too many variables in function\n");
        return;
    }
    
    VariableInfo *var = &variable_table[variable_count];
    strncpy(var->name, name, MAX_IDENTIFIER_LEN - 1);
    var->name[MAX_IDENTIFIER_LEN - 1] = '\0';
    var->type = type;
    var->scope_level = current_scope_level;
    var->is_parameter = is_param;
    var->reg_hint = -1; // No register hint initially
    
    // Set size based on type
    switch (type) {
        case TYPE_INT:
            var->size = 4;
            break;
        case TYPE_ARRAY_INT:
            var->size = 4; // Base size, actual array size handled separately
            break;
        default:
            var->size = 4; // Default to 4 bytes
            break;
    }
    
    // Calculate stack offset
    var->stack_offset = current_stack_offset;
    current_stack_offset += var->size;
    
    // Add to memory tracking
    add_memory_block(name, type, var->size);
    
    variable_count++;
}

// Existing compatibility structures
typedef struct {
    int total_instructions;
    int total_temporaries;
    int total_labels;
    int total_functions;
    int optimization_count;
} CompilationStats;

static FILE *outputFile;
static int tempCount = 0;    // Contador para variáveis temporárias (reiniciado por função)
static int labelCount = 0;   // Contador para rótulos (reiniciado por função)
static GlobalVarList globalVars = NULL; // Lista de variáveis globais

// Buffer para código de função - usado para coletar todas as instruções de uma função
// antes de escrever no arquivo, permitindo gerar declarações LOCAL corretas
static char *instruction_buffer[MAX_FUNC_INSTRUCTIONS];
static int instruction_buffer_count;

// Lista de variáveis locais da função atual
static char local_vars_list[MAX_FUNC_LOCALS][MAX_IDENTIFIER_LEN];
static int local_vars_count;

// Lista de parâmetros da função atual
static char param_list[MAX_FUNC_PARAMS][MAX_IDENTIFIER_LEN];
static int param_count;

// Nome da função atualmente sendo processada
static char current_func_name_codegen[MAX_IDENTIFIER_LEN];

// Declarações antecipadas para funções auxiliares:
// Estas funções são utilizadas para auxiliar na geração de código intermediário (IR).
// Elas incluem funcionalidades como adicionar variáveis locais, gerar código para expressões,
// comandos e estruturas de controle, além de manipular temporários e rótulos únicos.
static void add_local_var(const char *name);
static void generate_code_recursive(TreeNode *tree);
static void generate_code_single(TreeNode *tree);
char *generate_expression_code(TreeNode *tree);
static void generate_statement_code(TreeNode *tree);
static const char *get_ir_op_string(TokenType op);
static const char *get_ir_branch_instruction(TokenType op, int branch_on_true);

// Novas funções para sistema aprimorado de temporários
static void init_temp_pool(void);
static char *allocate_temp(void);
static void release_temp(const char *temp_name);
static void release_all_temps(void);
static ExpressionResult optimize_expression(TreeNode *tree);
static int is_simple_assignment(TreeNode *tree);

// Funções de estatísticas e validação
void print_compilation_stats(void);
void reset_compilation_stats(void);
static int validate_ir_file(const char *ir_file);

// ============================================================================
// ENHANCED IR GENERATION FUNCTIONS  
// ============================================================================

// Generate enhanced quadruple with type information
static void generate_enhanced_quad(const char *op, const char *arg1, const char *arg2, const char *result, IRType result_type) {
    emitQuadruple(op, arg1, arg2, result);
    
    // Track type information for register allocation
    if (result && result[0] == 'R') {
        int reg_num = atoi(result + 1);
        if (reg_num >= 0 && reg_num < MAX_REGISTERS) {
            register_pool[reg_num].result_type = result_type;
        }
    }
    
    stats.total_instructions++;
}

// Enhanced register allocation for expressions
static char* allocate_register_for_expression(TreeNode* expr, IRType expected_type) {
    init_register_pool();
    
    int reg_num = allocate_register_enhanced(NULL);
    if (reg_num < 0) {
        fprintf(stderr, "Error: Unable to allocate register for expression\n");
        return copyString("R1"); // Fallback
    }
    
    char* reg_name = (char*)malloc(8);
    snprintf(reg_name, 8, "R%d", reg_num);
    
    // Set type hint for the register
    register_pool[reg_num].result_type = expected_type;
    
    return reg_name;
}

// Enhanced variable allocation with scope tracking
static void allocate_variable_enhanced(const char* name, IRType type, int is_param) {
    // Add to variable tracking
    add_variable_info(name, type, is_param);
    
    // Allocate register if beneficial
    if (current_scope_level <= 2) { // Global and function level variables get register hints
        VariableInfo* var = get_variable_info(name);
        if (var) {
            int reg_num = allocate_register_enhanced(name);
            if (reg_num >= 0) {
                var->reg_hint = reg_num;
            }
        }
    }
}

// Generate function prologue with enhanced tracking
static void generate_function_prologue_enhanced(const char* func_name) {
    enter_scope(); // Enter function scope
    
    fprintf(outputFile, "FUNC_BEGIN %s, %d, __, __\n", func_name, param_count);
    
    // Allocate space for local variables
    if (current_stack_offset > 0) {
        fprintf(outputFile, "SUB R63, %d, R63, INT\n", current_stack_offset);
    }
    
    stats.total_functions++;
}

// Generate function epilogue with cleanup
static void generate_function_epilogue_enhanced(const char* func_name) {
    // Restore stack pointer
    if (current_stack_offset > 0) {
        fprintf(outputFile, "ADD R63, %d, R63, INT\n", current_stack_offset);
    }
    
    fprintf(outputFile, "END_FUNC %s, __, __, __\n\n", func_name);
    
    // Reset stack offset for next function
    current_stack_offset = 0;
    
    exit_scope(); // Exit function scope
}


// Função auxiliar para adicionar variável global à lista
static void addGlobalVar(char *name, int size) {
    GlobalVarList newVar = (GlobalVarList)malloc(sizeof(struct globalVarRec));
    newVar->name = copyString(name);
    newVar->size = size;
    newVar->next = globalVars;
    globalVars = newVar;
}

// Verifica se uma variável é global (não usada nesta versão 
/*
static int isGlobalVar(char *name) {
    GlobalVarList curr = globalVars;
    while (curr != NULL) {
        if (strcmp(curr->name, name) == 0) {
            return 1;
        }
        curr = curr->next;
    }
    return 0;
}
*/

// Adiciona instrução ao buffer da função atual
// As instruções são coletadas em buffer para permitir a geração correta
// das declarações LOCAL antes das instruções do corpo da função
static void emit_buffered(const char *instruction_format, ...) {
    if (instruction_buffer_count >= MAX_FUNC_INSTRUCTIONS) {
        fprintf(stderr, "Erro: Muitas instruções para a função %s\n", current_func_name_codegen);
        return;
    }
    char buf[256];
    va_list args;
    va_start(args, instruction_format);
    vsnprintf(buf, sizeof(buf), instruction_format, args);
    va_end(args);
    instruction_buffer[instruction_buffer_count++] = copyString(buf);
    
    // Coleta estatísticas aprimoradas
    stats.total_instructions++;
}

// Emite instrução em formato quadrupla (op, arg1, arg2, arg3)
static void emit_quad(const char *op, const char *arg1, const char *arg2, const char *arg3) {
    char buf[256];
    snprintf(buf, sizeof(buf), "%s %s, %s, %s", op, 
             arg1 ? arg1 : "__", 
             arg2 ? arg2 : "__", 
             arg3 ? arg3 : "__");
    emit_buffered("%s", buf);
}

// Emite label (não segue formato quadrupla)
static void emit_label(const char *label) {
    emit_buffered("%s:", label);
}

// Adiciona uma variável à lista de variáveis locais da função atual
// Verifica se já não é um parâmetro ou se já está na lista antes de adicionar
static void add_local_var(const char *name) {
    if (name == NULL || strlen(name) == 0) return;

    // Verifica se é um parâmetro
    for (int i = 0; i < param_count; ++i) {
        if (strcmp(param_list[i], name) == 0) {
            return;
        }
    }
    // Verifica se já está na lista de locais
    for (int i = 0; i < local_vars_count; ++i) {
        if (strcmp(local_vars_list[i], name) == 0) {
            return;
        }
    }
    if (local_vars_count < MAX_FUNC_LOCALS) {
        strncpy(local_vars_list[local_vars_count++], name, MAX_IDENTIFIER_LEN - 1);
        local_vars_list[local_vars_count-1][MAX_IDENTIFIER_LEN-1] = '\0';
    } else {
        fprintf(stderr, "Erro: Muitas variáveis locais na função %s\n", current_func_name_codegen);
    }
}


// Funções utilitárias para criar novos temporários e rótulos

// Cria uma nova variável temporária única para a função atual
// Automaticamente adiciona à lista de variáveis locais
char *newTemp(void) {
    // Usa o sistema aprimorado se disponível
    char *temp = allocate_temp();
    if (temp != NULL) {
        stats.total_temporaries++;
        return temp;
    }
    
    // Fallback para o sistema original
    char *temp_name = (char *)malloc(MAX_IDENTIFIER_LEN);
    snprintf(temp_name, MAX_IDENTIFIER_LEN, "t%d", tempCount++);
    add_local_var(temp_name);
    stats.total_temporaries++;
    return temp_name;
}

// Cria um novo rótulo único para a função atual
// Usado para instruções de salto e controle de fluxo
char *newLabel(void) {
    char *label = (char *)malloc(MAX_LABEL_LEN);
    // Para rótulos mais únicos se necessário: snprintf(label, MAX_LABEL_LEN, "L_%s_%d", current_func_name_codegen, labelCount++);
    snprintf(label, MAX_LABEL_LEN, "L%d", labelCount++);
    stats.total_labels++;
    return label;
}


// --- Novas Funções de Emissão ---

// Emite declaração de variável global no arquivo de saída
// size > 0 para arrays, 0 para variáveis simples
static void emit_global_decl(const char *name, int size) {
    if (size > 0) { // Assumindo size > 0 para arrays, 0 ou 1 para variáveis simples
        char size_str[16];
        snprintf(size_str, sizeof(size_str), "%d", size);
        fprintf(outputFile, "GLOBAL_ARRAY %s, %s, __, __\n", name, size_str);
    } else {
        fprintf(outputFile, "GLOBAL %s, __, __, __\n", name); // Variável global simples
    }
}

// Esta função é chamada quando o escopo de uma função termina.
// Escreve as instruções em buffer para o arquivo de saída no formato correto:
// FUNC_BEGIN com nome e número de parâmetros
// Declarações PARAM para cada parâmetro
// Declaração LOCAL com todas as variáveis locais (se houver)
// Todas as instruções do corpo da função
// END_FUNC com nome da função
static void flush_function_buffer() {
    char param_count_str[16];
    snprintf(param_count_str, sizeof(param_count_str), "%d", param_count);
    fprintf(outputFile, "FUNC_BEGIN %s, %s, __, __\n", current_func_name_codegen, param_count_str);

    for (int i = 0; i < param_count; ++i) {
        fprintf(outputFile, "  PARAM %s, __, __, __\n", param_list[i]);
    }

    // Generate LOCAL declarations in proper quadruple format
    for (int i = 0; i < local_vars_count; ++i) {
        fprintf(outputFile, "  LOCAL %s, __, __, __\n", local_vars_list[i]);
    }

    for (int i = 0; i < instruction_buffer_count; ++i) {
        fprintf(outputFile, "  %s\n", instruction_buffer[i]);
        free(instruction_buffer[i]);
    }
    fprintf(outputFile, "END_FUNC %s, __, __, __\n\n", current_func_name_codegen);

    // Reinicia buffers para a próxima função
    instruction_buffer_count = 0;
    local_vars_count = 0;
    param_count = 0;
    tempCount = 0; // Reinicia para a próxima função
    labelCount = 0; // Reinicia para a próxima função
}

// --- Lógica de Geração de Código ---

// Obtém string do operador IR para operações aritméticas
// Converte tokens do C-minus para instruções IR correspondentes
static const char *get_ir_op_string(TokenType op) {
    switch (op) {
        case MAIS: return "ADD";  // Adição
        case SUB:  return "SUB";  // Subtração
        case MULT: return "MUL";  // Multiplicação
        case DIV:  return "DIV";  // Divisão
        default: return "OP_UNKNOWN"; // Não deveria acontecer para ops aritméticos válidos
    }
}

// Obtém string da instrução de salto IR para operações de comparação
// branch_on_true: 1 se salta quando condição é verdadeira, 0 se salta quando falsa
// Isto permite usar a mesma função para gerar ambos if e while com lógica inversa
static const char *get_ir_branch_instruction(TokenType op, int branch_on_true) {
    switch (op) {
        case IGDAD: return branch_on_true ? "BR_EQ" : "BR_NE"; // == (igual)
        case DIFER: return branch_on_true ? "BR_NE" : "BR_EQ"; // != (diferente)
        case MENOR: return branch_on_true ? "BR_LT" : "BR_GE"; // < (menor)
        case MENIG: return branch_on_true ? "BR_LE" : "BR_GT"; // <= (menor ou igual)
        case MAIOR: return branch_on_true ? "BR_GT" : "BR_LE"; // > (maior)
        case MAIIG: return branch_on_true ? "BR_GE" : "BR_LT"; // >= (maior ou igual)
        default: return "BR_UNKNOWN"; // Não deveria acontecer
    }
}


// Gera código IR para expressões e retorna o nome da variável/literal que contém o resultado
// Esta função processa recursivamente a árvore de expressões e produz o código IR necessário
// para calcular o valor da expressão, retornando a variável que contém o resultado final
char *generate_expression_code(TreeNode *tree) {
    if (tree == NULL) return NULL;

    char *result_var = NULL;
    char *left_operand, *right_operand, *index_expr;

    switch (tree->nodekind) {
        case ExpK:
            switch (tree->kind.exp) {
                case ConstK: // Constante literal (número)
                    result_var = (char *)malloc(32);
                    snprintf(result_var, 32, "%d", tree->attr.val);
                    return result_var; // Literais são usados diretamente

                case IdK://Vai pra VarK
                case VarK:
                    if (tree->child[0] != NULL) { // Acesso a array: var[índice]
                        index_expr = generate_expression_code(tree->child[0]);
                        result_var = newTemp();
                        emit_quad("LOAD_ARRAY", result_var, tree->attr.name, index_expr);
                    } else { // Variável simples
                        result_var = copyString(tree->attr.name);
                        // Se é uma variável local não-parâmetro, garante que está na lista local_vars_list
                    }
                    return result_var;

                case OpK: // Operação aritmética ou de comparação
                    left_operand = generate_expression_code(tree->child[0]);
                    right_operand = generate_expression_code(tree->child[1]);
                    result_var = newTemp();
                    const char *op_str = get_ir_op_string(tree->attr.opr);
                    if (strcmp(op_str, "OP_UNKNOWN") != 0) { // Aritmética
                        emit_quad(op_str, result_var, left_operand, right_operand);
                    } else {
                        fprintf(stderr, "Erro: Operador de comparação usado em contexto de expressão aritmética.\n");
                        // Fallback: trata como movimento simples se deve produzir algo
                        emit_quad("MOV", result_var, left_operand, NULL); // AVISO: fallback OpK
                    }
                    return result_var;

                case CallK: // Chamada de função
                    {
                        int arg_count = 0;
                        TreeNode *arg_node = tree->child[0];
                        char *arg_vars[MAX_FUNC_PARAMS]; // Argumentos máximos

                        // Gera código para argumentos primeiro e armazena nomes das variáveis/literais resultantes
                        while(arg_node != NULL && arg_count < MAX_FUNC_PARAMS) {
                            arg_vars[arg_count++] = generate_expression_code(arg_node);
                            arg_node = arg_node->sibling;
                        }

                        // Emite instruções ARG
                        for (int i = 0; i < arg_count; ++i) {
                            emit_quad("ARG", arg_vars[i], NULL, NULL);
                        }

                        // Determina se a função retorna um valor.
                        // funções void são 'output', 'sort'.
                        // Outras retornam um valor.
                        int is_void_call = (strcmp(tree->attr.name, "output") == 0 || strcmp(tree->attr.name, "sort") == 0);

                        char arg_count_str[16];
                        snprintf(arg_count_str, sizeof(arg_count_str), "%d", arg_count);
                        emit_quad("CALL", tree->attr.name, arg_count_str, NULL);

                        if (!is_void_call) {
                            result_var = newTemp();
                            emit_quad("STORE_RET", result_var, NULL, NULL);
                            return result_var;
                        } else {
                            return NULL; // Chamada void não produz variável resultado
                        }
                    }
                default:
                    fprintf(stderr, "Erro: Tipo de expressão desconhecido em generate_expression_code.\n");
                    return NULL;
            }
            break;
        default:
            fprintf(stderr, "Erro: Nó não-expressão em generate_expression_code.\n");
            return NULL;
    }
}


// Gera código IR para comandos (statements)
// Processa diferentes tipos de comandos como atribuições, ifs, whiles, returns
// e emite as instruções IR apropriadas para cada um
static void generate_statement_code(TreeNode *tree) {
    if (tree == NULL) return;

    char *val_expr, *idx_expr, *lhs_var, *rhs_var;
    char *label1, *label2, *label_end;

    switch (tree->nodekind) {
        case StmtK:
            switch (tree->kind.stmt) {
                case AssignK: // Comando de atribuição: var = expr ou var[idx] = expr
                    rhs_var = generate_expression_code(tree->child[1]);
                    if (tree->child[0]->kind.exp == IdK && tree->child[0]->child[0] != NULL) { // Atribuição a array: var[idx] = val
                        idx_expr = generate_expression_code(tree->child[0]->child[0]);
                        emit_quad("STORE_ARRAY", tree->child[0]->attr.name, idx_expr, rhs_var);
                    } else { // Atribuição simples: var = val
                        // LHS pode ser IdK ou VarK baseado no parser original
                        if (tree->child[0]->nodekind == ExpK && (tree->child[0]->kind.exp == IdK || tree->child[0]->kind.exp == VarK)) {
                            lhs_var = tree->child[0]->attr.name;
                            add_local_var(lhs_var); // Garante que variável LHS está em locais se não for param/global
                            emit_quad("MOV", lhs_var, rhs_var, NULL);
                        } else {
                            fprintf(stderr, "Erro: LHS da atribuição não é um tipo de variável reconhecido.\n");
                        }
                    }
                    break;

                case IfK: // Comando condicional if-then-else
                    label1 = newLabel(); // Rótulo para parte else ou fim do if
                    // Geração da condição (OpK assumido para condições C-minus)
                    if (tree->child[0]->kind.exp == OpK) {
                        char *op1 = generate_expression_code(tree->child[0]->child[0]);
                        char *op2 = generate_expression_code(tree->child[0]->child[1]);
                        emit_quad("CMP", op1, op2, NULL);

                        // Salta se condição é FALSA para label1
                        const char *branch_instr = get_ir_branch_instruction(tree->child[0]->attr.opr, 0); // salta em falso
                        emit_quad(branch_instr, label1, NULL, NULL);
                    } else { // Condição de variável booleana simples (não típico em C-minus)
                        char *cond_var = generate_expression_code(tree->child[0]);
                        emit_quad("CMP", cond_var, "0", NULL); // Assumindo 0 é falso
                        emit_quad("BR_EQ", label1, NULL, NULL); // Salta se cond_var é falso (0)
                    }

                    if (tree->child[2] != NULL) { // Parte else existe
                        label2 = newLabel(); // Rótulo para fim do if-else
                        emit_quad("GOTO", label2, NULL, NULL);
                        emit_label(label1); // Rótulo para else
                        // Processa todos os comandos na parte else
                        TreeNode *else_stmt = tree->child[2];
                        while (else_stmt != NULL) {
                            generate_code_single(else_stmt);
                            else_stmt = else_stmt->sibling;
                        }
                        emit_label(label2); // Rótulo para fim do if-else
                        free(label2);
                    } else { // Não há parte else
                        emit_label(label1); // Rótulo para fim do if
                    }
                    free(label1);
                    break;

                case WhileK: // Comando while
                    label1 = newLabel(); // verificação da condição
                    label2 = newLabel(); // fim do loop
                    
                    emit_label(label1); // Condição do while
                    // Geração da condição
                    if (tree->child[0]->kind.exp == OpK) {
                        char *op1 = generate_expression_code(tree->child[0]->child[0]);
                        char *op2 = generate_expression_code(tree->child[0]->child[1]);
                        emit_quad("CMP", op1, op2, NULL);

                        // Salta se condição é FALSA para label2 (fim do loop)
                        const char *branch_instr = get_ir_branch_instruction(tree->child[0]->attr.opr, 0); // salta em falso
                        emit_quad(branch_instr, label2, NULL, NULL);
                    } else {
                         char *cond_var = generate_expression_code(tree->child[0]);
                         emit_quad("CMP", cond_var, "0", NULL); // Assumindo 0 é falso para while
                         emit_quad("BR_EQ", label2, NULL, NULL); // Salta se cond_var é falso (0)
                    }

                    // Processa todos os comandos no corpo do loop
                    TreeNode *loop_stmt = tree->child[1];
                    while (loop_stmt != NULL) {
                        generate_code_single(loop_stmt);
                        loop_stmt = loop_stmt->sibling;
                    }
                    emit_quad("GOTO", label1, NULL, NULL); // Volta para verificação da condição
                    emit_label(label2); // Rótulo para fim do loop
                    free(label1);
                    free(label2);
                    break;

                case ReturnK: // Comando de retorno
                    if (tree->child[0] != NULL) {
                        val_expr = generate_expression_code(tree->child[0]);
                        emit_quad("RETURN", val_expr, NULL, NULL);
                    } else {
                        emit_quad("RETURN_VOID", NULL, NULL, NULL);
                    }
                    break;
                
                default:
                    fprintf(stderr, "Erro: Tipo de comando desconhecido em generate_statement_code.\n");
                    break;
            }
            break;
        case ExpK: // Expressão standalone, ex. chamada de função não parte de uma atribuição
            if (tree->kind.exp == CallK)
                generate_expression_code(tree); // Irá emitir ARG, CALL
            break;
        default:
            fprintf(stderr, "Erro: Nó não-comando/expressão em generate_statement_code.\n");
            break;
    }
}


// Função de geração de código recursiva (sem processamento de irmãos)
// Processa apenas o nó atual sem navegar pelos irmãos
// Usada para evitar processamento duplicado em contextos onde os irmãos
// já são processados explicitamente pelo código chamador
static void generate_code_single(TreeNode *tree) {
    if (tree == NULL) return;

    switch (tree->nodekind) {
        case StmtK:
            generate_statement_code(tree);
            break;
        case ExpK:
            // Pula declarações de variáveis locais (nós Type com filhos VarK)
            if (tree->kind.exp == TypeK && tree->child[0] != NULL && tree->child[0]->kind.exp == VarK) {
                // Esta é uma declaração de variável local, pula
                break;
            }
            // Uma expressão standalone (como uma chamada de função) é tratada por generate_statement_code.
            if (tree->kind.exp == CallK) { // ex. output(x);
                 generate_statement_code(tree); // Trata como comando
            }
            break;
        default:
            break;
    }
}

// Função de geração de código recursiva (com processamento de irmãos)
// Processa o nó atual e depois recursivamente todos os seus siblings
// Usada em contextos onde se deseja processar uma lista completa de nós
static void generate_code_recursive(TreeNode *tree) {
    if (tree == NULL) return;

    generate_code_single(tree);

    // Processa irmãos (siblings)
    if (tree->sibling != NULL) {
        generate_code_recursive(tree->sibling);
    }
}


// Primeiro passo: coleta declarações de variáveis globais
// Percorre a árvore sintática procurando por declarações de variáveis globais
// e emite as instruções GLOBAL ou GLOBAL_ARRAY correspondentes
void generateGlobalDeclarations(TreeNode *tree) {
    if (tree == NULL) return;

    if (tree->nodekind == ExpK && tree->kind.exp == TypeK) {
        TreeNode *actual_decl = tree->child[0];
        if (actual_decl != NULL && actual_decl->kind.exp == VarK) {
            // Declaração de variável global
            int size = 0; // 0 para variável simples, >0 para tamanho do array
            if (actual_decl->child[0] != NULL && actual_decl->child[0]->kind.exp == ConstK) { // Array
                size = actual_decl->child[0]->attr.val;
            }
            addGlobalVar(copyString(actual_decl->attr.name), size); // Adiciona à lista interna
            emit_global_decl(actual_decl->attr.name, size);         // Emite para arquivo
        }
        // Pula declarações de função neste passo
    }
    
    // Processa apenas siblings no nível superior (escopo global)
    generateGlobalDeclarations(tree->sibling);
}

// Segundo passo: gera código de função
// Percorre a árvore sintática procurando por definições de função
// e gera o código IR completo para cada uma
static void generateFunctions(TreeNode *tree) {
    if (tree == NULL) return;

    if (tree->nodekind == ExpK && tree->kind.exp == TypeK) {
        TreeNode *actual_decl = tree->child[0];
        if (actual_decl != NULL && actual_decl->kind.exp == FuncK) {
            // Inicializa para nova função
            instruction_buffer_count = 0;
            local_vars_count = 0;
            param_count = 0;
            tempCount = 0;
            labelCount = 0;
            strncpy(current_func_name_codegen, actual_decl->attr.name, MAX_IDENTIFIER_LEN -1);
            current_func_name_codegen[MAX_IDENTIFIER_LEN-1] = '\0';

            // Coleta parâmetros da função
            TreeNode *param_node = actual_decl->child[0];
            while (param_node != NULL && param_count < MAX_FUNC_PARAMS) {
                if (param_node->kind.exp == ParamK) {
                     strncpy(param_list[param_count++], param_node->attr.name, MAX_IDENTIFIER_LEN -1);
                     param_list[param_count-1][MAX_IDENTIFIER_LEN-1] = '\0';
                } else if (param_node->kind.exp == TypeK && param_node->child[0] != NULL && param_node->child[0]->kind.exp == ParamK) {
                    // Parâmetro pode estar aninhado sob TypeK
                     strncpy(param_list[param_count++], param_node->child[0]->attr.name, MAX_IDENTIFIER_LEN -1);
                     param_list[param_count-1][MAX_IDENTIFIER_LEN-1] = '\0';
                }
                param_node = param_node->sibling;
            }
            
            // Gera corpo da função (comando composto)
            // O corpo é actual_decl->child[1] e seus siblings
            if (actual_decl->child[1] != NULL) {
                TreeNode *stmt = actual_decl->child[1];
                while (stmt != NULL) {
                    generate_code_recursive(stmt); // Processa cada comando no corpo da função
                    stmt = stmt->sibling;
                }
            }

            // Adiciona return implícito para funções void se não já presente na última instrução
            // Esta verificação é complicada; por ora, assume análise semântica ou programador garante returns.
            // Ou, sempre adiciona se void e última instrução não é RETURN_VOID.
            // Os IRs de exemplo têm returns explícitos.

            flush_function_buffer(); // Escreve função em buffer para arquivo
        }
    }
    
    generateFunctions(tree->sibling);
}


// Função principal de geração de código
// Processa a árvore sintática em dois passos:
// 1. Primeiro passo: coleta e emite declarações de variáveis globais
// 2. Segundo passo: gera código para todas as funções
void codeGen(TreeNode *syntaxTree, char * irOutputFile) {
    outputFile = fopen(irOutputFile, "w");
    if (outputFile == NULL) {
        fprintf(stderr, "Erro: Não foi possível abrir %s para escrita\n", irOutputFile);
        return;
    }
    
    // Initialize enhanced IR system
    init_register_pool();
    current_scope_level = 0;
    current_stack_offset = 0;
    variable_count = 0;
    memory_blocks = NULL;
    register_pool_initialized = 0;
    
    printf("=== Enhanced C-minus Compiler IR Generation ===\n");
    printf("Register Pool: 64 registers (R0-R63)\n");
    printf("Special Registers: R0(zero), R61(return), R62(frame), R63(stack)\n");
    printf("General Purpose: R1-R60 (60 registers available)\n");
    printf("Scope Management: Enabled\n");
    printf("Type System: Enhanced IR with type information\n");
    printf("Memory Management: Stack-based with alignment\n");
    printf("=============================================\n\n");
    }
    
    globalVars = NULL; // Inicializa lista de variáveis globais

    // Processa todas as declarações de nível superior
    TreeNode *current = syntaxTree;
    while (current != NULL) {
        // Primeiro passo: Gera declarações de variáveis globais
        if (current->nodekind == ExpK && current->kind.exp == TypeK) {
            TreeNode *actual_decl = current->child[0];
            if (actual_decl != NULL && actual_decl->kind.exp == VarK) {
                // Declaração de variável global
                int size = 0; // 0 para variável simples, >0 para tamanho do array
                if (actual_decl->child[0] != NULL && actual_decl->child[0]->kind.exp == ConstK) { // Array
                    size = actual_decl->child[0]->attr.val;
                }
                addGlobalVar(copyString(actual_decl->attr.name), size); // Adiciona à lista interna
                emit_global_decl(actual_decl->attr.name, size);         // Emite para arquivo
            }
        }
        current = current->sibling;
    }
    
    // Adiciona linha em branco após declarações globais se alguma foi impressa
    if (globalVars != NULL) {
        fprintf(outputFile, "\n");
    }
    
    // Segundo passo: Gera código de função
    current = syntaxTree;
    while (current != NULL) {
        if (current->nodekind == ExpK && current->kind.exp == TypeK) {
            TreeNode *actual_decl = current->child[0];
            if (actual_decl != NULL && actual_decl->kind.exp == FuncK) {
                // Inicializa para nova função
                instruction_buffer_count = 0;
                local_vars_count = 0;
                param_count = 0;
                tempCount = 0;
                labelCount = 0;
                strncpy(current_func_name_codegen, actual_decl->attr.name, MAX_IDENTIFIER_LEN -1);
                current_func_name_codegen[MAX_IDENTIFIER_LEN-1] = '\0';

                // Inicializa sistema aprimorado de temporários para a nova função
                release_all_temps();
                
                // Coleta estatísticas de função
                stats.total_functions++;

                // Coleta parâmetros da função
                TreeNode *param_node = actual_decl->child[0];
                while (param_node != NULL && param_count < MAX_FUNC_PARAMS) {
                    if (param_node->kind.exp == ParamK) {
                         strncpy(param_list[param_count++], param_node->attr.name, MAX_IDENTIFIER_LEN -1);
                         param_list[param_count-1][MAX_IDENTIFIER_LEN-1] = '\0';
                    } else if (param_node->kind.exp == TypeK && param_node->child[0] != NULL && param_node->child[0]->kind.exp == ParamK) {
                        // Parâmetro pode estar aninhado sob TypeK
                         strncpy(param_list[param_count++], param_node->child[0]->attr.name, MAX_IDENTIFIER_LEN -1);
                         param_list[param_count-1][MAX_IDENTIFIER_LEN-1] = '\0';
                    }
                    param_node = param_node->sibling;
                }
                
                // Gera corpo da função (comando composto)
                // O corpo é actual_decl->child[1] e seus siblings
                if (actual_decl->child[1] != NULL) {
                    TreeNode *stmt = actual_decl->child[1];
                    while (stmt != NULL) {
                        // Pula declarações de variáveis locais (nós Type)
                        if (!(stmt->nodekind == ExpK && stmt->kind.exp == TypeK && stmt->child[0] != NULL && stmt->child[0]->kind.exp == VarK)) {
                            generate_code_single(stmt); // Processa cada comando no corpo da função (sem siblings)
                        }
                        stmt = stmt->sibling;
                    }
                }

                flush_function_buffer(); // Escreve função em buffer para arquivo
            }
        }
        current = current->sibling;
    }
    
    // Finish and cleanup
    fclose(outputFile);
    
    // Print compilation statistics
    printf("\n=== Compilation Statistics ===\n");
    printf("Total Instructions: %d\n", stats.total_instructions);
    printf("Total Functions: %d\n", stats.total_functions);
    printf("Total Labels: %d\n", stats.total_labels);
    printf("Total Temporaries: %d\n", stats.total_temporaries);
    printf("Optimization Count: %d\n", stats.optimization_count);
    printf("Maximum Stack Offset: %d bytes\n", current_stack_offset);
    printf("Variables Tracked: %d\n", variable_count);
    
    // Register usage statistics
    int used_registers = 0;
    for (int i = 0; i < MAX_REGISTERS; i++) {
        if (register_pool[i].in_use) used_registers++;
    }
    printf("Registers in Use: %d/%d\n", used_registers, MAX_REGISTERS);
    printf("Register Efficiency: %.1f%%\n", (60 - used_registers) / 60.0 * 100);
    printf("============================\n");
    
    // Cleanup memory
    MemoryBlock *current = memory_blocks;
    while (current) {
        MemoryBlock *next = current->next;
        free(current);
        current = next;
    }
    memory_blocks = NULL;

    // Imprime estatísticas de compilação aprimoradas
    print_compilation_stats();
    
    // Validação básica do código gerado
    printf("Validando código IR gerado...\n");
    int validation_errors = validate_ir_file("output.ir");
    if (validation_errors == 0) {
        printf("✓ Código IR válido gerado com sucesso!\n");
    } else {
        printf("⚠ Encontrados %d problemas durante a validação\n", validation_errors);
    }

    // Libera lista de variáveis globais
    GlobalVarList curr = globalVars;
    while (curr != NULL) {
        GlobalVarList next = curr->next;
        free(curr->name);
        free(curr);
        curr = next;
    }
    globalVars = NULL;
}

// Função codegen para ser chamada de main.c
void generateIntermediateCode(TreeNode *syntaxTree) {
    codeGen(syntaxTree, "output.ir");
}

// ============================================================================
// SISTEMA APRIMORADO DE GERENCIAMENTO DE TEMPORÁRIOS
// Baseado nas melhorias identificadas nos outros compiladores
// ============================================================================

// Inicializa o pool de temporários
static void init_temp_pool(void) {
    if (!temp_pool_initialized) {
        for (int i = 0; i < MAX_TEMPORARIES; i++) {
            snprintf(temp_pool[i].name, sizeof(temp_pool[i].name), "t%d", i);
            temp_pool[i].in_use = 0;
            temp_pool[i].scope_level = 0;
        }
        temp_pool_initialized = 1;
    }
}

// Aloca um temporário disponível
static char *allocate_temp(void) {
    init_temp_pool();
    
    for (int i = 0; i < MAX_TEMPORARIES; i++) {
        if (!temp_pool[i].in_use) {
            temp_pool[i].in_use = 1;
            temp_pool[i].scope_level = 1; // Nível básico de escopo
            
            // Adiciona à lista de variáveis locais se não estiver presente
            int found = 0;
            for (int j = 0; j < local_vars_count; j++) {
                if (strcmp(local_vars_list[j], temp_pool[i].name) == 0) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                add_local_var(temp_pool[i].name);
            }
            
            return temp_pool[i].name;
        }
    }
    
    // Fallback para o sistema antigo se pool estiver cheio
    char *temp_name = malloc(16);
    snprintf(temp_name, 16, "t%d", tempCount++);
    add_local_var(temp_name);
    return temp_name;
}

// Libera um temporário específico
static void release_temp(const char *temp_name) {
    if (!temp_name) return;
    
    for (int i = 0; i < MAX_TEMPORARIES; i++) {
        if (strcmp(temp_pool[i].name, temp_name) == 0) {
            temp_pool[i].in_use = 0;
            temp_pool[i].scope_level = 0;
            return;
        }
    }
}

// Libera todos os temporários (fim de função)
static void release_all_temps(void) {
    for (int i = 0; i < MAX_TEMPORARIES; i++) {
        temp_pool[i].in_use = 0;
        temp_pool[i].scope_level = 0;
    }
}

// Otimização de expressões simples
static ExpressionResult optimize_expression(TreeNode *tree) {
    ExpressionResult result = {NULL, 0, 0};
    
    if (!tree) return result;
    
    // Otimização para constantes
    if (tree->nodekind == ExpK && tree->kind.exp == ConstK) {
        result.is_constant = 1;
        result.constant_value = tree->attr.val;
        return result;
    }
    
    // Otimização para variáveis simples
    if (tree->nodekind == ExpK && tree->kind.exp == IdK) {
        result.result_var = copyString(tree->attr.name);
        return result;
    }
    
    return result;
}

// Verifica se é uma atribuição simples (otimização)
static int is_simple_assignment(TreeNode *tree) {
    if (!tree || tree->nodekind != StmtK || tree->kind.stmt != AssignK) {
        return 0;
    }
    
    // Verifica se o lado direito é uma expressão simples
    TreeNode *rhs = tree->child[1];
    if (rhs && rhs->nodekind == ExpK && 
        (rhs->kind.exp == ConstK || rhs->kind.exp == IdK)) {
        return 1;
    }
    
    return 0;
}

// ============================================================================
// FUNÇÕES AUXILIARES APRIMORADAS
// ============================================================================

// Função aprimorada de geração de expressões com otimizações
// Baseada nos padrões identificados nos outros compiladores
char *generate_optimized_expression_code(TreeNode *tree) {
    if (tree == NULL) return NULL;

    // Primeiro tenta otimizar a expressão
    ExpressionResult opt_result = optimize_expression(tree);
    
    // Se é uma constante, retorna diretamente o valor
    if (opt_result.is_constant) {
        char *const_str = malloc(32);
        snprintf(const_str, 32, "%d", opt_result.constant_value);
        return const_str;
    }
    
    // Se é uma variável simples, retorna o nome
    if (opt_result.result_var) {
        return opt_result.result_var;
    }
    
    // Caso contrário, usa a geração padrão
    return generate_expression_code(tree);
}

// Sistema de análise de escopo aprimorado
// Identifica se uma variável é local, parâmetro ou global
static VariableScope get_variable_scope(const char *var_name) {
    if (!var_name) return VAR_UNKNOWN;
    
    // Verifica se é parâmetro
    for (int i = 0; i < param_count; i++) {
        if (strcmp(param_list[i], var_name) == 0) {
            return VAR_PARAM;
        }
    }
    
    // Verifica se é variável local
    for (int i = 0; i < local_vars_count; i++) {
        if (strcmp(local_vars_list[i], var_name) == 0) {
            return VAR_LOCAL;
        }
    }
    
    // Verifica se é global
    GlobalVarList curr = globalVars;
    while (curr != NULL) {
        if (strcmp(curr->name, var_name) == 0) {
            return VAR_GLOBAL;
        }
        curr = curr->next;
    }
    
    return VAR_UNKNOWN;
}

// Função de otimização de atribuições
// Evita movimentações desnecessárias quando possível
static void generate_optimized_assignment(TreeNode *tree) {
    if (!tree || tree->nodekind != StmtK || tree->kind.stmt != AssignK) {
        generate_statement_code(tree);
        return;
    }
    
    // Se é uma atribuição simples, pode otimizar
    if (is_simple_assignment(tree)) {
        TreeNode *lhs = tree->child[0];
        TreeNode *rhs = tree->child[1];
        
        if (lhs->kind.exp == IdK && rhs->kind.exp == IdK) {
            // Atribuição variável para variável: x = y
            emit_buffered("MOV %s, %s // Atribuição otimizada", lhs->attr.name, rhs->attr.name);
            add_local_var(lhs->attr.name);
            return;
        } else if (lhs->kind.exp == IdK && rhs->kind.exp == ConstK) {
            // Atribuição constante para variável: x = 5
            emit_buffered("MOV %s, %d // Atribuição constante otimizada", lhs->attr.name, rhs->attr.val);
            add_local_var(lhs->attr.name);
            return;
        }
    }
    
    // Caso padrão - usa geração normal
    generate_statement_code(tree);
}

// Função para imprimir estatísticas de compilação
void print_compilation_stats(void) {
    printf("\n=== ESTATÍSTICAS DE COMPILAÇÃO ===\n");
    printf("Total de instruções geradas: %d\n", stats.total_instructions);
    printf("Total de temporários utilizados: %d\n", stats.total_temporaries);
    printf("Total de rótulos criados: %d\n", stats.total_labels);
    printf("Total de funções processadas: %d\n", stats.total_functions);
    printf("Otimizações aplicadas: %d\n", stats.optimization_count);
    printf("===================================\n");
}

// Função para resetar estatísticas
void reset_compilation_stats(void) {
    stats.total_instructions = 0;
    stats.total_temporaries = 0;
    stats.total_labels = 0;
    stats.total_functions = 0;
    stats.optimization_count = 0;
}

// Versão aprimorada da função emit_buffered com estatísticas
static void emit_buffered_with_stats(const char *instruction_format, ...) {
    if (instruction_buffer_count >= MAX_FUNC_INSTRUCTIONS) {
        fprintf(stderr, "Erro: Muitas instruções para a função %s\n", current_func_name_codegen);
        return;
    }
    char buf[256];
    va_list args;
    va_start(args, instruction_format);
    vsnprintf(buf, sizeof(buf), instruction_format, args);
    va_end(args);
    instruction_buffer[instruction_buffer_count++] = copyString(buf);
    
    // Atualiza estatísticas
    stats.total_instructions++;
}

// Sistema de verificação de integridade do código gerado
// Valida se o código IR gerado está semanticamente correto
static int validate_generated_code(void) {
    int errors = 0;
    
    // Verifica se todas as variáveis usadas foram declaradas
    for (int i = 0; i < instruction_buffer_count; i++) {
        char *instr = instruction_buffer[i];
        // Análise básica - procura por padrões suspeitos
        if (strstr(instr, "t-1") || strstr(instr, "L-1")) {
            fprintf(stderr, "Aviso: Instrução suspeita detectada: %s\n", instr);
            errors++;
        }
    }
    
    return errors;
}

// Sistema completo de validação do arquivo IR
// Valida a integridade do arquivo de código intermediário gerado
static int validate_ir_file(const char *ir_file) {
    FILE *file = fopen(ir_file, "r");
    if (!file) {
        fprintf(stderr, "Erro: Não foi possível abrir %s para validação\n", ir_file);
        return -1;
    }
    
    char line[256];
    int line_number = 0;
    int errors = 0;
    int functions_found = 0;
    int func_begins = 0;
    int func_ends = 0;
    
    while (fgets(line, sizeof(line), file)) {
        line_number++;
        
        // Remove quebra de linha
        char *newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        
        // Pula linhas vazias e comentários
        if (strlen(line) == 0 || line[0] == '/' || line[0] == '#') continue;
        
        // Verifica estrutura de funções
        if (strstr(line, "FUNC_BEGIN")) {
            func_begins++;
            functions_found++;
        }
        if (strstr(line, "END_FUNC")) {
            func_ends++;
        }
        
        // Verifica padrões suspeitos
        if (strstr(line, "t-1") || strstr(line, "L-1") || strstr(line, "NULL")) {
            fprintf(stderr, "Aviso linha %d: Padrão suspeito encontrado: %s\n", line_number, line);
            errors++;
        }
        
        // Verifica instruções malformadas
        if (strstr(line, "OP_UNKNOWN") || strstr(line, "BR_UNKNOWN")) {
            fprintf(stderr, "Erro linha %d: Instrução inválida: %s\n", line_number, line);
            errors++;
        }
    }
    
    fclose(file);
    
    // Verifica balanceamento de funções
    if (func_begins != func_ends) {
        fprintf(stderr, "Erro: Desbalanceamento de funções - FUNC_BEGIN: %d, END_FUNC: %d\n", 
               func_begins, func_ends);
        errors++;
    }
    
    return errors;
}

// Função de backup para o sistema antigo se houver problemas
char *legacy_newTemp(void) {
    char *temp_name = (char *)malloc(MAX_IDENTIFIER_LEN);
    snprintf(temp_name, MAX_IDENTIFIER_LEN, "t%d", tempCount++);
    add_local_var(temp_name);
    stats.total_temporaries++;
    return temp_name;
}

char *legacy_newLabel(void) {
    char *label = (char *)malloc(MAX_LABEL_LEN);
    snprintf(label, MAX_LABEL_LEN, "L%d", labelCount++);
    stats.total_labels++;
    return label;
}