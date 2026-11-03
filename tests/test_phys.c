/* Reproduz tests/phys_vectors.txt (gerado por tools/gen_phys_vectors.py) com src/phys.h e compara o estado bit a bit. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/phys.h"

static fx arg(const char *s) { return s[0]=='i' ? FX_INT(atoi(s+1)) : fx_parse(s); }
int main(int argc, char **argv)
{
    FILE *f = fopen(argc > 1 ? argv[1] : "tests/phys_vectors.txt", "r");
    if (!f) { perror("vetores"); return 2; }
    char line[256]; phys_t o = {0}; long steps = 0, bad = 0;
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\n")] = 0;
        if (!strcmp(line, "NEW")) {
            phys_init(&o); o.grav = fx_parse("0.8"); o.gfric = fx_parse("2.5"); o.afric = fx_parse("0.2");
            o.max_gspeed = fx_parse("15"); o.max_aspeed = fx_parse("10"); o.max_fall = fx_parse("15");
            continue;
        }
        if (line[0] == 'S') {
            long long v[10]; sscanf(line + 2, "%lld %lld %lld %lld %lld %lld %lld %lld %lld %lld", v,v+1,v+2,v+3,v+4,v+5,v+6,v+7,v+8,v+9);
            steps++;
            if (o.px!=v[0]||o.py!=v[1]||o.vx!=v[2]||o.vy!=v[3]||o.frx!=v[4]||o.fry!=v[5]||o.grounded!=v[6]||o.facing!=v[7]||o.snap!=v[8]||o.pushback!=v[9]) {
                if (bad++ < 5) printf("DIFF passo %ld: got px=%lld py=%lld vx=%lld vy=%lld g=%d pb=%lld | want %lld %lld %lld %lld g=%lld pb=%lld\n", steps,
                    (long long)o.px,(long long)o.py,(long long)o.vx,(long long)o.vy,o.grounded,(long long)o.pushback, v[0],v[1],v[2],v[3],v[6],v[9]);
            }
            continue;
        }
        char name[64], a[32] = "", b[32] = ""; sscanf(line, "%63s %31s %31s", name, a, b);
        if (!strcmp(name,"set_position")) phys_set_pos(&o, arg(a), arg(b));
        else if (!strcmp(name,"set_vel")) { o.vx = arg(a); o.vy = arg(b); }
        else if (!strncmp(name,"apply_force_relative",20)) phys_force_rel(&o, arg(a), arg(b));
        else if (!strncmp(name,"apply_force",11) && strcmp(name,"apply_forces") && strcmp(name,"apply_forces_no_limit")) phys_force(&o, arg(a), arg(b));
        else if (!strcmp(name,"apply_grav")) phys_grav(&o);
        else if (!strcmp(name,"apply_grav_custom")) phys_grav_custom(&o, arg(a), arg(b));
        else if (!strcmp(name,"apply_forces")) phys_apply_forces(&o);
        else if (!strcmp(name,"apply_forces_no_limit")) phys_apply_forces_no_limit(&o);
        else if (!strcmp(name,"update_grounded")) phys_update_grounded(&o);
        else if (!strcmp(name,"set_facing")) o.facing = atoi(a+1);
        else if (!strncmp(name,"move_directly_relative",22)) phys_move_rel(&o, arg(a), arg(b));
        else if (!strncmp(name,"move_directly",13)) phys_move(&o, arg(a), arg(b));
        else if (!strcmp(name,"apply_x_fric")) phys_x_fric(&o, arg(a));
        else if (!strcmp(name,"apply_y_fric")) phys_y_fric(&o, arg(a));
        else if (!strcmp(name,"apply_fric")) phys_fric(&o);
        else if (!strcmp(name,"reset_momentum")) phys_reset_momentum(&o);
        else if (!strcmp(name,"set_grounded")) o.grounded = !strcmp(a,"true");
        else if (!strcmp(name,"set_snap_to_ground")) o.snap = !strcmp(a,"true");
        else if (!strcmp(name,"set_gravity_modifier")) o.gmod = arg(a);
        else if (!strcmp(name,"add_pushback")) phys_add_pushback(&o, arg(a));
        else if (!strcmp(name,"apply_pushback")) phys_apply_pushback(&o, atoi(a+1));
        else if (!strcmp(name,"reset_pushback")) phys_reset_pushback(&o);
        else { printf("op desconhecida: %s\n", name); return 2; }
    }
    printf("%ld passos, %ld divergencias\n", steps, bad);
    return bad != 0;
}
