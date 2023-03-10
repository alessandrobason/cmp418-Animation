/*
 * default_sprite_shader.cpp
 *
 *  Created on: 28 Jan 2015
 *      Author: grant
 */

#include <graphics/default_sprite_shader.h>
#include <graphics/shader_interface.h>
#include <system/file.h>
#include <system/platform.h>
#include <system/allocator.h>
#include <system/debug_log.h>
#include <string>
#include <graphics/sprite.h>
#include <math.h>
#include <maths/matrix33.h>

#ifdef _WIN32
#include <platform/d3d11/graphics/shader_interface_d3d11.h>
#endif

namespace gef
{
	DefaultSpriteShader::DefaultSpriteShader(const Platform& platform)
		:Shader(platform)
		,sprite_data_variable_index_(-1)
		,projection_matrix_variable_index_(-1)
		,texture_sampler_index_(-1)
	{
		bool success = true;

		// load vertex shader source in from a file
		char* vs_shader_source = NULL;
		Int32 vs_shader_source_length = 0;
		success = LoadShader("default_sprite_shader_vs", "shaders/gef", &vs_shader_source, vs_shader_source_length, platform);

		char* ps_shader_source = NULL;
		Int32 ps_shader_source_length = 0;
		success = LoadShader("default_sprite_shader_ps", "shaders/gef", &ps_shader_source, ps_shader_source_length, platform);

		device_interface_->SetVertexShaderSource(vs_shader_source, vs_shader_source_length);
		device_interface_->SetPixelShaderSource(ps_shader_source, ps_shader_source_length);

		g_alloc->dealloc(vs_shader_source);
		g_alloc->dealloc(ps_shader_source);
		vs_shader_source = NULL;
		ps_shader_source = NULL;


		projection_matrix_variable_index_ = device_interface_->AddVertexShaderVariable("proj_matrix", ShaderInterface::kMatrix44);
		sprite_data_variable_index_ = device_interface_->AddVertexShaderVariable("sprite_data", ShaderInterface::kMatrix44);
		texture_sampler_index_ = device_interface_->AddTextureSampler("texture_sampler");

		device_interface_->AddVertexParameter("position", ShaderInterface::kVector3, 0, "POSITION", 0);
		device_interface_->set_vertex_size(12);

		device_interface_->CreateVertexFormat();

#ifdef _WIN32
		gef::ShaderInterfaceD3D11* shader_interface_d3d11 = static_cast<gef::ShaderInterfaceD3D11*>(device_interface_);

		// Create a texture sampler state description.
		D3D11_SAMPLER_DESC sampler_desc;
		sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampler_desc.MipLODBias = 0.0f;
		sampler_desc.MaxAnisotropy = 1;
		sampler_desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		sampler_desc.BorderColor[0] = 0;
		sampler_desc.BorderColor[1] = 0;
		sampler_desc.BorderColor[2] = 0;
		sampler_desc.BorderColor[3] = 0;
		sampler_desc.MinLOD = 0;
		sampler_desc.MaxLOD = D3D11_FLOAT32_MAX;

		shader_interface_d3d11->AddSamplerState(sampler_desc);
#endif

		success = device_interface_->CreateProgram();
	}

	DefaultSpriteShader::~DefaultSpriteShader()
	{

	}

	DefaultSpriteShader::DefaultSpriteShader()
		: sprite_data_variable_index_(-1)
		, projection_matrix_variable_index_(-1)
		, texture_sampler_index_(-1)
	{

	}


	void DefaultSpriteShader::SetSceneData(const Matrix44& projection_matrix)
	{
		Matrix44 projectionT;
		projectionT.Transpose(projection_matrix);
		device_interface_->SetVertexShaderVariable(projection_matrix_variable_index_, &projectionT);
//		device_interface_->SetVertexShaderVariable(projection_matrix_variable_index_, &projection_matrix);
	}

	void DefaultSpriteShader::SetSpriteData(const Sprite& sprite, const gef::Matrix33& transform, const Texture* texture)
	{
		Matrix44 sprite_data;
		BuildSpriteShaderData(sprite, transform, sprite_data);

		device_interface_->SetVertexShaderVariable(sprite_data_variable_index_, &sprite_data);
		device_interface_->SetTextureSampler(texture_sampler_index_, texture);
	}


	void DefaultSpriteShader::BuildSpriteShaderData(const Sprite& sprite, const gef::Matrix33& transform, Matrix44& sprite_data)
	{
		Vector2 sprite_origin(0.5f, 0.5f);
		Vector2 sprite_uv_origin(0.0f, 0.0f);
		Vector2 sprite_uv_size(1.0f, 1.0f);

		sprite_data.set_m(2, 0, transform.m[2][0]);
		sprite_data.set_m(2, 1, transform.m[2][1]);

		// origin ( not going to pass origin in so we can use for something else
		//        sprite_data.set_m(2,2, sprite_origin.x);
		//        sprite_data.set_m(2,3, sprite_origin.y);

		// depth
		sprite_data.set_m(2, 2, sprite.position().z());

		// scale*rotation
		sprite_data.set_m(0, 0, transform.m[0][0]);
		sprite_data.set_m(0, 1, transform.m[0][1]);
		sprite_data.set_m(1, 0, transform.m[1][0]);
		sprite_data.set_m(1, 1, transform.m[1][1]);
		
		// Source rectangle
		sprite_data.set_m(0, 2, sprite.uv_position().x);
		sprite_data.set_m(0, 3, sprite.uv_position().y);
		sprite_data.set_m(1, 2, sprite.uv_width());
		sprite_data.set_m(1, 3, sprite.uv_height());

		Colour colour;
		colour.SetFromAGBR(sprite.colour());

		sprite_data.set_m(3, 0, colour.r);
		sprite_data.set_m(3, 1, colour.g);
		sprite_data.set_m(3, 2, colour.b);
		sprite_data.set_m(3, 3, colour.a);
	}

	void DefaultSpriteShader::BuildSpriteTransform(const Sprite& sprite, gef::Matrix33& transform)
	{
		// scale*rotation
		if (sprite.rotation() == 0)
		{
			transform.m[0][0] = sprite.width();
			transform.m[0][1] = 0.0f;
			transform.m[1][0] = 0.0f;
			transform.m[1][1] = sprite.height();
		}
		else
		{
			transform.m[0][0] = cosf(sprite.rotation())*sprite.width();
			transform.m[0][1] = sinf(sprite.rotation())*sprite.width();
			transform.m[1][0] = -sinf(sprite.rotation())*sprite.height();
			transform.m[1][1] = cosf(sprite.rotation())*sprite.height();
		}

		transform.m[0][2] = 0.0f;
		transform.m[1][2] = 0.0f;

		gef::Vector4 position = sprite.position();
		transform.m[2][0] = position.x();
		transform.m[2][1] = position.y();
		transform.m[2][2] = 1.0f;
	}

}


