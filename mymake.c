#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#ifndef _WIN32
#include <sys/wait.h>
#else
#include "sys/wait.h"
#endif

/*
int h_addTarget(const char *target);
int h_addDependency(const char *target, const char *dep);
int h_addCommand(const char *target, const char *command);

int h_dryRun(const char *target);
int h_run(const char *target);

int h_destroy(void);
*/

struct targetNode_s;
struct node_s;

typedef struct node_s Node;
typedef struct targetNode_s TargetNode;

struct node_s {
    char *s;
    Node *next;
};
struct targetNode_s {
    char *s;
    TargetNode *next;
    Node *dependencies;
    Node *commands;
};

TargetNode *TARGETS = NULL;
char *NAME, *FILENAME;

Node * makeNode(const char *s);
TargetNode * makeTargetNode(const char *s);
void deleteNode(Node *n);
void deleteList(Node *list);
void freeAll(void);
TargetNode * findTarget(TargetNode *list, const char *s);

void addToTargets(Node **list, Node **back, const char *s); //need to implement
void addToDeps(Node **list, Node **back, const char *s); //need to implement
void addToComs(Node **list, Node **back, const char *s); //need to implement
char *lstrip(const char *s); //need to implement
void addAll(Node *targets, Node *deps, Node *coms); //need to implement
void run(const char *target, const char *from); //need to implement



int main(int argc, char **argv) {
    NAME = argv[0];
    FILENAME = argv[1];
    if (argc != 3 && argc != 2) {
        fprintf(stderr, "Usage: %s makefile [target]\n", NAME);
        return 1;
    }
    char *myTarget;
    if (argc == 3) {
        myTarget = argv[2];
    } else {
        myTarget = NULL;
    }
    FILE *file;
    if ((file = fopen(argv[1], "r")) == NULL) {
        perror(argv[1]);
        return 1;
    }
    char *line = NULL;
    size_t len = 0;
    char *tok;
    char delim[] = " \n\t\r";
    int foundColon;
    char *colonString1, *colonString2;
    Node *targets = NULL, *targetsBack = NULL;
    Node *deps = NULL, *depsBack = NULL;
    Node *coms = NULL, *comsBack = NULL;
    long lineno = 1;
    while (getline(&line, &len, file) > 0) {
        if (line[0] == '\t') { //if its part of a recipe
            if (targets == NULL) {
                //stop parsing the makefile
                fprintf(stderr, "%s:%ld: Recipe begins before first target.\n", FILENAME, lineno);
                return 1;
            }
            //do commands
            addToComs(&coms, &comsBack, lstrip(tok));
        } else { //if its a target
            //first add all targets and their stuff
            if (targets == NULL) {
                addAll(targets, deps, coms);
                //clear all the lists
                deleteList(targets);
                deleteList(deps);
                deleteList(coms);
                targets = targetsBack = NULL;
                deps = depsBack = NULL;
                coms = comsBack = NULL;
            }
            //setup stuff
            foundColon = 0;
            tok = strtok(line, delim);
            while (tok) {
                if (foundColon) {
                    //do dependencies
                    addToDeps(&deps, &depsBack, tok);

                    tok = strtok(NULL, delim);
                    continue;
                }
                //check for colon
                if (strchr(tok, ':') != NULL) {
                    foundColon = 1;
                    colonString1 = strdup(tok);
                    if (colonString1 == NULL) {
                        perror(argv[0]);
                        return 1;
                    }
                    colonString2 = strchr(colonString1, ':');
                    colonString2[0] = '\0';
                    colonString2++;
                    if (strchr(colonString2, ':') != NULL) {
                        fprintf(stderr, "%s:%ld: Too many colons.\n", FILENAME, lineno);
                        return 1;
                    }
                    //add colonString1 to targets
                    addToTargets(&targets, &targetsBack, colonString1);
                    //add colonString2 to deps
                    addToDeps(&deps, &depsBack, colonString2);
                    free(colonString1);
                    //dont worry about colonString2 cause its a substring of colonString1 technically

                    tok = strtok(NULL, delim);
                    continue;
                }
                //do targets
                addToTargets(&targets, &targetsBack, tok);

                tok = strtok(NULL, delim);
            }
        }
        lineno++;
    }
    addAll(targets, deps, coms);
    //clear all the lists
    deleteList(targets);
    deleteList(deps);
    deleteList(coms);
    targets = targetsBack = NULL;
    deps = depsBack = NULL;
    coms = comsBack = NULL;

    //everything should be added now
    //run the target
    if (myTarget == NULL) {
        if (TARGETS == NULL) {
            fprintf(stderr, "%s: No target in makefile\n", NAME);
            return 1;
        }
        run(TARGETS->s, NULL);
    } else {
        run(myTarget, NULL);
    }
    return 0;
}



