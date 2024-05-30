#pragma once
#include <d3d12.h>
#include <DirectXMath.h>
#include <vector>
#include <wrl.h>
#include <string>
#include <unordered_map>
#include <DirectXCollision.h>

namespace DX12Lib
{
	struct Vertex
	{
		//Vertex() = default;
		Vertex() {}
		Vertex(
			const DirectX::XMFLOAT3& p,
			const DirectX::XMFLOAT3& n,
			const DirectX::XMFLOAT3& t,
			const DirectX::XMFLOAT2& uv) :
			Position(p),
			Normal(n),
			TexCoord(uv),
			TangentU(t) {}
		Vertex(
			float px, float py, float pz,
			float nx, float ny, float nz,
			float tx, float ty, float tz,
			float u, float v) :
			Position(px, py, pz),
			Normal(nx, ny, nz),
			TexCoord(u, v),
			TangentU(tx, ty, tz) {}

		DirectX::XMFLOAT3 Position;
		DirectX::XMFLOAT3 Normal;
		DirectX::XMFLOAT2 TexCoord;
		DirectX::XMFLOAT3 TangentU;
	};

	struct SkinnedVertex
	{
		DirectX::XMFLOAT3 Position;
		DirectX::XMFLOAT3 Normal;
		DirectX::XMFLOAT2 TexCoord;
		DirectX::XMFLOAT3 TangentU;
		DirectX::XMFLOAT3 BoneWeights;
		BYTE BoneIndices[4];
	};

	struct MeshData
	{
		std::vector<Vertex> Vertices;
		std::vector<uint32_t> Indices32;

		std::vector<uint16_t>& GetIndices16()
		{
			if (mIndices16.empty())
			{
				mIndices16.resize(Indices32.size());
				for (size_t i = 0; i < Indices32.size(); ++i)
					mIndices16[i] = static_cast<uint16_t>(Indices32[i]);
			}

			return mIndices16;
		}
	private:
		std::vector<uint16_t> mIndices16;
	};


	class MeshGenerator
	{
	public:
		static MeshData Box(float width, float height, float depth, uint32_t numSubdivisions = 0);
		static MeshData Sphere(float radius, uint32_t sliceCount, uint32_t stackCount);
		static MeshData Cylinder(float bottomRadius, float topRadius, float height, uint32_t sliceCount, uint32_t stackCount);
		static MeshData Grid(float width, float depth, uint32_t m, uint32_t n);
		static MeshData Quad(float x, float y, float width, float height, float depth);

	private:
		static void Subdivide(MeshData& meshData);
		static Vertex MidPoint(const Vertex& v0, const Vertex& v1);
		static void BuildCylinderTopCap(float bottomRadius, float topRadius, float height, uint32_t sliceCount, uint32_t stackCount, MeshData& meshData);
		static void BuildCylinderBottomCap(float bottomRadius, float topRadius, float height, uint32_t sliceCount, uint32_t stackCount, MeshData& meshData);
	};

	class Mesh
	{
	public:
		Mesh(const std::wstring& name, const MeshData& data)
			: Name(name)
			, Data(data)
		{
		}

		std::wstring Name;
		MeshData Data;
	};

	struct Submesh
	{
		UINT IndexCount = 0;
		UINT StartIndexLocation = 0;
		INT BaseVertexLocation = 0;
		DirectX::BoundingBox Bound;
	};

	class MeshGroup
	{
	public:
		MeshGroup(const std::wstring& name) : Name(name) {}

		std::wstring Name;

		Microsoft::WRL::ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
		Microsoft::WRL::ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

		Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

		Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

		// Data about the buffers.
		UINT VertexByteStride = 0;
		UINT VertexBufferByteSize = 0;
		DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;
		UINT IndexBufferByteSize = 0;

		std::unordered_map<std::wstring, Submesh> DrawArgs;

		D3D12_VERTEX_BUFFER_VIEW VertexBufferView() const
		{
			D3D12_VERTEX_BUFFER_VIEW vbv;
			vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
			vbv.StrideInBytes = VertexByteStride;
			vbv.SizeInBytes = VertexBufferByteSize;

			return vbv;
		}

		D3D12_INDEX_BUFFER_VIEW IndexBufferView() const
		{
			D3D12_INDEX_BUFFER_VIEW ibv;
			ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
			ibv.Format = IndexFormat;
			ibv.SizeInBytes = IndexBufferByteSize;

			return ibv;
		}

		// We can free this memory after we finish upload to the GPU.
		void DisposeUploaders()
		{
			VertexBufferUploader = nullptr;
			IndexBufferUploader = nullptr;
		}
	};


	struct Keyframe
	{
		Keyframe();
		~Keyframe() = default;

