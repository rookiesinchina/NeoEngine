#pragma once



#include "d3dx12.h"
#include <unordered_map>
#include <vector>
#include <mutex>


class CommandList;
class Resource;

class ResourceStateTracker
{
public:
    ResourceStateTracker();
    ~ResourceStateTracker();

    //Commite a barrier to interior structure.This function will automaticaaly check
    //the StateBefore and StateAfter of barrier.
    void ResourceBarrier(const D3D12_RESOURCE_BARRIER& barrier);
    //A transition wrapper function,this function will invoke ResourceBarrier() function.
    void TransitionResource(ID3D12Resource* pResource, D3D12_RESOURCE_STATES StateAfter, UINT subResource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
    void TransitionResource(const Resource* pResource, D3D12_RESOURCE_STATES StateAfter, UINT subResource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
    //A alias barrier wrapper function, this function will invoke ResourceBarrier() function.
    //@param: default parameters both are nullptr which indicates any resource in descriptor heap is alias.
    void AliasBarrier(const Resource* pResourceBefore = nullptr, const Resource* pResourceAfter = nullptr);
    //A Uav barrier wrapper function,this function will invoke ResourceBarrier() function.
    //@param: default paramters is default whichi indicates that any Uav will use barrier.
    void UavBarrier(const Resource* pResource = nullptr);
    //Flush all valid resource barrier to commandlist
    //
    void FlushValidResourceBarrier(CommandList& commandList);
    //Flush pengding resource barrier,this will check if other commandlists change resource state
    //If the state has been changed.We will use a commandlist to insert resource barrier into the middle of
    //other two commandlist.
    UINT FlushPendingResourceBarrier(CommandList& commandList);
    //Update global resource state for next commandlist or next frame.
    void CommitFinalResourceState();
    
    static void Lock();

    static void UnLock();
    //Reset all local barrier container to empty.
    void Reset();
    //Add a resource state to global resource state containers.
    //As long as a new resource is created, we must use this function to record resource state.
    static void AddGlobalResourceState(ID3D12Resource* pResource,D3D12_RESOURCE_STATES State);
    //Remove a resource state from global resource state containers.
    //As long as a resource is destroyed, we must use this function to erase resource state.
    static void RemoveGlobalResourceState(ID3D12Resource* pResource);

protected:

private:
    struct ResourceState
    {
        explicit ResourceState(D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON)
            : State(state) {};


        void SetResourceState(UINT subResource, D3D12_RESOURCE_STATES state)
        {
            if (subResource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
            {
                State = state;
                subResourceState.clear();
            }
            else
            {
                subResourceState[subResource] = state;
            }
        }
        
        D3D12_RESOURCE_STATES GetSubResourceState(UINT subResource)const
        {
            D3D12_RESOURCE_STATES state = State;
            auto pos = subResourceState.find(subResource);
            if (pos != subResourceState.end())
            {
                state = pos->second;
            }
            return state;
        }


        D3D12_RESOURCE_STATES State;
        //if subresource state is empty,that means all subresources have same state.
        std::unordered_map<UINT, D3D12_RESOURCE_STATES> subResourceState;
    };
    //a pending resource barrier vector container.
    //This variable will be used to verify if need to add a barrier between two commandlist
    //We need to reset this variable after every flush.
    std::vector<D3D12_RESOURCE_BARRIER> m_PendingResourceBarrier;
    //a valid resource barrier which means these barrier can commite to commandlist directly.
    //We need to reset this variable after every flush
    std::vector<D3D12_RESOURCE_BARRIER> m_ValidResourceBarrier;
    //a local resource state container which will be used to track resource state.
    std::unordered_map<ID3D12Resource*, ResourceState> m_FinalResourceState;
    //a global resource state container which stores all resources states.
    static std::unordered_map<ID3D12Resource*, ResourceState> m_GlobalResourceState;
    //some static variable to control thread.
    static std::mutex ms_GlobalMutex;
    static bool m_IsLock;
};
