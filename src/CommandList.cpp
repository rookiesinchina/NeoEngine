#include "CommandList.h"
#include "ResourceStateTracker.h"
#include "Resource.h"
#include "UploadBuffer.h"
#include "DynamicDescriptorHeap.h"
#include "d3dUtil.h"
#include "RootSignature.h"
#include "Application.h"
#include "Texture.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "GenerateMips.h"
#include "Model.h"
#include "Scene.h"
#include "Environment.h"
#include "DDSTextureLoader.h"
#include "FrustumCulling.h"
#include "GenerateSAT.h"
#include "Pass.h"

#include <filesystem>
#include <DirectXTex.h>

std::unordered_map<std::wstring, Texture*> CommandList::m_TextureResourceMap;
std::mutex CommandList::ms_TextureCacheMutex;


CommandList::CommandList(D3D12_COMMAND_LIST_TYPE Type)
    :m_d3d12GraphicsCommandList2(nullptr)
    , m_d3d12CommandListType(Type)
    , m_pResourceStateTracker(std::make_unique<ResourceStateTracker>())
    , m_pDynamicUploadBuffer(std::make_unique<UploadBuffer>())
    , m_d3d12RootSignature(nullptr)
    , m_d3d12PipelineState(nullptr)
{
    for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
    {
        m_pDynamicDescriptorHeap[i] = std::make_unique<DynamicDescriptorHeap>(static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(i));
        m_pCurrentDescriptorHeap[i] = nullptr;
    }

    auto device = Application::GetApp()->GetDevice();

    ThrowIfFailed(device->CreateCommandAllocator(Type, IID_PPV_ARGS(&m_d3d12CommandAlloctor)));
    ThrowIfFailed(device->CreateCommandList(0, Type, m_d3d12CommandAlloctor.Get(), nullptr, IID_PPV_ARGS(&m_d3d12GraphicsCommandList2)));
    //ThrowIfFailed(m_d3d12GraphicsCommandList2->Close());
    //Since we will use this commandlist immediately,so we do not Close() this commandlist.

    //here we just initialize compute commandlist to nullptr.
    m_pComputeCommandList = nullptr;
    m_pGenerateMips = nullptr;
}

CommandList::~CommandList()
{}


void CommandList::BarrierTransition(const Resource* pResource, D3D12_RESOURCE_STATES StateAfter, UINT subResource /* = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES */, bool IsFlushBarrier /* = false */)
{
    if (pResource)
    {
        m_pResourceStateTracker->TransitionResource(pResource, StateAfter, subResource);
    }

    if (IsFlushBarrier)
    {
        FlushResourceBarrier();
    }
}

void CommandList::BarrierAlias(const Resource* pResourceBefore, const Resource* pResourceAfter, bool IsFlushBarrier /* = false */)
{
    //Since ResourceStateTracker will check pResourceBefore and pResourceAfter,so we need not to check here. 
    m_pResourceStateTracker->AliasBarrier(pResourceBefore, pResourceAfter);

    if (IsFlushBarrier)
    {
        FlushResourceBarrier();
    }
}

void CommandList::BarrierUAV(const Resource* pResource, bool IsFlushBarrier /* = false */)
{
    //Since ResourceStateTracker will check pResource,so we need not to check here. 
    m_pResourceStateTracker->UavBarrier(pResource);

    if (IsFlushBarrier)
    {
        FlushResourceBarrier();
    }
}

void CommandList::FlushResourceBarrier()
{
    m_pResourceStateTracker->FlushValidResourceBarrier(*this);
}

void CommandList::AddObjectTracker(Microsoft::WRL::ComPtr<ID3D12Object> object)
{
    m_ObjectTracker.push_back(object);
}

void CommandList::AddResourceTracker(const Resource* pResource)
{
    if (pResource)
    {
        AddObjectTracker(pResource->GetD3D12Resource());
    }
}

void CommandList::ClearObjectTracker()
{
    m_ObjectTracker.clear();
}

void CommandList::CopyResource( Resource* pDstResource,const Resource* pSrcResource)
{
    if (pSrcResource && pDstResource)
    {
        //Before copy resource,we need to change resources to proper state.
        BarrierTransition(pSrcResource, D3D12_RESOURCE_STATE_COPY_SOURCE);
        BarrierTransition(pDstResource, D3D12_RESOURCE_STATE_COPY_DEST);
        //Make sure the resource has been changed to desired states
        FlushResourceBarrier();
        //then we start to copy
        m_d3d12GraphicsCommandList2->CopyResource(pDstResource->GetD3D12Resource().Get(), pSrcResource->GetD3D12Resource().Get());

        //add resources to tracker
        AddResourceTracker(pDstResource);
        AddResourceTracker(pSrcResource);
    }
}

void CommandList::CopyTextureSubResource(const Texture* pResource, UINT firstSubResource, UINT numSubResources, D3D12_SUBRESOURCE_DATA* pSubResourceData)
{
    if (pResource)
    {
        BarrierTransition(pResource, D3D12_RESOURCE_STATE_COPY_DEST);
        FlushResourceBarrier();

        auto dstResource = pResource->GetD3D12Resource();
        auto device = Application::GetApp()->GetDevice();

        Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource;

        UINT64 requiredBufferSize = GetRequiredIntermediateSize(dstResource.Get(), firstSubResource, numSubResources);

        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(requiredBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&intermediateResource)));

        ResourceStateTracker::AddGlobalResourceState(intermediateResource.Get(), D3D12_RESOURCE_STATE_GENERIC_READ);
        FlushResourceBarrier();
        
        UpdateSubresources(m_d3d12GraphicsCommandList2.Get(), dstResource.Get(), intermediateResource.Get(), 0, firstSubResource,
            numSubResources, pSubResourceData);

        AddObjectTracker(dstResource);
        AddObjectTracker(intermediateResource);
    }
}

void CommandList::ResolveSubResource(Resource* pDstResource, const Resource* pSrcResource, UINT DstSubResource /* = 0 */, UINT SrcSubResource /* = 0 */)
{
    BarrierTransition(pDstResource, D3D12_RESOURCE_STATE_RESOLVE_DEST, DstSubResource);
    BarrierTransition(pSrcResource, D3D12_RESOURCE_STATE_RESOLVE_SOURCE, SrcSubResource);

    FlushResourceBarrier();

    m_d3d12GraphicsCommandList2->ResolveSubresource(
        pDstResource->GetD3D12Resource().Get(), DstSubResource, pSrcResource->GetD3D12Resource().Get(), SrcSubResource, pDstResource->GetD3D12ResourceDesc().Format);

    AddResourceTracker(pDstResource);
    AddResourceTracker(pSrcResource);
}

