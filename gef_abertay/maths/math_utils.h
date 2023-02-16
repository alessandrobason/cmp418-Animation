#ifndef _GEF_MATH_UTILS_H
#define _GEF_MATH_UTILS_H

#include "vector2.h"

#define FRAMEWORK_DEG_TO_RAD ((float)gef::pi/180.0f)
#define FRAMEWORK_RAD_TO_DEG ((float)180.f/(float)gef::pi)

#undef min
#undef max

namespace gef {
	constexpr float pi = 3.14159265358979323846264338327950288419716939937510f;

	inline constexpr float DegToRad(float angleInDegrees) {
		return angleInDegrees * FRAMEWORK_DEG_TO_RAD;
	}

	inline constexpr float RadToDeg(float angleInRadians) {
		return angleInRadians * FRAMEWORK_RAD_TO_DEG;
	}

	inline float ShortestAngleDiff(float a, float b) {
		float diff = a - b;
		return (diff < -(float)gef::pi) ? (diff + 2 * (float)gef::pi) : ((diff > (float)gef::pi) ? (diff - 2 * (float)gef::pi) : diff);
		//return diff;
	}

	template<typename T>
	constexpr T min(const T &a, const T &b) {
		return a < b ? a : b;
	}

	template<typename T>
	constexpr T max(const T &a, const T &b) {
		return a > b ? a : b;
	}

	template<typename T>
	constexpr T clamp(const T &value, const T &from, const T &to) {
		return min(max(value, from), to);
	}

	constexpr gef::Vector2 clamp(const gef::Vector2 &value, const gef::Vector2 &from, const gef::Vector2 &to) {
		return {
			min(max(value.x, from.x), to.x),
			min(max(value.y, from.y), to.y),
		};
	}

	template<typename T>
	T sign(const T &v) {
		return v >= 0.f ? 1.f : -1.f;
	}

	template<typename T>
	T lerp(const T &start, const T &end, float t) {
		return start * (1.f - t) + end * t;
	}
}

#endif // _GEF_MATH_UTILS_H
