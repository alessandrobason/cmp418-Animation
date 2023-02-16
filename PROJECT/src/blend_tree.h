#pragma once

#include <unordered_map>
#include <string>

#include <animation/skeleton.h>
#include <maths/vector2.h>
#include <system/ptr.h>
#include <system/vec.h>

#include "arena.h"

namespace gef {
	class SkinnedMeshInstance;
} // namespace gef

struct ITreeNode;
struct Animation3D;
class AnimSystem3D;

struct BlendTree {
	void init(AnimSystem3D *anim_system);
	void cleanup();
	void update(float delta_time);

	void read(FILE *fp);
	void save(FILE *fp) const;

	bool bindValue(const std::string &name, ITreeNode *node);
	bool setValue(const std::string &name, float value);
	float getValue(const std::string &name);

	Arena arena;
	AnimSystem3D *system = nullptr;
	gef::SkinnedMeshInstance *mesh = nullptr;
	ITreeNode *exit_node = nullptr;
	gef::Vec<ITreeNode *> all_nodes = &arena;
	std::unordered_map<std::string, float *> value_map;
};

enum class NodeType : uint8_t {
	Base, Clip, SyncClip, Blend, Blend1D, Count
};

struct ITreeNode {
	ITreeNode(BlendTree &tree);
	virtual ~ITreeNode() {}
	virtual void update(float delta_time) = 0;
	virtual float *getInputValue() { return nullptr; }

	BlendTree &tree;
	gef::SkeletonPose output;
	gef::Vec<ITreeNode *> input_nodes;
	NodeType node_type = NodeType::Base;
};

// plays a clip
// no inputs
struct ClipNode : public ITreeNode {
	ClipNode(BlendTree &tree);
	virtual void update(float delta_time) override;
	virtual float *getInputValue();

	Animation3D *clip = nullptr;
};

// syncs to animation clips so "clip" runs for the same time as "leader_clip"
// no inputs
struct SyncedClipNode : public ClipNode {
	SyncedClipNode(BlendTree &tree);
	virtual void update(float delta_time) override;

	Animation3D *leader_clip = nullptr;
};

// linearly interpolates between two inputs based on a blending value, range (0, 1)
// 2 inputs
struct BlendNode : public ITreeNode {
	BlendNode(BlendTree &tree);
	virtual void update(float delta_time) override;
	virtual float *getInputValue() { return &blending_value; }

	float blending_value = 0.5f;
};

// linearly interpolates between two of the tree inputs based on a blending value, range (-1, 1)
// if blending value < 0 it interpolates between the first and second input
// if blending value > 0 it interpolates between the second and third input
// 3 inputs
struct BlendNode1D : public ITreeNode {
	BlendNode1D(BlendTree &tree);
	virtual void update(float delta_time) override;
	virtual float *getInputValue() { return &blending_value; }

	float blending_value = 0.5f;
};
