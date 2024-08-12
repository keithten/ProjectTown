//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

// main
#include "ResourceList/ApparanceResourceListEntry_Variants.h"


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
// UApparanceResourceListEntry_Variants


// default resource icon
//
FName UApparanceResourceListEntry_Variants::GetIconName( bool is_open )
{
	return StaticIconName( is_open );
}

FName UApparanceResourceListEntry_Variants::StaticIconName( bool is_open )
{
	return FName("Kismet.VariableList.ArrayTypeIcon");
}

// asset id assigned to a particular variant
// variants start at 1, returns 0 for not set
//
int UApparanceResourceListEntry_Variants::GetVariantID( int variant_number, int cache_version ) const
{
	//invalidate assigned variant id range?
	if(CurrentCacheVersion!=cache_version)
	{
		FirstVariantID = 0;
	}

	//has been assigned?
	if(FirstVariantID>0)
	{
		//which in the range is it?
		return FirstVariantID + variant_number - 1;
	}

	//none-set/assigned yet
	return 0;
}

// assign range of ID's for this assets variants
// returns next free ID
//
int UApparanceResourceListEntry_Variants::SetVariantID( int first_variant_id, int cache_version ) const
{
	check(FirstVariantID==0);

	//assign
	FirstVariantID = first_variant_id;

	//up to date
	CurrentCacheVersion = cache_version;

	//next id after range
	return FirstVariantID + GetVariantCount();
}

// find the variant based on an object id
//
int UApparanceResourceListEntry_Variants::GetVariantNumber( int object_id ) const
{
	//has mapped ID range?
	if(FirstVariantID!=0)
	{
		int number = object_id-FirstVariantID+1;

		//valid, in range?
		if(number>=1 && number<=Children.Num())
		{
			return number;
		}
	}

	//no variants
	return 0;
}


// access to asset
//
UObject* UApparanceResourceListEntry_Variants::GetAsset( int variant_number ) const
{
	check(variant_number>0);
	//variant access
	int variant_index = variant_number-1;
	if(variant_index>=0 && variant_number<Children.Num())
	{
		UApparanceResourceListEntry_Asset* pvariant = CastChecked<UApparanceResourceListEntry_Asset>( Children[variant_index] );
		if(pvariant)
		{
			return pvariant->GetAsset();
		}
	}

	//not a valid variant index
	return nullptr;
}

// edit: change an assigned asset
//
void UApparanceResourceListEntry_Variants::SetAsset( UObject* passet, int variant_index )
{
	check(variant_index>=0);
	
	//variant editing
	if(variant_index>=0 && variant_index<Children.Num())
	{
		UApparanceResourceListEntry_Asset* pvariant = Cast<UApparanceResourceListEntry_Asset>( Children[variant_index] );
		if(pvariant)
		{
			pvariant->SetAsset( passet );

			//side-effects
			Editor_OnAssetChanged();
		}
	}
}

#if 0
// edit: insert a new variant
//
void UApparanceResourceListEntry_Variants::InsertVariant( UApparanceResourceListEntry_Asset* passet, int variant_index )
{
	check(variant_index>=0 && variant_index<=Children.Num());

	Children.Insert( passet, variant_index );

	//side-effects
	Editor_OnAssetChanged();
	Editor_NotifyStructuralChange( FName() );
}

// edit: remove an old variant
//
UApparanceResourceListEntry_Asset* UApparanceResourceListEntry_Variants::RemoveVariant( int variant_index )
{
	check(variant_index>=0 && variant_index<Children.Num());

	UApparanceResourceListEntry_Asset* old_asset = Cast<UApparanceResourceListEntry_Asset>( Children[variant_index] );
	Children.RemoveAt( variant_index );

	//side-effects
	Editor_OnAssetChanged();
	Editor_NotifyStructuralChange( FName() );
	
	return old_asset;
}
#endif


#if WITH_EDITOR
void UApparanceResourceListEntry_Variants::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	if(PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UApparanceResourceListEntry_Variants, Orientation)
		|| PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UApparanceResourceListEntry_Variants, Rotation)
		|| PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UApparanceResourceListEntry_Variants, UniformScale)
		|| (PropertyChangedEvent.MemberProperty && (PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UApparanceResourceListEntry_Variants, Scale)))
		|| PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UApparanceResourceListEntry_Variants, bFixPlacement))
	{
		//rebuild all variant bounds
		for(int i=0 ; i<Children.Num() ; i++)
		{
			UApparanceResourceListEntry_3DAsset* p3d = Cast<UApparanceResourceListEntry_3DAsset>( Children[i] );
			if(p3d)
			{
				p3d->Editor_OnVariantsFixupChanged();
			}
		}
	}
		
	Super::PostEditChangeProperty( PropertyChangedEvent );
}
#endif
