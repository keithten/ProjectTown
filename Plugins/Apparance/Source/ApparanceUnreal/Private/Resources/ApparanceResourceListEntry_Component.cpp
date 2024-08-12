//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

// main
#include "ResourceList/ApparanceResourceListEntry_Component.h"


// unreal
//#include "Components/BoxComponent.h"
//#include "Components/BillboardComponent.h"
//#include "Components/DecalComponent.h"
//#include "UObject/ConstructorHelpers.h"
//#include "Engine/Texture2D.h"
//#include "ProceduralMeshComponent.h"
//#include "Engine/World.h"
//#include "Components/InstancedStaticMeshComponent.h"
#include "Misc/EngineVersionComparison.h"

// module
#include "ApparanceUnreal.h"
#include "AssetDatabase.h"
#include "Apparance.h"
#include "Utility/ApparanceConversion.h"


#define LOCTEXT_NAMESPACE "ApparanceUnreal"

//////////////////////////////////////////////////////////////////////////
// global component reflection
TMap<const UClass*, FApparanceComponentInfo*> FApparanceComponentInventory::Components;

// is this property a type we can work with?
//
static EApparanceParameterType EvaluatePropertyType(const FProperty* pprop)
{
	//evaluate property
	auto prop_type = pprop->GetCPPType();
	if(prop_type == TEXT( "FName" ))
	{
		return EApparanceParameterType::String;
	}
	if (prop_type == TEXT("FText"))
	{
		return EApparanceParameterType::String;
	}
	else if (prop_type == TEXT("FString"))
	{
		return EApparanceParameterType::String;
	}
	else if (prop_type == TEXT("FLinearColor"))
	{
		return EApparanceParameterType::Colour;
	}
	else if(prop_type == TEXT( "FColor" ))
	{
		return EApparanceParameterType::Colour;
	}
	else if (prop_type == TEXT("float"))
	{
		return EApparanceParameterType::Float;
	}
	else if(prop_type == TEXT( "bool" ))
	{
		return EApparanceParameterType::Bool;
	}
	else if (prop_type == TEXT("int32"))
	{
		return EApparanceParameterType::Integer;
	}
	else if (prop_type == TEXT("uint32"))
	{
		return EApparanceParameterType::Integer;
	}	
	else if(prop_type == TEXT( "FVector" ))
	{
		return EApparanceParameterType::Vector;
	}
	else if(prop_type == TEXT( "FBox" ))
	{
		return EApparanceParameterType::Frame;
	}
	else if(prop_type == TEXT( "FTransform" ))
	{
		return EApparanceParameterType::Frame;
	}
	else if(prop_type == TEXT( "FMatrix" ))
	{
		return EApparanceParameterType::Frame;
	}
	else if(prop_type == TEXT( "FRotator" ))
	{
		return EApparanceParameterType::Frame;
	}
	//enum? (often stored as byte)
	const FByteProperty* byte_prop = CastField<FByteProperty>( pprop );
	if(byte_prop)
	{
		return EApparanceParameterType::Integer;
	}

	//nope
	return EApparanceParameterType::None;
}

// describe a function to give a bit more information in action menu
//
static FText BuildFunctionDescription( FApparanceMethodInfo* pmi )
{
	FString s;
	const UEnum* type_enum = StaticEnum<EApparanceParameterType>();
	for (int i = 0; i < pmi->Arguments.Num(); i++)
	{		
		FText type_name = type_enum->GetDisplayNameTextByValue( (int64)pmi->Arguments[i]->Type );
		FName arg_name = pmi->Arguments[i]->Name;
		if (!s.IsEmpty())
		{
			s.Append(TEXT(",\n"));
		}
		s.Appendf(TEXT("%s %s"), *type_name.ToString(), *arg_name.ToString());
	}

	return FText::FromString(s);
}

// gather component info:
// 1. Properties that we can set
// 2. Functions that we can call
//
FApparanceComponentInfo::FApparanceComponentInfo(const UClass* component_type)
{
	//properties
	FProperty* pprop = component_type->PropertyLink;
	while(pprop)
	{
		//what type?
		EApparanceParameterType prop_type = EvaluatePropertyType(pprop);
		
		//one we can use?
		if (prop_type != EApparanceParameterType::None)
		{
			AddProperty(pprop, prop_type, pprop->GetFName());
		}

		//next
		pprop = pprop->PropertyLinkNext;
	}
	Properties.Sort([](FApparancePropertyInfo& LHS, FApparancePropertyInfo& RHS)  { return LHS.Name.Compare( RHS.Name )<0; } );

	//functions
	TArray<FName> function_names;
	const UClass* c = component_type;
	for(TFieldIterator<UFunction> PropIt( c ); PropIt; ++PropIt)
	{
		UFunction* pfunc = *PropIt;
		function_names.Add( pfunc->GetFName() );
	}
	function_names.Sort(FNameLexicalLess());
	for (int i = 0; i < function_names.Num(); i++)
	{
		UFunction* Function = component_type->FindFunctionByName(function_names[i]);
		check(Function);

		//skip undesirables, no const, no out, no events/delegates, no internals, no dev
		if(Function->HasAnyFunctionFlags( FUNC_Event | FUNC_Static | FUNC_MulticastDelegate | FUNC_Private | FUNC_Protected | FUNC_Delegate | FUNC_HasOutParms | FUNC_BlueprintEvent | FUNC_EditorOnly | FUNC_Const ))
		{
			continue;
		}

		//assess args for suitability
		bool all_args_usable = true;
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			FProperty* arg = *It;

			//param flags check
			if (arg->HasAnyPropertyFlags(CPF_OutParm|CPF_ReturnParm)
				|| !arg->HasAllPropertyFlags(CPF_Parm))
			{
				all_args_usable = false;
				break;
			}

			//param type check
			EApparanceParameterType arg_type = EvaluatePropertyType(arg);
			if(arg_type==EApparanceParameterType::None)
			{
				all_args_usable = false;
				break;
			}
		}
		if (!all_args_usable)
		{
			continue;
		}

		//record method
		FApparanceMethodInfo* pmi = new FApparanceMethodInfo();
		pmi->Name = function_names[i];
		pmi->Function = Function;
		Methods.Add(pmi);
		
		//method args
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			FProperty* arg = *It;
			
			FApparancePropertyInfo* parginfo = new FApparancePropertyInfo();
			parginfo->Name = arg->GetFName();
			parginfo->Type = EvaluatePropertyType(arg);
			parginfo->Property = arg;
			pmi->Arguments.Add(parginfo);
		}
		
		//description based on args
		pmi->Description = BuildFunctionDescription( pmi );		
	}
}

