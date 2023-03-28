#include "d3dUtil.h"
#include <comdef.h>

DxException::DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber) :
    ErrorCode(hr),
    FunctionName(functionName),
    Filename(filename),
    LineNumber(lineNumber)
{
}

std::wstring DxException::ToString()const
{
    // Get the string description of the error code.
    _com_error err(ErrorCode);
    std::wstring msg = err.ErrorMessage();

    return FunctionName + L" failed in " + Filename + L"; line " + std::to_wstring(LineNumber) + L"; error: " + msg;
}

Microsoft::WRL::ComPtr<ID3D12Resource> d3dUtil::CreateDefaultBuffer(
    ID3D12Device2* device,
    ID3D12GraphicsCommandList2* commandlist,
    const void* initData,
    UINT byteSize,
    Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer)
{
    Microsoft::WRL::ComPtr<ID3D12Resource> defaultBuffer;

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(byteSize),
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&defaultBuffer)));

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(byteSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadBuffer)));

    //Describe the data that we want to copy into the default buffer
    D3D12_SUBRESOURCE_DATA subResourceData = {};
    subResourceData.pData = initData;
    subResourceData.RowPitch = byteSize;
    subResourceData.SlicePitch = subResourceData.RowPitch;

    commandlist->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        uploadBuffer.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_SOURCE));

    ::UpdateSubresources(commandlist, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);

    commandlist->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        defaultBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));

    return defaultBuffer;
}

Microsoft::WRL::ComPtr<ID3DBlob> d3dUtil::CompileShader(
    const std::wstring& filename,
    const D3D_SHADER_MACRO* defines,
    const std::string& entrypoint,
    const std::string& target)
{
    UINT compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)  
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT hr = S_OK;

    Microsoft::WRL::ComPtr<ID3DBlob> byteCode = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    hr = ::D3DCompileFromFile(filename.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entrypoint.c_str(), target.c_str(), compileFlags, 0, &byteCode, &errors);

    if (errors != nullptr)
        ::OutputDebugStringA((char*)errors->GetBufferPointer());

    ThrowIfFailed(hr);

    return byteCode;
}

std::vector<D3D12_STATIC_SAMPLER_DESC> d3dUtil::GetStaticSamplers()
{
    CD3DX12_STATIC_SAMPLER_DESC PointClamp = {};
    PointClamp.Init(
        0,
        D3D12_FILTER_MIN_MAG_MIP_POINT,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
    CD3DX12_STATIC_SAMPLER_DESC PointMirror = {};
    PointMirror.Init(
        1,
        D3D12_FILTER_MIN_MAG_MIP_POINT,
        D3D12_TEXTURE_ADDRESS_MODE_MIRROR,
        D3D12_TEXTURE_ADDRESS_MODE_MIRROR,
        D3D12_TEXTURE_ADDRESS_MODE_MIRROR);
    CD3DX12_STATIC_SAMPLER_DESC PointWrap = {};
    PointWrap.Init(
        2,
        D3D12_FILTER_MIN_MAG_MIP_POINT,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP);
    CD3DX12_STATIC_SAMPLER_DESC LinearClamp = {};
    LinearClamp.Init(
        3,
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
    CD3DX12_STATIC_SAMPLER_DESC LinearMirror = {};
    LinearMirror.Init(
        4,
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_MIRROR,
        D3D12_TEXTURE_ADDRESS_MODE_MIRROR,
        D3D12_TEXTURE_ADDRESS_MODE_MIRROR);
    CD3DX12_STATIC_SAMPLER_DESC LinearWrap = {};
    LinearWrap.Init(
        5,
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP);
    CD3DX12_STATIC_SAMPLER_DESC AnisotropicClamp = {};
    AnisotropicClamp.Init(
        6,
        D3D12_FILTER_ANISOTROPIC,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
    CD3DX12_STATIC_SAMPLER_DESC AnisotropicMirror = {};
    AnisotropicMirror.Init(
        7,
        D3D12_FILTER_ANISOTROPIC,
        D3D12_TEXTURE_ADDRESS_MODE_MIRROR,
        D3D12_TEXTURE_ADDRESS_MODE_MIRROR,
        D3D12_TEXTURE_ADDRESS_MODE_MIRROR);
    CD3DX12_STATIC_SAMPLER_DESC AnisotropicWrap = {};
    AnisotropicWrap.Init(
        8,
        D3D12_FILTER_ANISOTROPIC,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP);
    return { PointClamp,PointMirror,PointWrap,LinearClamp,LinearMirror,LinearWrap,AnisotropicClamp,AnisotropicMirror,AnisotropicWrap };
}
