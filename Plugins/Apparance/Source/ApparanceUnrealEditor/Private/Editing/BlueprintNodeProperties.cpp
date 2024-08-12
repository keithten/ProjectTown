//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

//main
#include "BlueprintNodeProperties.h"

//unreal
//#include "Core.h"
//unreal:editor
#include "Editor.h"
#include "DetailLayoutBuilder.h"


#define LOCTEXT_NAMESPACE "ApparanceUnreal"



void FNodePropertyCustomisation_Base::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	//keep ref to node we are editing
	TArray<TWeakObjectPtr<UObject>> EditedObjects;
	DetailBuilder.GetObjectsBeingCustomized( EditedObjects );	
	for (int32 i = 0; i < EditedObjects.Num(); i++)
	{
		Node = Cast<UApparanceBaseNode>( EditedObjects[i].Get() );
		if(Node)
		{
			break;
		}
	}		
	
	//carry on with UI building
	FEntityPropertyCustomisation::CustomizeDetails(DetailBuilder);
}


// current pin state
//
bool FNodePropertyCustomisation_Base::IsPinEnabled(int param_id) const
{
	return Node->IsPinEnabled(param_id);
}

// change pin state
//
void FNodePropertyCustomisation_Base::SetPinEnable(int param_id, bool enable, bool editing_transaction)
{
	if(editing_transaction)
	{
		//we are going to commit change made to parameter collection
		//(edits are applied to the non-persistent, expanded version of the parameters, then applied)
		GEditor->BeginTransaction( LOCTEXT("ParameterNodeEdit", "Enable/Disable Parameter Pin") );
		
		//we are about to change the entity
		Node->Modify();
	}
	
	//apply edit
	Node->SetPinEnabled(param_id, enable);
	
	//end transaction		
	if (editing_transaction)
	{		
		//commit edit
		GEditor->EndTransaction();
	}
}



#undef LOCTEXT_NAMESPACE
