#include "batch2d.h"

#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

#include <system/platform.h>
#include <system/allocator.h>
#include <graphics/texture.h>
#include <graphics/shader_interface.h>
#include <graphics/vertex_buffer.h>
#include <graphics/sprite.h>
#include <maths/math_utils.h>

#include "utils.h"

#ifdef _WIN32
#include <platform/d3d11/system/platform_d3d11.h>
#include <platform/d3d11/graphics/shader_interface_d3d11.h>
#endif

#define check(cond, ...) do { if(!(cond)) { err(__VA_ARGS__); return; } } while(0)

void Batch2D::init(gef::Platform &in_platform) {
	platform = &in_platform;
	if (vertices.capacity() == 0) {
		vertices.reserve(512);
	}
	PushAllocInfo("Batch2D");
	shader = gef::ptr<Batch2DShader>::make(in_platform);
	default_texture = gef::Texture::CreateCheckerTexture(16, 1, in_platform);
	PopAllocInfo();
}

void Batch2D::initConst(gef::Platform &in_platform, size_t batch_size) {
	// make the batch size a multiple of 6
	batch_size -= batch_size % 6;
	vertices.reserve(batch_size);
	max_vbuf_size = batch_size;
	is_const = true;

	PushAllocInfo("Batch2DConst");
	vbuf = gef::VertexBuffer::Create(in_platform);
	vbuf->Init(in_platform, vertices.data(), (UInt32)batch_size, sizeof(Vertex2D), false);
	PopAllocInfo();

	init(in_platform);
}

void Batch2D::cleanup() {
	if (vbuf) {
		vbuf->set_vertex_data(nullptr);
	}
}

void Batch2D::begin(bool clear) {
	if (!platform) {
		return;
	}

	if (clear) {
		platform->Clear();
	}

	proj = platform->OrthographicFrustum(
		0.0f, (float)platform->width(),
		0.0f, (float)platform->height(),
		-1.0f, 1.f
	);

	shader->device_interface()->UseProgram();
	shader->setProjection(proj);
	shader->device_interface()->SetVertexFormat();
}

void Batch2D::end() {
	flush();

	shader->device_interface()->UnbindTextureResources(*platform);

	if (vbuf && platform) {
		vbuf->Unbind(*platform);
	}
}

void Batch2D::flush() {
	drawBatch();
}

void Batch2D::invalidate() {
	texture = nullptr;
	vertices.clear();
}

void Batch2D::setZ(float new_z) {
	cur_z = new_z;
}

void Batch2D::resetZ() {
	cur_z = 0.f;
}

void Batch2D::setTexture(const gef::Texture *new_texture) {
	if (new_texture != texture) {
		flush();
		texture = new_texture;
	}
}

void Batch2D::drawTri(const gef::Vector2 &p1, const gef::Vector2 &p2, const gef::Vector2 &p3, const gef::Colour &c1, const gef::Colour &c2, const gef::Colour &c3) {
	drawTri(p1, p2, p3, c1, c2, c3, gef::Vector2::kZero, gef::Vector2::kZero, gef::Vector2::kZero);
}

void Batch2D::drawTri(const gef::Vector2 &p1, const gef::Vector2 &p2, const gef::Vector2 &p3, const gef::Colour &c1, const gef::Colour &c2, const gef::Colour &c3, const gef::Vector2 &uv1, const gef::Vector2 &uv2, const gef::Vector2 &uv3) {
	vertices.emplace_back(p1, uv1, c1, cur_z);
	vertices.emplace_back(p2, uv2, c2, cur_z);
	vertices.emplace_back(p3, uv3, c3, cur_z);

	if (is_const) {
		checkConstSize();
	}
}

void Batch2D::drawRect(const gef::Rect &rec, const gef::Colour &col) {
	drawRect(rec, rec, 0.f, gef::Vector2::kZero, col);
}

