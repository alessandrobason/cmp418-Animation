#pragma once

#include <system/vec.h>
#include <unordered_map>

#include <system/ptr.h>
#include <graphics/sprite.h>
#include <graphics/texture.h>

#include "anim_system.h"
#include "rect.h"

namespace gef {
	class Platform;
}

struct Batch2D;
class AnimSystemSprite;

struct Transform2D {
	gef::Vector2 position = gef::Vector2::kZero;
	gef::Vector2 scale = gef::Vector2::kOne;
	// origin is in [0, 1] range as each frame can have a very different size
	// and pixel-style origin would be unusable otherwise
	gef::Vector2 origin = gef::Vector2::kZero;
	float rotation = 0.f;
};

struct IBaseSpriteAnim {
	virtual ~IBaseSpriteAnim() {}
	virtual bool update(float dt) = 0;
	virtual void draw(Batch2D *batch, const Transform2D &transform) = 0;
	virtual void read(FILE *fp, AnimSystemSprite *system) = 0;
	virtual void save(FILE *fp, const AnimSystemSprite *system) const = 0;

	char debug_name[16];
};

// atlas-based animation, each frame can be of different length but they're all the same
// size. this way it only needs to store the index
struct AtlasAnim : public IBaseSpriteAnim {
	virtual bool update(float dt) override;
	virtual void draw(Batch2D *batch, const Transform2D &transform) override;
	virtual void read(FILE *fp, AnimSystemSprite *system) override;
	virtual void save(FILE *fp, const AnimSystemSprite *system) const override;

	struct Frame {
		Frame() = default;
		Frame(int id, float len) : id(id), length(len) {}
		int id = 0;
		float length = 0.1f;
	};

	gef::Texture *atlas = nullptr;
	int columns = 0;
	int rows = 0;
	gef::Vec<Frame> frames;
	int current_frame = 0;
	float timer = 0.f;
};

// very flexible sprite animation, every frame can be a subrect of a texture,
// the texture and length of the animation can change each frame
struct SpriteAnim : IBaseSpriteAnim {
	virtual bool update(float dt) override;
	virtual void draw(Batch2D *batch, const Transform2D &transform) override;
	virtual void read(FILE *fp, AnimSystemSprite *system) override;
	virtual void save(FILE *fp, const AnimSystemSprite *system) const override;

	struct Frame {
		Frame() = default;
		Frame(const gef::Rect &s, float l, gef::Texture *t, const gef::Vector2 &o = gef::Vector2::kZero) 
			: source(s), length(l), texture(t), offset(o) { }
		Frame(const gef::Rect &s, gef::Texture *t) : source(s), texture(t) {}
		gef::Rect source;
		gef::Vector2 offset = gef::Vector2::kZero;
		float length = 0.1f;
		gef::Texture *texture = nullptr;
	};

	gef::Vec<Frame> frames;
	int current_frame = 0;
	float timer = 0.f;
};

class AnimSystemSprite : public AnimSystem {
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

	void init(Batch2D *batcher, gef::Platform &plat);
	void cleanup();

	int addTexture(const char *filename, const char *name = nullptr);

	gef::Vec<gef::ptr<IBaseSpriteAnim>> &getAnimations() { return animations; }
	const gef::Vec<gef::ptr<IBaseSpriteAnim>> &getAnimations() const { return animations; }

	gef::Vec<gef::ptr<gef::Texture>> &getTextures() { return textures; }
	const gef::Vec<gef::ptr<gef::Texture>> &getTextures() const { return textures; }

	gef::Vec<std::string> &getTexturesName() { return texture_names; }
	const gef::Vec<std::string> &getTexturesName() const { return texture_names; }

private:
	void addInitialTexture();

	gef::Platform *platform = nullptr;
	Batch2D *batch = nullptr;
	gef::Vec<gef::ptr<gef::Texture>> textures;
	gef::Vec<std::string> texture_names;
	gef::Vec<std::string> texture_paths;
	gef::Vec<gef::ptr<IBaseSpriteAnim>> animations;
	Transform2D transform;
};
