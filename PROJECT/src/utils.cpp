#include "utils.h"

#include <math.h>
#include <string.h>

#include <assets/png_loader.h>
#include <graphics/image_data.h>
#include <graphics/texture.h>
#include <graphics/mesh.h>
#include <maths/math_utils.h>
#include <maths/vector2.h>

#ifdef _WIN32
#pragma warning(disable:4996) // _CRT_SECURE_NO_WARNINGS.
#include <platform/d3d11/graphics/texture_d3d11.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#define IMGUI_DEFINE_MATH_OPERATORS
#include <external/ImGui/imgui.h>
#include <external/ImGui/imgui_internal.h>

#include <external/portable-file-dialogs/portable-file-dialogs.h>

#include "scene_loader.h"
#include "anim_system.h"

// -- file utils --

CFile::CFile(const char *filename, const char *mode) {
	open(filename, mode);
}

CFile::~CFile() {
	close();
}

bool CFile::open(const char *filename, const char *mode) {
	errno_t error = fopen_s(&fp, filename, mode);
	return error == 0;
}

bool CFile::close() {
	if (fp) {
		return fclose(fp) == 0;
	}
	return false;
}

template<>
bool fileRead(std::string &value, FILE *fp) {
	bool success = fread(&value[0], 1, value.size(), fp) == value.size();
	assert(success);
	return success;
}

template<>
bool fileWrite(const std::string &value, FILE *fp) {
	bool success = fwrite(value.c_str(), 1, value.size(), fp) == value.size();
	assert(success);
	return success;
}

// -- useful ImGui stuff --

void *imGetTexData(gef::Texture *tex) {
#ifdef _WIN32
	return tex ? ((gef::TextureD3D11 *)tex)->shader_resource_view() : nullptr;
#else
	return nullptr;
#endif
}

void imHelper(const char *msg, bool with_text) {
	if (with_text) {
		ImGui::SameLine();
		ImGui::TextDisabled("(?)");
	}
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
		ImGui::TextUnformatted(msg);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

bool imImageBtn(
	const char *str_id,
	gef::Texture *tex,
	const gef::Vector2 &btn_size,
	gef::Vector2 img_size,
	gef::Rect src
) {
	ImGuiContext &g = *ImGui::GetCurrentContext();
	ImGuiWindow *window = ImGui::GetCurrentWindow();
	if (window->SkipItems)
		return false;

	ImGuiID id = window->GetID(str_id);

	if (btn_size.x < img_size.x) {
		img_size.y = btn_size.x * img_size.y / img_size.x;
		img_size.x = btn_size.x;
	}

	if (btn_size.y < img_size.y) {
		img_size.x = btn_size.y * img_size.x / img_size.y;
		img_size.y = btn_size.y;
	}

	if (img_size == gef::Vector2::kZero) {
		img_size = btn_size;
	}

	const gef::Vector2 padding = g.Style.FramePadding;
	const gef::Vector2 img_padding = (btn_size - img_size) / 2.f + padding;
	const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + btn_size + padding * 2.0f);
	ImGui::ItemSize(bb);
	if (!ImGui::ItemAdd(bb, id))
		return false;

	bool hovered, held;
	bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

	// Render
	const ImU32 col = ImGui::GetColorU32((held && hovered) ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
	ImGui::RenderNavHighlight(bb, id);
	ImGui::RenderFrame(bb.Min, bb.Max, col, true, ImClamp((float)ImMin(padding.x, padding.y), 0.0f, g.Style.FrameRounding));
	window->DrawList->AddImage(
		imGetTexData(tex),
		bb.Min + img_padding,
		bb.Max - img_padding,
		src.pos,
		src.pos + src.size
	);

	return pressed;
}

bool imBtnFillWidth(const char *label, float height) {
	float width = ImGui::GetContentRegionAvail().x;
	return ImGui::Button(label, { width, height });
}

static int inputTextCB(ImGuiInputTextCallbackData *data) {
	std::string *str = (std::string *)data->UserData;
	if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
		// Resize string callback
		// If for some reason we refuse the new length (BufTextLen) and/or capacity (BufSize) we need to set them back to what we want.
		IM_ASSERT(data->Buf == str->c_str());
		str->resize(data->BufTextLen);
		data->Buf = (char *)str->c_str();
	}
	return 0;
}

bool imInputText(const char *label, std::string &str) {
	return ImGui::InputText(label, &str[0], str.capacity() + 1, ImGuiInputTextFlags_CallbackResize, inputTextCB, &str);
}

