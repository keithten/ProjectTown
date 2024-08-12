//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#pragma once

// unreal
#include "GameFramework/Actor.h"

// apparance
#include "Apparance.h"

// module

// auto (last)
#include "IProceduralObject.generated.h"



// what a parameter collection is used for
//
enum class EParametersRole
{
	None = 0,
		//parameter source
		Specification = 0x0001,	//from procedure definition inputs
		Blueprint     = 0x0002,	//from blueprint default entity object
		Instance      = 0x0004,	//from entity in scene
		Creation      = 0x0100,	//result of above (script read)
		Override      = 0x0008,	//from script/bp overrides (script write)
		Generation    = 0x0200,	//result of above for actual proc gen
		//composite sources
		ForCreation   = 0x0007, //all contributions to creation params
		ForGeneration = 0x0108, //all contributions to generation params
		ForUpdate     = 0x000F,	//direct re-collect, not using creation stage
		ForDetails    = 0x000F,	//direct influences, not using creation stage
		All           = 0xFFFF
};


// abstraction of some object that has procedural input state, defined by: 
//   1. a specific procedure (ID)
//   2. a set of input parameters
// in addition to entities themselves, entity presets also follow this pattern
UINTERFACE()
class APPARANCEUNREAL_API UProceduralObject : public UInterface
{
	GENERATED_BODY()
};
class IProceduralObject
{
	GENERATED_BODY()
public:
	//query
	virtual int IPO_GetProcedureID() const { check(false); return 0; }
	virtual void IPO_GetParameters(TArray<const Apparance::IParameterCollection*>& parameter_cascade, EParametersRole needed_parameters ) const {check(false);}
	virtual bool IPO_IsParameterOverridden(Apparance::ValueID input_id) const { check(false); return false; }
	virtual bool IPO_IsParameterDefault(Apparance::ValueID param_id) const { check(false); return false; }
	virtual FText IPO_GenerateParameterLabel( Apparance::ValueID input_id ) const { check(false); return FText(); }
	virtual FText IPO_GenerateParameterTooltip(Apparance::ValueID input_id) const { check(false); return FText(); }

	//editing
	virtual void IPO_Modify() {check(false);}	//alias for UObject::Modify
	virtual void IPO_SetProcedureID(int proc_id) {check(false);}
	virtual Apparance::IParameterCollection* IPO_BeginEditingParameters() { check(false); return nullptr;  }
	virtual const Apparance::IParameterCollection* IPO_EndEditingParameters(bool dont_pack_interactive_edits) { check(false); return nullptr; }
	virtual void IPO_PostParameterEdit( const Apparance::IParameterCollection* params, Apparance::ValueID changed_param) { check(false); }
	virtual void IPO_InteractiveParameterEdit( const Apparance::IParameterCollection* params, Apparance::ValueID changed_param ) { check(false); }
};

