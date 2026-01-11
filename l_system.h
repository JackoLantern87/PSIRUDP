//
// Created by user on 05.01.2026.
//

#ifndef L_SYSTEM_SAMODZIELNIE_L_SYSTEM_H
#define L_SYSTEM_SAMODZIELNIE_L_SYSTEM_H
#define MAX_RULES 64
#define MAX_WORD 200000

typedef struct {
    char symbol;
    char replacement[128];
} Rule;

typedef struct {
    Rule rules[MAX_RULES];
    char* axiom;
    int rule_count;
} L_system;

void lsystem_init(L_system* lsys, const char* axiom);
void lsystem_generate(L_system* lsys, int iterations, char *out);
void lsystem_add_rule(L_system* lsys, char symbol, const char *replacement);
#endif //L_SYSTEM_SAMODZIELNIE_L_SYSTEM_H
