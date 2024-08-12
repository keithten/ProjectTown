//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

// main
#include "ResourceList/ApparanceResourceListEntry.h"

// unreal

// module
#include "ApparanceUnreal.h"
#include "AssetDatabase.h"
//#include "Geometry.h"


//////////////////////////////////////////////////////////////////////////
// UApparanceResourceListEntry

// default entries are folder structure for resources
//
FName UApparanceResourceListEntry::GetIconName( bool is_open )
{
	return StaticIconName( is_open );
}

FName UApparanceResourceListEntry::StaticIconName( bool is_open )
{
	if(is_open)
	{
		return FName("SceneOutliner.FolderOpen");
	}
	else
	{
		return FName("SceneOutliner.FolderClosed");
	}
}	

// conversion between types
//
void UApparanceResourceListEntry::Editor_AssignFrom( UApparanceResourceListEntry* psource )
{	
	check(Children.Num()==0); //can only be used on leaf entries

	//take on name
	Name = psource->Name;
}


#if WITH_EDITOR
void UApparanceResourceListEntry::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	if(PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UApparanceResourceListEntry, Children))
	{
		//hierarchy is a structural change
		Editor_NotifyStructuralChange( PropertyChangedEvent );
	}

	if(PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UApparanceResourceListEntry, Name))
	{
		//name changes invalidate cache
		Editor_NotifyCacheInvalidatingChange();
	}

	Super::PostEditChangeProperty( PropertyChangedEvent );
}

// need editors invalidated after Undo too
void UApparanceResourceListEntry::PostEditUndo()
{
	//no idea cause of change
	Editor_NotifyStructuralChange( FName() );
}

#endif

void UApparanceResourceListEntry::Editor_NotifyStructuralChange( struct FPropertyChangedEvent& PropertyChangedEvent )
{
	Editor_NotifyStructuralChange( PropertyChangedEvent.GetPropertyName() );
}
	
// common path for entries to notify of property changes that affect structure of this asset
void UApparanceResourceListEntry::Editor_NotifyStructuralChange( FName property )
{
	//find owner
	UApparanceResourceList* pres_list = Cast<UApparanceResourceList>( GetOuter() );
	if(pres_list)
	{
		pres_list->Editor_NotifyStructuralChange( this, property );
	}
}

// request cache rebuild
//
void UApparanceResourceListEntry::Editor_NotifyCacheInvalidatingChange()
{
	//this change invalidates the cached state
	FAssetDatabase* pdb = FApparanceUnrealModule::GetModule()->GetAssetDatabase();
	if(pdb)
	{
		pdb->Invalidate();
	}
}

// some property edits are direct and buried in an object hierarchy
// these notify here directy via the custom edit controls
//
void UApparanceResourceListEntry::NotifyPropertyChanged( UObject* pobject )
{
	//implicitly, this entry has been edited and users of this asset are potentially needing update
	Editor_OnAssetChanged();
}

// an asset has been changed (or changes that affect the effective asset)
//
void UApparanceResourceListEntry::Editor_OnAssetChanged()
{
	Editor_NotifyCacheInvalidatingChange();
}


// recursive parent finder
static UApparanceResourceListEntry* FindEntryParent( UApparanceResourceListEntry* psearch, UApparanceResourceListEntry* pentry)
{
	//scan children
	for (int i = 0; i < psearch->Children.Num(); i++)
	{
		UApparanceResourceListEntry* p = psearch->Children[i];
		if (p == pentry)
		{
			//we are the parent
			return psearch;
		}

		//search within
		p = FindEntryParent(p, pentry);
		if (p)
		{
			return p;
		}
	}
	return nullptr;
}
static const UApparanceResourceListEntry* FindEntryParent( const UApparanceResourceListEntry* psearch, const UApparanceResourceListEntry* pentry )
{
	//scan children
	for(int i = 0; i < psearch->Children.Num(); i++)
	{
		const UApparanceResourceListEntry* p = psearch->Children[i];
		if(p == pentry)
		{
			//we are the parent
			return psearch;
		}

		//search within
		p = FindEntryParent( p, pentry );
		if(p)
		{
			return p;
		}
	}
	return nullptr;
}

// recursive descriptor builder
static FString FindEntryDescriptor( const UApparanceResourceListEntry* psearch, const UApparanceResourceListEntry* pentry)
{
	//me?
	if (pentry == psearch)
	{
		//right, now return all the way back up building the descriptor
		return pentry->Name;
	}
	
	//scan children
	for (int i = 0; i < psearch->Children.Num(); i++)
	{
		UApparanceResourceListEntry* p = psearch->Children[i];
		FString d = FindEntryDescriptor(p, pentry);
		if (!d.IsEmpty())
		{
			//found, prefix with our name
			d.InsertAt(0, TEXT("."));
			d.InsertAt(0, psearch->Name);
			return d;
		}
	}
	return TEXT("");
}


// build resource descriptor string for this resource
// NOTE: requires resource list search, see FindParent
//
FString UApparanceResourceListEntry::BuildDescriptor() const
{
	FString descriptor;
	UApparanceResourceList* pres_list = GetTypedOuter<UApparanceResourceList>();
	if (pres_list)
	{
		//scan from resource root down for entry
		descriptor = FindEntryDescriptor(pres_list->Resources, this);
		if (descriptor.IsEmpty())
		{
			//check references too
			descriptor = FindEntryDescriptor(pres_list->References, this);
		}
	}
	return descriptor;
}

// get parent
// NOTE: need to search down from containing Resource List as parentage isn't maintained nor reflected in UObject outer hierarchy
//
const UApparanceResourceListEntry* UApparanceResourceListEntry::FindParent() const
{
	const UApparanceResourceListEntry* p = nullptr;
	const UApparanceResourceList* pres_list = GetTypedOuter<UApparanceResourceList>();
	if (pres_list)
	{
		//try resources
		p = FindEntryParent(pres_list->Resources, this);
		if (!p)
		{
			//try references
			p = FindEntryParent(pres_list->References, this);
		}
	}
	return p;
}
UApparanceResourceListEntry* UApparanceResourceListEntry::FindParent()
{
	UApparanceResourceListEntry* p = nullptr;
	UApparanceResourceList* pres_list = GetTypedOuter<UApparanceResourceList>();
	if(pres_list)
	{
		//try resources
		p = FindEntryParent( pres_list->Resources, this );
		if(!p)
		{
			//try references
			p = FindEntryParent( pres_list->References, this );
		}
	}
	return p;
}
