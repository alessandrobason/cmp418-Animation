#ifndef _GEF_MODEL_H
#define _GEF_MODEL_H

#include <gef.h>
#include <system/vec.h>

namespace gef
{
	class Mesh;
	class Texture;
	class Material;

	class Model
	{
	public:
		Model();
		~Model();
		void Release();

		inline void set_mesh(Mesh* mesh) { mesh_ = mesh; }
		inline Mesh* mesh() { return mesh_; }
		inline void set_textures(const gef::Vec<Texture*>& textures) { textures_ = textures; }

		inline void AddMaterial(Material* material) { materials_.push_back(material); }
		inline const Material* material(Int32 material_num) const { return materials_[material_num]; }
	private:
		Mesh* mesh_;
		gef::Vec<Texture*> textures_;
		gef::Vec<Material*> materials_;
	};
}

#endif // _GEF_MODEL_H
