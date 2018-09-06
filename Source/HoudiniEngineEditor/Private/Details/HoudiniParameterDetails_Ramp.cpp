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

/** We need to inherit from curve editor in order to get subscription to mouse events. **/
class SHoudiniAssetParameterRampCurveEditor : public SCurveEditor
{
public:

    SLATE_BEGIN_ARGS(SHoudiniAssetParameterRampCurveEditor)
        : _ViewMinInput(0.0f)
        , _ViewMaxInput(10.0f)
        , _ViewMinOutput(0.0f)
        , _ViewMaxOutput(1.0f)
        , _InputSnap(0.1f)
        , _OutputSnap(0.05f)
        , _InputSnappingEnabled(false)
        , _OutputSnappingEnabled(false)
        , _ShowTimeInFrames(false)
        , _TimelineLength(5.0f)
        , _DesiredSize(FVector2D::ZeroVector)
        , _DrawCurve(true)
        , _HideUI(true)
        , _AllowZoomOutput(true)
        , _AlwaysDisplayColorCurves(false)
        , _ZoomToFitVertical(true)
        , _ZoomToFitHorizontal(true)
        , _ShowZoomButtons(true)
        , _XAxisName()
        , _YAxisName()
        , _ShowInputGridNumbers(true)
        , _ShowOutputGridNumbers(true)
        , _ShowCurveSelector(true)
        , _GridColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.3f))
    {
        _Clipping = EWidgetClipping::ClipToBounds;
    }

    SLATE_ATTRIBUTE(float, ViewMinInput)
        SLATE_ATTRIBUTE(float, ViewMaxInput)
        SLATE_ATTRIBUTE(TOptional<float>, DataMinInput)
        SLATE_ATTRIBUTE(TOptional<float>, DataMaxInput)
        SLATE_ATTRIBUTE(float, ViewMinOutput)
        SLATE_ATTRIBUTE(float, ViewMaxOutput)
        SLATE_ATTRIBUTE(float, InputSnap)
        SLATE_ATTRIBUTE(float, OutputSnap)
        SLATE_ATTRIBUTE(bool, InputSnappingEnabled)
        SLATE_ATTRIBUTE(bool, OutputSnappingEnabled)
        SLATE_ATTRIBUTE(bool, ShowTimeInFrames)
        SLATE_ATTRIBUTE(float, TimelineLength)
        SLATE_ATTRIBUTE(FVector2D, DesiredSize)
        SLATE_ATTRIBUTE(bool, AreCurvesVisible)
        SLATE_ARGUMENT(bool, DrawCurve)
        SLATE_ARGUMENT(bool, HideUI)
        SLATE_ARGUMENT(bool, AllowZoomOutput)
        SLATE_ARGUMENT(bool, AlwaysDisplayColorCurves)
        SLATE_ARGUMENT(bool, ZoomToFitVertical)
        SLATE_ARGUMENT(bool, ZoomToFitHorizontal)
        SLATE_ARGUMENT(bool, ShowZoomButtons)
        SLATE_ARGUMENT(TOptional<FString>, XAxisName)
        SLATE_ARGUMENT(TOptional<FString>, YAxisName)
        SLATE_ARGUMENT(bool, ShowInputGridNumbers)
        SLATE_ARGUMENT(bool, ShowOutputGridNumbers)
        SLATE_ARGUMENT(bool, ShowCurveSelector)
        SLATE_ARGUMENT(FLinearColor, GridColor)
        SLATE_EVENT(FOnSetInputViewRange, OnSetInputViewRange)
        SLATE_EVENT(FOnSetOutputViewRange, OnSetOutputViewRange)
        SLATE_EVENT(FOnSetAreCurvesVisible, OnSetAreCurvesVisible)
        SLATE_EVENT(FSimpleDelegate, OnCreateAsset)
        SLATE_END_ARGS()

public:

    /** Widget construction. **/
    void Construct(const FArguments & InArgs);

protected:

    /** Handle mouse up events. **/
    virtual FReply OnMouseButtonUp(const FGeometry & MyGeometry, const FPointerEvent & MouseEvent) override;

public:

    /** Set parent ramp parameter. **/
    void SetParentRampParameter(UHoudiniAssetParameterRamp * InHoudiniAssetParameterRamp)
    {
        HoudiniAssetParameterRamp = InHoudiniAssetParameterRamp;
    }

protected:

    /** Parent ramp parameter. **/
    TWeakObjectPtr<UHoudiniAssetParameterRamp> HoudiniAssetParameterRamp;
};

void
SHoudiniAssetParameterRampCurveEditor::Construct(const FArguments & InArgs)
{
    SCurveEditor::Construct(SCurveEditor::FArguments()
        .ViewMinInput(InArgs._ViewMinInput)
        .ViewMaxInput(InArgs._ViewMaxInput)
        .ViewMinOutput(InArgs._ViewMinOutput)
        .ViewMaxOutput(InArgs._ViewMaxOutput)
        .XAxisName(InArgs._XAxisName)
        .YAxisName(InArgs._YAxisName)
        .HideUI(InArgs._HideUI)
        .DrawCurve(InArgs._DrawCurve)
        .TimelineLength(InArgs._TimelineLength)
        .AllowZoomOutput(InArgs._AllowZoomOutput)
        .ShowInputGridNumbers(InArgs._ShowInputGridNumbers)
        .ShowOutputGridNumbers(InArgs._ShowOutputGridNumbers)
        .ShowZoomButtons(InArgs._ShowZoomButtons)
        .ZoomToFitHorizontal(InArgs._ZoomToFitHorizontal)
        .ZoomToFitVertical(InArgs._ZoomToFitVertical)
    );

    HoudiniAssetParameterRamp = nullptr;

    UCurveEditorSettings * CurveEditorSettings = GetSettings();
    if (CurveEditorSettings)
    {
        CurveEditorSettings->SetCurveVisibility(ECurveEditorCurveVisibility::AllCurves);
        CurveEditorSettings->SetTangentVisibility(ECurveEditorTangentVisibility::NoTangents);
    }
}

