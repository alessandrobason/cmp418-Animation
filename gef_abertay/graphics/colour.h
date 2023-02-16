#ifndef _GEF_COLOUR_H
#define _GEF_COLOUR_H

#include <gef.h>
#include <maths/vector4.h>

namespace gef
{
	class Colour
	{
	public:
		Colour() = default;
		constexpr Colour(float r, float g, float b, float a = 1.0f)
			: r(r), g(g), b(b), a(a) {}

		UInt32 GetRGBA() const;
		UInt32 GetABGR() const;
		inline Vector4 GetRGBAasVector4() const { return Vector4(r, g, b, a); }
		inline Vector4 GetABGRasVector4() const { return Vector4(a, b, g, r); }
		void SetFromRGBA(const UInt32 rgba);
		void SetFromAGBR(const UInt32 rgba);

		static Colour fromRGBA(UInt32 rgba) { Colour out; out.SetFromRGBA(rgba); return out; }
		static Colour fromAGBR(UInt32 agbr) { Colour out; out.SetFromAGBR(agbr); return out; }

		static const Colour light_gray;
		static const Colour gray;
		static const Colour dark_gray;
		static const Colour yellow;
		static const Colour gold;
		static const Colour orange;
		static const Colour pink;
		static const Colour red;
		static const Colour maroon;
		static const Colour green;
		static const Colour lime;
		static const Colour dark_green;
		static const Colour sky_blue;
		static const Colour blue;
		static const Colour true_blue;
		static const Colour dark_blue;
		static const Colour purple;
		static const Colour violet;
		static const Colour dark_purple;
		static const Colour beige;
		static const Colour brown;
		static const Colour darkbrown;
		static const Colour white;
		static const Colour black;
		static const Colour blank;
		static const Colour magenta;
		static const Colour bone;

		Colour operator+(const Colour &v) const { return { r + v.r, g + v.g, b + v.b, a + v.a }; }
		Colour operator-(const Colour &v) const { return { r - v.r, g - v.g, b - v.b, a - v.a }; }
		Colour operator*(const Colour &v) const { return { r * v.r, g * v.g, b * v.b, a * v.a }; }
		Colour operator/(const Colour &v) const { return { r / v.r, g / v.g, b / v.b, a / v.a }; }

		Colour &operator+=(const Colour &v) { r += v.r; g += v.g; b += v.b; a += v.a; return *this; }
		Colour &operator-=(const Colour &v) { r -= v.r; g -= v.g; b -= v.b; a -= v.a; return *this; }
		Colour &operator*=(const Colour &v) { r *= v.r; g *= v.g; b *= v.b; a *= v.a; return *this; }
		Colour &operator/=(const Colour &v) { r /= v.r; g /= v.g; b /= v.b; a /= v.a; return *this; }

		Colour operator+(float v) const { return { r + v, g + v, b + v, a + v }; }
		Colour operator-(float v) const { return { r - v, g - v, b - v, a - v }; }
		Colour operator*(float v) const { return { r * v, g * v, b * v, a * v }; }
		Colour operator/(float v) const { return { r / v, g / v, b / v, a / v }; }

		Colour &operator+=(float v) { r += v; g += v; b += v; a += v; return *this; }
		Colour &operator-=(float v) { r -= v; g -= v; b -= v; a -= v; return *this; }
		Colour &operator*=(float v) { r *= v; g *= v; b *= v; a *= v; return *this; }
		Colour &operator/=(float v) { r /= v; g /= v; b /= v; a /= v; return *this; }

		bool operator==(const Colour &v) const { return r == v.r && g == v.g && b == v.b && a == v.a; }
		bool operator!=(const Colour &v) const { return !(*this == v); }

		float r = 0.f;
		float g = 0.f;
		float b = 0.f;
		float a = 1.f;
	};
}

#endif // _GEF_COLOUR_H
