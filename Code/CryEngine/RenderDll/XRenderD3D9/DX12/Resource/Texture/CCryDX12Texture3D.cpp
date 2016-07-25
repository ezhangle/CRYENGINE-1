// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CCryDX12Texture3D.hpp"
#include "../GI/CCryDX12SwapChain.hpp"

#include "DX12/Device/CCryDX12Device.hpp"

CCryDX12Texture3D* CCryDX12Texture3D::Create(CCryDX12Device* pDevice)
{
	D3D12_RESOURCE_DESC desc12;
	D3D11_TEXTURE3D_DESC desc11;

	ZeroStruct(desc12);
	ZeroStruct(desc11);

	desc12.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc12.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
	desc11.Usage = D3D11_USAGE_DEFAULT;

	CCryDX12Texture3D* result = DX12_NEW_RAW(CCryDX12Texture3D(pDevice, desc11, nullptr, D3D12_RESOURCE_STATE_COMMON, CD3DX12_RESOURCE_DESC(desc12), NULL));

	return result;
}

CCryDX12Texture3D* CCryDX12Texture3D::Create(CCryDX12Device* pDevice, CCryDX12SwapChain* pSwapChain, ID3D12Resource* pResource)
{
	DX12_ASSERT(pResource);

	D3D12_RESOURCE_DESC desc12 = pResource->GetDesc();

	D3D11_TEXTURE3D_DESC desc11;
	desc11.Width = static_cast<UINT>(desc12.Width);
	desc11.Height = desc12.Height;
	desc11.MipLevels = static_cast<UINT>(desc12.MipLevels);
	desc11.Format = desc12.Format;
	desc11.Usage = D3D11_USAGE_DEFAULT;
	desc11.CPUAccessFlags = 0;
	desc11.BindFlags = 0;
	desc11.MiscFlags = 0;

	if (desc12.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
	{
		desc11.BindFlags |= D3D11_BIND_RENDER_TARGET;
	}

	if (desc12.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
	{
		desc11.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
	}

	if (desc12.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
	{
		desc11.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
	}

	if (!(desc12.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE))
	{
		desc11.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
	}

	CCryDX12Texture3D* result = DX12_NEW_RAW(CCryDX12Texture3D(pDevice, desc11, pResource, D3D12_RESOURCE_STATE_PRESENT, CD3DX12_RESOURCE_DESC(desc12), NULL));
	result->GetDX12Resource().SetDX12SwapChain(pSwapChain->GetDX12SwapChain());

	return result;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CCryDX12Texture3D* CCryDX12Texture3D::Create(CCryDX12Device* pDevice, const FLOAT cClearValue[4], const D3D11_TEXTURE3D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData)
{
	DX12_ASSERT(pDesc, "CreateTexture() called without description!");

	CD3DX12_RESOURCE_DESC desc12 = CD3DX12_RESOURCE_DESC::Tex3D(
	  pDesc->Format,
	  pDesc->Width, pDesc->Height, pDesc->Depth,
	  pDesc->MipLevels,
	  D3D12_RESOURCE_FLAG_NONE,
	  D3D12_TEXTURE_LAYOUT_UNKNOWN,

	  // Alignment
	  0);

	D3D12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT, pDevice->GetCreationMask(false), pDevice->GetVisibilityMask(false));
	D3D12_RESOURCE_STATES resourceUsage = D3D12_RESOURCE_STATE_COPY_DEST;
	D3D12_CLEAR_VALUE clearValue = NCryDX12::GetDXGIFormatClearValue(desc12.Format, (pDesc->BindFlags & D3D11_BIND_DEPTH_STENCIL) != 0);
	bool allowClearValue = desc12.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER;

#if DX12_ALLOW_TEXTURE_ACCESS
	if (pDesc->Usage == D3D11_USAGE_IMMUTABLE)
	{
		heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT, pDevice->GetCreationMask(false), pDevice->GetVisibilityMask(false));
		resourceUsage = D3D12_RESOURCE_STATE_COPY_DEST;
	}
	else if (pDesc->Usage == D3D11_USAGE_STAGING)
	{
		heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK, pDevice->GetCreationMask(true), pDevice->GetVisibilityMask(true));
		resourceUsage = D3D12_RESOURCE_STATE_COPY_DEST;
	}
	else if (pDesc->Usage == D3D11_USAGE_DYNAMIC)
	{
		heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD, pDevice->GetCreationMask(true), pDevice->GetVisibilityMask(true));
		resourceUsage = D3D12_RESOURCE_STATE_GENERIC_READ;
	}

	if (pDesc->CPUAccessFlags != 0)
	{
		if (pDesc->CPUAccessFlags == D3D11_CPU_ACCESS_WRITE)
		{
			heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD, pDevice->GetCreationMask(true), pDevice->GetVisibilityMask(true));
			resourceUsage = D3D12_RESOURCE_STATE_GENERIC_READ;
		}
		else if (pDesc->CPUAccessFlags == D3D11_CPU_ACCESS_READ)
		{
			heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK, pDevice->GetCreationMask(true), pDevice->GetVisibilityMask(true));
			resourceUsage = D3D12_RESOURCE_STATE_COPY_DEST;
		}
		else
		{
			DX12_NOT_IMPLEMENTED;
			return nullptr;
		}
	}