//////////////////////////////////////////////////////////////////////////
// UApparanceResourceListEntry_Component

FName UApparanceResourceListEntry_Component::GetIconName( bool is_open )
{
	return StaticIconName( is_open );
}

FName UApparanceResourceListEntry_Component::StaticIconName( bool is_open )
{
#if UE_VERSION_OLDER_THAN(5,0,0)
	return FName( "Level.ColorIcon40x" );
#else
	return FName( "GraphEditor.State_16x" );
#endif
}

FText UApparanceResourceListEntry_Component::GetAssetTypeName() const
{
	return UApparanceResourceListEntry_Component::StaticAssetTypeName();
}

FText UApparanceResourceListEntry_Component::StaticAssetTypeName()
{
	return LOCTEXT( "ComponentAssetTypeName", "Component");
}

// what is the actual component class selected?
//
const UClass* UApparanceResourceListEntry_Component::GetTemplateClass() const
{
	switch (TemplateType)
	{
		case EComponentTemplateType::Component:
		{
			//the class of the template instance provided
			if (ComponentTemplate)
			{
				return ComponentTemplate->GetClass();
			}
			break;
		}
		case EComponentTemplateType::Blueprint:
		{
			//the component the blueprint generates
			if (BlueprintTemplate)
			{
				return BlueprintTemplate->ParentClass.Get();
			}
			break;
		}
	}

	//not known/chosen
	return nullptr;
}

///////////////// Parameter accessor helpers ////////////////////