void Batch2D::drawRect(const gef::Rect &rec, float rot, const gef::Vector2 &origin, const gef::Colour &col) {
	drawRect(rec, rec, rot, origin, col);
}

void Batch2D::drawRect(const gef::Rect &dst, const gef::Rect &src, const gef::Colour &col) {
	drawRect(dst, src, 0.f, gef::Vector2::kZero, col);
}

void Batch2D::drawRect(const gef::Rect &dst, gef::Rect src, float rot, const gef::Vector2 &origin, const gef::Colour &col) {
	bool flip_x = false;

	if (src.w < 0) { flip_x = true; src.w *= -1; }
	if (src.h < 0) src.y -= src.h;

	gef::Vector2 top_left = gef::Vector2::kZero;
	gef::Vector2 top_right = gef::Vector2::kZero;
	gef::Vector2 bottom_left = gef::Vector2::kZero;
	gef::Vector2 bottom_right = gef::Vector2::kZero;

	// Only calculate rotation if needed
	if (rot == 0.0f) {
		float x = dst.x - origin.x;
		float y = dst.y - origin.y;
		top_left = { x, y };
		top_right = { x + dst.w, y };
		bottom_left = { x, y + dst.h };
		bottom_right = { x + dst.w, y + dst.h };
	}
	else {
		float sin_rot = sinf(gef::DegToRad(rot));
		float cos_rot = cosf(gef::DegToRad(rot));
		float x = dst.x;
		float y = dst.y;
		float dx = -origin.x;
		float dy = -origin.y;

		top_left.x = x + dx * cos_rot - dy * sin_rot;
		top_left.y = y + dx * sin_rot + dy * cos_rot;

		top_right.x = x + (dx + dst.w) * cos_rot - dy * sin_rot;
		top_right.y = y + (dx + dst.w) * sin_rot + dy * cos_rot;

		bottom_left.x = x + dx * cos_rot - (dy + dst.h) * sin_rot;
		bottom_left.y = y + dx * sin_rot + (dy + dst.h) * cos_rot;

		bottom_right.x = x + (dx + dst.w) * cos_rot - (dy + dst.h) * sin_rot;
		bottom_right.y = y + (dx + dst.w) * sin_rot + (dy + dst.h) * cos_rot;
	}

	gef::Vector2 img_size = gef::Vector2::kOne;

	if (texture) {
		img_size = texture->GetSize();
	}

	gef::Vector2 uv_tl = src.pos / img_size;
	gef::Vector2 uv_bl = (src.pos + src.size) / img_size;

	if (flip_x) {
		vertices.emplace_back(top_left, gef::Vector2(uv_bl.x, uv_tl.y), col, cur_z);
		vertices.emplace_back(top_right, uv_tl, col, cur_z);
		vertices.emplace_back(bottom_right, gef::Vector2(uv_tl.x, uv_bl.y), col, cur_z);

		vertices.emplace_back(bottom_right, gef::Vector2(uv_tl.x, uv_bl.y), col, cur_z);
		vertices.emplace_back(bottom_left, uv_bl, col, cur_z);
		vertices.emplace_back(top_left, uv_tl, col, cur_z);
	}
	else {
		vertices.emplace_back(top_left, uv_tl, col, cur_z);
		vertices.emplace_back(top_right, gef::Vector2(uv_bl.x, uv_tl.y), col, cur_z);
		vertices.emplace_back(bottom_right, uv_bl, col, cur_z);
		
		vertices.emplace_back(bottom_right, uv_bl, col, cur_z);
		vertices.emplace_back(bottom_left, gef::Vector2(uv_tl.x, uv_bl.y), col, cur_z);
		vertices.emplace_back(top_left, uv_tl, col, cur_z);
	}

	if (is_const) {
		checkConstSize();
	}
}

