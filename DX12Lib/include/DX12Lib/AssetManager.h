#pragma once
#include <unordered_map>
#include <memory>
#include "Mesh.h"
#include "Actor.h"
#include "Util.h"

namespace DX12Lib
{
	class AssetManager
	{
	public:
		AssetManager() = default;
		AssetManager(const AssetManager&) = delete;
		AssetManager& operator=(const AssetManager&) = delete;
		~AssetManager() = default;

		// Mesh
		Mesh CreateMesh(const std::wstring& name, const MeshData& data);
		MeshGroup* CreateMeshGroup(const std::wstring& name, std::vector<Mesh>& meshes);
		MeshGroup* CreateMeshGroup(std::unique_ptr<MeshGroup>& group);
		MeshGroup* GetMeshGroup(const std::wstring& name) const;

		// Material
		Material* CreateMaterial(const std::wstring& name);
		Material* GetMaterial(const std::wstring& name) const;
		std::vector<Material*> GetAllMaterials() const;
		inline size_t GetMaterialsCount() const { return mMaterials.size(); }
		int FindMaterialIndex(const std::wstring& name) const;

		// Texture
		Texture* CreateTexture(const std::wstring& name, const std::wstring& filename);
		Texture* GetTexture(const std::wstring& name) const;
		std::vector<Texture*> GetAllTextures() const;
		inline size_t GetTexturesCount() const { return mTextures.size(); }
		int FindTextureIndex(const std::wstring& name) const;

		// Shader
		Microsoft::WRL::ComPtr<ID3DBlob> CreateShader(const std::wstring& name, const std::wstring& filename, const D3D_SHADER_MACRO* defines, const std::string& entrypoint, const std::string& target);
		Microsoft::WRL::ComPtr<ID3DBlob> GetShader(const std::wstring& name) const;

		// Actor
		Actor* CreateActor(const std::wstring& name);
		Actor* GetActor(const std::wstring& name) const;
		std::vector<Actor*> GetActors(UINT layer = Render_Layer_All) const;
		inline size_t GetActorsCount(UINT layer = Render_Layer_All) const;

	private:
		std::unordered_map<std::wstring, std::unique_ptr<MeshGroup>> mMeshGroups;
		std::unordered_map<std::wstring, std::unique_ptr<Material>> mMaterials;
		std::unordered_map<std::wstring, std::unique_ptr<Texture>> mTextures;
		std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3DBlob>> mShaders;
		std::unordered_map<std::wstring, std::unique_ptr<Actor>> mActors;
	};
}
