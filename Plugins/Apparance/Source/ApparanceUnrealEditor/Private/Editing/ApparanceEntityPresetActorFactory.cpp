//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

//main
#include "ApparanceEntityPresetActorFactory.h"

//unreal
#include "UObject/ConstructorHelpers.h"

//module
#include "ApparanceEntity.h"
#include "ApparanceEntityPreset.h"

//module editor


#define LOCTEXT_NAMESPACE "ApparanceUnreal"

// ctor
//
UApparanceEntityPresetActorFactory::UApparanceEntityPresetActorFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("Entity Preset Asset", "Entity Preset");
	NewActorClass = AApparanceEntity::StaticClass();
	
	//still want it in the new-actor creation menus

}

// check we can use this asset
//
bool UApparanceEntityPresetActorFactory::CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg)
{
	if (!AssetData.IsValid() || !AssetData.GetClass()->IsChildOf(UApparanceEntityPreset::StaticClass()))
	{
		OutErrorMsg = LOCTEXT("NoEntityActorSpawner", "A valid Entity Preset must be specified.");
		return false;
	}
	
	return true;
}

#if UE_VERSION_OLDER_THAN(4,27,0)
// create an actor in the level based on the supplied asset
//
AActor* UApparanceEntityPresetActorFactory::SpawnActor( UObject* Asset, ULevel* InLevel, const FTransform& Transform, EObjectFlags NewObjectFlags, const FName Name )
{			
	//determine spawn details from preset asset
	UApparanceEntityPreset* entity_preset = Cast<UApparanceEntityPreset>(Asset);
	if(!entity_preset)
	{
		return nullptr;
	}

	//create new actor
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.OverrideLevel = InLevel;
	SpawnInfo.ObjectFlags = NewObjectFlags;
	SpawnInfo.Name = Name;
	AApparanceEntity* entity = (AApparanceEntity*)InLevel->OwningWorld->SpawnActor(AApparanceEntity::StaticClass(), &Transform, SpawnInfo);

	//apply preset settings
	entity->ApplyPreset( entity_preset );

	return entity;
}
#else //4.27.0 onwards (but not UE 5.0 EA)
AActor* UApparanceEntityPresetActorFactory::SpawnActor( UObject* InAsset, ULevel* InLevel, const FTransform& InTransform, const FActorSpawnParameters& InSpawnParams )
{
	//determine spawn details from preset asset
	UApparanceEntityPreset* entity_preset = Cast<UApparanceEntityPreset>( InAsset );
	if(!entity_preset)
	{
		return nullptr;
	}

	//create new actor
	AApparanceEntity* entity = (AApparanceEntity*)InLevel->OwningWorld->SpawnActor( AApparanceEntity::StaticClass(), &InTransform, InSpawnParams );

	//apply preset settings
	entity->ApplyPreset( entity_preset );

	return entity;
}
#endif //versioned



#undef LOCTEXT_NAMESPACE

