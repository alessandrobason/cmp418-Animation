#pragma once

#include <maths/vector2.h>
#include <graphics/colour.h>

#include "rect.h"

namespace gef {
	class Texture;
	class Platform;
	class Matrix33;
} // namespace gef

class AnimSystemSke2D;
struct AnimationSke2D;
struct Slot;
struct Batch2D;

struct Ske2DEditor {
	void init(AnimSystemSke2D *ske2d_sys, Batch2D *ren);
	void cleanup();
	void draw();

	void open() { is_open = true; }
	void close() { is_open = false; }

private:
	void drawSlot(Slot *slot);
	void drawBone(Slot *slot, gef::Matrix33 &transform);
	void drawBoneEditor(Slot *slot);
	void drawAnimation(AnimationSke2D &anim);

	AnimSystemSke2D *system = nullptr;
	Slot *selected = nullptr;
	bool is_open = false;
	Batch2D *renderer = nullptr;
	gef::Colour bone_colour = gef::Colour::red;
	gef::Colour selected_colour = gef::Colour::true_blue;
};