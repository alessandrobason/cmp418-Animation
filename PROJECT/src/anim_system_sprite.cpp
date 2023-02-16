#include "anim_system_sprite.h"

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <assert.h>

#include <system/platform.h>
#include <maths/quaternion.h>
#include <maths/math_utils.h>
#include <graphics/image_data.h>
#include <graphics/sprite_renderer.h>
#include <assets/png_loader.h>

#include <external/ImGui/imgui.h>

#include "batch2d.h"
#include "utils.h"

#ifdef _WIN32
#include <platform/d3d11/graphics/texture_d3d11.h>
#endif

enum class AnimType : UInt8 {
	None, Atlas, Sprite
};

// increase this every time a change to the format is made
// it'll make sure that it won't try to load the wrong 
// version of the file
static constexpr uint8_t format_ver = 4;

/*
save format:

name          | type
------------------------------
           header
------------------------------
pos_x         | float
pos_y         | float
scale_x       | float
scale_y       | float
origin_x      | float
origin_y      | float
rot           | float
anim_count    | uint32_t
texture_count | uint32_t
------ for each texture ------
path_len      | uint8_t
name_len      | uint8_t
path          | char * path_len
name          | char * name_len
------------------------------
         anim header
------------------------------
anim_type     | uint8_t (AnimType)
name_len      | uint8_t
name          | char * uint8_t
------------------------------
           atlas
------------------------------
texture       | uint32_t (index)
rows          | uint32_t
columns       | uint32_t
frame_count   | uint32_t
------- for each frame -------
id            | int32_t
length        | float
------------------------------
           sprite
------------------------------
frame_count   | uint32_t
------- for each frame -------
texture       | uint32_t (index)
x             | float
y             | float
w             | float
h             | float
offset_x      | float
offset_y      | float
length        | float
*/

// == Atlas Based Animation ==================================================================

bool AtlasAnim::update(float dt) {
	if (frames.empty()) {
		return true;
	}

	current_frame = gef::clamp(current_frame, 0, (int)frames.size() - 1);
	bool finished = false;
	timer += dt;
	while (timer >= frames[current_frame].length) {
		timer -= frames[current_frame].length;
		current_frame++;
		if (current_frame >= frames.size()) {
			current_frame = 0;
			finished = true;
		}
	}
	return finished;
}

void AtlasAnim::draw(Batch2D *batch, const Transform2D &transform) {
	if (frames.empty()) {
		return;
	}

	current_frame = gef::clamp(current_frame, 0, (int)frames.size() - 1);
	
	Frame &frame = frames[current_frame];
	float x = (float)(frame.id % columns);
	float y = (float)(frame.id / columns);
	
	gef::Vector2 tile_size = atlas->GetSize() / gef::Vector2((float)columns, (float)rows);
	gef::Rect dst = { transform.position, tile_size * transform.scale };
	gef::Rect src = { { x, y }, tile_size };
	src.pos *= tile_size;

	batch->setTexture(atlas);
	batch->drawRect(
		dst,
		src,
		transform.rotation,
		dst.size * transform.origin,
		gef::Colour::white
	);
}

void AtlasAnim::read(FILE *fp, AnimSystemSprite *system) {
	frames.clear();

	uint8_t name_len     = 0;
	uint32_t texture_id  = 0;
	uint32_t urows       = 0;
	uint32_t ucolumns    = 0;
	uint32_t frame_count = 0;

	fileRead(name_len,    fp);
	fread(debug_name, 1, name_len, fp);
	fileRead(texture_id,  fp);
	fileRead(urows,       fp);
	fileRead(ucolumns,    fp);
	fileRead(frame_count, fp);
	
	atlas = system->getTextures()[texture_id].get();
	rows = urows;
	columns = ucolumns;
	frames.reserve(frame_count);

	for (uint32_t i = 0; i < frame_count; ++i) {
		int32_t id;
		float length;
		fileRead(id,     fp);
		fileRead(length, fp);
		frames.emplace_back(id, length);
	}
}

void AtlasAnim::save(FILE *fp, const AnimSystemSprite *system) const {
	AnimType type        = AnimType::Atlas;
	uint8_t name_len     = (uint8_t)strlen(debug_name);
	uint32_t texture_id  = 0;
	uint32_t urows       = (uint32_t)rows;
	uint32_t ucolumns    = (uint32_t)columns;
	uint32_t frame_count = (uint32_t)frames.size();
	
	const auto &textures = system->getTextures();

	for (size_t i = 0; i < textures.size(); ++i) {
		if (textures[i].get() == atlas) {
			texture_id = (uint32_t)i;
			break;
		}
	}

	fileWrite(type,        fp);
	fileWrite(name_len,    fp);
	fwrite(debug_name, 1, name_len, fp);
	fileWrite(texture_id,  fp);
	fileWrite(urows,       fp);
	fileWrite(ucolumns,    fp);
	fileWrite(frame_count, fp);
	
	for (const auto &f : frames) {
		int32_t id   = f.id;
		float length = f.length;
		fileWrite(id,     fp);
		fileWrite(length, fp);
	}
}

