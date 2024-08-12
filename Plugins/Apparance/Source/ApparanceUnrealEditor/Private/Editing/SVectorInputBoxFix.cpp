// Copyright Epic Games, Inc. All Rights Reserved.

#include "SVectorInputBoxFix.h"	//APPARANCE: class renamed

#if UE_VERSION_AT_LEAST( 5, 0, 0 ) 	//local copies to fix a bug (UE 5.1 version)
//code moved to headers after 5.0
#else

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "HAL/IConsoleManager.h"
#include "SNumericEntryBoxFix.h"  //APPARANCE: use fixed version
#include "Widgets/Layout/SWidgetSwitcher.h"

#define LOCTEXT_NAMESPACE "SVectorInputBox"

TAutoConsoleVariable<float> CVarCrushThem(TEXT("Slate.AllowNumericLabelCrush"), 1.0f, TEXT("Should we crush the vector input box?."));
TAutoConsoleVariable<float> CVarStopCrushWhenAbove(TEXT("Slate.NumericLabelWidthCrushStop"), 200.0f, TEXT("Stop crushing when the width is above."));
TAutoConsoleVariable<float> CVarStartCrushWhenBelow(TEXT("Slate.NumericLabelWidthCrushStart"), 190.0f, TEXT("Start crushing when the width is below."));

void SVectorInputBoxFix::Construct( const FArguments& InArgs )	//APPARANCE: class renamed
{
	bCanBeCrushed = InArgs._AllowResponsiveLayout;

	TSharedRef<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);

	ChildSlot
	[
		HorizontalBox
	];

	ConstructX( InArgs, HorizontalBox );
	ConstructY( InArgs, HorizontalBox );
	ConstructZ( InArgs, HorizontalBox );
}

void SVectorInputBoxFix::ConstructX( const FArguments& InArgs, TSharedRef<SHorizontalBox> HorizontalBox )	//APPARANCE: class renamed
{	
	const FLinearColor LabelColor = InArgs._bColorAxisLabels ?  SNumericEntryBoxFix<float>::RedLabelBackgroundColor : FLinearColor(0.0f,0.0f,0.0f,.5f); //APPARANCE: use fixed version
	TSharedRef<SWidget> LabelWidget = BuildDecoratorLabel(LabelColor, FLinearColor::White, LOCTEXT("X_Label", "X"));
	TAttribute<FMargin> MarginAttribute;
	if (bCanBeCrushed)
	{
		MarginAttribute = TAttribute<FMargin>::Create(TAttribute<FMargin>::FGetter::CreateSP(this, &SVectorInputBoxFix::GetTextMargin));	//APPARANCE: class renamed
	}

	TAttribute<TOptional<float>> Value = InArgs._X;

	HorizontalBox->AddSlot()
	.VAlign( VAlign_Center )
	.FillWidth( 1.0f )
	.Padding( 0.0f, 1.0f, 2.0f, 1.0f )
	[
		SNew( SNumericEntryBoxFix<float> ) //APPARANCE: use fixed version
		.AllowSpin(InArgs._AllowSpin)
		.Font( InArgs._Font )
		.Value( InArgs._X )
		.OnValueChanged( InArgs._OnXChanged )
		.OnValueCommitted( InArgs._OnXCommitted )
		.ToolTipText(MakeAttributeLambda([Value]
		{
			if (Value.Get().IsSet())
			{
				return FText::Format(LOCTEXT("X_ToolTip", "X Value = {0}"), Value.Get().GetValue());
			}
			return LOCTEXT("MultipleValues", "Multiple Values");
		}))
		.UndeterminedString( LOCTEXT("MultipleValues", "Multiple Values") )
		.LabelPadding(0)
		.OverrideTextMargin(MarginAttribute)
		.ContextMenuExtender( InArgs._ContextMenuExtenderX )
		.TypeInterface(InArgs._TypeInterface)
		.MinValue(TOptional<float>())
		.MaxValue(TOptional<float>())
		.MinSliderValue(TOptional<float>())
		.MaxSliderValue(TOptional<float>())
		.LinearDeltaSensitivity(1)
		.Delta(InArgs._SpinDelta)
		.OnBeginSliderMovement(InArgs._OnBeginSliderMovement)
		.OnEndSliderMovement(InArgs._OnEndSliderMovement)
		.Label()
		[
			LabelWidget
		]
	];
	
}

