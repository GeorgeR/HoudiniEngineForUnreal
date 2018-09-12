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
#include "HoudiniEngineMaterialUtils.h"
#include "HoudiniRuntimeSettings.h"

#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "HoudiniAssetTask_Cook.h"

FHoudiniAssetInstanceData::FHoudiniAssetInstanceData(const UHoudiniAssetInstance* Instance)
{
    this->Asset = Instance->Asset;
    this->AssetName = Instance->AssetName;

    this->Parameters = Instance->Parameters;
    this->Inputs = Instance->Inputs;
    this->InstanceInputs = Instance->InstanceInputs;
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

void FHoudiniAssetInstanceData::GetParametersByName(TMap<FString, UHoudiniAssetParameter*>& ParametersByName)
{
    if (this->ParametersByName.Num() > 0)
        ParametersByName = this->ParametersByName;

    this->ParametersByName.Reserve(Parameters.Num());
    for (auto& Parameter : Parameters)
        this->ParametersByName.Add(Parameter->GetParameterName(), Parameter);

    ParametersByName = this->ParametersByName;
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
    if (bIsNewAsset)
    {
        this->Asset = InAsset;
        ClearPreviousData();

        SetAssetName(AssetName);

        return;
    }
}

void UHoudiniAssetInstance::SetAssetName(const FString& InAssetName)
{
    if (Asset == nullptr)
        return;

    auto PreviousData = GetOrCreatePreviousData();
    auto bIsNewName = !PreviousData->AssetNameIsSame(AssetName);

    // Refresh asset name list
    if (GetAssetNames(true).Num() <= 0)
        return;

    if (HasError())
        return;

    ensure(this->AssetNames.Num() == this->HAPIAssetNames.Num());

    // If the assetname is empty, just restore the previous
    if (InAssetName.IsEmpty())
        this->AssetName = PreviousData->AssetName.IsEmpty() ? this->AssetNames[0] : PreviousData->AssetName;

    // If the asset has changed and the named asset no longer exists, just set it to the first available
    if (auto FoundAsset = AssetNames.FindByPredicate([&](FString& Str) -> bool { return Str.Equals(this->AssetName, ESearchCase::IgnoreCase); }))
        this->AssetName = *FoundAsset;
    else
        this->AssetName = this->AssetNames[0];

    ClearPreviousData();
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
        On_Instantiated();
    });

    auto TaskScheduler = FHoudiniEngine::Get().GetTaskScheduler();
    TaskScheduler->QueueTask(InstantiateTask);
}

const TArray<FString>& UHoudiniAssetInstance::GetAssetNames(bool bReload)
{
    if (this->AssetNames.Num() > 0 && !bReload)
        return this->AssetNames;

    AssetNames.Empty();
    HAPIAssetNames.Empty();

    HAPI_AssetLibraryId LibraryId = this->AssetLibraryId;
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

    if (GIsCookerLoadingPackage)
        return;

    if (this->HasAnyFlags(EObjectFlags::RF_ClassDefaultObject))
        return;

    if (this->Asset == nullptr)
        return;

    auto World = GetWorld();
    if ((World && (World->WorldType != EWorldType::Editor && World->WorldType != EWorldType::Inactive)) || !World)
        return;
    
    Clear(true);

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

    this->bIsInstantiated = true;

    if (this->OnInstantiated.IsBound())
        OnInstantiated.Broadcast();

    if (this->OnInstantiatedBP.IsBound())
        OnInstantiatedBP.Broadcast();

    TSharedPtr<FHoudiniAssetTask_Cook> CookTask = MakeShareable(new FHoudiniAssetTask_Cook(this));
    CookTask->OnComplete.AddLambda([&](HAPI_NodeId AssetId) -> void
    {
        SetAssetId(AssetId);
        On_Cooked();
    });

    auto TaskScheduler = FHoudiniEngine::Get().GetTaskScheduler();
    TaskScheduler->QueueTask(CookTask);
}

