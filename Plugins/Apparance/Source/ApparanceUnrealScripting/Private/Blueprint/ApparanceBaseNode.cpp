//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#include "Blueprint/ApparanceBaseNode.h"
#include "ApparanceUnreal.h"
#include "Blueprint/ApparanceBlueprintLibrary.h"
#include "ApparanceEntity.h"
#include "Utility/Synchroniser.h"
#include "Utility/ApparanceUtility.h"

//unreal
#include "BlueprintActionDatabaseRegistrar.h" //FBlueprintActionDatabaseRegistrar
#include "BlueprintNodeSpawner.h" //UBlueprintNodeSpawner
#include "EdGraphSchema_K2.h" //UEdGraphSchema_K2
#include "KismetCompiler.h" //FKismetCompilerContext
#include "K2Node_CallFunction.h" //UK2Node_Function
#include "Engine/SimpleConstructionScript.h" //USimpleConstructionScript

#define LOCTEXT_NAMESPACE "ApparanceUnreal"

// basic node properties
//
FText UApparanceBaseNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	//check what's needed
	if (TitleType == ENodeTitleType::EditableTitle)
	{
		return FText();
	}
	bool want_extended_title = TitleType == ENodeTitleType::FullTitle;

	//ask derived
	FString title = GetNodeTitleText().ToString();

	//context?
	if (want_extended_title)
	{
		FString subtitle;
		if (ProcedureID!=Apparance::InvalidID && !ProcedureName.IsEmpty())
		{
			subtitle = ProcedureName;
		}
		else
		{
			subtitle = GetNoProcSubtitleText().ToString();
		}
		if(!subtitle.IsEmpty())
		{
			title.Append(TEXT("\n"));
			title.Append( subtitle );
		}
	}

	return FText::FromString(title);
}
FText UApparanceBaseNode::GetTooltipText() const
{
	//ask derived
	return GetNodeTooltipText();
}
FText UApparanceBaseNode::GetMenuCategory() const
{
	return LOCTEXT("ApparanceBaseNode_DefaultMenuCategory", "Apparance");
}

// one-time init
//
void UApparanceBaseNode::PostPlacedNewNode()
{
	if(ProcedureID==Apparance::InvalidID)
	{
		UBlueprint* pbp = GetBlueprint();
		if(pbp)
		{
			//found Apparance Entity BP?
			if(pbp->ParentClass==AApparanceEntity::StaticClass())
			{
				//dig out instance object for bp defaults
				AActor* EditorActorInstance = pbp->SimpleConstructionScript->GetComponentEditorActorInstance();
				AApparanceEntity* pentity = Cast<AApparanceEntity>( EditorActorInstance );
				if(pentity)
				{
					SetProcedureID( pentity->ProcedureID );

					//update input nodes to show proc inputs
					ReconstructNode();
				}
			}
		}
	}
}

// put node in blueprint menu
//
void UApparanceBaseNode::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	Super::GetMenuActions( ActionRegistrar );

	UClass* Action = GetClass();
	if (ActionRegistrar.IsOpenForRegistration( Action ))
	{
		UBlueprintNodeSpawner* Spawner = UBlueprintNodeSpawner::Create(GetClass());
		check(Spawner != nullptr);
		ActionRegistrar.AddBlueprintAction(Action, Spawner);
	}
}

// lookup spec from current procedure
const Apparance::ProcedureSpec* UApparanceBaseNode::GetProcSpec()
{
	const Apparance::ProcedureSpec* spec = nullptr; 
	if (ProcedureID != Apparance::InvalidID)
	{
		if(FApparanceUnrealModule::GetLibrary())
		{
			spec = FApparanceUnrealModule::GetLibrary()->FindProcedureSpec( ProcedureID );
		}
		else
		{
			UE_LOG( LogApparance, Error, TEXT( "Unable to resolve procedure %08X details before library initialised." ), ProcedureID );
			return nullptr;
		}
	}
	return spec;
}

