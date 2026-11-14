/*
 * scanner.c - Adaptador entre o scanner do TP1 (microc.flex) e o
 * analisador sintatico gerado (secao 3.1 do enunciado).
 *
 * Usa o scanner flex gerado com %option prefix="microc_yy" e a
 * variavel global linha_atual (TP1, secao 4.4).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scanner.h"
#include "microc_yylval.h"

/* ---- simbolos do scanner gerado pelo flex ---- */
extern int   microc_yylex(void);
extern FILE *microc_yyin;
extern char *microc_yytext;
extern int   linha_atual;
extern int   microc_yylex_destroy(void);

/* Libera o lexema dinamico do token anterior. So e chamada depois de
 * copiar o texto para Token.lexema. */
static void liberar_simbolo(void)
{
    free(microc_yylval.symbol);
    free(microc_yylval.error_msg);
    microc_yylval.symbol = NULL;
    microc_yylval.error_msg = NULL;
}

/* Texto a guardar em Token.lexema para o token recem-lido:
 *   - UNDEF: mensagem de erro do TP1;
 *   - ID e constantes: Symbol, com escapes ja convertidos;
 *   - demais tokens: texto reconhecido (microc_yytext). */
static const char *texto_do_lexema(int tipo)
{
    if (tipo == UNDEF && microc_yylval.error_msg != NULL)
        return microc_yylval.error_msg;
    if (microc_yylval.symbol != NULL)
        return microc_yylval.symbol;
    return microc_yytext;
}

/* O TP1 permite que tokens de um caractere sejam devolvidos pelo proprio
 * codigo ASCII (TP1, secao 5). O analisador gerado compara com os nomes
 * de TokenType, entao esses codigos sao convertidos aqui.
 * Cuidado: valores ate o ultimo item da enumeracao (PRINT = 36 no
 * tokens.h do livro) sao sempre TokenType. Por isso '!' (33, que e
 * RETURN), '"', '#' e '$' nao podem ser devolvidos como ASCII. */
static TokenType normalizar(int tipo)
{
    if (tipo <= (int) PRINT)
        return (TokenType) tipo;
    switch (tipo) {
    case '+': return PLUS;
    case '-': return MINUS;
    case '*': return MUL;
    case '/': return DIV;
    case '%': return MOD;
    case '<': return LT;
    case '>': return GT;
    case '=': return ASSIGN;
    case ';': return SEMICOLON;
    case ',': return COMMA;
    case '(': return LPAREN;
    case ')': return RPAREN;
    case '{': return LBRACE;
    case '}': return RBRACE;
    case '[': return LBRACKET;
    case ']': return RBRACKET;
    default:  return (TokenType) tipo;
    }
}

void scanner_iniciar(FILE *entrada)
{
    microc_yyin = entrada;
    linha_atual = 1;
}

Token proximo_token(void)
{
    Token t;
    int tipo;
    const char *texto;

    memset(&t, 0, sizeof t);
    microc_yylval.symbol = NULL;
    microc_yylval.error_msg = NULL;
    tipo = microc_yylex();
    /* O flex devolve 0 no fim da entrada; o TP1 deve devolver
     * END_OF_FILE na regra <<EOF>>. */
    t.tipo = normalizar(tipo);
    t.linha = linha_atual;
    texto = t.tipo == END_OF_FILE ? "" : texto_do_lexema(tipo);
    if (texto != NULL) {
        strncpy(t.lexema, texto, sizeof t.lexema - 1);
        t.lexema[sizeof t.lexema - 1] = '\0';
    }
    liberar_simbolo();
    return t;
}

void scanner_finalizar(void)
{
    liberar_simbolo();
    microc_yylex_destroy();
}