void CommandList::SetGraphicsDynamicConstantBuffer(UINT rootParameterIndex,UINT SizeInByte , const void* pMappedData)
{
    if (pMappedData)
    {
        auto allocation = m_pDynamicUploadBuffer->Allocate(SizeInByte,D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

        ::memcpy(allocation.CPU, pMappedData, SizeInByte);

        m_d3d12GraphicsCommandList2->SetGraphicsRootConstantBufferView(rootParameterIndex, allocation.GPU);
    }
}

void CommandList::SetGraphicsStructuredBuffer(UINT rootParameterIndex, UINT numElements,UINT elementByteSize ,const void* pMappedData)
{
    if (pMappedData)
    {
        UINT bufferSize = numElements * elementByteSize;
        auto allocation = m_pDynamicUploadBuffer->Allocate(bufferSize, elementByteSize);

        memcpy(allocation.CPU, pMappedData, bufferSize);

        m_d3d12GraphicsCommandList2->SetGraphicsRootShaderResourceView(rootParameterIndex, allocation.GPU);
    }
}

void CommandList::SetGraphics32BitConstants(UINT rootParameterIndex, UINT num32BitValues, UINT offset32Bit, const void* pMapppedData)
{
    if (pMapppedData)
    {
        m_d3d12GraphicsCommandList2->SetGraphicsRoot32BitConstants(rootParameterIndex, num32BitValues, pMapppedData, offset32Bit);
    }
}

void CommandList::SetComputeDynamicConstantBuffer(UINT rootParameter, UINT sizeInByte, const void* pMappedData)
{
    if (pMappedData)
    {
        auto allocation = m_pDynamicUploadBuffer->Allocate(sizeInByte, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

        memcpy(allocation.CPU, pMappedData, sizeInByte);

        m_d3d12GraphicsCommandList2->SetComputeRootConstantBufferView(rootParameter, allocation.GPU);
    }
}

void CommandList::SetComputeStructuredBuffer(UINT rootParameter, UINT numElements, UINT elementSizeInByte, const void* pElementMappedData)
{
    if (pElementMappedData)
    {
        auto bufferSize = numElements * elementSizeInByte;
        auto allocation = m_pDynamicUploadBuffer->Allocate(bufferSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

        memcpy(allocation.CPU, pElementMappedData, bufferSize);

        m_d3d12GraphicsCommandList2->SetComputeRootShaderResourceView(rootParameter, allocation.GPU);
    }
}

void CommandList::SetCompute32BitConstants(UINT rootParameterIndex, UINT num32BitConstants, UINT destOffset32Bit, const void* pConstant)
{
    if (pConstant)
    {
        m_d3d12GraphicsCommandList2->SetComputeRoot32BitConstants(rootParameterIndex, num32BitConstants, pConstant, destOffset32Bit);
    }
}

void CommandList::SetShaderResourceView(
    UINT rootParameterIndex,
    UINT offsetInTable,
    const Resource* pResource,
    D3D12_RESOURCE_STATES stateAfter,
    UINT firstSubResource,
    UINT numSubResource,
    const D3D12_SHADER_RESOURCE_VIEW_DESC* pSrvDesc)
{
    if (pResource)
    {
        if (numSubResource < D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
        {
            for (UINT i = 0; i < numSubResource; ++i)
            {
                BarrierTransition(pResource, stateAfter, firstSubResource + i);
            }
        }
        else
        {
            BarrierTransition(pResource, stateAfter);
        }

        m_pDynamicDescriptorHeap[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->StageDescriptors(
            pResource->GetShaderResourceView(pSrvDesc),
            rootParameterIndex,
            offsetInTable,
            1);
    }
    AddResourceTracker(pResource);
}

void CommandList::SetUnorderedAccessView(
    UINT rootParameterIndex,
    UINT offstInTable,
    const Resource* pResource,
    D3D12_RESOURCE_STATES stateAfter,
    UINT firstSubResource /* = 0 */,
    UINT subResource /* = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES */,
    const D3D12_UNORDERED_ACCESS_VIEW_DESC* pUavDesc /* = nullptr */)
{
    if (pResource)
    {
        if (subResource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
        {
            BarrierTransition(pResource, stateAfter);
        }
        else
        {
            for (UINT i = 0; i < subResource; ++i)
            {
                BarrierTransition(pResource, stateAfter, firstSubResource + i);
            }
        }

        m_pDynamicDescriptorHeap[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->StageDescriptors(
            pResource->GetUnorderedAccessView(pUavDesc),
            rootParameterIndex,
            offstInTable,
            1);
    }
}

void CommandList::SetSamplers( UINT rootParameterIndex,UINT offsetInTable ,const D3D12_SAMPLER_DESC* pSamplerDesc)
{
    if (pSamplerDesc)
    {
        auto allocation = Application::GetApp()->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 1);
        Application::GetApp()->GetDevice()->CreateSampler(pSamplerDesc, allocation.GetDescriptorHandle());

        m_pDynamicDescriptorHeap[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER]->StageDescriptors(allocation.GetDescriptorHandle(), rootParameterIndex, offsetInTable, 1);
    }
}

void CommandList::Draw(UINT vertexCount,UINT instanceCount,UINT startVertexLocation,UINT startInstanceLocation)
{
    //Before draw,we must flush resource barrier 
    FlushResourceBarrier();
    //Bound all descirptor to commandlist
    for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
    {
        m_pDynamicDescriptorHeap[i]->CommittedStagedDescriptorsForDraw(*this);
    }
    //Draw mesh
    m_d3d12GraphicsCommandList2->DrawInstanced(vertexCount, instanceCount, startVertexLocation, startInstanceLocation);
}

void CommandList::DrawIndexed(UINT indexCount, UINT instanceCount, UINT startIndexLocation, UINT baseVertexLocation, UINT startInstanceLocation)
{
    //Before drawing,we must flush resource barriers.
    FlushResourceBarrier();
    //Bound all descriptors to commandlist
    for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
    {
        m_pDynamicDescriptorHeap[i]->CommittedStagedDescriptorsForDraw(*this);
    }
    m_d3d12GraphicsCommandList2->DrawIndexedInstanced(indexCount, instanceCount, startIndexLocation, baseVertexLocation, startInstanceLocation);
}

void CommandList::Dispatch(UINT NumX, UINT NumY, UINT NumZ)
{
    //Before dispath ,we need to flush barriers firstly.
    FlushResourceBarrier();

    for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
    {
        m_pDynamicDescriptorHeap[i]->CommittedStagedDescriptorsForDispatch(*this);
    }
    m_d3d12GraphicsCommandList2->Dispatch(NumX, NumY, NumZ);
}

void CommandList::SetGraphicsRootSignature(const RootSignature* pRootSignature)
{
    if (pRootSignature)
    {
        auto rootSignature = pRootSignature->GetRootSignature();
        if (rootSignature != m_d3d12RootSignature)
        {
            m_d3d12RootSignature = rootSignature;

            for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
            {
                m_pDynamicDescriptorHeap[i]->ParseRootSignature(*pRootSignature);
            }

            m_d3d12GraphicsCommandList2->SetGraphicsRootSignature(pRootSignature->GetRootSignature().Get());

            AddObjectTracker(m_d3d12RootSignature);
        }
    }
}

void CommandList::SetComputeRootSignature(const RootSignature* pComputeRootSignature)
{
    if (pComputeRootSignature)
    {
        auto rootSignature = pComputeRootSignature->GetRootSignature();
        if (rootSignature != m_d3d12RootSignature)
        {
            m_d3d12RootSignature = rootSignature;

            for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
            {
                m_pDynamicDescriptorHeap[i]->ParseRootSignature(*pComputeRootSignature);
            }

            m_d3d12GraphicsCommandList2->SetComputeRootSignature(pComputeRootSignature->GetRootSignature().Get());

            AddObjectTracker(m_d3d12RootSignature);
        }
    }
}

void CommandList::SetD3D12ViewPort(const D3D12_VIEWPORT* pViewPort)
{
    if (pViewPort)
    {
        m_d3d12GraphicsCommandList2->RSSetViewports(1, pViewPort);
    }
}

void CommandList::SetD3D12ScissorRect(const RECT* pScissorRect)
{
    if (pScissorRect)
    {
        m_d3d12GraphicsCommandList2->RSSetScissorRects(1, pScissorRect);
    }
}

void CommandList::SetD3D12PipelineState(Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState)
{
    if (pipelineState)
    {
        if (m_d3d12PipelineState != pipelineState)
        {
            m_d3d12PipelineState = pipelineState;
            m_d3d12GraphicsCommandList2->SetPipelineState(m_d3d12PipelineState.Get());
        }
        AddObjectTracker(pipelineState);
    }
}

void CommandList::ClearRenderTargetTexture(const Texture* pTexture, const float ClearColor[4],bool isFlushBarrier)
{
    if (pTexture)
    {
        //Do not forget to transition render target state.
        BarrierTransition(pTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, isFlushBarrier);

        m_d3d12GraphicsCommandList2->ClearRenderTargetView(pTexture->GetRenderTargetView(), ClearColor, 0, nullptr);

        AddResourceTracker(pTexture);
    }
}

void CommandList::ClearDepthStencilTexture(const Texture* pTexture, D3D12_CLEAR_FLAGS Flags, float depth, UINT stencil)
{
    if (pTexture)
    {
        //Do not forget to transition depth stencil state.
        BarrierTransition(pTexture, D3D12_RESOURCE_STATE_DEPTH_WRITE);

        m_d3d12GraphicsCommandList2->ClearDepthStencilView(pTexture->GetDepthStencilView(), Flags, depth, stencil, 0, nullptr);

        AddResourceTracker(pTexture);
    }
}

void CommandList::ClearRenderTarget(const RenderTarget* pRenderTarget)
{
    //Clear render targets
    for (int i = 0; i < (int)AttachmentPoint::DepthStencil; ++i)
    {
        auto texture = pRenderTarget->GetTexture(static_cast<AttachmentPoint>(i));
        if (texture.IsValidResource())
        {
            ClearRenderTargetTexture(&texture, texture.GetClearValue()->Color);
        }
    }
    //Clear depth stencil
    auto depth = pRenderTarget->GetTexture(AttachmentPoint::DepthStencil);
    if (depth.IsValidResource())
    {
        ClearDepthStencilTexture(&depth, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL);
    }
}

void CommandList::LoadTextureFromFile(Texture* pTexture, const std::wstring& filename, TextureUsage textureUsage,bool IsCubeMap)
{
    if (pTexture)
    {
        //Check if this texture has been loaded.
        //auto iter = m_TextureResourceMap.find(filename);
        //if (iter != m_TextureResourceMap.end())
        //{
        //    pTexture = iter->second;
        //    pTexture->SetTextureUsage(textureUsage);
        //    pTexture->SetName(filename);
        //}
        //else
        if(!IsCubeMap)
        {
            std::filesystem::path filepath(filename);
            if (!std::filesystem::exists(filepath))
            {
                throw std::exception("This texture can not be found under this file load.");
            }

            DirectX::TexMetadata metadata;
            DirectX::ScratchImage scratchImage;
            if (filepath.extension() == ".dds")
            {
                ThrowIfFailed(DirectX::LoadFromDDSFile(filename.c_str(), DirectX::DDS_FLAGS_FORCE_RGB, &metadata, scratchImage));
            }
            else if (filepath.extension() == ".tga")
            {
                ThrowIfFailed(DirectX::LoadFromTGAFile(filename.c_str(), &metadata, scratchImage));
            }
            else if (filepath.extension() == ".hdr")
            {
                ThrowIfFailed(DirectX::LoadFromHDRFile(filename.c_str(), &metadata, scratchImage));
            }
            else
            {
                ThrowIfFailed(DirectX::LoadFromWICFile(filename.c_str(), DirectX::WIC_FLAGS_FORCE_RGB, &metadata, scratchImage));
            }

            DXGI_FORMAT format = metadata.format;
            if (textureUsage == TextureUsage::Diffuse)
            {
                format = DirectX::MakeSRGB(format);
            }
            D3D12_RESOURCE_DESC texDesc = {};

            switch (metadata.dimension)
            {
            case DirectX::TEX_DIMENSION_TEXTURE1D:
                texDesc = CD3DX12_RESOURCE_DESC::Tex1D(format, (UINT64)metadata.width, (UINT16)metadata.arraySize);
                break;
            case DirectX::TEX_DIMENSION_TEXTURE2D:
                texDesc = CD3DX12_RESOURCE_DESC::Tex2D(format, (UINT64)metadata.width, (UINT)metadata.height, (UINT16)metadata.arraySize);
                break;
            case DirectX::TEX_DIMENSION_TEXTURE3D:
                texDesc = CD3DX12_RESOURCE_DESC::Tex3D(format, (UINT64)metadata.width, (UINT)metadata.height, (UINT16)metadata.depth);
                break;
            }

            Microsoft::WRL::ComPtr<ID3D12Resource> textureResource;
            auto device = Application::GetApp()->GetDevice();

            ThrowIfFailed(device->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                D3D12_HEAP_FLAG_NONE,
                &texDesc,
                D3D12_RESOURCE_STATE_COMMON,
                nullptr,
                IID_PPV_ARGS(&textureResource)));
            //Add new d3d12 resource to global resource state tracker for managing resource state.
            ResourceStateTracker::AddGlobalResourceState(textureResource.Get(), D3D12_RESOURCE_STATE_COMMON);
            //set texture configuration information
            pTexture->SetD3D12Resource(textureResource);
            pTexture->SetName(filename);
            pTexture->SetTextureUsage(textureUsage);
            //set subresourcedata vector of this texture.
            std::vector<D3D12_SUBRESOURCE_DATA> subResourceData(scratchImage.GetImageCount());
            const DirectX::Image* image = scratchImage.GetImages();
            for (size_t i = 0; i < subResourceData.size(); ++i)
            {
                subResourceData[i].pData = image[i].pixels;
                subResourceData[i].RowPitch = image[i].rowPitch;
                subResourceData[i].SlicePitch = image[i].slicePitch;
            }
            //Copy pixel information to Texture default heap.
            CopyTextureSubResource(pTexture, 0, (UINT)subResourceData.size(), subResourceData.data());
            //Here we need to check if generate mipmaps for this texture
            if (subResourceData.size() < textureResource->GetDesc().MipLevels)
            {
                GenerateMipMaps(pTexture);
            }
            //Update global texture map
            m_TextureResourceMap[filename] = pTexture;
            //-----------------------------------------
            std::lock_guard<std::mutex> lock(ms_TextureCacheMutex);
            //------------------------------------------
        }
        else
        {
            Microsoft::WRL::ComPtr<ID3D12Resource> textureResource;
            Microsoft::WRL::ComPtr<ID3D12Resource> uploadResource;

            ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(Application::GetApp()->GetDevice().Get(),
                m_d3d12GraphicsCommandList2.Get(), filename.c_str(), textureResource, uploadResource));

            pTexture->SetD3D12Resource(textureResource);
            pTexture->SetName(filename);
            pTexture->SetTextureUsage(textureUsage);

            AddObjectTracker(textureResource);
            AddObjectTracker(uploadResource);
        }
    }
}

void CommandList::SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, ID3D12DescriptorHeap* descriptorHeap)
{
    if (m_pCurrentDescriptorHeap[heapType] != descriptorHeap)
    {
        m_pCurrentDescriptorHeap[heapType] = descriptorHeap;
        //then we reset descriptor heap
        ID3D12DescriptorHeap* heap[] = { descriptorHeap };

        m_d3d12GraphicsCommandList2->SetDescriptorHeaps(_countof(heap), heap);
    }
}

void CommandList::CopyBuffer(Buffer* buffer, UINT bufferSize, UINT elementByteSize, const void* pBufferData)
{
    if (buffer && pBufferData)
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> defaultBuffer;
        Microsoft::WRL::ComPtr<ID3D12Resource> intermediateBuffer;
        auto device = Application::GetApp()->GetDevice();
        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(bufferSize * elementByteSize),
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&defaultBuffer)));
        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(bufferSize * elementByteSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&intermediateBuffer)));

        ResourceStateTracker::AddGlobalResourceState(defaultBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST);
        ResourceStateTracker::AddGlobalResourceState(intermediateBuffer.Get(), D3D12_RESOURCE_STATE_GENERIC_READ);
        FlushResourceBarrier();

        D3D12_SUBRESOURCE_DATA subResource = { };
        subResource.pData = pBufferData;
        subResource.RowPitch = bufferSize * elementByteSize;
        subResource.SlicePitch = subResource.RowPitch;

        UpdateSubresources(m_d3d12GraphicsCommandList2.Get(), defaultBuffer.Get(), intermediateBuffer.Get(), 0, 0, 1, &subResource);

        AddObjectTracker(defaultBuffer);
        AddObjectTracker(intermediateBuffer);

        buffer->SetD3D12Resource(defaultBuffer,nullptr);
        buffer->CreateView(bufferSize, elementByteSize);
    }
}

