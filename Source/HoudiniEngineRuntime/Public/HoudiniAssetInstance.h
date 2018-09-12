#pragma once

#include "CoreMinimal.h"

#include "HoudiniAsset.h"
#include "HoudiniApi.h"
#include <DelegateCombinations.h>

#include "HoudiniAssetInstance.generated.h"

class UHoudiniAssetInput;
class UHoudiniAssetInstanceInput;
class UHoudiniAssetParameter;
class UHoudiniOutput;

struct FHoudiniAssetInstanceData
{
public:
    UHoudiniAsset * Asset;
    FString AssetName;
    TArray<UHoudiniAssetParameter*> Parameters;
    TArray<UHoudiniAssetInput*> Inputs;
    TArray<UHoudiniAssetInstanceInput*> InstanceInputs;

    FHoudiniAssetInstanceData(const UHoudiniAssetInstance* Instance);

    bool AssetIsSame(const UHoudiniAsset* Other);
    bool AssetNameIsSame(const FString& Other);

    void GetParametersByName(TMap<FString, UHoudiniAssetParameter*>& ParametersByName);

private:
    TMap<FString, UHoudiniAssetParameter*> ParametersByName;
};

UCLASS(BlueprintType)
class HOUDINIENGINERUNTIME_API UHoudiniAssetInstance
    : public UObject
{
    GENERATED_BODY()

private:
    UPROPERTY(EditAnywhere, BlueprintGetter = GetAsset, BlueprintSetter = SetAsset, Category = "Asset", meta = (AllowPrivateAccess))
    UHoudiniAsset* Asset;

    UPROPERTY(EditAnywhere, BlueprintGetter = GetAssetName, BlueprintSetter = SetAssetName, Category = "Asset", meta = (AllowPrivateAccess))
    FString AssetName;

public:
    UPROPERTY(BlueprintReadOnly, Transient, Category = "Asset")
    bool bIsInstantiated;

    DECLARE_MULTICAST_DELEGATE(FOnCleared);
    FOnCleared& GetOnCleared() { return OnCleared; }

    DECLARE_MULTICAST_DELEGATE(FOnInstantiated);
    FOnInstantiated& GetOnInstantiated() { return OnInstantiated; }

    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInstantiatedBP);
    UPROPERTY(BlueprintAssignable, Category = "Asset", meta = (DisplayName = "OnInstantiated"))
    FOnInstantiatedBP OnInstantiatedBP;

    UPROPERTY(BlueprintReadOnly, Transient, Category = "Asset")
    bool bIsCooked;

    DECLARE_MULTICAST_DELEGATE(FOnCooked);
    FOnCooked& GetOnCooked() { return OnCooked; }

    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCookedBP);
    UPROPERTY(BlueprintAssignable, Category = "Asset", meta = (DisplayName = "OnCooked"))
    FOnCookedBP OnCookedBP;

    UFUNCTION(BlueprintGetter, Category = "Asset")
    UHoudiniAsset* GetAsset() const { return Asset; }

    UFUNCTION(BlueprintSetter, Category = "Asset")
    void SetAsset(UPARAM(DisplayName = "Asset") UHoudiniAsset* InAsset);

    UFUNCTION(BlueprintGetter, Category = "Asset")
    const FORCEINLINE FString& GetAssetName() const { return AssetName; }

    UFUNCTION(BlueprintSetter, Category = "Asset")
    void SetAssetName(UPARAM(DisplayName = "AssetName") const FString& InAssetName);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Asset")
    const TArray<FString>& GetAssetNames(bool bReload = false);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HAPI")
    FORCEINLINE int32 GetAssetId() const { return AssetId; }

    FORCEINLINE void SetAssetId(HAPI_NodeId AssetId) { this->AssetId = AssetId; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HAPI")
    FORCEINLINE bool IsValidAssetId() const;

    bool GetAssetInfo(HAPI_AssetInfo& AssetInfo);

    bool GetNodeInfo(HAPI_NodeInfo& NodeInfo);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parameters")
    const FORCEINLINE TArray<UHoudiniAssetParameter*>& GetParameters() const { return Parameters; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inputs")
    const FORCEINLINE TArray<UHoudiniAssetInput*>& GetInputs() const { return Inputs; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inputs")
    const FORCEINLINE TArray<UHoudiniAssetInstanceInput*>& GetInstanceInputs() const { return InstanceInputs; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Outputs")
    const FORCEINLINE TArray<UHoudiniOutput*>& GetOutputs() const { return Outputs; }

    // TODO: GetInputs, GetAttributes, GetOutputs

    UFUNCTION(BlueprintCallable, Category = "Creation")
    static UHoudiniAssetInstance* Create(UObject* Outer, UPARAM(DisplayName = "Asset") UHoudiniAsset* InAsset, const FString& Name = TEXT(""));

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Debug")
    FORCEINLINE bool HasError() const { return LastError.Len() > 0; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Debug")
    FORCEINLINE FString GetLastError() const { return LastError; }

    // TODO: Get notified on asset changed/reloaded

    /* This checks instance validity, EXCLUDING any errors (use HasError()). */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Debug")
    bool IsValid() const;

#if WITH_EDITOR
    virtual void PreEditChange(class FEditPropertyChain& PropertyAboutToChange) override;
    virtual void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

    virtual void PostLoad() override;

private:
    friend struct FHoudiniAssetInstanceData;

    FOnCleared OnCleared;
    FOnInstantiated OnInstantiated;
    FOnCooked OnCooked;
    
    TSharedPtr<FHoudiniAssetInstanceData> PreviousData;
    TSharedPtr<FHoudiniAssetInstanceData> GetOrCreatePreviousData(bool bForceNew = false);
    void ClearPreviousData();

    UPROPERTY()
    FString LastError;

    void SetLastError(const FString& Message);

    UPROPERTY()
    TArray<FString> AssetNames;
    TArray<HAPI_StringHandle> HAPIAssetNames;

    HAPI_AssetLibraryId AssetLibraryId;
    HAPI_NodeId AssetId;
    
    TArray<UHoudiniAssetInput*> Inputs;
    TArray<UHoudiniAssetInstanceInput*> InstanceInputs;

    TArray<UHoudiniAssetParameter*> Parameters;
    TMap<HAPI_ParmId, UHoudiniAssetParameter*> ParametersById;
    TMap<FString, UHoudiniAssetParameter*> ParametersByName;

    TArray<UHoudiniOutput*> Outputs;

    void On_Instantiated();
    void On_Cooked();

    void Clear(bool bKeepNames = false);

    bool CreateParameters();
    bool CreateInputs();
    bool CreateAttributes();
    bool CreateOutputs();

    bool GetAssetAndNodeInfo(const HAPI_NodeId& AssetId, HAPI_AssetInfo& AssetInfo, HAPI_NodeInfo& NodeInfo);
};