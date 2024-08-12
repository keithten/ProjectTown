//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

// main
#include "ApparanceResourceList.h"


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
// UApparanceResourceList


//editing
void UApparanceResourceList::Editor_InitForEditing()
{
	//ensure has root category
	if(!Resources)
	{
		Resources = NewObject<UApparanceResourceListEntry>(this, NAME_None, RF_Transactional);
		Resources->SetName( TEXT("New category") );
	}

	//ensure has references root
	if(!References)
	{
		References = NewObject<UApparanceResourceListEntry>( this, NAME_None, RF_Transactional);
		References->SetName( TEXT("References") );
	}
	References->SetNameEditable( false );
}

// look for a resource entry for a specific asset
//
const UApparanceResourceListEntry* UApparanceResourceList::FindResourceEntry(const FString& asset_descriptor) const
{
	return SearchHierarchy( Resources, asset_descriptor, 0 );
}

// recursive search helper, matches entry names against successive parts of the resource descriptor
//
UApparanceResourceListEntry* UApparanceResourceList::SearchHierarchy( UApparanceResourceListEntry* pentry, const FString& asset_descriptor, int descriptor_index )
{
	//nothing to search in?
	if(!pentry)
	{
		return nullptr;
	}

	//match current part of descriptor with this entry name
	const TCHAR* pdfull = &asset_descriptor[0];
	const TCHAR* pdpart = (descriptor_index < asset_descriptor.Len()) ? &asset_descriptor[descriptor_index] : TEXT( "" );
	const TCHAR* pd = pdpart;
	FString name = pentry->GetName();
	const TCHAR* pn = &name[0];

	const TCHAR terminator = TCHAR('\0');
	const TCHAR separator = TCHAR('.');
	const TCHAR variant_signifier = TCHAR('#');
	
	do{
		TCHAR d = FChar::ToLower( *pd );
		TCHAR n = FChar::ToLower( *pn );

		if(d==terminator 
			|| d==variant_signifier)	//ignore variants for purposes of matching
		{
			//end of descriptor
			//if name also ends then must have matched this far
			if(n==terminator)
				return pentry;
			else
				return nullptr;
		}
		if(d==separator && n==terminator)
		{
			//scan children to resolve the rest of the descriptor string
			for(int i=0 ; i<pentry->Children.Num() ; i++)
			{
				int next_part = pd-pdfull+1;
				UApparanceResourceListEntry* found = SearchHierarchy( pentry->Children[i], asset_descriptor, next_part );
				if(found)
				{
					//matched by child entry
					return found;
				}
			}

			//no further entries here to use to resolve the descriptor
			return nullptr;
		}
		if (n == terminator)
		{
			//matched, but more of this part and name has run out so doesn't match
			return nullptr;
		}
		if (d != n)
		{
			//mis-match of names, can't match any deeper
			return nullptr;
		}

		//next character
		pd++;
		pn++;
	}while(true);
}

// part of our structure has changed, pass on notification to any editing tools interested
//
void UApparanceResourceList::Editor_NotifyStructuralChange( class UApparanceResourceListEntry* pobject, FName property_name )
{
	FApparanceUnrealModule::GetModule()->Editor_NotifyResourceListStructureChanged(this, pobject, property_name);
}

// just loaded
//
void UApparanceResourceList::PostLoad()
{
	Super::PostLoad();

	//fixups
	FixupOwnership( Resources );
	FixupOwnership( References );
}

/// <summary>
/// originally resource list entries were parented according to the entry hierarchy, but this makes for pain
/// when maintaining correctness and liable to break by moving things around the hierarchy. Now they are all
/// owned by the actual resource list itself. This fn fixes old versions of the data by re-parenting all entries.
/// </summary>
void UApparanceResourceList::FixupOwnership( class UApparanceResourceListEntry* p )
{
	//fix this entry
	if(p->GetOuter() != this)
	{
		UE_LOG( LogApparance, Display, TEXT( "Moving ResourceList Entry '%s' to new owner: '%s' --> '%s'" ), *p->GetName(), *p->GetOuter()->GetName(), *GetName() );
		ERenameFlags rename_flags = REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional;
		p->Rename( nullptr, this, rename_flags );
	}

	//recurse through children
	for(int i = 0; i < p->Children.Num(); i++)
	{
		FixupOwnership( p->Children[i] );
	}
}
