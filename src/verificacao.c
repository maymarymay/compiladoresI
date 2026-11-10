 /*
 * verificacao.c - Verificacoes semanticas da gramatica (etapa 2).
 *
 * Verificacoes:
 *   - verif_indefinidos:       nao terminais usados mas nao definidos;
 *   - verif_terminais:         terminais nomeados sem declaracao;
 *   - verif_inalcancaveis      (aviso)
 *   - verif_improdutivos       (erro)
 *   - verif_recursao_esquerda  (erro; direta e indireta)
 *
 * A duplicidade de definicoes ja e detectada durante a leitura
 * (ebnf_parser.c).
 */
#include <stdlib.h>
#include <string.h>
#include "verificacao.h"
#include "microc_mapa.h"
#include "util.h"

/* ------------------------------------------------------------------ */
/* Fornecido                                                          */
/* ------------------------------------------------------------------ */

static int verif_indefinidos(Gramatica *g)
{
    int i, erros = 0;
    for (i = 0; i < g->nnts; i++) {
        if (g->nts[i].regra == NULL) {
            diag_erro(g->nts[i].linha, g->nts[i].coluna,
                      "nao terminal %s usado mas nao definido", g->nts[i].nome);
            erros++;
        }
    }
    return erros;
}

static int verif_terminais(Gramatica *g, int modo_microc)
{
    int i, erros = 0, carregado = 0;
    Terminal *t;

    if (modo_microc)
        carregado = microc_carregar_tokens_h(g->tokens_externos) == 0;

    for (i = 1; i < g->nterms; i++) {
        t = &g->terms[i];
        if (modo_microc) {
            if (t->literal && microc_nome_literal(t->nome) == NULL) {
                diag_erro(t->linha, t->coluna, "literal %s nao corresponde a nenhum "
                          "token de Micro C", t->exibicao);
                erros++;
            } else if (!t->literal && carregado && !microc_token_existe(t->nome)) {
                diag_erro(t->linha, t->coluna, "terminal %s nao existe na enumeracao "
                          "TokenType de %s", t->nome, g->tokens_externos);
                erros++;
            }
        } else if (!t->declarado) {
            diag_erro(t->linha, t->coluna, "terminal %s nao declarado em TOKENS", t->nome);
            erros++;
        }
    }
    return erros;
}

/* ------------------------------------------------------------------ */
/* A implementar                                                      */
/* ------------------------------------------------------------------ */

/* marca em marcas[] os nao terminais que aparecem em e; "mudou" mostra se alguma marca nova foi feita */
typedef struct {
    int *marcas;
    int mudou;
} Marcas;

static void marcar_nt(Expr *e, void *ctx)
{
    Marcas *m = (Marcas *) ctx;
    if (e->tipo == E_NAOTERM && !m->marcas[e->simbolo]) {
        m->marcas[e->simbolo] = 1;
        m->mudou = 1;
    }
}

/* aviso para cada nao terminal que nao pode ser alcancado a partir do simbolo inicial. */
static int verif_inalcancaveis(Gramatica *g)
{
    int i, mudou;
    int *alcancado = (int *) xcalloc((size_t) g->nnts, sizeof(int));
    Marcas m;

    alcancado[g->inicial] = 1;
    do {
        mudou = 0;
        for (i = 0; i < g->nnts; i++) {
            if (alcancado[i] && g->nts[i].regra != NULL) {
                m.marcas = alcancado;
                m.mudou = 0;
                expr_percorrer(g->nts[i].regra, marcar_nt, &m);
                mudou = mudou || m.mudou;
            }
        }
    } while (mudou);

    for (i = 0; i < g->nnts; i++)
        if (!alcancado[i])
            diag_aviso(g->nts[i].linha, g->nts[i].coluna,
                       "nao terminal %s sobra inalcancavel a partir de %s",
                       g->nts[i].nome, g->nts[g->inicial].nome);
    free(alcancado);
    return 0;
}

