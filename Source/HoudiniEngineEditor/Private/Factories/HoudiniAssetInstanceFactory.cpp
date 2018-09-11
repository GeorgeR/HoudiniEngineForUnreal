#include "HoudiniApi.h"
#include "HoudiniAssetInstanceFactory.h"
#include "HoudiniAssetInstance.h"

UHoudiniAssetInstanceFactory::UHoudiniAssetInstanceFactory()
{
    SupportedClass = UHoudiniAssetInstance::StaticClass();
    bCreateNew = true;
    bEditAfterNew = true;
}

UObject* UHoudiniAssetInstanceFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName,
                                              EObjectFlags Flags, UObject* Context,
                                              FFeedbackContext* Warn)
{
    check(InClass->IsChildOf(UHoudiniAssetInstance::StaticClass()));

	auto Result = NewObject<UHoudiniAssetInstance>(InParent, InName, Flags | RF_Transactional);
	
    return Result;
}
