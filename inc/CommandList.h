#pragma once

#include "d3dUtil.h"
#include "ResourceStateTracker.h"
#include "RenderTarget.h"
#include "d3dx12.h"
#include "Scene.h"
#include "Environment.h"
#include <wrl.h>
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>



class UploadBuffer;
class DynamicDescriptorHeap;
class Resource;
class RootSignature;
class Texture;
class VertexBuffer;
class IndexBuffer;
class Buffer;
class GenerateMips;
class Model;
class ShadowBase;
class GenerateSAT;

class CommandList
{
public:
    explicit CommandList(D3D12_COMMAND_LIST_TYPE Type);
    ~CommandList();

    D3D12_COMMAND_LIST_TYPE GetCommandListType()const { return m_d3d12CommandListType; };

    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> GetGraphicsCommandList2()const { return m_d3d12GraphicsCommandList2; };
    /**
     * Transiton a resource to proper state before executing command
     */
    void BarrierTransition(const Resource* pResource, D3D12_RESOURCE_STATES StateAfter,UINT subResource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, bool IsFlushBarrier = false);
    /**
     * A barrier for alias,this is useful for ID3D12Device::CreatePlacedResource() or ID3D12Device::CreateReservedResource()
     * @param: pResourceBefore and pResourceAfter can both be nullptr,which means any placed or reserved resource could cause alisaing.
     * @see: CreatPlacedResource()https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createplacedresource.
     * @see: CreatReservedResource()https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createreservedresource.
     * @see: BarrierAlias https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_resource_aliasing_barrier
     */
    void BarrierAlias(const Resource* pResourceBefore, const Resource* pResourceAfter, bool IsFlushBarrier = false);
    /**
     * A barrier for UAV resource.Since Uav resource can be writed and read,we need a barrier to prevent write or read at same time.
     * Note: If we have two thread with one UAV resource,when both threads read from this UAV resource,then we do not need barrier.
     * Or if we know that two threads will write at different time,then we also do not need barrier.
     * And you can set pResource is NULL which indicates that any UAV access could require the barrier.
     * @see: https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_resource_uav_barrier
     */
    void BarrierUAV(const Resource* pResource, bool IsFlushBarrier = false);
    /**
     * Copy a vertex buffer in CPU to GPU default heap.
     * This can be used in the vertex data will not be changed after committing to the GPU.
     */
    void CopyVertexBuffer(VertexBuffer* pVertexBuffer,UINT numVertice,UINT strideByteSize,const void* pVertexData);
    template<class T>
    void CopyVertexBuffer(VertexBuffer* pVertexBuffer, const std::vector<T>& Vertice)
    {
        CopyVertexBuffer(pVertexBuffer, Vertice.size(), sizeof(T), Vertice.data());
    }
    /**
     * Copy a index buffer in CPU to GPU default heap.
     * This can be used in the index data will not be changed after committing to the GPU.
     */
    void CopyIndexBuffer(IndexBuffer* pIndexBuffer, UINT numIndice, DXGI_FORMAT format, const void* pIndexData);
    template<class T>
    void CopyIndexBuffer(IndexBuffer* pIndexBuffer ,const std::vector<T>& Indice)
    {
        assert((sizeof(T) == 2 || sizeof(T) == 4) && "Invalid index format");

        DXGI_FORMAT indexformat = (sizeof(T) == 2) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

        CopyIndexBuffer(pIndexBuffer, Indice.size(), indexformat, Indice.data());
    }
    /**
     * Bind the vertex and index buffer with commandlist
     */
    void SetVertexBuffer(UINT slot,const VertexBuffer* pVertexBuffer);
    void SetIndexBuffer(const IndexBuffer* pIndexBuffer);
    /**
     * Loading a texture
     */
    void LoadTextureFromFile(Texture* pTexture, const std::wstring& filename, TextureUsage textureUsage,bool IsCubeMap = false);
    /**
     * Copy a resource to other resource.
     * This function often be used to copy a off-screen texture to backbuffer.
     */
    void CopyResource(Resource* pDstResource,const Resource* pSrcResource);
    /**
     * Copy a texture resource or subresource from CPU to default heap in GPU.
     * This function often be used to LoadTextureFromFile
     */
    void CopyTextureSubResource(const Texture* pResource, UINT firstSubResource, UINT numSubResources, D3D12_SUBRESOURCE_DATA* pSubResourceData);
    /**
     * Copy a multi-sample texture to a non-multi-sample texture
     * @see:https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-resolvesubresource
     */
    void ResolveSubResource(Resource* pDstResource, const Resource* pSrcResource, UINT DstSubResource = 0, UINT SrcSubResource = 0);
    /**
     * Clear a color of depth value of a texture.
     */
    void ClearRenderTargetTexture(const Texture* pTexture, const float ClearColor[4],bool IsFlushBarrier = false);
    void ClearDepthStencilTexture(const Texture* pTexture, D3D12_CLEAR_FLAGS Flags, float depth = 1.0f, UINT stencil = 0);
    void ClearRenderTarget(const RenderTarget* pRenderTarget);
    /**
     * Set a viewport and scissor rect.
     */
    void SetD3D12ViewPort(const D3D12_VIEWPORT* pViewPort);
    void SetD3D12ScissorRect(const RECT* pScissorRect);
    /**
     * Set current pipeline state.
     */
    void SetD3D12PipelineState(Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState);

