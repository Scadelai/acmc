# acmc - Another C- Compiler

## Estruturua

* **Makefile** : Script para compilação, limpeza e verificação de vazamentos de memória.
* **acmc.l** : Arquivo de especificações do Flex (análise léxica).
* **acmc.y** : Arquivo de especificações do Bison (análise sintática).
* **analyze.c** : Módulo responsável pela análise semântica.
* **symtab.c** : Módulo para a construção e manipulação da tabela de símbolos.
* **util.c** : Funções utilitárias utilizadas pelo compilador.
* **main.c** : Função principal que integra todas as etapas do compilador.

## Requisitos

* **GCC**
* **Flex**
* **Bison**
* **Valgrind** (para a verificação de vazamentos de memória)

## Compilação

Para compilar o projeto, basta executar:

```bash
make
```

Isso gerará o executável `acmc`.

## Execução

Para executar o compilador, utilize o seguinte comando:

```bash
./acmc <nome_do_arquivo>
```

**Observação:**

Se o nome do arquivo fornecido não contiver uma extensão, a extensão `.c-` será automaticamente adicionada.

## Limpeza

Para remover os arquivos gerados durante a compilação, execute:

```bash
make clean
```

## Verificação de Vazamentos de Memória

Para checar possíveis vazamentos de memória durante a execução, use:

```bash
make check
```

Este comando executa o compilador com o  **Valgrind** .
