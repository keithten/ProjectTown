//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#include "Blueprint/ApparanceRebuildNode.h"
#include "ApparanceUnreal.h"
#include "Blueprint/ApparanceBlueprintLibrary.h"
#include "ApparanceEntity.h"
#include "Utility/Synchroniser.h"

//unreal
#include "BlueprintActionDatabaseRegistrar.h" //FBlueprintActionDatabaseRegistrar
#include "BlueprintNodeSpawner.h" //UBlueprintNodeSpawner
#include "EdGraphSchema_K2.h" //UEdGraphSchema_K2
#include "KismetCompiler.h" //FKismetCompilerContext
#include "K2Node_CallFunction.h" //UK2Node_Function
#include "K2Node_Self.h" //UK2Node_Self
#include "Engine/SimpleConstructionScript.h" //USimpleConstructionScript

#define LOCTEXT_NAMESPACE "ApparanceUnreal"

// name constants
//
namespace FApparanceRebuildNodePinNames
{
#if UE_VERSION_AT_LEAST(5,0,0)
	static const FName TargetPinName( UEdGraphSchema_K2::PN_Self );
	static const FText TargetPinDisplayName( LOCTEXT("TargetSelfPinDisplayName","Target" ) );
#else
	static const FName TargetPinName( "Target" );
#endif
	static const FName ParametersPinName("Parameters");
}  	
namespace FApparanceEntityFunctionNames
{
	static const FName RebuildEntity(GET_FUNCTION_NAME_CHECKED(UApparanceBlueprintLibrary, RebuildEntity));
}
namespace FApparanceEntityFunctionPinNames
{
	static const FName EntityPinName("Entity");
}
namespace FApparanceEntityParameterSetterFunctionNames
{
	static const FName IntSetterName(GET_FUNCTION_NAME_CHECKED(UApparanceBlueprintLibrary, SetEntityIntParameter));
	static const FName FloatSetterName(GET_FUNCTION_NAME_CHECKED(UApparanceBlueprintLibrary, SetEntityFloatParameter));
	static const FName BoolSetterName(GET_FUNCTION_NAME_CHECKED(UApparanceBlueprintLibrary, SetEntityBoolParameter));
	static const FName StringSetterName(GET_FUNCTION_NAME_CHECKED(UApparanceBlueprintLibrary, SetEntityStringParameter));
	static const FName ColourSetterName(GET_FUNCTION_NAME_CHECKED(UApparanceBlueprintLibrary, SetEntityColourParameter));
	static const FName Vector3SetterName(GET_FUNCTION_NAME_CHECKED(UApparanceBlueprintLibrary, SetEntityVector3Parameter));
	static const FName FrameSetterName(GET_FUNCTION_NAME_CHECKED(UApparanceBlueprintLibrary, SetEntityFrameParameter));
	static const FName ListStructSetterName(GET_FUNCTION_NAME_CHECKED(UApparanceBlueprintLibrary, SetEntityListParameter_Struct));
	static const FName ListArraySetterName(GET_FUNCTION_NAME_CHECKED(UApparanceBlueprintLibrary, SetEntityListParameter_Array));

	static const FName ParametersSetterName(GET_FUNCTION_NAME_CHECKED(UApparanceBlueprintLibrary, SetEntityParameters));
}
namespace FApparanceEntityParameterSetterPinNames
{
	static const FName EntityPinName("Entity");
	static const FName ParameterIDPinName("ParameterID");
	static const FName ValuePinName("Value");
	static const FName ParametersInputName("Parameters");
}

// basic node properties
//
FText UApparanceRebuildNode::GetNodeTitleText() const
{
	return LOCTEXT("ApparanceRebuildNode_Title", "Rebuild Apparance Entity");
}
FText UApparanceRebuildNode::GetNoProcSubtitleText() const
{
	return LOCTEXT("ApparanceRebuildNode_NoProc", "using parameter list");
}
FText UApparanceRebuildNode::GetNodeTooltipText() const
{
	return LOCTEXT("ApparanceRebuildNode_Tooltip", "Rebuilds the procedural content of an Apparance Entity actor");
}
FText UApparanceRebuildNode::GetMenuCategory() const
{
	return LOCTEXT("ApparanceRebuildNode_MenuCategory", "Apparance|Entity");
}

