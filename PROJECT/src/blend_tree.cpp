#include "blend_tree.h"

#include "anim_system_3d.h"

// == BLEND TREE ====================================

void BlendTree::init(AnimSystem3D *anim_system) {
	arena.setAllocator(g_alloc);
	system = anim_system;
	mesh = system->getSkinnedMesh();
}

void BlendTree::cleanup() {
	arena.cleanup();
	exit_node = nullptr;
	mesh = nullptr;
	value_map.clear();
	all_nodes.destroy();
}

void BlendTree::update(float delta_time) {
	if (!exit_node) {
		return;
	}

	static float t = 0.f;
	t += delta_time;

	setValue("sintime", sinf(t));
	setValue("sintime fast", sinf(t * 3.f));
	setValue("sintime slow", sinf(t / 3.f));

	setValue("normtime", (sinf(t) + 1.f) / 2.f);
	setValue("normtime fast", (sinf(t * 3.f) + 1.f) / 2.f);
	setValue("normtime slow", (sinf(t / 3.f) + 1.f) / 2.f);

	//setValue("blend", alpha);
	//setValue("blend2", alpha);

	exit_node->update(delta_time);
	mesh->UpdateBoneMatrices(exit_node->output);
}

void BlendTree::read(FILE *fp) {
	if (!fp) return;

	uint8_t nodes_count = 0;
	uint8_t map_count    = 0;
	uint8_t exit_node_id = 0;
	fileRead(nodes_count, fp);
	fileRead(map_count,    fp);
	fileRead(exit_node_id, fp);

	all_nodes.reserve(nodes_count);
	value_map.reserve(map_count);

	struct ToAdd {
		uint8_t a, b, c;
		bool is_tree = false;
		ITreeNode *node;
	};
	gef::Vec<ToAdd> to_add;

	for (uint8_t i = 0; i < nodes_count; ++i) {
		ITreeNode *new_node = nullptr;
		NodeType type = NodeType::Base;
		fileRead(type, fp);
		switch (type) {
		case NodeType::Clip:
		{
			ClipNode *node = arena.make<ClipNode>(*this);
			uint8_t clip_id = 0;
			fileRead(clip_id, fp);
			node->clip = system->getAnimation((int)clip_id);
			new_node = node;
			break;
		}
		case NodeType::SyncClip:
		{
			SyncedClipNode *node = arena.make<SyncedClipNode>(*this);
			uint8_t clip_id, lead_id;
			fileRead(clip_id, fp);
			fileRead(lead_id, fp);
			node->clip = system->getAnimation((int)clip_id);
			node->leader_clip = system->getAnimation((int)lead_id);
			new_node = node;
			break;
		}
		case NodeType::Blend:
		{
			BlendNode *node = arena.make<BlendNode>(*this);
			ToAdd add{};
			fileRead(node->blending_value, fp);
			fileRead(add.a, fp);
			fileRead(add.b, fp);
			add.node = node;
			to_add.emplace_back(add);
			new_node = node;
			break;
		}
		case NodeType::Blend1D:
		{
			BlendNode1D *node = arena.make<BlendNode1D>(*this);
			ToAdd add{};
			fileRead(node->blending_value, fp);
			fileRead(add.a, fp);
			fileRead(add.b, fp);
			fileRead(add.c, fp);
			add.node = node;
			add.is_tree = true;
			to_add.emplace_back(add);
			new_node = node;
			break;
		}
		}
		assert(new_node);
		all_nodes.emplace_back(new_node);
	}

	// we need to do this later as the node might not have been loaded yet
	for (ToAdd &add : to_add) {
		add.node->input_nodes.emplace_back(all_nodes[add.a]);
		add.node->input_nodes.emplace_back(all_nodes[add.b]);
		if (add.is_tree) {
			add.node->input_nodes.emplace_back(all_nodes[add.c]);
		}
	}

	for (uint8_t i = 0; i < map_count; ++i) {
		uint8_t namelen, node_id;
		std::string name;
		fileRead(node_id, fp);
		fileRead(namelen, fp);
		name.resize(namelen);
		fileRead(name, fp);
		bindValue(name, all_nodes[node_id]);
	}

	exit_node = all_nodes[exit_node_id];
}

