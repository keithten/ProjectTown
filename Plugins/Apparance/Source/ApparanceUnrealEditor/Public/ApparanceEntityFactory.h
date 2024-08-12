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
#include "ActorFactories/ActorFactory.h"

// apparance

// module

// auto (last)
#include "ApparanceEntityFactory.generated.h"


// makes an entity actor placeable
//
UCLASS()
class APPARANCEUNREALEDITOR_API UApparanceEntityFactory
	: public UActorFactory
{
	GENERATED_UCLASS_BODY()
};

