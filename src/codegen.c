/*
 * codegen.c - Geracao do analisador sintatico (etapa 5).
 *
 * Estrutura do arquivo .c gerado:
 *
 *   1. includes e conjuntos constantes (conj_0, conj_1, ...)
 *   2. estado e primitivas: avancar, match, erro_sintatico, arvore
 *   3. modo proprio: analisador lexico gerado a partir de TOKENS
 *      modo Micro C: tokens obtidos de proximo_token() (scanner do TP1)
 *   4. leitura de um arquivo de tokens (opcao --tokens do analisador)
 *   5. um procedimento parse_X() por nao terminal
 *   6. main
 *
 * Fornecido: toda a estrutura acima, os nomes dos tokens, os conjuntos,
 * as primitivas, o lexico (exceto o reconhecimento das expressoes
 * regulares) e a traducao de terminais e nao terminais.
 *
 * TODO(aluno):
 *   - gen_token_de_arquivo: leitura do arquivo de tokens do TP1.
 */
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen.h"
#include "microc_mapa.h"
#include "util.h"

/* ------------------------------------------------------------------ */
/* Emissao de texto                                                   */
/* ------------------------------------------------------------------ */

typedef struct {
    FILE *f;
    int nivel;          /* nivel de indentacao (4 espacos por nivel) */
} Emissor;

/* Emite uma linha indentada; fmt segue printf. */
static void emit(Emissor *em, const char *fmt, ...)
{
    int i;
    va_list ap;
    for (i = 0; i < em->nivel; i++)
        fputs("    ", em->f);
    va_start(ap, fmt);
    vfprintf(em->f, fmt, ap);
    va_end(ap);
    fputc('\n', em->f);
}

/* Emite linhas fixas, sem interpretar '%'. A lista termina em NULL. */
static void emit_linhas(FILE *f, const char *const *linhas)
{
    for (; *linhas != NULL; linhas++) {
        fputs(*linhas, f);
        fputc('\n', f);
    }
}

/* Texto dentro de comentario C: evita fechar o comentario por engano. */
static void emitir_comentario(FILE *f, const char *s)
{
    for (; *s != '\0'; s++) {
        if (s[0] == '*' && s[1] == '/') {
            fputs("* /", f);
            s++;
        } else if (*s == '\n') {
            fputc(' ', f);
        } else {
            fputc(*s, f);
        }
    }
}

/* String C entre aspas, com escapes. */
static void emitir_string_c(FILE *f, const char *s)
{
    fputc('"', f);
    for (; *s != '\0'; s++) {
        unsigned char c = (unsigned char) *s;
        if (c == '"' || c == '\\')
            fprintf(f, "\\%c", c);
        else if (c == '\n')
            fputs("\\n", f);
        else if (c == '\t')
            fputs("\\t", f);
        else if (c < 32 || c > 126)
            fprintf(f, "\\%03o", c);
        else
            fputc(c, f);
    }
    fputc('"', f);
}

/* ------------------------------------------------------------------ */
/* Nomes dos tokens no codigo gerado                                  */
/* ------------------------------------------------------------------ */

static const char *const nomes_simbolos[][2] = {
    { "+", "MAIS" }, { "-", "MENOS" }, { "*", "VEZES" }, { "/", "BARRA" },
    { "%", "PORCENTO" }, { "(", "ABRE_PAR" }, { ")", "FECHA_PAR" },
    { "[", "ABRE_COL" }, { "]", "FECHA_COL" }, { "{", "ABRE_CHAVE" },
    { "}", "FECHA_CHAVE" }, { ";", "PV" }, { ",", "VIRGULA" },
    { ".", "PONTO" }, { ":", "DOIS_PONTOS" }, { "=", "IGUAL" },
    { "==", "IGUAL_IGUAL" }, { "!=", "DIFERENTE" }, { "<", "MENOR" },
    { ">", "MAIOR" }, { "<=", "MENOR_IGUAL" }, { ">=", "MAIOR_IGUAL" },
    { "&&", "E_LOGICO" }, { "||", "OU_LOGICO" }, { "!", "NAO" },
    { ":=", "ATRIBUICAO" }, { "->", "SETA" }
};

static int eh_palavra(const char *s)
{
    if (!isalpha((unsigned char) *s) && *s != '_')
        return 0;
    for (; *s != '\0'; s++)
        if (!isalnum((unsigned char) *s) && *s != '_')
            return 0;
    return 1;
}

static int nome_em_uso(const Gramatica *g, int ate, const char *nome)
{
    int i;
    for (i = 0; i < ate; i++)
        if (strcmp(g->terms[i].nome_c, nome) == 0)
            return 1;
    return 0;
}

/* Define g->terms[i].nome_c para todos os terminais. */
static void nomear_terminais(Gramatica *g, int microc)
{
    int i;
    size_t k;
    char buf[256];
    Terminal *t;

    for (i = 0; i < g->nterms; i++) {
        t = &g->terms[i];
        free(t->nome_c);
        if (i == TERM_FIM) {
            t->nome_c = xstrdup(microc ? "END_OF_FILE" : "T_FIM");
            continue;
        }
        if (microc) {
            t->nome_c = xstrdup(t->literal ? microc_nome_literal(t->nome) : t->nome);
            continue;
        }
        buf[0] = '\0';
        if (!t->literal) {
            sprintf(buf, "T_%.200s", t->nome);
        } else if (eh_palavra(t->nome) && strlen(t->nome) < 200) {
            sprintf(buf, "T_%s", t->nome);
            for (k = 2; buf[k] != '\0'; k++)
                buf[k] = (char) toupper((unsigned char) buf[k]);
        } else {
            for (k = 0; k < sizeof(nomes_simbolos) / sizeof(nomes_simbolos[0]); k++)
                if (strcmp(nomes_simbolos[k][0], t->nome) == 0)
                    sprintf(buf, "T_%s", nomes_simbolos[k][1]);
            if (buf[0] == '\0')
                sprintf(buf, "T_LIT_%d", i);
        }
        if (nome_em_uso(g, i, buf))
            sprintf(buf + strlen(buf), "_%d", i);
        t->nome_c = xstrdup(buf);
    }
}

/* ------------------------------------------------------------------ */
/* Conjuntos constantes                                               */
/* ------------------------------------------------------------------ */

#define MAX_COMPARACOES 3   /* ate 3 tokens: tok.tipo == A || ...; acima: pertence */

static Bitset *conjuntos = NULL;
static int nconj = 0, capconj = 0;

static int bitset_igual(const Bitset *a, const Bitset *b)
{
    int i;
    for (i = 0; i < a->n; i++)
        if (bitset_tem(a, i) != bitset_tem(b, i))
            return 0;
    return 1;
}

