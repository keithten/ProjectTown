//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

//main
#include "ApparanceEntityFactory.h"

//unreal

//module
#include "ApparanceEntity.h"

//module editor


#define LOCTEXT_NAMESPACE "ApparanceUnreal"

UApparanceEntityFactory::UApparanceEntityFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT( "ApparanceEntityActorName", "Apparance Entity" );
	NewActorClass = AApparanceEntity::StaticClass();
	bShowInEditorQuickMenu = true;
	bUseSurfaceOrientation = true;
}


#undef LOCTEXT_NAMESPACE

