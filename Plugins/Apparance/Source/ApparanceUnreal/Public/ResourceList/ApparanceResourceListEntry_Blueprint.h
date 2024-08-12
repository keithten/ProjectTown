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
#include "Engine/Blueprint.h"

// apparance
#include "Apparance.h"

// module
#include "ApparanceResourceListEntry_3DAsset.h"

// auto (last)
#include "ApparanceResourceListEntry_Blueprint.generated.h"

// entry for mapping resource name to 3D blueprint asset
//
UCLASS()
class APPARANCEUNREAL_API UApparanceResourceListEntry_Blueprint
	: public UApparanceResourceListEntry_3DAsset
{
	GENERATED_BODY()

	//we need to persist the class as Blueprints themselves aren't available in a cooked build
	UPROPERTY()
	UBlueprintGeneratedClass* BlueprintClass;

public:

	UBlueprint* GetBlueprint() const { return Cast<UBlueprint>( GetAsset() ); }
#if WITH_EDITOR
	//access directly in editor
	UBlueprintGeneratedClass* GetBlueprintClass() const { return GetBlueprint()?(Cast<UBlueprintGeneratedClass>( GetBlueprint()->GeneratedClass.Get() )):nullptr; }
#else
	//access cached version in standalone
	UBlueprintGeneratedClass* GetBlueprintClass() const { return BlueprintClass; }
#endif

public:
	//editing
	virtual void Editor_RebuildSource() override;
	virtual void Editor_AssignFrom( UApparanceResourceListEntry* psource );

	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual void PostLoad() override;
	//~ End UObject Interface

	//access
	virtual UClass* GetAssetClass() const override { return UBlueprint::StaticClass(); }
	virtual FText GetAssetTypeName() const override;	
	virtual FName GetIconName( bool is_open=false ) override;

	static FText StaticAssetTypeName();
	static FName StaticIconName( bool is_open=false );

	//events
	virtual void Editor_OnAssetChanged() override;
};

