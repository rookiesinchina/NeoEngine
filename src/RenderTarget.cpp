#include "RenderTarget.h"

RenderTarget::RenderTarget()
{
    m_RenderTargetArray.resize(AttachmentPoint::NumAttachment);
}

RenderTarget::~RenderTarget() {};

void RenderTarget::AttachTexture(AttachmentPoint attchmentPoint,const Texture& pRenderTarget)
{
    m_RenderTargetArray[attchmentPoint] = std::move(const_cast<Texture&>(pRenderTarget));
}

void RenderTarget::Resize(UINT Width, UINT Height)
{
    //Here must be &
    for (auto& texture : m_RenderTargetArray)
    {
        texture.Resize(Width, Height);
    }
}

const std::vector<Texture>& RenderTarget::GetTextureArray()const
{
    return m_RenderTargetArray;
}

const Texture& RenderTarget::GetTexture(AttachmentPoint attachmentPoint)const
{
    return m_RenderTargetArray[attachmentPoint];
}

void RenderTarget::SetFormat(AttachmentPoint attachmentPoint,DXGI_FORMAT Format)
{
    if (m_RenderTargetArray[(size_t)attachmentPoint],IsValidResource())
    {
        m_RenderTargetArray[(size_t)attachmentPoint].ResetFormat(Format);
    }
}

D3D12_RT_FORMAT_ARRAY RenderTarget::GetRenderTargetFormats()const
{
    D3D12_RT_FORMAT_ARRAY rendertargetArray = {};
    for (size_t i = AttachmentPoint::Color0 ; i < AttachmentPoint::Color7 ;++i)
    {
        auto pTex = m_RenderTargetArray[i];
        if (pTex.IsValidResource())
        {
            rendertargetArray.RTFormats[rendertargetArray.NumRenderTargets++] = pTex.GetD3D12ResourceDesc().Format;
        }
    }
    return rendertargetArray;
}

DXGI_FORMAT RenderTarget::GetDepthStencilFormat()const
{
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    if (m_RenderTargetArray[AttachmentPoint::DepthStencil].IsValidResource())
    {
        format = m_RenderTargetArray[AttachmentPoint::DepthStencil].GetD3D12ResourceDesc().Format;
    }
    return format;
}