void Batch2D::drawSprite(const gef::Sprite &sprite) {
	setTexture(sprite.texture());
	drawRect(
		{ 
			sprite.position().x(),
			sprite.position().y(),
			sprite.width(), 
			sprite.height()
		},
		{ 
			sprite.uv_position(), 
			gef::Vector2(sprite.uv_width(), sprite.uv_height()) * sprite.texture()->GetSize()
		},
		sprite.rotation(),
		gef::Vector2(sprite.width(), sprite.height()) / 2.f,
		gef::Colour::fromAGBR(sprite.colour())
	);
}

void Batch2D::drawSprite(const gef::Sprite &sprite, const gef::Matrix33 &transform) {
	setTexture(sprite.texture());
	
	float w = sprite.width();
	float h = sprite.height();
	float x = sprite.position().x() - w / 2.f;
	float y = sprite.position().y() - h / 2.f;

	gef::Vector2 top_left     = gef::Vector2::Transform(transform, { x, y });
	gef::Vector2 top_right    = gef::Vector2::Transform(transform, { x + w, y });
	gef::Vector2 bottom_left  = gef::Vector2::Transform(transform, { x, y + h });
	gef::Vector2 bottom_right = gef::Vector2::Transform(transform, { x + w, y + h });

	gef::Vector2 uv_tl = sprite.uv_position();
	gef::Vector2 uv_bl = uv_tl + gef::Vector2(sprite.uv_width(), sprite.uv_height());

	gef::Colour col = gef::Colour::fromAGBR(sprite.colour());

	vertices.emplace_back(top_left, uv_tl, col, cur_z);
	vertices.emplace_back(top_right, gef::Vector2(uv_bl.x, uv_tl.y), col, cur_z);
	vertices.emplace_back(bottom_right, uv_bl, col, cur_z);

	vertices.emplace_back(bottom_right, uv_bl, col, cur_z);
	vertices.emplace_back(bottom_left, gef::Vector2(uv_tl.x, uv_bl.y), col, cur_z);
	vertices.emplace_back(top_left, uv_tl, col, cur_z);

	if (is_const) {
		checkConstSize();
	}
}

void Batch2D::drawLine(
	const gef::Vector2 &a, 
	const gef::Vector2 &b, 
	float weight, 
	const gef::Colour &col
) {
	// if they are on a line, simply draw a rectangle
	if (a.x == b.x || a.y == b.y) {
		gef::Rect rec = { a, { b - a } };
		rec.size += weight / 2.f;
		rec.pos -= weight / 2.f;
		drawRect(rec, col);
	}
	// otherwise do cool calculations
	else {
		gef::Vector2 delta = { b.x - a.x, b.y - a.y };
		float length = sqrtf(delta.x * delta.x + delta.y * delta.y);

		if ((length > 0) && (weight > 0)) {
			float scale = weight / (2 * length);
			gef::Vector2 radius = { -scale * delta.y, scale * delta.x };
			gef::Vector2 strip[4] = {
				{ a.x - radius.x, a.y - radius.y },
				{ a.x + radius.x, a.y + radius.y },
				{ b.x - radius.x, b.y - radius.y },
				{ b.x + radius.x, b.y + radius.y }
			};

			drawTri(strip[2], strip[1], strip[0], col, col, col);
			drawTri(strip[1], strip[2], strip[3], col, col, col);
		}
	}
}


void Batch2D::drawTextF(const gef::Font *font, const gef::Vector2 &pos, const gef::Colour &colour, const char *msg, ...) {
	va_list va;
	va_start(va, msg);
	char buf[255];
	int len = vsnprintf(buf, sizeof(buf), msg, va);
	va_end(va);
	if (len >= sizeof(buf)) {
		len = sizeof(buf) - 1;
	}
	buf[len] = '\0';
	drawText(font, pos, colour, buf);
}

void Batch2D::drawText(const gef::Font *font, const gef::Vector2 &pos, const gef::Colour &colour, const char *msg) {
	drawText(font, pos, 1.f, colour, gef::TJ_LEFT, msg);
}

