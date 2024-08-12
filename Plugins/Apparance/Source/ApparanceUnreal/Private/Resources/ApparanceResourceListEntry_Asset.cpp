//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

// main
#include "ResourceList/ApparanceResourceListEntry_Asset.h"


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
// UApparanceResourceListEntry_Asset


// default resource icon
//
FName UApparanceResourceListEntry_Asset::GetIconName( bool is_open )
{
	return StaticIconName( is_open );
}

FName UApparanceResourceListEntry_Asset::StaticIconName( bool is_open )
{
	return FName("GraphEditor.Macro.IsValid_16x"); //now used as placeholder for un-assigned type of asset
	//return FName("Kismet.VariableList.TypeIcon");
}

UClass* UApparanceResourceListEntry_Asset::GetAssetClass() const
{
	//show compatibility with any asset type potentially as we are used on out own to represent a 'unassigned' type of asset slot
	return UObject::StaticClass();
}

// access to asset
//
UObject* UApparanceResourceListEntry_Asset::GetAsset() const
{
	//non-variant access
	return Asset;
}

// edit: change an assigned asset
//
void UApparanceResourceListEntry_Asset::SetAsset( UObject* passet )
{
	check(passet==nullptr || passet->IsA( GetAssetClass() ));

	//non-variant editing
	Asset = passet;

	//option for derived init
	CheckAsset();
	
	//side-effects
	Editor_OnAssetChanged();
}

#if WITH_EDITOR
void UApparanceResourceListEntry_Asset::PostEditChangeProperty(struct FPropertyChangedEvent& e)
{
	if(e.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UApparanceResourceListEntry_Asset, Asset))
	{
		//res list editor needs to know of manual asset change so they can be changed to appropriate type
		UApparanceResourceList* pres_list = GetTypedOuter<UApparanceResourceList>();
		FApparanceUnrealModule::GetModule()->Editor_NotifyResourceListAssetChanged( pres_list, this, Asset );
		
		//invalidates built content too
		Editor_OnAssetChanged();
	}

	//changes within complex member properties
	if(e.MemberProperty && e.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED( UApparanceResourceListEntry_Asset, ExpectedParameters ))
	{
		CheckParameterList();
	}
	
	Super::PostEditChangeProperty( e );
}
#endif

// setup checks
//
void UApparanceResourceListEntry_Asset::PostLoad()
{
	CheckParameterList();
	Super::PostLoad();
}
void UApparanceResourceListEntry_Asset::PostInitProperties()
{
	CheckParameterList();
	Super::PostInitProperties();
}

// validate parameter list
//
void UApparanceResourceListEntry_Asset::CheckParameterList()
{
	//ensure the default first parameter is set up properly
	//for 3d placeable assets the first parameter will always be the frame used for placement
	if(IsPlaceable() && ExpectedParameters.Num() == 0)
	{
		FApparancePlacementParameter placement_frame;
		placement_frame.ID = 1;
		placement_frame.Type = EApparanceParameterType::Frame;
		placement_frame.Name = TEXT("Placement Frame");
		ExpectedParameters.Add( placement_frame );
	}

	//id checks
	int id = 0;
	for (int i = 0; i < ExpectedParameters.Num(); i++)
	{
		//find current max
		id = FMath::Max(id, ExpectedParameters[i].ID);
	}
	id++;
	for (int i = 0; i < ExpectedParameters.Num(); i++)
	{
		//ensure all have an ID
		if (ExpectedParameters[i].ID == 0)
		{
			ExpectedParameters[i].ID = id;
			id++;
		}
	}
}

// look up parameter info
//
const FApparancePlacementParameter* UApparanceResourceListEntry_Asset::FindParameterInfo(int param_id, int* param_index_out) const
{
	for (int i = 0; i < ExpectedParameters.Num(); i++)
	{
		if (ExpectedParameters[i].ID == param_id)
		{
			if (param_index_out)
			{
				*param_index_out = i;
			}
			return &ExpectedParameters[i];
		}
	}
	//not found
	if(param_index_out)
	{
		*param_index_out = -1;
	}
	return nullptr;
}

// look up name for an expected parameter
//
const FText UApparanceResourceListEntry_Asset::FindParameterNameText(int param_id) const
{
	for (int i = 0; i < ExpectedParameters.Num(); i++)
	{
		if (ExpectedParameters[i].ID==param_id)
		{
			return FText::FromString(ExpectedParameters[i].Name);
		}
	}
	//not found
	return LOCTEXT("ResourceListParameterNone","None");
}

// look up first frame info
//
const FApparancePlacementParameter* UApparanceResourceListEntry_Asset::FindFirstFrameParameterInfo() const
{
	for (int i = 0; i < ExpectedParameters.Num(); i++)
	{
		if (ExpectedParameters[i].Type==EApparanceParameterType::Frame)
		{
			return &ExpectedParameters[i];
		}
	}
	//not found
	return nullptr;
}

#undef LOCTEXT_NAMESPACE