/* Registra um conjunto e devolve seu numero N (emitido como conj_N).
 * Conjuntos iguais compartilham a mesma constante. */
static int registrar_conjunto(const Bitset *b)
{
    int i;
    for (i = 0; i < nconj; i++)
        if (bitset_igual(&conjuntos[i], b))
            return i;
    if (nconj == capconj) {
        capconj = capconj ? 2 * capconj : 16;
        conjuntos = (Bitset *) xrealloc(conjuntos, (size_t) capconj * sizeof(Bitset));
    }
    bitset_iniciar(&conjuntos[nconj], b->n);
    bitset_copiar(&conjuntos[nconj], b);
    return nconj++;
}

static void liberar_conjuntos(void)
{
    int i;
    for (i = 0; i < nconj; i++)
        bitset_liberar(&conjuntos[i]);
    free(conjuntos);
    conjuntos = NULL;
    nconj = capconj = 0;
}

static void emitir_conjuntos(FILE *f, const Gramatica *g)
{
    int i, t;
    for (i = 0; i < nconj; i++) {
        fprintf(f, "static const int conj_%d[] = { ", i);
        for (t = bitset_proximo(&conjuntos[i], 0); t >= 0;
             t = bitset_proximo(&conjuntos[i], t + 1))
            fprintf(f, "%s, ", g->terms[t].nome_c);
        fputs("-1 };\n", f);
    }
}

/* Escreve a condicao "o token corrente pertence a b":
 *   tok.tipo == T_A                     (um token)
 *   tok.tipo == T_A || tok.tipo == T_B  (ate MAX_COMPARACOES)
 *   pertence(tok.tipo, conj_N)          (mais tokens) */
static void emitir_teste(FILE *f, const Gramatica *g, const Bitset *b)
{
    int t, n = bitset_tamanho(b), primeiro = 1;
    if (n == 0) {
        fputs("0", f);
    } else if (n <= MAX_COMPARACOES) {
        for (t = bitset_proximo(b, 0); t >= 0; t = bitset_proximo(b, t + 1)) {
            fprintf(f, "%stok.tipo == %s", primeiro ? "" : " || ", g->terms[t].nome_c);
            primeiro = 0;
        }
    } else {
        fprintf(f, "pertence(tok.tipo, conj_%d)", registrar_conjunto(b));
    }
}

/* Linha "<antes><teste><depois>", ex.: emit_teste(em, "while (", g, b, ") {") */
static void emit_teste(Emissor *em, const char *antes, const Gramatica *g,
                       const Bitset *b, const char *depois)
{
    int i;
    for (i = 0; i < em->nivel; i++)
        fputs("    ", em->f);
    fputs(antes, em->f);
    emitir_teste(em->f, g, b);
    fputs(depois, em->f);
    fputc('\n', em->f);
}

/* Linha "erro_sintatico(conj_N);" com N = conjunto dos tokens esperados. */
static void emit_erro(Emissor *em, const Bitset *esperados)
{
    emit(em, "erro_sintatico(conj_%d);", registrar_conjunto(esperados));
}

/* ------------------------------------------------------------------ */
/* Traducao das expressoes (esquema da secao 4 do enunciado)          */
/* ------------------------------------------------------------------ */