// check to see if pin is enabled
//
bool UApparanceBaseNode::IsPinEnabled(int id) const
{
	//enabled if not in the disabled pin list
	return !DisabledPins.Contains(id);
}

// edit pin enable state
//
void UApparanceBaseNode::SetPinEnabled(int id, bool enable)
{
	bool already_enabled = IsPinEnabled(id);
	if (enable != already_enabled)
	{
		if (enable)
		{
			DisabledPins.Remove(id);
		}
		else
		{
			DisabledPins.Add(id);
		}

		//need to refresh pin list
		ReconstructNode();			
	}
}


// node creation, populate/setup
//
void UApparanceBaseNode::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	/*Create our pins*/
	// Execution pins
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);
	
	//parameters we are exposing
	const Apparance::ProcedureSpec* spec = GetProcSpec();
	if (spec)
	{
		spec->Inputs->BeginAccess();
	}
	
	// derived implementation pins
	CustomAllocateDefaultPins( spec );

	if (spec)
	{
		spec->Inputs->EndAccess();
	}

	//Parameter Inputs
	SyncInputs();
	
	Super::AllocateDefaultPins();
}

// fixup list pins?
//
void UApparanceBaseNode::PostReconstructNode()
{
	UpdateListPinTypes();
}


// property editing
//
void UApparanceBaseNode::PostEditChangeProperty(struct FPropertyChangedEvent& e)
{
	FName PropertyName = (e.Property != NULL) ? e.Property->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UApparanceBaseNode, ProcedureID))
	{
		//reapply to update cached name
		SetProcedureID( ProcedureID );
		
		//need to refresh title
		ReconstructNode();
		
		//update inputs		
		SyncInputs();
	}

	Super::PostEditChangeProperty(e);
}

// loaded
//
void UApparanceBaseNode::PostLoad()
{
	Super::PostLoad();

	//re-apply to find name
	SetProcedureID( ProcedureID );

	UpdateListPinTypes();
}

// pin access helpers
//
UEdGraphPin* UApparanceBaseNode::GetThenPin() const
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	UEdGraphPin* Pin = FindPinChecked(UEdGraphSchema_K2::PN_Then);
	check(Pin->Direction == EGPD_Output);
	return Pin;
}

// Unreal blueprint pin type -> Apparance parameter type
//
Apparance::Parameter::Type UApparanceBaseNode::ApparanceTypeFromPinType( FEdGraphPinType pin_type, FApparanceEntityParameterVariants::Type& OutTypeVariant )
{
	OutTypeVariant = FApparanceEntityParameterVariants::None;

	//check array before all others
	if (pin_type.IsArray())
	{
		OutTypeVariant = FApparanceEntityParameterVariants::Array;
		return Apparance::Parameter::List;
	}
	else if (pin_type.IsContainer())
	{
		//other containers not allowed
		return Apparance::Parameter::None;
	}
	else if (pin_type.PinCategory == UEdGraphSchema_K2::PC_Wildcard) //wild is probably a list		
	{
		return Apparance::Parameter::List;
	}
	else if(pin_type.PinCategory == UEdGraphSchema_K2::PC_Int)
	{
		return Apparance::Parameter::Integer;
	}
#if UE_VERSION_AT_LEAST(5,0,0)
	else if(pin_type.PinCategory == UEdGraphSchema_K2::PC_Real)
#else
	else if(pin_type.PinCategory == UEdGraphSchema_K2::PC_Float)
#endif
	{
		return Apparance::Parameter::Float;
	}
	else if(pin_type.PinCategory==UEdGraphSchema_K2::PC_Boolean)
	{
		return Apparance::Parameter::Bool;
	}
	else if(pin_type.PinCategory==UEdGraphSchema_K2::PC_String)
	{
		return Apparance::Parameter::String;
	}
	else if(pin_type.PinCategory==UEdGraphSchema_K2::PC_Struct)
	{
		if(pin_type.PinSubCategoryObject.IsValid() && pin_type.PinSubCategoryObject->IsA(UScriptStruct::StaticClass()) )
		{			
			const UScriptStruct* sub_type = (UScriptStruct*)pin_type.PinSubCategoryObject.Get();

			//vec3
			if(sub_type==TBaseStructure<FVector>::Get())
			{
				return Apparance::Parameter::Vector3;
			}

			//colour
			if(sub_type==TBaseStructure<FLinearColor>::Get())
			{
				return Apparance::Parameter::Colour;
			}
			
			//frame
			if(sub_type==TBaseStructure<FApparanceFrame>::Get())
			{
				return Apparance::Parameter::Frame;
			}

			//add other specific struct interpretation here: e.g. box/transform/etc

			//otherwise handle struct as a list of members
			OutTypeVariant = FApparanceEntityParameterVariants::Structure;
			return Apparance::Parameter::List;
		}
	}

	return Apparance::Parameter::None;
}

