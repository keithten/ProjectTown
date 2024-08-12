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

// auto (last)
#include "ApparanceResourceListEntry_Asset.generated.h"


// supported incoming paramter types
//
UENUM()
enum class EApparanceParameterType
{
	None,
	Integer,
	Float,
	Bool,
	String,
	Colour,
	Vector,
	Frame,

	//additional supported types (material use)
	Texture,
};

//incoming placement parameters
USTRUCT()
struct FApparancePlacementParameter
{
	GENERATED_BODY()

	//internal ID of placement parameter
	UPROPERTY()
	int ID;

	//expected type of the placement parameter
	UPROPERTY(EditAnywhere, Category = Apparance )
	EApparanceParameterType Type;
	
	//helpful label for the placement parameter
	UPROPERTY(EditAnywhere, Category = Apparance )
	FString Name;
	
	//defaults
	FApparancePlacementParameter()
	: ID(0)
	, Type( EApparanceParameterType::None ) {}
};


// entry for mapping resource name to actual assets
//
UCLASS()
class APPARANCEUNREAL_API UApparanceResourceListEntry_Asset
	: public UApparanceResourceListEntry
{
	GENERATED_BODY()

public:
	//Single asset reference
	UPROPERTY(EditAnywhere, Category = Apparance )
	UObject* Asset;

	//Expected incoming placement parameters
	UPROPERTY(EditAnywhere, Category = Apparance )
	TArray<FApparancePlacementParameter> ExpectedParameters;

	//assets that are placeable in 3d (i.e. using the Resource.Place operator)
	virtual bool IsPlaceable() const { return false; }
	
private:

	//transient
	mutable int FirstVariantID;	//beginning of current range of ID's assigned to my variants
	mutable int CurrentCacheVersion; //track invalidation of db/cached data
	
public:

	// get the asset
	UObject* GetAsset() const;

	// expected parameters
	virtual const FApparancePlacementParameter* FindParameterInfo( int parameter_id, int* param_index_out=nullptr ) const;
	virtual const FText FindParameterNameText( int param_id ) const;
	const FApparancePlacementParameter* FindFirstFrameParameterInfo() const;
	
	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	//~ End UObject Interface

	//editing
	void SetAsset( UObject* passet );

	//setup
	virtual void CheckAsset() {}
	virtual void CheckParameterList();

	//access
	virtual UClass* GetAssetClass() const;
	virtual FName GetIconName( bool is_open=false ) override;	
	static FName StaticIconName( bool is_open=false );
	virtual FText GetAssetTypeName() const { return FText::GetEmpty(); }
};

