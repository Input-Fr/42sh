#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ast.h"
#include "cd.h"
#include "expand/expand.h"
#include "hash_map/hash_map.h"
#include "lexer/lexer.h"
#include "parser/parser.h"

#define RUN(AST, HASH_TABLE) (*(AST)->ftable->run)((AST), (HASH_TABLE))

#define RUN_LIST(ASTS, ELT, HASH_TABLE)                                        \
    handle_list_ast((ASTS), (&ELT), HASH_TABLE) // évalue une liste d'ast

// crée la liste d'élément pour l'ast élément
#define MAKE_WORD(WORD, ASTS, NBR_ELT, H)                                      \
    create_words((WORD), (ASTS), &(NBR_ELT), H)

// FONCTION ANNEXE REDIR
struct s_redirection
{
    int original_fd;
    int saved_fd;
    struct s_redirection *next;
};

static int egal(char *word, const char *second);

// liste globale des fd sauvegardés
struct s_redirection *s_redir = NULL;

static void restore(void);

static int handle_redirection(int fd, enum REDIRECTION_TYPE redir_op,
                              char *word, struct hash_map *h);

static int isnum(const char *str)
{
    char *endptr;
    if (*str == 0) // nan
        return 0;

    strtol(str, &endptr, 10);
    return *endptr == 0;
}

static void printWbackslash(char *carg)
{
    size_t idx = 0;
    while (carg[idx])
    {
        if (carg[idx] == '\\')
        {
            idx++;
            if (carg[idx] == 'n')
            {
                printf("\n");
            }
            else if (carg[idx] == 't')
            {
                printf("\t");
            }
            else if (carg[idx] == '\\')
            {
                printf("\\");
            }
            else // connais pas
            {
                printf("\\");
                if (carg[idx] != '\0')
                {
                    printf("%c", carg[idx]);
                }
            }
        }
        else
        {
            printf("%c", carg[idx]);
        }
        idx++;
    }
}

// fonction qui gère les les ast avec une liste d'ast en attribut
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

// fonction pour la simple commande qui crée le word

// check if the element is a word
static int is_word(struct ast *ast)
{
    assert(ast && ast->type == AST_ELEMENT);
    return ((struct ast_element *)ast)->type == WORD;
}

static char **create_words(char *word, struct ast **asts, size_t *nbr_element,
                           struct hash_map *h)
{
    char **words = malloc(sizeof(char *));
    words[0] = word; // already expanded
    int size = 1;
    for (size_t i = 0; i < *nbr_element; i++)
    {
        if (is_word(asts[i]))
        {
            struct ast_element *elt = NULL;
            elt = (struct ast_element *)(asts[i]);
            size += 1;
            char **test = realloc(words, size * sizeof(char *));
            if (!test)
            {
                fprintf(stderr, "error");
                exit(2);
            }
            words = test;
            char *expands = expand(h, elt->elt.word);
            words[(size - 1)] = expands;
        }
    }
    size += 1;
    char **test = realloc(words, size * sizeof(char *));
    if (!test)
    {
        fprintf(stderr, "error");
        exit(2);
    }
    words = test;
    words[(size - 1)] = NULL;
    return words;
}

static size_t set_args(bool *newline, bool *backslash, char *args[],
                       size_t nb_args)
{
    size_t i = 0;
    while (i < nb_args && args[i][0] == '-')
    {
        size_t j = 1;
        while (args[i][j])
        {
            if (args[i][j] == 'n')
            {
                *newline = false;
            }
            else if (args[i][j] == 'e')
            {
                *backslash = true;
            }
            else if (args[i][j] == 'E')
            {
                *backslash = false;
            }
            else
            {
                // pas une option valide, juste afficher
                break;
            }
            j++;
        }
        if (!args[i][j])
        {
            i++;
            if (!strcmp(args[i - 1], "-"))
            {
                break;
            }
        }
        else
        {
            break;
        }
    }
    return i;
}

// args est de la forme ["arg1", "arg2", "arg3"]
static void echo_builtin(char *args[], size_t nb_args, struct hash_map *h)
{
    if (h == NULL)
    {
        return;
    }
    bool newline = true;
    bool backslash = false;
    size_t i = set_args(&newline, &backslash, args, nb_args);
    // echo les arguments
    while (i < nb_args)
    {
        char *str = args[i];
        if (backslash)
        {
            printWbackslash(str);
        }
        else
        {
            printf("%s", str);
        }
        if (i < nb_args - 1) // on sépare tout les argument d'un espace
        {
            printf(" ");
        }
        i++;
    }

    if (newline)
    {
        printf("\n");
    }

    fflush(stdout);
}
// for three evaluation