//int param access
float UApparanceResourceListEntry_Component::GetIntParameter( const Apparance::IParameterCollection* placement_parameters, int parameter_index )
{
	//param info
	int value = 0;
	Apparance::Parameter::Type param_type = placement_parameters->GetType(parameter_index);
	if (param_type == Apparance::Parameter::Integer)
	{
		placement_parameters->GetInteger(parameter_index, &value);
	}
	return value;
}
//float param access
float UApparanceResourceListEntry_Component::GetFloatParameter( const Apparance::IParameterCollection* placement_parameters, int parameter_index )
{
	//param info
	float value = 0;
	Apparance::Parameter::Type param_type = placement_parameters->GetType(parameter_index);
	if (param_type == Apparance::Parameter::Float)
	{
		placement_parameters->GetFloat(parameter_index, &value);
	}
	return value;
}
//bool param access
bool UApparanceResourceListEntry_Component::GetBoolParameter( const Apparance::IParameterCollection* placement_parameters, int parameter_index )
{
	//param info
	bool value = 0;
	Apparance::Parameter::Type param_type = placement_parameters->GetType(parameter_index);
	if (param_type == Apparance::Parameter::Bool)
	{
		placement_parameters->GetBool(parameter_index, &value);
	}
	return value;
}
//colour param access
FLinearColor UApparanceResourceListEntry_Component::GetColourParameter( const Apparance::IParameterCollection* placement_parameters, int parameter_index, bool allow_coersion )
{
	//param info
	Apparance::Parameter::Type param_type = placement_parameters->GetType(parameter_index);
	if (param_type == Apparance::Parameter::Colour)
	{
		Apparance::Colour c;
		placement_parameters->GetColour(parameter_index, &c);
		return UNREALCOLOR_FROM_APPARANCECOLOUR(c);
	}
	else if (allow_coersion && param_type == Apparance::Parameter::Vector3)
	{
		Apparance::Vector3 v;
		placement_parameters->GetVector3(parameter_index, &v);
		FLinearColor c(v.X, v.Y, v.Z, 1);
		return UNREALCOLOR_FROM_APPARANCECOLOUR(c);		
	}
	return FLinearColor::White;
}
//vector param access
Apparance::Vector3 UApparanceResourceListEntry_Component::GetVector3Parameter( const Apparance::IParameterCollection* placement_parameters, int parameter_index, bool allow_coersion )
{
	//param info
	Apparance::Parameter::Type param_type = placement_parameters->GetType(parameter_index);
	if (param_type == Apparance::Parameter::Vector3)
	{
		Apparance::Vector3 v;
		placement_parameters->GetVector3(parameter_index, &v);
		return v;
	}
	if (allow_coersion && param_type == Apparance::Parameter::Frame)
	{
		Apparance::Frame f;
		placement_parameters->GetFrame(parameter_index, &f);
		
		//coerce from frame (size)
		return f.Size;
	}
	return Apparance::Vector3();
}
//vector param access
FVector UApparanceResourceListEntry_Component::GetFVectorParameter( const Apparance::IParameterCollection* placement_parameters, int parameter_index, bool allow_coersion )
{
	//param info
	Apparance::Parameter::Type param_type = placement_parameters->GetType(parameter_index);
	if (param_type == Apparance::Parameter::Vector3)
	{
		Apparance::Vector3 v;
		placement_parameters->GetVector3(parameter_index, &v);
		return UNREALSPACE_FROM_APPARANCESPACE(v);
	}
	if (allow_coersion && param_type == Apparance::Parameter::Frame)
	{
		Apparance::Frame f;
		placement_parameters->GetFrame(parameter_index, &f);
		
		//coerce from frame (size)
		return UNREALSPACE_FROM_APPARANCESPACE(f.Size);
	}
	return FVector::ZeroVector;
}
//string param access
FString UApparanceResourceListEntry_Component::GetStringParameter( const Apparance::IParameterCollection* placement_parameters, int parameter_index )
{
	//param info
	Apparance::Parameter::Type param_type = placement_parameters->GetType(parameter_index);

	FString value;
	if (param_type == Apparance::Parameter::String)
	{
		int num_chars = 0;
		TCHAR* pbuffer = nullptr;
		if (placement_parameters->GetString( parameter_index, 0, nullptr, &num_chars))
		{
			value = FString::ChrN(num_chars, TCHAR(' '));
			pbuffer = value.GetCharArray().GetData();
			placement_parameters->GetString( parameter_index, num_chars, pbuffer);
		}
	}
	return value;
}
//transform param access
FTransform UApparanceResourceListEntry_Component::GetTransformParameter( const Apparance::IParameterCollection* placement_parameters, int parameter_index, bool allow_coersion )
{
	//param info
	Apparance::Parameter::Type param_type = placement_parameters->GetType(parameter_index);
	if (param_type == Apparance::Parameter::Frame)
	{
		Apparance::Frame f;
		placement_parameters->GetFrame(parameter_index, &f);
		
		//build transform for frame as matrix
		FMatrix frame_orientation;
		FVector frame_offset;
		FVector frame_scale;
		UnrealTransformsFromApparanceFrame(f, nullptr, nullptr, &frame_orientation, &frame_offset, &frame_scale);
		
		//revert from world size to scaling factor
		float revert_scale = APPARANCESCALE_FROM_UNREALSCALE( 1.0f );
		frame_scale *= revert_scale;
		
		//convert that to transform
		FTransform value( frame_orientation.Rotator(), frame_offset, frame_scale );
		return value;
	}
	else if (allow_coersion && param_type == Apparance::Parameter::Vector3)
	{
		Apparance::Vector3 v;
		placement_parameters->GetVector3(parameter_index, &v);

		//coerce from vector (size)
		FTransform t = FTransform::Identity;
		t.SetScale3D( UNREALSPACE_FROM_APPARANCESPACE(v) );
		return t;
	}
	return FTransform::Identity;
}
//frame param access
bool UApparanceResourceListEntry_Component::GetFrameParameter( const Apparance::IParameterCollection* placement_parameters, int parameter_index, Apparance::Frame& frame_out )
{
	//param info
	Apparance::Parameter::Type param_type = placement_parameters->GetType(parameter_index);
	if (param_type == Apparance::Parameter::Frame)
	{
		placement_parameters->GetFrame(parameter_index, &frame_out);
		return true;
	}
	return false;
}
//texture param access
class UTexture* UApparanceResourceListEntry_Component::GetTextureParameter(const TArray<Apparance::TextureID>& textures, int texture_index)
{
	//which texture?
	if (texture_index < textures.Num())
	{
		Apparance::TextureID texture_id = textures[texture_index];

		//look up in asset db
		FAssetDatabase* asset_db = FApparanceUnrealModule::GetAssetDatabase();
		if (asset_db)
		{
			//fetch
			class UTexture* ptexture = nullptr;
			asset_db->GetTexture( texture_id, ptexture ); //use returned fallback if not resolved
			return ptexture;
		}
	}

	return nullptr;
}



#if WITH_EDITOR
// property has changed
//
void UApparanceResourceListEntry_Component::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty( PropertyChangedEvent );
}
#endif

// loaded, init transient
//
void UApparanceResourceListEntry_Component::PostLoad()
{
	Super::PostLoad();

	CheckUpgrade();
	InitMemberMetadata();
}

