#include "DX12Lib/AssetManager.h"
#include <d3dcompiler.h>
#include "DX12Lib/FrameResource.h"
#include "DX12Lib/Application.h"

namespace DX12Lib
{
	Mesh AssetManager::CreateMesh(const std::wstring& name, const MeshData& data)
	{
		return Mesh(name, data);
	}

	MeshGroup* AssetManager::CreateMeshGroup(const std::wstring& name, std::vector<Mesh>& meshes)
	{
		if (GetMeshGroup(name))
			return nullptr;

		auto mesheGroup = std::make_unique<MeshGroup>(name);

		UINT currentVertexOffset = 0;
		UINT currentIndexOffset = 0;
		for (auto& mesh : meshes)
		{
			Submesh submesh;
			submesh.IndexCount = mesh.Data.Indices32.size();
			submesh.StartIndexLocation = currentIndexOffset;
			submesh.BaseVertexLocation = currentVertexOffset;
			
			DirectX::XMFLOAT3 vMinf3(+FLT_MAX, +FLT_MAX, +FLT_MAX);
			DirectX::XMFLOAT3 vMaxf3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
			DirectX::XMVECTOR vMin = DirectX::XMLoadFloat3(&vMinf3);
			DirectX::XMVECTOR vMax = DirectX::XMLoadFloat3(&vMaxf3);

			for (auto& v : mesh.Data.Vertices)
			{
				DirectX::XMVECTOR P = DirectX::XMLoadFloat3(&v.Position);
				vMin = DirectX::XMVectorMin(vMin, P);
				vMax = DirectX::XMVectorMax(vMax, P);
			}

			DirectX::BoundingBox bound;
			DirectX::XMStoreFloat3(&bound.Center, DirectX::XMVectorScale(DirectX::XMVectorAdd(vMin, vMax), 0.5));
			DirectX::XMStoreFloat3(&bound.Extents, DirectX::XMVectorScale(DirectX::XMVectorSubtract(vMax, vMin), 0.5));
			submesh.Bound = bound;

			currentIndexOffset += mesh.Data.Indices32.size();
			currentVertexOffset += mesh.Data.Vertices.size();

			mesheGroup->DrawArgs[mesh.Name] = submesh;
		}

		std::vector<Vertex> vertices(currentVertexOffset);
		std::vector<std::uint16_t> indices;

		UINT k = 0;
		for (auto& mesh : meshes)
		{
			for (size_t i = 0; i < mesh.Data.Vertices.size(); ++i, ++k)
			{
				vertices[k].Position = mesh.Data.Vertices[i].Position;
				vertices[k].Normal = mesh.Data.Vertices[i].Normal;
				vertices[k].TexCoord = mesh.Data.Vertices[i].TexCoord;
				vertices[k].TangentU = mesh.Data.Vertices[i].TangentU;
			}

			indices.insert(indices.end(), std::begin(mesh.Data.GetIndices16()), std::end(mesh.Data.GetIndices16()));
		}

		const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
		const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

		ThrowIfFailed(D3DCreateBlob(vbByteSize, &mesheGroup->VertexBufferCPU));
		CopyMemory(mesheGroup->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

		ThrowIfFailed(D3DCreateBlob(ibByteSize, &mesheGroup->IndexBufferCPU));
		CopyMemory(mesheGroup->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

		mesheGroup->VertexBufferGPU = CreateDefaultBuffer(Application::Get()->GetDevice().Get(), Application::Get()->GetDirectCommandList().Get(), vertices.data(), vbByteSize, mesheGroup->VertexBufferUploader);
		mesheGroup->IndexBufferGPU = CreateDefaultBuffer(Application::Get()->GetDevice().Get(), Application::Get()->GetDirectCommandList().Get(), indices.data(), ibByteSize, mesheGroup->IndexBufferUploader);

		mesheGroup->VertexByteStride = sizeof(Vertex);
		mesheGroup->VertexBufferByteSize = vbByteSize;
		mesheGroup->IndexFormat = DXGI_FORMAT_R16_UINT;
		mesheGroup->IndexBufferByteSize = ibByteSize;

		mMeshGroups[mesheGroup->Name] = std::move(mesheGroup);

		return GetMeshGroup(name);
	}

	DX12Lib::MeshGroup* AssetManager::CreateMeshGroup(std::unique_ptr<MeshGroup>& group)
	{
		if (GetMeshGroup(group->Name))
			return nullptr;

		mMeshGroups[group->Name] = std::move(group);
	}

	DX12Lib::MeshGroup* AssetManager::GetMeshGroup(const std::wstring& name) const
	{
		auto it = mMeshGroups.find(name);
		if (it != mMeshGroups.end())
		{
			return it->second.get();
		}
		return nullptr;
	}

	Material* AssetManager::CreateMaterial(const std::wstring& name)
	{
		if (GetMaterial(name))
			return nullptr;

		auto material = std::make_unique<Material>(name);

		mMaterials[name] = std::move(material);
		return GetMaterial(name);
	}

	Material* AssetManager::GetMaterial(const std::wstring& name) const
	{
		auto it = mMaterials.find(name);
		if (it != mMaterials.end())
		{
			return it->second.get();
		}
		return nullptr;
	}

	std::vector<Material*> AssetManager::GetAllMaterials() const
	{
		std::vector<Material*> materials;
		materials.reserve(GetMaterialsCount());

		for (auto& [name, material] : mMaterials)
		{
			materials.emplace_back(material.get());
		}

		return materials;
	}

	int AssetManager::FindMaterialIndex(const std::wstring& name) const
	{
		auto it = mMaterials.find(name);

		if (it != mMaterials.end())
		{
			int index = 0;
			for (auto i = mMaterials.begin(); i != it; ++i)
			{
				++index;
			}
			return index;
		}
		else
		{
			return -1;
		}
	}

	Texture* AssetManager::CreateTexture(const std::wstring& name, const std::wstring& filename)
	{
		if (GetTexture(name))
			return nullptr;

		auto texture = std::make_unique<Texture>(name, filename);
		mTextures[name] = std::move(texture);
		return GetTexture(name);
	}

	Texture* AssetManager::GetTexture(const std::wstring& name) const
	{
		auto it = mTextures.find(name);
		if (it != mTextures.end())
		{
			return it->second.get();
		}
		return nullptr;
	}

	std::vector<Texture*> AssetManager::GetAllTextures() const
	{
		std::vector<Texture*> textures;
		textures.reserve(GetTexturesCount());

		for (auto& [name, texture] : mTextures)
		{
			textures.emplace_back(texture.get());
		}

		return textures;
	}

	int AssetManager::FindTextureIndex(const std::wstring& name) const
	{
		auto it = mTextures.find(name);

		if (it != mTextures.end())
		{
			int index = 0;
			for (auto i = mTextures.begin(); i != it; ++i)
			{
				++index;
			}
			return index;
		}
		else
		{
			return -1;
		}
	}

	Microsoft::WRL::ComPtr<ID3DBlob> AssetManager::CreateShader(const std::wstring& name, const std::wstring& filename, const D3D_SHADER_MACRO* defines, const std::string& entrypoint, const std::string& target)
	{
		if (GetShader(name))
			return nullptr;

		mShaders[name] = CompileShader(filename.c_str(), defines, entrypoint.c_str(), target.c_str());
		return mShaders[name];
	}

	Microsoft::WRL::ComPtr<ID3DBlob> AssetManager::GetShader(const std::wstring& name) const
	{
		auto it = mShaders.find(name);
		if (it != mShaders.end())
		{
			return it->second;
		}
		return nullptr;
	}

	Actor* AssetManager::CreateActor(const std::wstring& name)
	{
		if (GetActor(name))
			return nullptr;

		auto actor = std::make_unique<Actor>(name);
		mActors[name] = std::move(actor);
		return GetActor(name);
	}

	Actor* AssetManager::GetActor(const std::wstring& name) const
	{
		auto it = mActors.find(name);
		if (it != mActors.end())
		{
			return it->second.get();
		}
		return nullptr;
	}

	std::vector<Actor*> AssetManager::GetActors(UINT layer) const
	{
		std::vector<Actor*> actors;

		if (layer == Render_Layer_All)
		{
			actors.reserve(GetActorsCount());
			for (auto& [name, actor] : mActors)
			{
				actors.emplace_back(actor.get());
			}
		}
		else
		{
			for (auto& [name, actor] : mActors)
			{
				if (actor->RenderLayer & layer)
					actors.emplace_back(actor.get());
			}
		}

		return actors;
	}

	size_t AssetManager::GetActorsCount(UINT layer) const
	{
		if (layer == Render_Layer_All)
			return mActors.size();

		size_t count = 0;
		for (auto& [name, actor] : mActors)
		{
			if (actor->RenderLayer & layer)
				count++;
		}
		
		return count;
	}

}
