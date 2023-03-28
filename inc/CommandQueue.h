#pragma once

#include <d3d12.h>//d3d12 objects
#include <wrl.h>  //COM interface

#include <cstdint>//uint64_t
#include <queue>  //std::queue

//A class for command queue , command list and signal
//class CommandQueue
//{
//public:
//    CommandQueue(Microsoft::WRL::ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT);
//    virtual ~CommandQueue();
//
//    // Get a commandlist that can record commands immediately.
//    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> GetCommandList();
//
//    // Execute commandlist.
//    // Return the fence value to wait for command list
//    uint64_t ExecuteCommandList(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandlist);
//    // Insert a signal value to CommandQueue
//    //@return signal value
//    uint64_t Signal();
//    // Check if fencevalue has been sent.
//    bool IsFenceCompleted(uint64_t fencevalue);
//    // Force to wait the fencevalue, so maintain the sync between CPU and GPU.
//    void WaitForFenceValue(uint64_t fencevalue);
//    // Force to flush command queue.
//    void Flush();
//
//    Microsoft::WRL::ComPtr<ID3D12CommandQueue> GetD3DCommandQueue()const;
//protected:
//    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CreateCommandAlloctor();
//    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> CreateCommandList(Microsoft::WRL::ComPtr<ID3D12CommandAllocator> alloctor);
//private:
//    struct CommandAlloctorEntry
//    {
//        uint64_t FenceValue;
//        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CommandAlloctor;
//    };
//
//    Microsoft::WRL::ComPtr<ID3D12Device2> m_d3d12Device;
//    Microsoft::WRL::ComPtr<ID3D12Fence>   m_d3d12Fence;
//    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_d3d12CommandQueue;
//
//    HANDLE   m_hEvent;
//    uint64_t m_FenceValue = 0;
//    D3D12_COMMAND_LIST_TYPE m_d3d12CommandListType;
//
//    using CommandAlloctorQueue = std::queue<CommandAlloctorEntry>;
//    using CommandListQueue = std::queue < Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>>;
//
//    CommandAlloctorQueue m_d3d12CommandAlloctorQueue;
//    CommandListQueue     m_d3d12CommandListQueue;
//};

#include "ThreadSafeQueue.h"

#include <d3d12.h>              // For ID3D12CommandQueue, ID3D12Device2, and ID3D12Fence
#include <wrl.h>                // For Microsoft::WRL::ComPtr

#include <atomic>               // For std::atomic_bool
#include <cstdint>              // For uint64_t
#include <condition_variable>   // For std::condition_variable.



class CommandList;

class CommandQueue
{
public:
    CommandQueue(D3D12_COMMAND_LIST_TYPE type);
    virtual ~CommandQueue();

    // Get an available command list from the command queue.
    std::shared_ptr<CommandList> GetCommandList();

    // Execute a command list.
    // Returns the fence value to wait for for this command list.
    uint64_t ExecuteCommandList(std::shared_ptr<CommandList> commandList);
    uint64_t ExecuteCommandLists(const std::vector<std::shared_ptr<CommandList> >& commandLists);

    uint64_t Signal();
    bool IsFenceComplete(uint64_t fenceValue);
    void WaitForFenceValue(uint64_t fenceValue);
    void Flush();

    // Wait for another command queue to finish.
    void Wait(const CommandQueue& other);

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> GetD3D12CommandQueue() const;

private:
    // Free any command lists that are finished processing on the command queue.
    void ProccessInFlightCommandLists();

    // Keep track of command allocators that are "in-flight"
    // The first member is the fence value to wait for, the second is the 
    // a shared pointer to the "in-flight" command list.
    using CommandListEntry = std::tuple<uint64_t, std::shared_ptr<CommandList> >;

    D3D12_COMMAND_LIST_TYPE                         m_CommandListType;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue>      m_d3d12CommandQueue;
    Microsoft::WRL::ComPtr<ID3D12Fence>             m_d3d12Fence;
    std::atomic_uint64_t                            m_FenceValue;

    ThreadSafeQueue<CommandListEntry>               m_InFlightCommandLists;
    ThreadSafeQueue<std::shared_ptr<CommandList> >  m_AvailableCommandLists;

    // A thread to process in-flight command lists.
    std::thread m_ProcessInFlightCommandListsThread;
    std::atomic_bool m_bProcessInFlightCommandLists;
    std::mutex m_ProcessInFlightCommandListsThreadMutex;
    std::condition_variable m_ProcessInFlightCommandListsThreadCV;
};



