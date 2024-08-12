//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#pragma once

#include "IDetailPropertyRow.h" //FResetToDefaultOverride
#include "ApparanceEntity.h"

/** Widget showing the reset to default value button */
class SResetToDefaultParameter : public SCompoundWidget
{

public:

	SLATE_BEGIN_ARGS( SResetToDefaultParameter )
		{}
	SLATE_END_ARGS()

	~SResetToDefaultParameter();

	void Construct( const FArguments& InArgs, IProceduralObject* entity, int parameter_id );
private:
	FText GetResetToolTip() const;

	EVisibility GetDiffersFromDefaultAsVisibility() const;

	void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime );

	FReply OnResetClicked();

	void UpdateDiffersFromDefaultState();
private:
	TWeakObjectPtr<UObject> Entity;
	int ParameterID;

	bool bValueDiffersFromDefault;
};