static void gen_expr(Emissor *em, const Gramatica *g, const Expr *e)
{
    int i, primeiro = 1;
    const Expr *a = NULL, *alt_anulavel = NULL;

    switch (e->tipo) {
    case E_TERM:
        emit(em, "match(%s);", g->terms[e->simbolo].nome_c);
        break;

    case E_NAOTERM:
        emit(em, "parse_%s();", g->nts[e->simbolo].nome);
        break;

    case E_SEQ:
        for (i = 0; i < e->nfilhos; i++)
            gen_expr(em, g, e->filhos[i]);
        break;

    case E_ALT:
        for (i = 0; i < e->nfilhos; i++) {
            a = e->filhos[i];
            if (a->anulavel) {
                /* sem teste: vai para o else final */
                alt_anulavel = a;
                continue;
            }
            emit_teste(em, primeiro ? "if (" : "} else if (", g, &a->first, ") {");
            primeiro = 0;
            em->nivel++;
            gen_expr(em, g, a);
            em->nivel--;
        }
        emit(em, "} else {");
        em->nivel++;
        if (alt_anulavel != NULL)
            gen_expr(em, g, alt_anulavel);
        else
            emit_erro(em, &e->first);
        em->nivel--;
        emit(em, "}");
        break;

    case E_OPC:
        a = e->filhos[0];
        emit_teste(em, "if (", g, &a->first, ") {");
        em->nivel++;
        gen_expr(em, g, a);
        em->nivel--;
        emit(em, "}");
        break;

    case E_REP:
        a = e->filhos[0];
        emit_teste(em, "while (", g, &a->first, ") {");
        em->nivel++;
        gen_expr(em, g, a);
        em->nivel--;
        emit(em, "}");
        break;

    case E_VAZIO:
    default:
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Expressoes regulares de TOKENS (modo proprio)                      */
/* ------------------------------------------------------------------ */

/* ---- Passo 6, parte 1: analise da expressao regular em arvore ---- */

typedef enum {
    RX_VAZIO,     /* cadeia vazia */
    RX_CLASSE,    /* um caractere de um conjunto (bitmap de 256 bits) */
    RX_CONCAT,    /* esq seguido de dir */
    RX_ALT,       /* esq | dir */
    RX_ESTRELA,   /* esq* */
    RX_MAIS,      /* esq+ */
    RX_OPC        /* esq? */
} RxTipo;

typedef struct Rx {
    RxTipo tipo;
    unsigned char conj[32];      /* 256 bits: quais caracteres a classe aceita */
    struct Rx *esq, *dir;
} Rx;

typedef struct {
    const char *ini;   /* expressao completa, para a mensagem de erro */
    const char *p;     /* posicao atual */
    int erro;
} RxParser;

static void rx_bit_set(unsigned char *conj, int c)
{
    conj[c >> 3] = (unsigned char) (conj[c >> 3] | (1 << (c & 7)));
}

static Rx *rx_novo(RxTipo tipo)
{
    Rx *r = (Rx *) xcalloc(1, sizeof(Rx));
    r->tipo = tipo;
    return r;
}

static void rx_liberar(Rx *r)
{
    if (r == NULL) {
        return;
    }
    rx_liberar(r->esq);
    rx_liberar(r->dir);
    free(r);
}

static void rx_erro(RxParser *ps, const char *msg)
{
    if (!ps->erro)
        diag_erro(0, 0, "expressao regular invalida (%s): %s", ps->ini, msg);
    ps->erro = 1;
}

/* Traduz o caractere escapado: \n, \t, \r; os demais valem para si mesmos (\\, \/). */
static int rx_escape(int c)
{
    if (c == 'n')
        return '\n';
    if (c == 't')
        return '\t';
    if (c == 'r')
        return '\r';
    return c;
}

/* Le um caractere literal ou escapado e avanca a posicao. */
static int rx_le_char(RxParser *ps)
{
    int c = (unsigned char) *ps->p;

    if (c == '\\') {
        ps->p++;
        if (*ps->p == '\0') {
            rx_erro(ps, "barra no fim da expressao");
            return 0;
        }
        c = rx_escape((unsigned char) *ps->p);
    }
    ps->p++;
    return c;
}

/* Declaracoes antecipadas: a gramatica das regras e mutuamente recursiva. */
static Rx *rx_alt(RxParser *ps);

/* [ ... ] : intervalos a-z, escapes e negacao com ^ */
static Rx *rx_classe(RxParser *ps)
{
    Rx *r = rx_novo(RX_CLASSE);
    int neg = 0, a, b, c, i;

    ps->p++;                          /* pula '[' */
    if (*ps->p == '^') {
        neg = 1;
        ps->p++;
    }
    while (*ps->p != ']' && !ps->erro) {
        if (*ps->p == '\0') {
            rx_erro(ps, "']' esperado");
            break;
        }
        a = rx_le_char(ps);
        b = a;
        if (*ps->p == '-' && ps->p[1] != ']' && ps->p[1] != '\0') {
            ps->p++;
            b = rx_le_char(ps);
            if (b < a)
                rx_erro(ps, "intervalo invertido em classe");
        }
        for (c = a; c <= b; c++)
            rx_bit_set(r->conj, c);
    }
    if (ps->erro) {
        rx_liberar(r);
        return NULL;
    }
    ps->p++;                          /* pula ']' */
    if (neg)
        for (i = 0; i < 32; i++)
            r->conj[i] = (unsigned char) ~r->conj[i];
    return r;
}

/* atomo: (expr) | [classe] | . | \escape | caractere */
static Rx *rx_atomo(RxParser *ps)
{
    Rx *r;
    int c = (unsigned char) *ps->p;
    int i;

    if (c == '(') {
        ps->p++;
        r = rx_alt(ps);
        if (r == NULL)
            return NULL;
        if (*ps->p != ')') {
            rx_erro(ps, "')' esperado");
            rx_liberar(r);
            return NULL;
        }
        ps->p++;
        return r;
    }
    if (c == '[')
        return rx_classe(ps);
    if (c == '*' || c == '+' || c == '?') {
        rx_erro(ps, "operador sem expressao antes");
        return NULL;
    }

    r = rx_novo(RX_CLASSE);
    if (c == '.') {
        /* qualquer caractere, exceto a quebra de linha */
        for (i = 0; i < 256; i++)
            if (i != '\n')
                rx_bit_set(r->conj, i);
        ps->p++;
    } else {
        c = rx_le_char(ps);
        if (ps->erro) {
            rx_liberar(r);
            return NULL;
        }
        rx_bit_set(r->conj, c);
    }
    return r;
}

/* repeticao: atomo seguido de zero ou mais * + ? */
static Rx *rx_rep(RxParser *ps)
{
    Rx *r = rx_atomo(ps), *n;

    while (r != NULL && (*ps->p == '*' || *ps->p == '+' || *ps->p == '?')) {
        if (*ps->p == '*')
            n = rx_novo(RX_ESTRELA);
        else if (*ps->p == '+')
            n = rx_novo(RX_MAIS);
        else
            n = rx_novo(RX_OPC);
        n->esq = r;
        r = n;
        ps->p++;
    }
    return r;
}

/* concatenacao: repeticoes ate encontrar '|', ')' ou o fim */
static Rx *rx_cat(RxParser *ps)
{
    Rx *esq = NULL, *n, *c;

    while (*ps->p != '\0' && *ps->p != '|' && *ps->p != ')') {
        n = rx_rep(ps);
        if (n == NULL) {
            rx_liberar(esq);
            return NULL;
        }
        if (esq == NULL) {
            esq = n;
        } else {
            c = rx_novo(RX_CONCAT);
            c->esq = esq;
            c->dir = n;
            esq = c;
        }
    }
    if (esq == NULL)
        esq = rx_novo(RX_VAZIO);
    return esq;
}

/* alternativa: concatenacoes separadas por | */
static Rx *rx_alt(RxParser *ps)
{
    Rx *esq, *n;

    esq = rx_cat(ps);
    while (esq != NULL && *ps->p == '|') {
        ps->p++;
        n = rx_novo(RX_ALT);
        n->esq = esq;
        n->dir = rx_cat(ps);
        if (n->dir == NULL) {
            rx_liberar(n);
            return NULL;
        }
        esq = n;
    }
    return esq;
}

/* Ponto de entrada: devolve a arvore, ou NULL se a expressao for invalida. */
static Rx *rx_analisar(const char *regex)
{
    RxParser ps;
    Rx *r;

    ps.ini = regex;
    ps.p = regex;
    ps.erro = 0;
    r = rx_alt(&ps);
    if (r != NULL && *ps.p != '\0') {
        rx_erro(&ps, "')' sem '(' correspondente");
        rx_liberar(r);
        r = NULL;
    }
    return r;
}

/* ---- Passo 6, parte 2: AFN de Thompson e AFD por subconjuntos ---- */

#define NFA_CLASSE 0    /* consome um caractere de um conjunto */
#define NFA_EPS    1    /* transicao vazia (ate duas saidas) */
#define NFA_FIM    2    /* estado de aceitacao da expressao inteira */

typedef struct {
    int tipo;
    unsigned char conj[32];   /* caracteres aceitos (so em NFA_CLASSE) */
    int out1, out2;           /* proximos estados; -1 = nenhum */
} NfaEstado;

typedef struct {
    NfaEstado *est;
    int n, cap;
    int ini;
} Nfa;

typedef struct {
    Bitset *conjs;          /* conjunto de estados do AFN de cada estado do AFD */
    int *trans;             /* trans[e * 256 + c] = proximo estado, ou -1 */
    unsigned char *aceita;  /* aceita[e] = 1 se o estado e final */
    int n, cap;
} Dfa;

/* ---- AFN ---- */

static int nfa_novo(Nfa *a, int tipo)
{
    if (a->n == a->cap) {
        a->cap = a->cap ? 2 * a->cap : 16;
        a->est = (NfaEstado *) xrealloc(a->est, (size_t) a->cap * sizeof(NfaEstado));
    }
    a->est[a->n].tipo = tipo;
    memset(a->est[a->n].conj, 0, sizeof a->est[a->n].conj);
    a->est[a->n].out1 = -1;
    a->est[a->n].out2 = -1;
    return a->n++;
}

/* Construcao de Thompson: monta o fragmento de r e devolve seu inicio e seu fim. */
static void nfa_construir(Nfa *a, const Rx *r, int *ini, int *fim)
{
    int i1, f1, i2, f2, s, f, g;

    switch (r->tipo) {
    case RX_VAZIO:
        s = nfa_novo(a, NFA_EPS);
        f = nfa_novo(a, NFA_EPS);
        a->est[s].out1 = f;
        break;

    case RX_CLASSE:
        s = nfa_novo(a, NFA_CLASSE);
        memcpy(a->est[s].conj, r->conj, sizeof r->conj);
        f = nfa_novo(a, NFA_EPS);
        a->est[s].out1 = f;
        break;

    case RX_CONCAT:
        nfa_construir(a, r->esq, &i1, &f1);
        nfa_construir(a, r->dir, &i2, &f2);
        a->est[f1].out1 = i2;
        s = i1;
        f = f2;
        break;

    case RX_ALT:
        nfa_construir(a, r->esq, &i1, &f1);
        nfa_construir(a, r->dir, &i2, &f2);
        s = nfa_novo(a, NFA_EPS);
        f = nfa_novo(a, NFA_EPS);
        a->est[s].out1 = i1;
        a->est[s].out2 = i2;
        a->est[f1].out1 = f;
        a->est[f2].out1 = f;
        break;

    case RX_ESTRELA:
        nfa_construir(a, r->esq, &i1, &f1);
        s = nfa_novo(a, NFA_EPS);
        f = nfa_novo(a, NFA_EPS);
        a->est[s].out1 = i1;
        a->est[s].out2 = f;
        a->est[f1].out1 = s;
        break;

    case RX_MAIS:
        nfa_construir(a, r->esq, &i1, &f1);
        g = nfa_novo(a, NFA_EPS);   /* volta ao inicio ou sai */
        f = nfa_novo(a, NFA_EPS);
        a->est[g].out1 = i1;
        a->est[g].out2 = f;
        a->est[f1].out1 = g;
        s = i1;
        break;

    case RX_OPC:
        nfa_construir(a, r->esq, &i1, &f1);
        s = nfa_novo(a, NFA_EPS);
        f = nfa_novo(a, NFA_EPS);
        a->est[s].out1 = i1;
        a->est[s].out2 = f;
        a->est[f1].out1 = f;
        break;

    default:
        s = nfa_novo(a, NFA_EPS);
        f = nfa_novo(a, NFA_EPS);
        a->est[s].out1 = f;
        break;
    }
    *ini = s;
    *fim = f;
}

/* Monta o AFN inteiro: o fim da expressao vira o estado de aceitacao. */
static void nfa_montar(Nfa *a, const Rx *r)
{
    int i, f;

    a->est = NULL;
    a->n = 0;
    a->cap = 0;
    nfa_construir(a, r, &i, &f);
    a->est[f].tipo = NFA_FIM;
    a->ini = i;
}

static void nfa_liberar(Nfa *a)
{
    free(a->est);
    a->est = NULL;
    a->n = a->cap = 0;
}

/* Fecho-epsilon: acrescenta a s todos os estados alcancaveis por transicoes vazias. */
static void fecho_rec(const Nfa *a, Bitset *s, int e)
{
    if (e < 0 || bitset_tem(s, e))
        return;
    bitset_add(s, e);
    if (a->est[e].tipo == NFA_EPS) {
        fecho_rec(a, s, a->est[e].out1);
        fecho_rec(a, s, a->est[e].out2);
    }
}

/* Fecho-epsilon: acrescenta a s todos os estados alcancaveis por transicoes vazias.
 * Calcula em um conjunto auxiliar: testar s diretamente impediria a expansao. */
static void nfa_fecho(const Nfa *a, Bitset *s)
{
    Bitset res;
    int e;

    bitset_iniciar(&res, a->n);
    for (e = bitset_proximo(s, 0); e >= 0; e = bitset_proximo(s, e + 1))
        fecho_rec(a, &res, e);
    bitset_copiar(s, &res);
    bitset_liberar(&res);
}

/* Estados alcancados a partir de s consumindo o caractere c (antes do fecho). */
static void nfa_mover(const Nfa *a, const Bitset *s, int c, Bitset *dest)
{
    int e;
    for (e = bitset_proximo(s, 0); e >= 0; e = bitset_proximo(s, e + 1)) {
        const NfaEstado *x = &a->est[e];
        if (x->tipo == NFA_CLASSE && (x->conj[c >> 3] & (1 << (c & 7))))
            bitset_add(dest, x->out1);
    }
}

/* ---- AFD ---- */

static void dfa_liberar(Dfa *d)
{
    int i;
    for (i = 0; i < d->n; i++)
        bitset_liberar(&d->conjs[i]);
    free(d->conjs);
    free(d->trans);
    free(d->aceita);
    d->conjs = NULL;
    d->trans = NULL;
    d->aceita = NULL;
    d->n = d->cap = 0;
}

static int dfa_achar(const Dfa *d, const Bitset *s)
{
    int i;
    for (i = 0; i < d->n; i++)
        if (bitset_igual(&d->conjs[i], s))
            return i;
    return -1;
}

static int dfa_adicionar(Dfa *d, const Nfa *a, const Bitset *s)
{
    int i, fim = 0;

    if (d->n == d->cap) {
        d->cap = d->cap ? 2 * d->cap : 8;
        d->conjs = (Bitset *) xrealloc(d->conjs, (size_t) d->cap * sizeof(Bitset));
        d->aceita = (unsigned char *) xrealloc(d->aceita, (size_t) d->cap);
        d->trans = (int *) xrealloc(d->trans, (size_t) d->cap * 256 * sizeof(int));
    }
    bitset_iniciar(&d->conjs[d->n], a->n);
    bitset_copiar(&d->conjs[d->n], s);
    for (i = bitset_proximo(s, 0); i >= 0; i = bitset_proximo(s, i + 1))
        if (a->est[i].tipo == NFA_FIM)
            fim = 1;
    d->aceita[d->n] = (unsigned char) fim;
    for (i = 0; i < 256; i++)
        d->trans[d->n * 256 + i] = -1;
    return d->n++;
}

/* Construcao por subconjuntos: cada novo conjunto de estados do AFN vira um estado do AFD. */
static void dfa_construir(Dfa *d, const Nfa *a)
{
    Bitset inicial, dest;
    int atual, c, t;

    d->conjs = NULL;
    d->trans = NULL;
    d->aceita = NULL;
    d->n = d->cap = 0;

    bitset_iniciar(&inicial, a->n);
    bitset_add(&inicial, a->ini);
    nfa_fecho(a, &inicial);
    dfa_adicionar(d, a, &inicial);
    bitset_liberar(&inicial);

    bitset_iniciar(&dest, a->n);
    for (atual = 0; atual < d->n; atual++) {
        for (c = 0; c < 256; c++) {
            bitset_limpar(&dest);
            nfa_mover(a, &d->conjs[atual], c, &dest);
            if (bitset_vazio(&dest))
                continue;
            nfa_fecho(a, &dest);
            t = dfa_achar(d, &dest);
            if (t < 0)
                t = dfa_adicionar(d, a, &dest);
            d->trans[atual * 256 + c] = t;
        }
    }
    bitset_liberar(&dest);
}

/* Regex -> AFD. Retorna 0 em sucesso; 1 se a expressao for invalida
 * (a mensagem ja foi reportada por rx_analisar). */
static int regex_para_dfa(const char *regex, Dfa *d)
{
    Rx *r = rx_analisar(regex);
    Nfa a;

    if (r == NULL)
        return 1;
    nfa_montar(&a, r);
    dfa_construir(d, &a);
    nfa_liberar(&a);
    rx_liberar(r);
    return 0;
}

/* ---- Passo 6, parte 3: emissao do AFD como codigo C ---- */

/* Emite o teste de um intervalo de codigos. As bordas 0 e 255 viram
 * testes simples, para o GCC nao acusar comparacao sempre verdadeira:
 *   "c == 48", "c <= 57", "c >= 97", "1" (intervalo total) ou
 *   "(c >= 48 && c <= 57)". */
static void emitir_faixa(FILE *f, int lo, int hi)
{
    if (lo == hi)
        fprintf(f, "c == %d", lo);
    else if (lo == 0 && hi == 255)
        fputs("1", f);
    else if (lo == 0)
        fprintf(f, "c <= %d", hi);
    else if (hi == 255)
        fprintf(f, "c >= %d", lo);
    else
        fprintf(f, "(c >= %d && c <= %d)", lo, hi);
}

/* Emite a funcao "static size_t <nome>(const char *s)" a partir do AFD d.
 *
 * A funcao comeca no estado 0 e avanca enquanto houver transicao. Sempre
 * que o estado atual e final, guarda a posicao i como o maior prefixo
 * reconhecido ate agora. Ao falhar, devolve essa ultima posicao. */
static void emitir_afd(FILE *f, const char *nome, const Dfa *d)
{
    int e, c, lo, t, primeiro = 1, aceitos = 0;

    for (e = 0; e < d->n; e++)
        if (d->aceita[e])
            aceitos++;

    fprintf(f, "static size_t %s(const char *s)\n{\n", nome);
    fputs("    size_t i = 0, melhor = 0;\n", f);
    fputs("    int e = 0, nx;\n", f);
    fputs("    unsigned char c;\n\n", f);
    fputs("    for (;;) {\n", f);

    /* estados finais: registra a posicao atual como o maior prefixo ate aqui */
    if (aceitos > 0) {
        fputs("        if (", f);
        for (e = 0; e < d->n; e++) {
            if (d->aceita[e]) {
                if (!primeiro)
                    fputs(" || ", f);
                fprintf(f, "e == %d", e);
                primeiro = 0;
            }
        }
        fputs(") {\n            melhor = i;\n        }\n", f);
    }

    fputs("        c = (unsigned char) s[i];\n", f);
    fputs("        if (c == 0)\n            break;\n", f);
    fputs("        switch (e) {\n", f);

    for (e = 0; e < d->n; e++) {
        fprintf(f, "        case %d:\n            nx = -1;\n", e);
        c = 0;
        while (c < 256) {
            t = d->trans[e * 256 + c];
            if (t < 0) {
                c++;
                continue;
            }
            /* agrupa caracteres consecutivos que vao para o mesmo estado */
            lo = c;
            while (c + 1 < 256 && d->trans[e * 256 + c + 1] == t)
                c++;
            fputs("            if (", f);
            emitir_faixa(f, lo, c);
            fprintf(f, ") nx = %d;\n", t);
            c++;
        }
        fputs("            break;\n", f);
    }
    fputs("        default:\n            nx = -1;\n            break;\n", f);
    fputs("        }\n", f);
    fputs("        if (nx < 0)\n            break;\n", f);
    fputs("        e = nx;\n        i++;\n", f);
    fputs("    }\n", f);
    fputs("    return melhor;\n}\n\n", f);
}

/* Emite a funcao de reconhecimento de uma expressao regular de TOKENS.
 * Retorna 0 em sucesso; 1 se a expressao for invalida (ja reportada). */
static int gen_casamento_regex(FILE *f, const char *nome, const char *regex)
{
    Dfa d;

    if (regex_para_dfa(regex, &d) != 0)
        return 1;
    fprintf(f, "/* AFD com %d estados */\n", d.n);
    emitir_afd(f, nome, &d);
    dfa_liberar(&d);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Leitura do arquivo de tokens (opcao --tokens do analisador gerado) */
/* ------------------------------------------------------------------ */

/* Emite "static Token token_de_arquivo(void)", que le o proximo token
 * do arquivo aberto em arquivo_tokens, no formato gravado pelo lexer do
 * TP1 (livro-texto, secao 3.3.2):
 *
 *   Token: tipo = <numero>, lexema = '<texto>', linha = <numero>
 *
 * Ao chegar ao fim do arquivo devolve um token TOKEN_FIM. */
static void gen_token_de_arquivo(FILE *f, int microc)
{
    /* TODO(aluno): emitir a leitura real com fgets + sscanf (formato
     * acima). Linhas em branco sao ignoradas; uma linha malformada deve
     * ser reportada com o numero da linha do arquivo de tokens. */
    static const char *const linhas[] = {
        "static Token token_de_arquivo(void)",
        "{",
        "    Token t;",
        "    memset(&t, 0, sizeof t);",
        "    fprintf(stderr, \"opcao --tokens ainda nao implementada\\n\");",
        "    exit(EXIT_FAILURE);",
        "    return t;",
        "}",
        "",
        NULL
    };
    (void) microc;
    emit_linhas(f, linhas);
}

/* ------------------------------------------------------------------ */
/* Partes fixas do codigo gerado                                      */
/* ------------------------------------------------------------------ */

static const char *const rt_estado[] = {
    "/* ---- estado do analisador ---- */",
    "",
    "static Token tok;                  /* token corrente (lookahead) */",
    "static int imprimir_arvore = 0;    /* opcao --tree */",
    "static int nivel_arvore = 0;",
    "static FILE *arquivo_tokens = NULL; /* opcao --tokens */",
    "",
    "static Token token_de_arquivo(void);",
    "",
    NULL
};

static const char *const rt_primitivas[] = {
    "/* ---- primitivas ---- */",
    "",
    "int pertence(int tipo, const int *conjunto)",
    "{",
    "    for (; *conjunto != -1; conjunto++)",
    "        if (*conjunto == tipo)",
    "            return 1;",
    "    return 0;",
    "}",
    "",
    "/* Reporta o erro com os tokens esperados (lista terminada em -1) e encerra. */",
    "void erro_sintatico(const int *esperados)",
    "{",
    "    int i;",
    "    escrever_posicao();",
    "    if ((int) tok.tipo == TOKEN_FIM)",
    "        fprintf(stderr, \"encontrado fim de arquivo, \");",
    "    else",
    "        fprintf(stderr, \"encontrado '%s', \", tok.lexema);",
    "    if (esperados[0] != -1 && esperados[1] == -1) {",
    "        fprintf(stderr, \"esperado %s\\n\", nome_token(esperados[0]));",
    "    } else {",
    "        fprintf(stderr, \"esperado um de {\");",
    "        for (i = 0; esperados[i] != -1; i++)",
    "            fprintf(stderr, \"%s%s\", i > 0 ? \", \" : \"\", nome_token(esperados[i]));",
    "        fprintf(stderr, \"}\\n\");",
    "    }",
    "    exit(EXIT_FAILURE);",
    "}",
    "",
    "static void recuo(void)",
    "{",
    "    int i;",
    "    for (i = 0; i < nivel_arvore; i++)",
    "        fputs(\"  \", stdout);",
    "}",
    "",
    "void arvore_entra(const char *nao_terminal)",
    "{",
    "    if (imprimir_arvore) {",
    "        recuo();",
    "        printf(\"%s\\n\", nao_terminal);",
    "    }",
    "    nivel_arvore++;",
    "}",
    "",
    "void arvore_sai(void)",
    "{",
    "    nivel_arvore--;",
    "}",
    "",
    "/* Consome o token corrente se for do tipo esperado; senao, erro. */",
    "void match(int esperado)",
    "{",
    "    int lista[2];",
    "    if ((int) tok.tipo != esperado) {",
    "        lista[0] = esperado;",
    "        lista[1] = -1;",
    "        erro_sintatico(lista);",
    "    }",
    "    if (SUPORTE_ARVORE && imprimir_arvore) {",
    "        recuo();",
    "        if (nome_token(tok.tipo)[0] == '\\'')",
    "            printf(\"%s\\n\", nome_token(tok.tipo));",
    "        else",
    "            printf(\"%s '%s'\\n\", nome_token(tok.tipo), tok.lexema);",
    "    }",
    "    avancar();",
    "}",
    "",
    NULL
};

/* Lexico proprio: partes fixas antes e depois das funcoes de regex. */
static const char *const rt_lexico_estado[] = {
    "/* ---- analisador lexico gerado a partir de TOKENS ---- */",
    "",
    "static char *fonte = NULL;",
    "static size_t pos = 0;",
    "static int linha = 1, coluna = 1;",
    "",
    "static char *ler_tudo(FILE *f)",
    "{",
    "    size_t cap = 4096, n = 0, lidos;",
    "    char *buf = (char *) malloc(cap), *novo;",
    "    if (buf == NULL) {",
    "        fprintf(stderr, \"memoria insuficiente\\n\");",
    "        exit(EXIT_FAILURE);",
    "    }",
    "    while ((lidos = fread(buf + n, 1, cap - n - 1, f)) > 0) {",
    "        n += lidos;",
    "        if (cap - n - 1 == 0) {",
    "            cap *= 2;",
    "            novo = (char *) realloc(buf, cap);",
    "            if (novo == NULL) {",
    "                fprintf(stderr, \"memoria insuficiente\\n\");",
    "                exit(EXIT_FAILURE);",
    "            }",
    "            buf = novo;",
    "        }",
    "    }",
    "    buf[n] = '\\0';",
    "    return buf;",
    "}",
    "",
    "static void consumir(size_t n)",
    "{",
    "    for (; n > 0 && fonte[pos] != '\\0'; n--, pos++) {",
    "        if (fonte[pos] == '\\n') {",
    "            linha++;",
    "            coluna = 1;",
    "        } else {",
    "            coluna++;",
    "        }",
    "    }",
    "}",
    "",
    NULL
};

static void escrever_cabecalho_c(FILE *f, const OpcoesGeracao *op)
{
    fputs("/*\n * Analisador sintatico descendente recursivo gerado a partir de ", f);
    emitir_comentario(f, op->origem);
    fputs(".\n * Arquivo gerado automaticamente: nao edite.\n */\n", f);
}

/* ------------------------------------------------------------------ */
/* Arquivo .h                                                         */
/* ------------------------------------------------------------------ */

static int gerar_h(const Gramatica *g, const OpcoesGeracao *op)
{
    char *caminho = (char *) xmalloc(strlen(op->prefixo) + 3);
    FILE *f;
    int i;

    sprintf(caminho, "%s.h", op->prefixo);
    f = fopen(caminho, "w");
    if (f == NULL) {
        diag_erro(0, 0, "nao foi possivel criar %s", caminho);
        free(caminho);
        return -1;
    }
    escrever_cabecalho_c(f, op);
    fputs("#ifndef PARSER_GERADO_H\n#define PARSER_GERADO_H\n\n", f);
    if (op->microc) {
        fputs("#include \"scanner.h\"   /* scanner do TP1: Token, TokenType, proximo_token */\n\n", f);
        fputs("#define TOKEN_FIM END_OF_FILE\n\n", f);
    } else {
        fputs("typedef enum {\n", f);
        for (i = 0; i < g->nterms; i++) {
            fprintf(f, "    %s%s   /* ", g->terms[i].nome_c, i + 1 < g->nterms ? "," : "");
            emitir_comentario(f, g->terms[i].exibicao);
            fputs(" */\n", f);
        }
        fputs("} TipoToken;\n\n", f);
        fputs("typedef struct {\n    int tipo;\n    char lexema[100];\n"
              "    int linha;\n    int coluna;\n} Token;\n\n", f);
        fputs("#define TOKEN_FIM T_FIM\n\n", f);
    }
    fputs("const char *nome_token(int tipo);\n", f);
    fputs("int  pertence(int tipo, const int *conjunto);\n", f);
    fputs("void avancar(void);\n", f);
    fputs("void match(int esperado);\n", f);
    fputs("void erro_sintatico(const int *esperados);\n", f);
    fputs("void arvore_entra(const char *nao_terminal);\n", f);
    fputs("void arvore_sai(void);\n\n", f);
    for (i = 0; i < g->nnts; i++)
        if (g->nts[i].regra != NULL)
            fprintf(f, "void parse_%s(void);\n", g->nts[i].nome);
    fputs("\n#endif\n", f);
    fclose(f);
    free(caminho);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Arquivo .c                                                         */
/* ------------------------------------------------------------------ */

static void gerar_nome_token(FILE *f, const Gramatica *g)
{
    int i;
    fputs("const char *nome_token(int tipo)\n{\n    switch (tipo) {\n", f);
    for (i = 0; i < g->nterms; i++) {
        fprintf(f, "    case %s: return ", g->terms[i].nome_c);
        emitir_string_c(f, i == TERM_FIM ? "fim de arquivo" : g->terms[i].exibicao);
        fputs(";\n", f);
    }
    fputs("    default: return \"token desconhecido\";\n    }\n}\n\n", f);
}

static void gerar_posicao_e_avancar(FILE *f, int microc)
{
    fputs("static void escrever_posicao(void)\n{\n", f);
    if (microc)
        fputs("    fprintf(stderr, \"linha %d: \", tok.linha);\n", f);
    else
        fputs("    fprintf(stderr, \"linha %d, coluna %d: \", tok.linha, tok.coluna);\n", f);
    fputs("}\n\n", f);
    fputs("/* Le o proximo token para tok. */\nvoid avancar(void)\n{\n", f);
    fputs("    if (arquivo_tokens != NULL)\n        tok = token_de_arquivo();\n    else\n", f);
    if (microc) {
        fputs("        tok = proximo_token();\n", f);
        fputs("    if (tok.tipo == UNDEF) {\n", f);
        fputs("        fprintf(stderr, \"linha %d: erro lexico: %s\\n\", tok.linha, tok.lexema);\n", f);
        fputs("        exit(EXIT_FAILURE);\n    }\n", f);
    } else {
        fputs("        tok = proximo_token_gerado();\n", f);
    }
    fputs("}\n\n", f);
}

static int gerar_lexico(FILE *f, const Gramatica *g)
{
    int i, nlit = 0, erros = 0;
    char nome[300];

    emit_linhas(f, rt_lexico_estado);
    for (i = 0; i < g->nskips; i++) {
        sprintf(nome, "casa_skip_%d", i);
        fputs("/* SKIP: ", f);
        emitir_comentario(f, g->skips[i]);
        fputs(" */\n", f);
        if (gen_casamento_regex(f, nome, g->skips[i]) != 0)
            erros++;
    }
    for (i = 1; i < g->nterms; i++) {
        if (g->terms[i].literal) {
            nlit++;
            continue;
        }
        sprintf(nome, "casa_%.250s", g->terms[i].nome_c);
        fprintf(f, "/* %s: ", g->terms[i].nome);
        emitir_comentario(f, g->terms[i].regex ? g->terms[i].regex : "");
        fputs(" */\n", f);
        if (gen_casamento_regex(f, nome, g->terms[i].regex ? g->terms[i].regex : "")
            != 0)
            erros++;
    }

    /* literais: comparacao direta, com prioridade em caso de empate */
    fputs("static const char *const texto_literais[] = {\n", f);
    for (i = 1; i < g->nterms; i++)
        if (g->terms[i].literal) {
            fputs("    ", f);
            emitir_string_c(f, g->terms[i].nome);
            fputs(",\n", f);
        }
    fputs("    NULL\n};\n\nstatic const int tipo_literais[] = {\n", f);
    for (i = 1; i < g->nterms; i++)
        if (g->terms[i].literal)
            fprintf(f, "    %s,\n", g->terms[i].nome_c);
    fputs("    -1\n};\n\n", f);

    fputs("/* Reconhece o maior prefixo da entrada; em caso de empate vence\n"
          " * o literal (palavras reservadas tem prioridade sobre ID). */\n", f);
    fputs("static Token proximo_token_gerado(void)\n{\n", f);
    fputs("    Token t;\n    size_t n, melhor;\n    int i, tipo, pulou;\n\n", f);
    fputs("    do {\n        pulou = 0;\n", f);
    for (i = 0; i < g->nskips; i++)
        fprintf(f, "        if ((n = casa_skip_%d(fonte + pos)) > 0) {\n"
                   "            consumir(n);\n            pulou = 1;\n        }\n", i);
    fputs("    } while (pulou);\n\n", f);
    fputs("    memset(&t, 0, sizeof t);\n", f);
    fputs("    t.linha = linha;\n    t.coluna = coluna;\n", f);
    fputs("    if (fonte[pos] == '\\0') {\n        t.tipo = T_FIM;\n        return t;\n    }\n", f);
    fputs("    melhor = 0;\n    tipo = -1;\n", f);
    fputs("    for (i = 0; texto_literais[i] != NULL; i++) {\n", f);
    fputs("        n = strlen(texto_literais[i]);\n", f);
    fputs("        if (n > melhor && strncmp(fonte + pos, texto_literais[i], n) == 0) {\n", f);
    fputs("            melhor = n;\n            tipo = tipo_literais[i];\n        }\n    }\n", f);
    for (i = 1; i < g->nterms; i++)
        if (!g->terms[i].literal)
            fprintf(f, "    if ((n = casa_%s(fonte + pos)) > melhor) {\n"
                       "        melhor = n;\n        tipo = %s;\n    }\n",
                    g->terms[i].nome_c, g->terms[i].nome_c);
    fputs("    if (melhor == 0) {\n", f);
    fputs("        fprintf(stderr, \"linha %d, coluna %d: erro lexico: caractere "
          "invalido '%c'\\n\",\n                linha, coluna, fonte[pos]);\n", f);
    fputs("        exit(EXIT_FAILURE);\n    }\n", f);
    fputs("    n = melhor < sizeof t.lexema ? melhor : sizeof t.lexema - 1;\n", f);
    fputs("    memcpy(t.lexema, fonte + pos, n);\n    t.lexema[n] = '\\0';\n", f);
    fputs("    t.tipo = tipo;\n    consumir(melhor);\n    return t;\n}\n\n", f);
    (void) nlit;
    return erros;
}

static void gerar_procedimentos(FILE *f, const Gramatica *g, const OpcoesGeracao *op)
{
    int i;
    Emissor em;

    em.f = f;
    em.nivel = 0;
    fputs("/* ---- procedimentos (um por nao terminal) ---- */\n\n", f);
    for (i = 0; i < g->nnts; i++) {
        if (g->nts[i].regra == NULL)
            continue;
        fprintf(f, "/* %s = ", g->nts[i].nome);
        {
            /* a regra em EBNF, sem "* /" */
            FILE *tmp = tmpfile();
            int c;
            if (tmp != NULL) {
                char buf[2] = { 0, 0 };
                expr_imprimir(tmp, g, g->nts[i].regra);
                rewind(tmp);
                while ((c = fgetc(tmp)) != EOF) {
                    buf[0] = (char) c;
                    emitir_comentario(f, buf);
                }
                fclose(tmp);
            }
        }
        fputs(" ; */\n", f);
        fprintf(f, "void parse_%s(void)\n{\n", g->nts[i].nome);
        em.nivel = 1;
        if (op->arvore)
            emit(&em, "arvore_entra(\"%s\");", g->nts[i].nome);
        gen_expr(&em, g, g->nts[i].regra);
        if (op->arvore)
            emit(&em, "arvore_sai();");
        fputs("}\n\n", f);
    }
}

static void gerar_main(FILE *f, const Gramatica *g, const OpcoesGeracao *op)
{
    fputs("/* ---- programa principal ---- */\n\n", f);
    fputs("int main(int argc, char **argv)\n{\n", f);
    fputs("    int i, fim[2];\n    const char *arquivo = NULL, *tokens = NULL;\n", f);
    fputs("    FILE *entrada = stdin;\n\n", f);
    fputs("    for (i = 1; i < argc; i++) {\n", f);
    fputs("        if (strcmp(argv[i], \"--tree\") == 0) {\n", f);
    fputs("            imprimir_arvore = 1;\n", f);
    fputs("        } else if (strcmp(argv[i], \"--tokens\") == 0 && i + 1 < argc) {\n", f);
    fputs("            tokens = argv[++i];\n", f);
    fputs("        } else if (argv[i][0] == '-' && argv[i][1] != '\\0') {\n", f);
    fputs("            fprintf(stderr, \"uso: %s [--tree] [--tokens arquivo.tok | "
          "programa]\\n\", argv[0]);\n", f);
    fputs("            return 2;\n        } else {\n            arquivo = argv[i];\n"
          "        }\n    }\n", f);
    fputs("    if (imprimir_arvore && !SUPORTE_ARVORE)\n", f);
    fputs("        fprintf(stderr, \"aviso: gere o analisador com --tree para "
          "imprimir a arvore\\n\");\n\n", f);
    fputs("    if (tokens != NULL) {\n", f);
    fputs("        arquivo_tokens = fopen(tokens, \"r\");\n", f);
    fputs("        if (arquivo_tokens == NULL) {\n", f);
    fputs("            fprintf(stderr, \"nao foi possivel abrir %s\\n\", tokens);\n", f);
    fputs("            return 2;\n        }\n    } else {\n", f);
    fputs("        if (arquivo != NULL && (entrada = fopen(arquivo, \"r\")) == NULL) {\n", f);
    fputs("            fprintf(stderr, \"nao foi possivel abrir %s\\n\", arquivo);\n", f);
    fputs("            return 2;\n        }\n", f);
    if (op->microc)
        fputs("        scanner_iniciar(entrada);\n", f);
    else
        fputs("        fonte = ler_tudo(entrada);\n", f);
    fputs("    }\n\n", f);
    fputs("    avancar();\n", f);
    fprintf(f, "    parse_%s();\n", g->nts[g->inicial].nome);
    fputs("    if (tok.tipo != TOKEN_FIM) {\n", f);
    fputs("        fim[0] = TOKEN_FIM;\n        fim[1] = -1;\n", f);
    fputs("        erro_sintatico(fim);\n    }\n", f);
    fputs("    printf(\"entrada aceita\\n\");\n\n", f);
    fputs("    if (arquivo_tokens != NULL) {\n        fclose(arquivo_tokens);\n", f);
    fputs("    } else {\n", f);
    if (op->microc)
        fputs("        scanner_finalizar();\n", f);
    else
        fputs("        free(fonte);\n", f);
    fputs("        if (entrada != stdin)\n            fclose(entrada);\n    }\n", f);
    fputs("    return 0;\n}\n", f);
}

static const char *nome_base(const char *caminho)
{
    const char *b = strrchr(caminho, '/');
    return b != NULL ? b + 1 : caminho;
}

static int gerar_c(const Gramatica *g, const OpcoesGeracao *op)
{
    char *caminho = (char *) xmalloc(strlen(op->prefixo) + 3);
    FILE *f, *corpo;
    int c, erros = 0;

    /* Os procedimentos sao gerados primeiro num arquivo temporario porque
     * registram os conjuntos (conj_N), que precisam vir antes no .c. */
    corpo = tmpfile();
    if (corpo == NULL) {
        diag_erro(0, 0, "nao foi possivel criar arquivo temporario");
        free(caminho);
        return -1;
    }
    gerar_procedimentos(corpo, g, op);

    sprintf(caminho, "%s.c", op->prefixo);
    f = fopen(caminho, "w");
    if (f == NULL) {
        diag_erro(0, 0, "nao foi possivel criar %s", caminho);
        fclose(corpo);
        free(caminho);
        return -1;
    }
    escrever_cabecalho_c(f, op);
    fputs("#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n", f);
    fprintf(f, "#include \"%s.h\"\n\n", nome_base(op->prefixo));
    fprintf(f, "#define SUPORTE_ARVORE %d\n\n", op->arvore ? 1 : 0);
    fputs("/* ---- conjuntos de tokens ---- */\n\n", f);
    emitir_conjuntos(f, g);
    fputc('\n', f);
    emit_linhas(f, rt_estado);
    if (!op->microc)
        erros += gerar_lexico(f, g);
    gerar_nome_token(f, g);
    gerar_posicao_e_avancar(f, op->microc);
    emit_linhas(f, rt_primitivas);
    gen_token_de_arquivo(f, op->microc);
    rewind(corpo);
    while ((c = fgetc(corpo)) != EOF)
        fputc(c, f);
    fclose(corpo);
    gerar_main(f, g, op);
    fclose(f);
    free(caminho);
    return erros == 0 ? 0 : -1;
}

int codegen_gerar(Gramatica *g, const OpcoesGeracao *op)
{
    int r;
    nomear_terminais(g, op->microc);
    r = gerar_h(g, op);
    if (r == 0)
        r = gerar_c(g, op);
    liberar_conjuntos();
    return r;
}