#include "chunk.h"

#include "constants.h"
#include "raw_generation.h"

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void Chunk::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_idx", "i", "j", "k"), &Chunk::get_idx);
    ClassDB::bind_method(D_METHOD("set_data"), &Chunk::set_data);
    ClassDB::bind_method(D_METHOD("set_mesh_data"), &Chunk::set_mesh_data);
    ClassDB::bind_method(D_METHOD("assign_mesh"), &Chunk::assign_mesh);
    ClassDB::bind_method(D_METHOD("assign_generated_collision"), &Chunk::assign_generated_collision);
    ClassDB::bind_method(D_METHOD("get_point_changes"), &Chunk::get_point_changes);
    ClassDB::bind_method(D_METHOD("get_occupants"), &Chunk::get_occupants);
}

Chunk::Chunk() {
    // attributes
	file_world_name = default_file_world_name;
    chunk_shape = default_chunk_shape;
	chunk_cube_size = default_chunk_cube_size;
    lattice_type = default_lattice_type;
	lattice_seed = default_lattice_seed;
	int _num_elems = chunk_shape.x * chunk_shape.y * chunk_shape.z;
	point_fullness_values.resize(_num_elems);
	point_material_values.resize(_num_elems);
    // the changes will be read to in a function call
}

Chunk::~Chunk() {
    write_changes();
}

void Chunk::setup(String p_file_world_name, Vector3i p_chunk_idx, Vector3i p_chunk_shape, Vector3 p_chunk_cube_size, int p_lattice_type, int p_lattice_seed) {
	file_world_name = p_file_world_name;
    chunk_idx = p_chunk_idx;
    global_pos = get_global_position();
	chunk_shape = p_chunk_shape;
	chunk_cube_size = p_chunk_cube_size;
    lattice_type = p_lattice_type;
    lattice_seed = p_lattice_seed;
	int _num_elems = chunk_shape.x * chunk_shape.y * chunk_shape.z;
	point_fullness_values.resize(_num_elems);
	point_material_values.resize(_num_elems);

    // file path
    String chunk_file_name = vformat("%d_%d_%d.bin", p_chunk_idx.x, p_chunk_idx.y, p_chunk_idx.z); // poor person who makes a really small chunk lol TODO maybe use lattice_name+chunk_idx pattern instead to let lattices move
    abstract_file_path = String("user://").path_join(file_world_name).path_join("chunk_changes").path_join(chunk_file_name);

    // children
	mesh_instance_ptr = memnew(MeshInstance3D);
    mesh_resource.instantiate();
    mesh_instance_ptr->set_mesh(mesh_resource);
    add_child(mesh_instance_ptr);
	
	collision_shape_instance_ptr = memnew(CollisionShape3D);
    collision_shape_resource.instantiate();
    collision_shape_instance_ptr->set_shape(collision_shape_resource);
    add_child(collision_shape_instance_ptr);
}

int Chunk::get_idx(int i, int j, int k) {
    return i * chunk_shape.y * chunk_shape.z + j * chunk_shape.z + k;
}

void Chunk::set_data() {
    // terrain and props
    set_chunk_raw_data(this);

    // point changes
    read_point_changes_into_hash(); // will be set after popping from queue
}

// file structure:
// [idxs array]
// [values array]
// [materials array] all solid blocks, and we know the num elements of each _num_triplets
void Chunk::read_point_changes_into_hash() {
    if (file_world_name == client_file_world_name_flag) return;
    
    static constexpr int BYTES_PER_TRIPLET = sizeof(int32_t) + sizeof(float) + sizeof(uint8_t);
    
    Ref<FileAccess> f = FileAccess::open(abstract_file_path, FileAccess::READ);
    if (!f.is_null()) {
        if (f->get_length() % BYTES_PER_TRIPLET != 0) {
            UtilityFunctions::printerr("corrupt chunk change file: ", abstract_file_path);
            return;
        }
        uint64_t _num_triplets = f->get_length() / BYTES_PER_TRIPLET;

        PackedInt32Array _changes_idx = (f->get_buffer(_num_triplets * sizeof(int32_t))).to_int32_array();
        PackedFloat32Array _changes_fullness = (f->get_buffer(_num_triplets * sizeof(float))).to_float32_array();
        PackedByteArray _changes_material = f->get_buffer(_num_triplets); // already a packed byte array

        add_point_hash_changes(_changes_idx, _changes_fullness, _changes_material);
    }
    // file does not exist, we can leave the changes empty
}

