#pragma once
#include "Util.h"
#include "FrameResource.h"
#include "Mesh.h"

namespace DX12Lib
{
	class Actor
	{
	public:
		Actor(const std::wstring& name) : Name(name) {}
		Actor(const Actor& other) = delete;
		Actor& operator=(const Actor& ohter) = delete;
		~Actor() = default;

		std::wstring ToString() const
		{
			Submesh submesh = Group->DrawArgs[DrawArg];

			std::wstring str = L"Name: " + Name;
			str += L", Hidden: " + std::to_wstring(Hidden);
			str += L", Visible: " + std::to_wstring(Visible);
			str += L", GroupName: " + (Group ? Group->Name : L"null");
			str += L", DrawArg: " + DrawArg;
			str += L", MaterialCBIndex: " + std::to_wstring(Instance.MaterialCBIndex);
			//str += L", InstanceBufferOffset: " + std::to_wstring(InstanceBufferOffset);
			//str += L", StartIndexLocation: " + std::to_wstring(submesh.StartIndexLocation);
			//str += L", IndexCount: " + std::to_wstring(submesh.IndexCount);

			return str;
		}

	public:
		std::wstring Name;
		//int NumFramesDirty = gNumFrameResources;

		MeshGroup* Group = nullptr;
		std::wstring DrawArg;
		D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		InstanceData Instance;
		UINT InstanceBufferOffset = 0;

		bool Visible = true;
		bool Hidden = false;
		UINT RenderLayer = Render_Layer_Opaque;

		// Only applicable to skinned render-items.
		UINT SkinnedCBIndex = -1;
		SkinnedMesh* mSkinnedMesh = nullptr;
	};
}