/* uma expressao eh produtiva se deriva alguma cadeia de terminais: terminais, E_VAZIO, [ .. ] e { .. } sempre sao; uma sequencia exige todos os filhos produtivos; uma alternativa exige ao menos um. */
static int produtiva(const Expr *e, const int *prod)
{
    int i;
    switch (e->tipo) {
    case E_TERM:
    case E_VAZIO:
    case E_OPC:
    case E_REP:
        return 1;
    case E_NAOTERM:
        return prod[e->simbolo];
    case E_SEQ:
        for (i = 0; i < e->nfilhos; i++)
            if (!produtiva(e->filhos[i], prod))
                return 0;
        return 1;
    case E_ALT:
        for (i = 0; i < e->nfilhos; i++)
            if (produtiva(e->filhos[i], prod))
                return 1;
        return 0;
    }
    return 0;
}

/* um erro para cada nao terminal definido que nao deriva nenhuma cadeia de terminais e retorna o numero de erros. */
static int verif_improdutivos(Gramatica *g)
{
    int i, mudou, erros = 0;
    int *prod = (int *) xcalloc((size_t) g->nnts, sizeof(int));

    do {
        mudou = 0;
        for (i = 0; i < g->nnts; i++) {
            if (!prod[i] && g->nts[i].regra != NULL && produtiva(g->nts[i].regra, prod)) {
                prod[i] = 1;
                mudou = 1;
            }
        }
    } while (mudou);

    for (i = 0; i < g->nnts; i++) {
        if (g->nts[i].regra != NULL && !prod[i]) {
            diag_erro(g->nts[i].linha_def, 0,
                      "nao terminal %s nao deriva nenhuma cadeia de terminais",
                      g->nts[i].nome);
            erros++;
        }
    }
    free(prod);
    return erros;
}

/* marca em esq[] os nao terminais que podem aparecer na primeira posicao de e*/
/* um filho de uma sequencia so deixa o proximo chegar a primeira posicao se for anulavel, o q cobre prefixos como [ x ] e { x }. */
static void primeiros_nts(const Expr *e, int *esq)
{
    int i;
    switch (e->tipo) {
    case E_NAOTERM:
        esq[e->simbolo] = 1;
        break;
    case E_SEQ:
        for (i = 0; i < e->nfilhos; i++) {
            primeiros_nts(e->filhos[i], esq);
            if (!e->filhos[i]->anulavel)
                break;
        }
        break;
    case E_ALT:
        for (i = 0; i < e->nfilhos; i++)
            primeiros_nts(e->filhos[i], esq);
        break;
    case E_OPC:
    case E_REP:
        primeiros_nts(e->filhos[0], esq);
        break;
    case E_TERM:
    case E_VAZIO:
        break;
    }
}

/* um erro pra cada nao terminal recursivo a esquerda: direta se ele mesmo aparece na primeira posicao da sua regra, indireta se chega a si mesmo por outros nao terminais*/ 
int verif_recursao_esquerda(Gramatica *g)
{
    int i, j, k, n = g->nnts, erros = 0;
    int *adj = (int *) xcalloc((size_t) n * n, sizeof(int));
    int *direta = (int *) xcalloc((size_t) n, sizeof(int));

    for (i = 0; i < n; i++)
        if (g->nts[i].regra != NULL)
            primeiros_nts(g->nts[i].regra, &adj[i * n]);
    for (i = 0; i < n; i++)
        direta[i] = adj[i * n + i];

    for (k = 0; k < n; k++)
        for (i = 0; i < n; i++)
            if (adj[i * n + k])
                for (j = 0; j < n; j++)
                    if (adj[k * n + j])
                        adj[i * n + j] = 1;

    for (i = 0; i < n; i++) {
        if (!adj[i * n + i])
            continue;
        diag_erro(g->nts[i].linha_def, 0, "nao terminal %s tem recursao a esquerda %s",
                  g->nts[i].nome, direta[i] ? "direta" : "indireta");
        erros++;
    }
    free(adj);
    free(direta);
    return erros;
}

int verif_basica(Gramatica *g, int modo_microc)
{
    int erros = 0;
    if (g->inicial < 0) {
        diag_erro(0, 0, "a gramatica nao tem nenhuma regra");
        return 1;
    }
    erros += verif_indefinidos(g);
    erros += verif_terminais(g, modo_microc);
    erros += verif_inalcancaveis(g);
    if (erros == 0)
        erros += verif_improdutivos(g);
    return erros;
}

}
