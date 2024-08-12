//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

// main
#include "ApparanceEntityPreset.h"


// unreal
//#include "Components/BoxComponent.h"
//#include "Components/BillboardComponent.h"
//#include "Components/DecalComponent.h"
//#include "UObject/ConstructorHelpers.h"
//#include "Engine/Texture2D.h"
//#include "ProceduralMeshComponent.h"
//#include "Engine/World.h"
//#include "Components/InstancedStaticMeshComponent.h"

// module
#include "ApparanceUnreal.h"
#include "AssetDatabase.h"
#include "Utility/ApparanceConversion.h"
#include "Utility/ApparanceUtility.h"
#include "ApparanceEntity.h"

#define LOCTEXT_NAMESPACE "ApparanceUnreal"

//////////////////////////////////////////////////////////////////////////
// UApparanceEntityPreset


#if WITH_EDITOR
void UApparanceEntityPreset::PostEditChangeProperty(struct FPropertyChangedEvent& e)
{
	FName PropertyName = (e.Property != NULL) ? e.Property->GetFName() : NAME_None;
	FName MemberPropertyName = (e.Property != NULL) ? e.MemberProperty->GetFName() : NAME_None;
	
	//handle prop effects
	bool notify = false;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AApparanceEntity, ProcedureID))
	{
		//proc type changed
		notify = true;
	}
	else if(PropertyName == GET_MEMBER_NAME_CHECKED(AApparanceEntity, bUseMeshInstancing))
	{
		//entity config changed
		notify = true;
	}

	//pass on
	if(notify)
	{
		//changes to base actor should cause instances to update
		//(if change is on parameter we are interested in)
		FApparanceUnrealModule::GetModule()->Editor_NotifyEntityProcedureTypeChanged( ProcedureID );
		FApparanceUnrealModule::GetModule()->Editor_NotifyProcedureDefaultsChange( ProcedureID, GetParameters(), Apparance::InvalidID );
	}
	
	Super::PostEditChangeProperty(e);	
}
#endif

// get just the entity parameters collection
//
const Apparance::IParameterCollection* UApparanceEntityPreset::GetParameters() const
{
	//init on demand (class default doesn't get load/create path)
	if(!PresetParameters.IsValid())
	{
		UnpackParameters();
	}
	return PresetParameters.Get();
}

// update parameter object from persisted bin blob
//
void UApparanceEntityPreset::UnpackParameters() const
{
	if(!PresetParameters.IsValid())
	{
		PresetParameters = MakeShareable( FApparanceUnrealModule::GetEngine()->CreateParameterCollection() );
	}
	
	//fill in
	int num_bytes = PresetParameterData.Num();
	unsigned char* pbytes = PresetParameterData.GetData();
	PresetParameters->SetBytes( num_bytes, pbytes );
	
	//validate against spec
	auto* plibrary = FApparanceUnrealModule::GetEngine()->GetLibrary();
	if(plibrary)
	{
		const Apparance::ProcedureSpec* proc_spec = plibrary->FindProcedureSpec( (Apparance::ProcedureID)ProcedureID );
		const Apparance::IParameterCollection* spec_params = proc_spec ? proc_spec->Inputs : nullptr;
		if(spec_params)
		{
			Apparance::IParameterCollection* ent_params = PresetParameters.Get();
			if(ent_params)
			{
				ent_params->Sanitise( spec_params );
			}
		}
	}
}

// store parameter object into bin blob for persistence
//
void UApparanceEntityPreset::PackParameters()
{
	//update
	if(PresetParameters.IsValid())
	{
		//get
		int byte_count = 0;
		const unsigned char* pdata = PresetParameters->GetBytes( byte_count );
		
		//store
		PresetParameterData.SetNum( byte_count );
		unsigned char* pstore = PresetParameterData.GetData();
		FMemory::Memcpy( (void*)pstore, (void*)pdata, byte_count );
	}
	else
	{
		//none
		PresetParameterData.Empty();
	}
}

// find parameters needed for our current procedures inputs from it's specification
//
const Apparance::IParameterCollection* UApparanceEntityPreset::GetSpecParameters() const
{
	//checks
	auto* pengine = FApparanceUnrealModule::GetEngine();
	auto* plibrary = pengine?pengine->GetLibrary():nullptr;
	if (!plibrary)
	{
		return nullptr;
	}
	
	//procedure inputs
	Apparance::ProcedureID proc_id = (Apparance::ProcedureID)ProcedureID;
	const Apparance::ProcedureSpec* proc_spec = plibrary->FindProcedureSpec( proc_id );
	return proc_spec?proc_spec->Inputs:nullptr;
}


