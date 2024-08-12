//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#pragma once

//unreal

//module
#include "ApparanceEntityPreset.h"
#include "EntityProperties.h"

//apparance

	
/// <summary>
/// entity preset property customisation
/// basically set properties that a placed entity will take on
/// </summary>
class FEntityPresetPropertyCustomisation 
	: public FEntityPropertyCustomisation
	, public IProceduralObject
{
	
public:
	FEntityPresetPropertyCustomisation() {}

	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable( new FEntityPresetPropertyCustomisation );		
	}	

protected:
	virtual bool ShowProcedureSelection() { return true; }
	virtual bool ShowProcedureParameters() { return true; }
	virtual bool ShowDiagnostics() { return false; }
	virtual bool ShowPinEnableUI() { return false; }
	virtual FName GetProcedureIDPropertyName()
	{
		return GET_MEMBER_NAME_CHECKED(UApparanceEntityPreset, ProcedureID);
	}
	virtual UClass* GetProcedureIDPropertyClass()
	{
		return UApparanceEntityPreset::StaticClass();
	}

private:
	
	
};