void Chunk::write_changes() {
    if (file_world_name == client_file_world_name_flag) return;

    if (point_changes.is_empty()) return; // no point in creating an empty file

    Ref<FileAccess> f = FileAccess::open(abstract_file_path, FileAccess::WRITE);
    if (f.is_null()) {
        UtilityFunctions::printerr("failed to open for writing: ", abstract_file_path);
        return;
    }
    
    // convert hash map to packed arrays
    uint64_t _size = point_changes.size();
    PackedInt32Array _changes_idx; _changes_idx.resize(_size);
    PackedFloat32Array _changes_fullness; _changes_fullness.resize(_size);
    PackedByteArray _changes_material; _changes_material.resize(_size);

    int64_t i = 0;
    for (const KeyValue<int32_t, PointChange> &kv : point_changes) {
        _changes_idx.set(i, kv.key);
        _changes_fullness.set(i, kv.value.fullness);
        _changes_material.set(i, kv.value.material);
        i++;
    }
    
    f->store_buffer(_changes_idx.to_byte_array()); // TODO if a game crashes on the server the files will be corrupt... atomic write needed
    f->store_buffer(_changes_fullness.to_byte_array());
    f->store_buffer(_changes_material);
}


void Chunk::add_point_hash_changes(PackedInt32Array idxs, PackedFloat32Array fullness_values, PackedByteArray material_values) {
    for (int64_t i = 0; i < (int64_t)idxs.size(); i++) {
        // convert to hash map
        point_changes[idxs[i]] = {fullness_values[i], material_values[i]};
    }
}

void Chunk::apply_point_hash_changes() {
    // apply changes to the array
    for (const KeyValue<int32_t, PointChange> &kv : point_changes) {
        point_fullness_values.set(kv.key, kv.value.fullness);
        point_material_values.set(kv.key, kv.value.material);
    }
}

// TODO apply prop changes and such

int Chunk::get_triangulation_idx(int x, int y, int z) {
	int idx = 0b00000000;
    idx |= (point_fullness_values[get_idx(x, y, z)] > ISO_LEVEL) << 0;
    idx |= (point_fullness_values[get_idx(x, y, z+1)] > ISO_LEVEL) << 1;
    idx |= (point_fullness_values[get_idx(x+1, y, z+1)] > ISO_LEVEL) << 2;
	idx |= (point_fullness_values[get_idx(x+1, y, z)] > ISO_LEVEL) << 3;
	idx |= (point_fullness_values[get_idx(x, y+1, z)] > ISO_LEVEL) << 4;
	idx |= (point_fullness_values[get_idx(x, y+1, z+1)] > ISO_LEVEL) << 5;
	idx |= (point_fullness_values[get_idx(x+1, y+1, z+1)] > ISO_LEVEL) << 6;
	idx |= (point_fullness_values[get_idx(x+1, y+1, z)] > ISO_LEVEL) << 7;
	
	return idx;
}

