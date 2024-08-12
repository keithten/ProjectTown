//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#pragma once

//unreal
#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "IDetailCustomization.h"

//module
#include "RemoteEditing.h"
#include "EntityProperties.h"
#include "Blueprint/ApparanceRebuildNode.h"
#include "Blueprint/ApparanceMakeParametersNode.h"
#include "Blueprint/ApparanceBreakParametersNode.h"

//apparance
#include "Apparance.h"
	
/// <summary>
/// base node customisation (shared logic)
/// </summary>
class FNodePropertyCustomisation_Base : public FEntityPropertyCustomisation
{
	UApparanceBaseNode* Node;

public:
	FNodePropertyCustomisation_Base() : Node(nullptr) {}
	
	//setup
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
	
	//FEntityPropertyCustomisation

protected:

	//pin enable/disable
	virtual bool IsPinEnabled(int param_id) const override;
	virtual void SetPinEnable(int param_id, bool enable, bool editing_transaction) override;
};

/// <summary>
/// blueprint node property customisation - ApparanceRebuildNode
/// </summary>
class FNodePropertyCustomisation_Rebuild : public FNodePropertyCustomisation_Base
{
	
public:
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable( new FNodePropertyCustomisation_Rebuild );
	}	

protected:
	//we want proc-select dropdown
	virtual bool ShowProcedureSelection() override { return true; }
	//but not actual property editing
	virtual bool ShowProcedureParameters() override { return false; }
	//allow pin enable/disable
	virtual bool ShowPinEnableUI() override { return true; }	
	//the procedure id
	virtual FName GetProcedureIDPropertyName() override
	{
		return GET_MEMBER_NAME_CHECKED(UApparanceRebuildNode, ProcedureID);
	}
	virtual UClass* GetProcedureIDPropertyClass() override
	{
		return UApparanceBaseNode::StaticClass();
	}
	
	
private:
};


/// <summary>
/// blueprint node property customisation - ApparanceParametersNode
/// </summary>
class FNodePropertyCustomisation_MakeParameters : public FNodePropertyCustomisation_Base
{
	
public:
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable( new FNodePropertyCustomisation_MakeParameters );
	}	
	
protected:
	//we want proc-select dropdown
	virtual bool ShowProcedureSelection() override { return true; }
	//but not actual property editing
	virtual bool ShowProcedureParameters() override { return false; }
	//allow pin enable/disable
	virtual bool ShowPinEnableUI() override { return true; }	
	//the procedure id
	virtual FName GetProcedureIDPropertyName() override
	{
		return GET_MEMBER_NAME_CHECKED(UApparanceMakeParametersNode, ProcedureID);
	}
	virtual UClass* GetProcedureIDPropertyClass() override
	{
		return UApparanceBaseNode::StaticClass();
	}
	
private:
};


/// <summary>
/// blueprint node property customisation - ApparanceParametersNode
/// </summary>
class FNodePropertyCustomisation_BreakParameters : public FNodePropertyCustomisation_Base
{
	
public:
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable( new FNodePropertyCustomisation_BreakParameters );
	}	
	
protected:
	//we want proc-select dropdown
	virtual bool ShowProcedureSelection() override { return true; }
	//but not actual property editing
	virtual bool ShowProcedureParameters() override { return false; }
	//allow pin enable/disable
	virtual bool ShowPinEnableUI() override { return true; }	
	//the procedure id
	virtual FName GetProcedureIDPropertyName() override
	{
		return GET_MEMBER_NAME_CHECKED(UApparanceBreakParametersNode, ProcedureID);
	}
	virtual UClass* GetProcedureIDPropertyClass() override
	{
		return UApparanceBaseNode::StaticClass();
	}
	
private:
};
