#include <graphics/vertex_buffer.h>
#include <system/allocator.h>
#include <stdlib.h>

namespace gef
{
	VertexBuffer::VertexBuffer() :
		num_vertices_(0),
		vertex_byte_size_(0),
		vertex_data_(NULL)
	{
	}

	VertexBuffer::~VertexBuffer()
	{
		g_alloc->dealloc(vertex_data_);
	}
}