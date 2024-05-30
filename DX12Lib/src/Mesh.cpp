#include "DX12Lib/Mesh.h"
#include "DX12Lib/UploadBuffer.h"
#include <fstream>

namespace DX12Lib
{
	MeshData MeshGenerator::Box(float width, float height, float depth, uint32_t numSubdivisions)
	{

		MeshData meshData;

		//
		// Create the vertices.
		//

		Vertex v[24];

		float w2 = 0.5f * width;
		float h2 = 0.5f * height;
		float d2 = 0.5f * depth;

		// Fill in the front face vertex data.
		v[0] = Vertex(-w2, -h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		v[1] = Vertex(-w2, +h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
		v[2] = Vertex(+w2, +h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
		v[3] = Vertex(+w2, -h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);

		// Fill in the back face vertex data.
		v[4] = Vertex(-w2, -h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
		v[5] = Vertex(+w2, -h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		v[6] = Vertex(+w2, +h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
		v[7] = Vertex(-w2, +h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f);

		// Fill in the top face vertex data.
		v[8] = Vertex(-w2, +h2, -d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		v[9] = Vertex(-w2, +h2, +d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
		v[10] = Vertex(+w2, +h2, +d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
		v[11] = Vertex(+w2, +h2, -d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);

		// Fill in the bottom face vertex data.
		v[12] = Vertex(-w2, -h2, -d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
		v[13] = Vertex(+w2, -h2, -d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
		v[14] = Vertex(+w2, -h2, +d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
		v[15] = Vertex(-w2, -h2, +d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f);

		// Fill in the left face vertex data.
		v[16] = Vertex(-w2, -h2, +d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f);
		v[17] = Vertex(-w2, +h2, +d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f);
		v[18] = Vertex(-w2, +h2, -d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f);
		v[19] = Vertex(-w2, -h2, -d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f);

		// Fill in the right face vertex data.
		v[20] = Vertex(+w2, -h2, -d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
		v[21] = Vertex(+w2, +h2, -d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
		v[22] = Vertex(+w2, +h2, +d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
		v[23] = Vertex(+w2, -h2, +d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);

		meshData.Vertices.assign(&v[0], &v[24]);

		//
		// Create the indices.
		//

		uint32_t i[36];

		// Fill in the front face index data
		i[0] = 0; i[1] = 1; i[2] = 2;
		i[3] = 0; i[4] = 2; i[5] = 3;

		// Fill in the back face index data
		i[6] = 4; i[7] = 5; i[8] = 6;
		i[9] = 4; i[10] = 6; i[11] = 7;

		// Fill in the top face index data
		i[12] = 8; i[13] = 9; i[14] = 10;
		i[15] = 8; i[16] = 10; i[17] = 11;

		// Fill in the bottom face index data
		i[18] = 12; i[19] = 13; i[20] = 14;
		i[21] = 12; i[22] = 14; i[23] = 15;

		// Fill in the left face index data
		i[24] = 16; i[25] = 17; i[26] = 18;
		i[27] = 16; i[28] = 18; i[29] = 19;

		// Fill in the right face index data
		i[30] = 20; i[31] = 21; i[32] = 22;
		i[33] = 20; i[34] = 22; i[35] = 23;

		meshData.Indices32.assign(&i[0], &i[36]);

		// Put a cap on the number of subdivisions.
		numSubdivisions = std::min<uint32_t>(numSubdivisions, 6u);

		for (uint32_t i = 0; i < numSubdivisions; ++i)
			Subdivide(meshData);

		return meshData;
	}

	MeshData MeshGenerator::Sphere(float radius, uint32_t sliceCount, uint32_t stackCount)
	{

		MeshData meshData;

		//
		// Compute the vertices stating at the top pole and moving down the stacks.
		//

		// Poles: note that there will be texture coordinate distortion as there is
		// not a unique point on the texture map to assign to the pole when mapping
		// a rectangular texture onto a sphere.
		Vertex topVertex(0.0f, +radius, 0.0f, 0.0f, +1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
		Vertex bottomVertex(0.0f, -radius, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);

		meshData.Vertices.push_back(topVertex);

		float phiStep = DirectX::XM_PI / stackCount;
		float thetaStep = DirectX::XM_2PI / sliceCount;

		// Compute vertices for each stack ring (do not count the poles as rings).
		for (uint32_t i = 1; i <= stackCount - 1; ++i)
		{
			float phi = i * phiStep;

			// Vertices of ring.
			for (uint32_t j = 0; j <= sliceCount; ++j)
			{
				float theta = j * thetaStep;

				Vertex v;

				// spherical to cartesian
				v.Position.x = radius * sinf(phi) * cosf(theta);
				v.Position.y = radius * cosf(phi);
				v.Position.z = radius * sinf(phi) * sinf(theta);

				// Partial derivative of P with respect to theta
				v.TangentU.x = -radius * sinf(phi) * sinf(theta);
				v.TangentU.y = 0.0f;
				v.TangentU.z = +radius * sinf(phi) * cosf(theta);

				DirectX::XMVECTOR T = DirectX::XMLoadFloat3(&v.TangentU);
				XMStoreFloat3(&v.TangentU, DirectX::XMVector3Normalize(T));

				DirectX::XMVECTOR p = DirectX::XMLoadFloat3(&v.Position);
				XMStoreFloat3(&v.Normal, DirectX::XMVector3Normalize(p));

				v.TexCoord.x = theta / DirectX::XM_2PI;
				v.TexCoord.y = phi / DirectX::XM_PI;

				meshData.Vertices.push_back(v);
			}
		}

		meshData.Vertices.push_back(bottomVertex);

		//
		// Compute indices for top stack.  The top stack was written first to the vertex buffer
		// and connects the top pole to the first ring.
		//

		for (uint32_t i = 1; i <= sliceCount; ++i)
		{
			meshData.Indices32.push_back(0);
			meshData.Indices32.push_back(i + 1);
			meshData.Indices32.push_back(i);
		}

		//
		// Compute indices for inner stacks (not connected to poles).
		//

		// Offset the indices to the index of the first vertex in the first ring.
		// This is just skipping the top pole vertex.
		uint32_t baseIndex = 1;
		uint32_t ringVertexCount = sliceCount + 1;
		for (uint32_t i = 0; i < stackCount - 2; ++i)
		{
			for (uint32_t j = 0; j < sliceCount; ++j)
			{
				meshData.Indices32.push_back(baseIndex + i * ringVertexCount + j);
				meshData.Indices32.push_back(baseIndex + i * ringVertexCount + j + 1);
				meshData.Indices32.push_back(baseIndex + (i + 1) * ringVertexCount + j);

				meshData.Indices32.push_back(baseIndex + (i + 1) * ringVertexCount + j);
				meshData.Indices32.push_back(baseIndex + i * ringVertexCount + j + 1);
				meshData.Indices32.push_back(baseIndex + (i + 1) * ringVertexCount + j + 1);
			}
		}

		//
		// Compute indices for bottom stack.  The bottom stack was written last to the vertex buffer
		// and connects the bottom pole to the bottom ring.
		//

		// South pole vertex was added last.
		uint32_t southPoleIndex = (uint32_t)meshData.Vertices.size() - 1;

		// Offset the indices to the index of the first vertex in the last ring.
		baseIndex = southPoleIndex - ringVertexCount;

		for (uint32_t i = 0; i < sliceCount; ++i)
		{
			meshData.Indices32.push_back(southPoleIndex);
			meshData.Indices32.push_back(baseIndex + i);
			meshData.Indices32.push_back(baseIndex + i + 1);
		}

		return meshData;
	}

	MeshData MeshGenerator::Cylinder(float bottomRadius, float topRadius, float height, uint32_t sliceCount, uint32_t stackCount)
	{
		MeshData meshData;

		//
		// Build Stacks.
		// 

		float stackHeight = height / stackCount;

		// Amount to increment radius as we move up each stack level from bottom to top.
		float radiusStep = (topRadius - bottomRadius) / stackCount;

		uint32_t ringCount = stackCount + 1;

		// Compute vertices for each stack ring starting at the bottom and moving up.
		for (uint32_t i = 0; i < ringCount; ++i)
		{
			float y = -0.5f * height + i * stackHeight;
			float r = bottomRadius + i * radiusStep;

			// vertices of ring
			float dTheta = DirectX::XM_2PI / sliceCount;
			for (uint32_t j = 0; j <= sliceCount; ++j)
			{
				Vertex vertex;

				float c = cosf(j * dTheta);
				float s = sinf(j * dTheta);

				vertex.Position = DirectX::XMFLOAT3(r * c, y, r * s);

				vertex.TexCoord.x = (float)j / sliceCount;
				vertex.TexCoord.y = 1.0f - (float)i / stackCount;

				// Cylinder can be parameterized as follows, where we introduce v
				// parameter that goes in the same direction as the v tex-coord
				// so that the bitangent goes in the same direction as the v tex-coord.
				//   Let r0 be the bottom radius and let r1 be the top radius.
				//   y(v) = h - hv for v in [0,1].
				//   r(v) = r1 + (r0-r1)v
				//
				//   x(t, v) = r(v)*cos(t)
				//   y(t, v) = h - hv
				//   z(t, v) = r(v)*sin(t)
				// 
				//  dx/dt = -r(v)*sin(t)
				//  dy/dt = 0
				//  dz/dt = +r(v)*cos(t)
				//
				//  dx/dv = (r0-r1)*cos(t)
				//  dy/dv = -h
				//  dz/dv = (r0-r1)*sin(t)

				// This is unit length.
				vertex.TangentU = DirectX::XMFLOAT3(-s, 0.0f, c);

				float dr = bottomRadius - topRadius;
				DirectX::XMFLOAT3 bitangent(dr * c, -height, dr * s);

				DirectX::XMVECTOR T = DirectX::XMLoadFloat3(&vertex.TangentU);
				DirectX::XMVECTOR B = DirectX::XMLoadFloat3(&bitangent);
				DirectX::XMVECTOR N = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(T, B));
				DirectX::XMStoreFloat3(&vertex.Normal, N);

				meshData.Vertices.push_back(vertex);
			}
		}

		// Add one because we duplicate the first and last vertex per ring
		// since the texture coordinates are different.
		uint32_t ringVertexCount = sliceCount + 1;

		// Compute indices for each stack.
		for (uint32_t i = 0; i < stackCount; ++i)
		{
			for (uint32_t j = 0; j < sliceCount; ++j)
			{
				meshData.Indices32.push_back(i * ringVertexCount + j);
				meshData.Indices32.push_back((i + 1) * ringVertexCount + j);
				meshData.Indices32.push_back((i + 1) * ringVertexCount + j + 1);

				meshData.Indices32.push_back(i * ringVertexCount + j);
				meshData.Indices32.push_back((i + 1) * ringVertexCount + j + 1);
				meshData.Indices32.push_back(i * ringVertexCount + j + 1);
			}
		}

		BuildCylinderTopCap(bottomRadius, topRadius, height, sliceCount, stackCount, meshData);
		BuildCylinderBottomCap(bottomRadius, topRadius, height, sliceCount, stackCount, meshData);

		return meshData;
	}

	MeshData MeshGenerator::Grid(float width, float depth, uint32_t m, uint32_t n)
	{

		MeshData meshData;

		uint32_t vertexCount = m * n;
		uint32_t faceCount = (m - 1) * (n - 1) * 2;

		//
		// Create the vertices.
		//

		float halfWidth = 0.5f * width;
		float halfDepth = 0.5f * depth;

		float dx = width / (n - 1);
		float dz = depth / (m - 1);

		float du = 1.0f / (n - 1);
		float dv = 1.0f / (m - 1);

		meshData.Vertices.resize(vertexCount);
		for (uint32_t i = 0; i < m; ++i)
		{
			float z = halfDepth - i * dz;
			for (uint32_t j = 0; j < n; ++j)
			{
				float x = -halfWidth + j * dx;

				meshData.Vertices[i * n + j].Position = DirectX::XMFLOAT3(x, 0.0f, z);
				meshData.Vertices[i * n + j].Normal = DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
				meshData.Vertices[i * n + j].TangentU = DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f);

				// Stretch texture over grid.
				meshData.Vertices[i * n + j].TexCoord.x = j * du;
				meshData.Vertices[i * n + j].TexCoord.y = i * dv;
			}
		}

		//
		// Create the indices.
		//

		meshData.Indices32.resize(faceCount * 3); // 3 indices per face

		// Iterate over each quad and compute indices.
		uint32_t k = 0;
		for (uint32_t i = 0; i < m - 1; ++i)
		{
			for (uint32_t j = 0; j < n - 1; ++j)
			{
				meshData.Indices32[k] = i * n + j;
				meshData.Indices32[k + 1] = i * n + j + 1;
				meshData.Indices32[k + 2] = (i + 1) * n + j;

				meshData.Indices32[k + 3] = (i + 1) * n + j;
				meshData.Indices32[k + 4] = i * n + j + 1;
				meshData.Indices32[k + 5] = (i + 1) * n + j + 1;

				k += 6; // next quad
			}
		}

		return meshData;
	}

	DX12Lib::MeshData MeshGenerator::Quad(float x, float y, float width, float height, float depth)
	{
		MeshData meshData;

		meshData.Vertices.resize(4);
		meshData.Indices32.resize(6);

		// Position coordinates specified in NDC space.
		meshData.Vertices[0] = Vertex(
			x, y - height, depth,
			0.0f, 0.0f, -1.0f,
			1.0f, 0.0f, 0.0f,
			0.0f, 1.0f);

		meshData.Vertices[1] = Vertex(
			x, y, depth,
			0.0f, 0.0f, -1.0f,
			1.0f, 0.0f, 0.0f,
			0.0f, 0.0f);

		meshData.Vertices[2] = Vertex(
			x + width, y, depth,
			0.0f, 0.0f, -1.0f,
			1.0f, 0.0f, 0.0f,
			1.0f, 0.0f);

		meshData.Vertices[3] = Vertex(
			x + width, y - height, depth,
			0.0f, 0.0f, -1.0f,
			1.0f, 0.0f, 0.0f,
			1.0f, 1.0f);

		meshData.Indices32[0] = 0;
		meshData.Indices32[1] = 1;
		meshData.Indices32[2] = 2;

		meshData.Indices32[3] = 0;
		meshData.Indices32[4] = 2;
		meshData.Indices32[5] = 3;

		return meshData;
	}

	void MeshGenerator::Subdivide(MeshData& meshData)
	{
		// Save a copy of the input geometry.
		MeshData inputCopy = meshData;

		meshData.Vertices.resize(0);
		meshData.Indices32.resize(0);

		//       v1
		//       *
		//      / \
		//     /   \
		//  m0*-----*m1
		//   / \   / \
		//  /   \ /   \
		// *-----*-----*
		// v0    m2     v2

		uint32_t numTris = (uint32_t)inputCopy.Indices32.size() / 3;
		for (uint32_t i = 0; i < numTris; ++i)
		{
			Vertex v0 = inputCopy.Vertices[inputCopy.Indices32[i * 3 + 0]];
			Vertex v1 = inputCopy.Vertices[inputCopy.Indices32[i * 3 + 1]];
			Vertex v2 = inputCopy.Vertices[inputCopy.Indices32[i * 3 + 2]];

			//
			// Generate the midpoints.
			//

			Vertex m0 = MidPoint(v0, v1);
			Vertex m1 = MidPoint(v1, v2);
			Vertex m2 = MidPoint(v0, v2);

			//
			// Add new geometry.
			//

			meshData.Vertices.push_back(v0); // 0
			meshData.Vertices.push_back(v1); // 1
			meshData.Vertices.push_back(v2); // 2
			meshData.Vertices.push_back(m0); // 3
			meshData.Vertices.push_back(m1); // 4
			meshData.Vertices.push_back(m2); // 5

			meshData.Indices32.push_back(i * 6 + 0);
			meshData.Indices32.push_back(i * 6 + 3);
			meshData.Indices32.push_back(i * 6 + 5);

			meshData.Indices32.push_back(i * 6 + 3);
			meshData.Indices32.push_back(i * 6 + 4);
			meshData.Indices32.push_back(i * 6 + 5);

			meshData.Indices32.push_back(i * 6 + 5);
			meshData.Indices32.push_back(i * 6 + 4);
			meshData.Indices32.push_back(i * 6 + 2);

			meshData.Indices32.push_back(i * 6 + 3);
			meshData.Indices32.push_back(i * 6 + 1);
			meshData.Indices32.push_back(i * 6 + 4);
		}
	}

	Vertex MeshGenerator::MidPoint(const Vertex& v0, const Vertex& v1)
	{
		DirectX::XMVECTOR p0 = DirectX::XMLoadFloat3(&v0.Position);
		DirectX::XMVECTOR p1 = DirectX::XMLoadFloat3(&v1.Position);

		DirectX::XMVECTOR n0 = DirectX::XMLoadFloat3(&v0.Normal);
		DirectX::XMVECTOR n1 = DirectX::XMLoadFloat3(&v1.Normal);

		DirectX::XMVECTOR tan0 = DirectX::XMLoadFloat3(&v0.TangentU);
		DirectX::XMVECTOR tan1 = DirectX::XMLoadFloat3(&v1.TangentU);

		DirectX::XMVECTOR tex0 = DirectX::XMLoadFloat2(&v0.TexCoord);
		DirectX::XMVECTOR tex1 = DirectX::XMLoadFloat2(&v1.TexCoord);

		// Compute the midpoints of all the attributes.  Vectors need to be normalized
		// since linear interpolating can make them not unit length.  
		DirectX::XMVECTOR pos = DirectX::XMVectorScale(DirectX::XMVectorAdd(p0, p1), 0.5f);
		DirectX::XMVECTOR normal = DirectX::XMVector3Normalize(DirectX::XMVectorScale(DirectX::XMVectorAdd(n0, n1), 0.5f));
		DirectX::XMVECTOR tangent = DirectX::XMVector3Normalize(DirectX::XMVectorScale(DirectX::XMVectorAdd(tan0, tan1), 0.5f));
		DirectX::XMVECTOR tex = DirectX::XMVectorScale(DirectX::XMVectorAdd(tex0, tex1), 0.5f);

		Vertex v;
		DirectX::XMStoreFloat3(&v.Position, pos);
		DirectX::XMStoreFloat3(&v.Normal, normal);
		DirectX::XMStoreFloat3(&v.TangentU, tangent);
		DirectX::XMStoreFloat2(&v.TexCoord, tex);

		return v;
	}

	void MeshGenerator::BuildCylinderTopCap(float bottomRadius, float topRadius, float height, uint32_t sliceCount, uint32_t stackCount, MeshData& meshData)
	{
		uint32_t baseIndex = (uint32_t)meshData.Vertices.size();

		float y = 0.5f * height;
		float dTheta = DirectX::XM_2PI / sliceCount;

		// Duplicate cap ring vertices because the texture coordinates and normals differ.
		for (uint32_t i = 0; i <= sliceCount; ++i)
		{
			float x = topRadius * cosf(i * dTheta);
			float z = topRadius * sinf(i * dTheta);

			// Scale down by the height to try and make top cap texture coord area
			// proportional to base.
			float u = x / height + 0.5f;
			float v = z / height + 0.5f;

			meshData.Vertices.push_back(Vertex(x, y, z, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, u, v));
		}

		// Cap center vertex.
		meshData.Vertices.push_back(Vertex(0.0f, y, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.5f, 0.5f));

		// Index of center vertex.
		uint32_t centerIndex = (uint32_t)meshData.Vertices.size() - 1;

		for (uint32_t i = 0; i < sliceCount; ++i)
		{
			meshData.Indices32.push_back(centerIndex);
			meshData.Indices32.push_back(baseIndex + i + 1);
			meshData.Indices32.push_back(baseIndex + i);
		}
	}

	void MeshGenerator::BuildCylinderBottomCap(float bottomRadius, float topRadius, float height, uint32_t sliceCount, uint32_t stackCount, MeshData& meshData)
	{
		uint32_t baseIndex = (uint32_t)meshData.Vertices.size();
		float y = -0.5f * height;

		// vertices of ring
		float dTheta = DirectX::XM_2PI / sliceCount;
		for (uint32_t i = 0; i <= sliceCount; ++i)
		{
			float x = bottomRadius * cosf(i * dTheta);
			float z = bottomRadius * sinf(i * dTheta);

			// Scale down by the height to try and make top cap texture coord area
			// proportional to base.
			float u = x / height + 0.5f;
			float v = z / height + 0.5f;

			meshData.Vertices.push_back(Vertex(x, y, z, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, u, v));
		}

		// Cap center vertex.
		meshData.Vertices.push_back(Vertex(0.0f, y, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.5f, 0.5f));

		// Cache the index of center vertex.
		uint32_t centerIndex = (uint32_t)meshData.Vertices.size() - 1;

		for (uint32_t i = 0; i < sliceCount; ++i)
		{
			meshData.Indices32.push_back(centerIndex);
			meshData.Indices32.push_back(baseIndex + i);
			meshData.Indices32.push_back(baseIndex + i + 1);
		}
	}

	Keyframe::Keyframe()
		: TimePos(0.0f)
		, Translation(0.0f, 0.0f, 0.0f)
		, Scale(1.0f, 1.0f, 1.0f)
		, RotationQuat(0.0f, 0.0f, 0.0f, 1.0f)
	{
	}

	void BoneAnimation::Interpolate(float t, DirectX::XMFLOAT4X4& M) const
	{
		if (t <= GetStartTime())
		{
			DirectX::XMVECTOR S = DirectX::XMLoadFloat3(&Keyframes.front().Scale);
			DirectX::XMVECTOR R = DirectX::XMLoadFloat4(&Keyframes.front().RotationQuat);
			DirectX::XMVECTOR T = DirectX::XMLoadFloat3(&Keyframes.front().Translation);
			DirectX::XMVECTOR origin = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
			DirectX::XMStoreFloat4x4(&M, DirectX::XMMatrixAffineTransformation(S, origin, R, T));
		}
		else if (t >= GetEndTime())
		{
			DirectX::XMVECTOR S = DirectX::XMLoadFloat3(&Keyframes.back().Scale);
			DirectX::XMVECTOR R = DirectX::XMLoadFloat4(&Keyframes.back().RotationQuat);
			DirectX::XMVECTOR T = DirectX::XMLoadFloat3(&Keyframes.back().Translation);
			DirectX::XMVECTOR origin = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
			DirectX::XMStoreFloat4x4(&M, DirectX::XMMatrixAffineTransformation(S, origin, R, T));
		}
		else
		{
			for (UINT i = 0; i < Keyframes.size() - 1; ++i)
			{
				if (t >= Keyframes[i].TimePos && t <= Keyframes[i + 1].TimePos)
				{
					float lerpPercent = (t - Keyframes[i].TimePos) / (Keyframes[i + 1].TimePos - Keyframes[i].TimePos);

					DirectX::XMVECTOR S0 = DirectX::XMLoadFloat3(&Keyframes[i].Scale);
					DirectX::XMVECTOR S1 = DirectX::XMLoadFloat3(&Keyframes[i + 1].Scale);

					DirectX::XMVECTOR R0 = DirectX::XMLoadFloat4(&Keyframes[i].RotationQuat);
					DirectX::XMVECTOR R1 = DirectX::XMLoadFloat4(&Keyframes[i + 1].RotationQuat);

					DirectX::XMVECTOR T0 = DirectX::XMLoadFloat3(&Keyframes[i].Translation);
					DirectX::XMVECTOR T1 = DirectX::XMLoadFloat3(&Keyframes[i + 1].Translation);

					DirectX::XMVECTOR S = DirectX::XMVectorLerp(S0, S1, lerpPercent);
					DirectX::XMVECTOR R = DirectX::XMQuaternionSlerp(R0, R1, lerpPercent);
					DirectX::XMVECTOR T = DirectX::XMVectorLerp(T0, T1, lerpPercent);

					DirectX::XMVECTOR origin = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
					DirectX::XMStoreFloat4x4(&M, DirectX::XMMatrixAffineTransformation(S, origin, R, T));

					break;
				}
			}
		}
	}

	float AnimationClip::GetStartTime() const
	{
		float t = FLT_MAX;
		for (UINT i = 0; i < BoneAnimations.size(); ++i)
		{
			t = std::min(t, BoneAnimations[i].GetStartTime());
		}
		return t;
	}

	float AnimationClip::GetEndTime() const
	{
		float t = 0.0f;
		for (UINT i = 0; i < BoneAnimations.size(); ++i)
		{
			t = std::max(t, BoneAnimations[i].GetEndTime());
		}
		return t;
	}

	void AnimationClip::Interpolate(float t, std::vector<DirectX::XMFLOAT4X4>& boneTransforms) const
	{
		for (UINT i = 0; i < BoneAnimations.size(); ++i)
		{
			BoneAnimations[i].Interpolate(t, boneTransforms[i]);
		}
	}

	float SkinnedData::GetAnimationClipStartTime(const std::wstring& clipName) const
	{
		auto clip = mAnimations.find(clipName);
		return clip->second.GetStartTime();
	}

	float SkinnedData::GetAnimationClipEndTime(const std::wstring& clipName) const
	{
		auto clip = mAnimations.find(clipName);
		return clip->second.GetEndTime();
	}


	void SkinnedData::GetFinalTransform(const std::wstring& clipName, float timePos, std::vector<DirectX::XMFLOAT4X4>& finalTransforms) const
	{
		UINT numBones = mBoneOffsets.size();

		std::vector<DirectX::XMFLOAT4X4> toParentTransforms(numBones);
		auto clip = mAnimations.find(clipName);
		clip->second.Interpolate(timePos, toParentTransforms);

		std::vector<DirectX::XMFLOAT4X4> toRootTransforms(numBones);
		toRootTransforms[0] = toParentTransforms[0];
	
		for (UINT i = 1; i < numBones; ++i)
		{
			DirectX::XMMATRIX toParent = DirectX::XMLoadFloat4x4(&toParentTransforms[i]);
			DirectX::XMMATRIX parentToRoot = DirectX::XMLoadFloat4x4(&toRootTransforms[mBoneHierarchy[i]]);
			DirectX::XMStoreFloat4x4(&toRootTransforms[i], DirectX::XMMatrixMultiply(toParent, parentToRoot));
		}

		for (UINT i = 1; i < numBones; ++i)
		{
			DirectX::XMMATRIX offset = DirectX::XMLoadFloat4x4(&mBoneOffsets[i]);
			DirectX::XMMATRIX toRoot = DirectX::XMLoadFloat4x4(&toRootTransforms[i]);
			DirectX::XMMATRIX finalTransform = DirectX::XMMatrixMultiply(offset, toRoot);
			DirectX::XMStoreFloat4x4(&finalTransforms[i], DirectX::XMMatrixTranspose(finalTransform));
		}
	}

	bool M3DLoader::LoadM3d(const std::wstring& filename,
		std::vector<Vertex>& vertices,
		std::vector<USHORT>& indices,
		std::vector<Subset>& subsets,
		std::vector<M3dMaterial>& mats)
	{
		std::wifstream fin(filename);

		UINT numMaterials = 0;
		UINT numVertices = 0;
		UINT numTriangles = 0;
		UINT numBones = 0;
		UINT numAnimationClips = 0;

		std::wstring ignore;

		if (fin)
		{
			fin >> ignore; // file header text
			fin >> ignore >> numMaterials;
			fin >> ignore >> numVertices;
			fin >> ignore >> numTriangles;
			fin >> ignore >> numBones;
			fin >> ignore >> numAnimationClips;

			ReadMaterials(fin, numMaterials, mats);
			ReadSubsetTable(fin, numMaterials, subsets);
			ReadVertices(fin, numVertices, vertices);
			ReadTriangles(fin, numTriangles, indices);

			return true;
		}
		return false;
	}

	bool M3DLoader::LoadM3d(const std::wstring& filename,
		std::vector<SkinnedVertex>& vertices,
		std::vector<USHORT>& indices,
		std::vector<Subset>& subsets,
		std::vector<M3dMaterial>& mats,
		SkinnedData& skinInfo)
	{
		std::wifstream fin(filename);

		UINT numMaterials = 0;
		UINT numVertices = 0;
		UINT numTriangles = 0;
		UINT numBones = 0;
		UINT numAnimationClips = 0;

		std::wstring ignore;

		if (fin)
		{
			fin >> ignore; // file header text
			fin >> ignore >> numMaterials;
			fin >> ignore >> numVertices;
			fin >> ignore >> numTriangles;
			fin >> ignore >> numBones;
			fin >> ignore >> numAnimationClips;

			std::vector<DirectX::XMFLOAT4X4> boneOffsets;
			std::vector<int> boneIndexToParentIndex;
			std::unordered_map<std::wstring, AnimationClip> animations;

			ReadMaterials(fin, numMaterials, mats);
			ReadSubsetTable(fin, numMaterials, subsets);
			ReadSkinnedVertices(fin, numVertices, vertices);
			ReadTriangles(fin, numTriangles, indices);
			ReadBoneOffsets(fin, numBones, boneOffsets);
			ReadBoneHierarchy(fin, numBones, boneIndexToParentIndex);
			ReadAnimationClips(fin, numBones, numAnimationClips, animations);

			skinInfo.SetBoneHierarchy(boneIndexToParentIndex);
			skinInfo.SetBoneOffsets(boneOffsets);
			skinInfo.SetAnimations(animations);
			return true;
		}
		return false;
	}

	void M3DLoader::ReadMaterials(std::wifstream& fin, UINT numMaterials, std::vector<M3dMaterial>& mats)
	{
		std::wstring ignore;
		mats.resize(numMaterials);

		std::wstring diffuseMapName;
		std::wstring normalMapName;

		fin >> ignore; // materials header text
		for (UINT i = 0; i < numMaterials; ++i)
		{
			fin >> ignore >> mats[i].Name;
			fin >> ignore >> mats[i].DiffuseAlbedo.x >> mats[i].DiffuseAlbedo.y >> mats[i].DiffuseAlbedo.z;
			fin >> ignore >> mats[i].FresnelR0.x >> mats[i].FresnelR0.y >> mats[i].FresnelR0.z;
			fin >> ignore >> mats[i].Roughness;
			fin >> ignore >> mats[i].AlphaClip;
			fin >> ignore >> mats[i].MaterialTypeName;
			fin >> ignore >> mats[i].DiffuseMapName;
			fin >> ignore >> mats[i].NormalMapName;
		}
	}

	void M3DLoader::ReadSubsetTable(std::wifstream& fin, UINT numSubsets, std::vector<Subset>& subsets)
	{
		std::wstring ignore;
		subsets.resize(numSubsets);

		fin >> ignore; // subset header text
		for (UINT i = 0; i < numSubsets; ++i)
		{
			fin >> ignore >> subsets[i].Id;
			fin >> ignore >> subsets[i].VertexStart;
			fin >> ignore >> subsets[i].VertexCount;
			fin >> ignore >> subsets[i].FaceStart;
			fin >> ignore >> subsets[i].FaceCount;
		}
	}

	void M3DLoader::ReadVertices(std::wifstream& fin, UINT numVertices, std::vector<Vertex>& vertices)
	{
		std::wstring ignore;
		vertices.resize(numVertices);

		fin >> ignore; // vertices header text
		for (UINT i = 0; i < numVertices; ++i)
		{
			float blah;
			fin >> ignore >> vertices[i].Position.x >> vertices[i].Position.y >> vertices[i].Position.z;
			//fin >> ignore >> vertices[i].TangentU.x >> vertices[i].TangentU.y >> vertices[i].TangentU.z >> vertices[i].TangentU.w;
			fin >> ignore >> vertices[i].TangentU.x >> vertices[i].TangentU.y >> vertices[i].TangentU.z >> blah /*vertices[i].TangentU.w*/;
			fin >> ignore >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;
			fin >> ignore >> vertices[i].TexCoord.x >> vertices[i].TexCoord.y;
		}
	}

	void M3DLoader::ReadSkinnedVertices(std::wifstream& fin, UINT numVertices, std::vector<SkinnedVertex>& vertices)
	{
		std::wstring ignore;
		vertices.resize(numVertices);

		fin >> ignore; // vertices header text
		int boneIndices[4];
		float weights[4];
		for (UINT i = 0; i < numVertices; ++i)
		{
			float blah;
			fin >> ignore >> vertices[i].Position.x >> vertices[i].Position.y >> vertices[i].Position.z;
			fin >> ignore >> vertices[i].TangentU.x >> vertices[i].TangentU.y >> vertices[i].TangentU.z >> blah /*vertices[i].TangentU.w*/;
			fin >> ignore >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;
			fin >> ignore >> vertices[i].TexCoord.x >> vertices[i].TexCoord.y;
			fin >> ignore >> weights[0] >> weights[1] >> weights[2] >> weights[3];
			fin >> ignore >> boneIndices[0] >> boneIndices[1] >> boneIndices[2] >> boneIndices[3];

			vertices[i].BoneWeights.x = weights[0];
			vertices[i].BoneWeights.y = weights[1];
			vertices[i].BoneWeights.z = weights[2];

			vertices[i].BoneIndices[0] = (BYTE)boneIndices[0];
			vertices[i].BoneIndices[1] = (BYTE)boneIndices[1];
			vertices[i].BoneIndices[2] = (BYTE)boneIndices[2];
			vertices[i].BoneIndices[3] = (BYTE)boneIndices[3];
		}
	}

	void M3DLoader::ReadTriangles(std::wifstream& fin, UINT numTriangles, std::vector<USHORT>& indices)
	{
		std::wstring ignore;
		indices.resize(numTriangles * 3);

		fin >> ignore; // triangles header text
		for (UINT i = 0; i < numTriangles; ++i)
		{
			fin >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];
		}
	}

	void M3DLoader::ReadBoneOffsets(std::wifstream& fin, UINT numBones, std::vector<DirectX::XMFLOAT4X4>& boneOffsets)
	{
		std::wstring ignore;
		boneOffsets.resize(numBones);

		fin >> ignore; // BoneOffsets header text
		for (UINT i = 0; i < numBones; ++i)
		{
			fin >> ignore >>
				boneOffsets[i](0, 0) >> boneOffsets[i](0, 1) >> boneOffsets[i](0, 2) >> boneOffsets[i](0, 3) >>
				boneOffsets[i](1, 0) >> boneOffsets[i](1, 1) >> boneOffsets[i](1, 2) >> boneOffsets[i](1, 3) >>
				boneOffsets[i](2, 0) >> boneOffsets[i](2, 1) >> boneOffsets[i](2, 2) >> boneOffsets[i](2, 3) >>
				boneOffsets[i](3, 0) >> boneOffsets[i](3, 1) >> boneOffsets[i](3, 2) >> boneOffsets[i](3, 3);
		}
	}

	void M3DLoader::ReadBoneHierarchy(std::wifstream& fin, UINT numBones, std::vector<int>& boneIndexToParentIndex)
	{
		std::wstring ignore;
		boneIndexToParentIndex.resize(numBones);

		fin >> ignore; // BoneHierarchy header text
		for (UINT i = 0; i < numBones; ++i)
		{
			fin >> ignore >> boneIndexToParentIndex[i];
		}
	}

	void M3DLoader::ReadAnimationClips(std::wifstream& fin, UINT numBones, UINT numAnimationClips,
		std::unordered_map<std::wstring, AnimationClip>& animations)
	{
		std::wstring ignore;
		fin >> ignore; // AnimationClips header text
		for (UINT clipIndex = 0; clipIndex < numAnimationClips; ++clipIndex)
		{
			std::wstring clipName;
			fin >> ignore >> clipName;
			fin >> ignore; // {

			AnimationClip clip;
			clip.BoneAnimations.resize(numBones);

			for (UINT boneIndex = 0; boneIndex < numBones; ++boneIndex)
			{
				ReadBoneKeyframes(fin, numBones, clip.BoneAnimations[boneIndex]);
			}
			fin >> ignore; // }

			animations[clipName.c_str()] = clip;
		}
	}

	void M3DLoader::ReadBoneKeyframes(std::wifstream& fin, UINT numBones, BoneAnimation& boneAnimation)
	{
		std::wstring ignore;
		UINT numKeyframes = 0;
		fin >> ignore >> ignore >> numKeyframes;
		fin >> ignore; // {

		boneAnimation.Keyframes.resize(numKeyframes);
		for (UINT i = 0; i < numKeyframes; ++i)
		{
			float t = 0.0f;
			DirectX::XMFLOAT3 p(0.0f, 0.0f, 0.0f);
			DirectX::XMFLOAT3 s(1.0f, 1.0f, 1.0f);
			DirectX::XMFLOAT4 q(0.0f, 0.0f, 0.0f, 1.0f);
			fin >> ignore >> t;
			fin >> ignore >> p.x >> p.y >> p.z;
			fin >> ignore >> s.x >> s.y >> s.z;
			fin >> ignore >> q.x >> q.y >> q.z >> q.w;

			boneAnimation.Keyframes[i].TimePos = t;
			boneAnimation.Keyframes[i].Translation = p;
			boneAnimation.Keyframes[i].Scale = s;
			boneAnimation.Keyframes[i].RotationQuat = q;
		}

		fin >> ignore; // }
	}

}