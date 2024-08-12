//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

// main
#include "ResourceList/ApparanceResourceListEntry_Blueprint.h"


// unreal
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
//#include "Components/BoxComponent.h"
//#include "Components/BillboardComponent.h"
//#include "Components/DecalComponent.h"
//#include "UObject/ConstructorHelpers.h"
//#include "Engine/Texture2D.h"
//#include "ProceduralMeshComponent.h"
//#include "Components/InstancedStaticMeshComponent.h"
// module
#include "ApparanceUnreal.h"
#include "AssetDatabase.h"
//#include "Geometry.h"

#define LOCTEXT_NAMESPACE "ApparanceUnreal"


//////////////////////////////////////////////////////////////////////////
// UApparanceResourceListEntry_Blueprint

FName UApparanceResourceListEntry_Blueprint::GetIconName( bool is_open )
{
	return StaticIconName( is_open );
}

FName UApparanceResourceListEntry_Blueprint::StaticIconName( bool is_open )
{
	return FName("ClassIcon.SphereComponent");
}

FText UApparanceResourceListEntry_Blueprint::GetAssetTypeName() const
{
	return UApparanceResourceListEntry_Blueprint::StaticAssetTypeName();
}

FText UApparanceResourceListEntry_Blueprint::StaticAssetTypeName()
{
	return LOCTEXT( "BlueprintAssetTypeName", "Blueprint");
}

// extract and cache off bounds information
//
void UApparanceResourceListEntry_Blueprint::Editor_RebuildSource()
{
	check(GIsEditor);
	//if our bounds is wanted
	if(BoundsSource == EApparanceResourceBoundsSource::Asset)
	{
		//dynamically attempt to determine bounds by seeing what the blueprint spawns

		//attempt to build from an instance of the blueprint
		UBlueprint* pblueprint = GetBlueprint();
		if(pblueprint)
		{
			//spawn
			FActorSpawnParameters asp;
			asp.ObjectFlags = RF_Transient | RF_DuplicateTransient;
#if WITH_EDITOR
				asp.bTemporaryEditorActor = true;
				asp.bHideFromSceneOutliner = true;
#endif
			FVector offset = FVector::ZeroVector;
			FRotator rot = FRotator::ZeroRotator;
			UWorld* temp_world = FApparanceUnrealModule::GetModule()->Editor_GetTempWorld();
			AActor* pactor = temp_world->SpawnActor( pblueprint->GeneratedClass, &offset, &rot, asp );

			//find largest mesh component
			TArray<UStaticMeshComponent*> meshes;
			pactor->GetComponents( meshes, true );
			UStaticMeshComponent* largest = nullptr;
			float volume = -1.0f;
			for(int i = 0; i < meshes.Num(); i++)
			{
				FVector extent = meshes[i]->Bounds.BoxExtent;
				float v = extent.X * extent.Y * extent.Z;
				if(v > volume)
				{
					largest = meshes[i];
					volume = v;
				}
			}
			if(largest)
			{
				//use this as our bounds
				FBox bounds = largest->Bounds.GetBox();
				BoundsCentre = bounds.GetCenter();
				BoundsExtent = bounds.GetExtent();
			}

			//clean up
			pactor->Destroy();
		}
	}
	Super::Editor_RebuildSource();
}


// conversion between types
//
void UApparanceResourceListEntry_Blueprint::Editor_AssignFrom( UApparanceResourceListEntry* psource )
{	
	UApparanceResourceListEntry_StaticMesh* pmesh = Cast<UApparanceResourceListEntry_StaticMesh>( psource );
	if(pmesh)
	{
		//assigning from a mesh, we adopt it as our bounds example
		BoundsMesh = pmesh->GetMesh();
	}
	
	Super::Editor_AssignFrom( psource );
}


#if WITH_EDITOR
void UApparanceResourceListEntry_Blueprint::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	if(PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UApparanceResourceListEntry_Blueprint, BoundsMesh))
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
void UApparanceResourceListEntry_Blueprint::PostLoad()
{
#if WITH_EDITOR
	//initial sync cache
	BlueprintClass = GetBlueprintClass();	
#endif

	Super::PostLoad();
}


// our asset changed
//
void UApparanceResourceListEntry_Blueprint::Editor_OnAssetChanged()
{
#if WITH_EDITOR
	//sync cache
	BlueprintClass = GetBlueprintClass();
#endif

	Super::Editor_OnAssetChanged();
}