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
#include "ApparanceMakeParametersNode.generated.h"


UCLASS()
class APPARANCEUNREALSCRIPTING_API UApparanceMakeParametersNode : public UApparanceBaseNode
{
	GENERATED_BODY()
	
public:
	
private:

	//specialist	
	UEdGraphPin* GetOutputPin() const;	
	UFunction* FindParameterCreateFunction() const;
	UFunction* FindParameterSetterFunctionByType(Apparance::Parameter::Type param_type, FApparanceEntityParameterVariants::Type variant);
	
protected:

	//derived specialisation	
	virtual FText GetNodeTooltipText() const override;
	virtual FText GetNoProcSubtitleText() const override;
	virtual FText GetNodeTitleText() const override;
	virtual FText GetMenuCategory() const override;
	virtual void CustomAllocateDefaultPins( const Apparance::ProcedureSpec* spec ) override;
	virtual void CustomExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, const Apparance::ProcedureSpec* spec ) override;
};
