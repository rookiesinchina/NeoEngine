#pragma once

#include "d3dUtil.h"
#include <wrl.h>
#include <queue>
#include <memory>


//UploadBuffer Class is a dynamic buffer upload class.This class is mainly responsible for uploading buffer data in CPU to GPU.
//The UploadBuffer is a dynamic buffer which means this buffer will upload data to GPU each frame.So many buffer datas
//may be use this class such as Vertex Buffer,Index Buffer,Constant Buffer and so on.Since the DirectX need the constant buffer
//alignment 256 byte so many functions provide alignment argument.
//After a fence is finished,Reset() method will reuse all existed buffer.


class UploadBuffer
{
public:
    //A struct for uploadbuffer output,this contains CPU and GPU address of the data
    struct Allocation
    {
        void* CPU;
        D3D12_GPU_VIRTUAL_ADDRESS GPU;
    };
    
    //Construction and destruction funtion
    UploadBuffer(UINT pageSize = _2MB);
    ~UploadBuffer();
    //Get memory page size
    UINT PageSize()const;
    //Allocate a enough large space to store the data
    Allocation Allocate(UINT sizeInBytes, UINT alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    //Reset all memory page for resue.
    //Note Reset() method only can be invoked after all command list which reference resource has been execuated.
    void Reset();
private:
    struct Page 
    {
    public:
        Page(UINT pageSize);
        ~Page();
        //Check if this page has enough memory to store data.
        bool HasSpace(UINT sizeInBytes, UINT alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

        Allocation Allocate(UINT sizeInBytes, UINT alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

        void Reset();
    private:
        Microsoft::WRL::ComPtr<ID3D12Resource> m_PageResource;
        //Base pointer
        void* m_CPUPtr;
        D3D12_GPU_VIRTUAL_ADDRESS m_GPUPtr;
        //Offset
        UINT m_Offset;
        //Memory page size
        UINT m_PageSize;
    };

    using PagePool = std::deque<std::shared_ptr<Page>>;

    PagePool m_AllPagePool;
    PagePool m_AvailablePagePool;

    std::shared_ptr<Page> m_CurrentPage;

    std::shared_ptr<Page> RequestPage();

    UINT m_PageSize;
};

