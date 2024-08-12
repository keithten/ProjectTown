//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

//main
#include "ApparanceResourceListFactory.h"

//unreal

//module
#include "ApparanceResourceList.h"

//module editor


#define LOCTEXT_NAMESPACE "ApparanceUnreal"

UApparanceResourceListFactory::UApparanceResourceListFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UApparanceResourceList::StaticClass();
}

UObject* UApparanceResourceListFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UApparanceResourceList* NewObjectAsset = NewObject< UApparanceResourceList>(InParent, Name, Flags | RF_Transactional);
	return NewObjectAsset;
}


#undef LOCTEXT_NAMESPACE