void SVectorInputBoxFix::ConstructY( const FArguments& InArgs, TSharedRef<SHorizontalBox> HorizontalBox )	//APPARANCE: class renamed
{
	const FLinearColor LabelColor = InArgs._bColorAxisLabels ?  SNumericEntryBoxFix<float>::GreenLabelBackgroundColor : FLinearColor(0.0f,0.0f,0.0f,.5f); //APPARANCE: use fixed version
	TSharedRef<SWidget> LabelWidget = BuildDecoratorLabel(LabelColor, FLinearColor::White, LOCTEXT("Y_Label", "Y"));
	TAttribute<FMargin> MarginAttribute;
	if (bCanBeCrushed)
	{
		MarginAttribute = TAttribute<FMargin>::Create(TAttribute<FMargin>::FGetter::CreateSP(this, &SVectorInputBoxFix::GetTextMargin));	//APPARANCE: class renamed
	}

	TAttribute<TOptional<float>> Value = InArgs._Y;

	HorizontalBox->AddSlot()
	.VAlign( VAlign_Center )
	.FillWidth( 1.0f )
	.Padding( 0.0f, 1.0f, 2.0f, 1.0f )
	[
		SNew( SNumericEntryBoxFix<float> ) //APPARANCE: use fixed version
		.AllowSpin(InArgs._AllowSpin)
		.Font( InArgs._Font )
		.Value( InArgs._Y )
		.OnValueChanged( InArgs._OnYChanged )
		.OnValueCommitted( InArgs._OnYCommitted )
		.ToolTipText(MakeAttributeLambda([Value]
		{
			if (Value.Get().IsSet())
			{
				return FText::Format(LOCTEXT("Y_ToolTip", "Y Value = {0}"), Value.Get().GetValue());
			}
			return LOCTEXT("MultipleValues", "Multiple Values");
		}))
		.UndeterminedString( LOCTEXT("MultipleValues", "Multiple Values") )
		.LabelPadding(0)
		.OverrideTextMargin(MarginAttribute)
		.ContextMenuExtender(InArgs._ContextMenuExtenderY)
		.TypeInterface(InArgs._TypeInterface)
		.MinValue(TOptional<float>())
		.MaxValue(TOptional<float>())
		.MinSliderValue(TOptional<float>())
		.MaxSliderValue(TOptional<float>())
		.LinearDeltaSensitivity(1)
		.Delta(InArgs._SpinDelta)
		.OnBeginSliderMovement(InArgs._OnBeginSliderMovement)
		.OnEndSliderMovement(InArgs._OnEndSliderMovement)
		.Label()
		[
			LabelWidget
		]
	];

}

