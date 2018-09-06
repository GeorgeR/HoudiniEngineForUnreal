#pragma once

#include "CoreMinimal.h"
#include <Queue.h>
#include "TimerManager.h"

class FHoudiniAssetTask;

class FHoudiniTaskScheduler
{
public:
    FHoudiniTaskScheduler();
    virtual ~FHoudiniTaskScheduler();
    
    void QueueTask(TSharedPtr<FHoudiniAssetTask> Task);

    const FORCEINLINE bool IsEmpty() const { return TaskQueue.IsEmpty(); }

private:
    FTimerHandle TickHandle;
    TQueue<TSharedPtr<FHoudiniAssetTask>> TaskQueue;

    TSharedPtr<FHoudiniAssetTask> CurrentTask;
    float LastTask = -1.0f;
    float Timeout = 5.0f;

    virtual void Start(float TickRate = 0.25f);
    virtual void Tick();
    virtual void Stop();
};