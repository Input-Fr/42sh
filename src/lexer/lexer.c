#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include "token.h"
#include <string.h>

struct lexer *lexer_new(const char *input)
{
    struct lexer *lex = malloc(sizeof(struct lexer));
    lex->input = input;
    lex->pos = 0;
    return lex;
}

void lexer_free(struct lexer *lexer)
{
    free(lexer);
}

size_t token_reco(struct lexer *lexer)
{
    while (lexer->input[lexer->pos] == ' ')
    {
        lexer->pos++;
    }
    size_t index = lexer->pos;
    short isQuoting = 0;

    while (1)
    {
        if (lexer->input[index] == 0)
        {
            return index - 1;
        }
        else if ((lexer->input[index] == '\\' || lexer->input[index] == '\'') && !isQuoting)
        {
            isQuoting = isQuoting != 0 ? 0 : 1;
            index++;
        }
        else if (!isQuoting && lexer->input[index] == '\n')
        {
            return index - 1;
        }
        else if (!isQuoting && lexer->input[index] == ' ')
        {
            return index - 1;
        }
        else if (!isQuoting && lexer->input[index] == ';')
        {
            return index - 1;
        }
        else if (index > 0 && index - 1 >= lexer->pos)
        {
            index++;
        }
        else
        {
            index++;
        }
    }
}

struct token token(struct lexer *lexer)
{
    if (test_if(lexer))
    {
        return token_if();
    }
    else if (test_fi(lexer))
    {
        return token_fi();
    }
    else if (test_elif(lexer))
    {
        return token_elif();
    }
    else if (test_else(lexer))
    {
        return token_else();
    }
    else if (test_then(lexer))
    {
        return token_then();
    }
    else if (test_EOF(lexer))
    {
        return token_EOF();
    }
    else
    {
        errx(1, "Syntax error");
    }

}

struct token lexer_next_token(struct lexer *lexer)
{
    struct token tok;
    size_t end_of_token = token_reco(lexer);
    tok = token(lexer);
    lexer->pos = end_of_token + 1;
    return tok;
}

struct token lexer_peek(struct lexer *lexer)
{
    size_t tmp = lexer->pos;
    struct token tok = lexer_next_token(lexer);
    lexer->pos = tmp;
    return tok;
}

struct token lexer_pop(struct lexer *lexer)
{
    struct token tok = lexer_next_token(lexer);
    return tok;
}
