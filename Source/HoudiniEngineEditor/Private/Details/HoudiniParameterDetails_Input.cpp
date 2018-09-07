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
FHoudiniParameterDetails::CreateWidgetInput(IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetInput& InParam)
{
    TWeakObjectPtr<UHoudiniAssetInput> MyParam(&InParam);

    // Get thumbnail pool for this builder.
    IDetailLayoutBuilder & DetailLayoutBuilder = LocalDetailCategoryBuilder.GetParentLayout();
    TSharedPtr< FAssetThumbnailPool > AssetThumbnailPool = DetailLayoutBuilder.GetThumbnailPool();
    FDetailWidgetRow & Row = LocalDetailCategoryBuilder.AddCustomRow(FText::GetEmpty());
    FText ParameterLabelText = FText::FromString(InParam.GetParameterLabel());
    FText ParameterTooltip = GetParameterTooltip(&InParam);
    Row.NameWidget.Widget =
        SNew(STextBlock)
        .Text(ParameterLabelText)
        .ToolTipText(ParameterTooltip)
        .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));

    TSharedRef< SVerticalBox > VerticalBox = SNew(SVerticalBox);

    if (InParam.StringChoiceLabels.Num() > 0)
    {
        // ComboBox :  Input Type
        TSharedPtr< SComboBox< TSharedPtr< FString > > > ComboBoxInputType;
        VerticalBox->AddSlot().Padding(2, 2, 5, 2)
            [
                //SNew(SComboBox<TSharedPtr< FString > >)
                SAssignNew(ComboBoxInputType, SComboBox< TSharedPtr< FString > >)
                .OptionsSource(&InParam.StringChoiceLabels)
            .InitiallySelectedItem(InParam.StringChoiceLabels[InParam.ChoiceIndex])
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
                    &InParam, &UHoudiniAssetInput::HandleChoiceContentText)))
            .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
            ]
            ];

        //ComboBoxInputType->SetSelectedItem( InParam.StringChoiceLabels[ InParam.ChoiceIndex ] );
    }

    // Checkbox : Keep World Transform
    {
        TSharedPtr< SCheckBox > CheckBoxTranformType;
        VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
            [
                SAssignNew(CheckBoxTranformType, SCheckBox)
                .Content()
            [
                SNew(STextBlock)
                .Text(LOCTEXT("KeepWorldTransformCheckbox", "Keep World Transform"))
            .ToolTipText(LOCTEXT("KeepWorldTransformCheckboxTip", "Set this Input's object_merge Transform Type to INTO_THIS_OBJECT. If unchecked, it will be set to NONE."))
            .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
            ]
        .IsChecked(TAttribute< ECheckBoxState >::Create(
            TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                &InParam, &UHoudiniAssetInput::IsCheckedKeepWorldTransform)))
            .OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(
                &InParam, &UHoudiniAssetInput::CheckStateChangedKeepWorldTransform))
            ];

        // the checkbox is read only for geo inputs
        if (InParam.ChoiceIndex == EHoudiniAssetInputType::GeometryInput)
            CheckBoxTranformType->SetEnabled(false);

        // Checkbox is read only if the input is an object-path parameter
        //if ( bIsObjectPathParameter )
        //    CheckBoxTranformType->SetEnabled(false);
    }

    // Checkbox Pack before merging
    if (InParam.ChoiceIndex == EHoudiniAssetInputType::GeometryInput
        || InParam.ChoiceIndex == EHoudiniAssetInputType::WorldInput)
    {
        TSharedPtr< SCheckBox > CheckBoxPackBeforeMerge;
        VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
            [
                SAssignNew(CheckBoxPackBeforeMerge, SCheckBox)
                .Content()
            [
                SNew(STextBlock)
                .Text(LOCTEXT("PackBeforeMergeCheckbox", "Pack Geometry before merging"))
            .ToolTipText(LOCTEXT("PackBeforeMergeCheckboxTip", "Pack each separate piece of geometry before merging them into the input."))
            .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
            ]
        .IsChecked(TAttribute< ECheckBoxState >::Create(
            TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                &InParam, &UHoudiniAssetInput::IsCheckedPackBeforeMerge)))
            .OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(
                &InParam, &UHoudiniAssetInput::CheckStateChangedPackBeforeMerge))
            ];
    }

    // Checkboxes Export LODs and Export Sockets
    if (InParam.ChoiceIndex == EHoudiniAssetInputType::GeometryInput
        || InParam.ChoiceIndex == EHoudiniAssetInputType::WorldInput)
    {
        /*
        // Add a checkbox to export all lods
        TSharedPtr< SCheckBox > CheckBoxExportAllLODs;
        VerticalBox->AddSlot().Padding( 2, 2, 5, 2 ).AutoHeight()
        [
        SAssignNew( CheckBoxExportAllLODs, SCheckBox )
        .Content()
        [
        SNew( STextBlock )
        .Text( LOCTEXT( "ExportAllLOD", "Export all LODs" ) )
        .ToolTipText( LOCTEXT( "ExportAllLODCheckboxTip", "If enabled, all LOD Meshes in this static mesh will be sent to Houdini." ) )
        .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
        ]
        .IsChecked( TAttribute< ECheckBoxState >::Create(
        TAttribute< ECheckBoxState >::FGetter::CreateUObject(
        &InParam, &UHoudiniAssetInput::IsCheckedExportAllLODs ) ) )
        .OnCheckStateChanged( FOnCheckStateChanged::CreateUObject(
        &InParam, &UHoudiniAssetInput::CheckStateChangedExportAllLODs ) )
        ];

        // the checkbox is read only if the input doesnt have LODs
        CheckBoxExportAllLODs->SetEnabled( InParam.HasLODs() );

        // Add a checkbox to export sockets
        TSharedPtr< SCheckBox > CheckBoxExportSockets;
        VerticalBox->AddSlot().Padding( 2, 2, 5, 2 ).AutoHeight()
        [
        SAssignNew( CheckBoxExportSockets, SCheckBox )
        .Content()
        [
        SNew( STextBlock )
        .Text( LOCTEXT( "ExportSockets", "Export Sockets" ) )
        .ToolTipText( LOCTEXT( "ExportSocketsTip", "If enabled, all Mesh Sockets in this static mesh will be sent to Houdini." ) )
        .Font( FEditorStyle::GetFontStyle( TEXT( "PropertyWindow.NormalFont" ) ) )
        ]
        .IsChecked( TAttribute< ECheckBoxState >::Create(
        TAttribute< ECheckBoxState >::FGetter::CreateUObject(
        &InParam, &UHoudiniAssetInput::IsCheckedExportSockets ) ) )
        .OnCheckStateChanged( FOnCheckStateChanged::CreateUObject(
        &InParam, &UHoudiniAssetInput::CheckStateChangedExportSockets ) )
        ];

        // the checkbox is read only if the input doesnt have LODs
        CheckBoxExportSockets->SetEnabled( InParam.HasSockets() );
        */

        TSharedPtr< SCheckBox > CheckBoxExportAllLODs;
        TSharedPtr< SCheckBox > CheckBoxExportSockets;
        VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
            .Padding(1.0f)
            .VAlign(VAlign_Center)
            .AutoWidth()
            [
                SAssignNew(CheckBoxExportAllLODs, SCheckBox)
                .Content()
            [
                SNew(STextBlock)
                .Text(LOCTEXT("ExportAllLOD", "Export LODs"))
            .ToolTipText(LOCTEXT("ExportAllLODCheckboxTip", "If enabled, all LOD Meshes in this static mesh will be sent to Houdini."))
            .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
            ]
        .IsChecked(TAttribute< ECheckBoxState >::Create(
            TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                &InParam, &UHoudiniAssetInput::IsCheckedExportAllLODs)))
            .OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(
                &InParam, &UHoudiniAssetInput::CheckStateChangedExportAllLODs))
            ]
        + SHorizontalBox::Slot()
            .Padding(1.0f)
            .VAlign(VAlign_Center)
            .AutoWidth()
            [
                SAssignNew(CheckBoxExportSockets, SCheckBox)
                .Content()
            [
                SNew(STextBlock)
                .Text(LOCTEXT("ExportSockets", "Export Sockets"))
            .ToolTipText(LOCTEXT("ExportSocketsTip", "If enabled, all Mesh Sockets in this static mesh will be sent to Houdini."))
            .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
            ]
        .IsChecked(TAttribute< ECheckBoxState >::Create(
            TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                &InParam, &UHoudiniAssetInput::IsCheckedExportSockets)))
            .OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(
                &InParam, &UHoudiniAssetInput::CheckStateChangedExportSockets))
            ]
            ];
    }

    if (InParam.ChoiceIndex == EHoudiniAssetInputType::GeometryInput)
    {
        const int32 NumInputs = InParam.InputObjects.Num();
        VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
            .Padding(1.0f)
            .VAlign(VAlign_Center)
            .AutoWidth()
            [
                SNew(STextBlock)
                .Text(FText::Format(LOCTEXT("NumArrayItemsFmt", "{0} elements"), FText::AsNumber(NumInputs)))
            .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
            ]
        + SHorizontalBox::Slot()
            .Padding(1.0f)
            .VAlign(VAlign_Center)
            .AutoWidth()
            [
                PropertyCustomizationHelpers::MakeAddButton(FSimpleDelegate::CreateUObject(&InParam, &UHoudiniAssetInput::OnAddToInputObjects), LOCTEXT("AddInput", "Adds a Geometry Input"), true)
            ]
        + SHorizontalBox::Slot()
            .Padding(1.0f)
            .VAlign(VAlign_Center)
            .AutoWidth()
            [
                PropertyCustomizationHelpers::MakeEmptyButton(FSimpleDelegate::CreateUObject(&InParam, &UHoudiniAssetInput::OnEmptyInputObjects), LOCTEXT("EmptyInputs", "Removes All Inputs"), true)
            ]
            ];

        for (int32 Ix = 0; Ix < NumInputs; Ix++)
        {
            UObject* InputObject = InParam.GetInputObject(Ix);
            Helper_CreateGeometryWidget(InParam, Ix, InputObject, AssetThumbnailPool, VerticalBox);
        }
    }
    else if (InParam.ChoiceIndex == EHoudiniAssetInputType::AssetInput)
    {
        // ActorPicker : Houdini Asset
        FMenuBuilder MenuBuilder = Helper_CreateCustomActorPickerWidget(InParam, LOCTEXT("AssetInputSelectableActors", "Houdini Asset Actors"), true);

        VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
            [
                MenuBuilder.MakeWidget()
            ];
    }
    else if (InParam.ChoiceIndex == EHoudiniAssetInputType::CurveInput)
    {
        // Go through all input curve parameters and build their widgets recursively.
        for (TMap< FString, UHoudiniAssetParameter * >::TIterator
            IterParams(InParam.InputCurveParameters); IterParams; ++IterParams)
        {
            // Note: Only choice and toggle supported atm
            if (UHoudiniAssetParameter * HoudiniAssetParameter = IterParams.Value())
                FHoudiniParameterDetails::CreateWidget(VerticalBox, HoudiniAssetParameter);
        }
    }
    else if (InParam.ChoiceIndex == EHoudiniAssetInputType::LandscapeInput)
    {
        // ActorPicker : Landscape
        FMenuBuilder MenuBuilder = Helper_CreateCustomActorPickerWidget(InParam, LOCTEXT("LandscapeInputSelectableActors", "Landscapes"), true);
        VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
            [
                MenuBuilder.MakeWidget()
            ];

        // Checkboxes : Export Landscape As
        //		Heightfield Mesh Points
        {
            // Label
            VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("LandscapeExportAs", "Export Landscape As"))
                .ToolTipText(LOCTEXT("LandscapeExportAsTooltip", "Choose the type of data you want the landscape to be exported to:\n * Heightfield\n * Mesh\n * Points"))
                .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
                ];

            //
            TSharedPtr<SUniformGridPanel> ButtonOptionsPanel;
            VerticalBox->AddSlot().Padding(5, 2, 5, 2).AutoHeight()
                [
                    SAssignNew(ButtonOptionsPanel, SUniformGridPanel)
                ];

            // Heightfield
            FText HeightfieldTooltip = LOCTEXT("LandscapeExportAsHeightfieldTooltip", "If enabled, the landscape will be exported to Houdini as a heighfield.");
            ButtonOptionsPanel->AddSlot(0, 0)
                [
                    SNew(SCheckBox)
                    .Style(FEditorStyle::Get(), "Property.ToggleButton.Start")
                .IsChecked(TAttribute< ECheckBoxState >::Create(
                    TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                        &InParam, &UHoudiniAssetInput::IsCheckedExportAsHeightfield)))
                .OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInput::CheckStateChangedExportAsHeightfield))
                .ToolTipText(HeightfieldTooltip)
                [
                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(2, 2)
                [
                    SNew(SImage)
                    .Image(FEditorStyle::GetBrush("ClassIcon.LandscapeComponent"))
                ]

            + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                .HAlign(HAlign_Center)
                .Padding(2, 2)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("LandscapeExportAsHeightfieldCheckbox", "Heightfield"))
                .ToolTipText(HeightfieldTooltip)
                .Font(IDetailLayoutBuilder::GetDetailFont())
                ]
                ]
                ];

            // Mesh
            FText MeshTooltip = LOCTEXT("LandscapeExportAsHeightfieldTooltip", "If enabled, the landscape will be exported to Houdini as a heighfield.");
            ButtonOptionsPanel->AddSlot(1, 0)
                [
                    SNew(SCheckBox)
                    .Style(FEditorStyle::Get(), "Property.ToggleButton.Middle")
                .IsChecked(TAttribute< ECheckBoxState >::Create(
                    TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                        &InParam, &UHoudiniAssetInput::IsCheckedExportAsMesh)))
                .OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInput::CheckStateChangedExportAsMesh))
                .ToolTipText(MeshTooltip)
                [
                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(2, 2)
                [
                    SNew(SImage)
                    .Image(FEditorStyle::GetBrush("ClassIcon.StaticMeshComponent"))
                ]

            + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                .HAlign(HAlign_Center)
                .Padding(2, 2)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("LandscapeExportAsMeshCheckbox", "Mesh"))
                .ToolTipText(MeshTooltip)
                .Font(IDetailLayoutBuilder::GetDetailFont())
                ]
                ]
                ];

            // Points
            FText PointsTooltip = LOCTEXT("LandscapeExportAsPointsTooltip", "If enabled, the landscape will be exported to Houdini as points.");
            ButtonOptionsPanel->AddSlot(2, 0)
                [
                    SNew(SCheckBox)
                    .Style(FEditorStyle::Get(), "Property.ToggleButton.End")
                .IsChecked(TAttribute< ECheckBoxState >::Create(
                    TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                        &InParam, &UHoudiniAssetInput::IsCheckedExportAsPoints)))
                .OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInput::CheckStateChangedExportAsPoints))
                .ToolTipText(PointsTooltip)
                [
                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                .Padding(2, 2)
                [
                    SNew(SImage)
                    .Image(FEditorStyle::GetBrush("Mobility.Static"))
                ]

            + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                .HAlign(HAlign_Center)
                .Padding(2, 2)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("LandscapeExportAsPointsCheckbox", "Points"))
                .ToolTipText(PointsTooltip)
                .Font(IDetailLayoutBuilder::GetDetailFont())
                ]
                ]
                ];
        }

        // CheckBox : Export selected components only
        {
            TSharedPtr< SCheckBox > CheckBoxExportSelected;
            VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
                [
                    SAssignNew(CheckBoxExportSelected, SCheckBox)
                    .Content()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("LandscapeSelectedCheckbox", "Export Selected Landscape Components Only"))
                .ToolTipText(LOCTEXT("LandscapeSelectedTooltip", "If enabled, only the selected Landscape Components will be exported."))
                .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
                ]
            .IsChecked(TAttribute< ECheckBoxState >::Create(
                TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetInput::IsCheckedExportOnlySelected)))
                .OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInput::CheckStateChangedExportOnlySelected))
                ];
        }

        // Checkboxes auto select components
        {
            TSharedPtr< SCheckBox > CheckBoxAutoSelectComponents;
            VerticalBox->AddSlot().Padding(10, 2, 5, 2).AutoHeight()
                [
                    SAssignNew(CheckBoxAutoSelectComponents, SCheckBox)
                    .Content()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("AutoSelectComponentCheckbox", "Auto-select component in asset bounds"))
                .ToolTipText(LOCTEXT("AutoSelectComponentCheckboxTooltip", "If enabled, when no Landscape components are curremtly selected, the one within the asset's bounding box will be exported."))
                .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
                ]
            .IsChecked(TAttribute< ECheckBoxState >::Create(
                TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetInput::IsCheckedAutoSelectLandscape)))
                .OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInput::CheckStateChangedAutoSelectLandscape))
                ];

            // Enable only when exporting selection
            // or when exporting heighfield (for now)
            if (!InParam.bLandscapeExportSelectionOnly)
                CheckBoxAutoSelectComponents->SetEnabled(false);
        }

        // Checkbox : Export materials
        {
            TSharedPtr< SCheckBox > CheckBoxExportMaterials;
            VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
                [
                    SAssignNew(CheckBoxExportMaterials, SCheckBox)
                    .Content()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("LandscapeMaterialsCheckbox", "Export Landscape Materials"))
                .ToolTipText(LOCTEXT("LandscapeMaterialsTooltip", "If enabled, the landscape materials will be exported with it."))
                .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
                ]
            .IsChecked(TAttribute< ECheckBoxState >::Create(
                TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetInput::IsCheckedExportMaterials)))
                .OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInput::CheckStateChangedExportMaterials))
                ];

            // Disable when exporting heightfields
            if (InParam.bLandscapeExportAsHeightfield)
                CheckBoxExportMaterials->SetEnabled(false);
        }

        // Checkbox : Export Tile UVs
        {
            TSharedPtr< SCheckBox > CheckBoxExportTileUVs;
            VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
                [
                    SAssignNew(CheckBoxExportTileUVs, SCheckBox)
                    .Content()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("LandscapeTileUVsCheckbox", "Export Landscape Tile UVs"))
                .ToolTipText(LOCTEXT("LandscapeTileUVsTooltip", "If enabled, UVs will be exported separately for each Landscape tile."))
                .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
                ]
            .IsChecked(TAttribute< ECheckBoxState >::Create(
                TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetInput::IsCheckedExportTileUVs)))
                .OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInput::CheckStateChangedExportTileUVs))
                ];

            // Disable when exporting heightfields
            if (InParam.bLandscapeExportAsHeightfield)
                CheckBoxExportTileUVs->SetEnabled(false);
        }

        // Checkbox : Export normalized UVs
        {
            TSharedPtr< SCheckBox > CheckBoxExportNormalizedUVs;
            VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
                [
                    SAssignNew(CheckBoxExportNormalizedUVs, SCheckBox)
                    .Content()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("LandscapeNormalizedUVsCheckbox", "Export Landscape Normalized UVs"))
                .ToolTipText(LOCTEXT("LandscapeNormalizedUVsTooltip", "If enabled, landscape UVs will be exported in [0, 1]."))
                .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
                ]
            .IsChecked(TAttribute< ECheckBoxState >::Create(
                TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetInput::IsCheckedExportNormalizedUVs)))
                .OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInput::CheckStateChangedExportNormalizedUVs))
                ];

            // Disable when exporting heightfields
            if (InParam.bLandscapeExportAsHeightfield)
                CheckBoxExportNormalizedUVs->SetEnabled(false);
        }

        // Checkbox : Export lighting
        {
            TSharedPtr< SCheckBox > CheckBoxExportLighting;
            VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
                [
                    SAssignNew(CheckBoxExportLighting, SCheckBox)
                    .Content()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("LandscapeLightingCheckbox", "Export Landscape Lighting"))
                .ToolTipText(LOCTEXT("LandscapeLightingTooltip", "If enabled, lightmap information will be exported with the landscape."))
                .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
                ]
            .IsChecked(TAttribute< ECheckBoxState >::Create(
                TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetInput::IsCheckedExportLighting)))
                .OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInput::CheckStateChangedExportLighting))
                ];

            // Disable when exporting heightfields
            if (InParam.bLandscapeExportAsHeightfield)
                CheckBoxExportLighting->SetEnabled(false);
        }

        // Checkbox : Export landscape curves
        {
            TSharedPtr< SCheckBox > CheckBoxExportCurves;
            VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
                [
                    SAssignNew(CheckBoxExportCurves, SCheckBox)
                    .Content()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("LandscapeCurvesCheckbox", "Export Landscape Curves"))
                .ToolTipText(LOCTEXT("LandscapeCurvesTooltip", "If enabled, Landscape curves will be exported."))
                .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
                ]
            .IsChecked(TAttribute< ECheckBoxState >::Create(
                TAttribute< ECheckBoxState >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetInput::IsCheckedExportCurves)))
                .OnCheckStateChanged(FOnCheckStateChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInput::CheckStateChangedExportCurves))
                ];

            // Disable curves until we have them implemented.
            if (CheckBoxExportCurves.IsValid())
                CheckBoxExportCurves->SetEnabled(false);
        }

        // Button : Recommit
        VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
            .Padding(1, 2, 4, 2)
            [
                SNew(SButton)
                .VAlign(VAlign_Center)
            .HAlign(HAlign_Center)
            .Text(LOCTEXT("LandscapeInputRecommit", "Recommit Landscape"))
            .ToolTipText(LOCTEXT("LandscapeInputRecommitTooltip", "Recommits the Landscape to Houdini."))
            .OnClicked(FOnClicked::CreateUObject(&InParam, &UHoudiniAssetInput::OnButtonClickRecommit))
            ]
            ];
    }
    else if (InParam.ChoiceIndex == EHoudiniAssetInputType::WorldInput)
    {
        // Button : Start Selection / Use current selection + refresh
        {
            TSharedPtr< SHorizontalBox > HorizontalBox = NULL;
            FPropertyEditorModule & PropertyModule =
                FModuleManager::Get().GetModuleChecked< FPropertyEditorModule >("PropertyEditor");

            // Get the details view
            bool bDetailsLocked = false;
            FName DetailsPanelName = "LevelEditorSelectionDetails";

            const IDetailsView* DetailsView = DetailLayoutBuilder.GetDetailsView();
            if (DetailsView)
            {
                DetailsPanelName = DetailsView->GetIdentifier();
                if (DetailsView->IsLocked())
                    bDetailsLocked = true;
            }

            auto ButtonLabel = LOCTEXT("WorldInputStartSelection", "Start Selection (Lock Details Panel)");
            if (bDetailsLocked)
                ButtonLabel = LOCTEXT("WorldInputUseCurrentSelection", "Use Current Selection (Unlock Details Panel)");

            FOnClicked OnSelectActors = FOnClicked::CreateStatic(&FHoudiniParameterDetails::Helper_OnButtonClickSelectActors, MyParam, DetailsPanelName);

            VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
                [
                    SAssignNew(HorizontalBox, SHorizontalBox)
                    + SHorizontalBox::Slot()
                [
                    SNew(SButton)
                    .VAlign(VAlign_Center)
                .HAlign(HAlign_Center)
                .Text(ButtonLabel)
                .OnClicked(OnSelectActors)
                ]
                ];
        }

        // ActorPicker : World Outliner
        {
            FMenuBuilder MenuBuilder = Helper_CreateCustomActorPickerWidget(InParam, LOCTEXT("WorldInputSelectedActors", "Currently Selected Actors"), false);

            VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
                [
                    MenuBuilder.MakeWidget()
                ];
        }

        {
            // Spline Resolution
            TSharedPtr< SNumericEntryBox< float > > NumericEntryBox;
            int32 Idx = 0;

            VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("SplineRes", "Unreal Spline Resolution"))
                .ToolTipText(LOCTEXT("SplineResTooltip", "Resolution used when marshalling the Unreal Splines to HoudiniEngine.\n(step in cm betweem control points)\nSet this to 0 to only export the control points."))
                .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
                ]
            + SHorizontalBox::Slot()
                .Padding(2.0f, 0.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(SNumericEntryBox< float >)
                    .AllowSpin(true)

                .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))

                .MinValue(-1.0f)
                .MaxValue(1000.0f)
                .MinSliderValue(0.0f)
                .MaxSliderValue(1000.0f)

                .Value(TAttribute< TOptional< float > >::Create(TAttribute< TOptional< float > >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetInput::GetSplineResolutionValue)))
                .OnValueChanged(SNumericEntryBox< float >::FOnValueChanged::CreateUObject(
                    &InParam, &UHoudiniAssetInput::SetSplineResolutionValue))
                .IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetInput::IsSplineResolutionEnabled)))

                .SliderExponent(1.0f)
                ]
            + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(2.0f, 0.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(SButton)
                    .ToolTipText(LOCTEXT("SplineResToDefault", "Reset to default value."))
                .ButtonStyle(FEditorStyle::Get(), "NoBorder")
                .ContentPadding(0)
                .Visibility(EVisibility::Visible)
                .OnClicked(FOnClicked::CreateUObject(&InParam, &UHoudiniAssetInput::OnResetSplineResolutionClicked))
                [
                    SNew(SImage)
                    .Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
                ]
                ]
                ];
        }
    }

    Row.ValueWidget.Widget = VerticalBox;
    Row.ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
}