// Apparance parameter type -> Unreal blueprint pin type
//
FName UApparanceBaseNode::PinTypeFromApparanceType( Apparance::Parameter::Type param_type, FName& out_sub_type, UScriptStruct*& out_sub_type_struct )
{
	out_sub_type = NAME_None;
	out_sub_type_struct = nullptr;
	switch(param_type)
	{
		case Apparance::Parameter::Integer:
			return UEdGraphSchema_K2::PC_Int;
		case Apparance::Parameter::Float:
#if UE_VERSION_AT_LEAST(5,0,0)
			out_sub_type = UEdGraphSchema_K2::PC_Float;
			return UEdGraphSchema_K2::PC_Real;
#else
			return UEdGraphSchema_K2::PC_Float;
#endif
		case Apparance::Parameter::Bool:
			return UEdGraphSchema_K2::PC_Boolean;
		case Apparance::Parameter::Colour:
			out_sub_type_struct = TBaseStructure<FLinearColor>::Get();
			return UEdGraphSchema_K2::PC_Struct;
		case Apparance::Parameter::String:
			return UEdGraphSchema_K2::PC_String;
		case Apparance::Parameter::Vector3:
			out_sub_type_struct = TBaseStructure<FVector>::Get();
			return UEdGraphSchema_K2::PC_Struct;
		case Apparance::Parameter::Frame:
			out_sub_type_struct = TBaseStructure<FApparanceFrame>::Get();
			return UEdGraphSchema_K2::PC_Struct;
		case Apparance::Parameter::List:
			return UEdGraphSchema_K2::PC_Wildcard; //lists default to wild
		default:
			//unknown
			return UEdGraphSchema_K2::PC_Wildcard;
	}
}

// store procedure
//
void UApparanceBaseNode::SetProcedureID( int proc_id )
{
	//id
	ProcedureID = proc_id;
	
	//resolve proc type
	const Apparance::ProcedureSpec* spec = GetProcSpec();
	
	//name
	if(spec)
	{
		ProcedureName = ANSI_TO_TCHAR( spec->Name );
	}
	else
	{
		ProcedureName.Empty();
	}
}


// input node synchronisation
//
struct FInputPinSynchroniser : FSynchroniser<int, UEdGraphPin*>
{
	//setup
	const Apparance::IParameterCollection* PinSpec;
	UK2Node* GraphNode;
	EEdGraphPinDirection PinDirection;
	TArray<int> ValidPinIDs;
	const TArray<int>& DisabledPins;

	FInputPinSynchroniser( int proc_id, UK2Node* node, const TArray<int>& disabled_pins, EEdGraphPinDirection pin_direction )
		: GraphNode( node )
		, PinDirection( pin_direction )
		, DisabledPins( disabled_pins )
	{
		const Apparance::ProcedureSpec* spec = (proc_id != Apparance::InvalidID && FApparanceUnrealModule::GetLibrary()) ? FApparanceUnrealModule::GetLibrary()->FindProcedureSpec( proc_id ) : nullptr;
		PinSpec = spec ? spec->Inputs : nullptr;
	}