// convert any old format data to new format
//
void UApparanceResourceListEntry_Component::CheckUpgrade()
{
	//any old data?
	if (ParameterMap.Num() > 0 || FunctionCalls.Num()>0)
	{
		const FApparanceComponentInfo* pcomp_info = FApparanceComponentInventory::GetComponentInfo(GetTemplateClass());
		if (!pcomp_info)
		{
			return;
		}

		//convert params
		for (int i = 0; i < ParameterMap.Num(); i++)
		{
			const FComponentPlacementMapping& cpm = ParameterMap[i];
			if (cpm.Action == EComponentPlacementMappingAction::SetProperty)
			{
				//old
				int iparameterindex = i;
				FString propname = cpm.Name;
				//new
				int iparameterid = iparameterindex + 1;	//hack, since we probably don't have id's mapped out yet, so we will make them up and any not found will be attempted as indices when used
				//create
				UApparanceComponentSetupAction_SetProperty* set_prop = NewObject<UApparanceComponentSetupAction_SetProperty>(this, UApparanceComponentSetupAction_SetProperty::StaticClass(), NAME_None, RF_Transactional);
				set_prop->Name = FName(*propname);			
				//resolve property info
				if (pcomp_info)
				{
					const FApparancePropertyInfo* ppi = pcomp_info->FindProperty( FName( *propname ) );
					if (ppi)
					{
						set_prop->SetProperty(ppi);

						//finish upgrade
						set_prop->SetParameter( iparameterid );
						//add
						ComponentSetup.Add( set_prop );
					}
				}
			}
		}

		//convert fn calls
		for (int i = 0; i < FunctionCalls.Num(); i++)
		{
			//old
			const FComponentPlacementCall& cpc = FunctionCalls[i];
			const FName func_name = cpc.Function;
			const TArray<FComponentPlacementCallParameter>& old_args = cpc.Parameters;
			//new
			//create
			UApparanceComponentSetupAction_CallMethod* call_fn = NewObject<UApparanceComponentSetupAction_CallMethod>(this, UApparanceComponentSetupAction_CallMethod::StaticClass(), NAME_None, RF_Transactional);
			call_fn->Name = func_name;
			const FApparanceMethodInfo* pmi = pcomp_info->FindMethod(func_name);
			if (pmi)
			{
				call_fn->SetMethod( pmi );
			}
			for (int a = 0; a < old_args.Num(); a++)
			{
				UApparanceValueSource* existing_arg = (a < call_fn->Arguments.Num())?call_fn->Arguments[a]:nullptr;
				const FComponentPlacementCallParameter& old_arg = old_args[a];
				if (old_arg.bUseParameter)
				{
					//parameter
					int iparameterindex = old_arg.ParameterIndex;
					//new
					int iparameterid = iparameterindex + 1;	//hack, since we probably don't have id's mapped out yet, so we will make them up and any not found will be attempted as indices when used
					//add
					if (existing_arg)
					{
						existing_arg->SetParameter(iparameterid);
					}
					else
					{
						call_fn->AddParameterArgument(iparameterid);
					}
				}
				else
				{
					//constant
					switch (old_arg.ConstantType)//0=none,1=int,2=float,3=bool,4=vector,5=colour,6=Transform
					{
						default:
						case 0://none
							if (existing_arg)
							{
								//already defaulted
							}
							else
							{
								call_fn->AddDefaultArgument();
							}
							break;
						case 1://int
							if (existing_arg)
							{
								existing_arg->SetConstant(old_arg.ConstantInt);
							}
							else
							{
								call_fn->AddConstantArgument(old_arg.ConstantInt);
							}
							break;
						case 2://float
							if (existing_arg)
							{
								existing_arg->SetConstant(old_arg.ConstantFloat);
							}
							else
							{
								call_fn->AddConstantArgument(old_arg.ConstantFloat);
							}
							break;
						case 3://bool
							if (existing_arg)
							{
								existing_arg->SetConstant(old_arg.ConstantBool);
							}
							else
							{
								call_fn->AddConstantArgument(old_arg.ConstantBool);
							}
							break;
						case 4://vector
							if (existing_arg)
							{
								existing_arg->SetConstant(old_arg.ConstantVector);
							}
							else
							{
								call_fn->AddConstantArgument(old_arg.ConstantVector);
							}
							break;
						case 5://colour
							if (existing_arg)
							{
								existing_arg->SetConstant(old_arg.ConstantColour);
							}
							else
							{
								call_fn->AddConstantArgument(old_arg.ConstantColour);
							}
							break;
						case 6://frame
							if (existing_arg)
							{
								existing_arg->SetConstant(old_arg.ConstantTransform);
							}
							else
							{
								call_fn->AddConstantArgument(old_arg.ConstantTransform);
							}
							break;
					}
				}
			}
			//add
			ComponentSetup.Add(call_fn);			
		}	

		//remove old
		ParameterMap.Empty();
		FunctionCalls.Empty();
	}
}

// populate all non-persistent metadata about methods and functions
//
void UApparanceResourceListEntry_Component::InitMemberMetadata()
{
	//clean dead entries (TODO: where from?)
	ComponentSetup.Remove( nullptr );

	//init
	auto template_class = GetTemplateClass();
	if(template_class)
	{
		const FApparanceComponentInfo* pcomp_info = FApparanceComponentInventory::GetComponentInfo( template_class );
		if(pcomp_info)
		{
			for(int i = 0; i < ComponentSetup.Num(); i++)
			{
				ComponentSetup[i]->InitMemberMetadata( pcomp_info );
			}
		}
	}
}

// attempt to apply a set of placement parameters to a component using
// the user supplied parameter mapping that comes with this resource entry
//
void UApparanceResourceListEntry_Component::ApplyParameters( Apparance::IParameterCollection* placement_parameters, UActorComponent* pcomponent ) const
{
	placement_parameters->BeginAccess();

	//apply actions
	for (int i = 0; i < ComponentSetup.Num(); i++)
	{
		ComponentSetup[i]->Apply( pcomponent, placement_parameters, this );
	}

	placement_parameters->EndAccess();
}