    /************************************************************************/
    /* Some functions for commit resource for graphics                      */
    /************************************************************************/
    //Commite a constant buffer to dynamic upload buffer in GPU
    //Note:we must make sure that the rootParameterIndex argument is a Cbv in RootSignature
    void SetGraphicsDynamicConstantBuffer(UINT rootParameterIndex,UINT sizeInByte,const void* pMappedData);
    template<typename T>
    void SetGraphicsDynamicConstantBuffer(UINT rootParameterIndex, const T& MappedData)
    {
        SetGraphicsDynamicConstantBuffer(rootParameterIndex, sizeof(T), &MappedData);
    }
    //Commite a structuredbuffer(array) to dynamic upload buffer in GPU
    //Note:we must make sure that the rootParameterIndex argument is a SRV in RootSignature
    void SetGraphicsStructuredBuffer(UINT rootParameterIndex, UINT numElements,UINT elementByteSize ,const void* pMappedData);
    template<typename T>
    void SetGraphicsStructuredBuffer(UINT rootParameterIndex, const std::vector<T>& mappedData)
    {
        SetGraphicsStructuredBuffer(rootParameterIndex, mappedData.size(), sizeof(T), mappedData.data());
    }
    //Commite some constants to GPU.
    //Note the data which will be uploaded must be a multiple of 4 Bytes.
    //And the root siagnature which rootParameterIndex present must be initialized 32BitRootConstants.
    void SetGraphics32BitConstants(UINT rootParameterIndex, UINT num32BitValues, UINT offset32Bit, const void* pMapppedData);
    template<typename T>
    void SetGraphics32BitConstants(UINT rootParameterIndex, UINT offset32Bit, const T& mappedData)
    {
        assert(sizeof(T) % sizeof(uint32_t) == 0 && "The size of mappedData must be a multiple of 4 Bytes");
        SetGraphics32BitConstants(rootParameterIndex, sizeof(T) / sizeof(uint32_t), offset32Bit, &mappedData);
    }
    /************************************************************************/
    /* Some functions for commit resource for dispatch                                                                      */
    /************************************************************************/

    //
    void SetComputeDynamicConstantBuffer(UINT rootParameter, UINT sizeInByte, const void* pMappedData);
    template<typename T>
    void SetComputeDynamicConstantBuffer(UINT rootParameter, const T& data)
    {
        SetComputeDynamicConstantBuffer(rootParameter, sizeof(T), &data);
    }

    //
    void SetComputeStructuredBuffer(UINT rootParameter, UINT numElements, UINT elementSizeInByte, const void* pElementMappedData);
    template<typename T>
    void SetComputeStructuredBuffer(UINT rootParameter, const std::vector<T>& buffer)
    {
        SetComputeDynamicConstantBuffer(rootParameter, buffer.size(), sizeof(T), buffer.data());
    }

