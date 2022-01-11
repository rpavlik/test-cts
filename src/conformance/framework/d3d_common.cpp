// Copyright (c) 2017-2022, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0
#if (defined(XR_USE_GRAPHICS_API_D3D11) || defined(XR_USE_GRAPHICS_API_D3D12)) && !defined(MISSING_DIRECTX_COLORS)

#include <wrl/client.h>  // For Microsoft::WRL::ComPtr
#include "swapchain_parameters.h"
#include "conformance_framework.h"
#include <common/xr_linear.h>
#include <DirectXColors.h>
#include <D3Dcompiler.h>

#include "d3d_common.h"

using namespace Microsoft::WRL;
using namespace DirectX;

namespace Conformance
{
    XMMATRIX XM_CALLCONV LoadXrPose(const XrPosef& pose)
    {
        return XMMatrixAffineTransformation(DirectX::g_XMOne, DirectX::g_XMZero,
                                            XMLoadFloat4(reinterpret_cast<const XMFLOAT4*>(&pose.orientation)),
                                            XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&pose.position)));
    }

    XMMATRIX XM_CALLCONV LoadXrMatrix(const XrMatrix4x4f& matrix)
    {
        // XrMatrix4x4f has same memory layout as DirectX Math (Row-major,post-multiplied = column-major,pre-multiplied)
        return XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(&matrix));
    }

    ComPtr<ID3DBlob> CompileShader(const char* hlsl, const char* entrypoint, const char* shaderTarget)
    {
        ComPtr<ID3DBlob> compiled;
        ComPtr<ID3DBlob> errMsgs;
        DWORD flags = D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR | D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS;

#if !defined(NDEBUG)
        flags |= D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_DEBUG;
#else
        flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

        HRESULT hr = D3DCompile(hlsl, strlen(hlsl), nullptr, nullptr, nullptr, entrypoint, shaderTarget, flags, 0, compiled.GetAddressOf(),
                                errMsgs.GetAddressOf());
        if (FAILED(hr)) {
            std::string errMsg((const char*)errMsgs->GetBufferPointer(), errMsgs->GetBufferSize());
            // Log(Fmt("D3DCompile failed %X: %s", hr, errMsg.c_str()));
            XRC_CHECK_THROW_HRESULT(hr, "D3DCompile");
        }

        return compiled;
    }

    bool operator==(LUID luid, uint64_t id)
    {
        return ((((uint64_t)luid.HighPart << 32) | (uint64_t)luid.LowPart) == id);
    }

    // If adapterId is 0 then use the first adapter we find, the default adapter.
    ComPtr<IDXGIAdapter1> GetDXGIAdapter(LUID adapterId) noexcept(false)
    {
        ComPtr<IDXGIAdapter1> dxgiAdapter;
        ComPtr<IDXGIFactory1> dxgiFactory;

        HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(dxgiFactory.ReleaseAndGetAddressOf()));
        XRC_CHECK_THROW_HRESULT(hr, "GetAdapter: CreateDXGIFactory1");

        for (UINT adapterIndex = 0;; adapterIndex++) {
            // EnumAdapters1 will fail with DXGI_ERROR_NOT_FOUND when there are no more adapters to enumerate.
            hr = dxgiFactory->EnumAdapters1(adapterIndex, dxgiAdapter.ReleaseAndGetAddressOf());

            DXGI_ADAPTER_DESC1 adapterDesc;
            hr = dxgiAdapter->GetDesc1(&adapterDesc);
            XRC_CHECK_THROW_HRESULT(hr, "dxgiAdapter->GetDesc1");

            if ((adapterId == 0) || memcmp(&adapterDesc.AdapterLuid, &adapterId, sizeof(adapterId)) == 0) {
                return dxgiAdapter;
            }
        }
    }

    // Shorthand constants for usage below.
    static const uint64_t XRC_COLOR_TEXTURE_USAGE = (XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT);

    static const uint64_t XRC_COLOR_TEXTURE_USAGE_MUTABLE = (XRC_COLOR_TEXTURE_USAGE | XR_SWAPCHAIN_USAGE_MUTABLE_FORMAT_BIT);

    static const uint64_t XRC_COLOR_TEXTURE_USAGE_COMPRESSED =
        (XR_SWAPCHAIN_USAGE_SAMPLED_BIT);  // Compressed textures can't be rendered to, so no XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT.

    static const uint64_t XRC_DEPTH_TEXTURE_USAGE = (XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT);