Node * makeNode(const char *s) {
    assert(s != NULL);
    Node *n = (Node *)malloc(sizeof(Node));
    if (n == NULL) {
        errno = ENOMEM;
        perror(NAME);
        exit(1);
        return NULL;
    }
    n->s = strdup(s);
    if (n->s == NULL) {
        free(n);
        perror(NAME);
        exit(1);
        return NULL;
    }
    n->next = NULL;
    return n;
}

TargetNode * makeTargetNode(const char *s) {
    assert(s != NULL);
    TargetNode *n = (TargetNode *)malloc(sizeof(TargetNode));
    if (n == NULL) {
        errno = ENOMEM;
        perror(NAME);
        exit(1);
        return NULL;
    }
    n->s = strdup(s);
    if (n->s == NULL) {
        free(n);
        perror(NAME);
        exit(1);
        return NULL;
    }
    n->next = NULL;
    n->dependencies = NULL;
    n->commands = NULL;
    return n;
}

void deleteNode(Node *n) {
    free(n->s);
    free(n);
}

void deleteList(Node *list) {
    Node *c;
    while (list != NULL) {
        c = list;
        list = list->next;
        deleteNode(c);
    }
}

void freeAll(void) {
    TargetNode *c = TARGETS;
    while (TARGETS != NULL) {
        c = TARGETS;
        TARGETS = TARGETS->next;
        deleteList(c->dependencies);
        deleteList(c->commands);
        free(c->s);
        free(c);
    }
}

TargetNode * findTarget(const char *s) {
    for (TargetNode *cur = TARGETS; cur != NULL; cur = cur->next) {
        if (strcmp(s, cur->s) == 0) {
            return cur;
        }
    }
    return NULL;
}


void addToTargets(Node **list, Node **back, const char *s); //need to implement

void addToDeps(Node **list, Node **back, const char *s); //need to implement

void addToComs(Node **list, Node **back, const char *s); //need to implement

char *lstrip(const char *s); //need to implement

void addAll(Node *targets, Node *deps, Node *coms); //need to implement

void run(const char *target, const char *from) {
    TargetNode *tn = findTarget(target);
    if (tn == NULL) { //no such target
        if (from == NULL) {
            fprintf(stderr, "%s: No rule to make target '%s'.\n", NAME, target);
        } else {
            fprintf(stderr, "%s: No rule to make target '%s', needed by '%s'.\n", NAME, target, from);
        }
        exit(1);
    }
    //check dependencies
    for (Node *d = tn->dependencies; d != NULL; d = d->next) {
        //in the future, use a list of targets being run to check for circular dependencies
        //first check for target
        //in the future, check for file and check time and stuff
        TargetNode *dt = findTarget(d->s);
        if (dt == NULL) {
            //check for file
            //TODO
        } else {
            run(dt->s, target);
        }
    }
    //run commands
    int ws, rv;
    for (Node *c = tn->commands; c != NULL; c = c->next) {
        ws = system(c->s);
        rv = WEXITSTATUS(ws);
        if (rv != 0) {
            fprintf(stderr, "%s: Recipe for target '%s' failed.\n", NAME, target);
            fprintf(stderr, "%s: [%s] Error %d\n", NAME, rv);
            exit(rv);
        }
    }
}