    //
    void SetCompute32BitConstants(UINT rootParameterIndex, UINT num32BitConstants, UINT destOffset32Bit, const void* pConstant);
    template<typename T>
    void SetCompute32BitConstants(UINT rootParameterIndex, UINT destOffset32Bit, const T& Constant)
    {
        assert(sizeof(T) % sizeof(uint32_t) == 0 && "The size of Constant must be a multiple of 4 Bytes");
        SetCompute32BitConstants(rootParameterIndex, sizeof(T) / sizeof(uint32_t), destOffset32Bit, &Constant);
    }
    //Set several shader resource views in CPU to map descriptors in GPU.
    //Before start to draw, these descriptors will be bound to commandlist.
    //@param:pSrvDesc could be nullptr which indicates that this resource will have a default SRV
    //@see:https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createshaderresourceview
    void SetShaderResourceView(
        UINT rootParameterIndex,
        UINT offsetInTable,
        const Resource* pResource,
        D3D12_RESOURCE_STATES stateAfter,
        UINT firstSubResource = 0,
        UINT subResource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
        const D3D12_SHADER_RESOURCE_VIEW_DESC* pSrvDesc = nullptr);
    //Set several unordered access views in CPU to map descriptors in GPU.
    //Before start to draw, these descriptors will be bound to commandlist.
    //@param:pUavDesc could be nullptr which indicates that this resource will have a default UAV
    //@see:https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createunorderedaccessview
    void SetUnorderedAccessView(
        UINT rootParameterIndex,
        UINT offstInTable,
        const Resource* pResource,
        D3D12_RESOURCE_STATES stateAfter,
        UINT firstSubResource = 0,
        UINT subResource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
        const D3D12_UNORDERED_ACCESS_VIEW_DESC* pUavDesc = nullptr);
    //Set a sampler in descirptor heap.We just often use static samplers.
    void SetSamplers(UINT rootParameterIndex,UINT offsetInTable,const D3D12_SAMPLER_DESC* pSamplerDesc);
    /**
     * Set a graphics root signature,this will invoke parse rootsignature and set root signature.
     */
    void SetGraphicsRootSignature(const RootSignature* pGraphicsRootSignature);
    /**
     * Set a compute root signature,this will invoke parse rootsignature and set root signature.
     */
    void SetComputeRootSignature(const RootSignature* pComputeRootSignature);
    //Set primitive type
    void SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology);
    //Set a render target array which indicates all render targets in one rendring.
    void SetRenderTargets(const RenderTarget& RenderTargets);
    //
    void SetDynamicVertexBuffer(UINT slot,UINT vertexCount, UINT vertexByteSize,const void* pVertexData);
    void SetDynamicIndexBuffer(UINT indexCount, DXGI_FORMAT indexFormat, const void* pIndexData);

    //Draw a group of vertex without index.This function often is used to post-process technology.
    void Draw(UINT vertexCount, UINT instanceCount, UINT startVertexLocation, UINT startInstanceLocation);
    //Draw a group of vertex with index.
    void DrawIndexed(UINT indexCount, UINT instanceCount, UINT startIndexLocation, UINT baseVertexLocation, UINT startInstanceLocation);
    //
    void Dispatch(UINT NumGroupX, UINT NumGroupY, UINT NumGroupZ);

    void GenerateMipMaps(Texture* pTexture);

    std::shared_ptr<CommandList> GetGenerateMipsCommandList()const { return m_pComputeCommandList; };

    /************************************************************************/
    /*Fllowing functions are ready for other interior classes to use        */
    /************************************************************************/

    /**
     * This is Close is for CommandQueue to use.
     * Before the command queue to execute,we need to flush all pending resource state.
     * If pending resource state is not empty.We need a new commandlist to insert resource barrier before current commandlist.
     * @param : pendingCommandList: a new commandlist to execute pending resource barrier.
     * @return : If the pending resource barrier is not empty,return TRUE otherwise FALSE.
     */
    bool Close(CommandList& pendingCommandList);
    /**
     * Just to close current commandlist
     * This is useful for pending commandlist 
     */
    void Close();
    /**
     * Reset current commandlist.
     * This function should be called before use CommandQueue::GetCommandList method for next reuse.
     */
    void Reset();
    //Set a descriptor heap to commandlist.
    //This method should only be invoked by DynamicDescriptorHeap class.
    void SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, ID3D12DescriptorHeap* descriptorHeap);
    //This function should only be called by class Scene
    //void DrawModel(const Model* pModel, std::function<void()> AdditonalResource = {});
    //This function shoule only be called by class shadow
    //Some shadow algorithm needs several passes to rendering.
    //The defualt passindex is Zero which indicates that this algorithm needs only one pass
    void RenderShadow(const ShadowBase* pShadow,UINT PassIndex = 0);

    DynamicDescriptorHeap* GetDynamicDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type) { return m_pDynamicDescriptorHeap[Type].get(); }

protected:
    friend class Filter;
    //Flush all non-pending resource barrier.
    void FlushResourceBarrier();

    void AddObjectTracker(Microsoft::WRL::ComPtr<ID3D12Object> object);

    void AddResourceTracker(const Resource* pResource);

    void ClearObjectTracker();
    //Copy a buffer data from CPU to GPU in default heap.
    //This function ofter is uesd to upload vertex or index to GPU in default heap,which indicates the data in GPU
    //will not be changed every frame.
    void CopyBuffer(Buffer* buffer,UINT bufferSize,UINT elementByteSize,const void* pBufferData);

    /**
     * 
     */
    void GenerateMips_UAV(Texture* pTexture);

    void GenerateMips_BGR(Texture* pTexture);

    void GenerateMips_SRGB(Texture* pTexture);
private:
    D3D12_COMMAND_LIST_TYPE m_d3d12CommandListType;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>     m_d3d12CommandAlloctor;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> m_d3d12GraphicsCommandList2;
    //a reserved compute command list for generating mips
    std::shared_ptr<CommandList> m_pComputeCommandList;
    std::unique_ptr<GenerateMips> m_pGenerateMips;
    std::unique_ptr<GenerateSAT> m_pGenerateSAT;

    std::unique_ptr<ResourceStateTracker> m_pResourceStateTracker;

    std::unique_ptr<UploadBuffer> m_pDynamicUploadBuffer;

    std::unique_ptr<DynamicDescriptorHeap> m_pDynamicDescriptorHeap[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
    //Current descriptor heap pointer for each descriptor heap type.
    //When current descriptor heap pointer change,we need to check and update it.
    ID3D12DescriptorHeap* m_pCurrentDescriptorHeap[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

    using ObjectTracker = std::vector < Microsoft::WRL::ComPtr<ID3D12Object>>;
    ObjectTracker m_ObjectTracker;
    //Current bind RootSignature with commandlist
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_d3d12RootSignature;
    //Current bind PipelineState with commandlist
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_d3d12PipelineState;

    static std::unordered_map<std::wstring, Texture*> m_TextureResourceMap;
    static std::mutex ms_TextureCacheMutex;
};