#pragma once

#include "CoreMinimal.h"

#include "HoudiniApi.h"
#include "Parameters/HoudiniAssetParameter.h"

class FHoudiniParameterFactory
{
public:
    typedef TFunction<UHoudiniAssetParameter*(UObject*, UHoudiniAssetParameter*, HAPI_NodeId, const HAPI_ParmInfo&)> FHoudiniParameterFactoryFunc;
    typedef TFunction<bool(UClass*)> FHoudiniParameterTypeCheckFunc;

    static UHoudiniAssetParameter* Create(HAPI_ParmType ParameterType, UObject* PrimaryObject, UHoudiniAssetParameter* ParentParameter, HAPI_NodeId NodeId, const HAPI_ParmInfo& ParameterInfo);

    template <HAPI_ParmType TParameterType>
    static UHoudiniAssetParameter* Create(UObject* PrimaryObject, UHoudiniAssetParameter* ParentParameter, HAPI_NodeId NodeId, const HAPI_ParmInfo& ParameterInfo);

    static bool CheckType(HAPI_ParmType ParameterType, UClass* Class);

    template <HAPI_ParmType TParameterType>
    static bool CheckType(UClass* Class);

private:
    static TMap<HAPI_ParmType, FHoudiniParameterFactoryFunc> Factories;
    static TMap<HAPI_ParmType, FHoudiniParameterTypeCheckFunc> TypeCheckers;
};