// add a property setting action
//
UApparanceComponentSetupAction_SetProperty* UApparanceResourceListEntry_Component::AddAction_SetProperty(const FApparancePropertyInfo* prop_info)
{
	UApparanceComponentSetupAction_SetProperty* set_prop = NewObject<UApparanceComponentSetupAction_SetProperty>(this, UApparanceComponentSetupAction_SetProperty::StaticClass(), NAME_None, RF_Transactional);
	set_prop->SetProperty(prop_info);
	ComponentSetup.Add(set_prop);
	return set_prop;
}

// add a method call action
//
UApparanceComponentSetupAction_CallMethod* UApparanceResourceListEntry_Component::AddAction_CallMethod(const FApparanceMethodInfo* method_info)
{
	UApparanceComponentSetupAction_CallMethod* call_method = NewObject<UApparanceComponentSetupAction_CallMethod>(this, UApparanceComponentSetupAction_CallMethod::StaticClass(), NAME_None, RF_Transactional);
	call_method->SetMethod(method_info);
	ComponentSetup.Add(call_method);
	return call_method;
}


////////////////////////////////// UApparanceComponentSetupAction_SetProperty //////////////////////////////////

// attempt to set a property on an object
//
bool UApparanceComponentSetupAction_SetProperty::Apply(UObject* ptarget, Apparance::IParameterCollection* placement_parameters, const UApparanceResourceListEntry_Component* pcomponententry) const
{
	//locate property
	check(Property);
	check(Property->Property);
	const UClass* pclass = ptarget->GetClass();
	if (!pclass->IsChildOf( Property->Property->GetOwnerClass() ))
	{
		UE_LOG(LogApparance, Error, TEXT("Property class mismatch in property set action, expected %s, got %s"), *Property->Property->GetOwnerClass()->GetName(), *pclass->GetName());
		return false;
	}
	const FProperty* pprop = Property->Property;
	if (!pprop)
	{
		return false;
	}

	//type based set
	auto prop_type = pprop->GetCPPType();
	if (prop_type == TEXT("FName"))
	{
		FString value = Value->GetStringValue(placement_parameters, pcomponententry);
		FName* pfield = pprop->ContainerPtrToValuePtr<FName>(ptarget);
		if (pfield)
		{
			*pfield = FName(value);
			return true;
		}
	}
	else if (prop_type == TEXT("FText"))
	{
		FString value = Value->GetStringValue(placement_parameters, pcomponententry);
		FText* pfield = pprop->ContainerPtrToValuePtr<FText>(ptarget);
		if (pfield)
		{
			*pfield = FText::FromString(value);
			return true;
		}
	}
	else if (prop_type == TEXT("FString"))
	{
		FString value = Value->GetStringValue(placement_parameters, pcomponententry);
		FString* pfield = pprop->ContainerPtrToValuePtr<FString>(ptarget);
		if (pfield)
		{
			*pfield = value;
			return true;
		}
	}
	else if (prop_type == TEXT("FLinearColor"))
	{
		FLinearColor value = Value->GetColourValue(placement_parameters, pcomponententry);
		FLinearColor* pfield = pprop->ContainerPtrToValuePtr<FLinearColor>(ptarget);
		if (pfield)
		{
			*pfield = value;
			return true;
		}
	}
	else if(prop_type == TEXT( "FColor" ))
	{
		FLinearColor value = Value->GetColourValue(placement_parameters, pcomponententry);
		FColor* pfield = pprop->ContainerPtrToValuePtr<FColor>( ptarget );
		if(pfield)
		{
			*pfield = value.ToFColor(false);
			return true;
		}
	}
	else if (prop_type == TEXT("float"))
	{
		float value = Value->GetFloatValue(placement_parameters, pcomponententry);
		float* pfield = pprop->ContainerPtrToValuePtr<float>(ptarget);
		if (pfield)
		{
			*pfield = value;
			return true;
		}
	}
	else if (prop_type == TEXT("int32"))
	{
		int value = Value->GetIntValue(placement_parameters, pcomponententry);
		int32* pfield = pprop->ContainerPtrToValuePtr<int32>(ptarget);
		if (pfield)
		{
			*pfield = (int32)value;
			return true;
		}
	}
	else if (prop_type == TEXT("uint32"))
	{
		int value = Value->GetIntValue(placement_parameters, pcomponententry);
		uint32* pfield = pprop->ContainerPtrToValuePtr<uint32>(ptarget);
		if (pfield)
		{
			*pfield = (uint32)value;
			return true;
		}
	}

	//enum? (often stored as byte)
	const FByteProperty* byte_prop = CastField<FByteProperty>( pprop );
	if(byte_prop)
	{
		int value = -1;
		const EApparanceParameterType param_type = Property->Type;
		UEnum* penum = byte_prop->GetIntPropertyEnum();
		if(penum)
		{
			//ooh, it's an enum, we can range check and use names
			if(param_type == EApparanceParameterType::Integer)
			{
				//set enum by index
				int index = Value->GetIntValue(placement_parameters, pcomponententry);
				int max_index = penum->NumEnums() - 2; //last one is always a _MAX enum entry
				if(index > max_index)
				{
					index = max_index;
				}
				int64 eval = penum->GetValueByIndex( index );
				if(eval != INDEX_NONE)
				{
					value = (int)eval;
				}
			}
			else if(param_type == EApparanceParameterType::String)
			{
				//set by name
				FString name = Value->GetStringValue(placement_parameters, pcomponententry);
				int64 eval = penum->GetValueByNameString( name );
				if(eval != INDEX_NONE)
				{
					value = (int)eval;
				}
			}
		}
		else
		{
			//just set byte from int
			if(param_type == EApparanceParameterType::Integer)
			{
				value = Value->GetIntValue(placement_parameters, pcomponententry);
			}
		}
		
		//apply byte value
		if(value != -1)
		{
			uint8* pfield = pprop->ContainerPtrToValuePtr<uint8>( ptarget );
			if(pfield)
			{
				*pfield = (uint8)value;
				return true;
			}
		}
	}

	return false;
}

