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

// apparance
#include "Apparance.h"

// module
#include "ApparanceResourceListEntry.h"
#include "ApparanceResourceListEntry_3DAsset.h"

// auto (last)
#include "ApparanceResourceListEntry_Variants.generated.h"


// entry for containing a list of assets that can be used to select appearance by number
//
UCLASS()
class APPARANCEUNREAL_API UApparanceResourceListEntry_Variants
	: public UApparanceResourceListEntry
{
	GENERATED_BODY()

	//transient
	mutable int FirstVariantID;	//beginning of current range of ID's assigned to my variants
	mutable int CurrentCacheVersion; //track invalidation of db/cached data
	
public:
	
	//placement adjustment
	UPROPERTY(EditAnywhere,Category="Placement",meta=(ToolTip="Perform placement fixups for all variants"))
	bool bFixPlacement;
	UPROPERTY(EditAnywhere,Category="Resource", meta=( EditConditionHides, EditCondition="bFixPlacement" ))
	EApparanceResourceOrientation Orientation;
	UPROPERTY(EditAnywhere,Category="Resource", meta=( EditConditionHides, EditCondition="bFixPlacement", ClampMin = -2, ClampMax = 2, UIMin = -2, UIMax = 2 ))
	int Rotation;
	UPROPERTY(EditAnywhere,Category="Resource", meta=( EditConditionHides, EditCondition="bFixPlacement" ))
	float UniformScale = 1.0f;
	UPROPERTY(EditAnywhere,Category="Resource", meta=( EditConditionHides, EditCondition="bFixPlacement" ))
	FVector Scale = FVector(1,1,1);

	// get a variant by number
	UObject* GetAsset( int variant_number ) const;

	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject Interface

	//editing
	void SetAsset( UObject* passet, int variant_index=-1 );

	//access
	int GetVariantCount() const { return Children.Num(); }
	virtual FName GetIconName( bool is_open=false ) override;	
	static FName StaticIconName( bool is_open=false );
	int GetVariantID( int variant_number, int cache_version ) const;	//variants start at 1, returns 0 for not set
	int SetVariantID( int first_variant_id, int cache_version ) const; //assign range of ID's for this
	int GetVariantNumber( int object_id ) const; //find the variant based on an object id
	
};