	virtual ~FInputPinSynchroniser() {}

	virtual void BeginSync() override
	{
		if(PinSpec)
		{
			//pre-filter (exclude disable pins)
			int num_inputs = PinSpec->BeginAccess();
			for (int i = 0; i < num_inputs; i++)
			{
				Apparance::ValueID id = PinSpec->GetID(i);
				if (!DisabledPins.Contains(id))
				{
					ValidPinIDs.Add(id);
				}				
			}
		}
	}
	virtual int CountA() override
	{
		return ValidPinIDs.Num();
	}
	bool RelevantPin( UEdGraphPin* pin )
	{
		return pin->Direction == PinDirection                            //correct direction only
			&& pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec    //ignore exec/then
			&& pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Object  //ignore non-param inputs (e.g. Target)
			&& pin->PinType.PinSubCategoryObject != TBaseStructure<FApparanceParameters>::Get(); //(e.g. Parameters)
	}	
	virtual int CountB() override
	{
		int count = 0;
		for(int i=0 ; i<GraphNode->Pins.Num() ; i++)
		{
			UEdGraphPin* pin = GraphNode->Pins[i];
			if(RelevantPin( pin ))
			{
				count++;
			}
		}
		return count;
	}
	virtual int GetA( int index ) override
	{
		return ValidPinIDs[index];
	}
	virtual UEdGraphPin* GetB( int index ) override
	{
		int count = 0;
		for(int i=0 ; i<GraphNode->Pins.Num() ; i++)
		{
			UEdGraphPin* pin = GraphNode->Pins[i];			
			if(RelevantPin( pin ))
			{
				if(count==index)
				{
					return pin;
				}
				count++;
			}
		}
		return nullptr;
	}
	virtual bool MatchAB( int a, UEdGraphPin* b ) override
	{
		//param
		Apparance::ValueID input_id = (Apparance::ValueID)a;
		Apparance::Parameter::Type input_type = PinSpec->FindType( input_id );

		//input pin
		UEdGraphPin* pin = b;
		FApparanceEntityParameterVariants::Type pin_type_variant;
		Apparance::Parameter::Type pin_type = UApparanceBaseNode::ApparanceTypeFromPinType( b->PinType, /*out*/pin_type_variant);
		
		//just match on type for now, name will sync below
		return pin_type==input_type;
	}
	//make name for pin that encodes parameter id
	static FName MakeCodeName(int input_id)
	{
		return FName(*FString::Format(TEXT("Pin#{0}"), { input_id }));
	}
	virtual UEdGraphPin* CreateB( int a ) override
	{
		//param
		Apparance::ValueID input_id = (Apparance::ValueID)a;
		Apparance::Parameter::Type input_type = PinSpec->FindType( input_id );
		const char* name = PinSpec->FindName( input_id );
		
		//input pin
		FName sub_type = NAME_None;
		UScriptStruct* sub_type_struct = nullptr;
		FName pin_type = UApparanceBaseNode::PinTypeFromApparanceType( input_type, sub_type, sub_type_struct );
		FString full_name = ANSI_TO_TCHAR( name );
		FText display_name = FText::FromString( FApparanceUtility::GetMetadataSection( full_name, 0 ) );
		FString tool_tip = FApparanceUtility::GetMetadataSection( full_name, 1 );
		FName code_name = MakeCodeName( input_id );

		//create
		UEdGraphPin* new_pin = GraphNode->CreatePin( PinDirection, pin_type, code_name );
		new_pin->PinType.PinSubCategory = sub_type;
		new_pin->PinType.PinSubCategoryObject = sub_type_struct;
		new_pin->PinFriendlyName = display_name;	//correct display name
		if(!tool_tip.IsEmpty())
		{
			new_pin->PinToolTip = tool_tip;
		}
		//they are auto-added, remove so we can insert where needed (node still considers it owned/etc)
		GraphNode->Pins.Remove( new_pin );

		return new_pin;
	}
	virtual void SyncAB( int a, UEdGraphPin* b ) override
	{
		//sync name
		Apparance::ValueID input_id = (Apparance::ValueID)a;
		const char* name = PinSpec->FindName( input_id );
		if(name)
		{
			FString full_name = ANSI_TO_TCHAR( name );
			FText display_name = FText::FromString( FApparanceUtility::GetMetadataSection( full_name, 0 ) );
			FString tool_tip = FApparanceUtility::GetMetadataSection( full_name, 1 );

			b->PinFriendlyName = display_name;
			b->PinToolTip = tool_tip;
		}
	}
	virtual void DestroyB( UEdGraphPin* b ) override
	{
		UEdGraphNode::DestroyPin( b );
	}
	virtual void InsertB( UEdGraphPin* b, int index ) override
	{
		GraphNode->Modify();
		int insert_index = -1;
		for(int i = 0; i < GraphNode->Pins.Num(); i++)
		{
			UEdGraphPin* pin = GraphNode->Pins[i];
			if(RelevantPin( pin ))
			{				
				if(index==0)
				{
					insert_index = i;
				}
				index--;
			}
		}
		if(insert_index == -1)
		{
			insert_index = GraphNode->Pins.Num();
		}
		GraphNode->Pins.Insert( b, insert_index );
	}
	virtual void RemoveB( int index ) override
	{
		UEdGraphPin* pin = GetB( index );
		//remove from list, but don't use RemovePin as this disconnects them
		GraphNode->Pins.Remove( pin );
	}
	virtual void EndSync() override
	{
		if(PinSpec)
		{
			PinSpec->EndAccess();
		}
	}

