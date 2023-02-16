#pragma once

#include <stdio.h>
#include <stdarg.h>
#include <string>

#include <system/ptr.h>
#include <maths/matrix33.h>
#include <maths/matrix44.h>

#include <external/ImGui/imgui.h>

#include "rect.h"

namespace gef {
	class Platform;
	class Texture;
	class Mesh;
	class Vector2;
}

class AnimSystem;

// -- file utilities --

struct CFile {
	CFile() = default;
	CFile(const char *filename, const char *mode = "rb");
	~CFile();
	bool open(const char *filename, const char *mode = "rb");
	bool close();
	operator FILE *() { return fp; }
	FILE *fp = nullptr;
};

template<typename T>
bool fileRead(T &value, FILE *fp) {
	bool success = fread(&value, 1, sizeof(T), fp) == sizeof(T);
	assert(success);
	return success;
}

template<typename T>
bool fileWrite(const T &value, FILE *fp) {
	bool success = fwrite(&value, 1, sizeof(T), fp) == sizeof(T);
	assert(success);
	return success;
}

template<>
bool fileRead(std::string &value, FILE *fp);

template<>
bool fileWrite(const std::string &value, FILE *fp);

// -- useful ImGui stuff --

enum { ResultNone, ResultSave, ResultRead };

void *imGetTexData(gef::Texture *tex);
void imHelper(const char *msg, bool with_text = true);
bool imImageBtn(const char *str_id, gef::Texture *tex, const gef::Vector2 &btn_size, gef::Vector2 img_size = { 0, 0 }, gef::Rect src = { 0, 0, 1, 1 });
bool imBtnFillWidth(const char *label, float height = 25.f);
bool imInputText(const char *label, std::string &str);
int imSaveAndRead(AnimSystem *system, const char *desc, const char *filetype, std::string *name = nullptr);

// -- loading helper --

gef::ptr<gef::Texture> loadPng(const char *png_filename, gef::Platform &platform, IAllocator *allocator = g_alloc);
gef::ptr<gef::Mesh> loadMesh(const char *filename, gef::Platform &platform);

// -- logging helpers --

#ifdef NDEBUG
#define DISABLE_LOGGING
#endif

enum class LogLevel {
    All, Trace, Debug, Info, Warn, Error, Fatal
};

void traceLog(LogLevel level, const char *fmt, ...);
void traceLogVaList(LogLevel level, const char *fmt, va_list args);

#ifdef DISABLE_LOGGING
#define tall(...)  
#define trace(...) 
#define debug(...) 
#define info(...)  
#define warn(...)  
#define err(...)   
#define fatal(...) 
#else
#define tall(...)  traceLog(LogLevel::All,   __VA_ARGS__)
#define trace(...) traceLog(LogLevel::Trace, __VA_ARGS__)
#define debug(...) traceLog(LogLevel::Debug, __VA_ARGS__)
#define info(...)  traceLog(LogLevel::Info,  __VA_ARGS__)
#define warn(...)  traceLog(LogLevel::Warn,  __VA_ARGS__)
#define err(...)   traceLog(LogLevel::Error, __VA_ARGS__)
#define fatal(...) traceLog(LogLevel::Fatal, __VA_ARGS__)
#endif

// -- useful matrix stuff --

gef::Matrix33 mat3FromMat4(const gef::Matrix44 &t);
gef::Matrix33 mat3FromPos(const gef::Vector2 &pos);
gef::Matrix33 mat3FromRot(float rot);
gef::Matrix33 mat3FromScale(const gef::Vector2 &scale);
gef::Matrix33 mat3FromPosRotScale(const gef::Vector2 &pos, float rot, const gef::Vector2 &scale);
gef::Matrix44 mat4FromPosScale(const gef::Vector4 &pos, const gef::Vector4 &scale);
gef::Matrix44 mat4FromPosRotScale(const gef::Vector2 &pos, float rot, const gef::Vector2 &scale);

// -- useful stuff for tweening --

float tweenGetAngleDiff(float start, float end);
float angleLerp(float start, float diff, float t);

// -- string helpers --

gef::ptr<char> strfmt(const char *fmt, ...);
gef::ptr<char> strfmtv(const char *fmt, va_list vlist);
gef::ptr<char> strcopy(const char *src);
bool strEndsWith(const std::string &str, const char *ends);
template<size_t N>
void strCopyInto(char (&dst)[N], const char *src) {
	strncpy_s(dst, src, N - 1);
}

// -- linked list helpers --

template<typename T>
bool listRemove(T *&head, T *obj) {
	if (head == obj) {
		head = obj->next;
		return true;
	}
	T *list = head;
	while (list) {
		if (list->next == obj) {
			list->next = obj->next;
			return true;
		}
		list = list->next;
	}
	return false;
}
