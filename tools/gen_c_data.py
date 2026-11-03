#!/usr/bin/env python3
"""Gera C (inicializadores constantes) a partir do JSON de personagem de tools/convert_char.py.

uso: gen_c_data.py <char.json> <char_id> <saida.c> [<raiz_do_jogo>]   (a raiz permite classificar estados por heranca: hurt/parry/counter/throw)
Emite hitbox_def_t / hurtstate_def_t / state_def_t / anim_def_t / chardata_t. Scripts de estado viram referencias
fracas a `vt_<caminho_do_script>` (se o port do script nao existir, o ponteiro e NULL e o estado usa so o comportamento base)."""
import sys, os, json, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from fxmodel import parse

def fxs(v):
    try: return 'FX_RAW(%d)' % parse(str(v)) if v not in (None, '') else '0'
    except ValueError: return '0'      # valor nao numerico no .tscn (ex.: "False" num ThrowBox): o original tambem nao o usa
def b(v): return '1' if v else '0'
def i(v): return str(int(v))
def cname(s): return re.sub(r'\W', '_', s)

def fnv1a(s):
    h = 0x811C9DC5
    for c in s.encode(): h = ((h ^ c) * 0x01000193) & 0xFFFFFFFF
    return h

def script_chain(root, script):
    """sequencia de scripts pelo 'extends' (ate sair de caminhos de arquivo)."""
    chain = []
    seen = set()
    while script and script not in seen and root:
        seen.add(script)
        path = os.path.join(root, script.replace('res://', ''))
        if not os.path.exists(path): break
        chain.append(script)
        m = None
        for ln in open(path, encoding='utf-8', errors='replace'):
            m = re.match(r'\s*extends\s+"?([^"\s]+)"?', ln)
            if m: break
        if not m: break
        t = m.group(1)
        if t.endswith('.gd'): script = t.replace('res://', '')
        else:
            # classe por nome: procurar class_name no projeto (cache simples)
            script = CLASSES.get(t)
            if script is None: chain.append('class:' + t); break
    return chain

CLASSES = {}
def index_classes(root):
    if not root: return
    for dp, _, fns in os.walk(root):
        for fn in fns:
            if fn.endswith('.gd'):
                p = os.path.join(dp, fn)
                for ln in open(p, encoding='utf-8', errors='replace'):
                    m = re.match(r'\s*class_name\s+(\w+)', ln)
                    if m: CLASSES[m.group(1)] = os.path.relpath(p, root).replace('\\', '/'); break

def kind_of(root, script):
    if not script: return 0
    names = [os.path.basename(x).replace('.gd', '') for x in script_chain(root, script) if not x.startswith('class:')]
    if 'HurtGrounded' in names: return 1
    if 'HurtAerial' in names: return 2
    if 'NewParry' in names: return 4
    if 'Parry' in names: return 3
    if 'CounterAttack' in names: return 5
    if 'ThrowState' in names: return 6
    return 0