void CommandList::CopyVertexBuffer(VertexBuffer* pVertexBuffer,UINT numVertice,UINT strideByteSize,const void* pVertexData)
{
    CopyBuffer(pVertexBuffer, numVertice, strideByteSize, pVertexData);
}

void CommandList::CopyIndexBuffer(IndexBuffer* pIndexBuffer, UINT numIndice, DXGI_FORMAT format, const void* pIndexData)
{
    UINT stride = (format == DXGI_FORMAT_R16_UINT) ? 2 : 4;
    CopyBuffer(pIndexBuffer, numIndice, stride, pIndexData);
}

void CommandList::SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology)
{
    m_d3d12GraphicsCommandList2->IASetPrimitiveTopology(PrimitiveTopology);
}

void CommandList::SetVertexBuffer(UINT slot,const VertexBuffer* pVertexBuffer)
{
    if (pVertexBuffer)
    {
        //Firstly,we need to convert vertex buffer to proper state
        BarrierTransition(pVertexBuffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

        m_d3d12GraphicsCommandList2->IASetVertexBuffers(slot, 1, &pVertexBuffer->GetVerterBufferView());

        AddResourceTracker(pVertexBuffer);
    }
}

void CommandList::SetIndexBuffer(const IndexBuffer* pIndexBuffer)
{
    if (pIndexBuffer)
    {
        BarrierTransition(pIndexBuffer, D3D12_RESOURCE_STATE_INDEX_BUFFER);

        m_d3d12GraphicsCommandList2->IASetIndexBuffer(&pIndexBuffer->GetIndexBufferView());

        AddResourceTracker(pIndexBuffer);
    }
}

bool CommandList::Close(CommandList& pendingCommandList)
{
    FlushResourceBarrier();
    ThrowIfFailed(m_d3d12GraphicsCommandList2->Close());

    UINT numPendingBarrier = m_pResourceStateTracker->FlushPendingResourceBarrier(pendingCommandList);

    //Remember to commit final resource state to Global resource state.
    m_pResourceStateTracker->CommitFinalResourceState();

    return numPendingBarrier > 0;
}

void CommandList::Close()
{
    FlushResourceBarrier();
    ThrowIfFailed(m_d3d12GraphicsCommandList2->Close());
}

void CommandList::Reset()
{
    ThrowIfFailed(m_d3d12CommandAlloctor->Reset());
    ThrowIfFailed(m_d3d12GraphicsCommandList2->Reset(m_d3d12CommandAlloctor.Get(), nullptr));
    m_pResourceStateTracker->Reset();
    m_pDynamicUploadBuffer->Reset();

    for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
    {
        m_pDynamicDescriptorHeap[i]->Reset();
        m_pCurrentDescriptorHeap[i] = nullptr;
    }

    ClearObjectTracker();

    m_d3d12PipelineState.Reset();
    m_d3d12RootSignature.Reset();

    m_pComputeCommandList.reset();
    m_pGenerateMips.reset();
}

void CommandList::SetRenderTargets(const RenderTarget& RenderTargets)
{
    auto rtvFormats = RenderTargets.GetRenderTargetFormats();
    UINT numRtv = rtvFormats.NumRenderTargets;

    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> hRtv;

    for (int i = AttachmentPoint::Color0; i <= AttachmentPoint::Color7; ++i)
    {
        const Texture texture = RenderTargets.GetTexture(static_cast<AttachmentPoint>(i));
        if (texture.IsValidResource())
        {
            BarrierTransition(&texture, D3D12_RESOURCE_STATE_RENDER_TARGET);
            hRtv.push_back(texture.GetRenderTargetView());

            AddResourceTracker(&texture);
        }
    }
    assert(numRtv == hRtv.size() && "Render Target Numbers Mismatching");

    auto depth = RenderTargets.GetTexture(DepthStencil);
    CD3DX12_CPU_DESCRIPTOR_HANDLE hDsv(D3D12_DEFAULT);
    if (depth.IsValidResource())
    {
        BarrierTransition(&depth, D3D12_RESOURCE_STATE_DEPTH_WRITE);
        hDsv = depth.GetDepthStencilView();

        AddResourceTracker(&depth);
    }
    CD3DX12_CPU_DESCRIPTOR_HANDLE* p_hDsv = (hDsv.ptr != 0) ? &hDsv : nullptr;

    m_d3d12GraphicsCommandList2->OMSetRenderTargets(numRtv, hRtv.data(), FALSE, p_hDsv);
}

void CommandList::GenerateMipMaps(Texture* pTexture)
{
    if (pTexture)
    {
        if (m_d3d12CommandListType == D3D12_COMMAND_LIST_TYPE_COPY)
        {
            if (!m_pComputeCommandList)
            {
                m_pComputeCommandList = std::make_shared<CommandList>(D3D12_COMMAND_LIST_TYPE_COMPUTE);
            }
            m_pComputeCommandList->GenerateMipMaps(pTexture);
            return;
        }

        auto Desc = pTexture->GetD3D12ResourceDesc();
        //This texture only has one mips,we have no need to do anything.
        if (Desc.MipLevels == 1)
        {
            return;
        }
        if (Desc.MipLevels == 0)
        {
            assert(FALSE && "Texture Mips can not be ZERO");
            return;
        }
        if (Desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D)
        {
            assert(FALSE && "Only 2D texture can generate Mips");
            return;
        }
        if (Desc.DepthOrArraySize != 1)
        {
            //assert(FALSE && "Texture Array or 3D texture can not generate Mips");
            return;
        }
        if (Desc.SampleDesc.Count > 1)
        {
            assert(FALSE && "Multi-sampled Texture can not generate Mips");
            return;
        }
        //we need to check this format support state in this device.
        auto device = Application::GetApp()->GetDevice();
        D3D12_FEATURE_DATA_D3D12_OPTIONS options = {};
        ThrowIfFailed(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options)));

        if (Texture::IsUavCompatibleFormat(Desc.Format))
        {
            GenerateMips_UAV(pTexture);
        }
        else if (Texture::IsBGRFormat(Desc.Format))
        {
            //For BGR format texture, we need to check if current device support to convert other format.
            if (!options.StandardSwizzle64KBSupported)
            {
                assert(FALSE && "Generating BGR format Mips needs Standard Swizzle 64KB Support");
            }
            GenerateMips_BGR(pTexture);
        }
        else if (Texture::IsSRGBFormat(Desc.Format))
        {
            GenerateMips_SRGB(pTexture);
        }
        else
        {
            assert(FALSE && "This texture format can not generate Mips");
        }
    }
}

