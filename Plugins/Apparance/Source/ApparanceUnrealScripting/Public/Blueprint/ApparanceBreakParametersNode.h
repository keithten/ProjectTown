//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#pragma once

//module
#include "Apparance.h"
#include "ApparanceBaseNode.h"

//unreal
#include "ApparanceBreakParametersNode.generated.h"


UCLASS()
class APPARANCEUNREALSCRIPTING_API UApparanceBreakParametersNode : public UApparanceBaseNode
{
	GENERATED_BODY()
	
public:
	
private:

	//specialist	
	UEdGraphPin* GetInputPin() const;	
	UFunction* FindParameterGetterFunctionByType(Apparance::Parameter::Type param_type, FApparanceEntityParameterVariants::Type variant);
protected:

	//derived specialisation	
	virtual FText GetNodeTooltipText() const override;
	virtual FText GetNoProcSubtitleText() const override;
	virtual FText GetNodeTitleText() const override;
	virtual FText GetMenuCategory() const override;
	virtual EEdGraphPinDirection GetDyanmicPinDirection() const override { return EGPD_Output; }
	virtual void CustomAllocateDefaultPins( const Apparance::ProcedureSpec* spec ) override;
	virtual void CustomExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, const Apparance::ProcedureSpec* spec ) override;
};
