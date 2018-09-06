#pragma once

#include "CoreMinimal.h"

#include "HoudiniAsset.h"
#include "HoudiniApi.h"
#include <DelegateCombinations.h>

#include "HoudiniAssetInstance.generated.h"

class UHoudiniAssetParameter;

struct FHoudiniAssetInstanceData
{
public:
    UHoudiniAsset * Asset;
    FString AssetName;
    TMap<HAPI_ParmId, UHoudiniAssetParameter*> Parameters;

    FHoudiniAssetInstanceData(const UHoudiniAssetInstance* Instance);

    bool AssetIsSame(const UHoudiniAsset* Other);
    bool AssetNameIsSame(const FString& Other);
};

UCLASS(BlueprintType)
class HOUDINIENGINERUNTIME_API UHoudiniAssetInstance
    : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Transient, Category = "Asset")
    bool bIsInstantiated;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInstantiated);
    UPROPERTY(BlueprintAssignable, Category = "Asset")
    FOnInstantiated OnInstantiated;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCooked);
    UPROPERTY(BlueprintAssignable, Category = "Asset")
    FOnCooked OnCooked;

    UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Asset")
    UHoudiniAsset* Asset;

    UFUNCTION(BlueprintGetter, Category = "Asset")
    const FString& GetAssetName() const { return AssetName; }

    UFUNCTION(BlueprintSetter, Category = "Asset")
    void SetAssetName(UPARAM(DisplayName = "AssetName") const FString& InAssetName);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Asset")
    const TArray<FString>& GetAssetNames(bool bReload = false);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Asset")
    FORCEINLINE int32 GetAssetId() const { return AssetId; }

    FORCEINLINE void SetAssetId(HAPI_NodeId AssetId) { this->AssetId = AssetId; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Asset")
    FORCEINLINE bool IsValidAssetId() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Parameters")
    const FORCEINLINE TArray<UHoudiniAssetParameter*>& GetParameters() const { return Parameters; }

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
    virtual void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

    virtual void PostLoad() override;

private:
    friend struct FHoudiniAssetInstanceData;

    UPROPERTY(EditAnywhere, BlueprintGetter = GetAssetName, BlueprintSetter = SetAssetName, Category = "Asset", meta = (AllowPrivateAccess))
    FString AssetName;

    UPROPERTY()
    FString LastError;

    UPROPERTY()
    TArray<FString> AssetNames;
    TArray<HAPI_StringHandle> HAPIAssetNames;

    HAPI_AssetLibraryId AssetLibraryId;
    HAPI_NodeId AssetId;
    
    UPROPERTY()
    TArray<UHoudiniAssetParameter*> Parameters;

    TMap<HAPI_ParmId, UHoudiniAssetParameter*> ParametersById;
    TMap<FString, UHoudiniAssetParameter*> ParametersByName;

    void Initialize(UHoudiniAsset* Asset, const FString& Name);

    void OnAssetNameSet(const FString& Name);

    void Clear(bool bKeepNames = false);

    bool CreateParameters();
    bool CreateInputs();
    bool CreateAttributes();

    bool GetAssetAndNodeInfo(const HAPI_NodeId& AssetId, HAPI_AssetInfo& AssetInfo, HAPI_NodeInfo& NodeInfo);
};