////////////////////////////////// UApparanceComponentSetupAction_CallMethod //////////////////////////////////

// action summary
//
FString UApparanceComponentSetupAction_CallMethod::GetSourceSummary(UApparanceResourceListEntry_Component* pcomponent) const
{
	FString s = "(";
	for (int i = 0; i < Arguments.Num(); i++)
	{
		if (i != 0)
		{
			s.Append(TEXT(", "));
		}
		s.Append(Arguments[i]->DescribeValue( pcomponent ));
	}
	s.Append(")");
	return s;
}

// attempt to call a function on an object
//
bool UApparanceComponentSetupAction_CallMethod::Apply(UObject* target, Apparance::IParameterCollection* placement_parameters, const UApparanceResourceListEntry_Component* pcomponententry) const
{
	//checks
	if (!Method)
	{
		return false;
	}
	UClass* Class = target->GetClass();
	if (Class != Method->Function->GetOwnerClass())
	{
		//mismatch
		return false;
	}
	UFunction* Function = Method->Function;
	
	//allocate parameter stack image
	uint8* Parms = (uint8*)FMemory_Alloca(Function->ParmsSize);
	FMemory::Memzero( Parms, Function->ParmsSize );
	
	//construct any non-trivial parameter objects (e.g. FString)
	for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		FProperty* LocalProp = *It;
		checkSlow(LocalProp);
		if (!LocalProp->HasAnyPropertyFlags(CPF_ZeroConstructor))
		{
			LocalProp->InitializeValue_InContainer(Parms);
		}
	}
	
	//set any parameters that aren't default
	int NumParamsEvaluated = 0;
	int ParamIndex = 0;
	bool bFailed = false;
	for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & (CPF_Parm | CPF_ReturnParm)) == CPF_Parm; ++It, NumParamsEvaluated++)
	{
		//set this pointer (first parameter)
		if (NumParamsEvaluated == 0 && target)
		{
			FObjectPropertyBase* Op = CastField<FObjectPropertyBase>(*It);
			if( Op && target->IsA(Op->PropertyClass) )
			{
				// First parameter is implicit reference to object executing the command.
				Op->SetObjectPropertyValue(Op->ContainerPtrToValuePtr<uint8>(Parms), target);
				continue;
			}
		}
		
		//assign further parameters
		uint8* RawFunctionParameter = It->ContainerPtrToValuePtr<uint8>( Parms );
		
		//attempt to use incoming parameters if available
		if (ParamIndex < Arguments.Num())
		{
			//arg values
			const UApparanceValueSource* Argument = Arguments[ParamIndex];
			//apply
			if (!Argument->ApplyProperty(*It, RawFunctionParameter, placement_parameters, pcomponententry))
			{
				bFailed = true;
			}
		}
		
		//next parameter
		ParamIndex++;
	}
	
	//call function
	if (!bFailed)
	{
		target->ProcessEvent(Function, Parms);
	}
	
	//destroy any parameter objects (e.g. FString)
	//!!destructframe see also UObject::ProcessEvent
	for( TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It )
	{
		It->DestroyValue_InContainer(Parms);
	}
	
	return !bFailed;
}


////////////////////////////////// UApparanceValueSource //////////////////////////////////

// source value access
//
FString UApparanceValueSource::DescribeValue( UApparanceResourceListEntry_Asset* passet)
{
	switch (Source)
	{
		case EApparanceValueSource::Constant:
		{
			if (Constant)
			{
				return Constant->DescribeValue();
			}
			return TEXT("");
		}
		case EApparanceValueSource::Parameter:
		{
			if (passet)
			{
				FText name = passet->FindParameterNameText(Parameter);
				return name.ToString();
			}
			return FString::Format(TEXT("Parameter {0}"), { Parameter });
		}
	}

	//!?!
	return TEXT("");
}