// custom pins
//
void UApparanceRebuildNode::CustomAllocateDefaultPins( const Apparance::ProcedureSpec* spec )
{
#if UE_VERSION_AT_LEAST(5,0,0)
	UEdGraphPin* TargetPin = CreatePin( EGPD_Input, UEdGraphSchema_K2::PC_Object, AApparanceEntity::StaticClass(), FApparanceRebuildNodePinNames::TargetPinName ); //seems that std name enables [self] default functionality NOTE: breaking change
# if WITH_EDITORONLY_DATA
	TargetPin->PinFriendlyName = FText( FApparanceRebuildNodePinNames::TargetPinDisplayName ); //maintain original name in UI
# endif
#else
	CreatePin( EGPD_Input, UEdGraphSchema_K2::PC_Object, AApparanceEntity::StaticClass(), FApparanceRebuildNodePinNames::TargetPinName );
#endif

	//parameter pin (if no proc selected)
	if (!spec)
	{
		CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Struct, TBaseStructure<FApparanceParameters>::Get(), FApparanceRebuildNodePinNames::ParametersPinName);
	}
}

// pin access helpers
//
UEdGraphPin* UApparanceRebuildNode::GetTargetPin() const
{
	UEdGraphPin* Pin = FindPin(FApparanceRebuildNodePinNames::TargetPinName);
	check(Pin == NULL || Pin->Direction == EGPD_Input);
	return Pin;
}
UEdGraphPin* UApparanceRebuildNode::GetParametersPin() const
{
	UEdGraphPin* Pin = FindPin(FApparanceRebuildNodePinNames::ParametersPinName);
	check(Pin == NULL || Pin->Direction == EGPD_Input);
	return Pin;
}


// find setter for particular parameter type
//
UFunction* UApparanceRebuildNode::FindParameterSetterFunctionByType( Apparance::Parameter::Type param_type, FApparanceEntityParameterVariants::Type variant )
{
	FName FunctionName = NAME_None;
	switch(param_type)
	{
		case Apparance::Parameter::Integer:
			FunctionName = FApparanceEntityParameterSetterFunctionNames::IntSetterName;
			break;
		case Apparance::Parameter::Float:
			FunctionName = FApparanceEntityParameterSetterFunctionNames::FloatSetterName;
			break;
		case Apparance::Parameter::Bool:
			FunctionName = FApparanceEntityParameterSetterFunctionNames::BoolSetterName;
			break;
		case Apparance::Parameter::String:
			FunctionName = FApparanceEntityParameterSetterFunctionNames::StringSetterName;
			break;
		case Apparance::Parameter::Colour:
			FunctionName = FApparanceEntityParameterSetterFunctionNames::ColourSetterName;
			break;
		case Apparance::Parameter::Vector3:
			FunctionName = FApparanceEntityParameterSetterFunctionNames::Vector3SetterName;
			break;
		case Apparance::Parameter::Frame:
			FunctionName = FApparanceEntityParameterSetterFunctionNames::FrameSetterName;
			break;
		case Apparance::Parameter::List:
			if (variant == FApparanceEntityParameterVariants::Structure)
			{
				FunctionName = FApparanceEntityParameterSetterFunctionNames::ListStructSetterName;
			}
			if (variant == FApparanceEntityParameterVariants::Array)
			{
				FunctionName = FApparanceEntityParameterSetterFunctionNames::ListArraySetterName;
			}
			break;
		
		//TODO: ?
	}
		
	//resolve function	
	if (!FunctionName.IsNone())
	{
		UClass* LibraryClass = UApparanceBlueprintLibrary::StaticClass();
		return LibraryClass->FindFunctionByName( FunctionName );
	}
	return nullptr;
}

// function to set up an entity from single parameter list
//
UFunction* UApparanceRebuildNode::FindParametersSetFunction()
{
	UClass* LibraryClass = UApparanceBlueprintLibrary::StaticClass();
	return LibraryClass->FindFunctionByName( FApparanceEntityParameterSetterFunctionNames::ParametersSetterName );
}

// function to rebuild an entity immediately
//
UFunction* UApparanceRebuildNode::FindRebuildFunction()
{
	UClass* LibraryClass = UApparanceBlueprintLibrary::StaticClass();
	return LibraryClass->FindFunctionByName( FApparanceEntityFunctionNames::RebuildEntity );
}