// list ast eval
int list_run(struct ast *ast, struct hash_map *h)
{
    assert(ast && ast->type == AST_LIST);
    struct ast_list *list = (struct ast_list *)ast;
    size_t i = 0;
    int res = 0;
    while (i < list->nbr_cmd && !(res = RUN(list->cmd[i], h)))
    {
        i += 1;
    }
    if (i >= list->nbr_cmd)
    {
        return 0;
    }
    return res;
}

static void exit_builtin(char *opt)
{
    if (opt && isnum(opt))
    {
        int n = atoi(opt);
        if (n != 0)
            fprintf(stderr, "error\n");
        exit(n);
    }
    exit(0);
}

static int unset_builtin(char *args[], size_t nb_args, struct hash_map *h)
{
    if (!nb_args)
    {
        return 0;
    }
    size_t i = 0;
    bool var = false;
    bool fonc = false;
    while (args[i][0] == '-')
    {
        char first = args[i][1];
        for (size_t j = 1; args[i][j] != '\0'; j += 1) // cas -vvvvfvvvv
        {
            if (strlen(args[i]) > 0 && args[i][j] != 'v' && args[i][j] != 'f')
                errx(2, "wrong option");
            if (first != args[i][j])
                errx(1, "wrong option");
        }

        if ((first == 'f' && var) || (first == 'v' && fonc)) // cas -f -v -f
            errx(1, "wrong option");

        if (strlen(args[i]) > 0 && args[i][1] != 'v' && args[i][1] != 'f')
            errx(2, "wrong option");

        if (args[i][1] != 'f')
        {
            var = false;
            fonc = true;
        }
        if (args[i][1] != 'v')
        {
            var = true;
            fonc = false;
        }
        i += 1;
    }

    if (!var && !fonc)
        var = true;

    for (; i < nb_args; i += 1)
    {
        unsetenv(args[i]);
        hash_map_remove(h, args[i]);
    }

    return 0;
}

static int test_ifvalexist(char *word)
{
    for (size_t i = 0; word[i] != '\0'; i += 1)
    {
        if (word[i] == '=')
        {
            return 1;
        }
    }
    return 0;
}

static int export_builtin(char *args[], size_t nb_args, struct hash_map *h)
{
    if (nb_args == 0)
    {
        return 0;
    }
    for (size_t i = 0; i < nb_args; i += 1)
    {
        char *word = args[i];
        if (word[0] == '-' && word[1] != 'p')
        {
            // fprintf(stderr, "error\n");
            return 2;
        }
        if (word[0] == '-')
        {
            word = args[i + 1];
        }
        if (!test_name(word))
        {
            // fprintf(stderr, "error\n");
            return 1;
        }
        else
        {
            if (test_ifvalexist(word))
            {
                char *name = calloc(1, strlen(word) + 1);
                char *val = calloc(1, strlen(word) + 1);
                separator_equal(name, val, word);
                hash_map_remove(h, name);
                hash_map_insert(h, name, val, VARIABLE);
                setenv(name, val, 1);
            }
            else
            {
                struct ast *test = NULL;
                setenv(word, hash_map_get(h, word, &test), 1);
            }
        }
    }
    return 0;
}

// if ast eval
int if_run(struct ast *ast, struct hash_map *h)
{
    assert(ast && ast->type == AST_IF);
    struct ast_if *if_ast = (struct ast_if *)ast;
    if (!RUN(if_ast->condition, h))
    {
        return RUN(if_ast->then_body, h);
    }
    else if (!if_ast->else_body)
    {
        return 0;
    }
    else
    {
        return RUN(if_ast->else_body, h);
    }
}

