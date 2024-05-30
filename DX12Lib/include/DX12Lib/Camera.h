#pragma once
#include "Util.h"

namespace DX12Lib
{
	class Camera
	{
	public:
		Camera() = default;
		~Camera() = default;

		inline void SetPosition(const DirectX::XMFLOAT3& position) { mPosition = position; }
		inline void SetPosition(float x, float y, float z) { mPosition.x = x; mPosition.y = y; mPosition.z = z; }

		void SetPerspective(float fovAngleY, float aspectRatio, float nearZ, float farZ);

		void LookAt(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& target, const DirectX::XMFLOAT3& up);
		void LookAt(const DirectX::XMVECTOR& position, const DirectX::XMVECTOR& target, const DirectX::XMVECTOR& up);
		void LookTo(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& direction, const DirectX::XMFLOAT3& up);
		void LookTo(const DirectX::XMVECTOR& position, const DirectX::XMVECTOR& direction, const DirectX::XMVECTOR& up);

		void MoveForward(float distance);
		void MoveRight(float distance);
		void MoveUp(float distance);

		void RotateX(float angle);
		void RotateY(float angle);

		void UpdateViewMatrix();

		inline float GetFovAngleY() const { return mFovAngleY; }
		inline float GetAspectRatio() const { return mAspectRatio; }
		inline float GetNearZ() const { return mNearZ; }
		inline float GetFarZ() const { return mFarZ; }

		inline DirectX::XMFLOAT3 GetPosition3f() const { return mPosition; }
		inline DirectX::XMFLOAT3 GetRight3f() const { return mRight; }
		inline DirectX::XMFLOAT3 GetUp3f() const { return mUp; }
		inline DirectX::XMFLOAT3 GetForward3f() const { return mForward; }
		inline DirectX::XMFLOAT4X4 GetViewMatrix4x4f() const { return mViewMatrix; }
		inline DirectX::XMFLOAT4X4 GetProjMatrix4x4f() const { return mProjMatrix; }

		inline DirectX::FXMVECTOR GetPosition() const { return DirectX::XMLoadFloat3(&mPosition); }
		inline DirectX::FXMVECTOR GetRight() const { return DirectX::XMLoadFloat3(&mRight); }
		inline DirectX::FXMVECTOR GetUp() const { return DirectX::XMLoadFloat3(&mUp); }
		inline DirectX::FXMVECTOR GetForward() const { return DirectX::XMLoadFloat3(&mForward); }
		inline DirectX::FXMMATRIX GetViewMatrix() const { return DirectX::XMLoadFloat4x4(&mViewMatrix); }
		inline DirectX::FXMMATRIX GetProjMatrix() const { return DirectX::XMLoadFloat4x4(&mProjMatrix); }

	private:
		bool mViewDirty = false;

		float mFovAngleY = 45.0f;
		float mAspectRatio = 16.0f / 9.0f;
		float mNearZ = 1.0f;
		float mFarZ = 1000.0f;

		DirectX::XMFLOAT3 mPosition = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 mRight = { 1.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 mUp = { 0.0f, 1.0f, 0.0f };
		DirectX::XMFLOAT3 mForward = { 0.0f, 0.0f, 1.0f };

		DirectX::XMFLOAT4X4 mViewMatrix = Identity4x4();
		DirectX::XMFLOAT4X4 mProjMatrix = Identity4x4();
	};
}
