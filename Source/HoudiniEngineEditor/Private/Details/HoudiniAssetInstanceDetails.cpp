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
#include "HoudiniAssetInstanceDetails.h"
#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngineBakeUtils.h"
#include "HoudiniAsset.h"
#include "HoudiniAssetInstance.h"
#include "HoudiniAssetLogWidget.h"
#include "HoudiniEngineString.h"
#include "HoudiniParameterDetails.h"

#include "Parameters/HoudiniAssetInput.h"
#include "Parameters/HoudiniAssetInstanceInput.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "ContentBrowserModule.h"
#include "Editor/PropertyEditor/Public/PropertyCustomizationHelpers.h"
#include "DetailWidgetRow.h"
#include "Editor.h"
#include "IContentBrowserSingleton.h"
#include "Landscape.h"
#include "SAssetDropTarget.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Images/SImage.h"
#include "Framework/Application/SlateApplication.h"

#include "HoudiniEngineRuntimePrivatePCH.h"
#include "Internationalization/Internationalization.h"
#include "DetailLayoutBuilder.h"
#include "IDetailGroup.h"
#include <SComboBox.h>

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE 

TSharedRef<IDetailCustomization>
FHoudiniAssetInstanceDetails::MakeInstance()
{
    return MakeShareable(new FHoudiniAssetInstanceDetails);
}

FHoudiniAssetInstanceDetails::FHoudiniAssetInstanceDetails() { }

FHoudiniAssetInstanceDetails::~FHoudiniAssetInstanceDetails() { }

void
FHoudiniAssetInstanceDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
    // Get all components which are being customized.
    TArray< TWeakObjectPtr<UObject>> ObjectsCustomized;
    DetailBuilder.GetObjectsBeingCustomized(ObjectsCustomized);

    auto HoudiniInstancesBeingCustomized = 0;

    // See if we need to enable baking option.
    for (int32 i = 0; i < ObjectsCustomized.Num(); ++i)
    {
        if (ObjectsCustomized[i].IsValid())
        {
            UObject * Object = ObjectsCustomized[i].Get();
            if (Object)
            {
                HoudiniAssetInstance = Cast<UHoudiniAssetInstance>(Object);
                HoudiniInstancesBeingCustomized++;
            }
        }
    }

    // For now only support single instance editing
    if (HoudiniInstancesBeingCustomized != 1)
        return;

    HoudiniAssetInstance->GetOnInstantiated().AddRaw(this, &FHoudiniAssetInstanceDetails::OnInstantiated);
    HoudiniAssetInstance->GetOnCooked().AddRaw(this, &FHoudiniAssetInstanceDetails::OnCooked);

    // Create Houdini Asset category.
    {
        IDetailCategoryBuilder& DetailCategoryBuilder = DetailBuilder.EditCategory("Asset", FText::GetEmpty(), ECategoryPriority::Important);
        
        // If we are running Houdini Engine Indie license, we need to display special label.
        if (FHoudiniEngineUtils::IsLicenseHoudiniEngineIndie())
        {
            FText ParameterLabelText = FText::FromString(TEXT("Houdini Engine Indie - For Limited Commercial Use Only"));

            FSlateFontInfo LargeDetailsFont = IDetailLayoutBuilder::GetDetailFontBold();
            LargeDetailsFont.Size += 2;

            FSlateColor LabelColor = FLinearColor(1.0f, 1.0f, 0.0f, 1.0f);

            DetailCategoryBuilder.AddCustomRow(FText::GetEmpty())
            [
                SNew(STextBlock)
                .Text(ParameterLabelText)
                .ToolTipText(ParameterLabelText)
                .Font(LargeDetailsFont)
                .Justification(ETextJustify::Center)
                .ColorAndOpacity(LabelColor)
            ];

            DetailCategoryBuilder.AddCustomRow(FText::GetEmpty())
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot()
                .Padding(0, 0, 5, 0)
                [
                    SNew(SSeparator)
                    .Thickness(2.0f)
                ]
            ];
        }

        /* From SPluginBrowser line 136 ish */
        DetailCategoryBuilder.AddCustomRow(FText::GetEmpty())
        [
            SNew(SBorder)
            .BorderBackgroundColor(FLinearColor::Red)
            .BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
            .Padding(8.0f)
            .Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &FHoudiniAssetInstanceDetails::GetLastErrorVisibility)))
            [
                SNew(SHorizontalBox)
                //+SHorizontalBox::Slot()
                //.AutoWidth()
                //.VAlign(VAlign_Center)
                //.Padding(FMargin(0,0,8,0))
                //[
                //    SNew(SImage)
                //    .Image(FPluginStyle::Get()->GetBrush("Plugins.Warning"))
                //]
                +SHorizontalBox::Slot()
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text_Raw(this, &FHoudiniAssetInstanceDetails::GetLastErrorText)
                ]
            ]
        ];

        CreateAssetWidget(DetailCategoryBuilder);
    }

    //if (!HoudiniAssetInstance->bIsInstantiated)
    //    return;

    // Create Houdini Inputs.
    {
        IDetailCategoryBuilder& DetailCategoryBuilder = DetailBuilder.EditCategory("Inputs", FText::GetEmpty(), ECategoryPriority::Important);
        //IDetailGroup& InputsGroup = DetailCategoryBuilder.AddGroup(TEXT("Inputs"), FText::GetEmpty(), false, true);
        for (auto HoudiniAssetInput : HoudiniAssetInstance->GetInputs())
            if (HoudiniAssetInput) FHoudiniParameterDetails::CreateWidget(DetailCategoryBuilder, HoudiniAssetInput);
    }

    // Create Houdini Instanced Inputs category.
    {
        IDetailCategoryBuilder& DetailCategoryBuilder = DetailBuilder.EditCategory("InstancedInputs", FText::GetEmpty(), ECategoryPriority::Important);
        for (auto InstanceInput : HoudiniAssetInstance->GetInstanceInputs())
            if (InstanceInput)
                FHoudiniParameterDetails::CreateWidget(DetailCategoryBuilder, InstanceInput);
    }

    // Create Houdini parameters.
    {
        IDetailCategoryBuilder& DetailCategoryBuilder = DetailBuilder.EditCategory("Parameters", FText::GetEmpty(), ECategoryPriority::Important);
        for (auto HoudiniAssetParameter : HoudiniAssetInstance->GetParameters())
            if (HoudiniAssetParameter && !HoudiniAssetParameter->IsChildParameter() && !HoudiniAssetParameter->IsPendingKill())
                FHoudiniParameterDetails::CreateWidget(DetailCategoryBuilder, HoudiniAssetParameter);
    }
}

