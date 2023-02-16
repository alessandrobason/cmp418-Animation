#pragma once

#include <gef.h>
#include <system/vec.h>
#include <system/ptr.h>
#include <maths/matrix33.h>
#include <maths/matrix44.h>
#include <graphics/colour.h>
#include <graphics/shader.h>
#include <graphics/font.h>
#include <graphics/vertex_buffer.h>

#include "rect.h"

namespace gef {
	class Sprite;
	class Platform;
	class Shader;
	class Texture;
} // namespace gef 

struct Batch2DShader;

struct Batch2D {
	struct Vertex2D {
		Vertex2D() = default;
		Vertex2D(const gef::Vector2 &p, const gef::Vector2 &tc, gef::Colour c, float z)
			: x(p.x), y(p.y), u(tc.x), v(tc.y), r(c.r), g(c.g), b(c.b), a(c.a), z(z) { }
		float x, y;
		float u, v;
		float r, g, b, a;
		float z;
	};

	void init(gef::Platform &platform);
	void initConst(gef::Platform &platform, size_t batch_size = 512);
	void cleanup();

	void begin(bool clear = true);
	void end();
	void flush();
	void invalidate();

	void setZ(float new_z);
	void resetZ();
	void setTexture(const gef::Texture *new_texture);
	void drawTri(const gef::Vector2 &p1, const gef::Vector2 &p2, const gef::Vector2 &p3, const gef::Colour &c1, const gef::Colour &c2, const gef::Colour &c3);
	void drawTri(const gef::Vector2 &p1, const gef::Vector2 &p2, const gef::Vector2 &p3, const gef::Colour &c1, const gef::Colour &c2, const gef::Colour &c3, const gef::Vector2 &uv1, const gef::Vector2 &uv2, const gef::Vector2 &uv3);
	void drawRect(const gef::Rect &rec, const gef::Colour &col = gef::Colour::white);
	void drawRect(const gef::Rect &rec, float rot, const gef::Vector2 &origin, const gef::Colour &col = gef::Colour::white);
	void drawRect(const gef::Rect &dst, const gef::Rect &src, const gef::Colour &col);
	void drawRect(const gef::Rect &dst, gef::Rect src, float rot, const gef::Vector2 &origin, const gef::Colour &col);

	void drawSprite(const gef::Sprite &sprite);
	void drawSprite(const gef::Sprite &sprite, const gef::Matrix33 &transform);
	
	void drawLine(const gef::Vector2 &a, const gef::Vector2 &b, float weight = 1.f, const gef::Colour &col = gef::Colour::white);

	void drawTextF(const gef::Font *font, const gef::Vector2 &pos, const gef::Colour &colour, const char *msg, ...);
	void drawText(const gef::Font *font, const gef::Vector2 &pos, const gef::Colour &colour, const char *msg);
	void drawTextF(const gef::Font *font, const gef::Vector2 &pos, float scale, const gef::Colour &colour, gef::TextJustification justification, const char *msg, ...);
	void drawText(const gef::Font *font, const gef::Vector2 &pos, float scale, const gef::Colour &colour, gef::TextJustification justification, const char *msg);
	void drawChar(const gef::Font *font, char c, const gef::Vector2 &pos, const gef::Colour &col = gef::Colour::white, const gef::Vector2 &scale = gef::Vector2::kOne, float rot = 0.f, const gef::Vector2 &origin = { 0.5f, 0.5f });

private:
	void drawBatch();
	void updateVertexBuffer();
	void checkConstSize();

	gef::Platform *platform = nullptr;

	gef::ptr<gef::Texture> default_texture = nullptr;
	gef::Vec<Vertex2D> vertices;
	gef::ptr<gef::VertexBuffer> vbuf = nullptr;
	gef::Matrix44 proj = gef::Matrix44::kIdentity;
	gef::ptr<Batch2DShader> shader = nullptr;

	bool is_const = false;
	size_t max_vbuf_size = 0;
	const gef::Texture *texture = nullptr;
	float cur_z = 0.f;
};

struct Batch2DShader : public gef::Shader {
	Batch2DShader(const gef::Platform &platform);
	~Batch2DShader();

	void setProjection(const gef::Matrix44 &proj);
	void setTexture(const gef::Texture *texture);

private:
	Int32 proj_index = -1;
	Int32 texture_sampler_index = -1;
};