int imSaveAndRead(AnimSystem *system, const char *desc, const char *filetype, std::string *name) {
	int result = ResultNone;
	
	if (ImGui::Button("Save", { 100, 30 })) {
		std::string destination = pfd::save_file::save_file(
			"Save file",
			".",
			{ desc, filetype }
		).result();
		if (!destination.empty()) {
			const char *typeonly = filetype + 1;
			if (!strEndsWith(destination, typeonly)) {
				destination += typeonly;
			}
			system->saveFile(destination.c_str());
			if (name) *name = destination;
			result = ResultSave;
		}
	}

	ImGui::SameLine();

	if (ImGui::Button("Load", { 100, 30 })) {
		std::vector<std::string> dest = pfd::open_file::open_file(
			"Open file",
			".",
			{ desc, filetype },
			pfd::opt::none
		).result();
		assert(dest.size() <= 1);
		if (!dest.empty()) {
			CFile fp(dest[0].c_str());
			if (!system->checkFormatVersion(fp)) {
				pfd::button choice = pfd::message::message(
					"Warning",
					"Version mismatch, do you want to continue? this might crash the "
					"application or result in corrupted data",
					pfd::choice::yes_no,
					pfd::icon::warning
				).result();
				if (choice == pfd::button::yes) {
					system->read(fp);
					if (name) *name = dest[0];
					result = ResultRead;
				}
			}
			else {
				system->read(fp);
				if (name) *name = dest[0];
				result = ResultRead;
			}
		}
	}

	return result;
}

// -- loading helper --

gef::ptr<gef::Texture> loadPng(const char *png_filename, gef::Platform &platform, IAllocator *allocator) {
	gef::PNGLoader png_loader;
	gef::ImageData image_data;

	// load image data from PNG file 
	png_loader.Load(png_filename, platform, image_data);

	// if the image data is valid, then create a texture from it
	if (image_data.image())
		return gef::Texture::Create(platform, image_data, allocator);

	return nullptr;
}

gef::ptr<gef::Mesh> loadMesh(const char *filename, gef::Platform &platform) {
	SceneLoader loader;
	if (loader.loadScene(platform, filename)) {
		loader.createMaterials(platform);
		loader.createMeshes(platform);
		return loader.popFirstMesh();
	}
	return nullptr;
}


// -- logging helpers --

static constexpr int max_tracelog_msg = 1024;

void traceLog(LogLevel level, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	traceLogVaList(level, fmt, args);
	va_end(args);
}

void traceLogVaList(LogLevel level, const char *fmt, va_list args) {
	char buffer[max_tracelog_msg];
	memset(buffer, 0, sizeof(buffer));

	const char *beg;
	switch (level) {
	case LogLevel::Trace: beg = "[TRACE]: ";   break;
	case LogLevel::Debug: beg = "[DEBUG]: ";   break;
	case LogLevel::Info:  beg = "[INFO]: ";    break;
	case LogLevel::Warn:  beg = "[WARNING]: "; break;
	case LogLevel::Error: beg = "[ERROR]: ";   break;
	case LogLevel::Fatal: beg = "[FATAL]: ";   break;
	default:              beg = "";                              break;
	}

	size_t offset = 0;
	offset = strlen(beg);
	strCopyInto(buffer, beg);
	vsnprintf(buffer + offset, sizeof(buffer) - offset, fmt, args);

	OutputDebugStringA(buffer);
	OutputDebugStringA("\n");

	if (level == LogLevel::Fatal) {
		abort();
	}
}

// -- useful matrix stuff --

gef::Matrix33 mat3FromMat4(const gef::Matrix44 &t) {
	gef::Matrix33 o;

	o.m[0][0] = t.m(0, 0);
	o.m[0][1] = t.m(0, 1);
	o.m[0][2] = 0.f;

	o.m[1][0] = t.m(1, 0);
	o.m[1][1] = t.m(1, 1);
	o.m[1][2] = 0.f;

	o.m[2][0] = t.m(3, 0);
	o.m[2][1] = t.m(3, 1);
	o.m[2][2] = 1.f;

	return o;
}

gef::Matrix33 mat3FromPos(const gef::Vector2 &pos) {
	gef::Matrix33 o = gef::Matrix33::kIdentity;
	o.m[2][0] = pos.x;
	o.m[2][1] = pos.y;
	return o;
}

