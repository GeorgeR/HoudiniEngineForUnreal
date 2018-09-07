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
FHoudiniParameterDetails::CreateNameWidget(UHoudiniAssetParameter* InParam, FDetailWidgetRow & Row, bool WithLabel)
{
    if (!InParam)
        return;

    FText ParameterLabelText = FText::FromString(InParam->GetParameterLabel());
    const FText & FinalParameterLabelText = WithLabel ? ParameterLabelText : FText::GetEmpty();

    FText ParameterTooltip = GetParameterTooltip(InParam);
    if (InParam->bIsChildOfMultiparm && InParam->ParentParameter)
    {
        TSharedRef< SHorizontalBox > HorizontalBox = SNew(SHorizontalBox);

        // We have to make sure the ParentParameter is a multiparm, as folders will cause issues here
        // ( we want to call RemoveMultiParmInstance or AddMultiParmInstance on the parent multiparm, not just the parent)
        UHoudiniAssetParameter * ParentMultiparm = InParam->ParentParameter;
        while (ParentMultiparm && !ParentMultiparm->bIsMultiparm)
            ParentMultiparm = ParentMultiparm->ParentParameter;

        // Failed to find the multiparm parent, better have the original parent than nullptr
        if (!ParentMultiparm)
            ParentMultiparm = InParam->ParentParameter;

        TSharedRef< SWidget > ClearButton = PropertyCustomizationHelpers::MakeClearButton(
            FSimpleDelegate::CreateUObject(
            (UHoudiniAssetParameterMultiparm *)ParentMultiparm,
                &UHoudiniAssetParameterMultiparm::RemoveMultiparmInstance,
                InParam->MultiparmInstanceIndex),
            LOCTEXT("RemoveMultiparmInstanceToolTip", "Remove"));
        TSharedRef< SWidget > AddButton = PropertyCustomizationHelpers::MakeAddButton(
            FSimpleDelegate::CreateUObject(
            (UHoudiniAssetParameterMultiparm *)ParentMultiparm,
                &UHoudiniAssetParameterMultiparm::AddMultiparmInstance,
                InParam->MultiparmInstanceIndex),
            LOCTEXT("InsertBeforeMultiparmInstanceToolTip", "Insert Before"));

        if (InParam->ChildIndex != 0)
        {
            AddButton.Get().SetVisibility(EVisibility::Hidden);
            ClearButton.Get().SetVisibility(EVisibility::Hidden);
        }

        // Adding eventual padding for nested multiparams
        UHoudiniAssetParameter* currentParentParameter = ParentMultiparm;
        while (currentParentParameter && currentParentParameter->bIsChildOfMultiparm)
        {
            if (currentParentParameter->bIsMultiparm)
                HorizontalBox->AddSlot().MaxWidth(16.0f);

            currentParentParameter = currentParentParameter->ParentParameter;
        }

        HorizontalBox->AddSlot().AutoWidth().Padding(2.0f, 0.0f)
            [
                ClearButton
            ];

        HorizontalBox->AddSlot().AutoWidth().Padding(0.0f, 0.0f)
            [
                AddButton
            ];

        HorizontalBox->AddSlot().Padding(2, 5, 5, 2)
            [
                SNew(STextBlock)
                .Text(FinalParameterLabelText)
            .ToolTipText(WithLabel ? ParameterTooltip : ParameterLabelText)
            .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
            ];

        Row.NameWidget.Widget = HorizontalBox;
    }
    else
    {
        Row.NameWidget.Widget =
            SNew(STextBlock)
            .Text(FinalParameterLabelText)
            .ToolTipText(ParameterTooltip)
            .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));
    }
}

FText
FHoudiniParameterDetails::GetParameterTooltip(UHoudiniAssetParameter* InParam)
{
    if (!InParam)
        return FText();

    /*
    FString Tooltip = InParam->GetParameterLabel() + TEXT(" (") + InParam->GetParameterName() + TEXT(")");
    if ( !InParam->GetParameterHelp().IsEmpty() )
    Tooltip += TEXT ( ":\n") + InParam->GetParameterHelp();

    return FText::FromString( Tooltip );
    */

    if (!InParam->GetParameterHelp().IsEmpty())
        return FText::FromString(InParam->GetParameterHelp());
    else
        return FText::FromString(InParam->GetParameterLabel() + TEXT(" (") + InParam->GetParameterName() + TEXT(")"));
}