// and or eval
int and_or_run(struct ast *ast, struct hash_map *h)
{
    assert(ast && ast->type == AST_AND_OR);
    struct ast_and_or *and_or_ast = (struct ast_and_or *)ast;
    if (and_or_ast->t == NODE_PIPELINE)
    {
        return RUN(and_or_ast->c.pipeline, h);
    }
    else
    {
        if (and_or_ast->c.op->op == AND_OP)
        {
            return RUN(and_or_ast->c.op->left, h)
                && RUN(and_or_ast->c.op->right, h);
        }
        else
        {
            return RUN(and_or_ast->c.op->left, h)
                || RUN(and_or_ast->c.op->right, h);
        }
    }
}

static int check_condition(int check, int cdt)
{
    if (cdt)
    {
        return check;
    }
    return !check;
}

// boucle (until and while) ast eval
int boucle_run(struct ast *ast, struct hash_map *h)
{
    assert(ast && ast->type == AST_BOUCLE);
    struct ast_boucle *boucle = (struct ast_boucle *)ast;
    int res = 0;
    while (check_condition(RUN(boucle->condition, h), boucle->run_condition))
    {
        res = RUN(boucle->do_body, h);
    }
    return res;
}

// redirection ast eval
int redirection_run(struct ast *ast, struct hash_map *h)
{
    assert(ast && ast->type == AST_REDIRECTION);
    struct ast_redirection *redi = (struct ast_redirection *)ast;
    int ret = handle_redirection(redi->n, redi->redir_op, redi->word, h);
    return ret;
}

// element ast eval
int element_run(struct ast *ast, struct hash_map *h)
{
    assert(ast && ast->type == AST_ELEMENT);
    struct ast_element *elt = (struct ast_element *)ast;
    if (elt->type == WORD)
    {
        return 0;
    }
    else
    {
        return RUN(elt->elt.redirection, h);
    }
}

int shell_cmd_run(struct ast *ast, struct hash_map *h)
{
    assert(ast && ast->type == AST_SHELL_CMD);
    struct ast_shell_cmd *cmd = (struct ast_shell_cmd *)ast;
    int j = 0;
    j = RUN_LIST(cmd->redirection, cmd->nbr_redirection, h);
    j = RUN(cmd->rule, h);
    restore();
    return j;
}

int variable_run(struct ast *ast, struct hash_map *h)
{
    assert(ast && ast->type == AST_VARIABLE);
    struct ast_variable *variable_ast = (struct ast_variable *)ast;
    char *tmp = variable_ast->val;
    char *str = expand(h, variable_ast->val);
    free(tmp);
    hash_map_remove(h, variable_ast->name);
    hash_map_insert(h, variable_ast->name, str, VARIABLE);
    return 1;
}

int pipeline_run(struct ast *ast, struct hash_map *h)
{
    assert(ast && ast->type == AST_PIPELINE);
    struct ast_pipeline *ast_pipe = (struct ast_pipeline *)ast;
    int save_stdout = dup(STDOUT_FILENO);
    int save_stdin = dup(STDIN_FILENO);
    int pipefd[2];
    int ret = 0;
    for (size_t i = 0; i < ast_pipe->nbr_cmd; i++)
    {
        if (i < ast_pipe->nbr_cmd - 1)
        {
            if (pipe(pipefd) == -1)
            {
                perror("pipe");
                break;
            }

            if (dup2(pipefd[1], STDOUT_FILENO) == -1)
            {
                perror("dup2");
                break;
            }
            close(pipefd[1]);
        }
        else // derniere commande
        {
            if (dup2(save_stdout, STDOUT_FILENO) == -1)
            {
                perror("dup2");
                break;
            }
        }
        ret = RUN(ast_pipe->cmd[i], h);

        if (i < ast_pipe->nbr_cmd - 1)
        {
            if (dup2(pipefd[0], STDIN_FILENO) == -1)
            {
                perror("dup2");
                break;
            }
            close(pipefd[0]);
        }
    }
    dup2(save_stdin, STDIN_FILENO);
    close(save_stdin);
    close(save_stdout);
    ret = ast_pipe->negation ? !ret : ret;
    return ret;
}

static int save_fd(int fd)
{
    int save = dup(fd);
    if (save == -1)
    {
        perror("dup");
        return 1;
    }
    struct s_redirection *new = calloc(1, sizeof(struct s_redirection));
    if (!new)
    {
        close(save);
        return 1;
    }

    new->original_fd = fd;
    new->saved_fd = save;
    new->next = s_redir;
    s_redir = new;
    return 0;
}