// == Sprite Based Animation =================================================================

bool SpriteAnim::update(float dt) {
	if (frames.empty()) {
		return true;
	}

	current_frame = gef::clamp(current_frame, 0, (int)frames.size() - 1);
	bool finished = false;
	timer += dt;
	while (timer >= frames[current_frame].length) {
		timer -= frames[current_frame].length;
		current_frame++;
		if (current_frame >= frames.size()) {
			current_frame = 0;
			finished = true;
		}
	}
	return finished;
}

void SpriteAnim::draw(Batch2D *batch, const Transform2D &transform) {
	if (frames.empty()) {
		return;
	}

	current_frame = gef::clamp(current_frame, 0, (int)frames.size() - 1);

	Frame &frame = frames[current_frame];

	gef::Rect dst = { transform.position + frame.offset, frame.source.size * transform.scale };

	batch->setTexture(frame.texture);
	batch->drawRect(
		dst,
		frame.source,
		transform.rotation,
		dst.size * transform.origin,
		gef::Colour::white
	);
}

void SpriteAnim::read(FILE *fp, AnimSystemSprite *system) {
	frames.clear();

	uint8_t name_len     = 0;
	uint32_t frame_count = 0;

	fileRead(name_len,   fp);
	fread(debug_name, 1, name_len, fp); 
	fileRead(frame_count, fp);
	frames.reserve(frame_count);

	for (uint32_t i = 0; i < frame_count; ++i) {
		uint32_t texture_id = 0;
		gef::Rect src;
		gef::Vector2 off;
		float length;
		fileRead(texture_id, fp);
		fileRead(src.x,  fp);
		fileRead(src.y,  fp);
		fileRead(src.w,  fp);
		fileRead(src.h,  fp);
		fileRead(off.x,  fp);
		fileRead(off.y,  fp);
		fileRead(length, fp);
		gef::Texture *texture = system->getTextures()[texture_id].get();
		frames.emplace_back(src, length, texture, off);
	}
}

void SpriteAnim::save(FILE *fp, const AnimSystemSprite *system) const {
	AnimType type        = AnimType::Sprite;
	uint8_t name_len     = (uint8_t)strlen(debug_name);
	uint32_t frame_count = (uint32_t)frames.size();
	
	fileWrite(type,        fp);
	fileWrite(name_len,    fp);
	fwrite(debug_name, 1, name_len, fp);
	fileWrite(frame_count, fp);

	const auto &textures = system->getTextures();

	for (const auto &f : frames) {
		uint32_t texture_id = 0;

		for (size_t i = 0; i < textures.size(); ++i) {
			if (textures[i].get() == f.texture) {
				texture_id = (uint32_t)i;
				break;
			}
		}

		fileWrite(texture_id, fp);

		fileWrite(f.source.x, fp);
		fileWrite(f.source.y, fp);
		fileWrite(f.source.w, fp);
		fileWrite(f.source.h, fp);

		fileWrite(f.offset.x, fp);
		fileWrite(f.offset.y, fp);

		fileWrite(f.length,   fp);
	}
}

// == Sprite Animation System ================================================================

bool AnimSystemSprite::update(float delta_time) {
	if (cur_animation == INVALID_ID || cur_animation >= animations.size()) {
		return true;
	}
	return animations[cur_animation]->update(delta_time);
}

void AnimSystemSprite::draw() {
	assert(batch);
	if (cur_animation == INVALID_ID || cur_animation >= animations.size()) {
		return;
	}
	animations[cur_animation]->draw(batch, transform);
}