#define XRC_COLOR_CREATE_FLAGS                                                             \
    {                                                                                      \
        0, XR_SWAPCHAIN_CREATE_PROTECTED_CONTENT_BIT, XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT \
    }

#define XRC_DEPTH_CREATE_FLAGS                  \
    {                                           \
        0, XR_SWAPCHAIN_CREATE_STATIC_IMAGE_BIT \
    }

    // clang-format off
    SwapchainTestMap dxgiSwapchainTestMap{
        {{DXGI_FORMAT_R32G32B32A32_TYPELESS}, {"DXGI_FORMAT_R32G32B32A32_TYPELESS", true, true, true, false, DXGI_FORMAT_R32G32B32A32_TYPELESS, {}, {}, {}, {}, {}}}, 
        {{DXGI_FORMAT_R32G32B32A32_FLOAT}, {"DXGI_FORMAT_R32G32B32A32_FLOAT", false, true, true, false, DXGI_FORMAT_R32G32B32A32_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_R32G32B32A32_UINT}, {"DXGI_FORMAT_R32G32B32A32_UINT", false, true, true, false, DXGI_FORMAT_R32G32B32A32_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_R32G32B32A32_SINT}, {"DXGI_FORMAT_R32G32B32A32_SINT", false, true, true, false, DXGI_FORMAT_R32G32B32A32_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{DXGI_FORMAT_R32G32B32_TYPELESS}, {"DXGI_FORMAT_R32G32B32_TYPELESS", true, true, true, false, DXGI_FORMAT_R32G32B32_TYPELESS, {}, {}, {}, {}, {}}}, 
        {{DXGI_FORMAT_R32G32B32_FLOAT}, {"DXGI_FORMAT_R32G32B32_FLOAT", false, true, true, false, DXGI_FORMAT_R32G32B32_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_R32G32B32_UINT}, {"DXGI_FORMAT_R32G32B32_UINT", false, true, true, false, DXGI_FORMAT_R32G32B32_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_R32G32B32_SINT}, {"DXGI_FORMAT_R32G32B32_SINT", false, true, true, false, DXGI_FORMAT_R32G32B32_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{DXGI_FORMAT_R16G16B16A16_TYPELESS}, {"DXGI_FORMAT_R16G16B16A16_TYPELESS", true, true, true, false, DXGI_FORMAT_R16G16B16A16_TYPELESS, {}, {}, {}, {}, {}}}, 
        {{DXGI_FORMAT_R16G16B16A16_FLOAT}, {"DXGI_FORMAT_R16G16B16A16_FLOAT", false, true, true, false, DXGI_FORMAT_R16G16B16A16_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_R16G16B16A16_UNORM}, {"DXGI_FORMAT_R16G16B16A16_UNORM", false, true, true, false, DXGI_FORMAT_R16G16B16A16_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_R16G16B16A16_UINT}, {"DXGI_FORMAT_R16G16B16A16_UINT", false, true, true, false, DXGI_FORMAT_R16G16B16A16_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_R16G16B16A16_SNORM}, {"DXGI_FORMAT_R16G16B16A16_SNORM", false, true, true, false, DXGI_FORMAT_R16G16B16A16_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_R16G16B16A16_SINT}, {"DXGI_FORMAT_R16G16B16A16_SINT", false, true, true, false, DXGI_FORMAT_R16G16B16A16_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{DXGI_FORMAT_R32G32_TYPELESS}, {"DXGI_FORMAT_R32G32_TYPELESS", true, true, true, false, DXGI_FORMAT_R32G32_TYPELESS, {}, {}, {}, {}, {}}}, 
        {{DXGI_FORMAT_R32G32_FLOAT}, {"DXGI_FORMAT_R32G32_FLOAT", false, true, true, false, DXGI_FORMAT_R32G32_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_R32G32_UINT}, {"DXGI_FORMAT_R32G32_UINT", false, true, true, false, DXGI_FORMAT_R32G32_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_R32G32_SINT}, {"DXGI_FORMAT_R32G32_SINT", false, true, true, false, DXGI_FORMAT_R32G32_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{DXGI_FORMAT_R32G8X24_TYPELESS}, {"DXGI_FORMAT_R32G8X24_TYPELESS", true, true, true, false, DXGI_FORMAT_R32G8X24_TYPELESS, {}, {}, {}, {}, {}}}, 
        {{DXGI_FORMAT_D32_FLOAT_S8X24_UINT}, {"DXGI_FORMAT_D32_FLOAT_S8X24_UINT", false, true, false, false, DXGI_FORMAT_R32G8X24_TYPELESS, {XRC_DEPTH_TEXTURE_USAGE}, XRC_DEPTH_CREATE_FLAGS, {}, {}, {}}},

        {{DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS}, {"DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS", true, true, true, false, DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS, {}, {}, {}, {}, {}}}, 
        {{DXGI_FORMAT_X32_TYPELESS_G8X24_UINT}, {"DXGI_FORMAT_X32_TYPELESS_G8X24_UINT", false, true, true, false, DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{DXGI_FORMAT_R10G10B10A2_TYPELESS}, {"DXGI_FORMAT_R10G10B10A2_TYPELESS", true, true, true, false, DXGI_FORMAT_R10G10B10A2_TYPELESS, {}, {}, {}, {}, {}}}, 
        {{DXGI_FORMAT_R10G10B10A2_UNORM}, {"DXGI_FORMAT_R10G10B10A2_UNORM", false, true, true, false, DXGI_FORMAT_R10G10B10A2_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_R10G10B10A2_UINT}, {"DXGI_FORMAT_R10G10B10A2_UINT", false, true, true, false, DXGI_FORMAT_R10G10B10A2_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        // This doesn't have a typeless equivalent, so it's created as-is by the runtime.
        {{DXGI_FORMAT_R11G11B10_FLOAT}, {"DXGI_FORMAT_R11G11B10_FLOAT", false, false, true, false, DXGI_FORMAT_R11G11B10_FLOAT, {XRC_COLOR_TEXTURE_USAGE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{DXGI_FORMAT_R8G8B8A8_TYPELESS}, {"DXGI_FORMAT_R8G8B8A8_TYPELESS", true, true, true, false, DXGI_FORMAT_R8G8B8A8_TYPELESS, {}, {}, {}, {}, {}}}, 
        {{DXGI_FORMAT_R8G8B8A8_UNORM}, {"DXGI_FORMAT_R8G8B8A8_UNORM", false, true, true, false, DXGI_FORMAT_R8G8B8A8_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_R8G8B8A8_UNORM_SRGB}, {"DXGI_FORMAT_R8G8B8A8_UNORM_SRGB", false, true, true, false, DXGI_FORMAT_R8G8B8A8_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_R8G8B8A8_UINT}, {"DXGI_FORMAT_R8G8B8A8_UINT", false, true, true, false, DXGI_FORMAT_R8G8B8A8_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}}, 
        {{DXGI_FORMAT_R8G8B8A8_SNORM}, {"DXGI_FORMAT_R8G8B8A8_SNORM", false, true, true, false, DXGI_FORMAT_R8G8B8A8_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_R8G8B8A8_SINT}, {"DXGI_FORMAT_R8G8B8A8_SINT", false, true, true, false, DXGI_FORMAT_R8G8B8A8_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{DXGI_FORMAT_R16G16_TYPELESS}, {"DXGI_FORMAT_R16G16_TYPELESS", true, true, true, false, DXGI_FORMAT_R16G16_TYPELESS, {}, {}, {}, {}, {}}}, 
        {{DXGI_FORMAT_R16G16_FLOAT}, {"DXGI_FORMAT_R16G16_FLOAT", false, true, true, false, DXGI_FORMAT_R16G16_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_R16G16_UNORM}, {"DXGI_FORMAT_R16G16_UNORM", false, true, true, false, DXGI_FORMAT_R16G16_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_R16G16_UINT}, {"DXGI_FORMAT_R16G16_UINT", false, true, true, false, DXGI_FORMAT_R16G16_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_R16G16_SNORM}, {"DXGI_FORMAT_R16G16_SNORM", false, true, true, false, DXGI_FORMAT_R16G16_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_R16G16_SINT}, {"DXGI_FORMAT_R16G16_SINT", false, true, true, false, DXGI_FORMAT_R16G16_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{DXGI_FORMAT_R32_TYPELESS}, {"DXGI_FORMAT_R32_TYPELESS", true, true, true, false, DXGI_FORMAT_R32_TYPELESS, {}, {}, {}, {}, {}}}, 
        {{DXGI_FORMAT_D32_FLOAT}, {"DXGI_FORMAT_D32_FLOAT", false, true, false, false, DXGI_FORMAT_R32_TYPELESS, {XRC_DEPTH_TEXTURE_USAGE}, XRC_DEPTH_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_R32_FLOAT}, {"DXGI_FORMAT_R32_FLOAT", false, true, true, false, DXGI_FORMAT_R32_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_R32_UINT}, {"DXGI_FORMAT_R32_UINT", false, true, true, false, DXGI_FORMAT_R32_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_R32_SINT}, {"DXGI_FORMAT_R32_SINT", false, true, true, false, DXGI_FORMAT_R32_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{DXGI_FORMAT_R24G8_TYPELESS}, {"DXGI_FORMAT_R24G8_TYPELESS", true, true, true, false, DXGI_FORMAT_R24G8_TYPELESS, {}, {}, {}, {}, {}}}, 
        {{DXGI_FORMAT_D24_UNORM_S8_UINT}, {"DXGI_FORMAT_D24_UNORM_S8_UINT", false, true, false, false, DXGI_FORMAT_R24G8_TYPELESS, {XRC_DEPTH_TEXTURE_USAGE}, XRC_DEPTH_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_R24_UNORM_X8_TYPELESS}, {"DXGI_FORMAT_R24_UNORM_X8_TYPELESS", true, true, true, false, DXGI_FORMAT_R24G8_TYPELESS, {}, {}, {}, {}, {}}}, 
        {{DXGI_FORMAT_X24_TYPELESS_G8_UINT}, {"DXGI_FORMAT_X24_TYPELESS_G8_UINT", true, true, true, false, DXGI_FORMAT_R24G8_TYPELESS, {}, {}, {}, {}, {}}}, 

        {{DXGI_FORMAT_R8G8_TYPELESS}, {"DXGI_FORMAT_R8G8_TYPELESS", true, true, true, false, DXGI_FORMAT_R8G8_TYPELESS, {}, {}, {}, {}, {}}}, 
        {{DXGI_FORMAT_R8G8_UNORM}, {"DXGI_FORMAT_R8G8_UNORM", false, true, true, false, DXGI_FORMAT_R8G8_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_R8G8_UINT}, {"DXGI_FORMAT_R8G8_UINT", false, true, true, false, DXGI_FORMAT_R8G8_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_R8G8_SNORM}, {"DXGI_FORMAT_R8G8_SNORM", false, true, true, false, DXGI_FORMAT_R8G8_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_R8G8_SINT}, {"DXGI_FORMAT_R8G8_SINT", false, true, true, false, DXGI_FORMAT_R8G8_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{DXGI_FORMAT_R16_TYPELESS}, {"DXGI_FORMAT_R16_TYPELESS", true, true, true, false, DXGI_FORMAT_R16_TYPELESS, {}, {}, {}, {}, {}}}, 
        {{DXGI_FORMAT_R16_FLOAT}, {"DXGI_FORMAT_R16_FLOAT", false, true, true, false, DXGI_FORMAT_R16_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_D16_UNORM}, {"DXGI_FORMAT_D16_UNORM", false, true, false, false, DXGI_FORMAT_R16_TYPELESS, {XRC_DEPTH_TEXTURE_USAGE}, XRC_DEPTH_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_R16_UNORM}, {"DXGI_FORMAT_R16_UNORM", false, true, true, false, DXGI_FORMAT_R16_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_R16_UINT}, {"DXGI_FORMAT_R16_UINT", false, true, true, false, DXGI_FORMAT_R16_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_R16_SNORM}, {"DXGI_FORMAT_R16_SNORM", false, true, true, false, DXGI_FORMAT_R16_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_R16_SINT}, {"DXGI_FORMAT_R16_SINT", false, true, true, false, DXGI_FORMAT_R16_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{DXGI_FORMAT_R8_TYPELESS}, {"DXGI_FORMAT_R8_TYPELESS", true, true, true, false, DXGI_FORMAT_R8_TYPELESS, {}, {}, {}, {}, {}}}, 
        {{DXGI_FORMAT_R8_UNORM}, {"DXGI_FORMAT_R8_UNORM", false, true, true, false, DXGI_FORMAT_R8_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_R8_UINT}, {"DXGI_FORMAT_R8_UINT", false, true, true, false, DXGI_FORMAT_R8_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_R8_SNORM}, {"DXGI_FORMAT_R8_SNORM", false, true, true, false, DXGI_FORMAT_R8_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_R8_SINT}, {"DXGI_FORMAT_R8_SINT", false, true, true, false, DXGI_FORMAT_R8_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_A8_UNORM}, {"DXGI_FORMAT_A8_UNORM", false, true, true, false, DXGI_FORMAT_R8_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        // These don't have typeless equivalents, so they are created as-is by the runtime.
        {{DXGI_FORMAT_R1_UNORM}, {"DXGI_FORMAT_R1_UNORM", false, true, true, false, DXGI_FORMAT_R1_UNORM, {XRC_COLOR_TEXTURE_USAGE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_R9G9B9E5_SHAREDEXP}, {"DXGI_FORMAT_R9G9B9E5_SHAREDEXP", false, false, true, false, DXGI_FORMAT_R9G9B9E5_SHAREDEXP, {XRC_COLOR_TEXTURE_USAGE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_R8G8_B8G8_UNORM}, {"DXGI_FORMAT_R8G8_B8G8_UNORM", false, false, true, false, DXGI_FORMAT_R8G8_B8G8_UNORM, {XRC_COLOR_TEXTURE_USAGE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_G8R8_G8B8_UNORM}, {"DXGI_FORMAT_G8R8_G8B8_UNORM", false, false, true, false, DXGI_FORMAT_G8R8_G8B8_UNORM, {XRC_COLOR_TEXTURE_USAGE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{DXGI_FORMAT_BC1_TYPELESS}, {"DXGI_FORMAT_BC1_TYPELESS", true, true, true, true, DXGI_FORMAT_BC1_TYPELESS, {}, {}, {}, {}, {}}}, 
        {{DXGI_FORMAT_BC1_UNORM}, {"DXGI_FORMAT_BC1_UNORM", false, true, true, true, DXGI_FORMAT_BC1_TYPELESS, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_BC1_UNORM_SRGB}, {"DXGI_FORMAT_BC1_UNORM_SRGB", false, true, true, true, DXGI_FORMAT_BC1_TYPELESS, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{DXGI_FORMAT_BC2_TYPELESS}, {"DXGI_FORMAT_BC2_TYPELESS", true, true, true, true, DXGI_FORMAT_BC2_TYPELESS, {}, {}, {}, {}, {}}}, 
        {{DXGI_FORMAT_BC2_UNORM}, {"DXGI_FORMAT_BC2_UNORM", false, true, true, true, DXGI_FORMAT_BC2_TYPELESS, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_BC2_UNORM_SRGB}, {"DXGI_FORMAT_BC2_UNORM_SRGB", false, true, true, true, DXGI_FORMAT_BC2_TYPELESS, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{DXGI_FORMAT_BC3_TYPELESS}, {"DXGI_FORMAT_BC3_TYPELESS", true, true, true, true, DXGI_FORMAT_BC3_TYPELESS, {}, {}, {}, {}, {}}}, 
        {{DXGI_FORMAT_BC3_UNORM}, {"DXGI_FORMAT_BC3_UNORM", false, true, true, true, DXGI_FORMAT_BC3_TYPELESS, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_BC3_UNORM_SRGB}, {"DXGI_FORMAT_BC3_UNORM_SRGB", false, true, true, true, DXGI_FORMAT_BC3_TYPELESS, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{DXGI_FORMAT_BC4_TYPELESS}, {"DXGI_FORMAT_BC4_TYPELESS", true, true, true, true, DXGI_FORMAT_BC4_TYPELESS, {}, {}, {}, {}, {}}}, 
        {{DXGI_FORMAT_BC4_UNORM}, {"DXGI_FORMAT_BC4_UNORM", false, true, true, true, DXGI_FORMAT_BC4_TYPELESS, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_BC4_SNORM}, {"DXGI_FORMAT_BC4_SNORM", false, true, true, true, DXGI_FORMAT_BC4_TYPELESS, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{DXGI_FORMAT_BC5_TYPELESS}, {"DXGI_FORMAT_BC5_TYPELESS", true, true, true, true, DXGI_FORMAT_BC5_TYPELESS, {}, {}, {}, {}, {}}}, 
        {{DXGI_FORMAT_BC5_UNORM}, {"DXGI_FORMAT_BC5_UNORM", false, true, true, true, DXGI_FORMAT_BC5_TYPELESS, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_BC5_SNORM}, {"DXGI_FORMAT_BC5_SNORM", false, true, true, true, DXGI_FORMAT_BC5_TYPELESS, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        // These don't have typeless equivalents, so they are created as-is by the runtime.
        {{DXGI_FORMAT_B5G6R5_UNORM}, {"DXGI_FORMAT_B5G6R5_UNORM", false, false, true, false, DXGI_FORMAT_B5G6R5_UNORM, {XRC_COLOR_TEXTURE_USAGE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_B5G5R5A1_UNORM}, {"DXGI_FORMAT_B5G5R5A1_UNORM", false, false, true, false, DXGI_FORMAT_B5G5R5A1_UNORM, {XRC_COLOR_TEXTURE_USAGE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM}, {"DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM", false, false, true, false, DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM, {XRC_COLOR_TEXTURE_USAGE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{DXGI_FORMAT_B8G8R8A8_TYPELESS}, {"DXGI_FORMAT_B8G8R8A8_TYPELESS", true, true, true, false, DXGI_FORMAT_B8G8R8A8_TYPELESS, {}, {}, {}, {}, {}}}, 
        {{DXGI_FORMAT_B8G8R8A8_UNORM}, {"DXGI_FORMAT_B8G8R8A8_UNORM", false, true, true, false, DXGI_FORMAT_B8G8R8A8_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_B8G8R8A8_UNORM_SRGB}, {"DXGI_FORMAT_B8G8R8A8_UNORM_SRGB", false, true, true, false, DXGI_FORMAT_B8G8R8A8_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{DXGI_FORMAT_B8G8R8X8_TYPELESS}, {"DXGI_FORMAT_B8G8R8X8_TYPELESS", true, true, true, false, DXGI_FORMAT_B8G8R8X8_TYPELESS, {}, {}, {}, {}, {}}}, 
        {{DXGI_FORMAT_B8G8R8X8_UNORM}, {"DXGI_FORMAT_B8G8R8X8_UNORM", false, true, true, false, DXGI_FORMAT_B8G8R8X8_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_B8G8R8X8_UNORM_SRGB}, {"DXGI_FORMAT_B8G8R8X8_UNORM_SRGB", false, true, true, false, DXGI_FORMAT_B8G8R8X8_TYPELESS, {XRC_COLOR_TEXTURE_USAGE, XRC_COLOR_TEXTURE_USAGE_MUTABLE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{DXGI_FORMAT_BC6H_TYPELESS}, {"DXGI_FORMAT_BC6H_TYPELESS", true, true, true, true, DXGI_FORMAT_BC6H_TYPELESS, {}, {}, {}, {}, {}}}, 
        {{DXGI_FORMAT_BC6H_UF16}, {"DXGI_FORMAT_BC6H_UF16", false, true, true, true, DXGI_FORMAT_BC6H_TYPELESS, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_BC6H_SF16}, {"DXGI_FORMAT_BC6H_SF16", false, true, true, true, DXGI_FORMAT_BC6H_TYPELESS, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        {{DXGI_FORMAT_BC7_TYPELESS}, {"DXGI_FORMAT_BC7_TYPELESS", true, true, true, true, DXGI_FORMAT_BC7_TYPELESS, {}, {}, {}, {}, {}}}, 
        {{DXGI_FORMAT_BC7_UNORM}, {"DXGI_FORMAT_BC7_UNORM", false, true, true, true, DXGI_FORMAT_BC7_TYPELESS, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},
        {{DXGI_FORMAT_BC7_UNORM_SRGB}, {"DXGI_FORMAT_BC7_UNORM_SRGB", false, true, true, true, DXGI_FORMAT_BC7_TYPELESS, {XRC_COLOR_TEXTURE_USAGE_COMPRESSED}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

        // This doesn't have a typeless equivalent, so it's created as-is by the runtime.
        {{DXGI_FORMAT_B4G4R4A4_UNORM}, {"DXGI_FORMAT_B4G4R4A4_UNORM", false, false, true, false, DXGI_FORMAT_B4G4R4A4_UNORM, {XRC_COLOR_TEXTURE_USAGE}, XRC_COLOR_CREATE_FLAGS, {}, {}, {}}},

    };
    // clang-format on

    SwapchainTestMap& GetDxgiSwapchainTestMap()
    {
        return dxgiSwapchainTestMap;
    }
}  // namespace Conformance

#endif
