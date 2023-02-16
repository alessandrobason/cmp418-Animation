#include "anim_system_ske2d.h"

#include <unordered_map>
#include <algorithm>

#include <graphics/sprite_renderer.h>
#include <graphics/font.h>
#include <maths/math_utils.h>
#include <system/file.h>
#include <external/ImGui/imgui.h>

#include "batch2d.h"
#include "utils.h"

#include "rapidjson/document.h"

#include <system/debug_log.h>

/*
save format:

name          | type
------------------------------
		   header
------------------------------
transform     | float * 16
------------------------------
		    atlas
------------------------------
tex_path_len  | uint8_t
tex_path      | char * tex_path_len
subtex_count  | uint8_t
---- for each sub texture ----
uv_x	      | float
uv_y	      | float
uv_w	      | float
uv_h	      | float
------------------------------
		    bones
------------------------------
bone_count    | uint8_t
------- for each bone --------
length	      | float
rotation      | float
pos_x	      | float
pos_y	      | float
scale_x	      | float
scale_y	      | float
name_len      | uint8_t
name          | char * name_len
------------------------------
		    slots
------------------------------
slot_count    | uint8_t
------- for each slot --------
sub_texture   | uint8_t
transform     | float * 9
draw_order    | float
bone_id       | uint8_t
------------------------------
		  animations
------------------------------
anim_count    | uint8_t
------- for each anim --------
name_len      | uint8_t
name          | char * name_len
duration      | float
anim_count    | uint16_t
------- for each anim --------
bone_id       | uint8_t
rot_count     | uint8_t
-------- for each rot --------
duration      | float
tween         | bool
angle         | float
------------------------------
trans_count   | uint8_t
------- for each tran --------
duration      | float
tween         | bool
off_x         | float
off_y         | float
*/

// increase this every time a change to the format is made
// it'll make sure that it won't try to load the wrong 
// version of the file
static constexpr uint8_t format_ver = 3;

namespace json = rapidjson;

static gef::ptr<char[]> LoadJSON(const char *filename);

template<typename T>
static T getOrValue(const json::Value &value, const char *name, const T &default) {
	const auto &found = value.FindMember(name);
	if (found == value.MemberEnd()) return default;
	return found->value.Get<T>();
}

template<typename T>
static T getOrZero(const json::Value &value, const char *name) {
	return getOrValue<T>(value, name, 0);
}

static gef::Matrix33 matFromBone(const Bone &bone) {
	return mat3FromPosRotScale(bone.translation, bone.rotation, bone.scale);
}

Atlas::Atlas(IAllocator *alloc) {
	sub_textures.setAllocator(alloc);
}

bool AnimSystemSke2D::update(float delta_time) {
	if (cur_animation < 0 || cur_animation >= animations.size()) {
		return true;
	}

	bool finished = false;

	AnimationSke2D &anim = animations[cur_animation];
	if (anim.duration) {
		anim.timer += delta_time;
		while (anim.timer >= anim.duration) {
			anim.timer -= anim.duration;
			finished = true;
		}
	}
	BoneAnimation *bone = anim.head;
	while (bone) {
		bone->update(delta_time);
		bone = bone->next;
	}

	return finished;
}

void AnimSystemSke2D::draw() {
	gef::Sprite sprite;
	sprite.set_texture(atlas.texture);

	gef::Matrix33 sprite_mat = getBaseTransform();

	drawSlot(slot_head, sprite, sprite_mat);
	batch->resetZ();
}

void AnimSystemSke2D::debugDraw() {
	int id = 0;
	for (AnimationSke2D &anim : animations) {
		ImGui::PushID(id++);

		ImGui::Text("Name: %s", anim.name.c_str());
		ImGui::Text("Timer: %.3f/%.3f", anim.timer, anim.duration);
		ImGui::Separator();

		ImGui::PopID();
	}
}

