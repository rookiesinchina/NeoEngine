#include "Buffer.h"

Buffer::Buffer(const std::wstring& bufferName /* = L"NoName" */)
    :Resource(bufferName)
{};

Buffer::Buffer(const D3D12_RESOURCE_DESC& bufferDesc, const std::wstring& bufferName /* = L"NoName" */)
    :Resource(bufferDesc, bufferName)
{};

Buffer::Buffer(Microsoft::WRL::ComPtr<ID3D12Resource> bufferResource, const std::wstring& bufferName /* = L"NoName" */)
    :Resource(bufferResource, bufferName)
{};

Buffer::~Buffer()
{};

void Buffer::SetD3D12Resource(Microsoft::WRL::ComPtr<ID3D12Resource>& d3d12Resource, const D3D12_CLEAR_VALUE* ClearValue)
{
    Resource::SetD3D12Resource(d3d12Resource, ClearValue);
}