//~ Begin IProceduralObject Interface
void UApparanceEntityPreset::IPO_GetParameters(TArray<const Apparance::IParameterCollection*>& parameter_cascade, EParametersRole needed_parameters) const
{
	//this preset contains what would be instance parameters
	if(((int)needed_parameters&(int)EParametersRole::Instance) != 0)
	{
		auto ppreset_parameters = GetParameters();
		if (ppreset_parameters)
		{
			parameter_cascade.Add(ppreset_parameters);
		}
	}
	
	//procedure spec defaults
	if (((int)needed_parameters&(int)EParametersRole::Specification) != 0)
	{
		const Apparance::ProcedureSpec* proc_spec = FApparanceUnrealModule::GetEngine()->GetLibrary()->FindProcedureSpec((Apparance::ProcedureID)ProcedureID);
		const Apparance::IParameterCollection* spec_params = proc_spec?proc_spec->Inputs:nullptr;
		if (spec_params)
		{
			parameter_cascade.Add(spec_params);
		}
	}
}
bool UApparanceEntityPreset::IPO_IsParameterOverridden(Apparance::ValueID input_id) const
{
	//presets can't be overridden by script
	return false;
}
bool UApparanceEntityPreset::IPO_IsParameterDefault(Apparance::ValueID param_id) const
{
	//check our actual params
	const Apparance::IParameterCollection* params = GetParameters();
	params->BeginAccess();
	bool is_set = params->FindType(param_id) != Apparance::Parameter::None;
	params->EndAccess();
	if (is_set)
	{
		return false;
	}
	
	//ok, will fall back to default then
	return true;	
}

FText UApparanceEntityPreset::IPO_GenerateParameterLabel( Apparance::ValueID input_id ) const
{
	FString s;

	//type/name
	const Apparance::IParameterCollection* spec_params = GetSpecParameters();
	if(spec_params)
	{
		//get name
		spec_params->BeginAccess();
		UTF8CHAR* raw_name = (UTF8CHAR*)spec_params->FindName( input_id );
		s = APPARANCE_UTF8_TO_TCHAR( raw_name );
		spec_params->EndAccess();

		//extract name part (remove desc/meta)
		int isep = FApparanceUtility::NextMetadataSeparator( s, 0 );
		if(isep != -1)
		{
			s = s.Left( isep );
		}
	}
	if(!s.IsEmpty())
	{
		return FText::FromString( s );
	}

	//empty
	return FText::Format( LOCTEXT( "UnnamedParameterLabelFormat", "Parameter #{0}" ), input_id );
}
FText UApparanceEntityPreset::IPO_GenerateParameterTooltip(Apparance::ValueID input_id) const
{
	FString s;
	
	//type/name
	const Apparance::IParameterCollection* spec_params = GetSpecParameters();
	if (spec_params)
	{
		//name info
		spec_params->BeginAccess();
		FString raw_name = APPARANCE_UTF8_TO_TCHAR( (UTF8CHAR*)spec_params->FindName( input_id ) );
		Apparance::Parameter::Type param_type = spec_params->FindType( input_id );
		spec_params->EndAccess();
		
		//extract description
		FString desc = FApparanceUtility::GetMetadataSection(raw_name,1);
		if (!desc.IsEmpty())
		{
			s = desc;
		}
		else
		{
			s.Append(ApparanceParameterTypeName(param_type));
			s.Append(TEXT(" parameter"));
		}
	}
	else
	{
		s.Append( LOCTEXT("EntityParamUnknown", "Unknown entity parameter!").ToString() );
	}
	
	//state
	s.Append(TEXT("\n"));
	if (IPO_IsParameterDefault(input_id))
	{
		//default
		s.Append(LOCTEXT("EntityParamIsDefault", "Parameter is set to its default value").ToString());
	}
	else
	{
		//specified
		s.Append(LOCTEXT("EntityParamIsEdited", "Parameter has been edited").ToString());
	}
	
	return FText::FromString(s);
}
void UApparanceEntityPreset::IPO_SetProcedureID(int proc_id)
{
	ProcedureID = proc_id;
}
Apparance::IParameterCollection* UApparanceEntityPreset::IPO_BeginEditingParameters()
{
	//init on demand (class default doesn't get load/create path)
	if(!PresetParameters.IsValid())
	{
		UnpackParameters();
	}
	
	Apparance::IParameterCollection* preset_params = PresetParameters.Get();
	preset_params->BeginEdit();
	return preset_params;
}
const Apparance::IParameterCollection* UApparanceEntityPreset::IPO_EndEditingParameters(bool suppress_pack)
{
	PresetParameters->EndEdit();
	
	if(!suppress_pack)
	{
		//ensure stored away so that if a BP is recompiled we don't lose them (placed proc objects)
		PackParameters();
	}
	
	return PresetParameters.Get();
}
// our own internal editing support notification
//
void UApparanceEntityPreset::IPO_PostParameterEdit(const Apparance::IParameterCollection* params, Apparance::ValueID changed_param)
{
	//move changes to persistent store
	PackParameters();
	
	//changes to preset should cause instances to update
	//(if change is on parameter we are interested in)
	FApparanceUnrealModule::GetModule()->Editor_NotifyProcedureDefaultsChange(ProcedureID, params, changed_param);
}

// our own internal editing support, during interactive edits
// don't commit, just do what's needed to update the visuals
//
void UApparanceEntityPreset::IPO_InteractiveParameterEdit(const Apparance::IParameterCollection* params, Apparance::ValueID changed_param)
{
	//changes to preset should cause instances to update
	//(if change is on parameter we are interested in)
	FApparanceUnrealModule::GetModule()->Editor_NotifyProcedureDefaultsChange(ProcedureID, params, changed_param);
}
//~ End IProceduralObject Interface

#undef LOCTEXT_NAMESPACE
