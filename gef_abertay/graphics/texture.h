#ifndef _GEF_TEXTURE_H
#define _GEF_TEXTURE_H

#include <gef.h>
#include <maths/vector2.h>

namespace gef
{
	class ImageData;
	class Platform;

class Texture
{
public:
	virtual ~Texture();
	virtual void Bind(const Platform& platform, const int texture_stage_num) const = 0;
	virtual void Unbind(const Platform& platform, const int texture_stage_num) const = 0;

	static Texture* Create(const Platform& platform, const ImageData& image_data, IAllocator *alloc = g_alloc);
	static Texture* CreateCheckerTexture(const Int32 size, const Int32 num_checkers, const Platform& platform, IAllocator *alloc = g_alloc);
	
	void setSize(const Vector2 &size);
	Vector2 GetSize() const;

protected:
	Texture();
	Texture(const Platform& platform, const ImageData& image_data);

	UInt32 width_;
	UInt32 height_;
};

}
#endif // _GEF_TEXTURE_H
