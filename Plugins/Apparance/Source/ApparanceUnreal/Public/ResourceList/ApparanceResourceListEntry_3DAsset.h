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
#include "ApparanceResourceListEntry_Asset.h"

// auto (last)
#include "ApparanceResourceListEntry_3DAsset.generated.h"


UENUM()
enum class EApparanceResourceBoundsSource : uint8
{
	//Obtain the bounds from the referenced asset if possible
	Asset,
	//Use another mesh to provide bounds reference
	Mesh,
	//Set the bounds explicitly to certain values
	Custom,
	//Capture the bounds from a component part of the blueprint or asset
	//Capture,
};

UENUM()
enum class EApparanceResourceOrientation : uint8
{
	//Unmodified orientation
	Default,
	//Tipped left
	Left,
	//Tipped right
	Right,
	//Tipped back
	Back,
	//Tipped forward
	Forward,
	//Turnned completely over
	Over
};

UENUM()
enum class EApparanceResourceTransformOverride : uint8
{
	//Apply both parent (shared fixup) and our own
	Relative,
	//Use our own fixup only
	Absolute,
};

UENUM()
enum class EApparanceSizeMode : uint8
{
	//Fit asset to specified size by scaling relative to asset bounds
	Size,
	//Just use a specific scale value
	Scale,
};

// an asset type that has a 3D nature
//
UCLASS(Abstract)
class APPARANCEUNREAL_API UApparanceResourceListEntry_3DAsset
	: public UApparanceResourceListEntry_Asset
{
	GENERATED_BODY()

	//derived asset information (based on original bounds and fixups)
	UPROPERTY()
	mutable FMatrix AssetTransform; //base transform with fixups applied
	UPROPERTY()
	mutable FMatrix NormalisedTransform; //asset transform but 'normalised' to unit cube (for sizing)
	UPROPERTY()
	mutable FVector AssetCentre;    //adjusted centre
	UPROPERTY()
	mutable FVector AssetExtent;	//adjusted extent
	
public:
	//where to obtain the bounds from?
	UPROPERTY(EditAnywhere, Category = Apparance )
	EApparanceResourceBoundsSource BoundsSource;

	//optional alternate bounds source (mesh)
	UPROPERTY(EditAnywhere, Category = Apparance )
	class UStaticMesh* BoundsMesh;

	//source bounds (cached or edited bounds, must be persisted)
	//Centre of bounding box obtained from selected source
	UPROPERTY(EditAnywhere, Category = Apparance )
	FVector BoundsCentre;
	//Extents of bounding box obtained from selected source (half of the size)
	UPROPERTY(EditAnywhere, Category = Apparance )
	FVector BoundsExtent;
	
	//transform
	//Which parameter should be used for the translation of the asset
	UPROPERTY(EditAnywhere,meta=(DisplayName="Offset Parameter"), Category = Apparance )
	int PlacementOffsetParameter = 1;	//default to first param (placement frame)
	//Explicitly specified translation to place the asset
	UPROPERTY(EditAnywhere,meta=(DisplayName="Custom Offset"), Category = Apparance )
	FVector CustomPlacementOffset;
	//scale or size?
	UPROPERTY(EditAnywhere,meta=(DisplayName="Size Mode"), Category = Apparance )
	EApparanceSizeMode PlacementSizeMode;
	//Which parameter should be used for the scale of the asset
	UPROPERTY(EditAnywhere,meta=(DisplayName="Scale Parameter"), Category = Apparance )
	int PlacementScaleParameter = 1;	//default to first param (placement frame)
	//Explicitly specified scale to place the asset with
	UPROPERTY(EditAnywhere,meta=(DisplayName="Custom Scale"), Category = Apparance )
	FVector CustomPlacementScale = FVector(1,1,1);
	//Which parameter should be used for the rotation of the asset
	UPROPERTY(EditAnywhere,meta=(DisplayName="Rotation Parameter"), Category = Apparance )
	int PlacementRotationParameter = 1;	//default to first param (placement frame)
	//Explicitly specified rotation to place the asset with
	UPROPERTY(EditAnywhere,meta=(DisplayName="Custom Rotation"), Category = Apparance )
	FVector CustomPlacementRotation;	//Euler
	
	//placement adjustment
	UPROPERTY(EditAnywhere, Category = Apparance )
	bool bFixPlacement;
	UPROPERTY(EditAnywhere, Category = Apparance )
	EApparanceResourceTransformOverride Override = EApparanceResourceTransformOverride::Relative;
	UPROPERTY(EditAnywhere, Category = Apparance )
	EApparanceResourceOrientation Orientation;
	UPROPERTY(EditAnywhere, meta=( ClampMin = -2, ClampMax = 2, UIMin = -2, UIMax = 2 ), Category = Apparance )
	int Rotation; //rotation about vertical
	UPROPERTY(EditAnywhere, Category = Apparance )
	float UniformScale = 1.0f;
	UPROPERTY(EditAnywhere, Category = Apparance )
	FVector Scale = FVector(1,1,1);

public:
	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual void PostLoad() override;
	//~ End UObject Interface

	//editing
	virtual void Editor_OnAssetChanged() override;
	virtual void Editor_RebuildSource();
	virtual void Editor_OnVariantsFixupChanged() { Editor_OnAssetChanged(); }	//parent variant list has a change affecting this variant
	virtual void Editor_AssignFrom( UApparanceResourceListEntry* psource );

	//access
	virtual FName GetIconName( bool is_open=false ) override;
	virtual bool GetBounds( FVector& centre_out, FVector& extent_out ) const override;
	FMatrix GetAssetTransform( bool normalised ) const { return normalised?NormalisedTransform:AssetTransform; }
	
	//utility
	static FMatrix CalculateFixupTransform( bool enable, EApparanceResourceOrientation orientation, int rotation, float uniform_scale, FVector scale );

protected:
	void Editor_RebuildBounds() const;	//calculate derived bounds info after a source change or something that affects the source
	
	//assets that are placeable in 3d (i.e. using the Resource.Place operator)
	virtual bool IsPlaceable() const override { return true; }
	
private:
	FMatrix GetFixupTransform( bool include_scaling ) const;
	
};

