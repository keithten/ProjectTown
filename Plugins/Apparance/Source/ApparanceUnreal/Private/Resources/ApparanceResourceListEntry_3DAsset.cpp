//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

// main
#include "ResourceList/ApparanceResourceListEntry_3DAsset.h"


// unreal
//#include "Components/BoxComponent.h"
//#include "Components/BillboardComponent.h"
//#include "Components/DecalComponent.h"
//#include "UObject/ConstructorHelpers.h"
//#include "Engine/Texture2D.h"
//#include "ProceduralMeshComponent.h"
//#include "Engine/World.h"
//#include "Components/InstancedStaticMeshComponent.h"

// module
#include "ApparanceUnreal.h"
#include "AssetDatabase.h"
//#include "Geometry.h"


//////////////////////////////////////////////////////////////////////////
// FResourceListEntry_3DAsset

FName UApparanceResourceListEntry_3DAsset::GetIconName( bool is_open )
{
	return FName("ClassIcon.BoxComponent");
}

// try to obtain bounds
//
bool UApparanceResourceListEntry_3DAsset::GetBounds(FVector& centre_out, FVector& extent_out ) const
{
#if WITH_EDITOR
	//ensure calculated
	if(AssetExtent.X == 0)
	{
		Editor_RebuildBounds();
	}
#endif

	centre_out = AssetCentre;
	extent_out = AssetExtent;
	return true;
}

// transform calculation utility
//
FMatrix UApparanceResourceListEntry_3DAsset::CalculateFixupTransform( bool enable, EApparanceResourceOrientation orientation, int rotation, float uniform_scale, FVector scale )
{
	FMatrix fixup_transform = FMatrix::Identity;
	if(enable)
	{
		//scaling combination
		scale *= uniform_scale;
		FMatrix scl = FScaleMatrix::Make( scale );

		//calculate top face rotation
		int yaw = rotation;
		FMatrix rot = FRotationMatrix::Make( FRotator( 0, (float)yaw*90.0f, 0 ) );
		
		//calculate where to put top face
		int pitch = 0;
		int roll = 0;
		switch(orientation)
		{
			case EApparanceResourceOrientation::Left:
				roll = +1;
				break;
			case EApparanceResourceOrientation::Right:
				roll = -1;
				break;
			case EApparanceResourceOrientation::Back:
				pitch = +1;
				break;
			case EApparanceResourceOrientation::Forward:
				pitch = -1;
				break;
			case EApparanceResourceOrientation::Over:
				roll = -2;	//face down
				break;
			default:
			case EApparanceResourceOrientation::Default:
				//leave as-is
				break;
		}
		FMatrix ori = FRotationMatrix::Make( FRotator( (float)pitch*90.0f, 0, (float)roll*90.0f ) );
		
		//final composition
		fixup_transform = scl * ori * rot;	//scale first, then orientation, then Z rotation - better of the fixup UX options
	}

	return fixup_transform;
}


// get my final fixup transform
//
FMatrix UApparanceResourceListEntry_3DAsset::GetFixupTransform( bool include_scaling ) const
{
	//my transform fixups
	FMatrix local_transform = CalculateFixupTransform( bFixPlacement, Orientation, Rotation, include_scaling?UniformScale:1.0f, include_scaling?Scale:FVector(1,1,1) );

	//override parent or combine?
	if(Override==EApparanceResourceTransformOverride::Absolute)
	{
		return local_transform;
	}
	else
	{
		//parent (variant list) supplied transform too?
		FMatrix parent_transform = FMatrix::Identity;
		const UApparanceResourceListEntry_Variants* pvariants = Cast<const UApparanceResourceListEntry_Variants>( FindParent() );
		if(pvariants)
		{
			parent_transform = CalculateFixupTransform( pvariants->bFixPlacement, pvariants->Orientation, pvariants->Rotation, include_scaling?pvariants->UniformScale:1.0f, include_scaling?pvariants->Scale:FVector(1,1,1) );
		}
		return parent_transform * local_transform;	//parent, then mine
	}	
}

// obtain the bounds parameters from wherever the user selected
// NOTE: Intended for edit time only use, i.e. when the user directly changes the source settings
//
void UApparanceResourceListEntry_3DAsset::Editor_RebuildSource()
{
	check( GIsEditor );

	//update from source
	switch(BoundsSource)
	{
		case EApparanceResourceBoundsSource::Asset:
			//specific asset implementation should override and provide
			//e.g. ApparanceResourceListEntry_StaticMesh
			break;

		case EApparanceResourceBoundsSource::Mesh:
			//obtain from another mesh			
			if(BoundsMesh)
			{
				//use first mesh as bounds
				FBox bounds = BoundsMesh->GetBoundingBox();
				BoundsCentre = bounds.GetCenter();
				BoundsExtent = bounds.GetExtent();
			}
			else
			{
				//default to unit cube if no mesh set
				BoundsCentre = FVector( 0.0f, 0.0f, 0.0f );
				BoundsExtent = FVector( 50.0f, 50.0f, 50.0f );	//100cm cube
			}
			break;

		default:
		//case EApparanceResourceBoundsSource::Capture:
		case EApparanceResourceBoundsSource::Custom:
			//already obtained or set by user, just leave as set
			break;
	}

	//apply
	Editor_RebuildBounds();
}