	//other helpers

	//extract parameter id from pin name
	static int ParseCodeName(FName name)
	{
		FString n = name.ToString();
		int index = 0;
		if (n.FindChar('#', index))
		{
			int id_index = index + 1;
			int id = FCString::Atoi( &n[id_index] );
			return id;
		}
		//not found
		return 0;
	}
	//alias with better name
	int GetPinCount()
	{
		return CountB();
	}
	//alias with better name
	UEdGraphPin* GetPin(int index)
	{
		return GetB(index);
	}
};


// update input pins to match selected procedure
//
void UApparanceBaseNode::SyncInputs()
{
	FInputPinSynchroniser pin_sync( ProcedureID, this, DisabledPins, GetDyanmicPinDirection() );
	pin_sync.Sync();
}

// collect up actual parameter io pins and infer the parameter ID they relate to
//
void UApparanceBaseNode::CollectParameterPins( TMap<int,UEdGraphPin*>& param_pins, bool enabled_only )
{
	param_pins.Empty();

	//(re-use this helper that knows about pin use)
	FInputPinSynchroniser util( ProcedureID, this, DisabledPins, GetDyanmicPinDirection() );
	int num_params = util.GetPinCount();
	for(int i=0 ; i<num_params ; i++)
	{
		UEdGraphPin* pin = util.GetPin(i);
		int id = util.ParseCodeName( pin->PinName );
		if (id != 0)
		{
			bool is_enabled = IsPinEnabled( id );
			bool any_enable_state = !enabled_only;
			if(is_enabled || any_enable_state)
			{
				param_pins.Add(id, pin);
			}
		}
	}
}

// runtime node operation functionality hookup
//
void UApparanceBaseNode::ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);	
	
	//parameters we need to set
	const Apparance::ProcedureSpec* spec = GetProcSpec();
	if(spec)
	{
		spec->Inputs->BeginAccess();
	}
	
	//handle custom node expansion
	CustomExpandNode( CompilerContext, SourceGraph, spec );

	if(spec)
	{
		//done with spec
		spec->Inputs->EndAccess();
	}
	
	//After we are done we break all links to this node (not the internally created one)
	//leaving the newly created internal nodes left to do the work
	BreakAllNodeLinks();
}



