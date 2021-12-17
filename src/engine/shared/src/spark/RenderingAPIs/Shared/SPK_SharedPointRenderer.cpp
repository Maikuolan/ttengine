//////////////////////////////////////////////////////////////////////////////////
// SPARK particle engine														//
// Copyright (C) 2008-2009 - Julien Fryer - julienfryer@gmail.com				//
//                           foulon matthieu - stardeath@wanadoo.fr				//
//																				//
// This software is provided 'as-is', without any express or implied			//
// warranty.  In no event will the authors be held liable for any damages		//
// arising from the use of this software.										//
//																				//
// Permission is granted to anyone to use this software for any purpose,		//
// including commercial applications, and to alter it and redistribute it		//
// freely, subject to the following restrictions:								//
//																				//
// 1. The origin of this software must not be misrepresented; you must not		//
//    claim that you wrote the original software. If you use this software		//
//    in a product, an acknowledgment in the product documentation would be		//
//    appreciated but is not required.											//
// 2. Altered source versions must be plainly marked as such, and must not be	//
//    misrepresented as being the original software.							//
// 3. This notice may not be removed or altered from any source distribution.	//
//////////////////////////////////////////////////////////////////////////////////

#include <tt/engine/renderer/Renderer.h>

#include <spark/RenderingAPIs/Shared/SPK_SharedPointRenderer.h>
#include <spark/Core/SPK_Particle.h>
#include <spark/Core/SPK_Group.h>

#if SPARK_USE_DX9POINT

namespace SPK
{
namespace Shared
{
	PointVertex* DX9PointRenderer::vertexBuffer = NULL;
	PointVertex* DX9PointRenderer::vertexIterator = NULL;

	LPDIRECT3DVERTEXBUFFER9 DX9PointRenderer::DX9VertexBuffer = NULL;

	const std::string DX9PointRenderer::VERTEX_BUFFER_NAME("SPK_DX9PointRenderer_Vertex");

	const float DX9PointRenderer::QUADRATIC_SCREEN[3] = {1.0f,0.0f,0.0f};

	float DX9PointRenderer::pixelPerUnit = 1024.0f;

	const float DX9PointRenderer::POINT_SIZE_CURRENT = 32.0f;
	const float DX9PointRenderer::POINT_SIZE_MIN = 1.0f;
	const float DX9PointRenderer::POINT_SIZE_MAX = 1024.0f;

	DX9PointRenderer::DX9PointRenderer(float size) :
		TTRenderer(),
		type(POINT_SQUARE),
		size(size),
		worldSize(false)
	{}

	DX9PointRenderer::~DX9PointRenderer()
	{
		DX9DestroyAllBuffers();
	}

	bool DX9PointRenderer::setType(PointType type)
	{
		this->type = type;
		return true;
	}

	bool DX9PointRenderer::enableWorldSize(bool worldSizeEnabled)
	{
		worldSize = worldSizeEnabled;
		return worldSize;
	}

	bool DX9PointRenderer::checkBuffers(const Group& group)
	{
		ArrayBuffer<PointVertex>* pvbBuffer;

		if ((pvbBuffer = dynamic_cast<ArrayBuffer<PointVertex>*>(group.getBuffer(VERTEX_BUFFER_NAME))) == NULL)
			return false;

		vertexIterator = vertexBuffer = pvbBuffer->getData();

		return true;
	}

	void DX9PointRenderer::createBuffers(const Group& group)
	{
		ArrayBuffer<PointVertex>* vbVertexBuffer = dynamic_cast<ArrayBuffer<PointVertex>*>(group.createBuffer(VERTEX_BUFFER_NAME, ArrayBufferCreator<PointVertex>(1), 0, false));
		vertexIterator = vertexBuffer = vbVertexBuffer->getData();
	}

	void DX9PointRenderer::destroyBuffers(const Group& group)
	{
		group.destroyBuffer(VERTEX_BUFFER_NAME);
	}

	void DX9PointRenderer::render(const Group& group)
	{
		if(group.getNbParticles() == 0)
			return;

		if( !DX9PrepareBuffers(group) )
			return;

		if( !prepareBuffers(group) )
			return;

		initBlending();
		initRenderingHints(); // Marco: Added to correctly setup render state

		if (worldSize)
		{
			enablePointParameter(size, true);
			DX9Info::getDevice()->SetRenderState(D3DRS_POINTSCALEENABLE, true);
		}
		else
		{
			DX9Info::getDevice()->SetRenderState(D3DRS_POINTSIZE, FtoDW(size));
			DX9Info::getDevice()->SetRenderState(D3DRS_POINTSCALEENABLE, false);
		}

		//DX9Info::getDevice()->SetTexture(0, textureIndex);
		tt::engine::renderer::Renderer::getInstance()->setTexture(textureIndex);

		DX9Info::getDevice()->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		DX9Info::getDevice()->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
		DX9Info::getDevice()->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TEXTURE);

		// Marco: Fixed this for correct alpha blending
		DX9Info::getDevice()->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		DX9Info::getDevice()->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
		DX9Info::getDevice()->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TEXTURE);

		DX9Info::getDevice()->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
		DX9Info::getDevice()->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

