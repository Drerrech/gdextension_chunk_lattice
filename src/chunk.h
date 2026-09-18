#ifndef CHUNK_H
#define CHUNK_H

#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/concave_polygon_shape3d.hpp>
#include <godot_cpp/templates/hash_set.hpp>
#include <godot_cpp/classes/file_access.hpp>

namespace godot {
/*
Chunk

*/
class Chunk : public StaticBody3D{
	GDCLASS(Chunk, StaticBody3D)

private:


protected:
	static void _bind_methods();

public:
	String file_world_name;
	String abstract_file_path;

	Vector3i chunk_idx;
	Vector3 global_pos;

	Vector3i chunk_shape;
	Vector3 chunk_cube_size;
	int lattice_type;
	int lattice_seed;

	struct LoaderAttributes {
        bool collision;
		int peer_id;
    };
	HashMap<ObjectID, LoaderAttributes> occupants;

	PackedFloat32Array point_fullness_values; // [-1, 1]
	PackedByteArray point_material_values; // {0, 1, ... 255}
	struct PointChange {
        float fullness;
        uint8_t material;
    };
    HashMap<int32_t, PointChange> point_changes;

	// children and their resources
	PackedVector3Array vertex_positions; // needed for collision
	Array mesh_arrays; // vertex normal and custom
	MeshInstance3D *mesh_instance_ptr;
	Ref<ArrayMesh> mesh_resource;

	bool mesh_resource_ready = false;
	bool set_collision = false;
	CollisionShape3D *collision_shape_instance_ptr;
	Ref<ConcavePolygonShape3D> collision_shape_resource;

	Chunk();
	~Chunk();

	void setup(String p_file_world_name, Vector3i p_chunk_idx, Vector3i p_chunk_shape, Vector3 p_chunk_cube_size, int p_lattice_type, int p_lattice_seed);

	int get_idx(int i, int j, int k);

	void set_data();
	
	void read_point_changes_into_hash();

	void write_changes();
	
	void add_point_hash_changes(PackedInt32Array idxs, PackedFloat32Array fullness_values, PackedByteArray material_values);
	void apply_point_hash_changes();

	int get_triangulation_idx(int x, int y, int z);

	void set_mesh_data(); // marches cubes, does not assign resources (mesh_arrays)
	void assign_mesh(); // assigns the vertex positions and custom0 data to the mesh

	void assign_generated_collision(); // creates a ConcavePolygonShape3D with the array_vertex and assigns it to the child

	Dictionary get_point_changes();
	
	Array get_occupants();
};

}

#endif