void
FHoudiniParameterDetails::CreateWidget(IDetailCategoryBuilder & LocalDetailCategoryBuilder, UHoudiniAssetParameter* InParam)
{
    check(InParam);

    if (auto ParamFloat = Cast<UHoudiniAssetParameterFloat>(InParam))
    {
        CreateWidgetFloat(LocalDetailCategoryBuilder, *ParamFloat);
    }
    else if (auto ParamFolder = Cast<UHoudiniAssetParameterFolder>(InParam))
    {
        CreateWidgetFolder(LocalDetailCategoryBuilder, *ParamFolder);
    }
    else if (auto ParamFolderList = Cast<UHoudiniAssetParameterFolderList>(InParam))
    {
        CreateWidgetFolderList(LocalDetailCategoryBuilder, *ParamFolderList);
    }
    // Test Ramp before Multiparm!
    else if (auto ParamRamp = Cast<UHoudiniAssetParameterRamp>(InParam))
    {
        CreateWidgetRamp(LocalDetailCategoryBuilder, *ParamRamp);
    }
    else if (auto ParamMultiparm = Cast<UHoudiniAssetParameterMultiparm>(InParam))
    {
        CreateWidgetMultiparm(LocalDetailCategoryBuilder, *ParamMultiparm);
    }
    else if (auto ParamButton = Cast<UHoudiniAssetParameterButton>(InParam))
    {
        CreateWidgetButton(LocalDetailCategoryBuilder, *ParamButton);
    }
    else if (auto ParamChoice = Cast<UHoudiniAssetParameterChoice>(InParam))
    {
        CreateWidgetChoice(LocalDetailCategoryBuilder, *ParamChoice);
    }
    else if (auto ParamColor = Cast<UHoudiniAssetParameterColor>(InParam))
    {
        CreateWidgetColor(LocalDetailCategoryBuilder, *ParamColor);
    }
    else if (auto ParamToggle = Cast<UHoudiniAssetParameterToggle>(InParam))
    {
        CreateWidgetToggle(LocalDetailCategoryBuilder, *ParamToggle);
    }
    else if (auto ParamInput = Cast<UHoudiniAssetInput>(InParam))
    {
        CreateWidgetInput(LocalDetailCategoryBuilder, *ParamInput);
    }
    else if (auto ParamInt = Cast<UHoudiniAssetParameterInt>(InParam))
    {
        CreateWidgetInt(LocalDetailCategoryBuilder, *ParamInt);
    }
    else if (auto ParamInstanceInput = Cast<UHoudiniAssetInstanceInput>(InParam))
    {
        CreateWidgetInstanceInput(LocalDetailCategoryBuilder, *ParamInstanceInput);
    }
    else if (auto ParamLabel = Cast<UHoudiniAssetParameterLabel>(InParam))
    {
        CreateWidgetLabel(LocalDetailCategoryBuilder, *ParamLabel);
    }
    else if (auto ParamString = Cast<UHoudiniAssetParameterString>(InParam))
    {
        CreateWidgetString(LocalDetailCategoryBuilder, *ParamString);
    }
    else if (auto ParamSeparator = Cast<UHoudiniAssetParameterSeparator>(InParam))
    {
        CreateWidgetSeparator(LocalDetailCategoryBuilder, *ParamSeparator);
    }
    else if (auto ParamFile = Cast<UHoudiniAssetParameterFile>(InParam))
    {
        CreateWidgetFile(LocalDetailCategoryBuilder, *ParamFile);
    }
    else
    {
        check(false);
    }
}

void
FHoudiniParameterDetails::CreateWidget(TSharedPtr< SVerticalBox > VerticalBox, class UHoudiniAssetParameter* InParam)
{
    check(InParam);

    if (auto ParamChoice = Cast<UHoudiniAssetParameterChoice>(InParam))
    {
        CreateWidgetChoice(VerticalBox, *ParamChoice);
    }
    else if (auto ParamToggle = Cast<UHoudiniAssetParameterToggle>(InParam))
    {
        CreateWidgetToggle(VerticalBox, *ParamToggle);
    }
    else
    {
        check(false);
    }
}