// rebuild derived bounds information after a source affecting change
//
void UApparanceResourceListEntry_3DAsset::Editor_RebuildBounds() const
{
	check(GIsEditor);

	//calculate asset normalisation matix
	FMatrix asset_transform = FTranslationMatrix::Make( -BoundsCentre );
	FMatrix asset_transform_unscaled = asset_transform;
	FVector normalise; //build 'safe' normalisation scale, near zero sizes won't scale up badly later anyway
	normalise.X = FMath::IsNearlyZero( BoundsExtent.X )?1.0f:(0.5f/BoundsExtent.X);
	normalise.Y = FMath::IsNearlyZero( BoundsExtent.Y )?1.0f:(0.5f/BoundsExtent.Y);
	normalise.Z = FMath::IsNearlyZero( BoundsExtent.Z )?1.0f:(0.5f/BoundsExtent.Z);
	asset_transform *= FScaleMatrix::Make( normalise );

	//calculate placement adjustment
	FMatrix fixup_transform = GetFixupTransform( false );
	FMatrix bounds_transform = GetFixupTransform( true );	//bounds has scaling, asset transform doesn't
	
	//apply fixup to bounds we expose too
	AssetCentre = bounds_transform.TransformVector( BoundsCentre );
	AssetExtent = bounds_transform.TransformVector( BoundsExtent );
	AssetExtent.X = FMath::Abs( AssetExtent.X );
	AssetExtent.Y = FMath::Abs( AssetExtent.Y );
	AssetExtent.Z = FMath::Abs( AssetExtent.Z );
	
	//compose
	NormalisedTransform = asset_transform * fixup_transform;
	AssetTransform = asset_transform_unscaled * fixup_transform;
}


// our asset changed
//
void UApparanceResourceListEntry_3DAsset::Editor_OnAssetChanged()
{
	//3d asset bounds likely to have changed, re-cache
	Editor_RebuildSource();

	Super::Editor_OnAssetChanged();
}

// conversion between types
//
void UApparanceResourceListEntry_3DAsset::Editor_AssignFrom( UApparanceResourceListEntry* psource )
{	
	UApparanceResourceListEntry_3DAsset* p3d = Cast<UApparanceResourceListEntry_3DAsset>( psource );
	if(p3d)
	{
		//assigning from another 3d asset
		AssetTransform = p3d->AssetTransform;
		AssetCentre = p3d->AssetCentre;
		AssetExtent = p3d->AssetExtent;
		BoundsCentre = p3d->BoundsCentre;
		BoundsExtent = p3d->BoundsExtent;
		bFixPlacement = p3d->bFixPlacement;
		Override = p3d->Override;
		Orientation = p3d->Orientation;
		Rotation = p3d->Rotation;
		UniformScale = p3d->UniformScale;
		Scale = p3d->Scale;
	}

	Super::Editor_AssignFrom( psource );
}

#if WITH_EDITOR
void UApparanceResourceListEntry_3DAsset::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	FName prop_name = PropertyChangedEvent.GetPropertyName();
	FName struct_name = NAME_None;
	if(PropertyChangedEvent.MemberProperty)
	{
		struct_name = PropertyChangedEvent.MemberProperty->GetFName();
	}
	if(
		//source
		   prop_name == GET_MEMBER_NAME_CHECKED(UApparanceResourceListEntry_3DAsset, BoundsSource)
		|| prop_name == GET_MEMBER_NAME_CHECKED(UApparanceResourceListEntry_3DAsset, BoundsMesh)
		|| struct_name == GET_MEMBER_NAME_CHECKED( UApparanceResourceListEntry_3DAsset, BoundsCentre )
		|| struct_name == GET_MEMBER_NAME_CHECKED( UApparanceResourceListEntry_3DAsset, BoundsExtent )
		//transform?
		|| prop_name == GET_MEMBER_NAME_CHECKED(UApparanceResourceListEntry_3DAsset, PlacementOffsetParameter)
		|| struct_name == GET_MEMBER_NAME_CHECKED(UApparanceResourceListEntry_3DAsset, CustomPlacementOffset)
		|| prop_name == GET_MEMBER_NAME_CHECKED(UApparanceResourceListEntry_3DAsset, PlacementSizeMode)
		|| prop_name == GET_MEMBER_NAME_CHECKED(UApparanceResourceListEntry_3DAsset, PlacementScaleParameter)
		|| struct_name == GET_MEMBER_NAME_CHECKED(UApparanceResourceListEntry_3DAsset, CustomPlacementScale)
		|| prop_name == GET_MEMBER_NAME_CHECKED(UApparanceResourceListEntry_3DAsset, PlacementRotationParameter)
		|| struct_name == GET_MEMBER_NAME_CHECKED(UApparanceResourceListEntry_3DAsset, CustomPlacementRotation)
		//bounds adjust
		|| prop_name == GET_MEMBER_NAME_CHECKED(UApparanceResourceListEntry_3DAsset, bFixPlacement)
		|| prop_name == GET_MEMBER_NAME_CHECKED(UApparanceResourceListEntry_3DAsset, Override)
		|| prop_name == GET_MEMBER_NAME_CHECKED(UApparanceResourceListEntry_3DAsset, Orientation)
		|| prop_name == GET_MEMBER_NAME_CHECKED(UApparanceResourceListEntry_3DAsset, Rotation)
		|| prop_name == GET_MEMBER_NAME_CHECKED(UApparanceResourceListEntry_3DAsset, UniformScale)
		|| struct_name == GET_MEMBER_NAME_CHECKED(UApparanceResourceListEntry_3DAsset, Scale)
	)
	{
		//re-build asset database with new asset info
		//(note: will trigger bounds rebuild we require, see Editor_OnAssetChanged override above)
		Editor_OnAssetChanged();
	}
	
	Super::PostEditChangeProperty( PropertyChangedEvent );
}
#endif

// setup checks
//
void UApparanceResourceListEntry_3DAsset::PostLoad()
{
#if WITH_EDITOR
	//ensure new property set up
	FMatrix empty(EForceInit::ForceInitToZero);
	if (NormalisedTransform == empty && AssetTransform != empty)
	{
		Editor_RebuildSource();
	}
#endif

	Super::PostLoad();
}
