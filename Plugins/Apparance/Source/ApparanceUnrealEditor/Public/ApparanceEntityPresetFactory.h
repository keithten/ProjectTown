//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#pragma once

// unreal
#include "Editor.h"

// apparance

// module

// auto (last)
#include "ApparanceEntityPresetFactory.generated.h"


// makes an entity preset asset
//
UCLASS()
class APPARANCEUNREALEDITOR_API UApparanceEntityPresetFactory
	: public UFactory
{
	GENERATED_UCLASS_BODY()

	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags tFlags, UObject* Context, FFeedbackContext* Warn) override;
};