void 
FHoudiniAssetInstanceDetails::CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& DetailBuilder)
{
    CachedDetailBuilder = DetailBuilder;
    CustomizeDetails(*DetailBuilder);
}

void
FHoudiniAssetInstanceDetails::CreateAssetWidget(IDetailCategoryBuilder& DetailCategoryBuilder)
{
    DetailCategoryBuilder.AddProperty(TEXT("Asset"));

    auto& AssetName = DetailCategoryBuilder.AddProperty(TEXT("AssetName"));
    auto AssetNameHandle = AssetName.GetPropertyHandle();

    AssetName
        .EditCondition(HasValidAssetAttribute(), nullptr)
        .CustomWidget()
        .NameContent()
        [
            AssetNameHandle->CreatePropertyNameWidget()
        ]
        .ValueContent()
        [
            PropertyCustomizationHelpers::MakePropertyComboBox(
                AssetName.GetPropertyHandle(),
                FOnGetPropertyComboBoxStrings::CreateRaw(this, &FHoudiniAssetInstanceDetails::GenerateAssetNameList))
        ];

    if (!HasValidInstance())
        return;

    //DetailCategoryBuilder.AddCustomRow(FText::GetEmpty())
    //    [
    //        SAssignNew(AssetNameComboBox, SComboBox<TSharedPtr<FString>>)
    //        .OptionsSource(&AssetNameList)
    //    .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
    //    return SNew(STextBlock)
    //        .Font(IDetailLayoutBuilder::GetDetailFont())
    //        .Text(FText::FromString(*Item));
    //})
    //    .OnSelectionChanged_Lambda([=](TSharedPtr<FString> Item, ESelectInfo::Type)
    //{
    //    if (HasValidInstance())
    //    {

    //    }
    //})
    //    .ContentPadding(FMargin(2.0f, 2.0f))
    //    ];

    return;

    /*
    // Reset Houdini asset related widgets.
    HoudiniAssetComboButton.Reset();
    HoudiniAssetThumbnailBorder.Reset();

    // Get thumbnail pool for this builder.
    auto& DetailLayoutBuilder = DetailCategoryBuilder.GetParentLayout();
    TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool = DetailLayoutBuilder.GetThumbnailPool();

    FSlateFontInfo NormalFont = FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont"));
    UHoudiniAsset* HoudiniAsset = nullptr;
    UHoudiniAssetInstance* HoudiniAssetInstance = nullptr;
    FString HoudiniAssetPathName = TEXT("");
    FString HoudiniAssetName = TEXT("");

    if (HoudiniAssetInstance.IsValid())
    {
        HoudiniAsset = HoudiniAssetInstance->GetAsset();

        if (HoudiniAsset)
        {
            HoudiniAssetPathName = HoudiniAsset->GetPathName();
            HoudiniAssetName = HoudiniAsset->GetName();
        }
    }

    // Create thumbnail for this Houdini asset.
    TSharedPtr<FAssetThumbnail> HoudiniAssetThumbnail = MakeShareable(new FAssetThumbnail(HoudiniAsset, 64, 64, AssetThumbnailPool));

    auto& OptionsGroup = DetailCategoryBuilder.AddGroup(TEXT("Options"), LOCTEXT("Options", "Asset Options"));
    OptionsGroup
        .AddWidgetRow()
        .NameContent()
        [
            SNew(STextBlock)
            .Text(LOCTEXT("HoudiniDigitalAsset", "Houdini Digital Asset"))
            .Font(NormalFont)
        ]
        .ValueContent()
        .MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
        [
            SNew(SAssetDropTarget)
            .OnIsAssetAcceptableForDrop(this, &FHoudiniAssetInstanceDetails::OnHoudiniAssetDraggedOver)
            .OnAssetDropped(this, &FHoudiniAssetInstanceDetails::OnHoudiniAssetDropped)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .Padding(0.0f, 0.0f, 2.0f, 0.0f)
                .AutoWidth()
                [
                    SAssignNew(HoudiniAssetThumbnailBorder, SBorder)
                    .Padding(5.0f)
                    .BorderImage(this, &FHoudiniAssetInstanceDetails::GetHoudiniAssetThumbnailBorder)
                    [
                        SNew(SBox)
                        .WidthOverride(64)
                        .HeightOverride(64)
                        .ToolTipText(FText::FromString(HoudiniAssetPathName))
                        [
                            HoudiniAssetThumbnail->MakeThumbnailWidget()
                        ]
                    ]
                ]
                +SHorizontalBox::Slot()
                .FillWidth(1.f)
                .Padding(0.0f, 4.0f, 4.0f, 4.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(SVerticalBox)
                    +SVerticalBox::Slot()
                    .HAlign(HAlign_Fill)
                    [
                        SNew(SHorizontalBox)
                        +SHorizontalBox::Slot()
                        [
                            SAssignNew(HoudiniAssetComboButton, SComboButton)
                            .ButtonStyle(FEditorStyle::Get(), "PropertyEditor.AssetComboStyle")
                            .ForegroundColor(FEditorStyle::GetColor("PropertyEditor.AssetName.ColorAndOpacity"))
                            .OnGetMenuContent(this, &FHoudiniAssetInstanceDetails::OnGetHoudiniAssetMenuContent)
                            .ContentPadding(2.0f)
                            .ButtonContent()
                            [
                                SNew(STextBlock)
                                .TextStyle(FEditorStyle::Get(), "PropertyEditor.AssetClass")
                                .Font(NormalFont)
                                .Text(FText::FromString(HoudiniAssetName))
                            ]
                        ]
                        +SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(2.0f, 0.0f)
                        .VAlign(VAlign_Center)
                        [
                            PropertyCustomizationHelpers::MakeBrowseButton(
                                FSimpleDelegate::CreateSP(this, &FHoudiniAssetInstanceDetails::OnHoudiniAssetBrowse),
                                TAttribute< FText >(FText::FromString(HoudiniAssetName)))
                        ]
                        +SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(2.0f, 0.0f)
                        .VAlign(VAlign_Center)
                        [
                            SNew(SButton)
                            .ToolTipText(LOCTEXT("ResetToBaseHoudiniAsset", "Reset Houdini Asset"))
                            .ButtonStyle(FEditorStyle::Get(), "NoBorder")
                            .ContentPadding(0)
                            .Visibility(EVisibility::Visible)
                            .OnClicked(this, &FHoudiniAssetInstanceDetails::OnResetHoudiniAssetClicked)
                            [
                                SNew(SImage)
                                .Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
                            ]
                        ]  // horizontal buttons next to thumbnail box
                    ]
                ]  // horizontal asset chooser box
            ]
        ];

    auto AddOptionRow = [&](const FText& NameText, FOnCheckStateChanged OnCheckStateChanged, TAttribute<ECheckBoxState> IsCheckedAttr)
    {
        OptionsGroup
            .AddWidgetRow()
            .NameContent()
            [
                SNew(STextBlock)
                .Text(NameText)
                .Font(NormalFont)
            ]
            .ValueContent()
            [
                SNew(SCheckBox)
                .OnCheckStateChanged(OnCheckStateChanged)
                .IsChecked(IsCheckedAttr)
            ];
    };

    AddOptionRow(LOCTEXT("HoudiniEnableCookingOnParamChange", "Enable Cooking on Parameter Change"),
        FOnCheckStateChanged::CreateSP(this, &FHoudiniAssetInstanceDetails::CheckStateChangedComponentSettingCooking, HoudiniAssetComponent),
        TAttribute<ECheckBoxState>::Create(TAttribute<ECheckBoxState>::FGetter::CreateSP(this, &FHoudiniAssetInstanceDetails::IsCheckedComponentSettingCooking, HoudiniAssetComponent)));

    AddOptionRow(LOCTEXT("HoudiniUploadTransformsToHoudiniEngine", "Upload Transforms to Houdini Engine"),
        FOnCheckStateChanged::CreateSP(this, &FHoudiniAssetInstanceDetails::CheckStateChangedComponentSettingUploadTransform, HoudiniAssetComponent),
        TAttribute<ECheckBoxState>::Create(TAttribute<ECheckBoxState>::FGetter::CreateSP(this, &FHoudiniAssetInstanceDetails::IsCheckedComponentSettingUploadTransform, HoudiniAssetComponent)));

    AddOptionRow(LOCTEXT("HoudiniTransformChangeTriggersCooks", "Transform Change Triggers Cooks"),
        FOnCheckStateChanged::CreateSP(this, &FHoudiniAssetInstanceDetails::CheckStateChangedComponentSettingTransformCooking, HoudiniAssetComponent),
        TAttribute<ECheckBoxState>::Create(TAttribute<ECheckBoxState>::FGetter::CreateSP(this, &FHoudiniAssetInstanceDetails::IsCheckedComponentSettingTransformCooking, HoudiniAssetComponent)));

    AddOptionRow(LOCTEXT("HoudiniUseHoudiniMaterials", "Use Native Houdini Materials"),
        FOnCheckStateChanged::CreateSP(this, &FHoudiniAssetInstanceDetails::CheckStateChangedComponentSettingUseHoudiniMaterials, HoudiniAssetComponent),
        TAttribute<ECheckBoxState>::Create(TAttribute<ECheckBoxState>::FGetter::CreateSP(this, &FHoudiniAssetInstanceDetails::IsCheckedComponentSettingUseHoudiniMaterials, HoudiniAssetComponent)));

    AddOptionRow(LOCTEXT("HoudiniCookingTriggersDownstreamCooks", "Cooking Triggers Downstream Cooks"),
        FOnCheckStateChanged::CreateSP(this, &FHoudiniAssetInstanceDetails::CheckStateChangedComponentSettingCookingTriggersDownstreamCooks, HoudiniAssetComponent),
        TAttribute<ECheckBoxState>::Create(TAttribute<ECheckBoxState>::FGetter::CreateSP(this, &FHoudiniAssetInstanceDetails::IsCheckedComponentSettingCookingTriggersDownstreamCooks, HoudiniAssetComponent)));

    auto ActionButtonSlot = [&](const FText& InText, const FText& InToolTipText, FOnClicked InOnClicked) -> SHorizontalBox::FSlot&
    {
        return SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.0f, 0.0f)
            .VAlign(VAlign_Center)
            .HAlign(HAlign_Center)
            [
                SNew(SButton)
                .VAlign(VAlign_Center)
                .HAlign(HAlign_Center)
                .OnClicked(InOnClicked)
                .Text(InText)
                .ToolTipText(InToolTipText)
            ];
    };

    auto& CookGroup = DetailCategoryBuilder.AddGroup(TEXT("Cooking"), LOCTEXT("CookingActions", "Cooking Actions"));
    CookGroup
        .AddWidgetRow()
        .WholeRowContent()
        [
            SNew(SHorizontalBox)
            +ActionButtonSlot(
                LOCTEXT("RecookHoudiniActor", "Recook Asset"),
                LOCTEXT("RecookHoudiniActorToolTip", "Recooks the outputs of the Houdini asset"),
                FOnClicked::CreateSP(this, &FHoudiniAssetInstanceDetails::OnRecookAsset))
            +ActionButtonSlot(
                LOCTEXT("RebuildHoudiniActor", "Rebuild Asset"),
                LOCTEXT("RebuildHoudiniActorToolTip", "Deletes and then re-creates and re-cooks the Houdini asset"),
                FOnClicked::CreateSP(this, &FHoudiniAssetInstanceDetails::OnRebuildAsset))
            +ActionButtonSlot(
                LOCTEXT("ResetHoudiniActor", "Reset Parameters"),
                LOCTEXT("ResetHoudiniActorToolTip", "Resets parameters to their default values"),
                FOnClicked::CreateSP(this, &FHoudiniAssetInstanceDetails::OnResetAsset))
        ];

        // Cook folder widget
        CookGroup.AddWidgetRow()
        .NameContent()
        [
            SNew(STextBlock)
            .Text(LOCTEXT("CookFolder", "Temporary Cook Folder"))
            .Font(NormalFont)
        ]
        .ValueContent()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SEditableText)
                .IsReadOnly(true)
                .Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &FHoudiniAssetInstanceDetails::GetTempCookFolderText)))
                .Font(IDetailLayoutBuilder::GetDetailFont())
                .ToolTipText(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &FHoudiniAssetInstanceDetails::GetTempCookFolderText)))
            ]
        ];

    auto& BakeGroup = DetailCategoryBuilder.AddGroup(TEXT("Baking"), LOCTEXT("Baking", "Baking"));
    TSharedPtr<SButton> BakeToInputButton;
    BakeGroup
        .AddWidgetRow()
        .WholeRowContent()
        [
            SNew(SHorizontalBox)
            +ActionButtonSlot(
                LOCTEXT("BakeBlueprintHoudiniActor", "Bake Blueprint"),
                LOCTEXT("BakeBlueprintHoudiniActorToolTip", "Bakes to a new Blueprint"),
                FOnClicked::CreateSP(this, &FHoudiniAssetInstanceDetails::OnBakeBlueprint))
            +ActionButtonSlot(
                LOCTEXT("BakeReplaceBlueprintHoudiniActor", "Replace With Blueprint"),
                LOCTEXT("BakeReplaceBlueprintHoudiniActorToolTip", "Bakes to a new Blueprint and replaces this Actor"),
                FOnClicked::CreateSP(this, &FHoudiniAssetInstanceDetails::OnBakeBlueprintReplace))
            +ActionButtonSlot(
                LOCTEXT("BakeToActors", "Bake to Actors"),
                LOCTEXT("BakeToActorsTooltip", "Bakes each output and creates new individual Actors"),
                FOnClicked::CreateSP(this, &FHoudiniAssetInstanceDetails::OnBakeToActors))
            +SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(2.0f, 0.0f)
            .VAlign(VAlign_Center)
            .HAlign(HAlign_Center)
            [
                SAssignNew(BakeToInputButton, SButton)
                .VAlign(VAlign_Center)
                .HAlign(HAlign_Center)
                .OnClicked(this, &FHoudiniAssetInstanceDetails::OnBakeToInput)
                .Text(LOCTEXT("BakeToInput", "Bake to Outliner Input"))
                .ToolTipText(LOCTEXT("BakeToInputTooltip", "Bakes single static mesh and sets it on the first outliner input actor and then disconnects it.\nNote: There must be one static mesh outliner input and one generated mesh."))
            ]
        ];

    // Bake folder widget
    FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
    FPathPickerConfig PathPickerConfig;
    PathPickerConfig.OnPathSelected = FOnPathSelected::CreateSP(this, &FHoudiniAssetInstanceDetails::OnBakeFolderSelected);
    PathPickerConfig.bAllowContextMenu = false;
    PathPickerConfig.bAllowClassesFolder = true;

    BakeGroup
        .AddWidgetRow()
        .NameContent()
        [
            SNew(STextBlock)
            .Text(LOCTEXT("BakeFolder", "Bake Folder"))
            .Font(NormalFont)
        ]
        .ValueContent()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            [
                SNew(SEditableText)
                .IsReadOnly(true)
                .Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &FHoudiniAssetInstanceDetails::GetBakeFolderText)))
                .Font(IDetailLayoutBuilder::GetDetailFont())
                .ToolTipText(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &FHoudiniAssetInstanceDetails::GetBakeFolderText)))
            ]
            +SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SComboButton)
                .ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
                .ToolTipText(LOCTEXT("ChooseABakeFolder", "Choose a baking output folder"))
                .ContentPadding(2.0f)
                .ForegroundColor(FSlateColor::UseForeground())
                .IsFocusable(false)
                .MenuContent()
                [
                    SNew(SBox)
                    .WidthOverride(300.0f)
                    .HeightOverride(300.0f)
                    [
                        ContentBrowserModule.Get().CreatePathPicker(PathPickerConfig)
                    ]
                ]
            ]
        ];

    BakeToInputButton->SetEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda([=] {
        return FHoudiniEngineBakeUtils::GetCanComponentBakeToOutlinerInput(HoudiniAssetComponent);
    })));

    auto& HelpGroup = DetailCategoryBuilder.AddGroup(TEXT("Help"), LOCTEXT("Help", "Help and Debugging"));
    HelpGroup
        .AddWidgetRow()
        .WholeRowContent()
        [
            SNew(SHorizontalBox)
            +ActionButtonSlot(
                LOCTEXT("FetchCookLogHoudiniActor", "Fetch Cook Log"),
                LOCTEXT("FetchCookLogHoudiniActorToolTip", "Fetches the cook log from Houdini"),
                FOnClicked::CreateSP(this, &FHoudiniAssetInstanceDetails::OnFetchCookLog))
        + ActionButtonSlot(
            LOCTEXT("FetchAssetHelpHoudiniActor", "Asset Help"),
            LOCTEXT("FetchAssetHelpHoudiniActorToolTip", "Displays the asset's Help text"),
            FOnClicked::CreateSP(this, &FHoudiniAssetInstanceDetails::OnFetchAssetHelp, HoudiniAssetComponent))
        ];
        */
}

