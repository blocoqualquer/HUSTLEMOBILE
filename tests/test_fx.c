#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/fx.h"
int main(int argc, char **argv)
{
    FILE *f = fopen(argc > 1 ? argv[1] : "tests/fx_vectors.txt", "r");
    if (!f) { perror("vetores"); return 2; }
    char line[256], s[64]; long long a, b, e, e2; int total = 0, bad = 0;
    while (fgets(line, sizeof line, f)) {
        total++; long long got = 0, got2 = 0; int two = 0, ok;
        if (sscanf(line, "mul %lld %lld %lld", &a, &b, &e) == 3) got = fx_mul(a, b);
        else if (sscanf(line, "div %lld %lld %lld", &a, &b, &e) == 3) got = fx_div(a, b);
        else if (sscanf(line, "round %lld %lld", &a, &e) == 2) got = fx_round(a);
        else if (sscanf(line, "vlen %lld %lld %lld", &a, &b, &e) == 3) got = fx_vec_len(a, b);
        else if (sscanf(line, "norm %lld %lld %lld %lld", &a, &b, &e, &e2) == 4) { fxv n = fxv_norm(fxv_make(a, b)); got = n.x; got2 = n.y; two = 1; }
        else if (sscanf(line, "parse %63s %lld", s, &e) == 2) got = fx_parse(s);
        else { fprintf(stderr, "linha invalida: %s", line); return 2; }
        ok = got == e && (!two || got2 == e2);
        if (!ok && bad++ < 10) fprintf(stderr, "FALHOU: %s  got=%lld,%lld\n", strtok(line, "\n"), got, got2);
    }
    printf("%d vetores, %d falhas\n", total, bad);
    return bad != 0;
}
