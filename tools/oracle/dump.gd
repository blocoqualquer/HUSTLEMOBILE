extends SceneTree
func walk(n, path, out):
	for c in n.get_children():
		var p = c.name if path == "" else path + "/" + c.name
		var d = {}
		if "damage" in c and "hitstun_ticks" in c:
			for k in ["damage","damage_in_combo","hitstun_ticks","victim_hitlag","hitlag_ticks","dir_x","dir_y","knockback","start_tick","active_ticks","x","y","width","height","plus_frames","hit_height"]:
				d[k] = c.get(k)
		elif "anim_length" in c:
			for k in ["anim_length","iasa_at","title","type","fallback_state","interrupt_into_string","air_type"]:
				if k in c: d[k] = c.get(k)
		if d.size() > 0: out[p] = d
		walk(c, p, out)
func _init():
	var scene_path = OS.get_environment("DUMP_SCENE")
	var s = load(scene_path).instance()
	var out = {}
	walk(s, "", out)
	var f = File.new()
	f.open(OS.get_environment("DUMP_OUT"), File.WRITE)
	f.store_string(to_json(out))
	f.close()
	quit()
