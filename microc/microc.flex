/*
 * microc.flex
 *
 * Analisador lexico (scanner) para a linguagem Micro C.
 * Disciplina: Compiladores I - FACOM
 *
 * Compilacao:
 *      flex microc.flex
 */
%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Vocabulario de tokens: TokenType vem do tokens.h (nao redefina aqui). */
#include "tokens.h"

/* Valor semantico do token corrente. */
typedef struct {
    char *symbol;      /* lexema para ID, INTEGERCONST, CHARCONST, STRINGCONST */
    char *error_msg;   /* mensagem de erro, usada quando o token e UNDEF        */
    int symbol_len;    /* tamanho do lexema, podendo conter caractere nulo      */
} YYSTYPE;

YYSTYPE microc_yylval;

/* Linha atual do arquivo-fonte. Incrementada a cada quebra de linha
 * consumida (em codigo, comentarios ou strings). */
int linha_atual = 1;

/* Ultimo token reconhecido, usado para decidir entre inteiro negativo
 * e operador de subtracao. */
int ultimo_token;

/* Copia o texto reconhecido (yytext) para microc_yylval.symbol. */
static void guarda_lexema(void) {
    microc_yylval.symbol = malloc(yyleng + 1);
    memcpy(microc_yylval.symbol, yytext, yyleng);
    microc_yylval.symbol[yyleng] = '\0';
    microc_yylval.symbol_len = yyleng;
}
%}

/* -----------------------------------------------------------------------
 * 2. SECAO DE DEFINICOES
 * ------------------------------------------------------------------- */
%option prefix="microc_yy"
%option noyywrap

DIGIT       [0-9]
LETRA       [a-zA-Z_]
ALFANUM     [a-zA-Z0-9_]
%x COMMENT

%%

 /* -----------------------------------------------------------------------
  * 3. SECAO DE REGRAS
  * --------------------------------------------------------------------- */

 /* Fim de arquivo: retorna END_OF_FILE explicitamente, pois UNDEF tambem
  * tem valor 0 no enum e nao podemos depender do retorno automatico. */
<INITIAL><<EOF>>    { return END_OF_FILE; }

 /* --- Espacos em branco e quebras de linha ---------------------------- */
\n                  { linha_atual++; }
[ \t\r]+            { /* ignora espacos em branco */ }

 /* --- Comentarios ------------------------------------------------------ */
"//".*              { /* comentario de linha: ignora ate o fim da linha */ }
"/*"                { BEGIN(COMMENT); }
<COMMENT>"*/"       { BEGIN(INITIAL); }
<COMMENT>\n         { linha_atual++; }
<COMMENT><<EOF>>    {
                        microc_yylval.error_msg = "EOF em comentario";
                        BEGIN(INITIAL);
                        return UNDEF;
                    }
<COMMENT>.          { /* consome qualquer outro caractere dentro do comentario */ }

 /* Fechamento de comentario sem abertura correspondente. */
"*/"                {
                        microc_yylval.error_msg = "Comentario nao iniciado";
                        return UNDEF;
                    }

 /* --- Palavras reservadas e identificadores ---------------------------- */
{LETRA}{ALFANUM}*   {
                        if (strcmp(yytext, "if") == 0) {
                            ultimo_token = IF;
                            return IF;
                        } else if (strcmp(yytext, "main") == 0) {
                            ultimo_token = MAIN;
                            return MAIN;
                        } else if (strcmp(yytext, "else") == 0) {
                            ultimo_token = ELSE;
                            return ELSE;
                        } else if (strcmp(yytext, "for") == 0) {
                            ultimo_token = FOR;
                            return FOR;
                        } else if (strcmp(yytext, "return") == 0) {
                            ultimo_token = RETURN;
                            return RETURN;
                        } else if (strcmp(yytext, "int") == 0) {
                            ultimo_token = INT;
                            return INT;
                        } else if (strcmp(yytext, "char") == 0) {
                            ultimo_token = CHAR;
                            return CHAR;
                        } else if (strcmp(yytext, "print") == 0) {
                            ultimo_token = PRINT;
                            return PRINT;
                        }
                        guarda_lexema();
                        ultimo_token = ID;
                        return ID;
                    }

 /* --- Constantes inteiras ----------------------------------------------
  * Um '-' seguido de digitos e numero negativo, exceto quando vem depois
  * de um valor: entao e o operador de subtracao. */
"-"{DIGIT}+         {
                        if (ultimo_token == INTEGERCONST || ultimo_token == ID ||
                            ultimo_token == RPAREN || ultimo_token == RBRACKET) {
                            /* vem depois de um valor: e subtracao.
                             * Devolve os digitos ao fluxo de entrada. */
                            yyless(1);
                            ultimo_token = MINUS;
                            return MINUS;
                        } else {
                            /* nao vem depois de um valor: numero negativo */
                            guarda_lexema();
                            ultimo_token = INTEGERCONST;
                            return INTEGERCONST;
                        }
                    }