FMenuBuilder
FHoudiniParameterDetails::Helper_CreateCustomActorPickerWidget(UHoudiniAssetInput& InParam, const TAttribute<FText>& HeadingText, const bool& bShowCurrentSelectionSection)
{
    // Custom Actor Picker showing only the desired Actor types.
    // Note: Code stolen from SPropertyMenuActorPicker.cpp
    FOnShouldFilterActor ActorFilter = FOnShouldFilterActor::CreateUObject(&InParam, &UHoudiniAssetInput::OnShouldFilterActor);

    //FHoudiniEngineEditor & HoudiniEngineEditor = FHoudiniEngineEditor::Get();
    //TSharedPtr< ISlateStyle > StyleSet = GEditor->GetSlateStyle();

    FMenuBuilder MenuBuilder(true, NULL);

    if (bShowCurrentSelectionSection)
    {
        MenuBuilder.BeginSection(NAME_None, LOCTEXT("CurrentActorOperationsHeader", "Current Selection"));
        {
            MenuBuilder.AddMenuEntry(
                TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateUObject(&InParam, &UHoudiniAssetInput::GetCurrentSelectionText)),
                TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateUObject(&InParam, &UHoudiniAssetInput::GetCurrentSelectionText)),
                FSlateIcon(),//StyleSet->GetStyleSetName(), "HoudiniEngine.HoudiniEngineLogo")),
                FUIAction(),
                NAME_None,
                EUserInterfaceActionType::Button,
                NAME_None);
        }
        MenuBuilder.EndSection();
    }

    MenuBuilder.BeginSection(NAME_None, HeadingText);
    {
        TSharedPtr< SWidget > MenuContent;

        FSceneOutlinerModule & SceneOutlinerModule =
            FModuleManager::Get().LoadModuleChecked< FSceneOutlinerModule >(TEXT("SceneOutliner"));

        SceneOutliner::FInitializationOptions InitOptions;
        InitOptions.Mode = ESceneOutlinerMode::ActorPicker;
        InitOptions.Filters->AddFilterPredicate(ActorFilter);
        InitOptions.bFocusSearchBoxWhenOpened = true;

        static const FVector2D SceneOutlinerWindowSize(350.0f, 200.0f);
        MenuContent =
            SNew(SBox)
            .WidthOverride(SceneOutlinerWindowSize.X)
            .HeightOverride(SceneOutlinerWindowSize.Y)
            [
                SNew(SBorder)
                .BorderImage(FEditorStyle::GetBrush("Menu.Background"))
            [
                SceneOutlinerModule.CreateSceneOutliner(
                    InitOptions, FOnActorPicked::CreateUObject(
                        &InParam, &UHoudiniAssetInput::OnActorSelected))
            ]
            ];

        MenuBuilder.AddWidget(MenuContent.ToSharedRef(), FText::GetEmpty(), true);
    }
    MenuBuilder.EndSection();

    return MenuBuilder;
}