void CommandList::GenerateMips_UAV(Texture* pTexture)
{
    auto textureDesc = pTexture->GetD3D12ResourceDesc();
    auto device = Application::GetApp()->GetDevice();

    Texture stagingTexture(*pTexture);
    //If this texture does not allow unordered access,we need to recreate a copy resource to generate mips.
    if ((textureDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) == 0)
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> stageResource;
        auto stageDesc = textureDesc;
        stageDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        //since this new texture could never be render target or depth stencil resource,so we cancel these bit mask to optimize.
        stageDesc.Flags &= ~(D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

        ThrowIfFailed(device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &stageDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&stageResource)));

        ResourceStateTracker::AddGlobalResourceState(stageResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST);

        stagingTexture.SetD3D12Resource(stageResource);
        stagingTexture.SetName(L"staging texture for Mips");

        CopyResource(&stagingTexture, pTexture);
    }
    AddResourceTracker(&stagingTexture);
    //-------------------------------------------
    if (!m_pGenerateMips)
    {
        m_pGenerateMips = std::make_unique<GenerateMips>();
    }

    GenerateMipsCB MipsCb;
    //Set pipeline state and root signature.
    SetD3D12PipelineState(m_pGenerateMips->GetPipelineState());
    SetComputeRootSignature(m_pGenerateMips->GetRootSignature());

    auto stageDesc = stagingTexture.GetD3D12ResourceDesc();
    for (UINT SrcMip = 0; SrcMip < stageDesc.MipLevels - 1;)
    {
        UINT SrcWidth = stageDesc.Width >> SrcMip;
        UINT SrcHeight = stageDesc.Height >> SrcMip;
        UINT DstWidth = SrcWidth >> 1;
        UINT DstHeight = SrcHeight >> 1;

        //Use bitmask to represent width and height situtation.
        //0b00: width / height is even
        //0b01: width is odd, height is even
        //0b10: width is even,height is odd
        //0b11: width / height is odd
        MipsCb.SrcDimension = ((SrcWidth & 1) << 1) | (SrcHeight & 1);
        //Before dispath, we need to decide how many mips we can generate in this dispatch.
        DWORD mipCount = 0;
        //Use bitmask to compute how many times downsampling can occur before resulting in an odd dimension.
        //Note:we use dstHeight and dstWidth to compute,so final count should be plus 1(first generated mip).
        _BitScanForward(&mipCount, (DstWidth == 1 ? DstHeight : DstWidth) |
                                    (DstHeight == 1 ? DstWidth : DstHeight));
        //Since we just can generate four mips at most in one dispatch,so need to clamp.
        mipCount = std::min<DWORD>(4, mipCount + 1);
        //At the meantime,we generate mips count can not exceed texture MipLevel,so need to clamp again.
        mipCount = SrcMip + mipCount >= stageDesc.MipLevels ? stageDesc.MipLevels - SrcMip - 1 : mipCount;
        //And we also need to clamp dimension.When the source dimension is not same,this will happen.
        DstWidth = std::max<UINT>(1, DstWidth);
        DstHeight = std::max<UINT>(1, DstHeight);
        //Fill constant buffer.
        MipsCb.numMips = mipCount;
        MipsCb.SrcMipLevel = SrcMip;
        MipsCb.TexelSize.x = 1.0f / DstWidth;
        MipsCb.TexelSize.y = 1.0f / DstHeight;

        SetCompute32BitConstants(GenerateMipsRoot::GenerateMipsCB, 0, MipsCb);
        SetShaderResourceView(GenerateMipsRoot::Src, 0, &stagingTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, SrcMip, 1);

        for (UINT i = 0; i < mipCount; ++i)
        {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.Format = stageDesc.Format;
            uavDesc.Texture2D.MipSlice = SrcMip + i + 1;
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

            SetUnorderedAccessView(GenerateMipsRoot::Mips, i, &stagingTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, SrcMip + i + 1, 1, &uavDesc);
        }
        //If the mipCount is less than 4 ,we need to fill null uav to shader for unused resource,this way make DirectX12 happy.
        if (mipCount < 4)
        {
            m_pDynamicDescriptorHeap[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->StageDescriptors(m_pGenerateMips->GetDefaultUAV(), GenerateMipsRoot::Mips, mipCount, 4 - mipCount);
        }

        Dispatch(ceil(DstWidth / 8.0f), ceil(DstHeight / 8.0f), 1);
        //Since we need to read texture in shaders after generating mips,so we must set barrier for uav to make sure that
        //the write operation has finished.
        BarrierUAV(&stagingTexture,true);
        //At then end,we increment srcmip for next dispatch
        SrcMip += mipCount;
    }
    //If we create a new resource to generate mips , we need to copy back to source texture.
    if (pTexture->GetD3D12Resource() != stagingTexture.GetD3D12Resource())
    {
        CopyResource(pTexture, &stagingTexture);
    }
}

