//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#include "Blueprint/ApparanceBreakParametersNode.h"
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
	static const FName InputPinName("Parameters");
}  	
namespace FApparanceParamsParameterGetterFunctionNames
{
	static const FName IntGetterName(GET_FUNCTION_NAME_CHECKED(UApparanceBlueprintLibrary, GetParamsIntParameter));
	static const FName FloatGetterName(GET_FUNCTION_NAME_CHECKED(UApparanceBlueprintLibrary, GetParamsFloatParameter));
	static const FName BoolGetterName(GET_FUNCTION_NAME_CHECKED(UApparanceBlueprintLibrary, GetParamsBoolParameter));
	static const FName StringGetterName(GET_FUNCTION_NAME_CHECKED(UApparanceBlueprintLibrary, GetParamsStringParameter));
	static const FName ColourGetterName(GET_FUNCTION_NAME_CHECKED(UApparanceBlueprintLibrary, GetParamsColourParameter));
	static const FName Vector3GetterName(GET_FUNCTION_NAME_CHECKED(UApparanceBlueprintLibrary, GetParamsVector3Parameter));
	static const FName FrameGetterName(GET_FUNCTION_NAME_CHECKED(UApparanceBlueprintLibrary, GetParamsFrameParameter));

	static const FName ListStructGetterName(GET_FUNCTION_NAME_CHECKED(UApparanceBlueprintLibrary, GetParamsListParameter_Struct));
	static const FName ListArrayGetterName(GET_FUNCTION_NAME_CHECKED(UApparanceBlueprintLibrary, GetParamsListParameter_Array));
}
namespace FApparanceParamsParameterGetterPinNames
{
	static const FName InputPinName("Params");
	static const FName ParameterIDPinName("ParameterID");
	static const FName OutputPinName_Old1("OutValue");
	static const FName OutputPinName("Value");
}

// basic node properties
//
FText UApparanceBreakParametersNode::GetNodeTitleText() const
{
	return LOCTEXT("ApparanceBreakParametersNode_Title", "Break-out Parameters");
}
FText UApparanceBreakParametersNode::GetNoProcSubtitleText() const
{
	return LOCTEXT("ApparanceBreakParametersNode_NoProc", "Select procedure parameters in Details");
}
FText UApparanceBreakParametersNode::GetNodeTooltipText() const
{
	return LOCTEXT("ApparanceBreakParametersNode_Tooltip", "Breaks out a procedure parameters stucture to individual parameter outputs");
}
FText UApparanceBreakParametersNode::GetMenuCategory() const
{
	return LOCTEXT("ApparanceBreakParametersNode_MenuCategory", "Apparance|Parameters");
}


// custom pins
//
void UApparanceBreakParametersNode::CustomAllocateDefaultPins( const Apparance::ProcedureSpec* spec )
{
	UEdGraphNode::FCreatePinParams PinParams;
	CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Struct, TBaseStructure<FApparanceParameters>::Get(), FApparanceParametersNodePinNames::InputPinName, PinParams);
}

// pin access helpers
//
UEdGraphPin* UApparanceBreakParametersNode::GetInputPin() const
{
	UEdGraphPin* Pin = FindPin(FApparanceParametersNodePinNames::InputPinName);
	check(Pin == NULL || Pin->Direction == EGPD_Input);
	return Pin;
}

