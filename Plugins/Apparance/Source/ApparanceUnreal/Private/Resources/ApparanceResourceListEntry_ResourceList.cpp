//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

// main
#include "ResourceList/ApparanceResourceListEntry_ResourceList.h"


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
// UApparanceResourceListEntry_ResourceList

UApparanceResourceList* UApparanceResourceListEntry_ResourceList::GetResourceList() const
{ 
	return Cast<UApparanceResourceList>( GetAsset() ); 
}

// name is fixed
//
void UApparanceResourceListEntry_ResourceList::SetName( FString name )
{
	//not allowed
	check(false);
}
FString UApparanceResourceListEntry_ResourceList::GetName() const
{
	UApparanceResourceList* pres_list = GetResourceList();
	if(pres_list)
	{
		return FName::NameToDisplayString( pres_list->GetName(), false );
	}
	return TEXT("unassigned");
}

UClass* UApparanceResourceListEntry_ResourceList::GetAssetClass() const
{ 
	return UApparanceResourceList::StaticClass(); 
}

FName UApparanceResourceListEntry_ResourceList::GetIconName( bool is_open )
{
	return StaticIconName( is_open );
}

FName UApparanceResourceListEntry_ResourceList::StaticIconName( bool is_open )
{
	return FName( "ContentBrowser.AssetActions.OpenSourceLocation" );
}

FText UApparanceResourceListEntry_ResourceList::GetAssetTypeName() const
{
	return UApparanceResourceListEntry_ResourceList::StaticAssetTypeName();
}

FText UApparanceResourceListEntry_ResourceList::StaticAssetTypeName()
{
	return LOCTEXT( "ResourceListAssetTypeName", "Resource List");
}