void CommandList::GenerateMips_BGR(Texture* pTexture)
{
    auto device = Application::GetApp()->GetDevice();
    auto textureDesc = pTexture->GetD3D12ResourceDesc();
    //Note:D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS can not set with D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
    //@see:https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_resource_flags
    auto uavDesc = textureDesc;
    uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    uavDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    uavDesc.Flags &= ~(D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
    //Since some BGR format may be SRGB,so before generate mips,if we set format without SRGB,this will apply gamma correction automatically.
    auto aliasDesc = uavDesc;
    aliasDesc.Format = ((aliasDesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM) || 
                        (aliasDesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)) ?
                        DXGI_FORMAT_B8G8R8A8_UNORM : DXGI_FORMAT_B8G8R8X8_UNORM;
    //Get heap info.
    //@see:https://learn.microsoft.com/zh-cn/windows/win32/api/d3d12/ns-d3d12-d3d12_heap_desc
    //@see:https://learn.microsoft.com/zh-cn/windows/win32/api/d3d12/ne-d3d12-d3d12_heap_flags
    D3D12_RESOURCE_DESC resDesc[] = { uavDesc,aliasDesc };
    auto allocationInfo = device->GetResourceAllocationInfo(0, _countof(resDesc), resDesc);
    //we need to create a heap
    Microsoft::WRL::ComPtr<ID3D12Heap> textureHeap;
    D3D12_HEAP_DESC heapDesc = {};
    heapDesc.Alignment = allocationInfo.Alignment;
    heapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES;
    heapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapDesc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
    
    ThrowIfFailed(device->CreateHeap(&heapDesc, IID_PPV_ARGS(&textureHeap)));
    //@see:https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createplacedresource
    Microsoft::WRL::ComPtr<ID3D12Resource> uavResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> aliasResource;

    ThrowIfFailed(device->CreatePlacedResource(
        textureHeap.Get(), 
        0, &uavDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&uavResource)));

    ThrowIfFailed(device->CreatePlacedResource(
        textureHeap.Get(),
        0, &aliasDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&aliasResource)));

    ResourceStateTracker::AddGlobalResourceState(uavResource.Get(), D3D12_RESOURCE_STATE_COMMON);
    ResourceStateTracker::AddGlobalResourceState(aliasResource.Get(), D3D12_RESOURCE_STATE_COMMON);

    Texture uavTexture(uavResource);
    Texture aliasTexture(aliasResource);

    //we need to copy texture to aliasTexture firstly.
    BarrierAlias(&Texture(), &aliasTexture);
    CopyResource(&aliasTexture, pTexture);
    //after copy,we need to use uavTexture to generate mips.
    //Note:here we utilize Data Inheritance in DirectX12,so after copy we can just use uavTexture to generate mips.
    //@see: https://learn.microsoft.com/en-us/windows/win32/direct3d12/memory-aliasing-and-data-inheritance
    BarrierAlias(&aliasTexture, &uavTexture);
    GenerateMips_UAV(&uavTexture);
    //after generate mips,we need copy back to pTexture
    BarrierAlias(&uavTexture, &aliasTexture);
    CopyResource(pTexture, &aliasTexture);

    //Since copy and generate mips opeartion will execute in commandlist,so we need to Tracker them to prevent releasing.
    //After command finished,we can safely release these resources.
    AddObjectTracker(textureHeap);
    AddResourceTracker(&uavTexture);
    AddResourceTracker(&aliasTexture);
    AddResourceTracker(pTexture);
}