void AnimSystemSke2D::setAnimation(const char *name) {
	for (size_t i = 0; i < animations.size(); ++i) {
		if (animations[i].name == name) {
			cur_animation = (int)i;
			break;
		}
	}
}

void AnimSystemSke2D::setAnimation(int id) {
	if (id < 0 || id >= animations.size()) {
		cur_animation = INVALID_ID;
		return;
	}
	cur_animation = id;
}

gef::Transform AnimSystemSke2D::getTransform() const {
	return transform;
}

void AnimSystemSke2D::setTransform(const gef::Transform &tran) {
	transform = tran.GetMatrix();
}

void AnimSystemSke2D::read(FILE *fp) {
	if (!fp) return;

	cleanup();

	uint8_t tex_path_len = 0;
	uint8_t subtex_count = 0;
	
	fileRead(transform, fp);
	fileRead(tex_path_len, fp);
	tex_path.resize(tex_path_len);
	fileRead(tex_path, fp);
	fileRead(subtex_count, fp);

	atlas.texture = loadPng(tex_path.c_str(), *platform, &arena).release();
	atlas.sub_textures.setAllocator(&arena);
	atlas.sub_textures.reserve(subtex_count);

	for (uint8_t i = 0; i < subtex_count; ++i) {
		SubTexture sub;
		fileRead(sub.uv.x, fp);
		fileRead(sub.uv.y, fp);
		fileRead(sub.uv.w, fp);
		fileRead(sub.uv.h, fp);
		atlas.sub_textures.emplace_back(sub);
	}

	uint8_t bone_count = 0;
	fileRead(bone_count, fp);

	gef::Vec<Bone *> id_to_bone;
	id_to_bone.reserve(bone_count);

	for (uint8_t i = 0; i < bone_count; ++i) {
		Bone *bone = arena.make<Bone>();
		uint8_t name_len = 0;

		fileRead(bone->length, fp);
		fileRead(bone->rotation, fp);
		fileRead(bone->translation.x, fp);
		fileRead(bone->translation.y, fp);
		fileRead(bone->scale.x, fp);
		fileRead(bone->scale.y, fp);
		fileRead(name_len, fp);
		assert(name_len < sizeof(bone->name));
		fread(bone->name, 1, name_len, fp);

		id_to_bone.emplace_back(bone);
	}

	// insert them in the opposite order, this way the original
	// saved order is mantained (this is not really needed but it
	// keeps the same order as when it was save, which is nice)
	for (int i = (int)id_to_bone.size() - 1; i >= 0; --i) {
		Bone *bone = id_to_bone[i];
		bone->next = bone_head;
		bone_head = bone;
	}

	uint8_t slot_count = 0;
	fileRead(slot_count, fp);

	struct TempSlot {
		Slot *slot;
		uint8_t parent_id;
	};
	gef::Vec<TempSlot> id_to_slot;
	id_to_slot.reserve(slot_count);

	for (uint8_t i = 0; i < slot_count; ++i) {
		TempSlot temp;
		temp.slot = arena.make<Slot>();
		Skin *skin = arena.make<Skin>();
		temp.slot->skin = skin;

		uint8_t sub_texture = 0;
		uint8_t bone_id = 0;

		fileRead(sub_texture, fp);
		fileRead(skin->transform.m, fp);
		fileRead(temp.slot->draw_order, fp);
		fileRead(bone_id, fp);
		fileRead(temp.parent_id, fp);

		temp.slot->bone = id_to_bone[bone_id];
		skin->sub_texture = (int)sub_texture;

		id_to_slot.emplace_back(temp);
	}

	for (TempSlot &temp : id_to_slot) {
		if (temp.parent_id == UINT8_MAX) {
			assert(!slot_head);
			slot_head = temp.slot;
		}
		else {
			Slot *parent = id_to_slot[temp.parent_id].slot;
			temp.slot->next = parent->first_child;
			parent->first_child = temp.slot;
		}
	}

	uint8_t anim_count = 0;
	fileRead(anim_count, fp);
	animations.reserve(anim_count);

	for (uint8_t i = 0; i < anim_count; ++i) {
		AnimationSke2D anim;
		uint8_t name_len = 0;
		uint16_t bone_anim_count = 0;

		fileRead(name_len, fp);
		anim.name.resize(name_len);
		fileRead(anim.name, fp);
		fileRead(anim.duration, fp);
		fileRead(bone_anim_count, fp);

		anim.bone_animations.reserve(bone_anim_count);

		for (uint16_t i = 0; i < bone_anim_count; ++i) {
			BoneAnimation *bone_anim = arena.make<BoneAnimation>(arena);

			uint8_t bone_id = 0;
			uint8_t rot_count = 0;
			uint8_t trans_count = 0;
			fileRead(bone_id, fp);

			fileRead(rot_count, fp);
			bone_anim->rotations.reserve(rot_count);

			for (uint8_t r = 0; r < rot_count; ++r) {
				AnimationRotation rot;
				fileRead(rot.duration, fp);
				fileRead(rot.tween,    fp);
				fileRead(rot.angle,    fp);
				bone_anim->rotations.emplace_back(rot);
			}

			fileRead(trans_count, fp);
			bone_anim->translations.reserve(trans_count);

			for (uint8_t t = 0; t < trans_count; ++t) {
				AnimationTranslation tran;
				fileRead(tran.duration, fp);
				fileRead(tran.tween,    fp);
				fileRead(tran.offset.x, fp);
				fileRead(tran.offset.y, fp);
				bone_anim->translations.emplace_back(tran);
			}

			Bone *bone = id_to_bone[bone_id];
			anim.bone_animations[bone] = bone_anim;
			bone_anim->next = anim.head;
			anim.head = bone_anim;
		}

		animations.emplace_back(std::move(anim));
	}
}