/** Create a single geometry widget for the given input object */
void FHoudiniParameterDetails::Helper_CreateGeometryWidget(
    UHoudiniAssetInput& InParam, int32 AtIndex, UObject* InputObject,
    TSharedPtr< FAssetThumbnailPool > AssetThumbnailPool, TSharedRef< SVerticalBox > VerticalBox)
{
    TWeakObjectPtr<UHoudiniAssetInput> MyParam(&InParam);

    // Create thumbnail for this static mesh.
    TSharedPtr< FAssetThumbnail > StaticMeshThumbnail = MakeShareable(
        new FAssetThumbnail(InputObject, 64, 64, AssetThumbnailPool));

    TSharedPtr< SHorizontalBox > HorizontalBox = NULL;
    // Drop Target: Static Mesh
    VerticalBox->AddSlot().Padding(0, 2).AutoHeight()
        [
            SNew(SAssetDropTarget)
            .OnIsAssetAcceptableForDrop(SAssetDropTarget::FIsAssetAcceptableForDrop::CreateLambda(
                [](const UObject* InObject) {
        return InObject && InObject->IsA< UStaticMesh >();
    }))
        .OnAssetDropped(SAssetDropTarget::FOnAssetDropped::CreateUObject(
            &InParam, &UHoudiniAssetInput::OnStaticMeshDropped, AtIndex))
        [
            SAssignNew(HorizontalBox, SHorizontalBox)
        ]
        ];

    // Thumbnail : Static Mesh
    FText ParameterLabelText = FText::FromString(InParam.GetParameterLabel());
    //FText ParameterTooltip = GetParameterTooltip( &InParam, true );
    TSharedPtr< SBorder > StaticMeshThumbnailBorder;

    HorizontalBox->AddSlot().Padding(0.0f, 0.0f, 2.0f, 0.0f).AutoWidth()
        [
            SAssignNew(StaticMeshThumbnailBorder, SBorder)
            .Padding(5.0f)
        .OnMouseDoubleClick(FPointerEventHandler::CreateUObject(&InParam, &UHoudiniAssetInput::OnThumbnailDoubleClick, AtIndex))
        [
            SNew(SBox)
            .WidthOverride(64)
        .HeightOverride(64)
        .ToolTipText(ParameterLabelText)
        [
            StaticMeshThumbnail->MakeThumbnailWidget()
        ]
        ]
        ];

    StaticMeshThumbnailBorder->SetBorderImage(TAttribute< const FSlateBrush * >::Create(
        TAttribute< const FSlateBrush * >::FGetter::CreateLambda([StaticMeshThumbnailBorder]()
    {
        if (StaticMeshThumbnailBorder.IsValid() && StaticMeshThumbnailBorder->IsHovered())
            return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailLight");
        else
            return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailShadow");
    }
    )));

    FText MeshNameText = FText::GetEmpty();
    if (InputObject)
        MeshNameText = FText::FromString(InputObject->GetName());

    // ComboBox : Static Mesh
    TSharedPtr< SComboButton > StaticMeshComboButton;

    TSharedPtr< SHorizontalBox > ButtonBox;
    HorizontalBox->AddSlot()
        .FillWidth(1.0f)
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
            SAssignNew(StaticMeshComboButton, SComboButton)
            .ButtonStyle(FEditorStyle::Get(), "PropertyEditor.AssetComboStyle")
        .ForegroundColor(FEditorStyle::GetColor("PropertyEditor.AssetName.ColorAndOpacity"))
        .ContentPadding(2.0f)
        .ButtonContent()
        [
            SNew(STextBlock)
            .TextStyle(FEditorStyle::Get(), "PropertyEditor.AssetClass")
        .Font(FEditorStyle::GetFontStyle(FName(TEXT("PropertyWindow.NormalFont"))))
        .Text(MeshNameText)
        ]
        ]
        ]
        ];

    StaticMeshComboButton->SetOnGetMenuContent(FOnGetContent::CreateLambda(
        [MyParam, AtIndex, StaticMeshComboButton]()
    {
        TArray< const UClass * > AllowedClasses;
        AllowedClasses.Add(UStaticMesh::StaticClass());

        TArray< UFactory * > NewAssetFactories;
        return PropertyCustomizationHelpers::MakeAssetPickerWithMenu(
            FAssetData(MyParam->GetInputObject(AtIndex)),
            true,
            AllowedClasses,
            NewAssetFactories,
            FOnShouldFilterAsset(),
            FOnAssetSelected::CreateLambda([MyParam, AtIndex, StaticMeshComboButton](const FAssetData & AssetData) {
            if (StaticMeshComboButton.IsValid())
            {
                StaticMeshComboButton->SetIsOpen(false);

                UObject * Object = AssetData.GetAsset();
                MyParam->OnStaticMeshDropped(Object, AtIndex);
            }
        }),
            FSimpleDelegate::CreateLambda([]() {}));
    }));

    // Create tooltip.
    FFormatNamedArguments Args;
    Args.Add(TEXT("Asset"), MeshNameText);
    FText StaticMeshTooltip = FText::Format(
        LOCTEXT("BrowseToSpecificAssetInContentBrowser",
            "Browse to '{Asset}' in Content Browser"), Args);

    // Button : Browse Static Mesh
    ButtonBox->AddSlot()
        .AutoWidth()
        .Padding(2.0f, 0.0f)
        .VAlign(VAlign_Center)
        [
            PropertyCustomizationHelpers::MakeBrowseButton(
                FSimpleDelegate::CreateUObject(&InParam, &UHoudiniAssetInput::OnStaticMeshBrowse, AtIndex),
                TAttribute< FText >(StaticMeshTooltip))
        ];

    // ButtonBox : Reset
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
        .OnClicked(FOnClicked::CreateUObject(&InParam, &UHoudiniAssetInput::OnResetStaticMeshClicked, AtIndex))
        [
            SNew(SImage)
            .Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
        ]
        ];

    ButtonBox->AddSlot()
        .Padding(1.0f)
        .VAlign(VAlign_Center)
        .AutoWidth()
        [
            PropertyCustomizationHelpers::MakeInsertDeleteDuplicateButton(
                FExecuteAction::CreateLambda([MyParam, AtIndex]()
    {
        MyParam->OnInsertGeo(AtIndex);
    }
                ),
                FExecuteAction::CreateLambda([MyParam, AtIndex]()
    {
        MyParam->OnDeleteGeo(AtIndex);
    }
                ),
        FExecuteAction::CreateLambda([MyParam, AtIndex]()
    {
        MyParam->OnDuplicateGeo(AtIndex);
    }
        ))
        ];

    {
        TSharedPtr<SButton> ExpanderArrow;
        TSharedPtr<SImage> ExpanderImage;
        VerticalBox->AddSlot().Padding(0, 2).AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
            .Padding(1.0f)
            .VAlign(VAlign_Center)
            .AutoWidth()
            [
                SAssignNew(ExpanderArrow, SButton)
                .ButtonStyle(FEditorStyle::Get(), "NoBorder")
            .ClickMethod(EButtonClickMethod::MouseDown)
            .Visibility(EVisibility::Visible)
            .OnClicked(FOnClicked::CreateUObject(&InParam, &UHoudiniAssetInput::OnExpandInputTransform, AtIndex))
            [
                SAssignNew(ExpanderImage, SImage)
                .ColorAndOpacity(FSlateColor::UseForeground())
            ]
            ]
        + SHorizontalBox::Slot()
            .Padding(1.0f)
            .VAlign(VAlign_Center)
            .AutoWidth()
            [
                SNew(STextBlock)
                .Text(LOCTEXT("GeoInputTransform", "Transform Offset"))
            .ToolTipText(LOCTEXT("GeoInputTransformTooltip", "Transform offset used for correction before sending the asset to Houdini"))
            .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
            ]
            ];
        // Set delegate for image
        ExpanderImage->SetImage(
            TAttribute<const FSlateBrush*>::Create(
                TAttribute<const FSlateBrush*>::FGetter::CreateLambda([=]() {
            FName ResourceName;
            if (MyParam->TransformUIExpanded[AtIndex])
            {
                if (ExpanderArrow->IsHovered())
                    ResourceName = "TreeArrow_Expanded_Hovered";
                else
                    ResourceName = "TreeArrow_Expanded";
            }
            else
            {
                if (ExpanderArrow->IsHovered())
                    ResourceName = "TreeArrow_Collapsed_Hovered";
                else
                    ResourceName = "TreeArrow_Collapsed";
            }
            return FEditorStyle::GetBrush(ResourceName);
        })));
    }

    // TRANSFORM 
    if (InParam.TransformUIExpanded[AtIndex])
    {
        // Position
        VerticalBox->AddSlot().Padding(0, 2).AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
            .Padding(1.0f)
            .VAlign(VAlign_Center)
            .AutoWidth()
            [
                SNew(STextBlock)
                .Text(LOCTEXT("GeoInputTranslate", "T"))
            .ToolTipText(LOCTEXT("GeoInputTranslateTooltip", "Translate"))
            .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
            ]
        + SHorizontalBox::Slot().MaxWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
            [
                SNew(SVectorInputBox)
                .bColorAxisLabels(true)
            .X(TAttribute< TOptional< float > >::Create(
                TAttribute< TOptional< float > >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetInput::GetPositionX, AtIndex)))
            .Y(TAttribute< TOptional< float > >::Create(
                TAttribute< TOptional< float> >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetInput::GetPositionY, AtIndex)))
            .Z(TAttribute< TOptional< float> >::Create(
                TAttribute< TOptional< float > >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetInput::GetPositionZ, AtIndex)))
            .OnXChanged(FOnFloatValueChanged::CreateUObject(
                &InParam, &UHoudiniAssetInput::SetPositionX, AtIndex))
            .OnYChanged(FOnFloatValueChanged::CreateUObject(
                &InParam, &UHoudiniAssetInput::SetPositionY, AtIndex))
            .OnZChanged(FOnFloatValueChanged::CreateUObject(
                &InParam, &UHoudiniAssetInput::SetPositionZ, AtIndex))
            ]
            ];

        // Rotation
        VerticalBox->AddSlot().Padding(0, 2).AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
            .Padding(1.0f)
            .VAlign(VAlign_Center)
            .AutoWidth()
            [
                SNew(STextBlock)
                .Text(LOCTEXT("GeoInputRotate", "R"))
            .ToolTipText(LOCTEXT("GeoInputRotateTooltip", "Rotate"))
            .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
            ]
        + SHorizontalBox::Slot().MaxWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
            [
                SNew(SRotatorInputBox)
                .AllowSpin(true)
            .bColorAxisLabels(true)
            .Roll(TAttribute< TOptional< float > >::Create(
                TAttribute< TOptional< float > >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetInput::GetRotationRoll, AtIndex)))
            .Pitch(TAttribute< TOptional< float> >::Create(
                TAttribute< TOptional< float > >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetInput::GetRotationPitch, AtIndex)))
            .Yaw(TAttribute<TOptional< float > >::Create(
                TAttribute< TOptional< float > >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetInput::GetRotationYaw, AtIndex)))
            .OnRollChanged(FOnFloatValueChanged::CreateUObject(
                &InParam, &UHoudiniAssetInput::SetRotationRoll, AtIndex))
            .OnPitchChanged(FOnFloatValueChanged::CreateUObject(
                &InParam, &UHoudiniAssetInput::SetRotationPitch, AtIndex))
            .OnYawChanged(FOnFloatValueChanged::CreateUObject(
                &InParam, &UHoudiniAssetInput::SetRotationYaw, AtIndex))
            ]
            ];

        // Scale
        VerticalBox->AddSlot().Padding(0, 2).AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
            .Padding(1.0f)
            .VAlign(VAlign_Center)
            .AutoWidth()
            [
                SNew(STextBlock)
                .Text(LOCTEXT("GeoInputScale", "S"))
            .ToolTipText(LOCTEXT("GeoInputScaleTooltip", "Scale"))
            .Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
            ]
        + SHorizontalBox::Slot().MaxWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
            [
                SNew(SVectorInputBox)
                .bColorAxisLabels(true)
            .X(TAttribute< TOptional< float > >::Create(
                TAttribute< TOptional< float > >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetInput::GetScaleX, AtIndex)))
            .Y(TAttribute< TOptional< float > >::Create(
                TAttribute< TOptional< float> >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetInput::GetScaleY, AtIndex)))
            .Z(TAttribute< TOptional< float> >::Create(
                TAttribute< TOptional< float > >::FGetter::CreateUObject(
                    &InParam, &UHoudiniAssetInput::GetScaleZ, AtIndex)))
            .OnXChanged(FOnFloatValueChanged::CreateUObject(
                &InParam, &UHoudiniAssetInput::SetScaleX, AtIndex))
            .OnYChanged(FOnFloatValueChanged::CreateUObject(
                &InParam, &UHoudiniAssetInput::SetScaleY, AtIndex))
            .OnZChanged(FOnFloatValueChanged::CreateUObject(
                &InParam, &UHoudiniAssetInput::SetScaleZ, AtIndex))
            ]
        /*
        + SHorizontalBox::Slot().AutoWidth()
        [
        // Add a checkbox to toggle between preserving the ratio of x,y,z components of scale when a value is entered
        SNew( SCheckBox )
        .Style( FEditorStyle::Get(), "TransparentCheckBox" )
        .ToolTipText( LOCTEXT( "PreserveScaleToolTip", "When locked, scales uniformly based on the current xyz scale values so the object maintains its shape in each direction when scaled" ) )
        .OnCheckStateChanged( FOnCheckStateChanged::CreateUObject(
        this, &UHoudiniAssetInput::CheckStateChanged, AtIndex ) )
        .IsChecked( TAttribute< ECheckBoxState >::Create(
        TAttribute<ECheckBoxState>::FGetter::CreateUObject(
        this, &UHoudiniAssetInput::IsChecked, AtIndex ) ) )
        [
        SNew( SImage )
        .Image( TAttribute<const FSlateBrush*>::Create(
        TAttribute<const FSlateBrush*>::FGetter::CreateUObject(
        this, &UHoudiniAssetInput::GetPreserveScaleRatioImage, AtIndex ) ) )
        .ColorAndOpacity( FSlateColor::UseForeground() )
        ]
        ]
        */
            ];
    }
}