void CommandList::GenerateMips_SRGB(Texture* pTexture)
{
    auto device = Application::GetApp()->GetDevice();

    auto textureDesc = pTexture->GetD3D12ResourceDesc();
    //Note:D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS can not set with D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
    //@see:https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_resource_flags
    auto uavDesc = textureDesc;
    uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    uavDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    uavDesc.Flags &= ~(D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
    //here we set format without SRGB to apply gamma correction automatically.
    auto aliasDesc = uavDesc;
    //@see:https://learn.microsoft.com/zh-cn/windows/win32/api/d3d12/ns-d3d12-d3d12_heap_desc
    //@see:https://learn.microsoft.com/zh-cn/windows/win32/api/d3d12/ne-d3d12-d3d12_heap_flags
    D3D12_RESOURCE_DESC resDesc[] = { uavDesc,aliasDesc };
    auto allocationInfo = device->GetResourceAllocationInfo(0, _countof(resDesc), resDesc);
    
    Microsoft::WRL::ComPtr<ID3D12Heap> texHeap;
    D3D12_HEAP_DESC heapDesc = {};
    heapDesc.Alignment = allocationInfo.Alignment;
    heapDesc.SizeInBytes = allocationInfo.SizeInBytes;
    heapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES;
    heapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapDesc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;

    ThrowIfFailed(device->CreateHeap(&heapDesc, IID_PPV_ARGS(&texHeap)));
    //@see:https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createplacedresource
    Microsoft::WRL::ComPtr<ID3D12Resource> uavResource;
    Microsoft::WRL::ComPtr<ID3D12Resource> aliasResource;

    ThrowIfFailed(device->CreatePlacedResource(texHeap.Get(), 0, &uavDesc, 
        D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&uavResource)));
    ThrowIfFailed(device->CreatePlacedResource(texHeap.Get(), 0, &aliasDesc,
        D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&aliasResource)));

    ResourceStateTracker::AddGlobalResourceState(uavResource.Get(), D3D12_RESOURCE_STATE_COMMON);
    ResourceStateTracker::AddGlobalResourceState(aliasResource.Get(), D3D12_RESOURCE_STATE_COMMON);
    
    Texture uavTexture(uavResource);
    Texture aliasTexture(aliasResource);
    //Before generate mips,we need to copy texture to alias texture
    BarrierAlias(&Texture(), &aliasTexture);
    CopyResource(&aliasTexture, pTexture);
    //After copy,we can start to generate mips
    //Note:here we utilize Data Inheritance in DirectX12,so after copy we can just use uavTexture to generate mips.
    //@see: https://learn.microsoft.com/en-us/windows/win32/direct3d12/memory-aliasing-and-data-inheritance
    BarrierAlias(&aliasTexture, &uavTexture);
    GenerateMips_UAV(&uavTexture);
    //Then,we can copy back to pTexture
    BarrierAlias(&uavTexture, &aliasTexture);
    CopyResource(pTexture, &aliasTexture);

    AddObjectTracker(texHeap);
    AddResourceTracker(&uavTexture);
    AddResourceTracker(&aliasTexture);
    AddResourceTracker(pTexture);
}