{DIGIT}+            {
                        guarda_lexema();
                        ultimo_token = INTEGERCONST;
                        return INTEGERCONST;
                    }

 /* --- Constantes de caractere ------------------------------------------ */
'([^'\n\\]|\\.)'    {
                        char c;
                        if (yytext[1] == '\\') {
                            if (yytext[2] == 'n')
                                c = '\n';
                            else if (yytext[2] == 't')
                                c = '\t';
                            else if (yytext[2] == '\\')
                                c = '\\';
                            else if (yytext[2] == '\'')
                                c = '\'';
                            else if (yytext[2] == '0')
                                c = '\0';
                            else
                                c = yytext[2];
                        } else {
                            c = yytext[1];
                        }
                        microc_yylval.symbol = malloc(1);
                        memcpy(microc_yylval.symbol, &c, 1);
                        microc_yylval.symbol_len = 1;
                        ultimo_token = CHARCONST;
                        return CHARCONST;
                    }

'[^'\n]*            {
                        microc_yylval.error_msg = "Constante de caractere nao terminada";
                        return UNDEF;
                    }

 /* --- Constantes de string --------------------------------------------- */
\"([^"\n\\]|\\.)*\0([^"\n\\]|\\.)*\" {
                        microc_yylval.error_msg = "String contem caractere nulo";
                        return UNDEF;
                    }

\"([^"\n\\]|\\.)*\"  {
                        int tamanho = yyleng - 2;
                        int tamanho_saida = 0;
                        int i;
                        microc_yylval.symbol = malloc(tamanho + 1);
                        for (i = 1; i < yyleng - 1; i++) {
                            if (yytext[i] == '\\') {
                                i++;
                                if (yytext[i] == 'n')
                                    microc_yylval.symbol[tamanho_saida++] = '\n';
                                else if (yytext[i] == 't')
                                    microc_yylval.symbol[tamanho_saida++] = '\t';
                                else if (yytext[i] == '\\')
                                    microc_yylval.symbol[tamanho_saida++] = '\\';
                                else if (yytext[i] == '"')
                                    microc_yylval.symbol[tamanho_saida++] = '"';
                                else if (yytext[i] == '0')
                                    microc_yylval.symbol[tamanho_saida++] = '\0';
                                else
                                    microc_yylval.symbol[tamanho_saida++] = yytext[i];
                            } else {
                                microc_yylval.symbol[tamanho_saida++] = yytext[i];
                            }
                        }
                        microc_yylval.symbol_len = tamanho_saida;
                        microc_yylval.symbol[tamanho_saida] = '\0';
                        ultimo_token = STRINGCONST;
                        return STRINGCONST;
                    }

\"([^"\n\\]|\\.)*\n {
                        yyless(yyleng - 1);
                        microc_yylval.error_msg = "String nao terminada";
                        return UNDEF;
}
\"([^"\n\\]|\\.)*   {
                        microc_yylval.error_msg = "EOF em string";
                        return UNDEF;
                    }

 /* --- Operadores relacionais e logicos --------------------------------- */
"=="                { ultimo_token = EQ;  return EQ; }
"="                 { ultimo_token = ASSIGN; return ASSIGN; }
"!="                { ultimo_token = NEQ; return NEQ; }
"!"                 { ultimo_token = NOT; return NOT; }
"<="                { ultimo_token = LEQ; return LEQ; }
"<"                 { ultimo_token = LT;  return LT; }
">="                { ultimo_token = GEQ; return GEQ; }
">"                 { ultimo_token = GT;  return GT; }
"&&"                { ultimo_token = AND; return AND; }
"||"                { ultimo_token = OR;  return OR; }

 /* --- Operadores aritmeticos e pontuacao ------------------------------- */
"+"                 { ultimo_token = PLUS;     return PLUS; }
"-"                 { ultimo_token = MINUS;    return MINUS; }
"*"                 { ultimo_token = MUL;      return MUL; }
"/"                 { ultimo_token = DIV;      return DIV; }
"%"                 { ultimo_token = MOD;      return MOD; }
";"                 { ultimo_token = SEMICOLON; return SEMICOLON; }
","                 { ultimo_token = COMMA;    return COMMA; }
"("                 { ultimo_token = LPAREN;   return LPAREN; }
")"                 { ultimo_token = RPAREN;   return RPAREN; }
"{"                 { ultimo_token = LBRACE;   return LBRACE; }
"}"                 { ultimo_token = RBRACE;   return RBRACE; }
"["                 { ultimo_token = LBRACKET; return LBRACKET; }
"]"                 { ultimo_token = RBRACKET; return RBRACKET; }

 /* --- Caractere invalido ----------------------------------------------
  * Deve ser SEMPRE a ultima regra do arquivo. */
.                   {
                        microc_yylval.error_msg = strdup(yytext);
                        return UNDEF;
                    }

%%