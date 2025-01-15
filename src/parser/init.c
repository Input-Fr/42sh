#include <assert.h>
#include <err.h>
#include <stdbool.h>
#include <stdlib.h>

#include "ast.h"


// if initiation
struct ast *ast_if_init(struct ast *condition, struct ast *then_body,
                        struct ast *else_body)
{
    static struct ast_ftable ftable = {
        .run = &if_run,
        .free = &if_free,
        .pretty_print = &if_pretty_print,
        .push = &if_push,
    };
    struct ast_if *if_ast = malloc(sizeof(struct ast_if));
    if (!if_ast)
    {
        return NULL;
    }
    if_ast->base.type = AST_IF;
    if_ast->base.ftable = &ftable;
    if_ast->condition = condition;
    if_ast->then_body = then_body;
    if_ast->else_body = else_body;
    return &if_ast->base;
}



// List initiation
struct ast *ast_list_init(void)
{
    static struct ast_ftable ftable = {
        .run = &list_run,
        .free = &list_free,
        .pretty_print = &list_pretty_print,
        .push = &list_push,
    };
    struct ast_list *list = calloc(1, sizeof(struct ast_list));
    if (!list)
    {
        return NULL;
    }
    list->base.type = AST_LIST;
    list->base.ftable = &ftable;
    return &list->base;
}

// boucle (while and until ) initialisation
struct ast * ast_boucle_init(struct ast * condition,
        struct ast * do_body, int run_condition)
{
    static struct ast_ftable ftable = {
        .run = &boucle_run,
        .free = &boucle_free,
        .pretty_print = &boucle_pretty_print,
        .push = &boucle_push
    };
    struct ast_boucle * boucle = malloc(sizeof(struct ast_boucle));
    if (!boucle)
    {
        return NULL;
    }
    boucle->base.type = AST_BOUCLE;
    boucle->base.ftable = &ftable;
    boucle->condition = condition;
    boucle->do_body = do_body;
    boucle->run_condition = run_condition;
    return &boucle->base;
}

struct ast * ast_redirection_init(int fd, char * words, 
        char * redirection_type)
{
    static struct ast_ftable ftable = {
        .run = &redirectin_run,
        .free = &redirection_free,
        .pretty_print = &redirection_pretty_print,
        .push = &redirection_push,
    };
    struct ast_redirection * redi = malloc(sizeof(struct ast_redirection));
    if (!redi)
    {
        return NULL;
    }
    redi->base.type = AST_REDIRECTION;
    redi->base.ftable = &ftable;
    redi->n = fd;
    redi->words = words;
    redi->redi_op = redirection_type;
    return &redi->base;
}

struct ast * ast_element_init(enum ELEMENT_TYPE type,char * word,
        struct ast * ast)
{
    static struct ast_ftable ftable = {
        .run = &element_run,
        .free = &element_free,
        .pretty_print = &element_pretty_print,
        .push = &element_push,
    };
    struct ast_element* element = malloc(sizeof(struct ast_element));
    if (!words)
    {
        return NULL;
    }
    element->base.type = AST_WORD;
    element->base.ftable = &ftable;
    element->type = type;
    if (type == WORD)
    {
        element->elt->redirection = ast;
    }
    else
    {
        element->elt->word = word;
    }
    return &element->base;
}

struct ast * ast_shell_cmd_init(struct ast * rule)
{
    static struct ast_ftable ftable = {
        .run  = &shell_cmd_run,
        .free = &shell_cmd_free,
        .pretty_print = &shell_cmd_pretty_print,
        .push = &shell_cmd_push,
    }
    struct ast_shell_cmd * cmd = malloc(sizeof(struct ast_shell_cmd));
    if (!cmd)
    {
        return NULL;
    }
    cmd->base.type = AST_SHELL_CMD;
    cmd->base.ftable = &ftable;
    cmd->rule = rule;
    return  &cmd->base;
}


