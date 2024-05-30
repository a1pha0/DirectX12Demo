#include "DX12Lib/Camera.h"

namespace DX12Lib
{
	void Camera::SetPerspective(float fovAngleY, float aspectRatio, float nearZ, float farZ)
	{
		mFovAngleY = fovAngleY;
		mAspectRatio = aspectRatio;
		mNearZ = nearZ;
		mFarZ = farZ;

		DirectX::XMMATRIX proj = DirectX::XMMatrixPerspectiveFovLH(mFovAngleY, mAspectRatio, mNearZ, mFarZ);
		DirectX::XMStoreFloat4x4(&mProjMatrix, proj);
	}

	void Camera::LookAt(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& target, const DirectX::XMFLOAT3& up)
	{
		mPosition = position;

		DirectX::XMVECTOR eyePosition = DirectX::XMLoadFloat3(&position);
		DirectX::XMVECTOR focusPosition = DirectX::XMLoadFloat3(&target);
		DirectX::XMVECTOR forwardVector = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(focusPosition, eyePosition));

		DirectX::XMVECTOR upVector = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&up));
		DirectX::XMVECTOR rightVector = DirectX::XMVector3Cross(upVector, forwardVector);

		DirectX::XMStoreFloat3(&mRight, rightVector);
		DirectX::XMStoreFloat3(&mUp, upVector);
		DirectX::XMStoreFloat3(&mForward, forwardVector);

		mViewDirty = true;
	}

	void Camera::LookAt(const DirectX::XMVECTOR& position, const DirectX::XMVECTOR& target, const DirectX::XMVECTOR& up)
	{
		DirectX::XMVECTOR forwardVector = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(target, position));
		DirectX::XMVECTOR upVector = DirectX::XMVector3Normalize(up);
		DirectX::XMVECTOR rightVector = DirectX::XMVector3Cross(upVector, forwardVector);

		DirectX::XMStoreFloat3(&mPosition, position);
		DirectX::XMStoreFloat3(&mRight, rightVector);
		DirectX::XMStoreFloat3(&mUp, upVector);
		DirectX::XMStoreFloat3(&mForward, forwardVector);

		mViewDirty = true;
	}

	void Camera::LookTo(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& direction, const DirectX::XMFLOAT3& up)
	{
		mPosition = position;

		DirectX::XMVECTOR forwardVector = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&direction));
		DirectX::XMVECTOR upVector = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&up));
		DirectX::XMVECTOR rightVector = DirectX::XMVector3Cross(upVector, forwardVector);

		DirectX::XMStoreFloat3(&mRight, rightVector);
		DirectX::XMStoreFloat3(&mUp, upVector);
		DirectX::XMStoreFloat3(&mForward, forwardVector);

		mViewDirty = true;
	}

	void Camera::LookTo(const DirectX::XMVECTOR& position, const DirectX::XMVECTOR& direction, const DirectX::XMVECTOR& up)
	{
		DirectX::XMVECTOR forwardVector = DirectX::XMVector3Normalize(direction);
		DirectX::XMVECTOR upVector = DirectX::XMVector3Normalize(up);
		DirectX::XMVECTOR rightVector = DirectX::XMVector3Cross(upVector, forwardVector);

		DirectX::XMStoreFloat3(&mPosition, position);
		DirectX::XMStoreFloat3(&mRight, rightVector);
		DirectX::XMStoreFloat3(&mUp, upVector);
		DirectX::XMStoreFloat3(&mForward, forwardVector);

		mViewDirty = true;
	}

	void Camera::MoveForward(float distance)
	{
		DirectX::XMVECTOR s = DirectX::XMVectorReplicate(distance);
		DirectX::XMVECTOR f = DirectX::XMLoadFloat3(&mForward);
		DirectX::XMVECTOR p = DirectX::XMLoadFloat3(&mPosition);
		DirectX::XMStoreFloat3(&mPosition, DirectX::XMVectorMultiplyAdd(s, f, p));

		mViewDirty = true;
	}

	void Camera::MoveRight(float distance)
	{
		DirectX::XMVECTOR s = DirectX::XMVectorReplicate(distance);
		DirectX::XMVECTOR r = DirectX::XMLoadFloat3(&mRight);
		DirectX::XMVECTOR p = DirectX::XMLoadFloat3(&mPosition);
		DirectX::XMStoreFloat3(&mPosition, DirectX::XMVectorMultiplyAdd(s, r, p));

		mViewDirty = true;
	}

	void Camera::MoveUp(float distance)
	{
		DirectX::XMVECTOR s = DirectX::XMVectorReplicate(distance);
		DirectX::XMVECTOR u = DirectX::XMLoadFloat3(&mUp);
		DirectX::XMVECTOR p = DirectX::XMLoadFloat3(&mPosition);
		DirectX::XMStoreFloat3(&mPosition, DirectX::XMVectorMultiplyAdd(s, u, p));

		mViewDirty = true;
	}

	void Camera::RotateX(float angle)
	{
		DirectX::XMMATRIX R = DirectX::XMMatrixRotationAxis(DirectX::XMLoadFloat3(&mRight), angle);
		DirectX::XMStoreFloat3(&mForward, DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&mForward), R));

		mViewDirty = true;
	}

	void Camera::RotateY(float angle)
	{
		DirectX::XMVECTOR upVector = DirectX::XMLoadFloat3(&mUp);
		DirectX::XMMATRIX R = DirectX::XMMatrixRotationAxis(upVector, angle);
		DirectX::XMVECTOR forwardVector = DirectX::XMVector3TransformNormal(DirectX::XMLoadFloat3(&mForward), R);

		DirectX::XMStoreFloat3(&mForward, forwardVector);
		DirectX::XMStoreFloat3(&mRight, DirectX::XMVector3Cross(upVector, forwardVector));

		mViewDirty = true;
	}

	void Camera::UpdateViewMatrix()
	{
		if (mViewDirty)
		{
			DirectX::XMVECTOR position = DirectX::XMLoadFloat3(&mPosition);
			DirectX::XMVECTOR forward = DirectX::XMLoadFloat3(&mForward);
			DirectX::XMVECTOR up = DirectX::XMLoadFloat3(&mUp);

			DirectX::XMMATRIX view = DirectX::XMMatrixLookToLH(position, forward, up);
			DirectX::XMStoreFloat4x4(&mViewMatrix, view);

			mViewDirty = false;
		}
	}
}
