#include "RootSignature.h"
#include "d3dUtil.h"
#include "Application.h"


RootSignature::RootSignature()
    :m_d3d12RootSignature(nullptr)
    , m_d3d12RootSigDesc1(D3D12_ROOT_SIGNATURE_DESC1())
    , m_CbvSrvUavDescriptorTableBitMask(0)
    , m_SamplerTableBitMask(0)
    , m_NumDescriptorsPerTable{0}
{};

RootSignature::RootSignature(const D3D12_ROOT_SIGNATURE_DESC1& rootSigDesc1, D3D_ROOT_SIGNATURE_VERSION rootSigVersion)
    :m_d3d12RootSignature(nullptr)
    ,m_d3d12RootSigDesc1()
    ,m_CbvSrvUavDescriptorTableBitMask(0)
    , m_SamplerTableBitMask(0)
    , m_NumDescriptorsPerTable{ 0 }
{
    SetRootSignatureDesc(rootSigDesc1, rootSigVersion);
}

RootSignature::~RootSignature()
{
    //Before desctruction, we must use destroy() firstly.
    Destroy();
}

void RootSignature::SetRootSignatureDesc(const D3D12_ROOT_SIGNATURE_DESC1& rootSigDesc1, D3D_ROOT_SIGNATURE_VERSION rootSigVersion)
{
    //Before set new root signaure,we need to release firstly.
    Destroy();

    D3D12_ROOT_PARAMETER1* pRootParameter = rootSigDesc1.NumParameters > 0 ? new D3D12_ROOT_PARAMETER1[rootSigDesc1.NumParameters] : nullptr;
    
    for (UINT i = 0; i < rootSigDesc1.NumParameters; ++i)
    {
        //Copy descriptor parameter infomation to new position.
        pRootParameter[i] = rootSigDesc1.pParameters[i];
        //We just care about descriptor table
        if (pRootParameter[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
        {
            //A descriptor table may has many descriptor ranges,we need to copy them all to new position.
            D3D12_DESCRIPTOR_RANGE1* pDescriptorRange = pRootParameter[i].DescriptorTable.NumDescriptorRanges > 0 ? 
                new D3D12_DESCRIPTOR_RANGE1[pRootParameter[i].DescriptorTable.NumDescriptorRanges] : nullptr;

            memcpy(pDescriptorRange, pRootParameter[i].DescriptorTable.pDescriptorRanges,
                sizeof(D3D12_DESCRIPTOR_RANGE1) * pRootParameter[i].DescriptorTable.NumDescriptorRanges);

            pRootParameter[i].DescriptorTable.pDescriptorRanges = pDescriptorRange;
            pRootParameter[i].DescriptorTable.NumDescriptorRanges = pRootParameter[i].DescriptorTable.NumDescriptorRanges;

            if (pDescriptorRange)
            {
                //Since different ranges maybe have different type(CBV,SRV,UAV,SAMPLER) in one descriptor table.
                //But we must note that the sampler can not be put same descriptor table with CBV,SRV and UAV.
                //That means:
                // ------------------------------------------------------------------------
                //                      SRV       CBV     UAV 
                //      Table 0 -----  Range 0   Range 1  Range 2 ........  This is OK.
                // ------------------------------------------------------------------------
                //                     SAMPLER   SAMPLER
                //      Table 1 -----  Range 0   Range 1 .......            This is OK.
                // ------------------------------------------------------------------------
                //                     SAMPLER    SRV      UAV
                //      Table 2 -----  Range 0   Range 1  Range2            No!Don't do this.
                //                                                          Samplers can not be same descriptor table 
                //                                                          with CBV,SRV or UAV.
                //                                                          @see:https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_root_descriptor_table           
                //-------------------------------------------------------------------------
                switch (pDescriptorRange[0].RangeType)
                {
                case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
                case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
                case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
                    m_CbvSrvUavDescriptorTableBitMask |= (1 << i);
                    break;
                case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER:
                    m_SamplerTableBitMask |= (1 << i);
                    break;
                }
            }
            //Compute all descriptor number in every descriptor table.
            for (UINT j = 0; j < pRootParameter[i].DescriptorTable.NumDescriptorRanges; ++j)
            {
                
                m_NumDescriptorsPerTable[i] += pDescriptorRange[j].NumDescriptors;
            }
        }
    }

    m_d3d12RootSigDesc1.pParameters = pRootParameter;
    m_d3d12RootSigDesc1.NumParameters = rootSigDesc1.NumParameters;
    //Copy static samplers to new position and other information.
    D3D12_STATIC_SAMPLER_DESC* pStaticSamplers = rootSigDesc1.NumStaticSamplers > 0 ? 
        new D3D12_STATIC_SAMPLER_DESC[rootSigDesc1.NumStaticSamplers] : nullptr;

    if (pStaticSamplers)
    {
        memcpy(pStaticSamplers, rootSigDesc1.pStaticSamplers, 
            sizeof(D3D12_STATIC_SAMPLER_DESC) * rootSigDesc1.NumStaticSamplers);
    }
    m_d3d12RootSigDesc1.pStaticSamplers = pStaticSamplers;
    m_d3d12RootSigDesc1.NumStaticSamplers = rootSigDesc1.NumStaticSamplers;

    m_d3d12RootSigDesc1.Flags = rootSigDesc1.Flags;
    //
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC versionRootSignatureDesc;
    versionRootSignatureDesc.Init_1_1(m_d3d12RootSigDesc1.NumParameters, m_d3d12RootSigDesc1.pParameters,
        m_d3d12RootSigDesc1.NumStaticSamplers, m_d3d12RootSigDesc1.pStaticSamplers, m_d3d12RootSigDesc1.Flags);

    Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSignature;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    //Create root signature
    HRESULT hr = D3DX12SerializeVersionedRootSignature(&versionRootSignatureDesc, rootSigVersion,
        serializedRootSignature.GetAddressOf(), errorBlob.GetAddressOf());

    if (FAILED(hr))
    {
        MessageBox(nullptr, L"Invalid RootSignature", L"Error", MB_OK | MB_ICONERROR);
    }
    ThrowIfFailed(hr);

    auto device = Application::GetApp()->GetDevice();
    ThrowIfFailed(device->CreateRootSignature(0, serializedRootSignature->GetBufferPointer(),
        serializedRootSignature->GetBufferSize(), IID_PPV_ARGS(&m_d3d12RootSignature)));
}

UINT RootSignature::GetDescriptorTableBitMask(D3D12_DESCRIPTOR_HEAP_TYPE type)const
{
    UINT mask = 0;
    switch (type)
    {
    case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV:
        mask = m_CbvSrvUavDescriptorTableBitMask;
        break;
    case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER:
        mask = m_SamplerTableBitMask;
        break;
    default:
        break;
    }
    return mask;
}

void RootSignature::Destroy()
{
    if (m_d3d12RootSigDesc1.NumParameters > 0)
    {
        for (UINT i = 0; i < m_d3d12RootSigDesc1.NumParameters; ++i)
        {
            const D3D12_ROOT_PARAMETER1& rootParameter = m_d3d12RootSigDesc1.pParameters[i];
            if (rootParameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
            {
                delete[] rootParameter.DescriptorTable.pDescriptorRanges;
            }
        }
        delete[] m_d3d12RootSigDesc1.pParameters;

        m_d3d12RootSigDesc1.pParameters = nullptr;
        m_d3d12RootSigDesc1.NumParameters = 0;
    }
    delete[]m_d3d12RootSigDesc1.pStaticSamplers;
    m_d3d12RootSigDesc1.pStaticSamplers = nullptr;
    m_d3d12RootSigDesc1.NumStaticSamplers = 0;

    m_CbvSrvUavDescriptorTableBitMask = 0;
    m_SamplerTableBitMask = 0;

    ::memset(m_NumDescriptorsPerTable, 0, sizeof(m_NumDescriptorsPerTable));
}