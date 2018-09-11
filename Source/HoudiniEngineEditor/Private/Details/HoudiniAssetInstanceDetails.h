/*
 * Copyright (c) <2017> Side Effects Software Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Produced by:
 *      Mykola Konyk
 *      Side Effects Software Inc
 *      123 Front Street West, Suite 1401
 *      Toronto, Ontario
 *      Canada   M5J 2M2
 *      416-504-9876
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "HoudiniGeoPartObject.h"
#include "DetailCategoryBuilder.h"
#include "IDetailCustomization.h"
#include "ContentBrowserDelegates.h"
#include "Materials/MaterialInterface.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SComboButton.h"
#include <SComboBox.h>


struct FGeometry;
struct FSlateBrush;
struct FPointerEvent;
struct FAssetData;

class UStaticMesh;
class IDetailLayoutBuilder;
class UHoudiniAssetInstance;
class ALandscape;

class FHoudiniAssetInstanceDetails : public IDetailCustomization
{
public:
    /** Constructor. **/
    FHoudiniAssetInstanceDetails();

    /** Destructor. **/
    virtual ~FHoudiniAssetInstanceDetails();

/** IDetailCustomization methods. **/
public:
    virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder ) override;
    virtual void CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& DetailBuilder) override;

    /** Create an instance of this detail layout class. **/
    static TSharedRef<IDetailCustomization> MakeInstance();

private:
    /** Helper method used to create widget for Houdini asset. **/
    void CreateAssetWidget(IDetailCategoryBuilder& DetailCategoryBuilder);

    void OnInstantiated();
    void OnCooked();

    FText GetLastErrorText() const;
    EVisibility GetLastErrorVisibility() const;
    EVisibility GetIsInstantiatedVisibility() const;

private:
    TWeakPtr<IDetailLayoutBuilder> CachedDetailBuilder;
    void RefreshDetailBuilder();

    /** Instance which is being customized. **/
    TWeakObjectPtr<UHoudiniAssetInstance> HoudiniAssetInstance;
    const bool HasValidInstance() const { return HoudiniAssetInstance.IsValid(); }
    //TArray<UHoudiniAssetInstance*> HoudiniAssetInstances;

    TAttribute<bool> HasValidAssetAttribute() const;

    void GenerateAssetNameList(TArray<TSharedPtr<FString>>& OutComboBoxStrings, TArray<TSharedPtr<SToolTip>>& OutToolTips, TArray<bool>& OutRestrictedItems);

    //FText GetAssetName() const;
    //TSharedRef<SWidget> GetAssetNames();
};