static Slot *findParent(Slot *slot, Slot *child) {
	if (!slot) {
		return nullptr;
	}

	// try and look in the immediate children
	Slot *cur_child = slot->first_child;
	while (cur_child) {
		if (cur_child == child) {
			return slot;
		}
		cur_child = cur_child->next;
	}

	// if nothing is found, try going inside the tree
	cur_child = slot->first_child;
	while (cur_child) {
		if (Slot *parent = findParent(cur_child, child)) {
			return parent;
		}
		cur_child = cur_child->next;
	}

	return nullptr;
}

static void countSlots(Slot *head, gef::Vec<Slot *> &slots) {
	if (!head) return;

	slots.emplace_back(head);
	Slot *child = head->first_child;
	while (child) {
		countSlots(child, slots);
		child = child->next;
	}
}

void AnimSystemSke2D::save(FILE *fp) const {
	if (!fp) return;

	fileWrite(format_ver, fp);
	fileWrite(transform, fp);

	assert(tex_path.size() < 255);
	assert(atlas.sub_textures.size() < 255);
	uint8_t tex_path_len = (uint8_t)tex_path.size();
	uint8_t subtex_count = (uint8_t)atlas.sub_textures.size();

	fileWrite(tex_path_len, fp);
	fileWrite(tex_path, fp);
	fileWrite(subtex_count, fp);

	for (uint8_t i = 0; i < subtex_count; ++i) {
		const SubTexture &sub = atlas.sub_textures[i];
		fileWrite(sub.uv.x, fp);
		fileWrite(sub.uv.y, fp);
		fileWrite(sub.uv.w, fp);
		fileWrite(sub.uv.h, fp);
	}

	gef::Vec<Bone *> id_to_bone;
	Bone *bone = bone_head;
	while (bone) {
		id_to_bone.emplace_back(bone);
		bone = bone->next;
	}

	assert(id_to_bone.size() < 255);
	uint8_t bone_count = (uint8_t)id_to_bone.size();
	fileWrite(bone_count, fp);

	for (Bone *bone : id_to_bone) {
		uint8_t name_len = (uint8_t)strlen(bone->name);

		fileWrite(bone->length, fp);
		fileWrite(bone->rotation, fp);
		fileWrite(bone->translation.x, fp);
		fileWrite(bone->translation.y, fp);
		fileWrite(bone->scale.x, fp);
		fileWrite(bone->scale.y, fp);
		fileWrite(name_len, fp);
		fwrite(bone->name, 1, name_len, fp);
	}

	gef::Vec<Slot *> id_to_slot;
	countSlots(slot_head, id_to_slot);
	
	assert(id_to_slot.size() < 255);
	uint8_t slot_count = (uint8_t)id_to_slot.size();
	fileWrite(slot_count, fp);

	for (Slot *slot : id_to_slot) {
		assert(slot->skin && slot->bone);
		Skin *skin = slot->skin;
		uint8_t sub_texture = (uint8_t)skin->sub_texture;

		size_t bone_index = id_to_bone.find(slot->bone);
		assert(bone_index < id_to_bone.size());
		uint8_t bone_id = (uint8_t)bone_index;

		uint8_t parent_id = UINT8_MAX;
		if (Slot *parent_slot = findParent(slot_head, slot)) {
			size_t parent_index = id_to_slot.find(parent_slot);
			assert(parent_index < id_to_slot.size());
			parent_id = (uint8_t)parent_index;
		}
		if (parent_id == UINT8_MAX) {
			int a = 2;
		}

		fileWrite(sub_texture, fp);
		fileWrite(skin->transform.m, fp);
		fileWrite(slot->draw_order, fp);
		fileWrite(bone_id, fp);
		fileWrite(parent_id, fp);

		slot = slot->next;
	}
	
	assert(animations.size() < 255);
	uint8_t anim_count = (uint8_t)animations.size();
	fileWrite(anim_count, fp);

	for (uint8_t i = 0; i < anim_count; ++i) {
		const AnimationSke2D &anim = animations[i];
		assert(anim.name.size() < 255);
		uint8_t name_len = (uint8_t)anim.name.size();
		uint16_t boneanim_count = (uint16_t)anim.bone_animations.size();

		fileWrite(name_len, fp);
		fileWrite(anim.name, fp);
		fileWrite(anim.duration, fp);
		fileWrite(boneanim_count, fp);

		for (const auto &[bone, bone_anim] : anim.bone_animations) {
			assert(bone_anim->rotations.size() < 255);
			assert(bone_anim->translations.size() < 255);
			size_t bone_index = id_to_bone.find(bone);
			assert(bone_index < id_to_bone.size());
			uint8_t bone_id = (uint8_t)bone_index;
			uint8_t rot_count = (uint8_t)bone_anim->rotations.size();
			uint8_t tran_count = (uint8_t)bone_anim->translations.size();
			fileWrite(bone_id, fp);
			fileWrite(rot_count, fp);
			for (const auto &rot : bone_anim->rotations) {
				fileWrite(rot.duration, fp);
				fileWrite(rot.tween, fp);
				fileWrite(rot.angle, fp);
			}
			
			fileWrite(tran_count, fp);
			for (const auto &tran : bone_anim->translations) {
				fileWrite(tran.duration, fp);
				fileWrite(tran.tween,    fp);
				fileWrite(tran.offset.x, fp);
				fileWrite(tran.offset.y, fp);
			}
		}
	}
}

