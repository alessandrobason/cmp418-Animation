#include <graphics/depth_buffer.h>
#include <graphics/texture.h>
#include <system/allocator.h>
#include <cstddef>

namespace gef
{
	DepthBuffer::DepthBuffer(UInt32 height, UInt32 width) :
		height_(height),
		width_(width),
		texture_(NULL)
	{

	}

	DepthBuffer::~DepthBuffer()
	{
		g_alloc->destroy(texture_);
	}
}