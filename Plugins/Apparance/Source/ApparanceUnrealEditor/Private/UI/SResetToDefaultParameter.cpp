//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#include "SResetToDefaultParameter.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "ApparanceUnrealEditor.h"

#define LOCTEXT_NAMESPACE "ApparanceUnreal"

SResetToDefaultParameter::~SResetToDefaultParameter()
{
}

void SResetToDefaultParameter::Construct(const FArguments& InArgs, IProceduralObject* entity, int parameter_id )
{
	bValueDiffersFromDefault = false;
	Entity = Cast<UObject>(entity);
	ParameterID = parameter_id;

	// Indicator for a value that differs from default. Also offers the option to reset to default.
	ChildSlot
	[
		SNew(SButton)
		.IsFocusable(false)
		.ToolTipText(this, &SResetToDefaultParameter::GetResetToolTip)
		.ButtonStyle( APPARANCE_STYLE, "NoBorder" )
		.ContentPadding(0) 
		.Visibility( this, &SResetToDefaultParameter::GetDiffersFromDefaultAsVisibility )
		.OnClicked( this, &SResetToDefaultParameter::OnResetClicked )
		.Content()
		[
			SNew(SImage)
			.Image( APPARANCE_STYLE.GetBrush("PropertyWindow.DiffersFromDefault") )
		]
	];

	UpdateDiffersFromDefaultState();
}

void SResetToDefaultParameter::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	UpdateDiffersFromDefaultState();
}

FText SResetToDefaultParameter::GetResetToolTip() const
{
	return LOCTEXT("ResetToDefaultParameterTooltip", "Reset to Default");
}

FReply SResetToDefaultParameter::OnResetClicked()
{
	IProceduralObject* pentity = Cast<IProceduralObject>(Entity.Get());
	if(pentity)
	{
		FScopedTransaction transaction( LOCTEXT("ResetToDefaultParameter","Reset Parameter To Default") );
		
		pentity->IPO_Modify();

		//default happens when parameter removed from entity
		Apparance::IParameterCollection* ent_params = pentity->IPO_BeginEditingParameters();
		ent_params->RemoveParameter( ParameterID );
		pentity->IPO_EndEditingParameters( false );

		pentity->IPO_PostParameterEdit( ent_params, ParameterID );
	}	
	return FReply::Handled();
}

void SResetToDefaultParameter::UpdateDiffersFromDefaultState()
{
	bValueDiffersFromDefault = false;

	//entity params
	IProceduralObject* pentity = Cast<IProceduralObject>(Entity.Get());
	if(pentity)
	{
		bValueDiffersFromDefault = !pentity->IPO_IsParameterDefault( (Apparance::ValueID)ParameterID );
	}
}

EVisibility SResetToDefaultParameter::GetDiffersFromDefaultAsVisibility() const
{
	return bValueDiffersFromDefault ? EVisibility::Visible : EVisibility::Hidden;
}

#undef LOCTEXT_NAMESPACE
