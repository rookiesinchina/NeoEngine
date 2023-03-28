#include "CommandQueue.h"
#include "d3dUtil.h"

//CommandQueue::CommandQueue(Microsoft::WRL::ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type)
//{
//    m_d3d12Device = device;
//    m_d3d12CommandListType = type;
//
//    // Create commandqueue and fence
//    D3D12_COMMAND_QUEUE_DESC cmdQueueDesc;
//    cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
//    cmdQueueDesc.NodeMask = 0;
//    cmdQueueDesc.Priority = 0;
//    cmdQueueDesc.Type = m_d3d12CommandListType;
//    // Create a handle for signal.
//    m_hEvent = CreateEventEx(0, 0, 0, EVENT_ALL_ACCESS);
//
//    ThrowIfFailed(m_d3d12Device->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&m_d3d12CommandQueue)));
//    ThrowIfFailed(m_d3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_d3d12Fence)));
//}
//
//CommandQueue::~CommandQueue()
//{
//    CloseHandle(m_hEvent);
//}
//
//Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> CommandQueue::GetCommandList()
//{
//    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandalloctor;
//    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandlist;
//    // There is an idle command alloctor in queue
//    // We should notice that the commandalloctor only can be reused when the commands in alloctor all are executed. 
//    if (!m_d3d12CommandAlloctorQueue.empty() && IsFenceCompleted(m_d3d12CommandAlloctorQueue.front().FenceValue))
//    {
//        commandalloctor = m_d3d12CommandAlloctorQueue.front().CommandAlloctor;
//        m_d3d12CommandAlloctorQueue.pop();
//        //Attention!!! If the command alloctor is not a new one,we must reset before use.
//        //Unless the command alloctor memory will be larger and larger when rendering.
//        ThrowIfFailed(commandalloctor->Reset());
//    }
//    // If no idle commandalloctor , we create a new one.
//    else
//    {
//        commandalloctor = CreateCommandAlloctor();
//    }
//    // Create command list
//    if (!m_d3d12CommandListQueue.empty())
//    {
//        commandlist = m_d3d12CommandListQueue.front();
//        m_d3d12CommandListQueue.pop();
//
//        ThrowIfFailed(commandlist->Reset(commandalloctor.Get(), nullptr));
//    }
//    else
//    {
//        commandlist = CreateCommandList(commandalloctor);
//
//        ThrowIfFailed(commandlist->Reset(commandalloctor.Get(), nullptr));
//    }
//
//    // We store a refrence which is associated with commandalloctor in commandlist.
//    // So we can easily get associated commandalloctor when we push commandqueue to queue.
//    ThrowIfFailed(commandlist->SetPrivateDataInterface(__uuidof(ID3D12CommandAllocator),commandalloctor.Get()));
//
//    return commandlist;
//}
//
//uint64_t CommandQueue::ExecuteCommandList(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandlist)
//{
//    // Before we execute commands, we need to close commandlist firstly.
//    ThrowIfFailed(commandlist->Close());
//
//    ID3D12CommandList* CmdList[] =
//    {
//        commandlist.Get()
//    };
//    m_d3d12CommandQueue->ExecuteCommandLists(_countof(CmdList), CmdList);
//
//    // We need to set a fence value to achieve the sync between CPU and GPU
//    uint64_t fencevalue = Signal();
//    // We get commandalloctor which is associated with current commandlist
//    ID3D12CommandAllocator* commandalloctor;
//    UINT byteSize = sizeof(commandalloctor);
//    ThrowIfFailed(commandlist->GetPrivateData(__uuidof(ID3D12CommandAllocator), &byteSize, &commandalloctor));
//    // Finally,we push the commandalloctor and commandlist in queue.
//    m_d3d12CommandAlloctorQueue.emplace(CommandAlloctorEntry{ fencevalue,commandalloctor });
//    m_d3d12CommandListQueue.push(commandlist);
//    // Notice:We must release temporary CommandAlloctor object to decrease COM object reference count
//    //, otherwise this will lead memeory leak.
//    commandalloctor->Release();
//    //
//    return fencevalue;
//}
//
//uint64_t CommandQueue::Signal()
//{
//    ++m_FenceValue;
//    ThrowIfFailed(m_d3d12CommandQueue->Signal(m_d3d12Fence.Get(), m_FenceValue));
//    return m_FenceValue;
//}
//
//bool CommandQueue::IsFenceCompleted(uint64_t fencevalue)
//{
//    return m_d3d12Fence->GetCompletedValue() >= fencevalue;
//}
//
//void CommandQueue::WaitForFenceValue(uint64_t fencevalue)
//{
//    if (!IsFenceCompleted(fencevalue))
//    {
//        m_d3d12Fence->SetEventOnCompletion(fencevalue, m_hEvent);
//        WaitForSingleObject(m_hEvent, INFINITE);
//    }
//}
//
//void CommandQueue::Flush()
//{
//    uint64_t fencevalue = Signal();
//    WaitForFenceValue(fencevalue);
//}
//
//Microsoft::WRL::ComPtr<ID3D12CommandQueue> CommandQueue::GetD3DCommandQueue()const
//{
//    return m_d3d12CommandQueue;
//}
//
//Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CommandQueue::CreateCommandAlloctor()
//{
//    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandalloctor;
//    ThrowIfFailed(m_d3d12Device->CreateCommandAllocator(m_d3d12CommandListType, IID_PPV_ARGS(&commandalloctor)));
//
//    return commandalloctor;
//}
//
//Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> CommandQueue::CreateCommandList(Microsoft::WRL::ComPtr<ID3D12CommandAllocator> alloctor)
//{
//    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandlist;
//    ThrowIfFailed(m_d3d12Device->CreateCommandList(0, m_d3d12CommandListType, alloctor.Get(), nullptr, IID_PPV_ARGS(&commandlist)));
//    ThrowIfFailed(commandlist->Close());
//    
//    return commandlist;
//}


