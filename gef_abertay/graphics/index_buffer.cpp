#include <graphics/index_buffer.h>
#include <system/allocator.h>
#include <stdlib.h>

namespace gef
{
	IndexBuffer::IndexBuffer() :
		num_indices_(0),
		index_byte_size_(0),
		index_data_(NULL)
	{
	}

	IndexBuffer::~IndexBuffer()
	{
		g_alloc->dealloc(index_data_);
	}
}