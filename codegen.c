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
    char *temp_name = (char *)malloc(MAX_IDENTIFIER_LEN);
    snprintf(temp_name, MAX_IDENTIFIER_LEN, "t%d", tempCount++);
    add_local_var(temp_name);
    return temp_name;
}

// Cria um novo rótulo único para a função atual
// Usado para instruções de salto e controle de fluxo
char *newLabel(void) {
    char *label = (char *)malloc(MAX_LABEL_LEN);
    // Para rótulos mais únicos se necessário: snprintf(label, MAX_LABEL_LEN, "L_%s_%d", current_func_name_codegen, labelCount++);
    snprintf(label, MAX_LABEL_LEN, "L%d", labelCount++);
    return label;
}


// --- Novas Funções de Emissão ---

// Emite declaração de variável global no arquivo de saída
// size > 0 para arrays, 0 para variáveis simples
static void emit_global_decl(const char *name, int size) {
    if (size > 0) { // Assumindo size > 0 para arrays, 0 ou 1 para variáveis simples
        fprintf(outputFile, "GLOBAL_ARRAY %s, %d\n", name, size);
    } else {
        fprintf(outputFile, "GLOBAL %s\n", name); // Variável global simples
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
    fprintf(outputFile, "FUNC_BEGIN %s, %d\n", current_func_name_codegen, param_count);

    for (int i = 0; i < param_count; ++i) {
        fprintf(outputFile, "  PARAM %s\n", param_list[i]);
    }

    if (local_vars_count > 0) {
        fprintf(outputFile, "  LOCAL ");
        for (int i = 0; i < local_vars_count; ++i) {
            fprintf(outputFile, "%s%s", local_vars_list[i], (i == local_vars_count - 1) ? "" : ", ");
        }
        fprintf(outputFile, "\n");
    }

    for (int i = 0; i < instruction_buffer_count; ++i) {
        fprintf(outputFile, "  %s\n", instruction_buffer[i]);
        free(instruction_buffer[i]);
    }
    fprintf(outputFile, "END_FUNC %s\n\n", current_func_name_codegen);

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
                        emit_buffered("LOAD_ARRAY %s, %s, %s", result_var, tree->attr.name, index_expr);
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
                        emit_buffered("%s %s, %s, %s", op_str, result_var, left_operand, right_operand);
                    } else {
                        fprintf(stderr, "Erro: Operador de comparação usado em contexto de expressão aritmética.\n");
                        // Fallback: trata como movimento simples se deve produzir algo
                        emit_buffered("MOV %s, %s // AVISO: fallback OpK", result_var, left_operand);
                    }
                    // free(left_operand); free(right_operand); // Se alocados
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
                            emit_buffered("ARG %s", arg_vars[i]);
                        }

                        // Determina se a função retorna um valor.
                        // funções void são 'output', 'sort'.
                        // Outras retornam um valor.
                        int is_void_call = (strcmp(tree->attr.name, "output") == 0 || strcmp(tree->attr.name, "sort") == 0);

                        emit_buffered("CALL %s, %d", tree->attr.name, arg_count);

                        if (!is_void_call) {
                            result_var = newTemp();
                            emit_buffered("STORE_RET %s", result_var);
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
                        emit_buffered("STORE_ARRAY %s, %s, %s", tree->child[0]->attr.name, idx_expr, rhs_var);
                        // free(idx_expr);
                    } else { // Atribuição simples: var = val
                        // LHS pode ser IdK ou VarK baseado no parser original
                        if (tree->child[0]->nodekind == ExpK && (tree->child[0]->kind.exp == IdK || tree->child[0]->kind.exp == VarK)) {
                            lhs_var = tree->child[0]->attr.name;
                            add_local_var(lhs_var); // Garante que variável LHS está em locais se não for param/global
                            emit_buffered("MOV %s, %s", lhs_var, rhs_var);
                        } else {
                            fprintf(stderr, "Erro: LHS da atribuição não é um tipo de variável reconhecido.\n");
                        }
                    }
                    // free(rhs_var); // Se alocado e temporário
                    break;

                case IfK: // Comando condicional if-then-else
                    label1 = newLabel(); // Rótulo para parte else ou fim do if
                    // Geração da condição (OpK assumido para condições C-minus)
                    if (tree->child[0]->kind.exp == OpK) {
                        char *op1 = generate_expression_code(tree->child[0]->child[0]);
                        char *op2 = generate_expression_code(tree->child[0]->child[1]);
                        emit_buffered("CMP %s, %s", op1, op2);
                        // free(op1); free(op2);

                        // Salta se condição é FALSA para label1
                        const char *branch_instr = get_ir_branch_instruction(tree->child[0]->attr.opr, 0); // salta em falso
                        emit_buffered("%s %s", branch_instr, label1);
                    } else { // Condição de variável booleana simples (não típico em C-minus)
                        char *cond_var = generate_expression_code(tree->child[0]);
                        emit_buffered("CMP %s, 0 // Assumindo 0 é falso", cond_var); // Ou usar valor específico true/false
                        emit_buffered("BR_EQ %s // Salta se cond_var é falso (0)", label1);
                        // free(cond_var);
                    }

                    // Processa todos os comandos na parte then
                    TreeNode *then_stmt = tree->child[1];
                    while (then_stmt != NULL) {
                        generate_code_single(then_stmt);
                        then_stmt = then_stmt->sibling;
                    }

                    if (tree->child[2] != NULL) { // Parte else existe
                        label2 = newLabel(); // Rótulo para fim do if-else
                        emit_buffered("GOTO %s", label2);
                        emit_buffered("%s:", label1); // Rótulo para else
                        // Processa todos os comandos na parte else
                        TreeNode *else_stmt = tree->child[2];
                        while (else_stmt != NULL) {
                            generate_code_single(else_stmt);
                            else_stmt = else_stmt->sibling;
                        }
                        emit_buffered("%s:", label2); // Rótulo para fim do if-else
                        free(label2);
                    } else { // Não há parte else
                        emit_buffered("%s:", label1); // Rótulo para fim do if
                    }
                    free(label1);
                    break;

                case WhileK: // Comando de repetição while
                    label1 = newLabel(); // Rótulo para verificação da condição
                    label2 = newLabel(); // Rótulo para fim do loop
                    
                    emit_buffered("%s: // Condição do while", label1);
                    // Geração da condição
                    if (tree->child[0]->kind.exp == OpK) {
                        char *op1 = generate_expression_code(tree->child[0]->child[0]);
                        char *op2 = generate_expression_code(tree->child[0]->child[1]);
                        emit_buffered("CMP %s, %s", op1, op2);
                        // free(op1); free(op2);

                        // Salta se condição é FALSA para label2 (fim do loop)
                        const char *branch_instr = get_ir_branch_instruction(tree->child[0]->attr.opr, 0); // salta em falso
                        emit_buffered("%s %s", branch_instr, label2);
                    } else {
                         char *cond_var = generate_expression_code(tree->child[0]);
                         emit_buffered("CMP %s, 0 // Assumindo 0 é falso para while", cond_var);
                         emit_buffered("BR_EQ %s // Salta se cond_var é falso (0)", label2);
                         // free(cond_var);
                    }

                    // Processa todos os comandos no corpo do loop
                    TreeNode *loop_stmt = tree->child[1];
                    while (loop_stmt != NULL) {
                        generate_code_single(loop_stmt);
                        loop_stmt = loop_stmt->sibling;
                    }
                    emit_buffered("GOTO %s", label1); // Volta para verificação da condição
                    emit_buffered("%s:", label2); // Rótulo para fim do loop
                    free(label1);
                    free(label2);
                    break;

                case ReturnK: // Comando de retorno
                    if (tree->child[0] != NULL) {
                        val_expr = generate_expression_code(tree->child[0]);
                        emit_buffered("RETURN %s", val_expr);
                        // free(val_expr);
                    } else {
                        emit_buffered("RETURN_VOID");
                    }
                    break;
                
                // Removido caso VarDeclK, pois AST C-minus pode não tê-lo explicitamente.
                // Variáveis locais são tipicamente identificadas por uso (ex. no LHS de assign)
                // ou como parâmetros.

                default:
                    fprintf(stderr, "Erro: Tipo de comando desconhecido em generate_statement_code.\n");
                    break;
            }
            break;
        case ExpK: // Expressão standalone, ex. chamada de função não parte de uma atribuição
            if (tree->kind.exp == CallK) {
                generate_expression_code(tree); // Irá emitir ARG, CALL
            } else {
                // Outras expressões como comandos geralmente não têm efeito colateral ou são erros
                // generate_expression_code(tree); // Poderia gerar código mas resultado não é usado
            }
            break;
        default:
            fprintf(stderr, "Erro: Nó não-comando/expressão em generate_statement_code.\n");
            break;
    }
}


// Função de geração de código recursiva (sem processamento de irmãos)
// Processa apenas o nó atual sem navegar pelos siblings
// Usada para evitar processamento duplicado em contextos onde os siblings
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
    
    fclose(outputFile);

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

// Função codegen para ser chamada de main.c (se main.c espera a assinatura antiga)
// Para compatibilidade se main.c chama codeGen(syntaxTree) sem nome de arquivo
void generateIntermediateCode(TreeNode *syntaxTree) {
    codeGen(syntaxTree, "output.ir");
}