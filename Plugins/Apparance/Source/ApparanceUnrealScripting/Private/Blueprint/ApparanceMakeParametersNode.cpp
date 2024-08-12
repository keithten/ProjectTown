//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#include "Blueprint/ApparanceMakeParametersNode.h"
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
#include "Engine/SimpleConstructionScript.h" //USimpleConstructionScript

#define LOCTEXT_NAMESPACE "ApparanceUnreal"

// name constants
//
namespace FApparanceParametersNodePinNames
{
	static const FName OutputPinName("Parameters");
}  	
namespace FApparanceParamsParameterSetterFunctionNames
{
	static const FName IntSetterName(GET_FUNCTION_NAME_CHECKED(UApparanceBlueprintLibrary, SetParamsIntParameter));
	static const FName FloatSetterName(GET_FUNCTION_NAME_CHECKED(UApparanceBlueprintLibrary, SetParamsFloatParameter));
	static const FName BoolSetterName(GET_FUNCTION_NAME_CHECKED(UApparanceBlueprintLibrary, SetParamsBoolParameter));
	static const FName StringSetterName(GET_FUNCTION_NAME_CHECKED(UApparanceBlueprintLibrary, SetParamsStringParameter));
	static const FName ColourSetterName(GET_FUNCTION_NAME_CHECKED(UApparanceBlueprintLibrary, SetParamsColourParameter));
	static const FName Vector3SetterName(GET_FUNCTION_NAME_CHECKED(UApparanceBlueprintLibrary, SetParamsVector3Parameter));
	static const FName FrameSetterName(GET_FUNCTION_NAME_CHECKED(UApparanceBlueprintLibrary, SetParamsFrameParameter));
	static const FName ListStructSetterName(GET_FUNCTION_NAME_CHECKED(UApparanceBlueprintLibrary, SetParamsListParameter_Struct));
	static const FName ListArraySetterName(GET_FUNCTION_NAME_CHECKED(UApparanceBlueprintLibrary, SetParamsListParameter_Array));
}
namespace FApparanceParamsParameterSetterPinNames
{
	static const FName InputPinName("Params");
	static const FName ParameterIDPinName("ParameterID");
	static const FName ValuePinName("Value");
}
namespace FApparanceParameterUtilityFunctionNames
{
	static const FName MakeParametersName(GET_FUNCTION_NAME_CHECKED(UApparanceBlueprintLibrary, MakeParameters));
}

// basic node properties
//
FText UApparanceMakeParametersNode::GetNodeTitleText() const
{
	return LOCTEXT("ApparanceMakeParametersNode_Title", "Make Parameters");
}
FText UApparanceMakeParametersNode::GetNoProcSubtitleText() const
{
	return LOCTEXT("ApparanceMakeParametersNode_NoProc", "Select procedure parameters in Details");
}
FText UApparanceMakeParametersNode::GetNodeTooltipText() const
{
	return LOCTEXT("ApparanceMakeParametersNode_Tooltip", "Makes a procedure parameters stucture from individual parameter inputs");
}
FText UApparanceMakeParametersNode::GetMenuCategory() const
{
	return LOCTEXT("ApparanceMakeParametersNode_MenuCategory", "Apparance|Parameters");
}

// custom pins
//
void UApparanceMakeParametersNode::CustomAllocateDefaultPins( const Apparance::ProcedureSpec* spec )
{
	UEdGraphNode::FCreatePinParams PinParams;
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Struct, TBaseStructure<FApparanceParameters>::Get(), FApparanceParametersNodePinNames::OutputPinName, PinParams);
}

// pin access helpers
//
UEdGraphPin* UApparanceMakeParametersNode::GetOutputPin() const
{
	UEdGraphPin* Pin = FindPin(FApparanceParametersNodePinNames::OutputPinName);
	check(Pin == NULL || Pin->Direction == EGPD_Output);
	return Pin;
}

// find setter for particular parameter type
//
UFunction* UApparanceMakeParametersNode::FindParameterSetterFunctionByType( Apparance::Parameter::Type param_type, FApparanceEntityParameterVariants::Type variant )
{
	FName FunctionName = NAME_None;
	switch(param_type)
	{
		case Apparance::Parameter::Integer:
			FunctionName = FApparanceParamsParameterSetterFunctionNames::IntSetterName;
			break;
		case Apparance::Parameter::Float:
			FunctionName = FApparanceParamsParameterSetterFunctionNames::FloatSetterName;
			break;
		case Apparance::Parameter::Bool:
			FunctionName = FApparanceParamsParameterSetterFunctionNames::BoolSetterName;
			break;
		case Apparance::Parameter::String:
			FunctionName = FApparanceParamsParameterSetterFunctionNames::StringSetterName;
			break;
		case Apparance::Parameter::Colour:
			FunctionName = FApparanceParamsParameterSetterFunctionNames::ColourSetterName;
			break;
		case Apparance::Parameter::Vector3:
			FunctionName = FApparanceParamsParameterSetterFunctionNames::Vector3SetterName;
			break;
		case Apparance::Parameter::Frame:
			FunctionName = FApparanceParamsParameterSetterFunctionNames::FrameSetterName;
			break;
		case Apparance::Parameter::List:
			if (variant == FApparanceEntityParameterVariants::Structure)
			{
				FunctionName = FApparanceParamsParameterSetterFunctionNames::ListStructSetterName;
			}
			if (variant == FApparanceEntityParameterVariants::Array)
			{
				FunctionName = FApparanceParamsParameterSetterFunctionNames::ListArraySetterName;
			}
			break;

		//TODO: Colour, Vector3, Frame
	}
		
	//resolve function	
	if (!FunctionName.IsNone())
	{
		UClass* LibraryClass = UApparanceBlueprintLibrary::StaticClass();
		return LibraryClass->FindFunctionByName( FunctionName );
	}
	return nullptr;
}

