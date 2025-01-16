#ifndef AST_H
#define AST_H

#include <stddef.h>
#include "hash_map/hash_map.h"

enum AST_TYPE
{
    AST_LIST,
    AST_COMMAND,
    AST_IF,
    AST_VARIABLE,
};

struct ast_ftable;

struct ast
{
    enum AST_TYPE type;
    struct ast_ftable *ftable;
};

struct ast_ftable
{
    int (*run)(struct ast *ast, struct hash_map *h);
    void (*free)(struct ast *ast);
    int (*pretty_print)(struct ast *ast, int actual);
};

struct ast_cmd
{
    struct ast base;
    char **words;
};

struct ast_if
{
    struct ast base;
    struct ast *condition; // the condition
    struct ast *then_body; // the body of the then clause
    struct ast *else_body; // the body of the else, may be NULL
};

struct ast_list
{
    struct ast base;
    size_t nbr_cmd; // number of command
    struct ast **cmd; // the list of command
};

struct ast_variable
{
    struct ast base;
    char *name;
    char *val;
};

struct ast *ast_cmd_init(char **word);
struct ast *ast_if_init(struct ast *condition, struct ast *then_body,
                        struct ast *else_body);

struct ast * ast_variable_init(char *name, char *val);
struct ast *ast_list_init(void);

// list ast function
int list_run(struct ast *ast,struct hash_map *h);
void list_free(struct ast *ast);
int list_pretty_print(struct ast *ast, int actual);
void list_push(struct ast *list_ast, struct ast *new_children);

// cmd ast function
int cmd_run(struct ast *ast,struct hash_map *h);
void cmd_free(struct ast *ast);
int cmd_pretty_print(struct ast *ast, int actual);

// if ast function
int if_run(struct ast *ast,struct hash_map *h);
void if_free(struct ast *ast);
int if_pretty_print(struct ast *ast, int actual);

// pretty_print the entire_ast
void pretty_print_ast(struct ast *ast);

int variable_run(struct ast *ast, struct hash_map *h);
void variable_free(struct ast *ast);

//extand.c

int test_var(char *str);
char *expand(struct hash_map *h, char *str);

#endif /* !AST_H */
