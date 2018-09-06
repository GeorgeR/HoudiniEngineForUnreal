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
#include "HoudiniAssetInput.h"
#include "HoudiniAssetInstanceInput.h"
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
FHoudiniParameterDetails::CreateWidgetChoice(IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetParameterChoice& InParam)
{
    FDetailWidgetRow & Row = LocalDetailCategoryBuilder.AddCustomRow(FText::GetEmpty());

    // Create the standard parameter name widget.
    CreateNameWidget(&InParam, Row, true);

    TSharedRef< SHorizontalBox > HorizontalBox = SNew(SHorizontalBox);
    TSharedPtr< SComboBox< TSharedPtr< FString > > > ComboBox;

    TWeakObjectPtr<UHoudiniAssetParameterChoice> MyParam(&InParam);

    HorizontalBox->AddSlot().Padding(2, 2, 5, 2)
    [
        SAssignNew(ComboBox, SComboBox<TSharedPtr<FString>>)
        .OptionsSource(&InParam.StringChoiceLabels)
        .InitiallySelectedItem(InParam.StringChoiceLabels[InParam.CurrentValue])
        .OnGenerateWidget(SComboBox< TSharedPtr< FString > >::FOnGenerateWidget::CreateLambda(
            [](TSharedPtr< FString > ChoiceEntry) {
                FText ChoiceEntryText = FText::FromString(*ChoiceEntry);
                    return SNew(STextBlock)
                        .Text(ChoiceEntryText)
                        .ToolTipText(ChoiceEntryText)
                        .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));
            }))
            .OnSelectionChanged(SComboBox< TSharedPtr< FString > >::FOnSelectionChanged::CreateLambda(
                [=](TSharedPtr< FString > NewChoice, ESelectInfo::Type SelectType) {
                if (!NewChoice.IsValid() || !MyParam.IsValid())
                    return;
                MyParam->OnChoiceChange(NewChoice);
            }))
            [
                SNew(STextBlock)
                .Text(TAttribute< FText >::Create(TAttribute< FText >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetParameterChoice::HandleChoiceContentText)))
                .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
            ]
    ];

    if (ComboBox.IsValid())
        ComboBox->SetEnabled(!InParam.bIsDisabled);

    Row.ValueWidget.Widget = HorizontalBox;
    Row.ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
}

void
FHoudiniParameterDetails::CreateWidgetChoice(TSharedPtr< SVerticalBox > VerticalBox, class UHoudiniAssetParameterChoice& InParam)
{
    FText ParameterLabelText = FText::FromString(InParam.GetParameterLabel());
    FText ParameterTooltip = GetParameterTooltip(&InParam);

    TWeakObjectPtr<UHoudiniAssetParameterChoice> MyParam(&InParam);

    VerticalBox->AddSlot().Padding(2, 2, 2, 2)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().MaxWidth(80).Padding(7, 1, 0, 0).VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Text(ParameterLabelText)
            .ToolTipText(ParameterTooltip)
            .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
        ]
        + SHorizontalBox::Slot()
        [
            SNew(SComboBox<TSharedPtr<FString>>)
            .OptionsSource(&InParam.StringChoiceLabels)
            .InitiallySelectedItem(InParam.StringChoiceLabels[InParam.CurrentValue])
            .OnGenerateWidget(SComboBox< TSharedPtr< FString > >::FOnGenerateWidget::CreateLambda(
                [](TSharedPtr< FString > ChoiceEntry) {
                    FText ChoiceEntryText = FText::FromString(*ChoiceEntry);
                        return SNew(STextBlock)
                            .Text(ChoiceEntryText)
                            .ToolTipText(ChoiceEntryText)
                            .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));
                }))
                .OnSelectionChanged(SComboBox< TSharedPtr< FString > >::FOnSelectionChanged::CreateLambda(
                    [=](TSharedPtr< FString > NewChoice, ESelectInfo::Type SelectType) {
                        if (!NewChoice.IsValid() || !MyParam.IsValid())
                            return;
                        MyParam->OnChoiceChange(NewChoice);
                    }))
            [
                SNew(STextBlock)
                .Text(TAttribute< FText >::Create(TAttribute< FText >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetParameterChoice::HandleChoiceContentText)))
                .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
            ]
        ]
    ];
}