static void restore(void)
{
    while (s_redir)
    {
        struct s_redirection *cur = s_redir;

        if (dup2(cur->saved_fd, cur->original_fd) == -1)
        {
            perror("error during restore");
        }

        close(cur->saved_fd);
        s_redir = cur->next;
        free(cur);
    }
}

static int str_to_fd(const char *str)
{
    for (int i = 0; str[i]; i++)
    {
        if (!isdigit(str[i]))
        {
            return -1;
        }
    }
    return atoi(str);
}

static void set_vars(int *fd, int *flags, enum REDIRECTION_TYPE redir_op)
{
    switch (redir_op)
    {
    case LESS:
        *fd = *fd == -1 ? STDIN_FILENO : *fd;
        *flags = O_RDONLY;
        break;
    case GREATER:
        *fd = *fd == -1 ? STDOUT_FILENO : *fd;
        *flags = O_WRONLY | O_CREAT | O_TRUNC;
        break;
    case DGREATER:
        *fd = *fd == -1 ? STDOUT_FILENO : *fd;
        *flags = O_WRONLY | O_CREAT | O_APPEND;
        break;
    case CLOBBER:
        *fd = *fd == -1 ? STDOUT_FILENO : *fd;
        *flags = O_WRONLY | O_CREAT | O_TRUNC;
        break;
    case LESS_GREATER:
        *fd = *fd == -1 ? STDIN_FILENO : *fd;
        *flags = O_RDWR | O_CREAT;
        break;
    case GREATER_AND:
        *fd = *fd == -1 ? STDOUT_FILENO : *fd;
        break;
    case LESS_AND:
        *fd = *fd == -1 ? STDIN_FILENO : *fd;
        break;
    default:
        return;
    }
}

static int handle_redirection(int fd, enum REDIRECTION_TYPE redir_op,
                              char *word, struct hash_map *h)
{
    int flags = -1;
    int new_fd = -1;
    set_vars(&fd, &flags, redir_op);

    if (redir_op == GREATER_AND || redir_op == LESS_AND)
    {
        if (word[0] == '-')
        {
            if (save_fd(fd) != -1)
            {
                close(fd);
                return 0;
            }
            return 1;
        }
        int fd2 = str_to_fd(word);
        if (fd2 < 0 || fcntl(fd2, F_GETFL) == -1)
        {
            return 1; // not a regular fd
        }
        if (save_fd(fd) == -1)
        {
            return 1;
        }
        if (dup2(fd2, fd) == -1)
        {
            return 1; // error while duplicating
        }

        return 0;
    }
    if (save_fd(fd) == -1)
    {
        return 1;
    }
    // expand
    char *expanded = expand(h, word);
    if (strcmp(expanded, "")) // if not empty
    {
        word = expanded;
    }
    new_fd = open(word, flags, 0644);
    if (strcmp(expanded, "")) // if not empty
    {
        free(expanded);
    }
    if (new_fd == -1)
    {
        return 1; // error while opening
    }

    if (dup2(new_fd, fd) == -1)
    {
        perror("dup2");
        close(new_fd);
        return 1;
    }

    close(new_fd);
    return 0;
}

static void free_words(char **words)
{
    if (words)
    {
        for (size_t i = 0; words[i] != NULL; i++)
        {
            free(words[i]);
        }
    }
    free(words);
}

static int handle_executable_builtin(char **words)
{
    pid_t pid = fork();
    if (pid == 0)
    {
        int status_code = execvp(words[0], words);
        if (status_code == -1)
        {
            fprintf(stderr, "command error\n");
            exit(127);
        }
    }
    int wstatus;
    waitpid(pid, &wstatus, 0);
    int return_value = WEXITSTATUS(wstatus);
    if (return_value == 127)
    {
        fprintf(stderr, "error\n");
        return 127;
    }
    return 0;
}

static int handle_special_builtin(char **words, struct hash_map *h)
{
    if (!words)
    {
        return 2;
    }
    else
    {
        int idx = 1;
        while (words[idx])
        {
            idx++;
        }
        if (!strcmp(words[0], "echo"))
        {
            echo_builtin(words + 1, idx - 1, h);
        }
        else if (!strcmp(words[0], "true"))
        {
            return 0;
        }
        else if (!strcmp(words[0], "false"))
        {
            return 1;
        }
        else if (!strcmp(words[0], "exit"))
        {
            exit_builtin(words[1]);
        }
        else if (!strcmp(words[0], "unset"))
        {
            return unset_builtin(words + 1, idx - 1, h);
        }
        else if (!strcmp(words[0], "export"))
        {
            return export_builtin(words + 1, idx - 1, h);
        }
        else if (!strcmp(words[0], "cd"))
        {
            if (words[1])
                return cmd_cd(words[1]);
            return cmd_cd("");
        }
        return 0;
    }
}