// set a property from a source (whether parameter or constant)
//
bool UApparanceValueSource::ApplyProperty(FProperty* property, uint8* pdata, Apparance::IParameterCollection* placement_parameters, const UApparanceResourceListEntry_Component* pcomponententry) const
{
	FString type = property->GetCPPType();

	//attempt to apply
	if (type == TEXT("FName"))
	{
		FString value = GetStringValue(placement_parameters, pcomponententry);
		*((FName*)pdata) = FName(value);
	}
	else if (type == TEXT("FText"))
	{
		FString value = GetStringValue(placement_parameters, pcomponententry);
		*((FText*)pdata) = FText::FromString(value);
	}
	else if (type == TEXT("FString"))
	{
		FString value = GetStringValue(placement_parameters, pcomponententry);
		*((FString*)pdata) = value;
	}
	else if (type == TEXT("int32"))
	{
		int value = GetIntValue(placement_parameters, pcomponententry);
		*((int*)pdata) = value;
	}
	else if (type == TEXT("float"))
	{
		float value = GetFloatValue(placement_parameters, pcomponententry);
		*((float*)pdata) = value;
	}
	else if (type == TEXT("bool"))
	{
		bool value = GetBoolValue(placement_parameters, pcomponententry);
		*((bool*)pdata) = value;
	}
	else if (type == TEXT("FVector"))
	{
		FVector value = GetVectorValue(placement_parameters, pcomponententry);
		*((FVector*)pdata) = value;
	}
	else if(type == TEXT("FLinearColor"))
	{
		FVector vector = GetVectorValue( placement_parameters, pcomponententry );
		FLinearColor value( (float)vector.X, (float)vector.Y, (float)vector.Z );
		*((FLinearColor*)pdata) = value;
	}
	else if(type == TEXT("FColor"))
	{
		FVector vector = GetVectorValue( placement_parameters, pcomponententry );
		FLinearColor value( (float)vector.X, (float)vector.Y, (float)vector.Z );
		*((FColor*)pdata) = UNREALSRGB_FROM_APPARANCECOLOUR(value);
	}
	else if(type == TEXT("FTransform"))
	{
		FTransform value = GetTransformValue(placement_parameters, pcomponententry);
		*((FTransform*)pdata) = value;
	}
	else
	{
		UE_LOG(LogApparance, Warning, TEXT("Source unable to set property of type '%s'"), *type);
		return false;
	}

	return true;
}

// string value access
//
FString UApparanceValueSource::GetStringValue(Apparance::IParameterCollection* placement_parameters, const UApparanceResourceListEntry_Asset* passet) const
{
	if(placement_parameters)
	{
		if(Source == EApparanceValueSource::Parameter && Parameter != 0)
		{
			//access parameter value after checks
			int parameter_index;
			const FApparancePlacementParameter* param_info = passet->FindParameterInfo( Parameter, &parameter_index );
			if(param_info && param_info->Type == EApparanceParameterType::String)
			{
				return UApparanceResourceListEntry_Component::GetStringParameter( placement_parameters, parameter_index );
			}
		}
		else if(Source == EApparanceValueSource::Constant && Constant)
		{
			//access constant value
			UApparanceValueConstant_String* pconst = Cast<UApparanceValueConstant_String>( Constant );
			if(pconst)
			{
				return pconst->Value;
			}
		}
	}
	return TEXT("");
}

// colour value access
//
FLinearColor UApparanceValueSource::UApparanceValueSource::GetColourValue(Apparance::IParameterCollection* placement_parameters, const UApparanceResourceListEntry_Asset* passet) const
{
	if(placement_parameters)
	{
		if(Source == EApparanceValueSource::Parameter && Parameter != 0)
		{
			//access parameter value after checks
			int parameter_index;
			const FApparancePlacementParameter* param_info = passet->FindParameterInfo( Parameter, &parameter_index );
			if(param_info && (param_info->Type == EApparanceParameterType::Colour || param_info->Type == EApparanceParameterType::Vector))
			{
				return UApparanceResourceListEntry_Component::GetColourParameter( placement_parameters, parameter_index, true );
			}
		}
		else if(Source == EApparanceValueSource::Constant && Constant)
		{
			//access constant value
			UApparanceValueConstant_Colour* pconst = Cast<UApparanceValueConstant_Colour>( Constant );
			if(pconst)
			{
				return pconst->Value;
			}
		}
	}
	return FLinearColor::Black;
}
// int value access
//
int UApparanceValueSource::GetIntValue(Apparance::IParameterCollection* placement_parameters, const UApparanceResourceListEntry_Asset* passet) const
{
	if(placement_parameters)
	{
		if(Source == EApparanceValueSource::Parameter && Parameter != 0 && placement_parameters)
		{
			//access parameter value after checks
			int parameter_index;
			const FApparancePlacementParameter* param_info = passet->FindParameterInfo( Parameter, &parameter_index );
			if(param_info && param_info->Type == EApparanceParameterType::Integer)
			{
				return UApparanceResourceListEntry_Component::GetIntParameter( placement_parameters, parameter_index );
			}
		}
		else if(Source == EApparanceValueSource::Constant && Constant)
		{
			//access constant value
			UApparanceValueConstant_Integer* pconst = Cast<UApparanceValueConstant_Integer>( Constant );
			if(pconst)
			{
				return pconst->Value;
			}
		}
	}
	return 0;	
}

// float value access
//
float UApparanceValueSource::GetFloatValue(Apparance::IParameterCollection* placement_parameters, const UApparanceResourceListEntry_Asset* passet) const
{
	if(placement_parameters)
	{
		if(Source == EApparanceValueSource::Parameter && Parameter != 0 && placement_parameters)
		{
			//access parameter value after checks
			int parameter_index;
			const FApparancePlacementParameter* param_info = passet->FindParameterInfo( Parameter, &parameter_index );
			if(param_info && param_info->Type == EApparanceParameterType::Float)
			{
				return UApparanceResourceListEntry_Component::GetFloatParameter( placement_parameters, parameter_index );
			}
		}
		else if(Source == EApparanceValueSource::Constant && Constant)
		{
			//access constant value
			UApparanceValueConstant_Float* pconst = Cast<UApparanceValueConstant_Float>( Constant );
			if(pconst)
			{
				return pconst->Value;
			}
		}
	}
	return 0.0f;
}

