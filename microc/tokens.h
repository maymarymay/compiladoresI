#ifndef TOKENS_H
#define TOKENS_H

typedef enum {
    END_OF_FILE, UNDEF,
    MAIN, INT, CHAR, IF, ELSE, FOR, RETURN, PRINT,
    ID, INTEGERCONST, CHARCONST, STRINGCONST,
    LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET, SEMICOLON, COMMA,
    ASSIGN, EQ, NEQ, LT, GT, LEQ, GEQ, AND, OR, NOT,
    PLUS, MINUS, MUL, DIV, MOD
} TokenType;

#define MAX_LEXEMA 256

typedef struct {
    TokenType tipo;
    char lexema[MAX_LEXEMA];
    int linha;
} Token;

#endif /* TOKENS_H */