void Batch2D::drawTextF(const gef::Font *font, const gef::Vector2 &pos, float scale, const gef::Colour &colour, gef::TextJustification justification, const char *msg, ...) {
	va_list va;
	va_start(va, msg);
	char buf[255];
	int len = vsnprintf(buf, sizeof(buf), msg, va);
	va_end(va);
	if (len >= sizeof(buf)) {
		len = sizeof(buf) - 1;
	}
	buf[len] = '\0';
	drawText(font, pos, colour, buf);
}

void Batch2D::drawText(const gef::Font *font, const gef::Vector2 &pos, float scale, const gef::Colour &colour, gef::TextJustification justification, const char *msg) {
	if (!msg) {
		return;
	}

	UInt32 character_count = (UInt32)strlen(msg);
	float string_length = font->GetStringLength(msg);

	gef::Vector2 cursor = pos;

	switch (justification) {
	case gef::TJ_CENTRE:
		cursor.x -= string_length * 0.5f * scale;
		break;
	case gef::TJ_RIGHT:
		cursor.x -= string_length * scale;
		break;
	default:
		break;
	}

	const gef::Font::Charset &charset = font->charset();
	setTexture(font->font_texture());
	
	for (UInt32 i = 0; i < character_count; ++i) {
		const auto &ch = charset.Chars[(UInt32)msg[i]];

		gef::Rect src = {
			(float)ch.x, (float)ch.y,
			(float)ch.Width, (float)ch.Height
		};

		gef::Rect dst = { gef::Vector2::kZero, src.size * scale };
		dst.pos = {
			cursor.x + (float)ch.XOffset * scale,
			cursor.y + (float)ch.YOffset * scale
		};

		drawRect(dst, src, colour);
		cursor.x += ((float)ch.XAdvance) * scale;
	}
}

void Batch2D::drawChar(
	const gef::Font *font, 
	char c, 
	const gef::Vector2 &pos, 
	const gef::Colour &col, 
	const gef::Vector2 &scale,
	float rot, 
	const gef::Vector2 &origin
) {
	setTexture(font->font_texture());
	const auto &ch = font->charset().Chars[(UInt32)c];

	gef::Rect src = {
		(float)ch.x, (float)ch.y,
		(float)ch.Width, (float)ch.Height
	};

	gef::Rect dst = { gef::Vector2::kZero, src.size * scale };
	dst.pos = {
		pos.x + (float)ch.XOffset * scale.x,
		pos.y + (float)ch.YOffset * scale.y
	};

	drawRect(dst, src, rot, origin, col);
}

void Batch2D::drawBatch() {
	if (!platform || vertices.empty()) {
		return;
	}

	updateVertexBuffer();

	if (!vbuf) {
		warn("vertex buffer is null");
		return;
	}

	vbuf->Bind(*platform);

	shader->device_interface()->SetVariableData();
	shader->setTexture(texture ? texture : default_texture.get());
	shader->device_interface()->BindTextureResources(*platform);

#ifdef _WIN32
	gef::PlatformD3D11 *platform_d3d = (gef::PlatformD3D11 *)platform;
	platform_d3d->device_context()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	platform_d3d->device_context()->Draw((UINT)vertices.size(), 0);
#else
	// TODO vita support
#endif

	texture = nullptr;
	vertices.clear();
}

void Batch2D::checkConstSize() {
	if (vertices.size() == max_vbuf_size) {
		const gef::Texture *old_tex = texture;
		flush();
		texture = old_tex;
	}
}

