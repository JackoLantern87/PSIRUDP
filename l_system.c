#include "l_system.h"
#include "string.h"

void lsystem_init(L_system *lsys, const char *axiom) {
    strcpy(lsys->axiom, axiom);
    lsys->rule_count = 0;
}

void lsystem_add_rule(L_system *lsys, char symbol, const char *replacement) {
    Rule *rule = &lsys->rules[lsys->rule_count++];
    rule->symbol = symbol;
    strcpy(rule->replacement, replacement);
}

static void derive_once(
        const char *input,
        char *output,
        L_system *lsys
) {
    output[0] = '\0';

    for (int i = 0; input[i]; i++) {
        char c = input[i];
        int replaced = 0;

        for (int r = 0; r < lsys->rule_count; r++) {
            if (lsys->rules[r].symbol == c) {
                strcat(output, lsys->rules[r].replacement);
                replaced = 1;
                break;
            }
        }

        if (!replaced) {
            int len = strlen(output);
            output[len] = c;
            output[len + 1] = '\0';
        }
    }
}

void lsystem_generate(L_system *lsys, int iterations, char *out) {
    static char buf1[MAX_WORD];
    static char buf2[MAX_WORD];

    strcpy(buf1, lsys->axiom);

    for (int i = 0; i < iterations; i++) {
        derive_once(buf1, buf2, lsys);
        strcpy(buf1, buf2);
    }

    strcpy(out, buf1);
}