#else
	DX12_ASSERT(pDesc->Usage == D3D11_USAGE_DEFAULT && !pDesc->CPUAccessFlags, "Unsupported Texture access requested!");
#endif

	allowClearValue = !!(pDesc->BindFlags & (D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_RENDER_TARGET));
	if (cClearValue)
	{
		if (pDesc->BindFlags & D3D11_BIND_DEPTH_STENCIL)
		{
			clearValue.DepthStencil.Depth = FLOAT(cClearValue[0]);
			clearValue.DepthStencil.Stencil = UINT8(cClearValue[1]);
		}
		else
		{
			clearValue.Color[0] = cClearValue[0];
			clearValue.Color[1] = cClearValue[1];
			clearValue.Color[2] = cClearValue[2];
			clearValue.Color[3] = cClearValue[3];
		}
	}

	if (pDesc->BindFlags & D3D11_BIND_UNORDERED_ACCESS)
	{
		desc12.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		resourceUsage = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	}

	if (pDesc->BindFlags & D3D11_BIND_DEPTH_STENCIL)
	{
		desc12.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
		resourceUsage = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	}

	if (pDesc->BindFlags & D3D11_BIND_RENDER_TARGET)
	{
		desc12.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
		resourceUsage = D3D12_RESOURCE_STATE_RENDER_TARGET;
	}

	if (!(pDesc->BindFlags & D3D11_BIND_SHADER_RESOURCE))
	{
		desc12.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
	}

	// Any of these flags require a view with a GPUVirtualAddress, in which case the resource needs to be duplicated for all GPUs
	if (pDesc->BindFlags & (
		D3D11_BIND_VERTEX_BUFFER   | // vertex buffer    view
		D3D11_BIND_INDEX_BUFFER    | // index buffer     view
		D3D11_BIND_CONSTANT_BUFFER | // constant buffer  view
		D3D11_BIND_SHADER_RESOURCE | // shader resource  view
		D3D11_BIND_RENDER_TARGET   | // render target    view
		D3D11_BIND_DEPTH_STENCIL   | // depth stencil    view
		D3D11_BIND_UNORDERED_ACCESS  // unordered access view
	))
	{
		heapProperties.CreationNodeMask = pDevice->GetCreationMask(false) | pDevice->GetVisibilityMask(false);
		heapProperties.VisibleNodeMask = pDevice->GetVisibilityMask(false);

#if CRY_USE_DX12_MULTIADAPTER_SIMULATION
		// Always allow getting GPUAddress (CreationMask == VisibilityMask), if running simulation
		if (CRenderer::CV_r_StereoEnableMgpu < 0 && (heapProperties.Type == D3D12_HEAP_TYPE_UPLOAD || heapProperties.Type == D3D12_HEAP_TYPE_READBACK))
		{
			heapProperties.CreationNodeMask = 1U;
			heapProperties.VisibleNodeMask = pDevice->GetCreationMask(false) | pDevice->GetVisibilityMask(false);
		}
#endif
	}

	ID3D12Resource* resource = NULL;

	if (S_OK != pDevice->GetD3D12Device()->CreateCommittedResource(
	      &heapProperties,
	      D3D12_HEAP_FLAG_NONE,
	      &desc12,
	      resourceUsage,
	      allowClearValue ? &clearValue : NULL,
	      IID_PPV_ARGS(&resource)
	      ) || !resource)
	{
		DX12_ASSERT(0, "Could not create texture 3D resource!");
		return NULL;
	}

	auto result = DX12_NEW_RAW(CCryDX12Texture3D(pDevice, *pDesc, resource, resourceUsage, CD3DX12_RESOURCE_DESC(resource->GetDesc()), pInitialData, desc12.MipLevels));
	resource->Release();

#if !defined(RELEASE) && defined(_DEBUG)
	resource->SetPrivateData(WKPDID_D3DDebugClearValue, sizeof(clearValue), &clearValue);
#endif

	return result;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CCryDX12Texture3D::CCryDX12Texture3D(CCryDX12Device* pDevice, const D3D11_TEXTURE3D_DESC& desc11, ID3D12Resource* pResource, D3D12_RESOURCE_STATES eInitialState, const CD3DX12_RESOURCE_DESC& desc12, const D3D11_SUBRESOURCE_DATA* pInitialData, size_t numInitialData)
	: Super(pDevice, pResource, eInitialState, desc12, pInitialData, numInitialData)
	, m_Desc11(desc11)
{
	m_DX12Resource.MakeConcurrentWritable(m_Desc11.MiscFlags & D3D11_RESOURCE_MISC_UAV_OVERLAP ? true : false);
}

CCryDX12Texture3D::~CCryDX12Texture3D()
{

}