void UHoudiniAssetInstance::On_Cooked()
{
    ensure(IsValidAssetId());

    CreateOutputs();
    CreateAttributes();

    this->bIsCooked = true;

    if (this->OnCooked.IsBound())
        OnCooked.Broadcast();

    if (this->OnCookedBP.IsBound())
        OnCookedBP.Broadcast();
}

void UHoudiniAssetInstance::Clear(bool bKeepNames)
{
    LastError.Empty();
    if (!bKeepNames) 
    {
        AssetNames.Empty();
        HAPIAssetNames.Empty();
    }

    bIsInstantiated = false;

    Parameters.Empty();
    ParametersById.Empty();
    ParametersByName.Empty();

    Inputs.Empty();
    InstanceInputs.Empty();

    Outputs.Empty();

    //Attributes.Empty();

    //Outputs.Empty();

    AssetLibraryId = -1;
    AssetId = -1;
}

bool UHoudiniAssetInstance::CreateParameters()
{
    auto Result = HAPI_RESULT_SUCCESS;

    Parameters.Empty();

    if (!IsValidAssetId())
        return false;

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

    auto PreviousData = GetOrCreatePreviousData();

    TMap<FString, UHoudiniAssetParameter*> PreviousParameters;
    PreviousData->GetParametersByName(PreviousParameters);

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
            if (const auto ParentInfoPtr = ParameterInfos.FindByPredicate(
            [=](const auto& Info) {
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
        auto FoundParameter = PreviousParameters.Find(ParameterName);

        if (FoundParameter && *FoundParameter)
        {
            if ((*FoundParameter)->GetTupleSize() == ParameterInfo.size)
            {
                auto FoundClass = (*FoundParameter)->GetClass();
                bool bFailedTypeCheck = !FHoudiniParameterFactory::CheckType(FoundClass);

                if (!bFailedTypeCheck)
                {
                    Parameter = *FoundParameter;
                    Parameters.Add(Parameter);
                    continue;
                }
            }
        }
    
        Parameter = FHoudiniParameterFactory::Create(ParameterInfo.type, this, nullptr, AssetInfo.nodeId, ParameterInfo);
        check(Parameter);
    
        Parameters.Add(Parameter);
        //ParametersById.Add(ParameterInfo.id, Parameter);
    }
    
    return true;
}

bool UHoudiniAssetInstance::CreateInputs()
{
    auto Result = HAPI_RESULT_SUCCESS;

    Inputs.Empty();

    if (!IsValidAssetId())
        return false;

    HAPI_AssetInfo AssetInfo;
    if (!GetAssetInfo(AssetInfo))
        return false;

    auto InputCount = AssetInfo.geoInputCount;
    for (auto i = 0; i < InputCount; i++)
        Inputs.Add(UHoudiniAssetInput::Create(this, i, AssetId));

    return true;
}

bool UHoudiniAssetInstance::CreateAttributes()
{
    

    return true;
}

/* Semi duplicated of CreateStaticMeshesFromHoudiniAsset, without mesh generation. */
/* NOTE: Split into factories? */
bool UHoudiniAssetInstance::CreateOutputs()
{
    Outputs.Empty();

    // TODO: Inputs/Outputs
    FHoudiniCookParams HoudiniCookParameters(Asset);
    FTransform ComponentTransform;
    bool bForceRecookAll;

    FlushRenderingCommands();

    const auto HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
    check(HoudiniRuntimeSettings);

    float GeneratedGeometryScaleFactor = HAPI_UNREAL_SCALE_FACTOR_POSITION;
    EHoudiniRuntimeSettingsAxisImport ImportAxis = HRSAI_Unreal;

    // Attribute marshaling names.
    std::string MarshallingAttributeNameLightmapResolution = HAPI_UNREAL_ATTRIB_LIGHTMAP_RESOLUTION;
    std::string MarshallingAttributeNameMaterial = HAPI_UNREAL_ATTRIB_MATERIAL;
    std::string MarshallingAttributeNameMaterialFallback = HAPI_UNREAL_ATTRIB_MATERIAL_FALLBACK;
    std::string MarshallingAttributeNameMaterialInstance = HAPI_UNREAL_ATTRIB_MATERIAL_INSTANCE;
    std::string MarshallingAttributeNameFaceSmoothingMask = HAPI_UNREAL_ATTRIB_FACE_SMOOTHING_MASK;

    // Group name prefix used for collision geometry generation.
    FString CollisionGroupNamePrefix = TEXT("collision_geo");
    FString RenderedCollisionGroupNamePrefix = TEXT("rendered_collision_geo");
    FString UCXCollisionGroupNamePrefix = TEXT("collision_geo_ucx");
    FString UCXRenderedCollisionGroupNamePrefix = TEXT("rendered_collision_geo_ucx");
    FString SimpleCollisionGroupNamePrefix = TEXT("collision_geo_simple");
    FString SimpleRenderedCollisionGroupNamePrefix = TEXT("rendered_collision_geo_simple");

    // Add runtime setting for those?
    FString LodGroupNamePrefix = TEXT("lod");
    FString SocketGroupNamePrefix = TEXT("socket");

    if (HoudiniRuntimeSettings)
    {
        GeneratedGeometryScaleFactor = HoudiniRuntimeSettings->GeneratedGeometryScaleFactor;
        ImportAxis = HoudiniRuntimeSettings->ImportAxis;

        if (!HoudiniRuntimeSettings->MarshallingAttributeLightmapResolution.IsEmpty())
            FHoudiniEngineUtils::ConvertUnrealString(HoudiniRuntimeSettings->MarshallingAttributeLightmapResolution, MarshallingAttributeNameLightmapResolution);

        if (!HoudiniRuntimeSettings->MarshallingAttributeMaterial.IsEmpty())
            FHoudiniEngineUtils::ConvertUnrealString(HoudiniRuntimeSettings->MarshallingAttributeMaterial, MarshallingAttributeNameMaterial);

        if (!HoudiniRuntimeSettings->MarshallingAttributeFaceSmoothingMask.IsEmpty())
            FHoudiniEngineUtils::ConvertUnrealString(HoudiniRuntimeSettings->MarshallingAttributeFaceSmoothingMask, MarshallingAttributeNameFaceSmoothingMask);

        if (!HoudiniRuntimeSettings->CollisionGroupNamePrefix.IsEmpty())
            CollisionGroupNamePrefix = HoudiniRuntimeSettings->CollisionGroupNamePrefix;

        if (!HoudiniRuntimeSettings->RenderedCollisionGroupNamePrefix.IsEmpty())
            RenderedCollisionGroupNamePrefix = HoudiniRuntimeSettings->RenderedCollisionGroupNamePrefix;

        if (!HoudiniRuntimeSettings->UCXCollisionGroupNamePrefix.IsEmpty())
            UCXCollisionGroupNamePrefix = HoudiniRuntimeSettings->UCXCollisionGroupNamePrefix;

        if (!HoudiniRuntimeSettings->UCXRenderedCollisionGroupNamePrefix.IsEmpty())
            UCXRenderedCollisionGroupNamePrefix = HoudiniRuntimeSettings->UCXRenderedCollisionGroupNamePrefix;

        if (!HoudiniRuntimeSettings->SimpleCollisionGroupNamePrefix.IsEmpty())
            SimpleCollisionGroupNamePrefix = HoudiniRuntimeSettings->SimpleCollisionGroupNamePrefix;

        if (!HoudiniRuntimeSettings->SimpleRenderedCollisionGroupNamePrefix.IsEmpty())
            SimpleRenderedCollisionGroupNamePrefix = HoudiniRuntimeSettings->SimpleRenderedCollisionGroupNamePrefix;
    }

    // Get platform manager LOD specific information.
    auto CurrentPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
    check(CurrentPlatform);

    const FStaticMeshLODGroup& LODGroup = CurrentPlatform->GetStaticMeshLODSettings().GetLODGroup(NAME_None);
    int32 DefaultNumLODs = LODGroup.GetDefaultNumLODs();

    HAPI_AssetInfo AssetInfo;
    if (!GetAssetInfo(AssetInfo)) return false;

    FTransform AssetTransform;
    if (!FHoudiniEngineUtils::HapiGetAssetTransform(AssetId, AssetTransform)) return false;
    ComponentTransform = AssetTransform;

    TArray<HAPI_ObjectInfo> ObjectInfos;
    if (!FHoudiniEngineUtils::HapiGetObjectInfos(AssetId, ObjectInfos)) return false;
    const auto ObjectCount = ObjectInfos.Num();

    TArray<HAPI_Transform> ObjectTransforms;
    if (!FHoudiniEngineUtils::HapiGetObjectTransforms(AssetId, ObjectTransforms)) return false;

    TSet<HAPI_NodeId> UniqueMaterialIds;
    TSet<HAPI_NodeId> UniqueInstancerMaterialIds;
    TMap<FHoudiniGeoPartObject, HAPI_NodeId> InstancerMaterialMap;
    FHoudiniEngineUtils::ExtractUniqueMaterialIds(AssetInfo, UniqueMaterialIds, UniqueInstancerMaterialIds, InstancerMaterialMap);

    TMap<FString, UMaterialInterface*> Materials;
    FHoudiniEngineMaterialUtils::HapiCreateMaterials(AssetId, HoudiniCookParameters, AssetInfo, UniqueMaterialIds, UniqueInstancerMaterialIds, Materials, bForceRecookAll);

    HoudiniCookParameters.HoudiniCookManager->ClearAssignmentMaterials();
    for (const auto& Pair : Materials)
        HoudiniCookParameters.HoudiniCookManager->AddAssignmentMaterial(Pair.Key, Pair.Value);

    auto Result = HAPI_RESULT_SUCCESS;

    for (auto i = 0; i < ObjectInfos.Num(); i++)
    {
        const HAPI_ObjectInfo& ObjectInfo = ObjectInfos[i];

        FString ObjectName = TEXT("");
        FHoudiniEngineString HoudiniEngineString(ObjectInfo.nameSH);
        HoudiniEngineString.ToFString(ObjectName);

        const HAPI_Transform& ObjectTransform = ObjectTransforms[i];
        FTransform TransformMatrix;
        FHoudiniEngineUtils::TranslateHapiTransform(ObjectTransform, TransformMatrix);

        auto EditableNodeCount = 0;

        Result = FHoudiniApi::ComposeChildNodeList(
            FHoudiniEngine::Get().GetSession(), ObjectInfo.nodeId,
            HAPI_NODETYPE_SOP, HAPI_NODEFLAGS_EDITABLE, true, &EditableNodeCount);

        if (Result != HAPI_RESULT_SUCCESS)
        {
            SetLastError(FHoudiniEngineUtils::GetErrorDescription());
            return false;
        }

        if (EditableNodeCount > 0)
        {
            TArray<HAPI_NodeId> EditableNodeIds;
            EditableNodeIds.SetNumUninitialized(EditableNodeCount);

            for (auto j = 0; j < EditableNodeCount; j++)
            {
                HAPI_GeoInfo EditableGeoInfo;
                Result = FHoudiniApi::GetGeoInfo(FHoudiniEngine::Get().GetSession(), EditableNodeIds[j], &EditableGeoInfo);

                if (Result != HAPI_RESULT_SUCCESS)
                {
                    SetLastError(FHoudiniEngineUtils::GetErrorDescription());
                    return false;
                }

                if (EditableGeoInfo.isDisplayGeo)
                    continue;

                // TODO: Handle other types
                if (EditableGeoInfo.type != HAPI_GEOTYPE_CURVE)
                    continue;

                FHoudiniGeoPartObject HoudiniGeoPartObject(
                    TransformMatrix, ObjectName, ObjectName, AssetId,
                    ObjectInfo.nodeId, EditableGeoInfo.nodeId, 0);

                HoudiniGeoPartObject.bIsVisible = ObjectInfo.isVisible;
                HoudiniGeoPartObject.bIsInstancer = false;
                HoudiniGeoPartObject.bIsEditable = EditableGeoInfo.isEditable;
                HoudiniGeoPartObject.bHasGeoChanged = EditableGeoInfo.hasGeoChanged;

                // TODO: Create static mesh output here
            }
        }

        HAPI_GeoInfo GeoInfo;
        Result = FHoudiniApi::GetDisplayGeoInfo(FHoudiniEngine::Get().GetSession(), ObjectInfo.nodeId, &GeoInfo);
        if (Result != HAPI_RESULT_SUCCESS)
            continue;

        TArray<FString> ObjectGeoGroupNames;
        FHoudiniEngineUtils::HapiGetGroupNames(AssetId, ObjectInfo.nodeId, GeoInfo.nodeId, 0, HAPI_GROUPTYPE_PRIM, ObjectGeoGroupNames, false);

        FKAggregateGeom AggregateCollisionGeo;
        bool bHasAggregateGeometryCollision = false;

        TArray<FTransform> SocketTransforms;
        TArray<FString> SocketNames;
        TArray<FString> SocketActors;
        TArray<FString> SocketTags;

        for (auto j = 0; j < GeoInfo.partCount; ++j)
        {
            HAPI_PartInfo PartInfo;
            Result = FHoudiniApi::GetPartInfo(FHoudiniEngine::Get().GetSession(), GeoInfo.nodeId, j, &PartInfo);
            if (Result != HAPI_RESULT_SUCCESS)
            {
                HOUDINI_LOG_MESSAGE(
                    TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d] unable to retrieve PartInfo - skipping."),
                    ObjectInfo.nodeId, *ObjectName, GeoInfo.nodeId, j);

                continue;
            }
            
            FString PartName = TEXT("");
            FHoudiniEngineString HoudiniEngineString(PartInfo.nameSH);
            HoudiniEngineString.ToFString(PartName);

            if (PartInfo.type == HAPI_PARTTYPE_INVALID)
                continue;

            FHoudiniGeoPartObject HoudiniGeoPartObject(
                TransformMatrix, ObjectName, PartName, AssetId,
                ObjectInfo.nodeId, GeoInfo.nodeId, PartInfo.id);

            HoudiniGeoPartObject.bIsVisible = ObjectInfo.isVisible && !PartInfo.isInstanced;
            HoudiniGeoPartObject.bIsInstancer = ObjectInfo.isInstancer;
            HoudiniGeoPartObject.bIsCurve = (PartInfo.type == HAPI_PARTTYPE_CURVE);
            HoudiniGeoPartObject.bIsEditable = GeoInfo.isEditable;
            HoudiniGeoPartObject.bHasGeoChanged = GeoInfo.hasGeoChanged;
            HoudiniGeoPartObject.bIsBox = (PartInfo.type == HAPI_PARTTYPE_BOX);
            HoudiniGeoPartObject.bIsSphere = (PartInfo.type == HAPI_PARTTYPE_SPHERE);
            HoudiniGeoPartObject.bIsVolume = (PartInfo.type == HAPI_PARTTYPE_VOLUME);

            TArray<FString> GeneratedMeshNames;
            {
                HAPI_AttributeInfo AttributeInfo;
                FMemory::Memzero<HAPI_AttributeInfo>(AttributeInfo);
                std::string MarshallingAttributeName = HAPI_UNREAL_ATTRIB_GENERATED_MESH_NAME;

                if (HoudiniRuntimeSettings)
                {
                    FHoudiniEngineUtils::ConvertUnrealString(
                        HoudiniRuntimeSettings->MarshallingAttributeGeneratedMeshName,
                        MarshallingAttributeName);
                }

                FHoudiniEngineUtils::HapiGetAttributeDataAsString(
                    AssetId, ObjectInfo.nodeId, GeoInfo.nodeId, PartInfo.id,
                    MarshallingAttributeName.c_str(), AttributeInfo, GeneratedMeshNames);

                if (GeneratedMeshNames.Num() > 0)
                {
                    const FString& CustomPartName = GeneratedMeshNames[0];
                    if (!CustomPartName.IsEmpty())
                        HoudiniGeoPartObject.SetCustomName(CustomPartName);
                }
            }

            TArray<FString> BakeFolderOverrides;
            {
                HAPI_AttributeInfo AttributeInfo;
                FMemory::Memzero<HAPI_AttributeInfo>(AttributeInfo);

                FHoudiniEngineUtils::HapiGetAttributeDataAsString(
                    AssetId, ObjectInfo.nodeId, GeoInfo.nodeId, PartInfo.id,
                    HAPI_UNREAL_ATTRIB_BAKE_FOLDER, AttributeInfo, BakeFolderOverrides);

                if (BakeFolderOverrides.Num() > 0)
                {
                    const FString& BakeFolderOverride = BakeFolderOverrides[0];
                    if (!BakeFolderOverride.IsEmpty())
                        HoudiniCookParameters.BakeFolder = FText::FromString(BakeFolderOverride);
                }
            }

            // TODO:
            //AddMeshSocketToList(AssetId, ObjectInfo.nodeId, GeoInfo.nodeId, PartInfo.id, Sockets, SocketNames, SocketActors, SocketTags, PartInfo.isInstanced);

            if (PartInfo.type == HAPI_PARTTYPE_INSTANCER)
            {
                HoudiniGeoPartObject.bIsVisible = ObjectInfo.isVisible && !ObjectInfo.isInstanced;
                HoudiniGeoPartObject.bIsInstancer = false;
                HoudiniGeoPartObject.bIsPackedPrimitiveInstancer = true;

                // TODO: Add staticmesh output
                continue;
            }
            else if (PartInfo.type == HAPI_PARTTYPE_VOLUME)
            {
                HoudiniGeoPartObject.bIsVisible = ObjectInfo.isVisible && !ObjectInfo.isInstanced;
                HoudiniGeoPartObject.bIsInstancer = false;
                HoudiniGeoPartObject.bIsPackedPrimitiveInstancer = false;
                HoudiniGeoPartObject.bIsVolume = true;

                HoudiniGeoPartObject.bHasGeoChanged = (GeoInfo.hasGeoChanged || bForceRecookAll);

                // TODO: Add output
                continue;
            }
            else if (PartInfo.type == HAPI_PARTTYPE_CURVE)
            {
                // TODO: Add output
                continue;
            }
            else if (!ObjectInfo.isInstancer && PartInfo.vertexCount <= 0)
            {
                if (HoudiniGeoPartObject.IsAttributeInstancer() || HoudiniGeoPartObject.IsAttributeOverrideInstancer())
                {
                    HoudiniGeoPartObject.bIsInstancer = true;
                    // TODO: Add output
                }
                continue;
            }

            if (PartInfo.vertexCount <= 0 && PartInfo.pointCount <= 0)
            {
                HOUDINI_LOG_MESSAGE(
                    TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] no points or vertices found - skipping."),
                    ObjectInfo.nodeId, *ObjectName, GeoInfo.nodeId, j, *PartName);
                continue;
            }

            if (ObjectInfo.isInstancer && PartInfo.pointCount <= 0)
            {
                HOUDINI_LOG_MESSAGE(
                    TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] is instancer but has 0 points - skipping."),
                    ObjectInfo.nodeId, *ObjectName, GeoInfo.nodeId, j, *PartName);

                continue;
            }

            TArray<HAPI_NodeId> PartFaceMaterialIds;
            HAPI_Bool bSingleFaceMaterial = false;
            bool bPartHasMaterials = false;
            bool bMaterialsChanged = false;

            if (PartInfo.faceCount > 0)
            {
                PartFaceMaterialIds.SetNumUninitialized(PartInfo.faceCount);

                Result = FHoudiniApi::GetMaterialNodeIdsOnFaces(
                    FHoudiniEngine::Get().GetSession(), GeoInfo.nodeId, PartInfo.id,
                    &bSingleFaceMaterial, &PartFaceMaterialIds[0], 0, PartInfo.faceCount);

                if (Result != HAPI_RESULT_SUCCESS)
                {
                    HOUDINI_LOG_MESSAGE(
                        TEXT("Creating Static Meshes: Object [%d %s], Geo [%d], Part [%d %s] unable to retrieve material face assignments, ")
                        TEXT("- skipping."),
                        ObjectInfo.nodeId, *ObjectName, GeoInfo.nodeId, j, *PartName);
                    continue;
                }

                TArray<HAPI_NodeId> PartUniqueMaterialIds;
                for (auto k = 0; k < PartFaceMaterialIds.Num(); ++k)
                    PartUniqueMaterialIds.AddUnique(PartFaceMaterialIds[k]);

                PartUniqueMaterialIds.RemoveSingle(-1);
                bPartHasMaterials = PartUniqueMaterialIds.Num() > 0;

                if (bPartHasMaterials)
                {
                    for (auto k = 0; k < PartUniqueMaterialIds.Num(); ++k)
                    {
                        HAPI_MaterialInfo MaterialInfo;
                        Result = FHoudiniApi::GetMaterialInfo(FHoudiniEngine::Get().GetSession(), PartUniqueMaterialIds[k], &MaterialInfo);
                        if (Result != HAPI_RESULT_SUCCESS)
                        {
                            SetLastError(FHoudiniEngineUtils::GetErrorDescription());
                            continue;
                        }
                        
                        if (MaterialInfo.hasChanged)
                        {
                            bMaterialsChanged = true;
                            break;
                        }
                    }
                }
            }

            if (ObjectInfo.isInstancer && PartInfo.pointCount > 0)
            {
                const HAPI_NodeId* FoundInstancerMaterialId = InstancerMaterialMap.Find(HoudiniGeoPartObject);
                if (FoundInstancerMaterialId)
                {
                    HAPI_NodeId InstancerMaterialId = *FoundInstancerMaterialId;
                    FString InstancerMaterialShopName = TEXT("");
                    if (InstancerMaterialId > -1 && FHoudiniEngineMaterialUtils::GetUniqueMaterialShopName(AssetId, InstancerMaterialId, InstancerMaterialShopName))
                    {
                        HoudiniGeoPartObject.bInstancerMaterialAvailable = true;
                        HoudiniGeoPartObject.InstancerMaterialName = InstancerMaterialShopName;
                    }
                }

                {
                    HAPI_AttributeInfo AttributeInfo;
                    FMemory::Memset<HAPI_AttributeInfo>(AttributeInfo, 0);

                    TArray<FString> InstancerAttributeMaterials;

                    FHoudiniEngineUtils::HapiGetAttributeDataAsString(
                        AssetId, ObjectInfo.nodeId, GeoInfo.nodeId,
                        PartInfo.id, MarshallingAttributeNameMaterial.c_str(), AttributeInfo, 
                        InstancerAttributeMaterials);

                    if (AttributeInfo.exists && InstancerAttributeMaterials.Num() > 0)
                    {
                        const FString& InstancerAttributeMaterialName = InstancerAttributeMaterials[0];
                        if (!InstancerAttributeMaterialName.IsEmpty())
                        {
                            HoudiniGeoPartObject.bInstancerAttributeMaterialAvailable = true;
                            HoudiniGeoPartObject.InstancerAttributeMaterialName = InstancerAttributeMaterialName;
                        }
                    }
                }

                // TODO: Add output
                continue;
            }

            // Mesh generation goes here
        }
    }

    return true;
}