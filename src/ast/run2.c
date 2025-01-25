#include <assert.h>
#include <err.h>
#include <stdlib.h>

#include "ast.h"
#include "hash_map/hash_map.h"

#define RUN(AST, HASH_TABLE) (*(AST)->ftable->run)((AST), (HASH_TABLE))
#define RUN_LIST(ASTS, ELT, HASH_TABLE)                                        \
    handle_list_ast((ASTS), (&ELT), HASH_TABLE) // évalue une liste d'ast
static int handle_list_ast(struct ast **asts, size_t *nbr_element,
                           struct hash_map *h)
{
    int j = 0;
    for (size_t i = 0; i < *nbr_element; i++)
    {
        j = RUN(asts[i], h);
    }
    return j;
}

char **expand_all(char **word, size_t nbr, struct hash_map *h)
{
    char **new_word = calloc(nbr, sizeof(char *));
    if (!new_word)
    {
        return NULL;
    }
    for (size_t i = 0; i < nbr; i++)
    {
        if (test_var(word[i]) || test_quote(word[i]))
        {
            new_word[i] = expand(h, word[i]);
        }
        else
        {
            new_word[i] = word[i];
        }
    }
    return new_word;
}

int for_run(struct ast *ast, struct hash_map *h)
{
    assert(ast && ast->type == AST_FOR);
    struct ast_for *boucle = (struct ast_for *)ast;
    int res = 0;
    if (boucle->nbr_elt)
    {
        for (size_t i = 0; i < boucle->nbr_elt; i++)
        {
	    hash_map_insert(h, boucle->variable, expand(h,boucle->list[i]));
            RUN(boucle->do_body, h);
        }
	hash_map_remove(h, boucle->variable);
    }
    return res;
}

int function_run(struct ast *ast, struct hash_map *h)
{
    assert(ast && ast->type == AST_FUNCTION);
    struct ast_function *func = (struct ast_function *)ast;
    RUN_LIST(func->redirection, func->nbr_redirection, h);
    int res = RUN(func->shell_command, h);
    return res;
}
