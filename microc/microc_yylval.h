#ifndef MICROC_YYLVAL_H
#define MICROC_YYLVAL_H

/* Valor semantico do token corrente (TP1, secao 4.3).
 * Compartilhado entre microc.flex e scanner.c. */
typedef struct {
    char *symbol;      /* lexema para ID, INTEGERCONST, CHARCONST, STRINGCONST */
    char *error_msg;   /* mensagem de erro, usada quando o token e UNDEF        */
    int symbol_len;    /* tamanho do lexema, podendo conter caractere nulo      */
} YYSTYPE;

extern YYSTYPE microc_yylval;

#endif