// bool value access
//
bool UApparanceValueSource::GetBoolValue(Apparance::IParameterCollection* placement_parameters, const UApparanceResourceListEntry_Asset* passet) const
{
	if(placement_parameters)
	{
		if(Source == EApparanceValueSource::Parameter && Parameter != 0 && placement_parameters)
		{
			//access parameter value after checks
			int parameter_index;
			const FApparancePlacementParameter* param_info = passet->FindParameterInfo( Parameter, &parameter_index );
			if(param_info && param_info->Type == EApparanceParameterType::Bool)
			{
				return UApparanceResourceListEntry_Component::GetBoolParameter( placement_parameters, parameter_index );
			}
		}
		else if(Source == EApparanceValueSource::Constant && Constant)
		{
			//access constant value
			UApparanceValueConstant_Bool* pconst = Cast<UApparanceValueConstant_Bool>( Constant );
			if(pconst)
			{
				return pconst->Value;
			}
		}
	}
	return false;
}

// vector value access
//
FVector UApparanceValueSource::GetVectorValue(Apparance::IParameterCollection* placement_parameters, const UApparanceResourceListEntry_Asset* passet) const
{
	if(placement_parameters)
	{
		if(Source == EApparanceValueSource::Parameter && Parameter != 0 && placement_parameters)
		{
			//access parameter value after checks
			int parameter_index;
			const FApparancePlacementParameter* param_info = passet->FindParameterInfo( Parameter, &parameter_index );
			if(param_info && (param_info->Type == EApparanceParameterType::Vector || param_info->Type == EApparanceParameterType::Frame))
			{
				return UApparanceResourceListEntry_Component::GetFVectorParameter( placement_parameters, parameter_index, true );
			}
		}
		else if(Source == EApparanceValueSource::Constant && Constant)
		{
			//access constant value
			UApparanceValueConstant_Vector* pconstv = Cast<UApparanceValueConstant_Vector>( Constant );
			if(pconstv)
			{
				return pconstv->Value;
			}
			UApparanceValueConstant_Transform* pconstt = Cast<UApparanceValueConstant_Transform>( Constant );
			if(pconstt)
			{
				//use scale from transform constants
				return pconstt->Value.GetScale3D();
			}
		}
	}
	return FVector::ZeroVector;
}

// transform value access
//
FTransform UApparanceValueSource::GetTransformValue(Apparance::IParameterCollection* placement_parameters, const UApparanceResourceListEntry_Asset* passet) const
{
	if(placement_parameters)
	{
		if(Source == EApparanceValueSource::Parameter && Parameter != 0 && placement_parameters)
		{
			//access parameter value after checks
			int parameter_index;
			const FApparancePlacementParameter* param_info = passet->FindParameterInfo( Parameter, &parameter_index );
			if(param_info && (param_info->Type == EApparanceParameterType::Vector || param_info->Type == EApparanceParameterType::Frame))
			{
				return UApparanceResourceListEntry_Component::GetTransformParameter( placement_parameters, parameter_index, true );
			}
		}
		else if(Source == EApparanceValueSource::Constant && Constant)
		{
			//access constant value
			UApparanceValueConstant_Transform* pconstt = Cast<UApparanceValueConstant_Transform>( Constant );
			if(pconstt)
			{
				return pconstt->Value;
			}
			UApparanceValueConstant_Vector* pconstv = Cast<UApparanceValueConstant_Vector>( Constant );
			if(pconstv)
			{
				//just scaling transform from vector
				FTransform t = FTransform::Identity;
				t.SetScale3D( pconstv->Value );
				return t;
			}
		}
	}
	return FTransform::Identity;
}

// texture value access
//
class UTexture* UApparanceValueSource::GetTextureValue(const TArray<Apparance::TextureID>& textures, const UApparanceResourceListEntry_Asset* passet) const
{
	if (Source == EApparanceValueSource::Parameter && Parameter!=0)
	{
		//access parameter value after checks
		int parameter_index;
		const FApparancePlacementParameter* param_info = passet->FindParameterInfo(Parameter, &parameter_index);
		if (param_info && param_info->Type==EApparanceParameterType::Texture)
		{			
			//textures indexed above regular parameters
			int num_regular_parameters = passet->ExpectedParameters.Num();
			if(parameter_index >= num_regular_parameters)
			{
				int texture_index = parameter_index - num_regular_parameters;
				return UApparanceResourceListEntry_Component::GetTextureParameter(textures, texture_index);
			}
			else
			{
				UE_LOG(LogApparance, Error, TEXT("Failed to find texture parameter info with ID %i, returned index %i of %i normal parameters"), Parameter, parameter_index, num_regular_parameters);
				return nullptr;
			}
		}
	}
	else if(Source==EApparanceValueSource::Constant && Constant)
	{
		//access constant value
		UApparanceValueConstant_Texture* pconstt = Cast<UApparanceValueConstant_Texture>(Constant);
		if (pconstt)
		{
			return pconstt->Value;
		}
	}
	return nullptr;
}
