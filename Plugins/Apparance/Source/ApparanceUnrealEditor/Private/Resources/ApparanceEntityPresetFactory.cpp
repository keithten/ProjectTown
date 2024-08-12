//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

//main
#include "ApparanceEntityPresetFactory.h"

//unreal

//module
#include "ApparanceEntityPreset.h"

//module editor


#define LOCTEXT_NAMESPACE "ApparanceUnreal"

UApparanceEntityPresetFactory::UApparanceEntityPresetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UApparanceEntityPreset::StaticClass();
}

UObject* UApparanceEntityPresetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UApparanceEntityPreset* NewObjectAsset = NewObject<UApparanceEntityPreset>(InParent, Name, Flags | RF_Transactional);
	return NewObjectAsset;
}


#undef LOCTEXT_NAMESPACE