void Batch2D::updateVertexBuffer() {
	PushAllocInfo("Batch2DUpdateVbuf");

	if (!vbuf || (max_vbuf_size < vertices.size() && !is_const)) {
		if (vbuf) {
			vbuf->set_vertex_data(nullptr);
		}
		vbuf = gef::VertexBuffer::Create(*platform);
		vbuf->Init(*platform, vertices.data(), (UInt32)vertices.size(), sizeof(Vertex2D), false);
		info("resizing Batch2D from %zu to %zu", max_vbuf_size, vertices.size());
		max_vbuf_size = vertices.size();
	}
	else {
		vbuf->set_num_vertices((UInt32)vertices.size());
		vbuf->set_vertex_data(vertices.data());
		vbuf->Update(*platform);
	}

	PopAllocInfo();
}

Batch2DShader::Batch2DShader(const gef::Platform &platform) 
	: Shader(platform)
{
	bool success = true;

	// load vertex shader source in from a file
	char *vs_shader_source = NULL;
	Int32 vs_shader_source_length = 0;
	success = LoadShader("batch2d_vs", "shaders", &vs_shader_source, vs_shader_source_length, platform);

	char *ps_shader_source = NULL;
	Int32 ps_shader_source_length = 0;
	success = LoadShader("batch2d_ps", "shaders", &ps_shader_source, ps_shader_source_length, platform);

	device_interface_->SetVertexShaderSource(vs_shader_source, vs_shader_source_length);
	device_interface_->SetPixelShaderSource(ps_shader_source, ps_shader_source_length);

	g_alloc->dealloc(vs_shader_source);
	g_alloc->dealloc(ps_shader_source);
	vs_shader_source = NULL;
	ps_shader_source = NULL;

	proj_index = device_interface_->AddVertexShaderVariable("proj", gef::ShaderInterface::kMatrix44);
	texture_sampler_index = device_interface_->AddTextureSampler("texture_sampler");

	device_interface_->AddVertexParameter(
		"position", 
		gef::ShaderInterface::kVector2, 
		offsetof(Batch2D::Vertex2D, x), 
		"POSITION", 
		0
	);
	device_interface_->AddVertexParameter(
		"uv", 
		gef::ShaderInterface::kVector2,
		offsetof(Batch2D::Vertex2D, u), 
		"TEXCOORD", 
		0
	);
	device_interface_->AddVertexParameter(
		"colour", 
		gef::ShaderInterface::kVector4,
		offsetof(Batch2D::Vertex2D, r), 
		"COLOR", 
		0
	);
	device_interface_->AddVertexParameter(
		"z", 
		gef::ShaderInterface::kFloat,
		offsetof(Batch2D::Vertex2D, z), 
		"ZVALUE", 
		0
	);
	device_interface_->set_vertex_size(sizeof(Batch2D::Vertex2D));
	device_interface_->CreateVertexFormat();

#ifdef _WIN32
	gef::ShaderInterfaceD3D11 *shader_interface_d3d11 = static_cast<gef::ShaderInterfaceD3D11 *>(device_interface_);

	// Create a texture sampler state description.
	D3D11_SAMPLER_DESC sampler_desc;
	sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
	sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampler_desc.MipLODBias = 0.0f;
	sampler_desc.MaxAnisotropy = 1;
	sampler_desc.ComparisonFunc = D3D11_COMPARISON_GREATER_EQUAL;
	sampler_desc.BorderColor[0] = 0;
	sampler_desc.BorderColor[1] = 0;
	sampler_desc.BorderColor[2] = 0;
	sampler_desc.BorderColor[3] = 0;
	sampler_desc.MinLOD = 0;
	sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;

	shader_interface_d3d11->AddSamplerState(sampler_desc);
#endif

	success = device_interface_->CreateProgram();
}

Batch2DShader::~Batch2DShader() {
}

void Batch2DShader::setProjection(const gef::Matrix44 &proj) {
	gef::Matrix44 projT;
	projT.Transpose(proj);
	device_interface_->SetVertexShaderVariable(proj_index, &projT);
}

void Batch2DShader::setTexture(const gef::Texture *texture) {
	device_interface_->SetTextureSampler(texture_sampler_index, texture);
}
