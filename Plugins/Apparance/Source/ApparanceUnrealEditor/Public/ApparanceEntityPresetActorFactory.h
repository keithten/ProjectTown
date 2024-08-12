//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#pragma once

// unreal
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "ActorFactories/ActorFactory.h"
#include "Misc/EngineVersionComparison.h"

// apparance

// module

// auto (last)
#include "ApparanceEntityPresetActorFactory.generated.h"


// makes entity actor in the level when you drag in an entity preset asset
//
UCLASS()
class UApparanceEntityPresetActorFactory
	: public UActorFactory
{
	GENERATED_UCLASS_BODY()

	//~ Begin UActorFactory Interface		
	virtual bool CanCreateActorFrom( const FAssetData& AssetData, FText& OutErrorMsg );

#if UE_VERSION_OLDER_THAN(4,27,0)
	virtual AActor* SpawnActor( UObject* Asset, ULevel* InLevel, const FTransform& Transform, EObjectFlags ObjectFlags, const FName Name ) override;
#else //4.27.0 onwards (but not UE 5.0 EA)
	virtual AActor* SpawnActor( UObject* InAsset, ULevel* InLevel, const FTransform& InTransform, const FActorSpawnParameters& InSpawnParams ) override;
#endif


	//~ End UActorFactory Interface
};
