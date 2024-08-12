//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

//main
#include "ApparanceResourceListActions.h"

//unreal

//module
#include "ApparanceUnrealEditor.h"
#include "ApparanceResourceList.h"
#include "ResourceListEditor.h"

//module editor


#define LOCTEXT_NAMESPACE "ApparanceUnreal"

// IAssetTypeActions Implementation
FText FApparanceUnrealEditorResourceListActions::GetName() const
{
	return NSLOCTEXT("ApparanceUnreal", "ApparanceResourceList", "Apparance Resource List");
}

FColor FApparanceUnrealEditorResourceListActions::GetTypeColor() const
{
	return FColor::Emerald;
}

UClass* FApparanceUnrealEditorResourceListActions::GetSupportedClass() const
{
	return UApparanceResourceList::StaticClass();
}

uint32 FApparanceUnrealEditorResourceListActions::GetCategories()
{
	return RegisteredCategoryHandle;
}

void FApparanceUnrealEditorResourceListActions::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor )
{
	for (UObject* obj : InObjects)
	{
		UApparanceResourceList* res_list = Cast<UApparanceResourceList>(obj);
		if (res_list)
		{
			//open panel
			TSharedRef< FResourceListEditor > new_resource_list_editor( new FResourceListEditor() );
			new_resource_list_editor->InitResourceListEditor( EToolkitMode::Standalone, EditWithinLevelEditor, res_list );
		}
	}

	//return new_resource_list_editor;
}

#undef LOCTEXT_NAMESPACE

