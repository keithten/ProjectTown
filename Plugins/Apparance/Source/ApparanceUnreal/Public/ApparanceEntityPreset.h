//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#pragma once

// unreal

// apparance

// module
#include "IProceduralObject.h"

// auto (last)
#include "ApparanceEntityPreset.generated.h"



// Entity preset definition
//
UCLASS(config = Engine)
class APPARANCEUNREAL_API UApparanceEntityPreset
	: public UObject
	, public IProceduralObject
{
	GENERATED_BODY()

	//transient
	mutable TSharedPtr<Apparance::IParameterCollection>	PresetParameters;

public:
	// persistent properties
	UPROPERTY(EditAnywhere, BlueprintReadOnly,Category=Apparance,meta=(Tooltip = "Procedure we will build our entity from"))
	int						ProcedureID;

	// inputs persisted as BLOB and have custom editing UI (since they are dynamic)
	UPROPERTY(BlueprintReadOnly,Category=Apparance) //prevent replacement
	mutable TArray<uint8>	PresetParameterData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,Category=Apparance,AdvancedDisplay)
	bool 					bUseMeshInstancing;
	
public:

	//access
	const Apparance::IParameterCollection* GetParameters() const;

#if WITH_EDITOR
	void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent);
#endif

	//~ Begin IProceduralObject Interface
	//query
	virtual int IPO_GetProcedureID() const  override { return ProcedureID; }
	virtual void IPO_GetParameters(TArray<const Apparance::IParameterCollection*>& parameter_cascade, EParametersRole needed_parameters) const override;
	virtual bool IPO_IsParameterOverridden(Apparance::ValueID input_id) const;
	virtual bool IPO_IsParameterDefault(Apparance::ValueID param_id) const;
	virtual FText IPO_GenerateParameterLabel( Apparance::ValueID input_id ) const;
	virtual FText IPO_GenerateParameterTooltip(Apparance::ValueID input_id) const;
	//editing
	virtual void IPO_Modify() override { Modify(); }
	virtual void IPO_SetProcedureID(int proc_id) override;
	virtual Apparance::IParameterCollection* IPO_BeginEditingParameters() override;
	virtual const Apparance::IParameterCollection* IPO_EndEditingParameters(bool dont_pack_interactive_edits) override;
	virtual void IPO_PostParameterEdit(const Apparance::IParameterCollection* params, Apparance::ValueID changed_param) override;
	virtual void IPO_InteractiveParameterEdit( const Apparance::IParameterCollection* params, Apparance::ValueID changed_param ) override;
	//~ End IProceduralObject Interface

private:
	const Apparance::IParameterCollection* GetSpecParameters() const;
	void UnpackParameters() const;
	void PackParameters();
	
};