bool AnimSystemSke2D::checkFormatVersion(FILE *fp) {
	if (!fp) return false;
	uint8_t version = 0;
	fileRead(version, fp);
	return version == format_ver;
}

void AnimSystemSke2D::init(Batch2D *sprite_batcher, gef::Platform &plat) {
	type = AnimSystemType::Skeleton2D;

	batch = sprite_batcher;
	platform = &plat;
	arena.setAllocator(g_alloc);
	animations.setAllocator(&arena);
	atlas.sub_textures.setAllocator(&arena);
}

void AnimSystemSke2D::cleanup() {
	atlas.texture = nullptr;
	atlas.sub_textures.destroy();
	tex_path.clear();
	bone_head = nullptr;
	slot_head = nullptr;
	animations.destroy();
	transform = gef::Matrix44::kIdentity;
	arena.cleanup();
}

void AnimSystemSke2D::loadFromDragonBones(const char *tex_file, const char *ske_file) {
	PushAllocInfo("AnimSke2DLoad");

	IAllocator *old_g_alloc = g_alloc;
	g_alloc = &arena;

	std::unordered_map<std::string, int> name_to_sub;
	std::unordered_map<std::string, int> name_to_bone;
	std::unordered_map<std::string, int> name_to_skin;

	// == PARSE TEXTURE FILE =========================================================

	json::Document tex_doc;
	tex_doc.Parse(LoadJSON(tex_file));

	tex_path = tex_doc["imagePath"].GetString();
	atlas.texture = loadPng(tex_doc["imagePath"].GetString(), *platform).release();

	const auto &sub_textures = tex_doc["SubTexture"].GetArray();

	// instead of saving the pixel position, we save the exact uv coordinate by
	// using the atlas size
	float texel_width = 1.f / tex_doc["width"].GetFloat();
	float texel_height = 1.f / tex_doc["height"].GetFloat();

	for (const auto &sub_tex : sub_textures) {
		SubTexture sub{};

		name_to_sub[sub_tex["name"].GetString()] = (int)atlas.sub_textures.size();
		//sub.name = sub_tex["name"].GetString();

		sub.uv.x = getOrZero<float>(sub_tex, "x");
		sub.uv.y = getOrZero<float>(sub_tex, "y");
		sub.uv.w = getOrZero<float>(sub_tex, "width");
		sub.uv.h = getOrZero<float>(sub_tex, "height");

		gef::Rect frame = {
			getOrZero<float>(sub_tex, "frameX"),
			getOrZero<float>(sub_tex, "frameY"),
			getOrZero<float>(sub_tex, "frameWidth"),
			getOrZero<float>(sub_tex, "frameHeight")
		};

		sub.uv.x *= texel_width;
		sub.uv.w *= texel_width;

		sub.uv.y *= texel_height;
		sub.uv.h *= texel_height;

		atlas.sub_textures.emplace_back(sub);
	}

	// == PARSE SKELETON FILE ========================================================

	json::Document ske_doc;
	ske_doc.Parse(LoadJSON(ske_file).get());

	const auto &armature = ske_doc["armature"][0];
	const auto &ske_slots = armature["skin"][0]["slot"].GetArray();
	const auto &ske_bones = armature["bone"].GetArray();
	const auto &slot_order = armature["slot"].GetArray();

	struct BoneTemp {
		Bone *bone = nullptr;
		int parent = -1;
	};

	struct SlotTemp {
		Slot *slot = nullptr;
		BoneTemp *temp = nullptr;
	};

	gef::Vec<BoneTemp> bonetemps;
	gef::Vec<Skin *> skins;
	gef::Vec<SlotTemp> slottemps;

	float draw_step = 1.f / (float)(slot_order.Size() + 1);
	float draw_order = 1.f;

	for (const auto &slot : ske_slots) {
		Skin *skin = arena.make<Skin>();

		name_to_skin[slot["name"].GetString()] = (int)skins.size();

		const auto &slot_display = slot["display"][0];
		const auto &transform = slot_display["transform"];

		skin->sub_texture = name_to_sub[slot_display["name"].GetString()];

		gef::Vector2 skin_pos = {
			getOrZero<float>(transform, "x"),
			getOrZero<float>(transform, "y")
		};
		float skin_rot = gef::DegToRad(getOrZero<float>(transform, "skX"));

		skin->transform = mat3FromRot(skin_rot) * mat3FromPos(skin_pos);

		skins.emplace_back(skin);
	}

	for (const auto &ske_bone : ske_bones) {
		BoneTemp temp;
		temp.bone = arena.make<Bone>();

		const char *name = ske_bone["name"].GetString();
		name_to_bone[name] = (int)bonetemps.size();

		strCopyInto(temp.bone->name, name);
		temp.bone->length = getOrZero<float>(ske_bone, "length");

		if (auto parent = getOrZero<const char *>(ske_bone, "parent")) {
			const auto &it = name_to_bone.find(parent);
			if (it != name_to_bone.end()) {
			     temp.parent = it->second;
			}
		}

		const auto &transform_it = ske_bone.FindMember("transform");
		if (transform_it != ske_bone.MemberEnd()) {
			const auto &tran = transform_it->value;
			temp.bone->translation.x = getOrZero<float>(tran, "x");
			temp.bone->translation.y = getOrZero<float>(tran, "y");
			temp.bone->rotation = gef::DegToRad(getOrZero<float>(tran, "skX"));
			temp.bone->scale.x = getOrValue<float>(tran, "scX", 1.f);
			temp.bone->scale.y = getOrValue<float>(tran, "scY", 1.f);
		}
		else {
			temp.bone->translation = gef::Vector2::kZero;
			temp.bone->rotation = 0.f;
			temp.bone->scale = gef::Vector2::kOne;
		}

		bonetemps.emplace_back(temp);
		temp.bone->next = bone_head;
		bone_head = temp.bone;
	}

	for (const auto &cur_slot : slot_order) {
		SlotTemp temp;
		temp.slot = arena.make<Slot>();
		temp.slot->draw_order = draw_order;
		draw_order -= draw_step;

		const char *name = cur_slot["name"].GetString();
		const char *parent = cur_slot["parent"].GetString();

		const auto &name_it = name_to_bone.find(name);
		const auto &parent_it = name_to_skin.find(parent);

		if (name_it != name_to_bone.end()) {
			temp.slot->bone = bonetemps[name_it->second].bone;
			temp.temp = &bonetemps[name_it->second];
		}

		if (parent_it != name_to_skin.end()) {
			temp.slot->skin = skins[parent_it->second];
		}

		slottemps.emplace_back(temp);
	}

	for (SlotTemp &slottemp: slottemps) {
		if (slottemp.temp->parent != -1) {
			Slot *slot = slottemp.slot;
			BoneTemp *bonetemp = slottemp.temp;
			Bone *parent_bone = bonetemps[bonetemp->parent].bone;

			Slot *parent = nullptr;
			for (const auto &s : slottemps) {
				if (s.slot->bone == parent_bone) {
					parent = s.slot;
					break;
				}
			}
			// if it can't find a parent slot, it means it is the root
			// which doesn't usually have a skin attached to it
			// in that case let's act like this is the slot_head
			if (!parent) {
				// there should only be one bone without a parent
				assert(!slot_head);
				slot_head = slottemp.slot;
				continue;
			}

			assert(parent);

			if (parent->first_child) {
				Slot *child = parent->first_child;
				while (child) {
					if (!child->next) {
						child->next = slot;
						break;
					}
					child = child->next;
				}
			}
			else {
				parent->first_child = slot;
			}
		}
		// this shouldn't ever actually be called as the root usually doesn't
		// have a skin, meaning no slot
		else {
			// there should only be one bone without a parent
			assert(!slot_head);
			slot_head = slottemp.slot;
		}
	}

	assert(slot_head);

	// == PARSE BONE ANIMATION =======================================================

	const auto &anim_arr = armature["animation"].GetArray();
	float frame_len = 1.f / armature["frameRate"].GetFloat();

	for (const auto &ske_anim : anim_arr) {
		AnimationSke2D anim{};

		anim.name = ske_anim["name"].GetString();
		anim.duration = ske_anim["duration"].GetFloat() * frame_len;

		for (const auto &bone : ske_anim["bone"].GetArray()) {
			BoneAnimation *bone_anim = arena.make<BoneAnimation>(arena);

			int bone_id = name_to_bone[bone["name"].GetString()];
			Bone *bone_ptr = bonetemps[bone_id].bone;

			const auto &trans_frame = bone.FindMember("translateFrame");
			const auto &rot_frame = bone.FindMember("rotateFrame");

			if (trans_frame != bone.MemberEnd()) {
				for (const auto &frame : trans_frame->value.GetArray()) {
					AnimationTranslation tran{};

					tran.duration = getOrZero<float>(frame, "duration") * frame_len;
					tran.offset.x = getOrZero<float>(frame, "x");
					tran.offset.y = getOrZero<float>(frame, "y");
					tran.tween = frame.HasMember("tweenEasing");

					bone_anim->translations.emplace_back(tran);
				}
			}

			if (rot_frame != bone.MemberEnd()) {
				for (const auto &frame : rot_frame->value.GetArray()) {
					AnimationRotation rot{};

					rot.duration = getOrZero<float>(frame, "duration") * frame_len;
					rot.angle = gef::DegToRad(getOrZero<float>(frame, "rotate"));
					rot.tween = frame.HasMember("tweenEasing");

					bone_anim->rotations.emplace_back(rot);
				}
			}

			anim.bone_animations[bone_ptr] = bone_anim;
			bone_anim->next = anim.head;
			anim.head = bone_anim;
		}

		animations.emplace_back(anim);
	}

	PopAllocInfo();

	g_alloc = old_g_alloc;
}

