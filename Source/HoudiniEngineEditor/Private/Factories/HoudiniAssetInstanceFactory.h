#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"

#include "HoudiniAssetInstanceFactory.generated.h"

UCLASS()
class UHoudiniAssetInstanceFactory : public UFactory
{
    GENERATED_BODY()

public:
    UHoudiniAssetInstanceFactory();

    virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName,
                                      EObjectFlags Flags, UObject* Context,
                                      FFeedbackContext* Warn) override;

    FORCEINLINE virtual bool ShouldShowInNewMenu() const override { return true; }
};