void BlendTree::save(FILE *fp) const {
	if (!fp) return;

	size_t exit_node_id = all_nodes.find(exit_node);
	assert(exit_node_id != SIZE_MAX);

	fileWrite((uint8_t)all_nodes.size(), fp);
	fileWrite((uint8_t)value_map.size(), fp);
	fileWrite((uint8_t)exit_node_id,     fp);

	for (const ITreeNode *base_node : all_nodes) {
		fileWrite(base_node->node_type, fp);
		switch (base_node->node_type) {
		case NodeType::Clip:
		{
			ClipNode *node = (ClipNode *)base_node;
			int clip_id = system->getAnimationId(node->clip);
			assert(clip_id != INVALID_ID);
			fileWrite((uint8_t)clip_id, fp);
			break;
		}
		case NodeType::SyncClip:
		{
			SyncedClipNode *node = (SyncedClipNode *)base_node;
			int clip_id = system->getAnimationId(node->clip);
			int lead_id = system->getAnimationId(node->leader_clip);
			assert(clip_id != INVALID_ID && lead_id != INVALID_ID);
			fileWrite((uint8_t)clip_id, fp);
			fileWrite((uint8_t)lead_id, fp);
			break;
		}
		case NodeType::Blend:
		{
			BlendNode *node = (BlendNode *)base_node;
			size_t inp_0 = all_nodes.find(node->input_nodes[0]);
			size_t inp_1 = all_nodes.find(node->input_nodes[1]);
			assert(inp_0 != SIZE_MAX && inp_1 != SIZE_MAX);
			fileWrite(node->blending_value, fp);
			fileWrite((uint8_t)inp_0, fp);
			fileWrite((uint8_t)inp_1, fp);
			break;
		}
		case NodeType::Blend1D:
		{
			BlendNode1D *node = (BlendNode1D *)base_node;
			size_t right = all_nodes.find(node->input_nodes[0]);
			size_t centre = all_nodes.find(node->input_nodes[1]);
			size_t left = all_nodes.find(node->input_nodes[2]);
			assert(right != SIZE_MAX && centre != SIZE_MAX && left != SIZE_MAX);
			fileWrite(node->blending_value, fp);
			fileWrite((uint8_t)right, fp);
			fileWrite((uint8_t)centre, fp);
			fileWrite((uint8_t)left, fp);
			break;
		}
		}
	}

	for (const auto &[name, ptr] : value_map) {
		size_t node_id = 0;
		for (size_t i = 0; i < all_nodes.size(); ++i) {
			if (all_nodes[i]->getInputValue() == ptr) {
				node_id = i;
				break;
			}
		}
		assert(node_id != SIZE_MAX);
		fileWrite((uint8_t)node_id, fp);
		fileWrite((uint8_t)name.size(), fp);
		fileWrite(name,    fp);
	}
}

bool BlendTree::bindValue(const std::string &name, ITreeNode *node) {
	auto &it = value_map.find(name);
	if (it == value_map.end()) {
		if (float *value = node->getInputValue()) {
			value_map[name] = value;
			return true;
		}
		else {
			warn("trying to bind value with name %s but the node doesn't have a bindable value", name.c_str());
		}
	}
	else {
		warn("trying to bind value with name %s but the value is already binded", name.c_str());
	}
	return false;
}

bool BlendTree::setValue(const std::string &name, float value) {
	auto &it = value_map.find(name);
	if (it != value_map.end()) {
		if (it->second) {
			*it->second = value;
			return true;
		}
	}
	return false;
}

float BlendTree::getValue(const std::string &name) {
	auto &it = value_map.find(name);
	if (it != value_map.end() && it->second) {
		return *it->second;
	}
	return 0.f;
}

// == TREE NODE =====================================

ITreeNode::ITreeNode(BlendTree &tree)
	: tree(tree),
	  output(&tree.arena)
{
	assert(tree.mesh);
	output = tree.mesh->bind_pose();
	input_nodes.setAllocator(&tree.arena);
}

// == CLIP NODE =====================================

ClipNode::ClipNode(BlendTree &tree)
	: ITreeNode(tree) 
{
	node_type = NodeType::Clip;
}

void ClipNode::update(float delta_time) {
	if (!clip) {
		return;
	}

	// clip node should not have any input
	assert(input_nodes.empty());

	clip->update(delta_time, output, tree.mesh->bind_pose());
}

float *ClipNode::getInputValue() {
	return clip ? &clip->timer : nullptr;
}

// == SYNCED CLIP NODE ==============================

SyncedClipNode::SyncedClipNode(BlendTree &tree)
	: ClipNode(tree) 
{
	node_type = NodeType::SyncClip;
}

void SyncedClipNode::update(float delta_time) {
	if (!clip) return;
	if (!leader_clip) return ClipNode::update(delta_time);

	// synced clip node should not have any input
	assert(input_nodes.empty());

	float norm = leader_clip->timer / leader_clip->duration;
	clip->timer = clip->duration * norm;

	clip->updatePose(output, tree.mesh->bind_pose());
}

// == BLEND NODE ====================================

BlendNode::BlendNode(BlendTree &tree)
	: ITreeNode(tree) 
{
	node_type = NodeType::Blend;
}

void BlendNode::update(float delta_time) {
	if (input_nodes.size() != 2) {
		return;
	}

	for (ITreeNode *node : input_nodes) {
		node->update(delta_time);
	}

	output.Linear2PoseBlend(
		input_nodes[0]->output,
		input_nodes[1]->output,
		blending_value
	);
}

// == BLEND 1D NODE =================================

BlendNode1D::BlendNode1D(BlendTree &tree)
	: ITreeNode(tree) 
{
	node_type = NodeType::Blend1D;
}

void BlendNode1D::update(float delta_time) {
	if (input_nodes.size() != 3) {
		return;
	}

	for (ITreeNode *node : input_nodes) {
		node->update(delta_time);
	}

	if (blending_value > 0) {
		output.Linear2PoseBlend(
			input_nodes[1]->output,
			input_nodes[2]->output,
			blending_value
		);
	}
	else if (blending_value < 0) {
		output.Linear2PoseBlend(
			input_nodes[1]->output,
			input_nodes[0]->output,
			fabsf(blending_value)
		);
	}
	else {
		output = input_nodes[1]->output;
	}
}