// runtime node operation functionality hookup
//
void UApparanceRebuildNode::CustomExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, const Apparance::ProcedureSpec* spec )
{
	//input pins : exec (execution triggered)
	UEdGraphPin* MainExecPin = GetExecPin();
	//input pins : target (entity actor)
	UEdGraphPin* MainTargetPin = GetTargetPin();
	//output pins : then (execution continues)
	UEdGraphPin* MainThenPin = GetThenPin();

	//if target not connected, default to a self node
	UEdGraphPin* SelfTargetPin = nullptr;
#if UE_VERSION_AT_LEAST(5,0,0)
	if(MainTargetPin->LinkedTo.IsEmpty())
	{
		UK2Node_Self* SelfNode = CompilerContext.SpawnIntermediateNode<UK2Node_Self>( this, SourceGraph );
		SelfNode->AllocateDefaultPins();
		SelfTargetPin = SelfNode->FindPinChecked( UEdGraphSchema_K2::PSC_Self );
	}
#endif

	//use individual parameter inputs to set up entity
	UEdGraphPin* CurrentTargetPin = nullptr;
	UEdGraphPin* CurrentThenPin = nullptr;
	if (spec)
	{
		//input pins : parameter inputs
		TMap<int,UEdGraphPin*> param_pins;
		CollectParameterPins( param_pins, true/*connected only*/ );

		//each parameter pin needs a function call node chained together to fully set up the parameter input of the entity
		for (TMap<int, UEdGraphPin*>::TIterator It(param_pins); It; ++It)
		{
			bool first_time = CurrentThenPin == nullptr;

			//id of param we are hooking up
			Apparance::ValueID ParameterID = (Apparance::ValueID)It.Key();
			UEdGraphPin* ParameterPin = It.Value();
			if (ParameterPin == nullptr)
			{
				//skip unconnected List pins (still wild)
				continue;
			}

			//ensure parameter type is correct
			Apparance::Parameter::Type ParameterSpecType = spec->Inputs->FindType(ParameterID);
			FApparanceEntityParameterVariants::Type ParameterPinTypeVariant = FApparanceEntityParameterVariants::None;
			Apparance::Parameter::Type ParameterPinType = ApparanceTypeFromPinType( ParameterPin->PinType, /*out*/ParameterPinTypeVariant );
			if (ParameterPinType != ParameterSpecType)
			{
				CompilerContext.MessageLog.Error(*FString::Format(*LOCTEXT("ParamTypeMismatch", "Type of input parameter {0} has changed to {1}, expected {2}.").ToString(), { (int)ParameterID, ParameterPinType, ParameterSpecType }), this);
				//can't bind this pin
				continue;
			}

			//find a function to handle setting this parameter on the entity
			UFunction* ParamSetFunction = FindParameterSetterFunctionByType(ParameterPinType, ParameterPinTypeVariant);
			if (!ParamSetFunction)
			{
				if (ParameterPinType != Apparance::Parameter::List) //just skip unconnected list params
				{
				CompilerContext.MessageLog.Error(*FString::Format(*LOCTEXT("MissingEntityParamSetter", "Failed to find function to set entity parameter of type {0}.").ToString(), { ParameterPinType }), this);
			}
				continue;
			}

			//create intermediate node to call the setter
			UK2Node_CallFunction* CallSetterFn = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
			CallSetterFn->SetFromFunction(ParamSetFunction);
			CallSetterFn->AllocateDefaultPins();
			CompilerContext.MessageLog.NotifyIntermediateObjectCreation(CallSetterFn, this);

			//setter fn pins
			UEdGraphPin* FnExecPin = CallSetterFn->GetExecPin();
			UEdGraphPin* FnThenPin = CallSetterFn->GetThenPin();
			UEdGraphPin* FnTargetPin = CallSetterFn->FindPinChecked(FApparanceEntityParameterSetterPinNames::EntityPinName);
			UEdGraphPin* FnParamPin = CallSetterFn->FindPinChecked(FApparanceEntityParameterSetterPinNames::ParameterIDPinName);
			UEdGraphPin* FnValuePin = CallSetterFn->FindPinChecked(FApparanceEntityParameterSetterPinNames::ValuePinName);

			//hook up function inputs to previous outputs
			if (first_time)
			{
				//first one uses main input exec
				CompilerContext.MovePinLinksToIntermediate(*MainExecPin, *FnExecPin);
			}
			else
			{
				//subsequent need internal links
				CurrentThenPin->MakeLinkTo(FnExecPin);
			}

			//hook up target input (same for all setters)
			if (first_time)
			{
				//first use...
				if(SelfTargetPin)
				{
					//self node (default if unconnected main target pin)
					FnTargetPin->MakeLinkTo( SelfTargetPin );
				}
				else
				{
					//main input
					CompilerContext.MovePinLinksToIntermediate(*MainTargetPin, *FnTargetPin);
				}
			}
			else
			{
				//then copy from prev
				CompilerContext.CopyPinLinksToIntermediate(*CurrentTargetPin, *FnTargetPin);
			}
			CurrentTargetPin = FnTargetPin;

			//hook up parameter input
			if (FnValuePin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard)
			{
				//need to propagate type to generic input pin on list set fn node
				FnValuePin->PinType = ParameterPin->PinType;
			}
			CompilerContext.MovePinLinksToIntermediate(*ParameterPin, *FnValuePin);

			//set param id function input
			FnParamPin->DefaultValue = FString::FromInt(ParameterID);

			//move on to function outputs
			CurrentThenPin = FnThenPin;
		}
	}
	else
	{
		//use single parameter collection input to set up entity
		UEdGraphPin* MainParametersPin = GetParametersPin();
		if (MainParametersPin->HasAnyConnections())
		{
			//find a function to handle setting this parameter on the entity
			UFunction* ParametersSetFunction = FindParametersSetFunction();
			//create intermediate node to call the setter
			UK2Node_CallFunction* CallParamsSetFn = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
			CallParamsSetFn->SetFromFunction(ParametersSetFunction);
			CallParamsSetFn->AllocateDefaultPins();
			CompilerContext.MessageLog.NotifyIntermediateObjectCreation(CallParamsSetFn, this);
			//setter fn pins
			UEdGraphPin* FnExecPin = CallParamsSetFn->GetExecPin();
			UEdGraphPin* FnThenPin = CallParamsSetFn->GetThenPin();
			UEdGraphPin* FnTargetPin = CallParamsSetFn->FindPinChecked(FApparanceEntityParameterSetterPinNames::EntityPinName);
			UEdGraphPin* FnParamsPin = CallParamsSetFn->FindPinChecked(FApparanceEntityParameterSetterPinNames::ParametersInputName);

			//hook up fn inputs
			if(SelfTargetPin)
			{
				//self node (default if unconnected main target pin)
				FnTargetPin->MakeLinkTo( SelfTargetPin );
			}
			else
			{
				//main input
				CompilerContext.MovePinLinksToIntermediate( *MainTargetPin, *FnTargetPin );
			}
			CurrentTargetPin = FnTargetPin;
			CompilerContext.MovePinLinksToIntermediate(*MainParametersPin, *FnParamsPin);

			//hook up exec in
			CompilerContext.MovePinLinksToIntermediate(*MainExecPin, *FnExecPin);

			//continue exec from this fn
			CurrentThenPin = FnThenPin;
		}
	}

	//final exec chain hookup
	if (CurrentThenPin)
	{		
		//hook up last in function node chain to trigger rebuild
		//find a function to handle triggering a rebuild
		UFunction* RebuildFunction = FindRebuildFunction();
		//create intermediate node to call the rebuild fn
		UK2Node_CallFunction* CallRebuildFn = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
		CallRebuildFn->SetFromFunction(RebuildFunction);
		CallRebuildFn->AllocateDefaultPins();
		CompilerContext.MessageLog.NotifyIntermediateObjectCreation(CallRebuildFn, this);
		//rebuild fn pins
		UEdGraphPin* FnExecPin = CallRebuildFn->GetExecPin();
		UEdGraphPin* FnThenPin = CallRebuildFn->GetThenPin();
		UEdGraphPin* FnTargetPin = CallRebuildFn->FindPinChecked(FApparanceEntityFunctionPinNames::EntityPinName);

		//hook up exec
		CurrentThenPin->MakeLinkTo(FnExecPin);

		//hook up target
		if (CurrentTargetPin)
		{
			CompilerContext.CopyPinLinksToIntermediate(*CurrentTargetPin, *FnTargetPin);
		}
		else
		{
			CompilerContext.MovePinLinksToIntermediate(*CurrentTargetPin, *FnTargetPin);
		}	

		//then on to final top level then pin
		CompilerContext.MovePinLinksToIntermediate(*MainThenPin, *FnThenPin);
	}
	else
	{
		//no internal functionality required (no procedure selected or nothing connected), passthrough (right call???)
		//CompilerContext.MovePinLinksToIntermediate(*MainThenPin, *MainExecPin);
		//CompilerContext.MovePinLinksToIntermediate(*MainExecPin, *MainThenPin);
		MainThenPin->MakeLinkTo(MainExecPin);
	}
	
}

#undef LOCTEXT_NAMESPACE
