#include <graphics/image_data.h>
#include <cstdlib>
#include <system/allocator.h>

namespace gef
{
	ImageData::ImageData() :
		image_(NULL),
		clut_(NULL)
	{
	}

	ImageData::~ImageData()
	{
		g_alloc->dealloc(image_);
		g_alloc->dealloc(clut_);
	}
}