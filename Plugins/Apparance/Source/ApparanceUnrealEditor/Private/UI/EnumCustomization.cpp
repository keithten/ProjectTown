//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#include "EnumCustomization.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorStyleSet.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "DetailCategoryBuilder.h"

#include "ApparanceUnrealEditor.h"

#define LOCTEXT_NAMESPACE "ApparanceUnreal"

void FEnumCustomization::CreateEnumCustomization(IDetailCategoryBuilder& category, TSharedPtr<IPropertyHandle> enum_property, UEnum* enum_type, FText name )
{
	PropertyHandle = enum_property;
	EnumType = enum_type;

	TSharedPtr<SUniformGridPanel> Panel;
		
	IDetailPropertyRow& Row = category.AddProperty( PropertyHandle );
	Row.CustomWidget()
	.NameContent()
	[
		SNew(STextBlock)
		.Text( name )
		.ToolTipText(this, &FEnumCustomization::GetEnumToolTip)
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MaxDesiredWidth(0)
	[
		SAssignNew( Panel, SUniformGridPanel)
	];

	//gather enum info
	int num_enums = EnumType->NumEnums();
	if(EnumType->ContainsExistingMax())
	{
		num_enums--;
	}
	for(int i = 0 ; i<num_enums ; i++)
	{
		//style
		FName style;
		if(i == 0)
			style = "Property.ToggleButton.Start";
		else if(i == (num_enums - 1))
			style = "Property.ToggleButton.End";
		else
			style = "Property.ToggleButton.Middle";
		
		//each enum
		uint8 enum_value = (uint8)EnumType->GetValueByIndex( i );
		FText enum_tooltip = EnumType->GetToolTipTextByIndex( i );
		FText enum_name = EnumType->GetDisplayNameTextByIndex( i );

		Panel->AddSlot(i, 0)
		[
			SNew(SCheckBox)
			.Style(APPARANCE_STYLE, style)
			.IsChecked(this, &FEnumCustomization::IsEnumActive, enum_value)
			.OnCheckStateChanged(this, &FEnumCustomization::OnEnumChanged, enum_value )
			.ToolTipText( enum_tooltip )
			[
				SNew(SHorizontalBox)

#if 0 //image?
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(3, 2)
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("Mobility.Static"))
				]
#endif

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.Padding(6, 2)
				[
					SNew(STextBlock)
					.Text( enum_name )
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.ColorAndOpacity(this, &FEnumCustomization::GetEnumTextColor, enum_value )
				]
			]
		];
	}
}

ECheckBoxState FEnumCustomization::IsEnumActive(uint8 enum_value) const
{
	if (PropertyHandle.IsValid())
	{
		uint8 value;
		PropertyHandle->GetValue(value);

		return value == enum_value ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	return ECheckBoxState::Unchecked;
}

FSlateColor FEnumCustomization::GetEnumTextColor( uint8 enum_value ) const
{
	if (PropertyHandle.IsValid())
	{
		uint8 value;
		PropertyHandle->GetValue( value );

		return value == enum_value ? FSlateColor(FLinearColor(0, 0, 0)) : FSlateColor(FLinearColor(0.72f, 0.72f, 0.72f, 1.f));
	}

	return FSlateColor(FLinearColor(0.72f, 0.72f, 0.72f, 1.f));
}

void FEnumCustomization::OnEnumChanged(ECheckBoxState checked_state, uint8 enum_value )
{
	if (PropertyHandle.IsValid() && checked_state == ECheckBoxState::Checked)
	{
		PropertyHandle->SetValue( enum_value );
	}
}

FText FEnumCustomization::GetEnumToolTip() const
{
	if (PropertyHandle.IsValid())
	{
		return PropertyHandle->GetToolTipText();
	}

	return FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE

