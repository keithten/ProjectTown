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

//unreal
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/Blueprint.h"
#include "K2Node.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "ApparanceBaseNode.generated.h"

namespace FApparanceEntityParameterVariants
{
	enum Type
	{
		None = 0,
		Structure,
		Array,
	};
}


UCLASS(abstract)
class APPARANCEUNREALSCRIPTING_API UApparanceBaseNode : public UK2Node
{
	GENERATED_BODY()
protected:
	//cached proc name, when we can resolve from ProcedureID
	FString	ProcedureName;
	
public:
	//the procedure to gather inputs for, or none for parameter structure input
	UPROPERTY(EditAnywhere,Category="Apparance")
	int ProcedureID;

	//any pins that are disabled, not to be included in the operation of this node
	//NOTE: inverted so that simple/empty default is all enabled
	UPROPERTY()
	TArray<int> DisabledPins;
#if 0
	//remember types of list pins when they were attached
	UPROPERTY()
	TArray<FListPinType> ListPinTypes;
#endif
	//editing
	bool IsPinEnabled(int id) const;
	void SetPinEnabled(int id, bool enable);
		
	//~ Begin UObject Interface
	//virtual void Serialize(FArchive& Ar) override;	
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& e) override;
	virtual void PostLoad() override;
	//~ End UObject Interface
	
	//~ Begin UEdGraphNode Interface
	virtual void AllocateDefaultPins() override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetMenuCategory() const override;
	//virtual void GetNodeContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const override;
	//virtual bool IncludeParentNodeContextMenu() const override { return true; }
	//virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	virtual void PostPlacedNewNode();
	//~ End UEdGraphNode Interface
	
	//~ Begin K2Node Interface
	virtual void NotifyPinConnectionListChanged(UEdGraphPin* Pin) override;
	//virtual bool IsNodePure() const override { return bIsPureGet; }
	//virtual void ReallocatePinsDuringReconstruction(TArray<UEdGraphPin*>& OldPins) override;
	virtual bool ShouldShowNodeProperties() const override { return true; }
	//virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual void PostReconstructNode() override;
	//~ End K2Node Interface


	//conversion	
	static Apparance::Parameter::Type ApparanceTypeFromPinType( FEdGraphPinType pin_type, FApparanceEntityParameterVariants::Type& OutTypeVariant );
	static FName PinTypeFromApparanceType( Apparance::Parameter::Type param_type, FName& sub_type, UScriptStruct*& sub_type_out );
	
protected:
	
	//access helpers
	UEdGraphPin* GetThenPin() const;
	const Apparance::ProcedureSpec* GetProcSpec();
	
	//operation
	void SetProcedureID( int proc_id );
	void SyncInputs();
	void CollectParameterPins( TMap<int,UEdGraphPin*>& param_pins, bool enabled_only=false );
	
	//derived specialisation	
	virtual FText GetNodeTooltipText() const { check(false); /*must implement*/ return FText(); }
	virtual FText GetNoProcSubtitleText() const { return FText(); }
	virtual FText GetNodeTitleText() const { check(false); /*must implement*/ return FText(); }
	virtual EEdGraphPinDirection GetDyanmicPinDirection() const { return EGPD_Input; }
	virtual void CustomAllocateDefaultPins( const Apparance::ProcedureSpec* spec ) { check(false); /*must implement*/ }
	virtual void CustomExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, const Apparance::ProcedureSpec* spec ) { check(false); /*must implement*/ }

private:

	void UpdateListPinTypes();
	void UpdateListPinType(UEdGraphPin* Pin);
};
