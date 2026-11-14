aluno: Maryana 202319040649; Mileny 202319040282
uso_ia: sim

# TP2 - Gerador de Analisadores Sintáticos

Esse trabalho é um programa em C (o `gerador`) que lê uma gramática escrita em EBNF e cria o código de um analisador sintático. Depois a gente compila esse código e usa o analisador para ver se um programa está certo ou não.

O uso de IA está no arquivo `USO_IA.md`.

## O que precisa ter instalado

```
sudo apt install build-essential flex valgrind
```

## Compilar

```
make
```

Isso cria o programa `./gerador`. Para apagar tudo e começar de novo, use `make clean`.

## Rodar os testes

```
make test
```

No final tem que aparecer que todos os testes passaram.

## Como usar

1. Gerar o analisador:

```
./gerador testes/gramaticas/expr.ebnf -o build/expr
```

2. Compilar o analisador que foi gerado:

```
gcc build/expr.c -o build/expr_bin
```

3. Rodar com um programa de entrada:

```
./build/expr_bin testes/programas/expr/aceito_1.txt
```

Se estiver tudo certo aparece `entrada aceita`. Se tiver erro, ele mostra a linha, a coluna e o que esperava, por exemplo:

```
linha 1, coluna 10: encontrado ')', esperado um de {NUM, ID, '('}
```

### Opções do gerador

- `--sets`: mostra anulável, FIRST e FOLLOW
- `--debug`: mostra a gramática do jeito que foi lida
- `--tree`: no passo 2, deixa o analisador mostrar a árvore (use `./build/expr_bin --tree arquivo.txt`)

### Gramáticas com erro

O gerador não gera nada quando a gramática tem erro, por exemplo recursão à esquerda ou conflito LL(1). Para ver:

```
./gerador testes/gramaticas/conflito_ll1.ebnf -o build/x
```

## Micro C

Para o Micro C o analisador usa o scanner do TP1 (pasta `microc/`). O `make` já compila ele.

```
./build/microc/parser_microc testes/programas/microc/soma.mc
```

Também dá para ler um arquivo de tokens gerado pelo TP1:

```
./lexer programa.mc > tokens.txt
./build/microc/parser_microc --tokens tokens.txt
```

O `./lexer` é o executável do TP1.

### O que mudou do TP1

- os tokens saíram do `microc.flex` e foram para o `tokens.h`
- a struct `YYSTYPE` foi para o `microc_yylval.h`
- o flex usa `%option prefix="microc_yy" noyywrap` e no `<<EOF>>` devolve `END_OF_FILE`
- tirei o `main()` do TP1
- o `microc/scanner.c` transforma o resultado do scanner em `Token`

  ## O que foi feito

O gerador faz 5 etapas, sempre nessa ordem, e para na primeira que achar erro:

1. **Leitura da gramática** (`ebnf_parser.c`): lê as regras, com `|`, `[ ]`, `{ }` e `( )`, e monta a árvore de cada regra. Mostra erros com linha e coluna.
2. **Verificação da gramática** (`verificacao.c`): acha não terminal usado e não definido, terminal não declarado, não terminal inalcançável (só aviso) e não terminal que não gera nada (improdutivo).
3. **Anulável, FIRST e FOLLOW** (`conjuntos.c`), e também a **recursão à esquerda**, direta e indireta (`verificacao.c`), que precisa do anulável.
4. **Condição LL(1)** (`ll1.c`): recusa quando duas alternativas começam igual, e avisa no caso do `else` pendente.
5. **Geração do código** (`codegen.c`): escreve o `.c` e o `.h` do analisador, com o léxico feito a partir das expressões regulares e a leitura do arquivo de tokens.

### Gramáticas de teste

- Válidas, com programas aceitos e rejeitados: `expr`, `json` e `microc`
- Válida só com aviso: `ll1_opc_follow`
- Recusadas de propósito: `recursao_esquerda`, `recursao_mascarada`, `conflito_ll1`, `ll1_ambas_anulaveis`, `ll1_first_comum` e `ll1_follow_colide`

O `make test` roda 29 testes e todos passam. O `valgrind` não achou vazamento no gerador nem nos analisadores gerados.

## Pastas

- `src/`: código do gerador
- `microc/`: scanner do TP1 adaptado
- `testes/`: gramáticas, programas de teste e saídas esperadas
