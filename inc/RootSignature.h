#pragma once

#include "d3dx12.h"
#include <wrl.h>
#include <assert.h>



// The RootSignature class is a wrapper of ID3D12RootSignature ComObject.
// The main function is to parse struct D3D12_ROOT_SIGNATURE_DESC1 and mark the descriptor table root parameter.
// Since these descriptors in descriptor tables in CPU need to copy descriptor heap in GPU before rendering.
// And this class also create rootsignature according to root signature version.
// The DynamicDescriptorHeap class will need this class object to manage descriptors in CPU to GPU.

//Note:For safety,the descirptor table index in root parameters should not exceed 32 better.
class RootSignature
{
public:
    RootSignature();
    //This class will use 32bit uint to mask descriptor table index in descriptor parameter.
    //So for better use,we should try our best to set descriptor table in the head of descriptor parameter and not exceed 32 descriptor table.
    RootSignature(const D3D12_ROOT_SIGNATURE_DESC1& rootSigDesc1, D3D_ROOT_SIGNATURE_VERSION rootSigVersion);
    //Since we use memory in memory heap,so we need to delete the copy and assign functions.
    RootSignature(const RootSignature& copy) = delete;
    RootSignature& operator=(const RootSignature& assign) = delete;

    ~RootSignature();
    //Parse descriptor tables and create rootsignature
    //@Param: 
    //* rootSigDesc1: A given rootsignature description sturct to parse descriptor tables infomation.
    //* rootSigVersion: A rootsignature version to create rootsignature.
    void SetRootSignatureDesc(const D3D12_ROOT_SIGNATURE_DESC1& rootSigDesc1,D3D_ROOT_SIGNATURE_VERSION rootSigVersion);
    //Get a created rootsignature from rootsignature description.
    Microsoft::WRL::ComPtr<ID3D12RootSignature> GetRootSignature()const { return m_d3d12RootSignature; }
    //Use index to get descriptor number in different descriptor table.
    //The maximum value of index is 32.
    UINT GetNumDescriptorsInTable(UINT index)const { assert(index < 32); return m_NumDescriptorsPerTable[index]; }
    //Use descriptor_heap_type to get descriptor table mask
    //The support type are:
    //D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
    //---------------------------------
    //D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER
    UINT GetDescriptorTableBitMask(D3D12_DESCRIPTOR_HEAP_TYPE type)const;
    //@return: A rootsignature desc which stores all descriptors infomation.
    //The return value shoule be reference or pointer.
    const D3D12_ROOT_SIGNATURE_DESC1& GetRootSignatureDesc1()const { return m_d3d12RootSigDesc1; }
protected:
    //Free memory in memory heap.
    void Destroy();
private:
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_d3d12RootSignature;
    D3D12_ROOT_SIGNATURE_DESC1 m_d3d12RootSigDesc1;
    //Bit mask for cbv_srv_uav descriptor table.The Cbv,Srv,Uav have different heap with sampler.
    UINT m_CbvSrvUavDescriptorTableBitMask;
    //Bit mask for sampler descriptor table.
    UINT m_SamplerTableBitMask;
    //Descriptor number in different table.
    UINT m_NumDescriptorsPerTable[32];

};