//void CommandList::DrawModel(const Model* pModel, std::function<void()> SetAdditonalResourceFunc /* = */ )
//{
//    if (pModel)
//    {
//        assert(pModel->m_pRootSignature && "RootSignature of this model has not been initilized!");
//        assert(pModel->m_d3d12PipelineState && "PipelineState of this model has not been initilized!");
//        assert(Scene::GetScene()->m_pPassConstant && "PassConstant of this model has not been initilized");
//        assert(Scene::GetScene()->m_pLightConstant && "LightConstant of this model has not been initilized");
//
//        auto meshes = pModel->m_ModelLoader->Meshes();
//
//        SetD3D12PipelineState(pModel->m_d3d12PipelineState);
//        SetGraphicsRootSignature(pModel->m_pRootSignature.get());
//
//        SetVertexBuffer(0, pModel->m_pVertexBuffer.get());
//        SetIndexBuffer(pModel->m_pIndexBuffer.get());
//
//        //Note:For variables which are for all meshes,we just need to bind at the beginning of this frame.
//        SetGraphicsDynamicConstantBuffer(SceneRootParameter::PassConstantCB, *(Scene::GetScene()->m_pPassConstant));
//        SetGraphicsDynamicConstantBuffer(SceneRootParameter::LightConstantCB, *(Scene::GetScene()->m_pLightConstant));
//        SetGraphicsStructuredBuffer(SceneRootParameter::StructuredMaterials, pModel->m_MeshMaterials);
//        for (int i = 0; i < TextureUsage::NumTextureUsage; ++i)
//        {
//            switch (static_cast<TextureUsage>(i))
//            {
//            case TextureUsage::Diffuse:
//            {
//                for (size_t i = 0; i < pModel->m_pTexture[TextureUsage::Diffuse].size(); ++i)
//                {
//                    SetShaderResourceView(SceneRootParameter::DiffuseTexture, i, pModel->m_pTexture[TextureUsage::Diffuse][i].get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
//                }
//                if (pModel->m_pTexture[TextureUsage::Diffuse].size() < Scene::m_MaxTextureNum)
//                {
//                    m_pDynamicDescriptorHeap[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->StageDescriptors(
//                        pModel->m_DefaultSRV[TextureUsage::Diffuse].GetDescriptorHandle(),
//                        SceneRootParameter::DiffuseTexture,
//                        pModel->m_pTexture[TextureUsage::Diffuse].size(),
//                        Scene::m_MaxTextureNum - pModel->m_pTexture[TextureUsage::Diffuse].size());
//                }
//            }
//            break;
//            case TextureUsage::Specular:
//            {
//                for (size_t i = 0; i < pModel->m_pTexture[TextureUsage::Specular].size(); ++i)
//                {
//                    SetShaderResourceView(SceneRootParameter::SpecularTexture, i, pModel->m_pTexture[TextureUsage::Specular][i].get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
//                }
//                if (pModel->m_pTexture[TextureUsage::Specular].size() < Scene::m_MaxTextureNum)
//                {
//                    m_pDynamicDescriptorHeap[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->StageDescriptors(
//                        pModel->m_DefaultSRV[TextureUsage::Specular].GetDescriptorHandle(),
//                        SceneRootParameter::SpecularTexture,
//                        pModel->m_pTexture[TextureUsage::Specular].size(),
//                        Scene::m_MaxTextureNum - pModel->m_pTexture[TextureUsage::Specular].size());
//                }
//            }
//            break;
//            case TextureUsage::HeightMap:
//            {
//                for (size_t i = 0; i < pModel->m_pTexture[TextureUsage::HeightMap].size(); ++i)
//                {
//                    SetShaderResourceView(SceneRootParameter::HeightTexture, i, pModel->m_pTexture[TextureUsage::HeightMap][i].get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
//                }
//                if (pModel->m_pTexture[TextureUsage::HeightMap].size() < Scene::m_MaxTextureNum)
//                {
//                    m_pDynamicDescriptorHeap[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->StageDescriptors(
//                        pModel->m_DefaultSRV[TextureUsage::HeightMap].GetDescriptorHandle(),
//                        SceneRootParameter::HeightTexture,
//                        pModel->m_pTexture[TextureUsage::HeightMap].size(),
//                        Scene::m_MaxTextureNum - pModel->m_pTexture[TextureUsage::HeightMap].size());
//                }
//            }
//            break;
//            case TextureUsage::NormalMap:
//            {
//                for (size_t i = 0; i < pModel->m_pTexture[TextureUsage::NormalMap].size(); ++i)
//                {
//                    SetShaderResourceView(SceneRootParameter::NormalTexture, i, pModel->m_pTexture[TextureUsage::NormalMap][i].get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
//                }
//                if (pModel->m_pTexture[TextureUsage::NormalMap].size() < Scene::m_MaxTextureNum)
//                {
//                    m_pDynamicDescriptorHeap[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->StageDescriptors(
//                        pModel->m_DefaultSRV[TextureUsage::NormalMap].GetDescriptorHandle(),
//                        SceneRootParameter::NormalTexture,
//                        pModel->m_pTexture[TextureUsage::NormalMap].size(),
//                        Scene::m_MaxTextureNum - pModel->m_pTexture[TextureUsage::NormalMap].size());
//                }
//            }
//            break;
//            default:
//                assert(FALSE && "Error");
//            }
//        }
//        //Since some rendering task need to add additional root parameter,so here we use insert a function for this.
//        SetAdditonalResourceFunc();
//        //After binding resources,we can begin to draw
//        for (size_t i = 0; i < meshes.size(); ++i)
//        {
//            //Check if this mesh is culled by frustum.
//            if (meshes[i].m_IsCulled == false)
//            {
//                //Bind resources to shader
//                SetGraphicsDynamicConstantBuffer(SceneRootParameter::MeshConstantCB, pModel->m_MeshConstants[i]);
//                DrawIndexed(meshes[i].mIndices.size(), 1, meshes[i].mIndexOffset, meshes[i].mVertexOffset, 0);
//            }
//        }
//    }
//}

