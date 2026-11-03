extends SceneTree
# Executa sequencias de comandos em FGObject. Cada linha: "new" | "call metodo arg..." | "data".
# Args com prefixo "i" sao int; senao string. Imprime get_data() depois de cada "call"/"data".
func _init():
	var inp = File.new(); inp.open(OS.get_environment("PH_IN"), File.READ)
	var out = File.new(); out.open(OS.get_environment("PH_OUT"), File.WRITE)
	var o = null
	while not inp.eof_reached():
		var line = inp.get_line().strip_edges()
		if line == "" or line.begins_with("#"): continue
		var p = line.split(" ")
		if p[0] == "new":
			o = load("res://bin/FGObject.gdns").new()
			out.store_line("NEW")
			continue
		if p[0] == "call":
			var args = []
			for i in range(2, p.size()):
				var a = p[i]
				if a.begins_with("i") and a.substr(1).is_valid_integer(): args.append(int(a.substr(1)))
				elif a == "true": args.append(true)
				elif a == "false": args.append(false)
				else: args.append(a)
			var r = o.callv(p[1], args)
			out.store_line("CALL " + line.substr(5) + " -> " + str(r))
		if p[0] == "call" or p[0] == "data":
			out.store_line("DATA " + to_json(o.get_data()))
	out.close()
	quit()