AnimationSke2D *AnimSystemSke2D::getAnimation(int id) {
	if (id < 0 || id >= animations.size()) {
		return nullptr;
	}
	return &animations[id];
}

void AnimSystemSke2D::drawSlot(Slot *slot, gef::Sprite &sprite, const gef::Matrix33 &transform) {
	if (!slot || !slot->bone || !slot->skin) {
		return;
	}

	Bone *bone = slot->bone;
	Skin *skin = slot->skin;
	SubTexture &sub = atlas.sub_textures[skin->sub_texture];

	gef::Matrix33 bone_tran = getMatFromAnimatedBone(slot->bone) * transform;

	sprite.set_uv_position({ sub.uv.x, sub.uv.y });
	sprite.set_uv_width(sub.uv.w);
	sprite.set_uv_height(sub.uv.h);
	sprite.set_width(sub.uv.w * atlas.texture->GetSize().x);
	sprite.set_height(sub.uv.h * atlas.texture->GetSize().y);

	batch->setZ(slot->draw_order);
	batch->drawSprite(
		sprite,
		skin->transform * bone_tran
	);

	Slot *child = slot->first_child;
	while (child) {
		drawSlot(child, sprite, bone_tran);
		child = child->next;
	}
}

gef::Matrix33 AnimSystemSke2D::getMatFromAnimatedBone(Bone *bone) {
	if (!bone) {
		return gef::Matrix33::kIdentity;
	}
	// get base bone transformation values
	float rotation = bone->rotation;
	gef::Vector2 pos = bone->translation;
	gef::Vector2 scale = bone->scale;

	if (cur_animation < 0 || cur_animation >= animations.size()) {
		return mat3FromPosRotScale(pos, rotation, scale);
	}

	const auto &anim = animations[cur_animation];
	auto it = anim.bone_animations.find(bone);
	if (it == anim.bone_animations.end()) {
		return mat3FromPosRotScale(pos, rotation, scale);
	}
	BoneAnimation *bone_anim = it->second;

	// animations works by key frame, you have a beginning frame and an ending frame and
	// interpolate between them
	// if the beginning frame is the last frame of the animation, we just set its value
	// as the new rotation/translation/scale

	AnimationRotation *start_rot = nullptr;
	AnimationRotation *end_rot = nullptr;
	bone_anim->getRotationKeyFrames(&start_rot, &end_rot);

	if (start_rot) {
		float start = rotation + start_rot->angle;
		if (start_rot->tween && end_rot) {
			// to correctly interpolate between angles we need to check that it's going
			// to rotate in the correct direction (clockwise/counter-clockwise)
			// so instead of using the normal lerp function we first get the correct
			// difference (which might be flipped from the original one) and use the
			// angleLerp function, which already takes a diff parameter instead of 
			// calculating it itself
			float end = rotation + end_rot->angle;
			float diff = tweenGetAngleDiff(start, end);
			rotation = angleLerp(start, diff, start_rot->timer / start_rot->duration);
		}
		else {
			rotation = start;
		}
	}

	// we use the same concept for translation

	AnimationTranslation *start_tran = nullptr;
	AnimationTranslation *end_tran = nullptr;
	bone_anim->getTranslationKeyFrames(&start_tran, &end_tran);

	if (start_tran) {
		gef::Vector2 start = pos + start_tran->offset;
		if (start_tran->tween && end_tran) {
			gef::Vector2 end = pos + end_tran->offset;
			pos = lerp(start, end, start_tran->timer / start_tran->duration);
		}
		else {
			pos = start;
		}
	}

	return mat3FromPosRotScale(pos, rotation, scale);
}