FReply
SHoudiniAssetParameterRampCurveEditor::OnMouseButtonUp(
    const FGeometry & MyGeometry,
    const FPointerEvent & MouseEvent)
{
    FReply Reply = SCurveEditor::OnMouseButtonUp(MyGeometry, MouseEvent);

    if (HoudiniAssetParameterRamp.IsValid())
        HoudiniAssetParameterRamp->OnCurveEditingFinished();

    return Reply;
}

void
FHoudiniParameterDetails::CreateWidgetRamp(IDetailCategoryBuilder & LocalDetailCategoryBuilder, class UHoudiniAssetParameterRamp& InParam)
{
    TWeakObjectPtr<UHoudiniAssetParameterRamp> MyParam(&InParam);
    FDetailWidgetRow & Row = LocalDetailCategoryBuilder.AddCustomRow(FText::GetEmpty());

    // Create the standard parameter name widget.
    CreateNameWidget(&InParam, Row, true);

    TSharedRef< SHorizontalBox > HorizontalBox = SNew(SHorizontalBox);

    FString CurveAxisTextX = TEXT("");
    FString CurveAxisTextY = TEXT("");
    UClass * CurveClass = nullptr;

    if (InParam.bIsFloatRamp)
    {
        CurveAxisTextX = TEXT(HAPI_UNREAL_RAMP_FLOAT_AXIS_X);
        CurveAxisTextY = TEXT(HAPI_UNREAL_RAMP_FLOAT_AXIS_Y);
        CurveClass = UHoudiniAssetParameterRampCurveFloat::StaticClass();
    }
    else
    {
        CurveAxisTextX = TEXT(HAPI_UNREAL_RAMP_COLOR_AXIS_X);
        CurveAxisTextY = TEXT(HAPI_UNREAL_RAMP_COLOR_AXIS_Y);
        CurveClass = UHoudiniAssetParameterRampCurveColor::StaticClass();
    }

    HorizontalBox->AddSlot().Padding(2, 2, 5, 2)
    [
        SNew(SBorder)
        .VAlign(VAlign_Fill)
        [
            SAssignNew(InParam.CurveEditor, SHoudiniAssetParameterRampCurveEditor)
            .ViewMinInput(0.0f)
            .ViewMaxInput(1.0f)
            .HideUI(true)
            .DrawCurve(true)
            .ViewMinInput(0.0f)
            .ViewMaxInput(1.0f)
            .ViewMinOutput(0.0f)
            .ViewMaxOutput(1.0f)
            .TimelineLength(1.0f)
            .AllowZoomOutput(false)
            .ShowInputGridNumbers(false)
            .ShowOutputGridNumbers(false)
            .ShowZoomButtons(false)
            .ZoomToFitHorizontal(false)
            .ZoomToFitVertical(false)
            .XAxisName(CurveAxisTextX)
            .YAxisName(CurveAxisTextY)
            .ShowCurveSelector(false)
        ]
    ];

    // Set callback for curve editor events.
    TSharedPtr< SHoudiniAssetParameterRampCurveEditor > HoudiniRampEditor = StaticCastSharedPtr<SHoudiniAssetParameterRampCurveEditor>(InParam.CurveEditor);
    if (HoudiniRampEditor.IsValid())
        HoudiniRampEditor->SetParentRampParameter(&InParam);

    if (InParam.bIsFloatRamp)
    {
        if (!InParam.HoudiniAssetParameterRampCurveFloat)
        {
            InParam.HoudiniAssetParameterRampCurveFloat = Cast< UHoudiniAssetParameterRampCurveFloat >(
                NewObject< UHoudiniAssetParameterRampCurveFloat >(
                    InParam.PrimaryObject, UHoudiniAssetParameterRampCurveFloat::StaticClass(),
                    NAME_None, RF_Transactional | RF_Public));

            InParam.HoudiniAssetParameterRampCurveFloat->SetParentRampParameter(&InParam);
        }

        // Set curve values.
        InParam.GenerateCurvePoints();

        // Set the curve that is being edited.
        InParam.CurveEditor->SetCurveOwner(InParam.HoudiniAssetParameterRampCurveFloat, true);
    }
    else
    {
        if (!InParam.HoudiniAssetParameterRampCurveColor)
        {
            InParam.HoudiniAssetParameterRampCurveColor = Cast< UHoudiniAssetParameterRampCurveColor >(
                NewObject< UHoudiniAssetParameterRampCurveColor >(
                    InParam.PrimaryObject, UHoudiniAssetParameterRampCurveColor::StaticClass(),
                    NAME_None, RF_Transactional | RF_Public));

            InParam.HoudiniAssetParameterRampCurveColor->SetParentRampParameter(&InParam);
        }

        // Set curve values.
        InParam.GenerateCurvePoints();

        // Set the curve that is being edited.
        InParam.CurveEditor->SetCurveOwner(InParam.HoudiniAssetParameterRampCurveColor, true);
    }

    Row.ValueWidget.Widget = HorizontalBox;
    Row.ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);

    // Recursively create all child parameters.
    for (UHoudiniAssetParameter * ChildParam : InParam.ChildParameters)
        FHoudiniParameterDetails::CreateWidget(LocalDetailCategoryBuilder, ChildParam);
}