void Chunk::set_mesh_data() {
    vertex_positions.clear();
    PackedVector3Array vertex_normals; // TODO: calling resize first is faster, but we can't know in advance, not sure if max size memory <-> trade is worth it
    PackedFloat32Array custom0;
    
    // marching cubes
    for (int x_idx = 0; x_idx < chunk_shape.x-1; x_idx++) {
		for (int y_idx = 0; y_idx < chunk_shape.y-1; y_idx++) {
			for (int z_idx = 0; z_idx < chunk_shape.z-1; z_idx++) {
                int triangulation_idx = get_triangulation_idx(x_idx, y_idx, z_idx);

                // adding verticies
                for (int i = 0; i < 16; i++) {
                    int edge_idx = TRIANGULATIONS[triangulation_idx][i];
                    if (edge_idx == -1) break;

                    const int *point_idx = EDGES[edge_idx];

                    const int *p0 = POINTS[point_idx[0]];
                    const int *p1 = POINTS[point_idx[1]];

                    int p0_idx = get_idx(x_idx + p0[0], y_idx + p0[1], z_idx + p0[2]);
                    float p0_fullness_val = point_fullness_values[p0_idx];
                    uint8_t p0_material = point_material_values[p0_idx];
                    
                    int p1_idx = get_idx(x_idx + p1[0], y_idx + p1[1], z_idx + p1[2]);
                    float p1_fullness_val = point_fullness_values[p1_idx];
                    uint8_t p1_material = point_material_values[p1_idx];

                    // interpolate and multiply vector by cell size (identical in logic, but less repetition)
                    float t = (ISO_LEVEL - p0_fullness_val) / (p1_fullness_val - p0_fullness_val);
                    Vector3 p_inter_pos(
                        (x_idx + p0[0] + t * (p1[0] - p0[0])) * chunk_cube_size.x,
                        (y_idx + p0[1] + t * (p1[1] - p0[1])) * chunk_cube_size.y,
                        (z_idx + p0[2] + t * (p1[2] - p0[2])) * chunk_cube_size.z
                    );

                    // custom0: [material_idx, ?, ?, ?]
                    float custom0_material_idx = t < 0.5f ? p0_material : p1_material;
                    float custom0_idk1 = 0.f;
                    float custom0_idk2 = 0.f;
                    float custom0_idk3 = 0.f;

                    // add vertex position
                    vertex_positions.append(p_inter_pos);
                    // add vertex custom0
                    custom0.append(custom0_material_idx); custom0.append(custom0_idk1); custom0.append(custom0_idk2); custom0.append(custom0_idk3);
                }
            }
        }
    }

    // compute normals
    vertex_normals.resize(vertex_positions.size());
    for (int64_t i = 0; i < (int64_t)vertex_positions.size(); i += 3) {
        Vector3 n = -(vertex_positions[i+1] - vertex_positions[i]).cross(vertex_positions[i+2] - vertex_positions[i]).normalized();
        vertex_normals.set(i,     n);
        vertex_normals.set(i + 1, n);
        vertex_normals.set(i + 2, n);
    }

    // fill resources
    mesh_arrays.resize(Mesh::ARRAY_MAX);

    mesh_arrays[Mesh::ARRAY_VERTEX]  = vertex_positions;
    mesh_arrays[Mesh::ARRAY_NORMAL]  = vertex_normals;
    mesh_arrays[Mesh::ARRAY_CUSTOM0] = custom0;
}

void Chunk::assign_mesh() {
    mesh_resource_ready = true;
    mesh_resource->clear_surfaces();

    // the mesh is clear now, if there is nothing do add it will stay that way
    if (vertex_positions.is_empty()) return;

    mesh_resource->add_surface_from_arrays(
        Mesh::PRIMITIVE_TRIANGLES,
        mesh_arrays,
        TypedArray<Array>(),   // blend_shapes (none)
        Dictionary(),          // lods (none)
        Mesh::ARRAY_CUSTOM_RGBA_FLOAT << Mesh::ARRAY_FORMAT_CUSTOM0_SHIFT
    );
}

void Chunk::assign_generated_collision() {
    // for collsiion the guard is not needed because to make an empty one we just pass in an empty packed array
    collision_shape_resource->set_faces(vertex_positions);
    set_collision = true;
}

Dictionary Chunk::get_point_changes() {
    uint64_t _size = point_changes.size();
    PackedInt32Array _changes_idx; _changes_idx.resize(_size);
    PackedFloat32Array _changes_fullness; _changes_fullness.resize(_size);
    PackedByteArray _changes_material; _changes_material.resize(_size);

    int64_t i = 0;
    for (const KeyValue<int32_t, PointChange> &kv : point_changes) {
        _changes_idx.set(i, kv.key);
        _changes_fullness.set(i, kv.value.fullness);
        _changes_material.set(i, kv.value.material);
        i++;
    }
    
    Dictionary d;
    d[String("changes_idx")] = _changes_idx;
    d[String("changes_fullness")] = _changes_fullness;
    d[String("changes_material")] = _changes_material;

    return d;
}

Array Chunk::get_occupants() {
    Array arr;

    for (KeyValue<ObjectID, LoaderAttributes> &kv : occupants) {
        Object* loader = ObjectDB::get_instance(kv.key);
        if (loader == nullptr) {
            UtilityFunctions::printerr("nulled loader was not removed from occupants");
            return Array();
        }
        
        Dictionary d;
        d[String("loader")] = loader;
        d[String("collision")] = kv.value.collision;
        d[String("peer_id")] = kv.value.peer_id;
        arr.append(d);
    }
    
    return arr;
}