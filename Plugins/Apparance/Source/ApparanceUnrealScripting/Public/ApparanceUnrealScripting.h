//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#pragma once

//unreal
#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

//module

//apparance

// Apparance scripting module
//
class FApparanceUnrealScriptingModule 
	: public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	
private:

	
	
};
