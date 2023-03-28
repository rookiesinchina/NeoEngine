#pragma once

#include "Texture.h"
//RenderTarget class is a texture array which includes eight possible render target textures and one depthstencil texture.
//This class can help us to manage multiple render target rendering.
//Note:If we use multiple rendering target technology, then all render target resources must possess SAME dimension!
enum AttachmentPoint
{
    Color0,
    Color1,
    Color2,
    Color3,
    Color4,
    Color5,
    Color6,
    Color7,
    DepthStencil,
    NumAttachment
};

class RenderTarget : public Texture
{
public:
    RenderTarget();
    ~RenderTarget();

    RenderTarget(const RenderTarget& copy) = default;
    RenderTarget& operator=(const RenderTarget& assign) = default;

    RenderTarget(RenderTarget&& move)noexcept = default;
    RenderTarget& operator=(RenderTarget&& move)noexcept = default;

    void AttachTexture(AttachmentPoint attchmentPoint, const Texture& pRenderTarget);

    void Resize(UINT Width, UINT Height);

    const std::vector<Texture>& GetTextureArray()const;

    const Texture& GetTexture(AttachmentPoint attachmentPoint)const;

    void SetFormat(AttachmentPoint attachmentPoint,DXGI_FORMAT Format);

    D3D12_RT_FORMAT_ARRAY GetRenderTargetFormats()const;

    DXGI_FORMAT GetDepthStencilFormat()const;
private:
    std::vector<Texture> m_RenderTargetArray;
};