void CommandList::RenderShadow(const ShadowBase* pShadow,UINT PassIndex)
{
    assert(pShadow && "Shadow has not been initialized!");

    SetD3D12PipelineState(pShadow->GetPipelineState());
    SetGraphicsRootSignature(pShadow->GetRootSignature());

    for (const auto& modelmap : Scene::GetScene()->m_SceneModelsMap)
    {
        auto model = modelmap.second.get();
        SetVertexBuffer(0, model->m_pVertexBuffer.get());
        SetIndexBuffer(model->m_pIndexBuffer.get());
        SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        SetGraphicsDynamicConstantBuffer(ShadowRootParameter::ShadowPassBuffer, pShadow->GetShadowPassBuffer(PassIndex));
        SetGraphicsStructuredBuffer(ShadowRootParameter::ShadowMaterialBuffer, model->m_MeshMaterials);
        for (size_t i = 0; i < model->m_pTexture[TextureUsage::Diffuse].size(); ++i)
        {
            SetShaderResourceView(ShadowRootParameter::ShadowAlphaTexture, i, model->m_pTexture[TextureUsage::Diffuse][i].get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }
        if (model->m_pTexture[TextureUsage::Diffuse].size() < m_MaxTextureNum)
        {
            m_pDynamicDescriptorHeap[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->StageDescriptors(
                model->m_DefaultSRV[TextureUsage::Diffuse].GetDescriptorHandle(),
                ShadowRootParameter::ShadowAlphaTexture,
                model->m_pTexture[TextureUsage::Diffuse].size(),
                m_MaxTextureNum - model->m_pTexture[TextureUsage::Diffuse].size());
        }
        //here we execute frustum culling.
        pShadow->GetFrustumCullinger()->BindModelCulled(model);
        
        auto meshes = model->m_ModelLoader->Meshes();
        for (size_t i = 0; i < meshes.size() ; ++i)
        {
            if (meshes[i].m_IsCulled == false)
            {
                SetGraphicsDynamicConstantBuffer(ShadowRootParameter::ShadowConstantBuffer, model->m_MeshConstants[i]);
                DrawIndexed(meshes[i].mIndices.size(), 1, meshes[i].mIndexOffset, meshes[i].mVertexOffset, 0);
            }
        }
    }
}

void CommandList::SetDynamicVertexBuffer(UINT slot,UINT vertexCount, UINT vertexByteSize,const void* pVertexData)
{
    if (pVertexData)
    {
        auto byteSize = vertexCount * vertexByteSize;

        auto allocation = m_pDynamicUploadBuffer->Allocate(byteSize, vertexByteSize);

        memcpy(allocation.CPU, pVertexData, byteSize);

        D3D12_VERTEX_BUFFER_VIEW vbv = {};
        vbv.BufferLocation = allocation.GPU;
        vbv.SizeInBytes = byteSize;
        vbv.StrideInBytes = vertexByteSize;

        m_d3d12GraphicsCommandList2->IASetVertexBuffers(slot, 1, &vbv);
    }
}

void CommandList::SetDynamicIndexBuffer(UINT indexCount, DXGI_FORMAT indexFormat, const void* pIndexData)
{
    if (pIndexData)
    {
        assert(indexFormat == DXGI_FORMAT_R16_UINT || indexFormat == DXGI_FORMAT_R32_UINT);
        UINT indexByteSize = indexFormat == DXGI_FORMAT_R16_UINT ? 2 : 4;

        auto byteSize = indexCount * indexByteSize;
        
        auto allocation = m_pDynamicUploadBuffer->Allocate(byteSize, indexByteSize);

        memcpy(allocation.CPU, pIndexData, byteSize);

        D3D12_INDEX_BUFFER_VIEW ibv = {};
        ibv.BufferLocation = allocation.GPU;
        ibv.Format = indexFormat;
        ibv.SizeInBytes = byteSize;

        m_d3d12GraphicsCommandList2->IASetIndexBuffer(&ibv);
    }
}