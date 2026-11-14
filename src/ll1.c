/*
 * ll1.c - Verificacao da condicao LL(1) (etapa 4).
 *
 * Condicoes (secao 3 do enunciado):
 *   1. Em a1 | a2 | ... | an, os FIRST das alternativas sao disjuntos dois
 *      a dois e no maximo uma alternativa e anulavel.
 *   2. Se uma alternativa ai e anulavel, FOLLOW(alternativa) e disjunto
 *      dos FIRST das demais.
 *   3. Em [ a ] e { a }, a nao e anulavel e FIRST(a) e disjunto do
 *      FOLLOW da construcao.
 *
 * Violacoes de 1, 2 e da primeira parte de 3 sao ERROS. A violacao de
 * FIRST(a) inter FOLLOW nao vazio em [ a ] ou { a } e reportada como
 * AVISO e resolvida de forma gulosa: o analisador gerado entra na
 * construcao sempre que o token estiver em FIRST(a). E o caso classico
 * do "else pendente":  if = "if" "(" expr ")" cmd [ "else" cmd ] ;
 * em que o else fica com o if mais proximo, como em C.
 *
 * Fornecido: reportar (formata as mensagens).
 */
#include <stdarg.h>
#include <stdio.h>
#include "ll1.h"
#include "util.h"

/* Reporta um conflito na regra do nao terminal nt, na posicao de e.
 * comum (pode ser NULL) e o conjunto de tokens em conflito. */
static void reportar(const Gramatica *g, int nt, const Expr *e, int eh_aviso,
                     const Bitset *comum, const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "%s:%d:%d: %s: conflito LL(1) em %s: ", g->arquivo, e->linha,
            e->coluna, eh_aviso ? "aviso" : "erro", g->nts[nt].nome);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    if (comum != NULL) {
        fprintf(stderr, " ");
        gram_imprimir_conjunto(stderr, g, comum);
    }
    fputc('\n', stderr);
}

/* TODO(aluno): percorrer as regras e, para cada no E_ALT, E_OPC e E_REP,
 * verificar as condicoes do cabecalho. Use bitset_intersecao/bitset_vazio
 * e reporte com reportar(...), por exemplo:
 *
 *   reportar(g, nt, e, 0, &comum, "alternativas %d e %d comecam com", i+1, j+1);
 *
 * que imprime:
 *   expr.ebnf:7:10: erro: conflito LL(1) em stmt: alternativas 1 e 2 comecam com {ID}
 *
 * Retorne o numero de ERROS (avisos nao contam). */
/* Verifica uma expressao da regra do nao terminal nt. Conta os erros em *erros. */
static void verificar_expr(const Gramatica *g, int nt, const Expr *e, int *erros)
{
    int i, j;
    Bitset comum;
    const Expr *a, *b;

    for (i = 0; i < e->nfilhos; i++)
        verificar_expr(g, nt, e->filhos[i], erros);

    if (e->tipo == E_ALT) {
        for (i = 0; i < e->nfilhos; i++) {
            a = e->filhos[i];
            for (j = i + 1; j < e->nfilhos; j++) {
                b = e->filhos[j];

                /* Condicao 1: FIRST das alternativas disjuntos */
                bitset_iniciar(&comum, g->nterms);
                bitset_intersecao(&comum, &a->first, &b->first);
                if (!bitset_vazio(&comum)) {
                    reportar(g, nt, e, 0, &comum,
                             "alternativas %d e %d comecam com", i + 1, j + 1);
                    (*erros)++;
                }
                bitset_liberar(&comum);

                /* Condicao 1: no maximo uma alternativa anulavel */
                if (a->anulavel && b->anulavel) {
                    reportar(g, nt, e, 0, NULL,
                             "alternativas %d e %d sao ambas anulaveis", i + 1, j + 1);
                    (*erros)++;
                    continue;
                }

                /* Condicao 2: se ai e anulavel, FOLLOW(ai) e disjunto de FIRST(aj) */
                if (a->anulavel) {
                    bitset_iniciar(&comum, g->nterms);
                    bitset_intersecao(&comum, &a->follow, &b->first);
                    if (!bitset_vazio(&comum)) {
                        reportar(g, nt, e, 0, &comum,
                                 "alternativa %d e anulavel e o FOLLOW colide com o FIRST da alternativa %d",
                                 i + 1, j + 1);
                        (*erros)++;
                    }
                    bitset_liberar(&comum);
                }
                if (b->anulavel) {
                    bitset_iniciar(&comum, g->nterms);
                    bitset_intersecao(&comum, &b->follow, &a->first);
                    if (!bitset_vazio(&comum)) {
                        reportar(g, nt, e, 0, &comum,
                                 "alternativa %d e anulavel e o FOLLOW colide com o FIRST da alternativa %d",
                                 j + 1, i + 1);
                        (*erros)++;
                    }
                    bitset_liberar(&comum);
                }
            }
        }
    } else if (e->tipo == E_OPC || e->tipo == E_REP) {
        const char *op = (e->tipo == E_OPC) ? "[ ]" : "{ }";
        a = e->filhos[0];

        /* Condicao 3a: o conteudo nao pode ser anulavel (erro) */
        if (a->anulavel) {
            reportar(g, nt, e, 0, NULL, "o conteudo de %s e anulavel", op);
            (*erros)++;
        } else {
            /* Condicao 3b: FIRST(a) x FOLLOW da construcao (aviso, resolvido de forma gulosa) */
            bitset_iniciar(&comum, g->nterms);
            bitset_intersecao(&comum, &a->first, &e->follow);
            if (!bitset_vazio(&comum))
                reportar(g, nt, e, 1, &comum,
                         "FIRST e FOLLOW de %s se intersectam (resolvido de forma gulosa) em", op);
            bitset_liberar(&comum);
        }
    }
}

int ll1_verificar(const Gramatica *g)
{
    int i, erros = 0;

    for (i = 0; i < g->nnts; i++)
        if (g->nts[i].regra != NULL)
            verificar_expr(g, i, g->nts[i].regra, &erros);
    return erros;
}