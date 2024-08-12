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
#include "Engine/StaticMesh.h"

// apparance
#include "Apparance.h"

// module
#include "ApparanceResourceListEntry_3DAsset.h"
#include "ApparanceEngineSetup.h"

// auto (last)
#include "ApparanceResourceListEntry_StaticMesh.generated.h"


// entry for mapping resource name to 3D static mesh asset
//
UCLASS()
class APPARANCEUNREAL_API UApparanceResourceListEntry_StaticMesh
	: public UApparanceResourceListEntry_3DAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY( EditAnywhere, Category = Apparance )
	EApparanceInstancingMode EnableInstancing = EApparanceInstancingMode::PerEntity;


	UStaticMesh* GetMesh() const { return Cast<UStaticMesh>( GetAsset() ); }
	
public:
	//editing
	virtual void Editor_RebuildSource() override;

	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent ) override;
#endif
	//~ End UObject Interface
	
	//access
	virtual UClass* GetAssetClass() const override { return UStaticMesh::StaticClass(); }
	virtual FName GetIconName( bool is_open=false ) override;
	virtual FText GetAssetTypeName() const override;

	static FText StaticAssetTypeName();
	static FName StaticIconName( bool is_open=false );
};

