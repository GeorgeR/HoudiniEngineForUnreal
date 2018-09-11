#include "HoudiniApi.h"
#include "HoudiniEngineRuntimePrivatePCH.h"

#include "HoudiniAssetInstance.h"

#include "HoudiniAsset.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngineString.h"
#include "HoudiniParameterFactory.h"

#include "Tasks/HoudiniTaskScheduler.h"
#include "Tasks/HoudiniAssetTask_Instantiate.h"

FHoudiniAssetInstanceData::FHoudiniAssetInstanceData(const UHoudiniAssetInstance* Instance)
{
    this->Asset = Instance->Asset;
    this->AssetName = Instance->AssetName;
    this->Parameters = Instance->ParametersById;
}

bool FHoudiniAssetInstanceData::AssetIsSame(const UHoudiniAsset* Other)
{
    if (this->Asset == nullptr)
        return false;

    return this->Asset->AssetFileName.Equals(Other->AssetFileName, ESearchCase::IgnoreCase);
}

bool FHoudiniAssetInstanceData::AssetNameIsSame(const FString& Other)
{
    if (this->AssetName.Len() <= 0 || Other.Len() <= 0)
        return false;

    return this->AssetName.Equals(Other, ESearchCase::IgnoreCase);
}

void UHoudiniAssetInstance::SetAsset(UHoudiniAsset* InAsset)
{
    if (InAsset == nullptr)
    {
        Clear();
        this->Asset = nullptr;
        return;
    }

    ensure(InAsset->GetAssetFileName().Len() > 0);

    auto PreviousData = GetOrCreatePreviousData();

    Clear(false);

    auto bIsNewAsset = !PreviousData->AssetIsSame(InAsset);
    auto bIsNewName = !PreviousData->AssetNameIsSame(AssetName);

    if (bIsNewAsset) this->Asset = InAsset;

    if (GetAssetNames(this->AssetLibraryId).Num() <= 0)
        return;

    auto PreviousOrNewAssetName = bIsNewName ? AssetName : PreviousData->AssetName;
    if (PreviousOrNewAssetName.Len() > 0)
    {
        if (auto FoundAsset = AssetNames.FindByPredicate([&](FString& Str) -> bool { return Str.Equals(PreviousOrNewAssetName, ESearchCase::IgnoreCase); }))
            this->AssetName = *FoundAsset;
        else
        {
            //SetLastError(TEXT("Asset with the name specified was not found in the input Asset."));
            this->AssetName = this->AssetNames[0];
        }
    }
    else
        this->AssetName = this->AssetNames[0];

    SetAssetName(this->AssetName);

    ClearPreviousData();
}

void UHoudiniAssetInstance::SetAssetName(const FString& InAssetName)
{
    if (Asset == nullptr)
        return;

    if (InAssetName.IsEmpty())
        return;

    ensure(this->AssetNames.Num() == this->HAPIAssetNames.Num());

    this->AssetName = InAssetName;

    Clear(true);

    auto NameIndex = this->AssetNames.IndexOfByKey(this->AssetName);
    if (NameIndex == INDEX_NONE)
    {
        this->AssetName.Empty();
        SetLastError(TEXT("Invalid AssetName."));
        return;
    }

    auto HAPIAssetName = this->HAPIAssetNames[NameIndex];

    TSharedPtr<FHoudiniAssetTask_Instantiate> InstantiateTask = MakeShareable(new FHoudiniAssetTask_Instantiate(this->Asset, HAPIAssetName));
    InstantiateTask->OnComplete.AddLambda([&](HAPI_NodeId AssetId) -> void
    {
        SetAssetId(AssetId);
        this->bIsInstantiated = true;

        On_Instantiated();
    });

    auto TaskScheduler = FHoudiniEngine::Get().GetTaskScheduler();
    TaskScheduler->QueueTask(InstantiateTask);
}