#include "CommandQueue.h"

#include "Application.h"
#include "CommandList.h"
#include "ResourceStateTracker.h"

CommandQueue::CommandQueue(D3D12_COMMAND_LIST_TYPE type)
    : m_FenceValue(0)
    , m_CommandListType(type)
    , m_bProcessInFlightCommandLists(true)
{
    auto device = Application::GetApp()->GetDevice();

    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type = type;
    desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.NodeMask = 0;

    ThrowIfFailed(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_d3d12CommandQueue)));
    ThrowIfFailed(device->CreateFence(m_FenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_d3d12Fence)));

    switch (type)
    {
    case D3D12_COMMAND_LIST_TYPE_COPY:
        m_d3d12CommandQueue->SetName(L"Copy Command Queue");
        break;
    case D3D12_COMMAND_LIST_TYPE_COMPUTE:
        m_d3d12CommandQueue->SetName(L"Compute Command Queue");
        break;
    case D3D12_COMMAND_LIST_TYPE_DIRECT:
        m_d3d12CommandQueue->SetName(L"Direct Command Queue");
        break;
    }

    m_ProcessInFlightCommandListsThread = std::thread(&CommandQueue::ProccessInFlightCommandLists, this);
}

CommandQueue::~CommandQueue()
{
    m_bProcessInFlightCommandLists = false;
    m_ProcessInFlightCommandListsThread.join();
}

uint64_t CommandQueue::Signal()
{
    uint64_t fenceValue = ++m_FenceValue;
    m_d3d12CommandQueue->Signal(m_d3d12Fence.Get(), fenceValue);
    return fenceValue;
}

bool CommandQueue::IsFenceComplete(uint64_t fenceValue)
{
    return m_d3d12Fence->GetCompletedValue() >= fenceValue;
}

void CommandQueue::WaitForFenceValue(uint64_t fenceValue)
{
    if (!IsFenceComplete(fenceValue))
    {
        auto event = ::CreateEvent(NULL, FALSE, FALSE, NULL);
        assert(event && "Failed to create fence event handle.");

        // Is this function thread safe?
        m_d3d12Fence->SetEventOnCompletion(fenceValue, event);
        ::WaitForSingleObject(event, DWORD_MAX);

        ::CloseHandle(event);
    }
}

void CommandQueue::Flush()
{
    std::unique_lock<std::mutex> lock(m_ProcessInFlightCommandListsThreadMutex);
    m_ProcessInFlightCommandListsThreadCV.wait(lock, [this] { return m_InFlightCommandLists.Empty(); });

    // In case the command queue was signaled directly 
    // using the CommandQueue::Signal method then the 
    // fence value of the command queue might be higher than the fence
    // value of any of the executed command lists.
    WaitForFenceValue(m_FenceValue);
}

