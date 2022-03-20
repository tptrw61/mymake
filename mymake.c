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
#include <sys/stat.h>
#include <time.h>

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

#ifdef st_mtime
#define USE_TIMESPEC
#endif

#ifdef USE_TIMESPEC
typedef struct timespec TimeType;
#else
typedef time_t TimeType;
#endif

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

void addToTargets(Node **list, Node **back, const char *s);
void addToDeps(Node **list, Node **back, const char *s);
void addToComs(Node **list, Node **back, const char *s);
char *lstrip(const char *s); //need to implement
void addAll(Node *targets, Node *deps, Node *coms);
int run(const char *target, const char *from, TimeType *prevTime);



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
    //lists are handled in addAll
    targets = targetsBack = NULL;
    deps = depsBack = NULL;
    coms = comsBack = NULL;

    //everything should be added now
    //run the target
    int rv;
    char *tname;
    if (myTarget == NULL) {
        if (TARGETS == NULL) {
            fprintf(stderr, "%s: No target in makefile\n", NAME);
            return 1;
        }
        tname = TARGETS->s;
    } else {
        tname = myTarget;
    }
    rv = run(tname, NULL, NULL);
    if (rv == 0) {
        printf("%s: '%s' is up to date.\n", NAME, tname);
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

void addToList(Node **list, Node **back, const char *s) {
    assert(list);
    assert(back);
    assert(s);
    Node *n = makeNode(s);
    if (*list == NULL) {
        *list = n;
        *back = n;
        return;
    }
    (*back)->next = n;
    *back = n;
}

void addToTargets(Node **list, Node **back, const char *s) {
    addToList(list, back, s);
}

void addToDeps(Node **list, Node **back, const char *s) {
    addToList(list, back, s);
}

void addToComs(Node **list, Node **back, const char *s) {
    addToList(list, back, s);
}

char *lstrip(const char *s); //need to implement

Node * dupList(Node *list, Node *end) {
    Node *newList = NULL;
    Node *back;
    for (Node *n = list; n != NULL; n = n->next) {
        addToList(&newList, &back, n->s);
    }
    if (list != NULL) {
        back->next = end;
    }
    return newList;
}

void addAll(Node *targets, Node *deps, Node *coms) {
    for (Node *tn = targets; tn != NULL; tn = tn->next) {
        //get target node
        TargetNode *curTargetNode;
        if (TARGETS == NULL) {
            TARGETS = curTargetNode = makeTargetNode(tn->s);
        } else {
            //
            int gotit = 0;
            for (TargetNode *tt = TARGETS; tt != NULL; tt = tt->next) {
                curTargetNode = tt;
                if (strcmp(tt->s, tn->s) == 0) {
                    gotit = 1;
                    break;
                }
            }
            if (!gotit) {
                TargetNode *newNode = makeTargetNode(tn->s);
                curTargetNode->next = newNode;
                curTargetNode = newNode;
            }
        }
        //old deps and commands
        Node *oldDeps = curTargetNode->dependencies;
        if (curTargetNode->commands != NULL) {
            fprintf(stderr, "%s: warning: overriding recipe for target '%s'.\n", FILENAME, tn->s);
            deleteList(curTargetNode->commands);
            curTargetNode->commands = NULL;
        }
        //add deps and commands
        if (tn->next == NULL) { //dont dup the list
            curTargetNode->dependencies = deps;
            curTargetNode->commands = coms;
        } else {
            curTargetNode->dependencies = dupList(deps, oldDeps);
            curTargetNode->commands = dupList(coms, NULL);
        }
    }
    deleteList(targets);
}

TimeType getTimeFromStat(struct stat st) {
#ifdef USE_TIMESPEC
    return st.st_mtim;
#else
    return st.st_mtime;
#endif
}

int cmpTime(TimeType a, TimeType b) {
    //
#ifdef USE_TIMESPEC
    if (a.tv_sec > b.tv_sec) {
        return 1;
    } else if (a.tv_sec == b.tv_sec) {
        if (a.tv_nsec > b.tv_nsec) return 1;
        else if (a.tv_nsec == b.tv_nsec) return 0;
        else return -1;
    } else {
        return -1;
    }
#else
    if (a > b) return 1;
    else if (a == b) return 0;
    else return -1;
#endif
}

void runComs(TargetNode *tn) {
    int ws, rv;
    for (Node *c = tn->commands; c != NULL; c = c->next) {
        ws = system(c->s);
        rv = WEXITSTATUS(ws);
        if (rv != 0) {
            fprintf(stderr, "%s: Recipe for target '%s' failed.\n", FILENAME, tn->s);
            fprintf(stderr, "%s: [%s] Error %d\n", NAME, rv);
            exit(rv);
        }
    }
}

//return 1 if target was run
//return 0 if target was not run
//if recursive run call returns 1 then current run call will have to build
int run(const char *target, const char *from, const TimeType *prevTime) {
    //get file stat and target pointer
    TargetNode *targetPtr = findTarget(target);
    struct stat targetStat;
    int tsrv = stat(target, &targetStat);
    TimeType targetTime;
    if (tsrv == 0) {
        targetTime = getTimeFromStat(targetStat);
    }

    if (tsrv == -1 && errno != ENOENT) { //just break if something went wrong
        perror(target);
        exit(1);
    }
    if (targetPtr == NULL && tsrv == -1) { //no recipe or file
        if (from == NULL) {
            fprintf(stderr, "%s: No rule to make target '%s'.\n", NAME, target);
        } else {
            fprintf(stderr, "%s: No rule to make target '%s', needed by '%s'.\n", NAME, target, from);
        }
        exit(1);
    } else
    if (targetPtr == NULL && tsrv == 0) { //just a file
        if (prevTime != NULL) {
            //check the times
            if (cmpTime(targetTime, *prevTime) == -1) { //if cur file is older than prev file
                return 0; //no need to update above target
            }
        }
        return 1; //force the previous target to run
    } else
    if (targetPtr != NULL && tsrv == -1) { //recipe with no file
        for (Node *c = targetPtr->dependencies; c != NULL; c = c->next) {
            run(c->s, target, NULL);
        }
        runComs(targetPtr);
        return 1;
    } else
    if (targetPtr != NULL && tsrv == 0) { //recipe with a file
        int shouldRunComs = 0;
        for (Node *c = targetPtr->dependencies; c != NULL; c = c->next) {
            shouldRunComs += run(c->s, target, &targetTime);
        }
        if (shouldRunComs > 0) {
            runComs(targetPtr);
            return 1;
        }
        return 0;
    }
    return 0;
}