		DX9Info::getDevice()->SetRenderState(D3DRS_POINTSPRITEENABLE, true);

		
		for(size_t i = 0; i < group.getNbParticles(); i++)
		{
			const Particle& particle = group.getParticle(i);

			Assign(vertexBuffer[i].position, particle.position());

			/*
			if( group.getModel()->isEnabled(PARAM_ALPHA) )
			{
				((PointVertex*)ptr)[i].color = D3DCOLOR_COLORVALUE(
					group.getParticle(i).getParamCurrentValue(PARAM_RED),
					group.getParticle(i).getParamCurrentValue(PARAM_GREEN),
					group.getParticle(i).getParamCurrentValue(PARAM_BLUE),
					group.getParticle(i).getParamCurrentValue(PARAM_ALPHA));
			}
			else
			{
				((PointVertex*)ptr)[i].color = D3DCOLOR_COLORVALUE(
					group.getParticle(i).getParamCurrentValue(PARAM_RED),
					group.getParticle(i).getParamCurrentValue(PARAM_GREEN),
					group.getParticle(i).getParamCurrentValue(PARAM_BLUE),
					1.0f);
			}
			*/
			D3DXCOLOR c(particle.getR(), particle.getG(), particle.getB(), particle.getParamCurrentValue(PARAM_ALPHA));
			vertexBuffer[i].color = c;
			/*((PointVertex*)ptr)[i].color = D3DCOLOR_COLORVALUE(
				group.getParticle(i).getR(),
				group.getParticle(i).getG(),
				group.getParticle(i).getB(),
				group.getParticle(i).getParamCurrentValue(PARAM_ALPHA));*/
		}

		void *ptr;
		if(SUCCEEDED(DX9VertexBuffer->Lock(0, 0, &ptr, D3DLOCK_DISCARD)))
		{
			memcpy(ptr, vertexBuffer, group.getNbParticles() * sizeof(PointVertex));
			DX9VertexBuffer->Unlock();

			LPDIRECT3DDEVICE9 device = DX9Info::getDevice();
			device->SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE);
			device->SetStreamSource(0, DX9VertexBuffer, 0, sizeof(PointVertex));
			device->DrawPrimitive(D3DPT_POINTLIST, 0, group.getNbParticles());
		}
	}

	bool DX9PointRenderer::DX9CheckBuffers(const Group& group)
	{
		if( !DX9Bind(group, DX9_VERTEX_BUFFER_KEY, (void**)&DX9VertexBuffer) )
		{
			DX9VertexBuffer = NULL;
			return false;
		}

		return true;
	}

	bool DX9PointRenderer::DX9CreateBuffers(const Group& group)
	{
		std::cout << "DX9PointRenderer::DX9CreateBuffers" << std::endl;

		if( DX9Info::getDevice() == NULL ) return false;

		LPDIRECT3DVERTEXBUFFER9 vb = NULL;

		if(SUCCEEDED(DX9Info::getDevice()->CreateVertexBuffer(
			group.getParticles().getNbReserved() * sizeof(PointVertex),
			D3DUSAGE_WRITEONLY|D3DUSAGE_DYNAMIC,
			(D3DFVF_XYZ|D3DFVF_DIFFUSE),
			D3DPOOL_DEFAULT, &vb, NULL)) == false)
		{
			return false;
		}

		// Insert buffer
		std::pair<const Group *, int> key(&group, DX9_VERTEX_BUFFER_KEY);
		DX9Buffers[key] = vb;

		// Point to the vertex buffer that we 've just created
		DX9VertexBuffer = vb;

		return true;
	}

	bool DX9PointRenderer::DX9DestroyBuffers(const Group& group)
	{
		DX9Release(group, DX9_VERTEX_BUFFER_KEY);
		DX9VertexBuffer = NULL;

		return true;
	}

	void DX9PointRenderer::setPixelPerUnit(float fovy,int screenHeight)
	{
		// the pixel per unit is computed for a distance from the camera of screenHeight
		pixelPerUnit = screenHeight / (2.0f * tan(fovy * 0.5f));
	}

	void DX9PointRenderer::enablePointParameter(float size,bool distance)
	{
		// derived size = size * sqrt(1 / (A + B * distance + C * distance�))
		if (distance)
		{
			const float sqrtC = POINT_SIZE_CURRENT / (size * pixelPerUnit);
			const float QUADRATIC_WORLD[3] = {0.0f, 0.0f, sqrtC * sqrtC}; // A = 0; B = 0; C = (POINT_SIZE_CURRENT / (size * pixelPerUnit))�
			DX9Info::getDevice()->SetRenderState(D3DRS_POINTSCALE_A, FtoDW(QUADRATIC_WORLD[0]));
			DX9Info::getDevice()->SetRenderState(D3DRS_POINTSCALE_B, FtoDW(QUADRATIC_WORLD[1]));
			DX9Info::getDevice()->SetRenderState(D3DRS_POINTSCALE_C, FtoDW(QUADRATIC_WORLD[2]));
		}
		else
		{
			const float sqrtA = POINT_SIZE_CURRENT / size;
			const float QUADRATIC_WORLD[3] = {sqrtA * sqrtA, 0.0f, 0.0f};	// A = (POINT_SIZE_CURRENT / size)�; B = 0; C = 0
			DX9Info::getDevice()->SetRenderState(D3DRS_POINTSCALE_A, FtoDW(QUADRATIC_WORLD[0]));
			DX9Info::getDevice()->SetRenderState(D3DRS_POINTSCALE_B, FtoDW(QUADRATIC_WORLD[1]));
			DX9Info::getDevice()->SetRenderState(D3DRS_POINTSCALE_C, FtoDW(QUADRATIC_WORLD[2]));
		}

		DX9Info::getDevice()->SetRenderState(D3DRS_POINTSIZE, FtoDW(size));
		DX9Info::getDevice()->SetRenderState(D3DRS_POINTSIZE_MIN, FtoDW(POINT_SIZE_MIN));
		DX9Info::getDevice()->SetRenderState(D3DRS_POINTSIZE_MAX, FtoDW(POINT_SIZE_MAX));
	}
}}

#endif
