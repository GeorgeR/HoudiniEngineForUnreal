#include "HoudiniApi.h"

#include "HoudiniAssetTask.h"

#include "UnrealEd.h"
#include "HoudiniEngine.h"

FHoudiniAssetTask::FHoudiniAssetTask()
{
    HapiGuid = FGuid::NewGuid();
    SetState(EHoudiniAssetTaskState::Created);
}

FORCEINLINE bool FHoudiniAssetTask::IsComplete() const
{
    auto State = GetState();
    return State == EHoudiniAssetTaskState::RanToCompletion
        || State == EHoudiniAssetTaskState::Faulted
        || State == EHoudiniAssetTaskState::Stopped;
}

FORCEINLINE bool FHoudiniAssetTask::IsFaulted() const
{
    return GetState() == EHoudiniAssetTaskState::Faulted;
}

FORCEINLINE bool FHoudiniAssetTask::IsWaiting() const
{
    auto State = GetState();
    return State == EHoudiniAssetTaskState::WaitingForActivation
        || State == EHoudiniAssetTaskState::WaitingForDependenciesToComplete
        || State == EHoudiniAssetTaskState::WaitingToRun
        || State == EHoudiniAssetTaskState::Created;
}

FORCEINLINE bool FHoudiniAssetTask::IsRunning() const
{
    return GetState() == EHoudiniAssetTaskState::Running;
}

void FHoudiniAssetTask::Start(float TickRate /*= 0.25f*/)
{
    ensure(GEditor);
    ensure(TickRate >= 0.0f);

    TSharedRef<FTimerManager> TimerManager = GEditor->GetTimerManager();

    FTimerDelegate TimerDelegate;
    TimerDelegate.BindRaw(this, &FHoudiniAssetTask::Tick);

    TimerManager->SetTimer(TickHandle, TimerDelegate, TickRate, true);
}

bool FHoudiniAssetTask::CheckDependencies()
{
    bool bHasDependencies = false;

    if (bHasDependencies)
        SetState(EHoudiniAssetTaskState::WaitingForDependenciesToComplete);
    else
    {
        SetState(EHoudiniAssetTaskState::WaitingToRun);
        if (OnDependenciesComplete.IsBound())
            OnDependenciesComplete.Broadcast();
    }
    
    return bHasDependencies;
}

void FHoudiniAssetTask::Invalidate()
{
    FHoudiniEngine::Get().RemoveTaskInfo(HapiGuid);
    HapiGuid.Invalidate();
}

void FHoudiniAssetTask::Notify(const FText& Message)
{
    if (OnNotify.IsBound())
        OnNotify.Broadcast(Message);
}

void FHoudiniAssetTask::Tick()
{
    if (bStopped == true || !HapiGuid.IsValid())
        return;

    if (bStop == true)
    {
        TSharedRef<FTimerManager> TimerManager = GEditor->GetTimerManager();
        TimerManager->ClearTimer(TickHandle);

        bStopped.AtomicSet(true);
        return;
    }
        
    if (!IsComplete() && !IsFaulted())
        SetState(EHoudiniAssetTaskState::Running);
}

void FHoudiniAssetTask::Stop()
{
    if (!bStopped)
    {
        bStop.AtomicSet(true);
        return;
    }
    
    if (!IsComplete() && !IsFaulted())
        SetState(EHoudiniAssetTaskState::Stopped);

    BroadcastResult();
}
