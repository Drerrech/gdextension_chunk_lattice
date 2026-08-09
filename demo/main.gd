extends Node

const c_shape = Vector3i(8, 8, 8)
const c_cube_size = Vector3(1, 1, 1)
const l_type = 0
const l_seed = 42

@onready var CL: ChunkLattice = $ChunkLattice
@onready var m_spawner = $MultiplayerSpawner

@onready var ui_interface = $Node2D

func _ready() -> void:
	multiplayer.peer_connected.connect(_peer_connected)
	multiplayer.peer_disconnected.connect(_peer_disconnected)

func start_server() -> void:
	print("starting server")
	CL.setup("world", c_shape, c_cube_size, l_type, l_seed)
	$server_camera/Camera3D.make_current()
	Input.set_mouse_mode(Input.MOUSE_MODE_CAPTURED)
	var peer = ENetMultiplayerPeer.new()
	peer.create_server(8998)
	multiplayer.multiplayer_peer = peer
	ui_interface.visible = false
	#m_spawner.request_spawn({"type": "player", "id": multiplayer.get_unique_id(), "glob_pos": Vector3(0, 5, 0)})

func start_client() -> void:
	CL.setup("client_empty", c_shape, c_cube_size, l_type, l_seed)
	var peer = ENetMultiplayerPeer.new()
	peer.create_client("127.0.0.1", 8998)
	multiplayer.multiplayer_peer = peer
	ui_interface.visible = false
	# spawned via _peer_connected

func _peer_connected(id: int) -> void:
	if not multiplayer.is_server(): return
	print("peer connected ", id)
	print("my id: ", multiplayer.get_unique_id())
	m_spawner.rpc_spawn.rpc_id(1, {"type": "player", "id": id, "glob_pos": Vector3(0, 5, 0)})

func _peer_disconnected(id: int) -> void:
	if not multiplayer.is_server(): return
	print("peer disconnected ", id)
	var player = m_spawner.get_node_or_null(str(id))
	if player:
		player.queue_free()

# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta: float) -> void:
	pass


func _on_server_button_down() -> void:
	start_server()


func _on_client_button_down() -> void:
	start_client()
