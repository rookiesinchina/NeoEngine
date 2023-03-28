#include "ResourceStateTracker.h"
#include <assert.h>
#include "Resource.h"
#include "CommandList.h"

std::unordered_map<ID3D12Resource*, ResourceStateTracker::ResourceState> ResourceStateTracker::m_GlobalResourceState;
bool ResourceStateTracker::m_IsLock = false;
std::mutex ResourceStateTracker::ms_GlobalMutex;

ResourceStateTracker::ResourceStateTracker()
{};

ResourceStateTracker::~ResourceStateTracker() {};

void ResourceStateTracker::ResourceBarrier(const D3D12_RESOURCE_BARRIER& barrier)
{
    //We just handle the type of barrier is Transition.
    if (barrier.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)
    {
        const auto resourceTransition = barrier.Transition;
        //check if this resource have already existed in final resource state
        auto posIter = m_FinalResourceState.find(resourceTransition.pResource);
        //If a resource has existed in final resource state,that means we can get the final state of this resource
        if (posIter != m_FinalResourceState.end())
        {
            auto& subresourceState = posIter->second.subResourceState;
            //
            if (resourceTransition.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES
                && !subresourceState.empty())
            {
                for (auto state : subresourceState)
                {
                    if (resourceTransition.StateAfter != state.second)
                    {
                        D3D12_RESOURCE_BARRIER newBarrier = barrier;
                        //-----------------------------------------------------
                        newBarrier.Transition.Subresource = state.first;
                        //----------------------------------------------------
                        newBarrier.Transition.StateBefore = state.second;

                        m_ValidResourceBarrier.push_back(newBarrier);
                    }
                }
            }
            else
            {
                auto FinalState = posIter->second.GetSubResourceState(resourceTransition.Subresource);
                if (resourceTransition.StateAfter != FinalState)
                {
                    D3D12_RESOURCE_BARRIER newBarrier = barrier;
                    newBarrier.Transition.StateBefore = FinalState;

                    m_ValidResourceBarrier.push_back(newBarrier);
                }
            }
        }
        //If the resource has not final state,push this barrier into pending barrier
        else
        {
            m_PendingResourceBarrier.push_back(barrier);
        }
        //Update final resource state
        m_FinalResourceState[resourceTransition.pResource].SetResourceState(resourceTransition.Subresource, resourceTransition.StateAfter);
    }
    else//We just push the UavBarrier and AliasBarrier directly.
    {
        m_ValidResourceBarrier.push_back(barrier);
    }
}

void ResourceStateTracker::TransitionResource(ID3D12Resource* pD3D12Resource, D3D12_RESOURCE_STATES StateAfter, UINT subResource /* = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES */)
{
    if (pD3D12Resource)
    {
        ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(pD3D12Resource, D3D12_RESOURCE_STATE_COMMON, StateAfter, subResource));
    }
}

void ResourceStateTracker::TransitionResource(const Resource* pResource, D3D12_RESOURCE_STATES StateAfter, UINT subResource /* = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES */)
{
    if (pResource)
    {
        TransitionResource(pResource->GetD3D12Resource().Get(), StateAfter, subResource);
    }
}

void ResourceStateTracker::AliasBarrier(const Resource* pResourceBefore /* = nullptr */, const Resource* pResourceAfter /* = nullptr */)
{
    auto resourceBefore = pResourceBefore == nullptr ? nullptr : pResourceBefore->GetD3D12Resource().Get();
    auto resourceAfter = pResourceAfter == nullptr ? nullptr : pResourceAfter->GetD3D12Resource().Get();

    ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Aliasing(resourceBefore, resourceAfter));
}

void ResourceStateTracker::UavBarrier(const Resource* pResource)
{
    auto presource = pResource == nullptr ? nullptr : pResource->GetD3D12Resource().Get();

    ResourceBarrier(CD3DX12_RESOURCE_BARRIER::UAV(presource));
}

