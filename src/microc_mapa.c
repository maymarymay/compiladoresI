/*
 * microc_mapa.c - Correspondencia com os tokens de Micro C (ver microc_mapa.h).
 *
 * A tabela de literais segue o tokens.h do livro-texto (secao 2.3.1).
 * Se o tokens.h do TP1 usar outros nomes, ajuste a tabela.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "microc_mapa.h"
#include "util.h"

static const char *const literais[][2] = {
    { "+", "PLUS" },     { "-", "MINUS" },     { "*", "MUL" },
    { "/", "DIV" },      { "%", "MOD" },
    { "==", "EQ" },      { "!=", "NEQ" },      { "<", "LT" },
    { ">", "GT" },       { "<=", "LEQ" },      { ">=", "GEQ" },
    { "&&", "AND" },     { "||", "OR" },       { "!", "NOT" },
    { "=", "ASSIGN" },   { ";", "SEMICOLON" }, { ",", "COMMA" },
    { "(", "LPAREN" },   { ")", "RPAREN" },    { "{", "LBRACE" },
    { "}", "RBRACE" },   { "[", "LBRACKET" },  { "]", "RBRACKET" },
    { "main", "MAIN" },  { "if", "IF" },       { "else", "ELSE" },
    { "for", "FOR" },    { "return", "RETURN" }, { "int", "INT" },
    { "char", "CHAR" },  { "print", "PRINT" }
};

static char **nomes = NULL;
static int nnomes = 0, capnomes = 0;

const char *microc_nome_literal(const char *literal)
{
    size_t i;
    for (i = 0; i < sizeof(literais) / sizeof(literais[0]); i++)
        if (strcmp(literais[i][0], literal) == 0)
            return literais[i][1];
    return NULL;
}

static void adicionar_nome(const char *s, size_t n)
{
    if (nnomes == capnomes) {
        capnomes = capnomes ? 2 * capnomes : 64;
        nomes = (char **) xrealloc(nomes, (size_t) capnomes * sizeof(char *));
    }
    nomes[nnomes++] = xstrndup(s, n);
}

/* Le o arquivo inteiro para um buffer terminado em '\0'.
 * Devolve NULL em caso de erro; *tam recebe o tamanho lido. */
static char *ler_texto(const char *arq, size_t *tam)
{
    FILE *f;
    long n;
    char *buf;
    size_t lidos;

    f = fopen(arq, "rb");
    if (f == NULL)
        return NULL;
    if (fseek(f, 0L, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    n = ftell(f);
    if (n < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    buf = (char *) malloc((size_t) n + 1);
    if (buf == NULL) {
        fclose(f);
        return NULL;
    }
    lidos = fread(buf, 1, (size_t) n, f);
    fclose(f);
    buf[lidos] = '\0';
    *tam = lidos;
    return buf;
}

/* Remove comentarios de bloco e de linha, in place. */
static void remove_comentarios(char *buf, size_t *tam)
{
    size_t i = 0, j = 0, n = *tam;

    while (i < n) {
        if (buf[i] == '/' && i + 1 < n && buf[i + 1] == '*') {
            i += 2;
            while (i + 1 < n && !(buf[i] == '*' && buf[i + 1] == '/'))
                i++;
            i += 2;
            buf[j++] = ' ';
        } else if (buf[i] == '/' && i + 1 < n && buf[i + 1] == '/') {
            while (i < n && buf[i] != '\n')
                i++;
        } else {
            buf[j++] = buf[i++];
        }
    }
    buf[j] = '\0';
    *tam = j;
}

/* Le o tokens.h, localiza a enumeracao TokenType e cadastra cada
 * identificador da lista. Valores explicitos ("X = 3") sao ignorados.
 * Retorna 0 em caso de sucesso e -1 se o arquivo nao puder ser usado. */
int microc_carregar_tokens_h(const char *caminho)
{
    const char *arq = caminho ? caminho : "microc/tokens.h";
    char *buf;
    size_t tam = 0;
    char *p;
    char *q;
    char *ini;
    int achou = 0;

    buf = ler_texto(arq, &tam);
    if (buf == NULL)
        return -1;
    remove_comentarios(buf, &tam);

    p = strstr(buf, "enum");
    if (p == NULL) {
        free(buf);
        return -1;
    }
    p = strchr(p, '{');
    if (p == NULL) {
        free(buf);
        return -1;
    }
    q = strchr(p, '}');
    if (q == NULL) {
        free(buf);
        return -1;
    }
    *q = '\0';
    p++;

    while (*p != '\0') {
        while (*p != '\0' && !isalpha((unsigned char) *p) && *p != '_')
            p++;
        if (*p == '\0')
            break;
        ini = p;
        while (*p != '\0' && (isalnum((unsigned char) *p) || *p == '_'))
            p++;
        adicionar_nome(ini, (size_t) (p - ini));
        achou = 1;
        /* Pula um eventual "= valor" ate a proxima virgula. */
        while (*p != '\0' && *p != ',')
            p++;
    }

    free(buf);
    return achou ? 0 : -1;
}

int microc_token_existe(const char *nome)
{
    int i;
    for (i = 0; i < nnomes; i++)
        if (strcmp(nomes[i], nome) == 0)
            return 1;
    return 0;
}

void microc_liberar(void)
{
    int i;
    for (i = 0; i < nnomes; i++)
        free(nomes[i]);
    free(nomes);
    nomes = NULL;
    nnomes = capnomes = 0;
}