// Pin was connected or disconnected
//
void UApparanceBaseNode::NotifyPinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::NotifyPinConnectionListChanged(Pin);

	UpdateListPinType(Pin);
}

// check wild inputs are correctly set up
// * List that is wild and now has connections needs changing to correct type (according to connection)
// * List that is not wild and nolonger connected needs changing back to wild
//
void UApparanceBaseNode::UpdateListPinType(UEdGraphPin* Pin)
{
	//find input pin
	TMap<int, UEdGraphPin*> param_pins;
	CollectParameterPins(param_pins);
	Apparance::ValueID ParameterID = 0;
	for (TMap<int, UEdGraphPin*>::TIterator It(param_pins); It; ++It)
	{
		UEdGraphPin* ParameterPin = It.Value();
		if (ParameterPin == Pin)
		{
			//found
			ParameterID = (Apparance::ValueID)It.Key();
			break;
		}
	}
	if (ParameterID == 0)
	{
		//not a parameter input pin
		return;
	}

	//what input type is it?
	const Apparance::ProcedureSpec* spec = GetProcSpec();
	if (!spec) //known?
	{
		return;
	}
	int num_inputs = spec->Inputs->BeginAccess();
	bool is_list = false;
	for (int i = 0; i < num_inputs; i++)
	{
		if (spec->Inputs->GetID(i) == ParameterID)
		{
			if (spec->Inputs->GetType(i) == Apparance::Parameter::List)
			{
				is_list = true;
				break;
			}
		}
	}
	spec->Inputs->EndAccess();

	//connected/disconnected
	if (is_list)
	{
		if (Pin->LinkedTo.Num() == 0 && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard)
		{
			// Reset input pin to wildcard
			Pin->PinType.ResetToDefaults();
			Pin->ResetDefaultValue();
			Pin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
		}
		else if (Pin->LinkedTo.Num() > 0 && Pin->LinkedTo[0]->PinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard)
		{
			//connected to non-wild
			auto incoming_type = Pin->LinkedTo[0]->PinType;

			if (incoming_type.PinCategory != UEdGraphSchema_K2::PC_Boolean
				&& incoming_type.PinCategory != UEdGraphSchema_K2::PC_Byte
				&& incoming_type.PinCategory != UEdGraphSchema_K2::PC_Enum
				&& incoming_type.PinCategory != UEdGraphSchema_K2::PC_Float
				&& incoming_type.PinCategory != UEdGraphSchema_K2::PC_Int
				&& incoming_type.PinCategory != UEdGraphSchema_K2::PC_Int64
				&& incoming_type.PinCategory != UEdGraphSchema_K2::PC_Name
				&& incoming_type.PinCategory != UEdGraphSchema_K2::PC_String
				&& incoming_type.PinCategory != UEdGraphSchema_K2::PC_Struct
				&& incoming_type.PinCategory != UEdGraphSchema_K2::PC_Text)
			{
				check(false);
				//CompilerContext.MessageLog.Error(*FString::Format(*LOCTEXT("ParamTypeInvalid", "Parameter pins don't support {0} data types").ToString(), { incoming_type.PinCategory }), this);
			}
			else
			{
				//apply
				Pin->PinType = incoming_type;
			}
		}
	}
}


// fix up list types
//
void UApparanceBaseNode::UpdateListPinTypes()
{
	//for all input pins
	TMap<int, UEdGraphPin*> param_pins;
	CollectParameterPins(param_pins);
	for (TMap<int, UEdGraphPin*>::TIterator It(param_pins); It; ++It)
	{
		UEdGraphPin* ParameterPin = It.Value();
		UpdateListPinType( ParameterPin );
	}
}



#undef LOCTEXT_NAMESPACE