void AnimSystemSprite::debugDraw() {
	int id = 0;
	for (gef::ptr<IBaseSpriteAnim> &anim : animations) {
		ImGui::PushID(id++);

		ImGui::Text("Name: %s", anim->debug_name);
		ImGui::Separator();

		constexpr float zoom = 3.f;

		if (auto atlas = dynamic_cast<AtlasAnim*>(anim.get())) {
			ImGuiStyle &style = ImGui::GetStyle();
			float window_visible_x2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;

			gef::Vector2 tex_size = atlas->atlas ? atlas->atlas->GetSize() : gef::Vector2(16, 16);
			gef::Vector2 tile_size = tex_size / gef::Vector2((float)atlas->columns, (float)atlas->rows);
			tile_size *= zoom;

			for (size_t i = 0; i < atlas->frames.size(); ++i) {
				const auto &f = atlas->frames[i];
				float w = 1.f / atlas->columns;
				float h = 1.f / atlas->rows;
				float x = (float)(f.id % atlas->columns) * w;
				float y = (float)(f.id / atlas->columns) * h;
				gef::Vector2 uv0 = { x, y };
				gef::Vector2 uv1 = { x + w, y + h };
				gef::Colour colour = gef::Colour::white;
				if ((id - 1) == cur_animation && i == atlas->current_frame) {
					colour = gef::Colour::red;
				}
				ImGui::Image(imGetTexData(atlas->atlas), tile_size, uv0, uv1, colour);

				float last_image_x2 = ImGui::GetItemRectMax().x;
				float next_image_x2 = last_image_x2 + style.ItemSpacing.x + tile_size.x; // Expected position if next button was on same line
				if (next_image_x2 < window_visible_x2)
					ImGui::SameLine();
			}
		}

		if (auto sprite = dynamic_cast<SpriteAnim*>(anim.get())) {
			ImGuiStyle &style = ImGui::GetStyle();
			float window_visible_x2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;

			for (size_t i = 0; i < sprite->frames.size(); ++i) {
				const auto &f = sprite->frames[i];
				gef::Vector2 tex_size = f.texture ? f.texture->GetSize() : gef::Vector2(50, 50);
				gef::Rect src = f.source / tex_size;
				gef::Colour colour = gef::Colour::white;
				if ((id - 1) == cur_animation && i == sprite->current_frame) {
					colour = gef::Colour::red;
				}
				ImGui::Image(
					imGetTexData(f.texture),
					f.source.size * zoom,
					src.pos,
					src.pos + src.size,
					colour
				);

				float last_image_x2 = ImGui::GetItemRectMax().x;
				float next_image_x2 = last_image_x2 + style.ItemSpacing.x + f.source.w * 2.f; // Expected position if next button was on same line
				if (next_image_x2 < window_visible_x2)
					ImGui::SameLine();
			}
		}

		ImGui::NewLine();
		ImGui::Separator();
		ImGui::PopID();
	}
}

void AnimSystemSprite::setAnimation(const char *name) {
	size_t index = texture_names.find(name);
	int new_id = INVALID_ID;
	if (index != SIZE_MAX) {
		new_id = (int)index;
	}
	setAnimation(new_id);
}

void AnimSystemSprite::setAnimation(int id) {
	if (id < 0 || id >= animations.size()) {
		id = INVALID_ID;
	}
	cur_animation = id;
}

gef::Transform AnimSystemSprite::getTransform() const {
	//return matrix;
	gef::Quaternion rot = { gef::Vector4(0.f, 0.f, 1.f), transform.rotation };
	gef::Vector4 pos = { transform.position.x, transform.position.y, 0.f };
	gef::Vector4 scale = { transform.scale.x, transform.scale.y, 1.f };
	return { rot, pos, scale };
}

void AnimSystemSprite::setTransform(const gef::Transform &tran) {
	gef::Vector4 trasl = tran.translation();
	gef::Vector4 scale4 = tran.scale();
	gef::Quaternion quat = tran.rotation();
	float angle = 0.f;
	
	if (!quat.IsIdentity()) {
		// if the quaternion is normalised and the axis of rotation is the
		// z axis, the w value is = cos(angle/2)
		float cos_angle = gef::clamp(quat.SafeNorm().w, -1.f, 1.f);
		angle = acosf(cos_angle) * 2.f;
	}

	transform.position = { trasl.x(), trasl.y() };
	transform.scale = { scale4.x(), scale4.y() };
	transform.rotation = angle;
}

void AnimSystemSprite::read(FILE *fp) {
	if (!fp) return;

	PushAllocInfo("AnimSpriteRead");

	textures.clear();
	texture_names.clear();
	texture_paths.clear();
	animations.clear();

	float pos_x, pos_y;
	float scale_x, scale_y;
	float origin_x, origin_y;
	float rot; 
	uint32_t anim_count;
	uint32_t texture_count;

	fileRead(pos_x,      fp);
	fileRead(pos_y,      fp);
	fileRead(scale_x,    fp);
	fileRead(scale_y,    fp);
	fileRead(origin_x,   fp);
	fileRead(origin_y,   fp);
	fileRead(rot,        fp);
	fileRead(anim_count, fp);
	fileRead(texture_count, fp);

	transform.position = { pos_x, pos_y };
	transform.scale    = { scale_x, scale_y };
	transform.origin   = { origin_x, origin_y };
	transform.rotation = rot;

	assert(texture_count < 100);

	if (texture_count) {
		texture_count++;
	}

	textures.reserve(texture_count);
	texture_names.reserve(texture_count);
	texture_paths.reserve(texture_count);

	addInitialTexture();

	for (uint32_t i = 1; i < texture_count; ++i) {
		uint8_t path_len, name_len;
		std::string path, name;
		fileRead(path_len, fp);
		fileRead(name_len, fp);
		path.resize(path_len);
		name.resize(name_len);
		fileRead(path, fp);
		fileRead(name, fp);
		textures.emplace_back(loadPng(path.c_str(), *platform));
		texture_names.emplace_back(std::move(name));
		texture_paths.emplace_back(std::move(path));
	}

	animations.reserve(anim_count);
	for (uint32_t i = 0; i < anim_count; ++i) {
		AnimType type = AnimType::None;
		fileRead(type, fp);

		IBaseSpriteAnim *anim = nullptr;
		switch (type) {
		case AnimType::Atlas:  anim = g_alloc->make<AtlasAnim>();  break;
		case AnimType::Sprite: anim = g_alloc->make<SpriteAnim>(); break;
		default: err("unknown animation type: %d", type);
		}
		assert(anim);
		if (anim) {
			anim->read(fp, this);
			animations.emplace_back(anim);
		}
	}

	PopAllocInfo();
}