// creation of an empty parameter collection
//
UFunction* UApparanceMakeParametersNode::FindParameterCreateFunction() const
{
	UClass* LibraryClass = UApparanceBlueprintLibrary::StaticClass();
	return LibraryClass->FindFunctionByName( FApparanceParameterUtilityFunctionNames::MakeParametersName );
}


// runtime node operation functionality hookup
//
void UApparanceMakeParametersNode::CustomExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, const Apparance::ProcedureSpec* spec )
{
	//input pins : exec (execution triggered)
	UEdGraphPin* MainExecPin = GetExecPin();
	//input pins : parameter inputs
	TMap<int,UEdGraphPin*> param_pins;
	CollectParameterPins( param_pins );
	//output pins : then (execution continues)
	UEdGraphPin* MainThenPin = FindPin(UEdGraphSchema_K2::PN_Then);	
	//output pins : output (parameter list struct)
	UEdGraphPin* MainOutputPin = GetOutputPin();

	//internal parameter source
	UFunction* ParamCreateFunction = FindParameterCreateFunction();
	UK2Node_CallFunction* CallCreateFn = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	CallCreateFn->SetFromFunction(ParamCreateFunction);
	CallCreateFn->AllocateDefaultPins();
	CompilerContext.MessageLog.NotifyIntermediateObjectCreation(CallCreateFn, this);
	//setter fn pins
	UEdGraphPin* CreateFnExecPin = CallCreateFn->GetExecPin();
	UEdGraphPin* CreateFnThenPin = CallCreateFn->GetThenPin();
	UEdGraphPin* CreateFnResultPin = CallCreateFn->GetReturnValuePin();
	//hook up create fn to exec first
	CompilerContext.MovePinLinksToIntermediate( *MainExecPin, *CreateFnExecPin );
	
	//each parameter pin needs a function call node chained together to fully set up the parameter input of the entity
	UEdGraphPin* CurrentThenPin = CreateFnThenPin;
	for(TMap<int,UEdGraphPin*>::TIterator It(param_pins); It; ++It)	
	{
		bool first_time = CurrentThenPin==nullptr;

		//id of param we are hooking up
		Apparance::ValueID ParameterID = (Apparance::ValueID)It.Key();
		UEdGraphPin* ParameterPin = It.Value();	

		//ensure parameter type is correct
		Apparance::Parameter::Type ParameterSpecType = spec->Inputs->FindType( ParameterID );
		FApparanceEntityParameterVariants::Type ParameterPinTypeVariant = FApparanceEntityParameterVariants::None;
		Apparance::Parameter::Type ParameterPinType = ApparanceTypeFromPinType( ParameterPin->PinType, /*out*/ParameterPinTypeVariant);
		if(ParameterPinType!=ParameterSpecType)
		{
			CompilerContext.MessageLog.Error( *FString::Format( *LOCTEXT("ParamTypeMismatch", "Type of input parameter {0} has changed to {1}, expected {2}.").ToString(), { (int)ParameterID, ParameterPinType, ParameterSpecType } ), this );
			return;
		}

		//find a function to handle setting this parameter on the entity
		UFunction* ParamSetFunction = FindParameterSetterFunctionByType( ParameterPinType, ParameterPinTypeVariant);
		if (!ParamSetFunction)
		{
			if (ParameterPinType != Apparance::Parameter::List) //just skip unconnected list params
			{
				CompilerContext.MessageLog.Error(*FString::Format(*LOCTEXT("MissingParametersParamSetter", "Failed to find function to set parameter list parameter of type {0}.").ToString(), { ParameterPinType }), this);
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
		UEdGraphPin* FnInputPin = CallSetterFn->FindPinChecked( FApparanceParamsParameterSetterPinNames::InputPinName );
		UEdGraphPin* FnParamPin = CallSetterFn->FindPinChecked( FApparanceParamsParameterSetterPinNames::ParameterIDPinName );
		UEdGraphPin* FnValuePin = CallSetterFn->FindPinChecked( FApparanceParamsParameterSetterPinNames::ValuePinName );
		
		//hook up function inputs to previous outputs
		CurrentThenPin->MakeLinkTo( FnExecPin );

		//hook up parameter collection struct input (same for all setters)
		CreateFnResultPin->MakeLinkTo( FnInputPin );

		//hook up parameter input
		if (FnValuePin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard)
		{
			//need to propagate type to generic input pin on list set fn node
			FnValuePin->PinType = ParameterPin->PinType;
		}
		CompilerContext.MovePinLinksToIntermediate( *ParameterPin, *FnValuePin );

		//set param id function input
		FnParamPin->DefaultValue = FString::FromInt( ParameterID );

		//move on to function outputs
		CurrentThenPin = FnThenPin;		
	}

	//hook up last in function node chain to then pin
	CompilerContext.MovePinLinksToIntermediate( *MainThenPin, *CurrentThenPin );

	//hook up parameter collection output
	CompilerContext.MovePinLinksToIntermediate( *MainOutputPin, *CreateFnResultPin );
}

#undef LOCTEXT_NAMESPACE
