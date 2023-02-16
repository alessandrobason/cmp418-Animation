#pragma once

#include <unordered_map>
#include <string>

#include <graphics/texture.h>
#include <graphics/sprite.h>
#include <maths/vector2.h>
#include <maths/matrix33.h>
#include <maths/matrix44.h>
#include <system/vec.h>

#include "anim_system.h"
#include "arena.h"
#include "rect.h"

namespace gef {
	class Platform;
	class Font;
	class InputManager;
}

struct Batch2D;
struct Rect;
struct SubTexture;
struct Bone;
struct Skin;
struct Slot;
struct AnimationKey;
struct AnimationRotation;
struct AnimationTranslation;
struct BoneAnimation;
struct AnimationSke2D;

/*
when removing bones/animations the data is neither freed nor reused as it is using the arena 
allocator. this isn't too much of a problem unless a lot of nodes are created/destroyed.
one way to fix it would be to put the freed nodes in a "freed" linked list and just pick up
the first one every time we need a new node.
e.g.
Bone *freed_bones;
void destroyBone(Bone *bone) {
	Bone *before = findBefore(bone);
	before->next = bone->next;
	bone->next = free_bones;
	freed_bones = bone;
}
Bone *getNewBone() {
	if (freed_bones) {
		Bone *new_bone = freed_bones;
		freed_bones = new_bone->next;
		new_bone->next = bones_head;
		bone_head = new_bone;
		return new_bone;
	}
	else {
		return arena.make<Bone>();
	}
}
*/

struct Atlas {
	Atlas(IAllocator *alloc);
	gef::Texture *texture;
	gef::Vec<SubTexture> sub_textures;
};

class AnimSystemSke2D : public AnimSystem {
public:
	virtual bool update(float delta_time) override;
	virtual void draw() override;
	virtual void debugDraw() override;
	virtual void setAnimation(const char *name) override;
	virtual void setAnimation(int id) override;
	virtual gef::Transform getTransform() const override;
	virtual void setTransform(const gef::Transform &tran) override;
	virtual void read(FILE *fp) override;
	virtual void save(FILE *fp) const override;
	virtual bool checkFormatVersion(FILE *fp) override;

	void init(Batch2D *sprite_batcher, gef::Platform &plat);
	void cleanup();
	void loadFromDragonBones(const char *tex_file, const char *ske_file);

	AnimationSke2D *getAnimation(int id);
	gef::Matrix33 getMatFromAnimatedBone(Bone *bone);
	gef::Matrix33 getBaseTransform() const;

	Atlas &getAtlas() { return atlas; }
	Batch2D *getBatch() { return batch; }
	Slot *getSlotHead() { return slot_head; }
	Bone *getBoneHead() { return bone_head; }
	Arena &getArena() { return arena; }
	gef::Vec<AnimationSke2D> &getAnimations() { return animations; }

private:
	void drawSlot(Slot *slot, gef::Sprite &sprite, const gef::Matrix33 &transform);

	Arena arena;
	Atlas atlas = &arena;
	std::string tex_path;
	Bone *bone_head = nullptr;
	Slot *slot_head = nullptr;
	gef::Vec<AnimationSke2D> animations;
	gef::Matrix44 transform = gef::Matrix44::kIdentity;
	Batch2D *batch = nullptr;
	gef::Platform *platform = nullptr;
};

struct SubTexture {
	gef::Rect uv;
};

struct Bone {
	float length;
	float rotation;
	char name[16];
	gef::Vector2 translation;
	gef::Vector2 scale;
	Bone *next;
};

struct Skin {
	int sub_texture;
	gef::Matrix33 transform;
};

struct Slot {
	Bone *bone = nullptr;
	Skin *skin = nullptr;
	Slot *first_child;
	Slot *next;
	float draw_order;
};

struct AnimationKey {
	float duration = 0.f;
	float timer = 0.f;
	bool tween = false;

	bool update(float delta_time);
};

struct AnimationRotation : public AnimationKey {
	float angle = 0.f;
};

struct AnimationTranslation : public AnimationKey {
	gef::Vector2 offset = gef::Vector2::kZero;
};

struct BoneAnimation {
	int cur_rot;
	int cur_tran;
	gef::Vec<AnimationRotation> rotations;
	gef::Vec<AnimationTranslation> translations;
	BoneAnimation *next = nullptr;

	BoneAnimation(Arena &arena);
	void update(float delta_time);
	void getRotationKeyFrames(AnimationRotation **out_start, AnimationRotation **out_end);
	void getTranslationKeyFrames(AnimationTranslation **out_start, AnimationTranslation **out_end);
};

struct AnimationSke2D {
	std::string name;
	float duration = 0.f;
	float timer = 0.f;
	std::unordered_map<Bone *, BoneAnimation *> bone_animations;
	BoneAnimation *head = nullptr;
};