void SVectorInputBoxFix::ConstructZ( const FArguments& InArgs, TSharedRef<SHorizontalBox> HorizontalBox )	//APPARANCE: class renamed
{
	const FLinearColor LabelColor = InArgs._bColorAxisLabels ?  SNumericEntryBoxFix<float>::BlueLabelBackgroundColor : FLinearColor(0.0f,0.0f,0.0f,.5f); //APPARANCE: use fixed version
	TSharedRef<SWidget> LabelWidget = BuildDecoratorLabel(LabelColor, FLinearColor::White, LOCTEXT("Z_Label", "Z"));
	TAttribute<FMargin> MarginAttribute;
	if (bCanBeCrushed)
	{
		MarginAttribute = TAttribute<FMargin>::Create(TAttribute<FMargin>::FGetter::CreateSP(this, &SVectorInputBoxFix::GetTextMargin));	//APPARANCE: class renamed
	}

	TAttribute<TOptional<float>> Value = InArgs._Z;

	HorizontalBox->AddSlot()
	.VAlign( VAlign_Center )
	.FillWidth( 1.0f )
	.Padding( 0.0f, 1.0f, 0.0f, 1.0f )
	[
		SNew( SNumericEntryBoxFix<float> ) //APPARANCE: use fixed version
		.AllowSpin(InArgs._AllowSpin)
		.Font( InArgs._Font )
		.Value( InArgs._Z )
		.OnValueChanged( InArgs._OnZChanged )
		.OnValueCommitted( InArgs._OnZCommitted )
		.ToolTipText(MakeAttributeLambda([Value]
		{	
			if (Value.Get().IsSet())
			{
				return FText::Format(LOCTEXT("Z_ToolTip", "Z Value = {0}"), Value.Get().GetValue());
			}
			return LOCTEXT("MultipleValues", "Multiple Values");
		}))
		.UndeterminedString( LOCTEXT("MultipleValues", "Multiple Values") )
		.LabelPadding(0)
		.OverrideTextMargin(MarginAttribute)
		.ContextMenuExtender(InArgs._ContextMenuExtenderZ)
		.TypeInterface(InArgs._TypeInterface)
		.MinValue(TOptional<float>())
		.MaxValue(TOptional<float>())
		.MinSliderValue(TOptional<float>())
		.MaxSliderValue(TOptional<float>())
		.LinearDeltaSensitivity(1)
		.Delta(InArgs._SpinDelta)
		.OnBeginSliderMovement(InArgs._OnBeginSliderMovement)
		.OnEndSliderMovement(InArgs._OnEndSliderMovement)
		.Label()
		[
			LabelWidget
		]
	];
}

TSharedRef<SWidget> SVectorInputBoxFix::BuildDecoratorLabel(FLinearColor BackgroundColor, FLinearColor InForegroundColor, FText Label)	//APPARANCE: class renamed
{
	TSharedRef<SWidget> LabelWidget = SNumericEntryBoxFix<float>::BuildLabel(Label, InForegroundColor, BackgroundColor); //APPARANCE: use fixed version

	TSharedRef<SWidget> ResultWidget = LabelWidget;
	
	if (bCanBeCrushed)
	{
		ResultWidget =
			SNew(SWidgetSwitcher)
			.WidgetIndex(this, &SVectorInputBoxFix::GetLabelActiveSlot)	//APPARANCE: class renamed
			+SWidgetSwitcher::Slot()
			[
				LabelWidget
			]
			+SWidgetSwitcher::Slot()
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("NumericEntrySpinBox.NarrowDecorator"))
				.BorderBackgroundColor(BackgroundColor)
				.ForegroundColor(InForegroundColor)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.Padding(FMargin(5.0f, 0.0f, 0.0f, 0.0f))
			];
	}

	return ResultWidget;
}

int32 SVectorInputBoxFix::GetLabelActiveSlot() const	//APPARANCE: class renamed
{
	return bIsBeingCrushed ? 1 : 0;
}

FMargin SVectorInputBoxFix::GetTextMargin() const	//APPARANCE: class renamed
{
	return bIsBeingCrushed ? FMargin(1.0f, 2.0f) : FMargin(4.0f, 2.0f);
}

void SVectorInputBoxFix::OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const	//APPARANCE: class renamed
{
	bool bFoop = bCanBeCrushed && (CVarCrushThem.GetValueOnAnyThread() > 0.0f);

	if (bFoop)
	{
		const float AlottedWidth = AllottedGeometry.GetLocalSize().X;

		const float CrushBelow = CVarStartCrushWhenBelow.GetValueOnAnyThread();
		const float StopCrushing = CVarStopCrushWhenAbove.GetValueOnAnyThread();

		if (bIsBeingCrushed)
		{
			bIsBeingCrushed = AlottedWidth < StopCrushing;
		}
		else
		{
			bIsBeingCrushed = AlottedWidth < CrushBelow;
		}
	}
	else
	{
		bIsBeingCrushed = false;
	}

	SCompoundWidget::OnArrangeChildren(AllottedGeometry, ArrangedChildren);
}

#undef LOCTEXT_NAMESPACE

#endif