// find getter for particular parameter type
//
UFunction* UApparanceBreakParametersNode::FindParameterGetterFunctionByType( Apparance::Parameter::Type param_type, FApparanceEntityParameterVariants::Type variant )
{
	FName FunctionName = NAME_None;
	switch(param_type)
	{
		case Apparance::Parameter::Integer:
			FunctionName = FApparanceParamsParameterGetterFunctionNames::IntGetterName;
			break;
		case Apparance::Parameter::Float:
			FunctionName = FApparanceParamsParameterGetterFunctionNames::FloatGetterName;
			break;
		case Apparance::Parameter::Bool:
			FunctionName = FApparanceParamsParameterGetterFunctionNames::BoolGetterName;
			break;
		case Apparance::Parameter::String:
			FunctionName = FApparanceParamsParameterGetterFunctionNames::StringGetterName;
			break;
		case Apparance::Parameter::Colour:
			FunctionName = FApparanceParamsParameterGetterFunctionNames::ColourGetterName;
			break;
		case Apparance::Parameter::Vector3:
			FunctionName = FApparanceParamsParameterGetterFunctionNames::Vector3GetterName;
			break;
		case Apparance::Parameter::Frame:
			FunctionName = FApparanceParamsParameterGetterFunctionNames::FrameGetterName;
			break;
		case Apparance::Parameter::List:
			if (variant == FApparanceEntityParameterVariants::Structure)
			{
				FunctionName = FApparanceParamsParameterGetterFunctionNames::ListStructGetterName;
			}
			if (variant == FApparanceEntityParameterVariants::Array)
			{
				FunctionName = FApparanceParamsParameterGetterFunctionNames::ListArrayGetterName;
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

// runtime node operation functionality hookup
//
void UApparanceBreakParametersNode::CustomExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, const Apparance::ProcedureSpec* spec )
{
	//input pins : exec (execution triggered)
	UEdGraphPin* MainExecPin = GetExecPin();
	//output pins : parameter outputs
	TMap<int,UEdGraphPin*> param_pins;
	CollectParameterPins( param_pins );
	//output pins : then (execution continues)
	UEdGraphPin* MainThenPin = FindPin(UEdGraphSchema_K2::PN_Then);	
	//input pins : output (parameter list struct)
	UEdGraphPin* MainInputPin = GetInputPin();
	
	//each parameter pin needs a function call node chained together to fully set up the parameter output from the params struct
	UEdGraphPin* CurrentThenPin = nullptr;
	UEdGraphPin* CurrentInputPin = nullptr;
	for(TMap<int,UEdGraphPin*>::TIterator It(param_pins); It; ++It)
	{	
		bool first_time = CurrentThenPin==nullptr;
		
		//id of param we are hooking up
		Apparance::ValueID ParameterID = (Apparance::ValueID)It.Key();
		UEdGraphPin* ParameterPin = It.Value();
		
		//skip unconnected pins, don't need to waste time invoking getter for unused value
		if (!ParameterPin->HasAnyConnections())
		{
			continue;
		}
		
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
		UFunction* ParamGetFunction = FindParameterGetterFunctionByType( ParameterPinType, ParameterPinTypeVariant );
		if (!ParamGetFunction)
		{
			if (ParameterPinType != Apparance::Parameter::List) //just skip unconnected list params
			{				
				CompilerContext.MessageLog.Error(*FString::Format(*LOCTEXT("MissingParametersParamGetter", "Failed to find function to get parameter list parameter of type {0}.").ToString(), { ParameterPinType }), this);
			}
			continue;
		}

		//create intermediate node to call the setter
		UK2Node_CallFunction* CallGetterFn = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
		CallGetterFn->SetFromFunction(ParamGetFunction);
		CallGetterFn->AllocateDefaultPins();
		CompilerContext.MessageLog.NotifyIntermediateObjectCreation(CallGetterFn, this);

		//getter fn pins
		UEdGraphPin* FnExecPin = CallGetterFn->GetExecPin();
		UEdGraphPin* FnThenPin = CallGetterFn->GetThenPin();
		UEdGraphPin* FnInputPin = CallGetterFn->FindPinChecked( FApparanceParamsParameterGetterPinNames::InputPinName );
		UEdGraphPin* FnParamPin = CallGetterFn->FindPinChecked( FApparanceParamsParameterGetterPinNames::ParameterIDPinName );
		UEdGraphPin* FnReturnPin = CallGetterFn->GetReturnValuePin();
		if (!FnReturnPin)
		{
			//try ref param for output instead (e.g. used for array/struct get)
			FnReturnPin = CallGetterFn->FindPinChecked( FApparanceParamsParameterGetterPinNames::OutputPinName );
			if (!FnReturnPin)
			{
				//legacy name
				FnReturnPin = CallGetterFn->FindPinChecked(FApparanceParamsParameterGetterPinNames::OutputPinName_Old1);				
			}
		}
		if (!FnReturnPin)
		{
			CompilerContext.MessageLog.Error(*FString::Format(*LOCTEXT("MissingParamGetterReturn", "Failed to find return or output pin on function {0} for getting parameter list parameter of type {1}.").ToString(), { *ParamGetFunction->GetName(), ParameterPinType }), this);
			return;
		}
		
		//hook up function inputs to previous outputs
		if (first_time)
		{
			//hook up external pin
			CompilerContext.MovePinLinksToIntermediate(*MainExecPin, *FnExecPin);
		}
		else
		{
			//chain previous then pin
			CurrentThenPin->MakeLinkTo(FnExecPin);
		}

		//hook up parameter collection struct input (same for all getters)
		if (first_time)
		{
			//take connection
			CompilerContext.MovePinLinksToIntermediate(*MainInputPin, *FnInputPin);
		}
		else
		{
			//copy connection
			CompilerContext.CopyPinLinksToIntermediate(*CurrentInputPin, *FnInputPin);
		}
		CurrentInputPin = FnInputPin; //copy from prev input pin along to all fn input pins

		//hook up parameter output
		if (FnReturnPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard)
		{
			//need to propagate type to generic input pin on list set fn node
			FnReturnPin->PinType = ParameterPin->PinType;
		}
		CompilerContext.MovePinLinksToIntermediate( *ParameterPin, *FnReturnPin );

		//set param id function input
		FnParamPin->DefaultValue = FString::FromInt( ParameterID );

		//move on to function outputs
		CurrentThenPin = FnThenPin;
	}

	//hook up last in function node chain to then pin
	if (CurrentThenPin)
	{
		CompilerContext.MovePinLinksToIntermediate(*MainThenPin, *CurrentThenPin);
	}
	else
	{
		//no connected pins, no generated internals, so bypass
		CompilerContext.MovePinLinksToIntermediate(*MainThenPin, *MainExecPin);	//right way to connect external pins?
	}
}

#undef LOCTEXT_NAMESPACE
