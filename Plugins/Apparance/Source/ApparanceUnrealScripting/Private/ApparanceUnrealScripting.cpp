//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

//main
#include "ApparanceUnrealScripting.h"

//unreal
//#include "Core.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"

//module
#include "ApparanceUnreal.h"
#include "ApparanceEntity.h"
#include "Utility/ApparanceConversion.h"
#include "Blueprint/ApparanceRebuildNode.h"

//module editor


DEFINE_LOG_CATEGORY(LogApparance);
#define LOCTEXT_NAMESPACE "FApparanceUnrealScriptingModule"


// Scripting module startup
//
void FApparanceUnrealScriptingModule::StartupModule()
{
}

// Scripting module shutdown
//
void FApparanceUnrealScriptingModule::ShutdownModule()
{
}




#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FApparanceUnrealScriptingModule, ApparanceUnrealScripting)