const TArray<FString>& UHoudiniAssetInstance::GetAssetNames(bool bReload)
{
    if (this->AssetNames.Num() > 0 && !bReload)
        return this->AssetNames;

    HAPI_AssetLibraryId LibraryId = -1;
    if (!this->Asset->GetAssetNames(LibraryId, HAPIAssetNames))
    {
        SetLastError(TEXT("Input Asset contains no AssetNames (it's empty)."));
        return this->AssetNames;
    }

    this->AssetLibraryId = LibraryId;

    for (HAPI_StringHandle& HAPIAssetName : HAPIAssetNames)
    {
        FString AssetNameStr;
        FHoudiniEngineString(HAPIAssetName).ToFString(AssetNameStr);
        this->AssetNames.Add(AssetNameStr);
    }

    return this->AssetNames;
}

FORCEINLINE bool UHoudiniAssetInstance::IsValidAssetId() const
{
    return FHoudiniEngineUtils::IsHoudiniNodeValid(AssetId);
}

bool UHoudiniAssetInstance::GetAssetInfo(HAPI_AssetInfo& AssetInfo)
{
    auto Result = HAPI_RESULT_SUCCESS;

    Result = FHoudiniApi::GetAssetInfo(FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo);
    if (Result != HAPI_RESULT_SUCCESS)
    {
        SetLastError(TEXT("Error getting AssetInfo"));
        return false;
    }

    return true;
}

bool UHoudiniAssetInstance::GetNodeInfo(HAPI_NodeInfo& NodeInfo)
{
    auto Result = HAPI_RESULT_SUCCESS;

    HAPI_AssetInfo AssetInfo;
    if (!GetAssetInfo(AssetInfo))
        return false;

    Result = FHoudiniApi::GetNodeInfo(FHoudiniEngine::Get().GetSession(), AssetInfo.nodeId, &NodeInfo);
    if (Result != HAPI_RESULT_SUCCESS)
    {
        LastError = TEXT("Error getting NodeInfo");
        return false;
    }

    return true;
}

UHoudiniAssetInstance* UHoudiniAssetInstance::Create(UObject* Outer, UHoudiniAsset* Asset, const FString& Name /*= TEXT("")*/)
{
    ensure(Outer);
    ensure(Asset);

    auto Result = NewObject<UHoudiniAssetInstance>(Outer);
    Result->SetAsset(Asset);
    Result->SetAssetName(Name);
    
    return Result;
}

// NOTE: Can be valid but be errored
bool UHoudiniAssetInstance::IsValid() const
{
    // TODO: add more validation
    return IsValidLowLevel()
        && !IsTemplate()
        && !IsPendingKillOrUnreachable()
        && this->Asset != nullptr
        && this->AssetName.Len() > 0
        && IsValidAssetId();
}

void UHoudiniAssetInstance::PostLoad()
{
    Super::PostLoad();

    if (this->HasAnyFlags(EObjectFlags::RF_ClassDefaultObject))
        return;

    if (this->Asset == nullptr)
        return;

    SetAsset(Asset);
    SetAssetName(AssetName);
}

#if WITH_EDITOR
void UHoudiniAssetInstance::PreEditChange(class FEditPropertyChain& PropertyAboutToChange)
{
    Super::PreEditChange(PropertyAboutToChange);
    
    GetOrCreatePreviousData(true);
}

void UHoudiniAssetInstance::PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent)
{
    Super::PostEditChangeChainProperty(PropertyChangedEvent);

    if (PropertyChangedEvent.GetPropertyName() == TEXT("Asset"))
        SetAsset(this->Asset);
    else if (PropertyChangedEvent.GetPropertyName() == TEXT("AssetName"))
        SetAssetName(this->AssetName);
}
#endif

TSharedPtr<FHoudiniAssetInstanceData> UHoudiniAssetInstance::GetOrCreatePreviousData(bool bForceNew /*= false*/)
{
    if (!PreviousData.IsValid() || bForceNew)
        PreviousData = MakeShareable(new FHoudiniAssetInstanceData(this));

    return PreviousData;
}

void UHoudiniAssetInstance::ClearPreviousData()
{
    if (PreviousData.IsValid())
        PreviousData.Reset();
}