gef::Matrix33 AnimSystemSke2D::getBaseTransform() const {
	return mat3FromMat4(transform);
}

// == ANIMATION KEY ==============================================================

bool AnimationKey::update(float delta_time) {
	timer += delta_time;
	if (timer >= duration) {
		timer = 0.f;
		return true;
	}
	return false;
}

// == BONE ANIMATION =============================================================

BoneAnimation::BoneAnimation(Arena &arena) {
	translations.setAllocator(&arena);
	rotations.setAllocator(&arena);
}

void BoneAnimation::update(float delta_time) {
	if (cur_rot < rotations.size()) {
		if (rotations[cur_rot].update(delta_time)) {
			if (++cur_rot >= rotations.size()) {
				cur_rot = 0;
			}
		}
	}

	if (cur_tran < translations.size()) {
		if (translations[cur_tran].update(delta_time)) {
			if (++cur_tran >= translations.size()) {
				cur_tran = 0;
			}
		}
	}
}

void BoneAnimation::getRotationKeyFrames(AnimationRotation **out_start, AnimationRotation **out_end) {
	if (!out_start || !out_end) {
		return;
	}

	(*out_start) = nullptr;
	(*out_end) = nullptr;

	if (cur_rot < rotations.size()) {
		(*out_start) = &rotations[cur_rot];
		if ((cur_rot + 1) < rotations.size()) {
			(*out_end) = &rotations[cur_rot + 1];
		}
	}
}