void FHoudiniAssetInstanceDetails::OnInstantiated()
{
    HOUDINI_LOG_MESSAGE(TEXT("OnInstantiated"));

    RefreshDetailBuilder();
}

void FHoudiniAssetInstanceDetails::OnCooked()
{
    HOUDINI_LOG_MESSAGE(TEXT("OnCooked"));

    RefreshDetailBuilder();
}

FText FHoudiniAssetInstanceDetails::GetLastErrorText() const
{
    if (!HasValidInstance())
        return FText::GetEmpty();

    return FText::FromString(HoudiniAssetInstance->GetLastError());
}

EVisibility FHoudiniAssetInstanceDetails::GetLastErrorVisibility() const
{
    if (!HasValidInstance())
        return EVisibility::Hidden;

    return HoudiniAssetInstance->HasError() ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FHoudiniAssetInstanceDetails::GetIsInstantiatedVisibility() const
{
    if (!HasValidInstance())
        return EVisibility::Hidden;
    
    return HoudiniAssetInstance->bIsInstantiated ? EVisibility::Visible : EVisibility::Collapsed;
}

void FHoudiniAssetInstanceDetails::RefreshDetailBuilder()
{
    if (!CachedDetailBuilder.IsValid())
        return;

    IDetailLayoutBuilder* DetailBuilder = CachedDetailBuilder.Pin().Get();
    if (DetailBuilder)
        DetailBuilder->ForceRefreshDetails();
}

TAttribute<bool> FHoudiniAssetInstanceDetails::HasValidAssetAttribute() const
{
    auto Lambda = [&]()
    {
        return HasValidInstance() && HoudiniAssetInstance->GetAsset() != nullptr;
    };

    return TAttribute<bool>::Create(Lambda);
}

void FHoudiniAssetInstanceDetails::GenerateAssetNameList(TArray<TSharedPtr<FString>>& OutComboBoxStrings, TArray<TSharedPtr<SToolTip>>& OutToolTips, TArray<bool>& OutRestrictedItems)
{
    if (!HasValidInstance())
    {
        OutComboBoxStrings.Empty();
        return;
    }

    OutComboBoxStrings.Empty(HoudiniAssetInstance->GetAssetNames().Num());
    for (auto& Name : HoudiniAssetInstance->GetAssetNames())
    {
        OutComboBoxStrings.Add(MakeShareable(new FString(Name)));
        OutToolTips.Add(SNew(SToolTip).Text(FText::FromString(Name)));
        OutRestrictedItems.Add(false);
    }
}

#undef LOCTEXT_NAMESPACE