#pragma once

#include <system/ptr.h>
#include <maths/vector2.h>

#include "rect.h"

namespace gef {
	class Texture;
	class Platform;
} // namespace gef

class AnimSystemSprite;
struct AtlasAnim;
struct SpriteAnim;

struct Frame2DEditor {
	void init(AnimSystemSprite *sprite_sys, gef::Platform &plat);
	void cleanup();
	void draw();

	void open() { is_open = true; }
	void close() { is_open = false; }

private:
	void drawNewAnimationPicker(char *name_buf, size_t name_len);

	void drawAtlasAnimation(AtlasAnim *anim);
	void drawSpriteAnimation(SpriteAnim *anim);

	bool drawAtlasPicker(float &length, int &id, gef::Texture *atlas, int columns, int rows);
	bool drawSpritePicker(gef::Texture *&texture, gef::Rect &source, gef::Vector2 &offset, float &length, bool is_new_frame = false);

	AnimSystemSprite *system = nullptr;
	bool is_open = false;
	gef::ptr<gef::Texture> plus_texture;
	gef::Vector2 button_size = { 100, 100 };
};