void BoneAnimation::getTranslationKeyFrames(AnimationTranslation **out_start, AnimationTranslation **out_end) {
	if (!out_start || !out_end) {
		return;
	}

	(*out_start) = nullptr;
	(*out_end) = nullptr;

	if (cur_tran < translations.size()) {
		(*out_start) = &translations[cur_tran];
		if ((cur_tran + 1) < translations.size()) {
			(*out_end) = &translations[cur_tran + 1];
		}
	}
}

// == PRIVATE FUNCTIONS ==========================================================

#ifdef NDEBUG
#define CHECK(cond) (cond)
#else
#define CHECK(cond) if (!(cond)) fatal(__FILE__":%d condition " #cond " failed", __LINE__)
#endif

static gef::ptr<char[]> LoadJSON(const char *filename) {
	gef::ptr<gef::File> file = gef::File::Create();
	Int32 file_size = 0, bytes_read = 0;

	CHECK(file->Open(filename));
	CHECK(file->GetSize(file_size));
	auto json_string = gef::ptr<char[]>::make(file_size + 1);
	CHECK(json_string);
	CHECK(file->Read(json_string.get(), file_size, bytes_read));
	CHECK(bytes_read == file_size);

	json_string[file_size] = '\0';
	file->Close();
	return json_string;
}