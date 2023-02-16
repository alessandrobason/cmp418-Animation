#include <graphics/colour.h>

#include <maths/math_utils.h>

namespace gef
{
	UInt32 Colour::GetRGBA() const
	{
		union {
			UInt32 as_uint;
			struct { UInt8 r, g, b, a; };
		} col = { 0 };
		col.r = (UInt8)gef::clamp(r * 255.f, 0.f, 255.f);
		col.g = (UInt8)gef::clamp(g * 255.f, 0.f, 255.f);
		col.b = (UInt8)gef::clamp(b * 255.f, 0.f, 255.f);
		col.a = (UInt8)gef::clamp(a * 255.f, 0.f, 255.f);
		return col.as_uint;
	}

	UInt32 Colour::GetABGR() const
	{
		union {
			UInt32 as_uint;
			struct { UInt8 a, b, g, r; };
		} col = { 0 };
		col.r = (UInt8)gef::clamp(r * 255.f, 0.f, 255.f);
		col.g = (UInt8)gef::clamp(g * 255.f, 0.f, 255.f);
		col.b = (UInt8)gef::clamp(b * 255.f, 0.f, 255.f);
		col.a = (UInt8)gef::clamp(a * 255.f, 0.f, 255.f);
		return col.as_uint;
	}

	void Colour::SetFromRGBA(const UInt32 rgba)
	{
		const float coefficient = 1.f / 255.f;
		a =static_cast<float>(rgba & 0x000000ff)*coefficient;
		b = static_cast<float>((rgba >> 8) & 0x000000ff)*coefficient;
		g = static_cast<float>((rgba >> 16) & 0x000000ff)*coefficient;
		r = static_cast<float>((rgba >> 24) & 0x000000ff)*coefficient;
	}

	void Colour::SetFromAGBR(const UInt32 rgba)
	{
		float coefficient = 1.f / 255.f;
		r = static_cast<float>(rgba & 0x000000ff)*coefficient;
		g = static_cast<float>((rgba >> 8) & 0x000000ff)*coefficient;
		b = static_cast<float>((rgba >> 16) & 0x000000ff)*coefficient;
		a = static_cast<float>((rgba >> 24) & 0x000000ff)*coefficient;
	}

	const Colour Colour::light_gray  = { 0.784f, 0.784f, 0.784f, 1.f };
	const Colour Colour::gray        = { 0.509f, 0.509f, 0.509f, 1.f };
	const Colour Colour::dark_gray   = { 0.313f, 0.313f, 0.313f, 1.f };
	const Colour Colour::yellow      = { 0.992f, 0.976f, 0.000f, 1.f };
	const Colour Colour::gold        = { 1.000f, 0.796f, 0.000f, 1.f };
	const Colour Colour::orange      = { 1.000f, 0.631f, 0.000f, 1.f };
	const Colour Colour::pink        = { 1.000f, 0.427f, 0.760f, 1.f };
	const Colour Colour::red         = { 0.901f, 0.160f, 0.215f, 1.f };
	const Colour Colour::maroon      = { 0.745f, 0.129f, 0.215f, 1.f };
	const Colour Colour::green       = { 0.000f, 0.894f, 0.188f, 1.f };
	const Colour Colour::lime        = { 0.000f, 0.619f, 0.184f, 1.f };
	const Colour Colour::dark_green  = { 0.000f, 0.458f, 0.172f, 1.f };
	const Colour Colour::sky_blue    = { 0.400f, 0.749f, 1.000f, 1.f };
	const Colour Colour::blue        = { 0.000f, 0.474f, 0.945f, 1.f };
	const Colour Colour::true_blue   = { 0.000f, 0.000f, 1.000f, 1.f };
	const Colour Colour::dark_blue   = { 0.000f, 0.321f, 0.674f, 1.f };
	const Colour Colour::purple      = { 0.784f, 0.478f, 1.000f, 1.f };
	const Colour Colour::violet      = { 0.529f, 0.235f, 0.745f, 1.f };
	const Colour Colour::dark_purple = { 0.439f, 0.121f, 0.494f, 1.f };
	const Colour Colour::beige       = { 0.827f, 0.690f, 0.513f, 1.f };
	const Colour Colour::brown       = { 0.498f, 0.415f, 0.309f, 1.f };
	const Colour Colour::darkbrown   = { 0.298f, 0.247f, 0.184f, 1.f };
	const Colour Colour::white       = { 1.000f, 1.000f, 1.000f, 1.f };
	const Colour Colour::black       = { 0.000f, 0.000f, 0.000f, 1.f };
	const Colour Colour::magenta     = { 1.000f, 0.000f, 1.000f, 1.f };
	const Colour Colour::bone        = { 0.960f, 0.960f, 0.960f, 1.f };
	const Colour Colour::blank       = { 0.000f, 0.000f, 0.000f, 0.f };
}