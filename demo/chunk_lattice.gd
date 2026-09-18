extends ChunkLattice

@onready var Main = get_tree().root.get_node("main")

var _t_start := 0
var _timing := true
var _last_count := -1
var _idle_frames := 0

func start_load_timer() -> void:
	_t_start = Time.get_ticks_msec()
	_timing = true
	_last_count = -1
	_idle_frames = 0

func _process(_delta):
	work_through_queues()
	if not _timing: return
	var c = get_child_count()
	if c != _last_count:
		_last_count = c
		_idle_frames = 0
	else:
		_idle_frames += 1
		if _idle_frames >= 30 and c > 0:
			print("full load: ", Time.get_ticks_msec() - _t_start, " ms, ", c, " chunks")
			_timing = false