void AnimSystemSprite::save(FILE *fp) const {
	if (!fp) return; 
	PushAllocInfo("AnimSpriteSave");

	float pos_x    = transform.position.x;
	float pos_y    = transform.position.y;
	float scale_x  = transform.scale.x;
	float scale_y  = transform.scale.y;
	float origin_x = transform.origin.x;
	float origin_y = transform.origin.y;
	float rot      = transform.rotation;
	uint32_t anim_count = (uint32_t)animations.size();
	uint32_t texture_count = (uint32_t)textures.size();

	if (!textures.empty()) {
		texture_count--;
	}

	fileWrite(format_ver,    fp);
	fileWrite(pos_x,         fp);
	fileWrite(pos_y,         fp);
	fileWrite(scale_x,       fp);
	fileWrite(scale_y,       fp);
	fileWrite(origin_x,      fp);
	fileWrite(origin_y,      fp);
	fileWrite(rot,           fp);
	fileWrite(anim_count,    fp);
	fileWrite(texture_count, fp);

	if (!textures.empty()) {
		texture_count++;
	}

	for (uint32_t i = 1; i < texture_count; ++i) {
		const auto &path = texture_paths[i];
		const auto &name = texture_names[i];
		uint8_t path_len = (uint8_t)path.size();
		uint8_t name_len = (uint8_t)name.size();

		fileWrite(path_len, fp);
		fileWrite(name_len, fp);
		fileWrite(path, fp);
		fileWrite(name, fp);
	}

	for (const auto &anim : animations) {
		anim->save(fp, this);
	}
	PopAllocInfo();
}

bool AnimSystemSprite::checkFormatVersion(FILE *fp) {
	if (!fp) return false;
	uint8_t version = 0;
	fileRead(version, fp);
	return version == format_ver;
}

void AnimSystemSprite::init(Batch2D *batcher, gef::Platform &plat) {
	batch = batcher;
	platform = &plat;
	type = AnimSystemType::Sprite2D;

	PushAllocInfo("AnimSpriteInit");

	CFile default_file("animations/default.2d");
	if (default_file && checkFormatVersion(default_file)) {
		read(default_file);
		cur_animation = 0;
	}
	else {
		addInitialTexture();
	}

	transform.position = { 0, 0 };
	transform.scale = { 5, 5 };
	transform.origin = { 0.5f, 0.5f };

	PopAllocInfo();
}

void AnimSystemSprite::cleanup() {
}

int AnimSystemSprite::addTexture(const char *filename, const char *name) {
	int id = (int)textures.size();
	textures.emplace_back(loadPng(filename, *platform));
	texture_paths.emplace_back(filename);
	std::string name_str = name ? name : filename;
	if (!name) {
		// get only the name of the file
		size_t last_slash = name_str.find_last_of('/');
		size_t last_backslash = name_str.find_last_of('\\');
		size_t last_dot = name_str.find_last_of('.');
		if (last_slash == std::string::npos) last_slash = 0;
		if (last_backslash == std::string::npos) last_backslash = 0;
		if (last_dot == std::string::npos) last_dot = name_str.size() - 1;

		size_t start = std::max(last_slash, last_backslash);
		if (start) ++start;
		size_t count = last_dot - start;

		name_str = name_str.substr(start, count);
	}
	texture_names.emplace_back(std::move(name_str));
	return id;
}

void AnimSystemSprite::addInitialTexture() {
	// add invalid texture at the beginning, this way textures[0] is the "invaid" value
	// instead of needing to use an index like -1
	textures.emplace_back(nullptr);
	texture_names.emplace_back("n/a");
	texture_paths.emplace_back("null");
}
