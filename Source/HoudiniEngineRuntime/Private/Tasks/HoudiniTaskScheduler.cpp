#include "HoudiniApi.h"

#include "HoudiniTaskScheduler.h"
#include "HoudiniAssetTask.h"
#include <Editor.h>

FHoudiniTaskScheduler::FHoudiniTaskScheduler()
{
    Start();
}

FHoudiniTaskScheduler::~FHoudiniTaskScheduler()
{
    TaskQueue.Empty();

    Stop();
}

void FHoudiniTaskScheduler::QueueTask(TSharedPtr<FHoudiniAssetTask> Task)
{
    TaskQueue.Enqueue(Task);
}

void FHoudiniTaskScheduler::Start(float TickRate /*= 0.25f*/)
{
    auto TimerManager = GEditor->GetTimerManager();
    if (TimerManager->IsTimerActive(TickHandle))
        return;
    
    FTimerDelegate TimerDelegate;
    TimerDelegate.BindRaw(this, &FHoudiniTaskScheduler::Tick);

    TimerManager->SetTimer(TickHandle, TimerDelegate, TickRate, true);
}

void FHoudiniTaskScheduler::Tick()
{
    if (!CurrentTask.IsValid() && IsEmpty() && (FPlatformTime::Seconds() - LastTask) > Timeout)
        Stop();

    if (!CurrentTask.IsValid())
    {
        if (TaskQueue.Dequeue(CurrentTask))
        {
            return;
        }
    }

    if (CurrentTask->GetState() == EHoudiniAssetTaskState::WaitingToRun)
        CurrentTask->Start();
}

void FHoudiniTaskScheduler::Stop()
{
    auto TimerManager = GEditor->GetTimerManager();
    TimerManager->ClearTimer(TickHandle);
}