static char *unspecified_behaviour[] = {
    "alloc",         "autoload",   "bind",     "bindkey",      "builtin",
    "bye",           "caller",     "cap",      "chdir",        "clone",
    "comparguments", "compcall",   "compctl",  "compdescribe", "compfiles",
    "compgen",       "compgroups", "complete", "compquote",    "comptags",
    "comptry",       "compvalues", "declare",  "dirs",         "disable",
    "disown",        "dosh",       "echotc",   "help",         "history",
    "hist",          "let",        "local",    "login",        "map",
    "mapfile",       "popd",       "print",    "pushd",        "readarray",
    "savehistory",   "source",     "shopt",    "stop",         "stop",
    "suspend",       "typeset",    "whence",
};

static char *type_ulimit[] = { "alias",   "bg",   "cd",   "command",
                               "false",   "fc",   "fg",   "getopts",
                               "hash",    "jobs", "kill", "newgrp",
                               "pwd",     "read", "true", "umask",
                               "unalias", "wait" };

static int egal(char *word, const char *second)
{
    return !strcmp(word, second);
}

static int is_special_builtin(char **words)
{
    int test = egal(words[0], "echo") || egal(words[0], "true");
    int test2 = egal(words[0], "false") || egal(words[0], "exit");
    int test3 = egal(words[0], "cd") || egal(words[0], "unset");
    return egal(words[0], "export") || test || test2 || test3;
}

static int is_unspecified_behaviour(char *word)
{
    short i = 0;
    while (i < 48 && !egal(word, unspecified_behaviour[i]))
    {
        i += 1;
    }
    return i < 48;
}

static int is_type_or_ulimit(char *word)
{
    short i = 0;
    while (i < 18 && !egal(word, type_ulimit[i]))
    {
        i += 1;
    }
    return i < 18;
}

static short is_created_function(char *words, struct hash_map *h)
{
    struct ast *save = NULL;
    hash_map_get(h, words, &save);
    return save != NULL;
}

static int get_nbrelt(char **words)
{
    int i = 0;
    while (words[i])
    {
        i += 1;
    }
    return i;
}

static int handle_created_function(char **words, struct hash_map *h)
{
    struct ast *save = NULL;
    hash_map_get(h, words[0], &save);
    assert(save && save->type == AST_FUNCTION);
    int save_nbrags = h->nb_args;
    char **save_args = h->all_args;
    h->nb_args = get_nbrelt(words);
    h->all_args = words;
    int res = function_run_hashmap(save, h);
    h->all_args = save_args;
    h->nb_args = save_nbrags;
    return res;
}

static int cmd_run(char **words, struct hash_map *h)
{
    if (!words)
    {
        return 2;
    }
    if (is_special_builtin(words)) // fist rule
    {
        return handle_special_builtin(words, h);
    }
    else if (is_unspecified_behaviour(words[0])) // second rule
    {
        return 1;
    }
    else if (is_created_function(words[0], h))
    {
        return handle_created_function(words, h);
    }
    else if (is_type_or_ulimit(words[0]))
    {
        return handle_executable_builtin(words);
    }
    return handle_executable_builtin(words);
}

int simple_cmd_run(struct ast *ast, struct hash_map *h)
{
    assert(ast && ast->type == AST_SIMPLE_CMD);
    struct ast_simp_cmd *cmd = (struct ast_simp_cmd *)ast;
    int result = RUN_LIST(cmd->prefix, cmd->nbr_prefix, h);
    result = RUN_LIST(cmd->element, cmd->nbr_element, h);
    if (cmd->word)
    {
        char *expanded = expand(h, cmd->word);
        char **words = NULL;
        words = MAKE_WORD(expanded, cmd->element, cmd->nbr_element, h);
        result = cmd_run(words, h);
        free_words(words);
    }
    restore();
    return result;
}

// FIN ANNEXE -------------
