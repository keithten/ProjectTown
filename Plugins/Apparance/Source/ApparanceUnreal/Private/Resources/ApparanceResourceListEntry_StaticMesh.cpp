//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

// main
#include "ResourceList/ApparanceResourceListEntry_StaticMesh.h"


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

#define LOCTEXT_NAMESPACE "ApparanceUnreal"


//////////////////////////////////////////////////////////////////////////
// UApparanceResourceListEntry_StaticMesh

FName UApparanceResourceListEntry_StaticMesh::GetIconName( bool is_open )
{
	return StaticIconName( is_open );
}

FName UApparanceResourceListEntry_StaticMesh::StaticIconName( bool is_open )
{
	//return FName("ClassIcon.StaticMeshActor"); //grey little house
	return FName("ShowFlagsMenu.StaticMeshes");	//blue version of little house
}

FText UApparanceResourceListEntry_StaticMesh::GetAssetTypeName() const
{
	return UApparanceResourceListEntry_StaticMesh::StaticAssetTypeName();
}

FText UApparanceResourceListEntry_StaticMesh::StaticAssetTypeName()
{
	return LOCTEXT( "StaticMeshAssetTypeName", "Static Mesh");
}

// extract and cache off bounds information
//
void UApparanceResourceListEntry_StaticMesh::Editor_RebuildSource()
{
	//if our bounds is wanted
	if(BoundsSource == EApparanceResourceBoundsSource::Asset)
	{
		UStaticMesh* pmesh = GetMesh();
		if(pmesh)
		{
			//use first mesh as bounds
			FBox bounds = pmesh->GetBoundingBox();
			BoundsCentre = bounds.GetCenter();
			BoundsExtent = bounds.GetExtent();
		}
	}

	Super::Editor_RebuildSource();
}


#if WITH_EDITOR
void UApparanceResourceListEntry_StaticMesh::PostEditChangeProperty( struct FPropertyChangedEvent& PropertyChangedEvent )
{
	FName prop_name = PropertyChangedEvent.GetPropertyName();
	FName struct_name = NAME_None;
	if(PropertyChangedEvent.MemberProperty)
	{
		struct_name = PropertyChangedEvent.MemberProperty->GetFName();
	}
	if(
		prop_name == GET_MEMBER_NAME_CHECKED( UApparanceResourceListEntry_StaticMesh, EnableInstancing )
		)
	{
		//re-build asset database with new asset info
		//(note: will trigger bounds rebuild we require, see Editor_OnAssetChanged override above)
		Editor_OnAssetChanged();
	}

	Super::PostEditChangeProperty( PropertyChangedEvent );
}
#endif

#undef LOCTEXT_NAMESPACE