def main(jpath, cid, out, root=None):
    index_classes(root)
    d = json.load(open(jpath)); nodes = d['nodes']
    sprite_anims = d['animations'].get('Flip/Sprite', {})
    anim_names = sorted(sprite_anims)
    states = [k for k in nodes if k.startswith('StateMachine/') and k.count('/') == 1]
    sid = {k.split('/')[1]: n for n, k in enumerate(states)}
    def sidof(name): return sid.get(name, -1) if name else -1
    names = [k.split('/')[1] for k in states]          # id do nome de estado == sid
    def nid(nm):
        nm = nm.strip()
        if nm not in names: names.append(nm)
        return names.index(nm)
    for nm in ('OffensiveBurst', 'InstantCancel', 'GroundedDefense', 'AerialDefense', 'GroundedGrab', 'AerialGrab', 'Wait', 'Fall', 'BusyNormal', 'BusyHurt', 'Normal', 'All'):
        nid(nm)
    extra = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'extra_names.txt')
    if os.path.exists(extra):
        for nm in open(extra).read().split():
            nid(nm)
    def nset(text):
        w = [0] * 8
        for ln in (text or '').split('\n'):
            if ln.strip():
                i_ = nid(ln); w[i_ >> 5] |= 1 << (i_ & 31)
        return '{{%s}}' % ','.join('0x%08XU' % x for x in w)
    L = ['/* GERADO por tools/gen_c_data.py - nao editar */', '#include "../chardata.h"', '']
    vts = set(); scripts = {}
    for k in states:
        sc = nodes[k].get('script')
        if sc:
            scripts[k] = 'vt_' + cname(sc.replace('.gd', '').replace('res://', ''))
            vts.add(scripts[k])
    for v in sorted(vts): L.append('extern const state_vt_t %s __attribute__((weak));' % v)
    L.append('')
    # animacoes
    for n, a in enumerate(anim_names):
        fr = sprite_anims[a]['frames']
        L.append('static const uint32_t anim_%s_%d[] = {%s};' % (cid, n, ','.join('0x%08X' % (fnv1a(f.replace('res://', '')) if f else 0) for f in fr)))
    L.append('static const anim_def_t anims_%s[] = {' % cid)
    for n, a in enumerate(anim_names):
        L.append(' {"%s", %d, anim_%s_%d, %d},' % (a, len(sprite_anims[a]['frames']), cid, n, 1 if sprite_anims[a].get('loop') else 0))
    L.append('};')
    nlimb_total = 0
    for k in states:
        sname = k.split('/')[1]; cn = cname(sname)
        kids = [(kk, nodes[kk]) for kk in nodes if kk.startswith(k + '/') and kk.count('/') == 2]
        hbs = [(kk, v) for kk, v in kids if 'hitbox_type' in v['props']]
        hss = [(kk, v) for kk, v in kids if 'start_tick' in v['props'] and 'hitbox_type' not in v['props'] and not (v.get('script') or '').endswith('LimbHurtbox.gd') and 'started' in v['props']]
        lbs = [(kk, v) for kk, v in kids if (v.get('script') or '').endswith('LimbHurtbox.gd')]
        if hbs:
            L.append('static const hitbox_def_t hb_%s_%s[] = {' % (cid, cn))
            for kk, v in hbs:
                p = v['props']
                f = [('x', i(p['x'])), ('y', i(p['y'])), ('w', i(p['width'])), ('h', i(p['height'])),
                     ('is_throwbox', b('throw_state' in p)), ('activated', b(p['activated'])),
                     ('damage', i(p['damage'])), ('damage_in_combo', i(p['damage_in_combo'])), ('minimum_damage', i(p['minimum_damage'])),
                     ('chip_damage_modifier', fxs(p['chip_damage_modifier'])), ('hitbox_type', i(p['hitbox_type'])),
                     ('hitstun_ticks', i(p['hitstun_ticks'])), ('combo_hitstun_ticks', i(p['combo_hitstun_ticks'])), ('hitlag_ticks', i(p['hitlag_ticks'])),
                     ('victim_hitlag', i(p['victim_hitlag'])), ('combo_victim_hitlag', i(p['combo_victim_hitlag'])), ('damage_proration', i(p['damage_proration'])),
                     ('scale_combo', b(p['scale_combo'])), ('combo_scaling_amount', i(p['combo_scaling_amount'])), ('cancellable', b(p['cancellable'])),
                     ('increment_combo', b(p['increment_combo'])), ('hits_otg', b(p['hits_otg'])), ('hits_vs_standing', b(p['hits_vs_standing'])),
                     ('hits_vs_grounded', b(p['hits_vs_grounded'])), ('hits_vs_aerial', b(p['hits_vs_aerial'])), ('can_counter_hit', b(p['can_counter_hit'])),
                     ('di_modifier', fxs(p['di_modifier'])), ('sdi_modifier', fxs(p['sdi_modifier'])), ('meter_gain_modifier', fxs(p['meter_gain_modifier'])),
                     ('parry_meter_gain', i(p['parry_meter_gain'])), ('ignore_armor', b(p['ignore_armor'])), ('followup_state', i(sidof(p['followup_state']))),
                     ('force_grounded', b(p['force_grounded'])), ('can_clash', b(p['can_clash'] and p['hitbox_type'] != 6)), ('hits_vs_dizzy', b(p['hits_vs_dizzy'])),
                     ('beats_grab', b(p['beats_grab'])), ('hits_projectiles', b(p['hits_projectiles'])), ('ignore_projectile_armor', b(p['ignore_projectile_armor'])),
                     ('allowed_to_hit_own_team', b(p['allowed_to_hit_own_team'])), ('guard_break', b(p['guard_break'])), ('guard_break_proration', i(p['guard_break_proration'])),
                     ('block_punishable', b(p['block_punishable'])), ('parriable', b(p['parriable'])), ('block_cancel_allowed', b(p['block_cancel_allowed'])),
                     ('plus_frames', i(p['plus_frames'])), ('hit_height', i(p['hit_height'])), ('priority', i(p['priority'])), ('group', i(p['group'])),
                     ('screenshake_amount', i(p['screenshake_amount'])), ('screenshake_frames', i(p['screenshake_frames'])), ('bump_on_whiff', b(p['bump_on_whiff'])),
                     ('rumble', b(p['rumble'])), ('bass_on_whiff', b(p['bass_on_whiff'])),
                     ('dir_x', fxs(p['dir_x'])), ('dir_y', fxs(p['dir_y'])), ('knockback', fxs(p['knockback'])), ('launch_reversible', b(p['launch_reversible'])),
                     ('vacuum', b(p['vacuum'])), ('send_away_from_center', b(p['send_away_from_center'])), ('block_pushback_modifier', fxs(p['block_pushback_modifier'])),
                     ('block_pushback_reversible', b(p['block_pushback_reversible'])), ('block_reverse_pushback_modifier', fxs(p['block_reverse_pushback_modifier'])),
                     ('pushback_x', fxs(p['pushback_x'])), ('grounded_hit_state', i(sidof(p['grounded_hit_state']))), ('aerial_hit_state', i(sidof(p['aerial_hit_state']))),
                     ('minimum_grounded_frames', i(p['minimum_grounded_frames'])), ('knockdown', b(p['knockdown'])), ('knockdown_extends_hitstun', b(p['knockdown_extends_hitstun'])),
                     ('hard_knockdown', b(p['hard_knockdown'])), ('disable_collision', b(p['disable_collision'])), ('air_ground_bounce', b(p['air_ground_bounce'])),
                     ('ground_bounce', b(p['ground_bounce'])), ('ground_bounce_knockback_modifier', fxs(p['ground_bounce_knockback_modifier'])), ('wall_slam', b(p['wall_slam'])),
                     ('start_tick', i(p['start_tick'])), ('active_ticks', i(p['active_ticks'])), ('always_on', b(p['always_on'])), ('looping', b(p['looping'])),
                     ('loop_active_ticks', i(p['loop_active_ticks'])), ('loop_inactive_ticks', i(p['loop_inactive_ticks'])),
                     ('throw_state', i(sidof(p.get('throw_state', '')))), ('sfx_hit', '-1'), ('sfx_whiff', '-1'), ('sfx_bass', '-1'), ('hit_particle', '-1'), ('whiff_particle', '-1')]
                L.append(' {' + ', '.join('.%s = %s' % kv for kv in f) + '},')
            L.append('};')
        for tag, lst in (('hs', hss), ('lb', lbs)):
            if lst:
                L.append('static const hurtstate_def_t %s_%s_%s[] = {' % (tag, cid, cn))
                for kk, v in lst:
                    p = v['props']
                    L.append(' {%s, %s, %s, %s, %s, %s, %s},' % (i(p['x']), i(p['y']), i(p['width']), i(p['height']), i(p['start_tick']), i(p['active_ticks']), b(p.get('endless'))))
                L.append('};')
        nlimb_total += len(lbs)
    for k in states:
        p = nodes[k]['props']
        if 'iasa_at' not in p: continue
        cn = cname(k.split('/')[1])
        ia = p['iasa_at']
        if ia < 0: ia = p['anim_length'] + ia
        fr = p.get('interrupt_frames') or []
        if fr: L.append('static const int16_t if_%s_%s[] = {%s};' % (cid, cn, ','.join(str(int(x)) for x in fr)))
        f = []
        bools = ['uses_air_movement', 'ignore_air_option_use_in_combos', 'land_cancel', 'throw_techable', 'interruptible_on_opponent_turn', 'update_facing_on_exit', 'dynamic_iasa', 'backdash_iasa', 'allow_framecheat', 'next_state_on_hold', 'next_state_on_hold_on_opponent_turn', 'combo_only', 'neutral_only', 'end_feint', 'usable_from_whiff_cancel_if_possible', 'count_projectile_hits', 'burst_cancellable', 'burstable', 'self_hit_cancellable', 'self_interruptable', 'reversible', 'instant_cancellable', 'force_feintable', 'can_feint_if_possible', 'selectable', 'always_usable', 'release_opponent_on_startup', 'release_opponent_on_exit', 'initiative_effect', 'apply_pushback', 'beats_backdash', 'can_be_counterhit', 'force_super_effect', 'show_in_menu', 'flip_icon', 'flip_with_facing']
        f += [(x, b(p[x])) for x in bools]
        f += [('super_effect', b(p['super_effect_'])), ('scale_combo_meter', b(p['scale_combo_meter_']))]
        f += [('type', i(p['type'])), ('air_type', i(p['air_type'])), ('busy_interrupt_type', i(p['busy_interrupt_type']))]
        f += [('landing_recovery', i(p['landing_recovery'])), ('min_land_cancel_frame', i(p['min_land_cancel_frame'])), ('iasa_at', str(ia)), ('iasa_on_hit', i(p['iasa_on_hit'])),
              ('initiative_startup_reduction_amount', i(p['initiative_startup_reduction_amount'])), ('no_collision_start_frame', i(p['no_collision_start_frame'])),
              ('no_collision_end_frame', i(p['no_collision_end_frame'])), ('tick_priority', i(p['tick_priority'])), ('super_level', i(p['super_level_'])),
              ('supers_used', i(p['supers_used_'])), ('super_freeze_ticks', i(p['super_freeze_ticks_'])),
              ('land_cancel_state', i(sidof(p['land_cancel_state']))), ('hold_restart', i(sidof(p['hold_restart']))),
              ('velocity_forward_meter_gain_multiplier', fxs(p['velocity_forward_meter_gain_multiplier'])), ('whiff_meter_gain_multiplier', fxs(p['whiff_meter_gain_multiplier'])),
              ('interrupt_from', nset(p['interrupt_from_string']) if p['selectable'] else nset('')), ('interrupt_into', nset(p['interrupt_into_string'])),
              ('hit_cancel_into', nset(p['hit_cancel_into_string'])), ('hit_cancel_exceptions', nset(p['hit_cancel_exceptions_string'])),
              ('interrupt_exceptions', nset(p['interrupt_exceptions_string'])), ('allowed_stances', nset(p['allowed_stances_string'])),
              ('title', '"%s"' % (p['title'] or k.split('/')[1]).replace('"', "'"))]
        if fr: f += [('interrupt_frames', 'if_%s_%s' % (cid, cn)), ('ninterrupt_frames', str(len(fr)))]
        L.append('static const charstate_def_t cs_%s_%s = {' % (cid, cn) + ', '.join('.%s = %s' % kv for kv in f) + '};')
    L.append('static const state_def_t states_%s[] = {' % cid)
    for n, k in enumerate(states):
        sname = k.split('/')[1]; cn = cname(sname); nd = nodes[k]; p = nd['props']
        kids = [(kk, nodes[kk]) for kk in nodes if kk.startswith(k + '/') and kk.count('/') == 2]
        nhb = sum(1 for kk, v in kids if 'hitbox_type' in v['props'])
        nhs = sum(1 for kk, v in kids if 'start_tick' in v['props'] and 'hitbox_type' not in v['props'] and not (v.get('script') or '').endswith('LimbHurtbox.gd') and 'started' in v['props'])
        nlb = sum(1 for kk, v in kids if (v.get('script') or '').endswith('LimbHurtbox.gd'))
        anim = p['sprite_animation'] or sname
        ai = anim_names.index(anim) if anim in anim_names else -1
        salen = p['sprite_anim_length']
        if salen < 0: salen = len(sprite_anims[anim]['frames']) if ai >= 0 else p['anim_length']
        ef = p['enter_force_speed'] != '0.0'; tf = p['force_speed'] != '0.0'
        fm = lambda a, s: 'FX_RAW(%d)' % ((parse(str(a)) * parse(str(s))) >> 16)
        f = [('name', '"%s"' % sname), ('vt', '&%s' % scripts[k] if k in scripts else 'NULL'), ('kind', str(kind_of(root, nd.get('script'))))]
        for nm in ('apply_forces', 'apply_fric', 'apply_grav', 'reset_momentum', 'reset_x_momentum', 'reset_y_momentum', 'apply_custom_x_fric', 'apply_custom_y_fric',
                   'apply_custom_grav', 'apply_forces_no_limit', 'apply_custom_limits', 'loop_animation', 'absolute_loop', 'refresh_loop', 'endless', 'disable_at_end',
                   'force_same_direction_as_previous_state', 'spawn_particle_on_enter', 'projectile_match_facing', 'projectile_local_pos'):
            f.append((nm, b(p[nm])))
        for nm in ('custom_x_fric', 'custom_y_fric', 'custom_grav', 'custom_grav_max_fall_speed', 'custom_max_air_speed', 'custom_max_ground_speed'):
            f.append((nm, fxs(p[nm])))
        f += [('anim_length', i(p['anim_length'])), ('sprite_anim_length', i(salen)), ('ticks_per_frame', i(p['ticks_per_frame'])),
              ('animation_loop_start', i(p['animation_loop_start'])), ('fallback_state', i(sidof(p['fallback_state']))),
              ('force_tick', i(p['force_tick'])), ('has_force', b(tf)),
              ('has_enter_force', b(ef)),
              ('state_screenshake_tick', i(p['state_screenshake_tick'])), ('flip_frame', i(p['flip_frame'])), ('sfx_tick', i(p['sfx_tick'])),
              ('projectile_tick', i(p['projectile_tick'])), ('timed_spawn_particle_tick', i(p['timed_spawn_particle_tick'])),
              ('extra_parry_hitlag', i(p['extra_parry_hitlag'])), ('sprite_anim', str(ai)), ('earliest_hitbox', i(p['earliest_hitbox']))]
        if ef: f += [('enter_force_x', fm(p['enter_force_dir_x'], p['enter_force_speed'])), ('enter_force_y', fm(p['enter_force_dir_y'], p['enter_force_speed']))]
        if tf: f += [('force_x', fm(p['force_dir_x'], p['force_speed'])), ('force_y', fm(p['force_dir_y'], p['force_speed']))]
        if nhb: f += [('hb', 'hb_%s_%s' % (cid, cn)), ('nhb', str(nhb))]
        if nhs: f += [('hurt', 'hs_%s_%s' % (cid, cn)), ('nhurt', str(nhs))]
        if nlb: f += [('limb', 'lb_%s_%s' % (cid, cn)), ('nlimb', str(nlb))]
        if 'iasa_at' in p: f += [('cs', '&cs_%s_%s' % (cid, cn))]
        if 'parry_type' in p and 'can_parry' in p:
            f += [('can_parry', b(p['can_parry'])), ('parry_type', i(p['parry_type'])), ('push', b(p.get('push'))), ('use_guard_sprites', b(p.get('use_guard_sprites'))),
                  ('autoguard', b(p.get('autoguard'))), ('disable_aerial_movement', b(p.get('disable_aerial_movement'))), ('reblock', b(p.get('reblock')))]
            pl = p.get('particle_location') or {}
            a = pl.get('a', [0, 0]) if isinstance(pl, dict) else [0, 0]
            f += [('particle_x', str(int(a[0]) if len(a) > 0 else 0)), ('particle_y', str(int(a[1]) if len(a) > 1 else 0))]
        f += [('name_id', str(n))]
        L.append(' {' + ', '.join('.%s = %s' % kv for kv in f) + '},')
    L.append('};')
    root = nodes['.']['props']
    hb = nodes.get('Hurtbox', {}).get('props', {}); cb = nodes.get('CollisionBox', {}).get('props', {})
    bx = lambda q: '{%s, %s, %s, %s, 0, 0, 1}' % (i(q.get('x', 0)), i(q.get('y', 0)), i(q.get('width', 0)), i(q.get('height', 0)))
    L.insert(L.index('static const state_def_t states_%s[] = {' % cid), 'static const char *const names_%s[] = {%s};' % (cid, ','.join('"%s"' % x for x in names)))
    L.append('const chardata_t chardata_%s = {"%s", %d, states_%s, %d, anims_%s, %d, names_%s, %s, %s, %s, %s, %s, %s, %s, %s};' % (
        cid, cid, len(states), cid, len(anim_names), cid, len(names), cid, fxs(root['gravity']), fxs(root['ground_friction']), fxs(root['air_friction']),
        fxs(root['max_ground_speed']), fxs(root['max_air_speed']), fxs(root['max_fall_speed']), bx(hb), bx(cb)))
    open(out, 'w').write('\n'.join(L) + '\n')
    print('%s: %d estados, %d animacoes, %d bytes de C' % (cid, len(states), len(anim_names), os.path.getsize(out)))

if __name__ == '__main__':
    main(*sys.argv[1:5])