gef::Matrix33 mat3FromRot(float rot) {
	gef::Matrix33 o = gef::Matrix33::kIdentity;
	float c = cosf(rot);
	float s = sinf(rot);
	o.m[0][0] = c;
	o.m[0][1] = s;
	o.m[1][0] = -s;
	o.m[1][1] = c;
	return o;
}

gef::Matrix33 mat3FromScale(const gef::Vector2 &scale) {
	gef::Matrix33 o = gef::Matrix33::kIdentity;
	o.m[0][0] = scale.x;
	o.m[1][1] = scale.y;
	return o;
}

gef::Matrix33 mat3FromPosRotScale(
	const gef::Vector2 &pos, 
	float rot, 
	const gef::Vector2 &scale
) {
	gef::Matrix33 o;

	float c = cosf(rot);
	float s = sinf(rot);

	o.m[0][0] = c;
	o.m[0][1] = s;
	o.m[0][2] = 0.f;

	o.m[1][0] = -s;
	o.m[1][1] = c;
	o.m[1][2] = 0.f;

	o.m[2][0] = pos.x;
	o.m[2][1] = pos.y;
	o.m[2][2] = 1.f;

	return mat3FromScale(scale) * o;
}

gef::Matrix44 mat4FromPosScale(const gef::Vector4 &pos, const gef::Vector4 &scale) {
	gef::Matrix44 o;

	o.SetRow(0, { scale.x(), 0.f,       0.f,       0.f });
	o.SetRow(1, { 0.f,       scale.y(), 0.f,       0.f });
	o.SetRow(2, { 0.f,       0.f,       scale.z(), 0.f });
	o.SetRow(3, { pos.x(),   pos.y(),   pos.z(),   1.f });

	return o;
}

gef::Matrix44 mat4FromPosRotScale(
	const gef::Vector2 &pos, 
	float rot, 
	const gef::Vector2 &scale
) {
	gef::Matrix44 o;
	gef::Matrix44 scale_mat;
	scale_mat.Scale({ scale.x, scale.y, 1.f });

	float c = cosf(rot);
	float s = sinf(rot);

	o.set_m(0, 0, c);
	o.set_m(0, 1, 0.f);
	o.set_m(0, 2, -s);
	o.set_m(0, 3, 0.f);

	o.set_m(1, 0, 0.f);
	o.set_m(1, 1, 1.f);
	o.set_m(1, 2, 0.f);
	o.set_m(1, 3, 0.f);

	o.set_m(2, 0, s);
	o.set_m(2, 1, 0.f);
	o.set_m(2, 2, c);
	o.set_m(2, 3, 0.f);

	o.set_m(3, 0, pos.x);
	o.set_m(3, 1, pos.y);
	o.set_m(3, 2, 0.f);
	o.set_m(3, 3, 1.f);

	return scale_mat * o;
}

// -- useful stuff for tweening

float tweenGetAngleDiff(float start, float end) {
	float diff = end - start;
	if (diff > gef::DegToRad(180.f)) {
		diff -= gef::DegToRad(355.f);
	}
	if (diff < gef::DegToRad(-180.f)) {
		diff += gef::DegToRad(355.f);
	}
	return diff;
}

float angleLerp(float start, float diff, float t) {
	return start + diff * t;
}

// -- string helpers --

gef::ptr<char> strfmt(const char *fmt, ...) {
	va_list va;
	va_start(va, fmt);
	gef::ptr<char> out = strfmtv(fmt, va);
	va_end(va);
	return out;
}

gef::ptr<char> strfmtv(const char *fmt, va_list vlist) {
	va_list vcopy;
	va_copy(vcopy, vlist);
	int len = vsnprintf(nullptr, 0, fmt, vcopy);
	va_end(vcopy);
	char *out = (char *)g_alloc->allocDebug(len + 1, "strfmtv");
	int actual_len = vsnprintf(out, len + 1, fmt, vlist);
	assert(actual_len == len);
	out[len] = '\0';
	return out;
}

gef::ptr<char> strcopy(const char *src) {
	size_t len = strlen(src);
	char *buf = g_alloc->allocDebug<char>(len + 1, "strcopy");
	memcpy(buf, src, len);
	return buf;
}

bool strEndsWith(const std::string &str, const char *ends) {
	size_t endlen = strlen(ends);
	if (str.size() < endlen) return false;
	return memcmp(&str[str.size() - endlen], ends, endlen) == 0;
}