FReply
FHoudiniParameterDetails::Helper_OnButtonClickSelectActors(TWeakObjectPtr<class UHoudiniAssetInput> InParam, FName DetailsPanelName)
{
    if (!InParam.IsValid())
        return FReply::Handled();

    // There's no undo operation for button.

    FPropertyEditorModule & PropertyModule =
        FModuleManager::Get().GetModuleChecked< FPropertyEditorModule >("PropertyEditor");

    // Locate the details panel.
    TSharedPtr< IDetailsView > DetailsView = PropertyModule.FindDetailView(DetailsPanelName);

    if (!DetailsView.IsValid())
        return FReply::Handled();

    class SLocalDetailsView : public SDetailsViewBase
    {
    public:
        void LockDetailsView() { SDetailsViewBase::bIsLocked = true; }
        void UnlockDetailsView() { SDetailsViewBase::bIsLocked = false; }
    };
    auto * LocalDetailsView = static_cast< SLocalDetailsView * >(DetailsView.Get());

    if (!DetailsView->IsLocked())
    {
        LocalDetailsView->LockDetailsView();
        check(DetailsView->IsLocked());

        // Force refresh of details view.
        InParam->OnParamStateChanged();

        // Select the previously chosen input Actors from the World Outliner.
        GEditor->SelectNone(false, true);
        for (auto & OutlinerMesh : InParam->InputOutlinerMeshArray)
        {
            if (OutlinerMesh.ActorPtr.IsValid())
                GEditor->SelectActor(OutlinerMesh.ActorPtr.Get(), true, true);
        }

        return FReply::Handled();
    }

    if (!GEditor || !GEditor->GetSelectedObjects())
        return FReply::Handled();

    // If details panel is locked, locate selected actors and check if this component belongs to one of them.

    FScopedTransaction Transaction(
        TEXT(HOUDINI_MODULE_RUNTIME),
        LOCTEXT("HoudiniInputChange", "Houdini World Outliner Input Change"),
        InParam->PrimaryObject);
    InParam->Modify();

    InParam->MarkPreChanged();
    InParam->bStaticMeshChanged = true;

    // Delete all assets and reset the array.
    // TODO: Make this process a little more efficient.
    InParam->DisconnectAndDestroyInputAsset();
    InParam->InputOutlinerMeshArray.Empty();

    USelection * SelectedActors = GEditor->GetSelectedActors();

    // If the builder brush is selected, first deselect it.
    for (FSelectionIterator It(*SelectedActors); It; ++It)
    {
        AActor * Actor = Cast< AActor >(*It);
        if (!Actor)
            continue;

        // Don't allow selection of ourselves. Bad things happen if we do.
        if (InParam->GetHoudiniAssetComponent() && (Actor == InParam->GetHoudiniAssetComponent()->GetOwner()))
            continue;

        InParam->UpdateInputOulinerArrayFromActor(Actor, false);
    }

    InParam->MarkChanged();

    AActor* HoudiniAssetActor = InParam->GetHoudiniAssetComponent()->GetOwner();

    if (DetailsView->IsLocked())
    {
        LocalDetailsView->UnlockDetailsView();
        check(!DetailsView->IsLocked());

        TArray< UObject * > DummySelectedActors;
        DummySelectedActors.Add(HoudiniAssetActor);

        // Reset selected actor to itself, force refresh and override the lock.
        DetailsView->SetObjects(DummySelectedActors, true, true);
    }

    // Reselect the Asset Actor. If we don't do this, our Asset parameters will stop
    // refreshing and the user will be very confused. It is also resetting the state
    // of the selection before the input actor selection process was started.
    GEditor->SelectNone(false, true);
    GEditor->SelectActor(HoudiniAssetActor, true, true);

    // Update parameter layout.
    InParam->OnParamStateChanged();

    // Start or stop the tick timer to check if the selected Actors have been transformed.
    if (InParam->InputOutlinerMeshArray.Num() > 0)
        InParam->StartWorldOutlinerTicking();
    else if (InParam->InputOutlinerMeshArray.Num() <= 0)
        InParam->StopWorldOutlinerTicking();

    return FReply::Handled();
}