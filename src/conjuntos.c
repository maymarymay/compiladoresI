/*
 * conjuntos.c - Calculo de anulavel, FIRST e FOLLOW (etapa 3).
 *
 * Fornecido: conj_iniciar e conj_imprimir.
 *
 */
#include <stdio.h>
#include <string.h>
#include "conjuntos.h"
#include "util.h"

/* ------------------------------------------------------------------ */
/* Fornecido                                                          */
/* ------------------------------------------------------------------ */

static void iniciar_expr(Expr *e, void *ctx)
{
    int n = *(int *) ctx;
    bitset_liberar(&e->first);
    bitset_liberar(&e->follow);
    bitset_iniciar(&e->first, n);
    bitset_iniciar(&e->follow, n);
    e->anulavel = 0;
}

void conj_iniciar(Gramatica *g)
{
    int i, n = g->nterms;
    for (i = 0; i < g->nnts; i++) {
        bitset_liberar(&g->nts[i].first);
        bitset_liberar(&g->nts[i].follow);
        bitset_iniciar(&g->nts[i].first, n);
        bitset_iniciar(&g->nts[i].follow, n);
        g->nts[i].anulavel = 0;
        expr_percorrer(g->nts[i].regra, iniciar_expr, &n);
    }
}

void conj_imprimir(FILE *f, const Gramatica *g)
{
    int i;
    size_t largura = strlen("NAO TERMINAL");
    for (i = 0; i < g->nnts; i++)
        if (strlen(g->nts[i].nome) > largura)
            largura = strlen(g->nts[i].nome);
    fprintf(f, "%-*s  ANULAVEL  FIRST / FOLLOW\n", (int) largura, "NAO TERMINAL");
    for (i = 0; i < g->nnts; i++) {
        fprintf(f, "%-*s  %-8s  FIRST  = ", (int) largura, g->nts[i].nome,
                g->nts[i].anulavel ? "sim" : "nao");
        gram_imprimir_conjunto(f, g, &g->nts[i].first);
        fprintf(f, "\n%-*s            FOLLOW = ", (int) largura, "");
        gram_imprimir_conjunto(f, g, &g->nts[i].follow);
        fputc('\n', f);
    }
}

/* ------------------------------------------------------------------ */
/* A implementar                                                      */
/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */
/* Anulavel                                                           */
/* ------------------------------------------------------------------ */

static int anulavel_expr(Gramatica *g, Expr *e, int *mudou)
{
    int i, r, f;

    switch (e->tipo) {
    case E_TERM:
        r = 0;
        break;
    case E_NAOTERM:
        r = g->nts[e->simbolo].anulavel;
        break;
    case E_SEQ:
        r = 1;
        for (i = 0; i < e->nfilhos; i++) {
            f = anulavel_expr(g, e->filhos[i], mudou);
            r = r && f;
        }
        break;
    case E_ALT:
        r = 0;
        for (i = 0; i < e->nfilhos; i++) {
            f = anulavel_expr(g, e->filhos[i], mudou);
            r = r || f;
        }
        break;
    case E_OPC:
    case E_REP:
        anulavel_expr(g, e->filhos[0], mudou);
        r = 1;
        break;
    default: /* E_VAZIO */
        r = 1;
        break;
    }
    if (r != e->anulavel) {
        e->anulavel = r;
        *mudou = 1;
    }
    return r;
}

void conj_anulavel(Gramatica *g)
{
    int i, r, mudou;

    do {
        mudou = 0;
        for (i = 0; i < g->nnts; i++) {
            if (g->nts[i].regra == NULL)
                continue;
            r = anulavel_expr(g, g->nts[i].regra, &mudou);
            if (r != g->nts[i].anulavel) {
                g->nts[i].anulavel = r;
                mudou = 1;
            }
        }
    } while (mudou);
}

/* ------------------------------------------------------------------ */
/* FIRST                                                              */
/* ------------------------------------------------------------------ */

static void first_expr(Gramatica *g, Expr *e, int *mudou)
{
    int i, parar;

    switch (e->tipo) {
    case E_TERM:
        if (!bitset_tem(&e->first, e->simbolo)) {
            bitset_add(&e->first, e->simbolo);
            *mudou = 1;
        }
        break;
    case E_NAOTERM:
        *mudou |= bitset_unir(&e->first, &g->nts[e->simbolo].first);
        break;
    case E_SEQ:
        parar = 0;
        for (i = 0; i < e->nfilhos; i++) {
            first_expr(g, e->filhos[i], mudou);
            if (!parar) {
                *mudou |= bitset_unir(&e->first, &e->filhos[i]->first);
                if (!e->filhos[i]->anulavel)
                    parar = 1;
            }
        }
        break;
    case E_ALT:
    case E_OPC:
    case E_REP:
        for (i = 0; i < e->nfilhos; i++) {
            first_expr(g, e->filhos[i], mudou);
            *mudou |= bitset_unir(&e->first, &e->filhos[i]->first);
        }
        break;
    default: /* E_VAZIO: FIRST vazio */
        break;
    }
}

void conj_first(Gramatica *g)
{
    int i, mudou;

    do {
        mudou = 0;
        for (i = 0; i < g->nnts; i++) {
            if (g->nts[i].regra == NULL)
                continue;
            first_expr(g, g->nts[i].regra, &mudou);
            mudou |= bitset_unir(&g->nts[i].first, &g->nts[i].regra->first);
        }
    } while (mudou);
}

/* ------------------------------------------------------------------ */
/* FOLLOW                                                             */
/* ------------------------------------------------------------------ */

static void follow_expr(Gramatica *g, Expr *e, int *mudou)
{
    int i, j, nulo;
    Bitset suf;
    Expr *f;

    switch (e->tipo) {
    case E_NAOTERM:
        *mudou |= bitset_unir(&g->nts[e->simbolo].follow, &e->follow);
        break;

    case E_SEQ:
        bitset_iniciar(&suf, g->nterms);
        for (i = 0; i < e->nfilhos; i++) {
            /* FIRST do sufixo filhos[i+1..] e se ele e anulavel */
            bitset_limpar(&suf);
            nulo = 1;
            for (j = i + 1; j < e->nfilhos; j++) {
                bitset_unir(&suf, &e->filhos[j]->first);
                if (!e->filhos[j]->anulavel) {
                    nulo = 0;
                    break;
                }
            }
            *mudou |= bitset_unir(&e->filhos[i]->follow, &suf);
            if (nulo)
                *mudou |= bitset_unir(&e->filhos[i]->follow, &e->follow);
            follow_expr(g, e->filhos[i], mudou);
        }
        bitset_liberar(&suf);
        break;

    case E_ALT:
    case E_OPC:
    case E_REP:
        for (i = 0; i < e->nfilhos; i++) {
            f = e->filhos[i];
            *mudou |= bitset_unir(&f->follow, &e->follow);
            if (e->tipo == E_REP)
                *mudou |= bitset_unir(&f->follow, &f->first);
            follow_expr(g, f, mudou);
        }
        break;

    default: /* E_TERM, E_VAZIO: nada a propagar */
        break;
    }
}

void conj_follow(Gramatica *g)
{
    int i, mudou;

    if (g->inicial >= 0)
        bitset_add(&g->nts[g->inicial].follow, TERM_FIM);

    do {
        mudou = 0;
        for (i = 0; i < g->nnts; i++) {
            if (g->nts[i].regra == NULL)
                continue;
            mudou |= bitset_unir(&g->nts[i].regra->follow, &g->nts[i].follow);
            follow_expr(g, g->nts[i].regra, &mudou);
        }
    } while (mudou);
}



