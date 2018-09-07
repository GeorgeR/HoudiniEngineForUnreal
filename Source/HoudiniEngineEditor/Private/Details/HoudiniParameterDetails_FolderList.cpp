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
*/

#include "HoudiniApi.h"
#include "HoudiniParameterDetails.h"
#include "HoudiniAssetComponentDetails.h"
#include "HoudiniEngineEditorPrivatePCH.h"

#include "HoudiniAssetComponent.h"
#include "Parameters/HoudiniAssetInput.h"
#include "Parameters/HoudiniAssetInstanceInput.h"
#include "HoudiniAssetInstanceInputField.h"

#include "Parameters/HoudiniAssetParameterButton.h"
#include "Parameters/HoudiniAssetParameterChoice.h"
#include "Parameters/HoudiniAssetParameterColor.h"
#include "Parameters/HoudiniAssetParameterFile.h"
#include "Parameters/HoudiniAssetParameterFolder.h"
#include "Parameters/HoudiniAssetParameterFolderList.h"
#include "Parameters/HoudiniAssetParameterFloat.h"
#include "Parameters/HoudiniAssetParameterInt.h"
#include "Parameters/HoudiniAssetParameterLabel.h"
#include "Parameters/HoudiniAssetParameterMultiparm.h"
#include "Parameters/HoudiniAssetParameterRamp.h"
#include "Parameters/HoudiniAssetParameterSeparator.h"
#include "Parameters/HoudiniAssetParameterString.h"
#include "Parameters/HoudiniAssetParameterToggle.h"

#include "HoudiniRuntimeSettings.h"

#include "Widgets/SNewFilePathPicker.h"

#include "CurveEditorSettings.h"
#include "DetailLayoutBuilder.h"
#include "Editor/SceneOutliner/Public/SceneOutlinerModule.h"
#include "Editor/SceneOutliner/Public/SceneOutlinerPublicTypes.h"
#include "Editor/UnrealEd/Public/AssetThumbnail.h"
#include "Editor/UnrealEd/Public/Layers/ILayers.h"
#include "Editor/PropertyEditor/Public/PropertyCustomizationHelpers.h"
#include "Editor/PropertyEditor/Private/PropertyNode.h"
#include "Editor/PropertyEditor/Private/SDetailsViewBase.h"
#include "EditorDirectories.h"
#include "Engine/Selection.h"
#include "Engine/SkeletalMesh.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Internationalization/Internationalization.h"
#include "IDetailGroup.h"
#include "Particles/ParticleSystemComponent.h"
#include "SCurveEditor.h"
#include "SAssetDropTarget.h"
#include "Sound/SoundBase.h"
#include "Math/UnitConversion.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Input/NumericUnitTypeInterface.inl"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SRotatorInputBox.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SUniformGridPanel.h"


#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

void
FHoudiniParameterDetails::CreateWidgetFolderList(IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetParameterFolderList& InParam)
{
    TWeakObjectPtr<UHoudiniAssetParameterFolderList> MyParam(&InParam);
    TSharedRef< SHorizontalBox > HorizontalBox = SNew(SHorizontalBox);

    LocalDetailCategoryBuilder.AddCustomRow(FText::GetEmpty())
    [
        SAssignNew(HorizontalBox, SHorizontalBox)
    ];

    for (int32 ParameterIdx = 0; ParameterIdx < InParam.ChildParameters.Num(); ++ParameterIdx)
    {
        UHoudiniAssetParameter * HoudiniAssetParameterChild = InParam.ChildParameters[ParameterIdx];
        if (HoudiniAssetParameterChild->IsA(UHoudiniAssetParameterFolder::StaticClass()))
        {
            FText ParameterLabelText = FText::FromString(HoudiniAssetParameterChild->GetParameterLabel());
            FText ParameterToolTip = GetParameterTooltip(HoudiniAssetParameterChild);

            HorizontalBox->AddSlot().Padding(0, 2, 0, 2)
            [
                    SNew(SButton)
                    .VAlign(VAlign_Center)
                    .HAlign(HAlign_Center)
                    .Text(ParameterLabelText)
                    .ToolTipText(ParameterToolTip)
                    .OnClicked(FOnClicked::CreateLambda([=]() {
                        if (MyParam.IsValid())
                        {
                            MyParam->ActiveChildParameter = ParameterIdx;
                            MyParam->OnParamStateChanged();
                        }
                        return FReply::Handled();
                    }))
            ];
        }
    }

    // Recursively create all child parameters.
    for (UHoudiniAssetParameter * ChildParam : InParam.ChildParameters)
        FHoudiniParameterDetails::CreateWidget(LocalDetailCategoryBuilder, ChildParam);

    if (InParam.ChildParameters.Num() > 1)
    {
        TSharedPtr< STextBlock > TextBlock;

        LocalDetailCategoryBuilder.AddCustomRow(FText::GetEmpty())
        [
            SAssignNew(TextBlock, STextBlock)
            .Text(FText::GetEmpty())
            .ToolTipText(FText::GetEmpty())
            .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
            .WrapTextAt(HAPI_UNREAL_DESIRED_ROW_FULL_WIDGET_WIDTH)
        ];

        if (TextBlock.IsValid())
            TextBlock->SetEnabled(!InParam.bIsDisabled);
    }
}