void ResourceStateTracker::FlushValidResourceBarrier(CommandList& commandList)
{
    if (!m_ValidResourceBarrier.empty())
    {
        commandList.GetGraphicsCommandList2()->ResourceBarrier(m_ValidResourceBarrier.size(), m_ValidResourceBarrier.data());
        m_ValidResourceBarrier.clear();
    }
}

UINT ResourceStateTracker::FlushPendingResourceBarrier(CommandList& commandList)
{
    assert(m_IsLock);

    std::vector<D3D12_RESOURCE_BARRIER> intermediateResourceBarrier;
    if (!m_PendingResourceBarrier.empty())
    {
        for (const auto& pendingBarrier : m_PendingResourceBarrier)
        {
            if (pendingBarrier.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)
            {
                auto pendingTransition = pendingBarrier.Transition;
                auto posIter = m_GlobalResourceState.find(pendingTransition.pResource);
                if (posIter != m_GlobalResourceState.end())
                {
                    //Check if the global resource state is same with stateAfter of pending barrier
                    if (pendingTransition.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES
                        && !posIter->second.subResourceState.empty())
                    {
                        auto resourceState = posIter->second;
                        for (const auto& subResState : resourceState.subResourceState)
                        {
                            if (subResState.second != pendingTransition.StateAfter)
                            {
                                D3D12_RESOURCE_BARRIER newBarrier = pendingBarrier;
                                newBarrier.Transition.StateBefore = subResState.second;
                                intermediateResourceBarrier.push_back(newBarrier);
                            }
                        }
                    }
                    else
                    {
                        auto finalState = posIter->second.GetSubResourceState(pendingTransition.Subresource);
                        if (finalState != pendingTransition.StateAfter)
                        {
                            D3D12_RESOURCE_BARRIER newBarrier = pendingBarrier;
                            newBarrier.Transition.StateBefore = finalState;
                            intermediateResourceBarrier.push_back(newBarrier);
                        }
                    }
                }
                else//As a debug state
                {
                    assert("Error!Resource is not existed in Global Resource State Map.Check if this resource is not inserted in map or the resource has been destroyed");
                }
            }
        }
    }

    UINT BarrierSize = (UINT)intermediateResourceBarrier.size();
    if (BarrierSize > 0)
    {  
        commandList.GetGraphicsCommandList2()->ResourceBarrier(intermediateResourceBarrier.size(), intermediateResourceBarrier.data());
    }
    m_PendingResourceBarrier.clear();

    return BarrierSize;
}

void ResourceStateTracker::CommitFinalResourceState()
{
    assert(m_IsLock);

    for (const auto& finalResourceState : m_FinalResourceState)
    {
        //We upate global resource state.
        //Here operator= is perfect for us.Since it will automatically update old resource state or add new resource state.
        m_GlobalResourceState[finalResourceState.first] = finalResourceState.second;
    }
    //Clear final resource state
    m_FinalResourceState.clear();
}

void ResourceStateTracker::AddGlobalResourceState(ID3D12Resource* pResource,D3D12_RESOURCE_STATES State)
{
    //Note: pointer state
    if (pResource)
    {
        std::lock_guard<std::mutex> lock(ms_GlobalMutex);
        m_GlobalResourceState[pResource].SetResourceState(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, State);
    }
}

void ResourceStateTracker::RemoveGlobalResourceState(ID3D12Resource* pResource)
{
    //Note: pointer state
    if (pResource)
    {
        std::lock_guard<std::mutex> lock(ms_GlobalMutex);
        m_GlobalResourceState.erase(pResource);
    }
}

void ResourceStateTracker::Lock()
{
    m_IsLock = true;
    ms_GlobalMutex.lock();
}

void ResourceStateTracker::UnLock()
{
    m_IsLock = false;
    ms_GlobalMutex.unlock();
}

void ResourceStateTracker::Reset()
{
    m_PendingResourceBarrier.clear();
    m_ValidResourceBarrier.clear();
    m_FinalResourceState.clear();
}