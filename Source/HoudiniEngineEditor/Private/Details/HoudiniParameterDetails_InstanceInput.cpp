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
FHoudiniParameterDetails::CreateWidgetInstanceInput(IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetInstanceInput& InParam)
{
    TWeakObjectPtr<UHoudiniAssetInstanceInput> MyParam(&InParam);

    // Get thumbnail pool for this builder.
    IDetailLayoutBuilder & DetailLayoutBuilder = LocalDetailCategoryBuilder.GetParentLayout();
    TSharedPtr< FAssetThumbnailPool > AssetThumbnailPool = DetailLayoutBuilder.GetThumbnailPool();

    // Classes allowed by instanced inputs.
    const TArray< const UClass * > AllowedClasses = {
        UStaticMesh::StaticClass(), AActor::StaticClass(), UBlueprint::StaticClass(),
        USoundBase::StaticClass(), UParticleSystem::StaticClass(),  USkeletalMesh::StaticClass() };

    const int32 FieldCount = InParam.InstanceInputFields.Num();
    for (int32 FieldIdx = 0; FieldIdx < FieldCount; ++FieldIdx)
    {
        UHoudiniAssetInstanceInputField * HoudiniAssetInstanceInputField = InParam.InstanceInputFields[FieldIdx];
        const int32 VariationCount = HoudiniAssetInstanceInputField->InstanceVariationCount();

        for (int32 VariationIdx = 0; VariationIdx < VariationCount; VariationIdx++)
        {
            UObject * InstancedObject = HoudiniAssetInstanceInputField->GetInstanceVariation(VariationIdx);
            if (!InstancedObject)
            {
                HOUDINI_LOG_WARNING(TEXT("Null Object found for instance variation %d"), VariationIdx);
                continue;
            }

            // Create thumbnail for this object.
            TSharedPtr< FAssetThumbnail > StaticMeshThumbnail =
                MakeShareable(new FAssetThumbnail(InstancedObject, 64, 64, AssetThumbnailPool));
            TSharedRef< SVerticalBox > PickerVerticalBox = SNew(SVerticalBox);
            TSharedPtr< SHorizontalBox > PickerHorizontalBox = nullptr;
            TSharedPtr< SBorder > StaticMeshThumbnailBorder;

            FString FieldLabel = InParam.GetFieldLabel(FieldIdx, VariationIdx);

            IDetailGroup& DetailGroup = LocalDetailCategoryBuilder.AddGroup(FName(*FieldLabel), FText::FromString(FieldLabel));
            DetailGroup.AddWidgetRow()
                .NameContent()
                [
                    SNew(SSpacer)
                    .Size(FVector2D(250, 64))
                ]
            .ValueContent()
                .MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
                [
                    PickerVerticalBox
                ];
            TWeakObjectPtr<UHoudiniAssetInstanceInputField> InputFieldPtr(HoudiniAssetInstanceInputField);
            PickerVerticalBox->AddSlot().Padding(0, 2).AutoHeight()
                [
                    SNew(SAssetDropTarget)
                    .OnIsAssetAcceptableForDrop(SAssetDropTarget::FIsAssetAcceptableForDrop::CreateLambda(
                        [=](const UObject* Obj) {
                for (auto Klass : AllowedClasses)
                {
                    if (Obj && Obj->IsA(Klass))
                        return true;
                }
                return false;
            })
                    )
                .OnAssetDropped(SAssetDropTarget::FOnAssetDropped::CreateUObject(
                    &InParam, &UHoudiniAssetInstanceInput::OnStaticMeshDropped, HoudiniAssetInstanceInputField, FieldIdx, VariationIdx))
                [
                    SAssignNew(PickerHorizontalBox, SHorizontalBox)
                ]
                ];

            PickerHorizontalBox->AddSlot().Padding(0.0f, 0.0f, 2.0f, 0.0f).AutoWidth()
                [
                    SAssignNew(StaticMeshThumbnailBorder, SBorder)
                    .Padding(5.0f)
                .OnMouseDoubleClick(FPointerEventHandler::CreateUObject(
                    &InParam, &UHoudiniAssetInstanceInput::OnThumbnailDoubleClick, InstancedObject))
                [
                    SNew(SBox)
                    .WidthOverride(64)
                .HeightOverride(64)
                .ToolTipText(FText::FromString(InstancedObject->GetPathName()))
                [
                    StaticMeshThumbnail->MakeThumbnailWidget()
                ]
                ]
                ];

            StaticMeshThumbnailBorder->SetBorderImage(TAttribute< const FSlateBrush * >::Create(
                TAttribute< const FSlateBrush * >::FGetter::CreateLambda([=]() {

                if (StaticMeshThumbnailBorder.IsValid() && StaticMeshThumbnailBorder->IsHovered())
                    return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailLight");
                else
                    return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailShadow");
            })));

            PickerHorizontalBox->AddSlot().AutoWidth().Padding(0.0f, 28.0f, 0.0f, 28.0f)
                [
                    PropertyCustomizationHelpers::MakeAddButton(
                        FSimpleDelegate::CreateUObject(
                            &InParam, &UHoudiniAssetInstanceInput::OnAddInstanceVariation,
                            HoudiniAssetInstanceInputField, VariationIdx),
                        LOCTEXT("AddAnotherInstanceToolTip", "Add Another Instance"))
                ];

            PickerHorizontalBox->AddSlot().AutoWidth().Padding(2.0f, 28.0f, 4.0f, 28.0f)
                [
                    PropertyCustomizationHelpers::MakeRemoveButton(
                        FSimpleDelegate::CreateUObject(
                            &InParam, &UHoudiniAssetInstanceInput::OnRemoveInstanceVariation,
                            HoudiniAssetInstanceInputField, VariationIdx),
                        LOCTEXT("RemoveLastInstanceToolTip", "Remove Last Instance"))
                ];

            TSharedPtr< SComboButton > AssetComboButton;
            TSharedPtr< SHorizontalBox > ButtonBox;

            PickerHorizontalBox->AddSlot()
                .FillWidth(10.0f)
                .Padding(0.0f, 4.0f, 4.0f, 4.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                .HAlign(HAlign_Fill)
                [
                    SAssignNew(ButtonBox, SHorizontalBox)
                    + SHorizontalBox::Slot()
                [
                    SAssignNew(AssetComboButton, SComboButton)
                    //.ToolTipText( this, &FHoudiniAssetComponentDetails::OnGetToolTip )
                .ButtonStyle(FEditorStyle::Get(), "PropertyEditor.AssetComboStyle")
                .ForegroundColor(FEditorStyle::GetColor("PropertyEditor.AssetName.ColorAndOpacity"))
                .OnMenuOpenChanged(FOnIsOpenChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInstanceInput::ChangedStaticMeshComboButton,
                    HoudiniAssetInstanceInputField, FieldIdx, VariationIdx))
                .ContentPadding(2.0f)
                .ButtonContent()
                [
                    SNew(STextBlock)
                    .TextStyle(FEditorStyle::Get(), "PropertyEditor.AssetClass")
                .Font(FEditorStyle::GetFontStyle(FName(TEXT("PropertyWindow.NormalFont"))))
                .Text(FText::FromString(InstancedObject->GetName()))
                ]
                ]
                ]
                ];

            // Create asset picker for this combo button.
            {
                TArray< UFactory * > NewAssetFactories;
                TSharedRef< SWidget > PropertyMenuAssetPicker =
                    PropertyCustomizationHelpers::MakeAssetPickerWithMenu(
                        FAssetData(InstancedObject), true,
                        AllowedClasses, NewAssetFactories, FOnShouldFilterAsset(),
                        FOnAssetSelected::CreateLambda([=](const FAssetData& AssetData) {
                    if (AssetComboButton.IsValid() && MyParam.IsValid() && InputFieldPtr.IsValid())
                    {
                        AssetComboButton->SetIsOpen(false);
                        UObject * Object = AssetData.GetAsset();
                        MyParam->OnStaticMeshDropped(Object, InputFieldPtr.Get(), FieldIdx, VariationIdx);
                    }
                }),
                        FSimpleDelegate::CreateUObject(
                            &InParam, &UHoudiniAssetInstanceInput::CloseStaticMeshComboButton,
                            HoudiniAssetInstanceInputField, FieldIdx, VariationIdx));

                AssetComboButton->SetMenuContent(PropertyMenuAssetPicker);
            }

            // Create tooltip.
            FFormatNamedArguments Args;
            Args.Add(TEXT("Asset"), FText::FromString(InstancedObject->GetName()));
            FText StaticMeshTooltip =
                FText::Format(LOCTEXT("BrowseToSpecificAssetInContentBrowser", "Browse to '{Asset}' in Content Browser"), Args);

            ButtonBox->AddSlot()
                .AutoWidth()
                .Padding(2.0f, 0.0f)
                .VAlign(VAlign_Center)
                [
                    PropertyCustomizationHelpers::MakeBrowseButton(
                        FSimpleDelegate::CreateUObject(&InParam, &UHoudiniAssetInstanceInput::OnInstancedObjectBrowse, InstancedObject),
                        TAttribute< FText >(StaticMeshTooltip))
                ];

            ButtonBox->AddSlot()
                .AutoWidth()
                .Padding(2.0f, 0.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(SButton)
                    .ToolTipText(LOCTEXT("ResetToBase", "Reset to default static mesh"))
                .ButtonStyle(FEditorStyle::Get(), "NoBorder")
                .ContentPadding(0)
                .Visibility(EVisibility::Visible)
                .OnClicked(FOnClicked::CreateUObject(
                    &InParam, &UHoudiniAssetInstanceInput::OnResetStaticMeshClicked,
                    HoudiniAssetInstanceInputField, FieldIdx, VariationIdx))
                [
                    SNew(SImage)
                    .Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
                ]
                ];

            TSharedRef< SVerticalBox > OffsetVerticalBox = SNew(SVerticalBox);
            FText LabelRotationText = LOCTEXT("HoudiniRotationOffset", "Rotation Offset");
            DetailGroup.AddWidgetRow()
                .NameContent()
                [
                    SNew(STextBlock)
                    .Text(LabelRotationText)
                .ToolTipText(LabelRotationText)
                .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
                ]
            .ValueContent()
                .MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH - 17)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().MaxWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH - 17)
                [
                    SNew(SRotatorInputBox)
                    .AllowSpin(true)
                .bColorAxisLabels(true)
                .Roll(TAttribute< TOptional< float > >::Create(
                    TAttribute< TOptional< float > >::FGetter::CreateUObject(
                        &InParam, &UHoudiniAssetInstanceInput::GetRotationRoll, HoudiniAssetInstanceInputField, VariationIdx)))
                .Pitch(TAttribute< TOptional< float> >::Create(
                    TAttribute< TOptional< float > >::FGetter::CreateUObject(
                        &InParam, &UHoudiniAssetInstanceInput::GetRotationPitch, HoudiniAssetInstanceInputField, VariationIdx)))
                .Yaw(TAttribute<TOptional< float > >::Create(
                    TAttribute< TOptional< float > >::FGetter::CreateUObject(
                        &InParam, &UHoudiniAssetInstanceInput::GetRotationYaw, HoudiniAssetInstanceInputField, VariationIdx)))
                .OnRollChanged(FOnFloatValueChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInstanceInput::SetRotationRoll, HoudiniAssetInstanceInputField, VariationIdx))
                .OnPitchChanged(FOnFloatValueChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInstanceInput::SetRotationPitch, HoudiniAssetInstanceInputField, VariationIdx))
                .OnYawChanged(FOnFloatValueChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInstanceInput::SetRotationYaw, HoudiniAssetInstanceInputField, VariationIdx))
                ]
                ];

            FText LabelScaleText = LOCTEXT("HoudiniScaleOffset", "Scale Offset");

            DetailGroup.AddWidgetRow()
                .NameContent()
                [
                    SNew(STextBlock)
                    .Text(LabelScaleText)
                .ToolTipText(LabelScaleText)
                .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
                ]
            .ValueContent()
                .MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().MaxWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
                [
                    SNew(SVectorInputBox)
                    .bColorAxisLabels(true)
                .X(TAttribute< TOptional< float > >::Create(
                    TAttribute< TOptional< float > >::FGetter::CreateUObject(
                        &InParam, &UHoudiniAssetInstanceInput::GetScaleX, HoudiniAssetInstanceInputField, VariationIdx)))
                .Y(TAttribute< TOptional< float > >::Create(
                    TAttribute< TOptional< float> >::FGetter::CreateUObject(
                        &InParam, &UHoudiniAssetInstanceInput::GetScaleY, HoudiniAssetInstanceInputField, VariationIdx)))
                .Z(TAttribute< TOptional< float> >::Create(
                    TAttribute< TOptional< float > >::FGetter::CreateUObject(
                        &InParam, &UHoudiniAssetInstanceInput::GetScaleZ, HoudiniAssetInstanceInputField, VariationIdx)))
                .OnXChanged(FOnFloatValueChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInstanceInput::SetScaleX, HoudiniAssetInstanceInputField, VariationIdx))
                .OnYChanged(FOnFloatValueChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInstanceInput::SetScaleY, HoudiniAssetInstanceInputField, VariationIdx))
                .OnZChanged(FOnFloatValueChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInstanceInput::SetScaleZ, HoudiniAssetInstanceInputField, VariationIdx))
                ]
            + SHorizontalBox::Slot().AutoWidth()
                [
                    // Add a checkbox to toggle between preserving the ratio of x,y,z components of scale when a value is entered
                    SNew(SCheckBox)
                    .Style(FEditorStyle::Get(), "TransparentCheckBox")
                .ToolTipText(LOCTEXT("PreserveScaleToolTip", "When locked, scales uniformly based on the current xyz scale values so the object maintains its shape in each direction when scaled"))
                .OnCheckStateChanged(FOnCheckStateChanged::CreateLambda([=](ECheckBoxState NewState) {
                if (MyParam.IsValid() && InputFieldPtr.IsValid())
                    MyParam->CheckStateChanged(NewState == ECheckBoxState::Checked, InputFieldPtr.Get(), VariationIdx);
            }))
                .IsChecked(TAttribute< ECheckBoxState >::Create(
                    TAttribute<ECheckBoxState>::FGetter::CreateLambda([=]() {
                if (InputFieldPtr.IsValid() && InputFieldPtr->AreOffsetsScaledLinearly(VariationIdx))
                    return ECheckBoxState::Checked;
                return ECheckBoxState::Unchecked;
            })))
                [
                    SNew(SImage)
                    .Image(TAttribute<const FSlateBrush*>::Create(
                        TAttribute<const FSlateBrush*>::FGetter::CreateLambda([=]() {
                if (InputFieldPtr.IsValid() && InputFieldPtr->AreOffsetsScaledLinearly(VariationIdx))
                {
                    return FEditorStyle::GetBrush(TEXT("GenericLock"));
                }
                return FEditorStyle::GetBrush(TEXT("GenericUnlock"));
            })))
                .ColorAndOpacity(FSlateColor::UseForeground())
                ]
                ]
                ];
        }
    }
}