void UHoudiniAssetInstance::SetLastError(const FString& Message)
{
    HOUDINI_LOG_ERROR(TEXT("%s"), *Message);
    LastError = Message;
}

void UHoudiniAssetInstance::On_Instantiated()
{
    ensure(IsValidAssetId());

    CreateParameters();
    CreateInputs();
    CreateAttributes();

    if (this->OnInstantiated.IsBound())
        OnInstantiated.Broadcast();

    if (this->OnInstantiatedBP.IsBound())
        OnInstantiatedBP.Broadcast();
}

void UHoudiniAssetInstance::On_Cooked()
{
    ensure(IsValidAssetId());

    if (this->OnCooked.IsBound())
        OnCooked.Broadcast();

    if (this->OnCookedBP.IsBound())
        OnCookedBP.Broadcast();
}

void UHoudiniAssetInstance::Clear(bool bKeepNames)
{
    LastError.Empty();
    if (!bKeepNames) AssetNames.Empty();

    ParametersById.Empty();
    ParametersByName.Empty();

    AssetLibraryId = -1;
    AssetId = -1;
}

bool UHoudiniAssetInstance::CreateParameters()
{
    auto Result = HAPI_RESULT_SUCCESS;

    HAPI_AssetInfo AssetInfo;
    if (!GetAssetInfo(AssetInfo))
        return false;

    HAPI_NodeInfo NodeInfo;
    if (!GetNodeInfo(NodeInfo))
        return false;

    if (NodeInfo.parmCount <= 0)
        return true;

    TArray<HAPI_ParmInfo> ParameterInfos;
    ParameterInfos.SetNumUninitialized(NodeInfo.parmCount);
    Result = FHoudiniApi::GetParameters(FHoudiniEngine::Get().GetSession(), AssetInfo.nodeId, &ParameterInfos[0], 0, NodeInfo.parmCount);
    if (Result != HAPI_RESULT_SUCCESS) return false;
    
    // TODO: Attempt to remap previous parameters
    for (auto i = 0; i < NodeInfo.parmCount; ++i)
    {
        const auto& ParameterInfo = ParameterInfos[i];
    
        if (ParameterInfo.id < 0 || ParameterInfo.childIndex < 0)
            continue;
            
        if(ParameterInfo.invisible)
            continue;
    
        bool bSkipParameter = false;
        auto ParentId = ParameterInfo.parentId;
        while (ParentId > 0 && !bSkipParameter)
        {
            if (const auto ParentInfoPtr = ParameterInfos.FindByPredicate([=](const auto& Info) {
                return Info.id == ParentId;
            }))
            {
                if (ParentInfoPtr->invisible && ParentInfoPtr->type == HAPI_PARMTYPE_FOLDER)
                    bSkipParameter = true;
    
                ParentId = ParentInfoPtr->parentId;
            }
            else
                bSkipParameter = true;
        }
    
        if (bSkipParameter)
            continue;
    
        UHoudiniAssetParameter* Parameter = nullptr;
        FString ParameterName;
        FHoudiniEngineString(ParameterInfo.nameSH).ToFString(ParameterName);
        auto FoundParameter = ParametersByName.Find(ParameterName);
    
        Parameter = FHoudiniParameterFactory::Create(ParameterInfo.type, this, nullptr, AssetInfo.nodeId, ParameterInfo);
        check(Parameter);
    
        Parameters.Add(Parameter);
        ParametersById.Add(ParameterInfo.id, Parameter);
    }
    
    TMap<FString, UHoudiniAssetParameter*> ParametersByName;
    ParametersByName.Reserve(ParametersById.Num());
    for (auto& Pair : ParametersById)
    {
        ParametersByName.Add(Pair.Value->GetParameterName(), Pair.Value);
    
        if (Pair.Value->HasChildParameters())
            Pair.Value->NotifyChildParametersCreated();
    }
    
    return true;
}

bool UHoudiniAssetInstance::CreateInputs()
{
    return true;
}

bool UHoudiniAssetInstance::CreateAttributes()
{
    return true;
}

bool UHoudiniAssetInstance::CreateOutputs()
{
    return true;
}