std::shared_ptr<CommandList> CommandQueue::GetCommandList()
{
    std::shared_ptr<CommandList> commandList;

    // If there is a command list on the queue.
    if (!m_AvailableCommandLists.Empty())
    {
        m_AvailableCommandLists.TryPop(commandList);
    }
    else
    {
        // Otherwise create a new command list.
        commandList = std::make_shared<CommandList>(m_CommandListType);
    }

    return commandList;
}

// Execute a command list.
// Returns the fence value to wait for for this command list.
uint64_t CommandQueue::ExecuteCommandList(std::shared_ptr<CommandList> commandList)
{
    return ExecuteCommandLists(std::vector<std::shared_ptr<CommandList> >({ commandList }));
}

uint64_t CommandQueue::ExecuteCommandLists(const std::vector<std::shared_ptr<CommandList> >& commandLists)
{
    ResourceStateTracker::Lock();

    // Command lists that need to put back on the command list queue.
    std::vector<std::shared_ptr<CommandList> > toBeQueued;
    toBeQueued.reserve(commandLists.size() * 2);        // 2x since each command list will have a pending command list.

    // Generate mips command lists.
    std::vector<std::shared_ptr<CommandList> > generateMipsCommandLists;
    generateMipsCommandLists.reserve(commandLists.size());

    // Command lists that need to be executed.
    std::vector<ID3D12CommandList*> d3d12CommandLists;
    d3d12CommandLists.reserve(commandLists.size() * 2); // 2x since each command list will have a pending command list.

    for (auto commandList : commandLists)
    {
        auto pendingCommandList = GetCommandList();
        bool hasPendingBarriers = commandList->Close(*pendingCommandList);
        pendingCommandList->Close();
        // If there are no pending barriers on the pending command list, there is no reason to 
        // execute an empty command list on the command queue.
        if (hasPendingBarriers)
        {
            d3d12CommandLists.push_back(pendingCommandList->GetGraphicsCommandList2().Get());
        }
        d3d12CommandLists.push_back(commandList->GetGraphicsCommandList2().Get());

        toBeQueued.push_back(pendingCommandList);
        toBeQueued.push_back(commandList);

        auto generateMipsCommandList = commandList->GetGenerateMipsCommandList();
        if (generateMipsCommandList)
        {
            generateMipsCommandLists.push_back(generateMipsCommandList);
        }
    }

    UINT numCommandLists = static_cast<UINT>(d3d12CommandLists.size());
    m_d3d12CommandQueue->ExecuteCommandLists(numCommandLists, d3d12CommandLists.data());


    uint64_t fenceValue = Signal();

    ResourceStateTracker::UnLock();

    // Queue command lists for reuse.
    for (auto commandList : toBeQueued)
    {
        m_InFlightCommandLists.Push({ fenceValue, commandList });
    }

    // If there are any command lists that generate mips then execute those
    // after the initial resource command lists have finished.
    if (generateMipsCommandLists.size() > 0)
    {
        auto computeQueue = Application::GetApp()->GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE);
        computeQueue->Wait(*this);
        computeQueue->ExecuteCommandLists(generateMipsCommandLists);
    }

    return fenceValue;
}

void CommandQueue::Wait(const CommandQueue& other)
{
    m_d3d12CommandQueue->Wait(other.m_d3d12Fence.Get(), other.m_FenceValue);
}

Microsoft::WRL::ComPtr<ID3D12CommandQueue> CommandQueue::GetD3D12CommandQueue() const
{
    return m_d3d12CommandQueue;
}

void CommandQueue::ProccessInFlightCommandLists()
{
    std::unique_lock<std::mutex> lock(m_ProcessInFlightCommandListsThreadMutex, std::defer_lock);

    while (m_bProcessInFlightCommandLists)
    {
        CommandListEntry commandListEntry;

        lock.lock();
        while (m_InFlightCommandLists.TryPop(commandListEntry))
        {
            auto fenceValue = std::get<0>(commandListEntry);
            auto commandList = std::get<1>(commandListEntry);

            WaitForFenceValue(fenceValue);

            commandList->Reset();

            m_AvailableCommandLists.Push(commandList);
        }
        lock.unlock();
        m_ProcessInFlightCommandListsThreadCV.notify_one();

        std::this_thread::yield();
    }
}