		float TimePos;
		DirectX::XMFLOAT3 Translation;
		DirectX::XMFLOAT3 Scale;
		DirectX::XMFLOAT4 RotationQuat;
	};

	struct BoneAnimation
	{
		inline float GetStartTime() const { return Keyframes.front().TimePos; }
		inline float GetEndTime() const { return Keyframes.back().TimePos; }

		void Interpolate(float t, DirectX::XMFLOAT4X4& M) const;

		std::vector<Keyframe> Keyframes;
	};

	struct AnimationClip
	{
		float GetStartTime() const;
		float GetEndTime() const;
		void Interpolate(float t, std::vector<DirectX::XMFLOAT4X4>& boneTransforms) const;
		
		std::vector<BoneAnimation> BoneAnimations;
	};

	class SkinnedData
	{
	public:
		inline UINT GetBoneCount() const { return mBoneHierarchy.size(); }

		float GetAnimationClipStartTime(const std::wstring& clipName) const;
		float GetAnimationClipEndTime(const std::wstring& clipName) const;

		inline void SetBoneHierarchy(std::vector<int>& boneHierarchy) { mBoneHierarchy = boneHierarchy; }
		inline void SetBoneOffsets(std::vector<DirectX::XMFLOAT4X4>& boneOffsets) { mBoneOffsets = boneOffsets; }
		inline void SetAnimations(std::unordered_map<std::wstring, AnimationClip>& animations) { mAnimations = animations; }
	
		void GetFinalTransform(const std::wstring& clipName, float timePos, std::vector<DirectX::XMFLOAT4X4>& finalTransforms) const;

	private:
		std::vector<int> mBoneHierarchy;
		std::vector<DirectX::XMFLOAT4X4> mBoneOffsets;
		std::unordered_map<std::wstring, AnimationClip> mAnimations;
	};

	class SkinnedMesh
	{
	public:
		SkinnedData* SkinnedInfo = nullptr;
		std::vector<DirectX::XMFLOAT4X4> FinalTransforms;
		std::wstring AnimationClipName;
		float TimePos = 0.0f;

		void UpdateSkinnedAnimation(float deltaTime)
		{
			TimePos += deltaTime;

			if (TimePos > SkinnedInfo->GetAnimationClipEndTime(AnimationClipName))
			{
				TimePos = 0.0f;
			}
			SkinnedInfo->GetFinalTransform(AnimationClipName, TimePos, FinalTransforms);
		}
	};

	class M3DLoader
	{
	public:
		struct Subset
		{
			UINT Id = -1;
			UINT VertexStart = 0;
			UINT VertexCount = 0;
			UINT FaceStart = 0;
			UINT FaceCount = 0;
		};

		struct M3dMaterial
		{
			std::wstring Name;

			DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
			DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
			float Roughness = 0.8f;
			bool AlphaClip = false;

			std::wstring MaterialTypeName;
			std::wstring DiffuseMapName;
			std::wstring NormalMapName;
		};

		bool LoadM3d(const std::wstring& filename,
			std::vector<Vertex>& vertices,
			std::vector<USHORT>& indices,
			std::vector<Subset>& subsets,
			std::vector<M3dMaterial>& mats);
		bool LoadM3d(const std::wstring& filename,
			std::vector<SkinnedVertex>& vertices,
			std::vector<USHORT>& indices,
			std::vector<Subset>& subsets,
			std::vector<M3dMaterial>& mats,
			SkinnedData& skinInfo);

	private:
		void ReadMaterials(std::wifstream& fin, UINT numMaterials, std::vector<M3dMaterial>& mats);
		void ReadSubsetTable(std::wifstream& fin, UINT numSubsets, std::vector<Subset>& subsets);
		void ReadVertices(std::wifstream& fin, UINT numVertices, std::vector<Vertex>& vertices);
		void ReadSkinnedVertices(std::wifstream& fin, UINT numVertices, std::vector<SkinnedVertex>& vertices);
		void ReadTriangles(std::wifstream& fin, UINT numTriangles, std::vector<USHORT>& indices);
		void ReadBoneOffsets(std::wifstream& fin, UINT numBones, std::vector<DirectX::XMFLOAT4X4>& boneOffsets);
		void ReadBoneHierarchy(std::wifstream& fin, UINT numBones, std::vector<int>& boneIndexToParentIndex);
		void ReadAnimationClips(std::wifstream& fin, UINT numBones, UINT numAnimationClips, std::unordered_map<std::wstring, AnimationClip>& animations);
		void ReadBoneKeyframes(std::wifstream& fin, UINT numBones, BoneAnimation& boneAnimation);
	};
}
