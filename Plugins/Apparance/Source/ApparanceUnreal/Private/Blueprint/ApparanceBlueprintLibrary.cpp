//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#include "Blueprint/ApparanceBlueprintLibrary.h"
#include "ApparanceUnreal.h"
#include "ApparanceEntity.h"
#include "Utility/ApparanceConversion.h"
#include "ApparanceParametersComponent.h"
#include "Geometry/EntityRendering.h"

//unreal


///////////////////////////////////// HELPERS ////////////////////////////////////

// helper to read a string parameter into an unreal FString
//
FString ParameterCollectionGetFString(const Apparance::IParameterCollection* params, int index, FString* fallback = nullptr, bool* success=nullptr)
{
	FString value;

	//get
	int Len = 0;
	if (params->GetString(index, 0, nullptr, &Len))
	{
		value = FString::ChrN(Len, TCHAR(' '));
		TCHAR* pbuffer = value.GetCharArray().GetData();
		if (params->GetString(index, Len, pbuffer))
		{
			if (success)
			{
				*success = true;
			}
			return value;
		}
	}

	//failed
	if (success)
	{
		*success = false;
	}
	if (fallback)
	{
		return *fallback;
	}
	else
	{
		return TEXT("");
	}
}







///////////////////////////////// ENTITY PARAMETERS //////////////////////////////////


// set specific integer parameter value of an entity
//
void UApparanceBlueprintLibrary::SetEntityIntParameter( AApparanceEntity* pentity, int parameter_id, int value )
{
	//checks (entity input connected?)
	if(!pentity) { return; }

	Apparance::IParameterCollection* p = pentity->GetOverrideParameters();
	p->BeginEdit();
	if(!p->ModifyInteger( parameter_id, value ))
	{
		int index = p->AddParameter( Apparance::Parameter::Integer, parameter_id );
		p->SetInteger( index, value );
	}
	p->EndEdit();
	pentity->RebuildDeferred(); //queue up rebuild (if nothing else triggers it in the meantime)
}

// set specific float parameter value of an entity
//
void UApparanceBlueprintLibrary::SetEntityFloatParameter( AApparanceEntity* pentity, int parameter_id, float value )
{
	//checks (entity input connected?)
	if(!pentity) { return; }

	Apparance::IParameterCollection* p = pentity->GetOverrideParameters();
	p->BeginEdit();
	if(!p->ModifyFloat( parameter_id, value ))
	{
		int index = p->AddParameter( Apparance::Parameter::Float, parameter_id );
		p->SetFloat( index, value );
	}
	p->EndEdit();
	pentity->RebuildDeferred(); //queue up rebuild (if nothing else triggers it in the meantime)
}

// set specific bool parameter value of an entity
//
void UApparanceBlueprintLibrary::SetEntityBoolParameter( AApparanceEntity* pentity, int parameter_id, bool value )
{
	//checks (entity input connected?)
	if(!pentity) { return; }

	Apparance::IParameterCollection* p = pentity->GetOverrideParameters();
	p->BeginEdit();
	if(!p->ModifyBool( parameter_id, value ))
	{
		int index = p->AddParameter( Apparance::Parameter::Bool, parameter_id );
		p->SetBool( index, value );
	}
	p->EndEdit();
	pentity->RebuildDeferred(); //queue up rebuild (if nothing else triggers it in the meantime)
}

// set specific string parameter value of an entity
//
void UApparanceBlueprintLibrary::SetEntityStringParameter( AApparanceEntity* pentity, int parameter_id, FString value )
{
	//checks (entity input connected?)
	if(!pentity) { return; }

	Apparance::IParameterCollection* p = pentity->GetOverrideParameters();
	p->BeginEdit();
	if(!p->ModifyString( parameter_id, value.Len(), *value ))
	{
		int index = p->AddParameter( Apparance::Parameter::String, parameter_id );
		p->SetString( index, value.Len(), *value );
	}
	p->EndEdit();
	pentity->RebuildDeferred(); //queue up rebuild (if nothing else triggers it in the meantime)
}

// set specific colour parameter value of an entity
//
void UApparanceBlueprintLibrary::SetEntityColourParameter( AApparanceEntity* pentity, int parameter_id, FLinearColor value )
{
	//checks (entity input connected?)
	if(!pentity) { return; }

	//convert
	Apparance::Colour colour_value = APPARANCECOLOUR_FROM_UNREALLINEARCOLOR(value);

	Apparance::IParameterCollection* p = pentity->GetOverrideParameters();
	p->BeginEdit();
	if(!p->ModifyColour( parameter_id, &colour_value ))
	{
		int index = p->AddParameter( Apparance::Parameter::Colour, parameter_id );
		p->SetColour( index, &colour_value );
	}
	p->EndEdit();
	pentity->RebuildDeferred(); //queue up rebuild (if nothing else triggers it in the meantime)
}

// set specific vector3 parameter value of an entity
//
void UApparanceBlueprintLibrary::SetEntityVector3Parameter( AApparanceEntity* pentity, int parameter_id, FVector value )
{
	//checks (entity input connected?)
	if(!pentity) { return; }

	//convert?!?
	Apparance::Vector3 vector_value(value.X, value.Y, value.Z);

	Apparance::IParameterCollection* p = pentity->GetOverrideParameters();
	p->BeginEdit();
	if(!p->ModifyVector3( parameter_id, &vector_value ))
	{
		int index = p->AddParameter( Apparance::Parameter::Vector3, parameter_id );
		p->SetVector3( index, &vector_value );
	}
	p->EndEdit();
	pentity->RebuildDeferred(); //queue up rebuild (if nothing else triggers it in the meantime)
}

// set specific frame parameter value of an entity
//
void UApparanceBlueprintLibrary::SetEntityFrameParameter( AApparanceEntity* pentity, int parameter_id, FApparanceFrame value )
{
	//checks (entity input connected?)
	if(!pentity) { return; }

	//convert
	Apparance::Frame frame_value;
	value.GetFrame(frame_value);

	Apparance::IParameterCollection* p = pentity->GetOverrideParameters();
	p->BeginEdit();
	if(!p->ModifyFrame( parameter_id, &frame_value ))
	{
		int index = p->AddParameter( Apparance::Parameter::Frame, parameter_id );
		p->SetFrame( index, &frame_value );
	}
	p->EndEdit();
	pentity->RebuildDeferred(); //queue up rebuild (if nothing else triggers it in the meantime)
}



//forward decl
static void PopulateParameterCollectionFromArray(Apparance::IParameterCollection* out_params, FArrayProperty* property, const void* StructPtr);
static void PopulateParameterCollectionFromStruct(Apparance::IParameterCollection* out_params, FStructProperty* StructType, const void* StructPtr);
static void PopulateArrayFromParameterCollection(FArrayProperty* ArrayType, void* StructPtr, const Apparance::IParameterCollection* in_params, int num_params);
static void PopulateStructFromParameterCollection(FStructProperty* StructType, void* StructPtr, const Apparance::IParameterCollection* in_params);

// handle adding single property value to a collection
//
static void PopulateParameterCollectionValue( Apparance::IParameterCollection* out_params, const void* value_ptr, FProperty* property, int param_id=0)
{
	// numeric
	if (FNumericProperty* NumericProperty = CastField<FNumericProperty>(property))
	{
		if (NumericProperty->IsFloatingPoint())
		{
			float value = NumericProperty->GetFloatingPointPropertyValue(value_ptr);
			int index = out_params->AddParameter(Apparance::Parameter::Float, param_id);
			out_params->SetFloat(index, value);
		}
		else if (NumericProperty->IsInteger())
		{
			int value = NumericProperty->GetSignedIntPropertyValue(value_ptr);
			int index = out_params->AddParameter(Apparance::Parameter::Integer, param_id);
			out_params->SetInteger(index, value);
		}
	}
	else if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(property))
	{
		bool value = BoolProperty->GetPropertyValue(value_ptr);
		int index = out_params->AddParameter(Apparance::Parameter::Bool, param_id);
		out_params->SetBool(index, value);
	}
	else if (FNameProperty* NameProperty = CastField<FNameProperty>(property))
	{
		FString value = NameProperty->GetPropertyValue(value_ptr).ToString();
		int index = out_params->AddParameter(Apparance::Parameter::String, param_id);
		out_params->SetString(index, value.Len(), *value);
	}
	else if (FStrProperty* StringProperty = CastField<FStrProperty>(property))
	{
		FString value = StringProperty->GetPropertyValue(value_ptr);
		int index = out_params->AddParameter(Apparance::Parameter::String, param_id);
		out_params->SetString(index, value.Len(), *value);
	}
	else if (FTextProperty* TextProperty = CastField<FTextProperty>(property))
	{
		FString value = TextProperty->GetPropertyValue(value_ptr).ToString();
		int index = out_params->AddParameter(Apparance::Parameter::String, param_id);
		out_params->SetString(index, value.Len(), *value);
	}
	else if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(property))
	{
		int index = out_params->AddParameter(Apparance::Parameter::List, param_id);
		Apparance::IParameterCollection* array_params = out_params->SetList(index);
		array_params->BeginEdit();
		PopulateParameterCollectionFromArray(array_params, ArrayProperty, value_ptr);
		array_params->EndEdit();
	}
	else if (FStructProperty* StructProperty = CastField<FStructProperty>(property))
	{
		if (property->GetCPPType() == TEXT("FVector"))
		{
			FVector value = *((FVector*)value_ptr);
			Apparance::Vector3 avalue = APPARANCEVECTOR3_FROM_UNREALVECTOR(value);
			int index = out_params->AddParameter(Apparance::Parameter::Vector3, param_id);
			out_params->SetVector3(index, &avalue);
		}
		else if (property->GetCPPType() == TEXT("FVector2D"))
		{
			FVector2D value2d = *((FVector2D*)value_ptr);
			FVector value(value2d.X, value2d.Y, 0);
			Apparance::Vector3 apparance_value = APPARANCEVECTOR3_FROM_UNREALVECTOR(value);
			int index = out_params->AddParameter(Apparance::Parameter::Vector3, param_id);
			out_params->SetVector3(index, &apparance_value);
		}
		else if (property->GetCPPType() == TEXT("FColor"))
		{
			FColor value = *((FColor*)value_ptr);
			Apparance::Colour apparance_value = APPARANCECOLOUR_FROM_UNREALCOLOR(value);
			int index = out_params->AddParameter(Apparance::Parameter::Colour, param_id);
			out_params->SetColour(index, &apparance_value);
		}
		else if (property->GetCPPType() == TEXT("FLinearColor"))
		{
			FLinearColor value = *((FLinearColor*)value_ptr);
			Apparance::Colour apparance_value = APPARANCECOLOUR_FROM_UNREALLINEARCOLOR(value);
			int index = out_params->AddParameter(Apparance::Parameter::Colour, param_id);
			out_params->SetColour(index, &apparance_value);
		}
		else if (property->GetCPPType() == TEXT("FApparanceFrame"))
		{
			FApparanceFrame value = *((FApparanceFrame*)value_ptr);
			Apparance::Frame apparance_value;
			value.GetFrame(apparance_value);
			ApparanceFrameFromUnrealWorldspaceFrame(apparance_value); //Always remap?  How would we choose?
			int index = out_params->AddParameter(Apparance::Parameter::Frame, param_id);
			out_params->SetFrame(index, &apparance_value);
		}
		else
		{
			//generic full member structure handling
			int index = out_params->AddParameter(Apparance::Parameter::List, param_id);
			Apparance::IParameterCollection* sub_list = out_params->SetList(index);
			sub_list->BeginEdit();
			PopulateParameterCollectionFromStruct(sub_list, StructProperty, value_ptr);
			sub_list->EndEdit();
		}
	}
	else
	{
		//??
		check(false);
	}
}


// handle reading single property value from a collection
//
static bool AccessParameterCollectionValue(void* value_ptr, FProperty* property, const Apparance::IParameterCollection* in_params, int param_index)
{
	// numeric
	if (FNumericProperty* NumericProperty = CastField<FNumericProperty>(property))
	{
		if (NumericProperty->IsFloatingPoint())
		{
			float value;
			if (in_params->GetFloat(param_index, &value))
			{
				NumericProperty->SetFloatingPointPropertyValue(value_ptr, (double)value);
				return true;
			}
		}
		else if (NumericProperty->IsInteger())
		{
			int value;
			if (in_params->GetInteger(param_index, &value))
			{
				NumericProperty->SetIntPropertyValue( value_ptr, (int64)value );
				return true;
			}
		}
	}
	else if (FBoolProperty* BoolProperty = CastField<FBoolProperty>(property))
	{
		bool value;
		if (in_params->GetBool(param_index, &value))
		{
			BoolProperty->SetPropertyValue(value_ptr, value);
			return true;
		}
	}
	else if (FNameProperty* NameProperty = CastField<FNameProperty>(property))
	{
		bool success;
		FString value = ParameterCollectionGetFString(in_params, param_index, nullptr, &success);
		if (success)
		{
			FName name(value);
			NameProperty->SetPropertyValue(value_ptr, name);
			return true;
		}
	}
	else if (FStrProperty* StringProperty = CastField<FStrProperty>(property))
	{
		bool success;
		FString value = ParameterCollectionGetFString(in_params, param_index, nullptr, &success);
		if (success)
		{
			StringProperty->SetPropertyValue(value_ptr, value);
			return true;
		}
	}
	else if (FTextProperty* TextProperty = CastField<FTextProperty>(property))	
	{
		bool success;
		FString value = ParameterCollectionGetFString(in_params, param_index, nullptr, &success);
		if (success)
		{
			FText text = FText::FromString(value);
			TextProperty->SetPropertyValue(value_ptr, text);
			return true;
		}
	}
	else if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(property))
	{
		const Apparance::IParameterCollection* array_params = in_params->GetList( param_index );
		if (array_params)
		{
			int num_items = array_params->BeginAccess();
			PopulateArrayFromParameterCollection( ArrayProperty, value_ptr, array_params, num_items);
			array_params->EndAccess();
			return true;
		}
	}
	else if (FStructProperty* StructProperty = CastField<FStructProperty>(property))
	{
		if (property->GetCPPType() == TEXT("FVector"))
		{
			Apparance::Vector3 avalue;
			if (in_params->GetVector3(param_index, &avalue))
			{
				FVector value = UNREALVECTOR_FROM_APPARANCEVECTOR3(avalue);
				*((FVector*)value_ptr) = value;
				return true;
			}
		}
		else if (property->GetCPPType() == TEXT("FVector2D"))
		{
			Apparance::Vector3 avalue;
			if (in_params->GetVector3(param_index, &avalue))
			{
				FVector value = UNREALVECTOR_FROM_APPARANCEVECTOR3(avalue);
				*((FVector2D*)value_ptr) = FVector2D(value.X, value.Y);
				return true;
			}
		}
		else if (property->GetCPPType() == TEXT("FColor"))
		{
			Apparance::Colour avalue;
			if (in_params->GetColour(param_index, &avalue))
			{
				FColor value = UNREALCOLOR_FROM_APPARANCECOLOUR(avalue);
				*((FColor*)value_ptr) = value;
				return true;
			}
		}
		else if (property->GetCPPType() == TEXT("FLinearColor"))
		{
			Apparance::Colour avalue;
			if (in_params->GetColour(param_index, &avalue))
			{
				FLinearColor value = UNREALLINEARCOLOR_FROM_APPARANCECOLOUR(avalue);
				*((FLinearColor*)value_ptr) = value;
				return true;
			}
		}
		else if (property->GetCPPType() == TEXT("FApparanceFrame"))
		{
			Apparance::Frame avalue;
			if (in_params->GetFrame(param_index, &avalue))
			{
				UnrealWorldspaceFrameFromApparanceFrame(avalue);
				FApparanceFrame* value = (FApparanceFrame*)value_ptr;
				value->SetFrame(avalue);
				return true;
			}
		}
		else
		{
			//generic full member structure handling
			const Apparance::IParameterCollection* sub_list = in_params->GetList( param_index );
			if (sub_list)
			{
				sub_list->BeginAccess();
				PopulateStructFromParameterCollection( StructProperty, value_ptr, sub_list );
				sub_list->EndAccess();
			}
		}
	}
	else
	{
		//unknown type
		check(false);
	}

	//failed?
	return false;
}

// handle adding an array of values to a collection
//
static void PopulateParameterCollectionFromArray(Apparance::IParameterCollection* out_params, FArrayProperty* ArrayType, const void* StructPtr)
{
	FScriptArrayHelper Helper(ArrayType, StructPtr);
	for (int32 i = 0, n = Helper.Num(); i < n; ++i)
	{
		const void* value_ptr = (const void*)Helper.GetRawPtr(i);
		PopulateParameterCollectionValue(out_params, value_ptr, ArrayType->Inner);
	}
}

// handle adding members of a struct to a collection
//
static void PopulateParameterCollectionFromStruct( Apparance::IParameterCollection* out_params, FStructProperty* StructType, const void* StructPtr )
{
	//read struct values
	UScriptStruct* pstruct = StructType->Struct;
	int dummy_id = 0;
	for (TFieldIterator<FProperty> It(pstruct); It; ++It)
	{
		dummy_id++;
		FProperty* property = *It;

		//FString prop_name = property->GetName();
		check(property->ArrayDim == 1);
		const void* value_ptr = property->ContainerPtrToValuePtr<void>(StructPtr);

		PopulateParameterCollectionValue(out_params, value_ptr, property, dummy_id);
	}
}


// handle passing a collection of values into an array
//
static void PopulateArrayFromParameterCollection(FArrayProperty* ArrayType, void* StructPtr, const Apparance::IParameterCollection* in_params, int num_params )
{
	//resize array
	FScriptArrayHelper Helper(ArrayType, StructPtr);
	Helper.Resize(num_params);

	//populate array
	int param_index = 0;
	for (int32 i = 0, n = Helper.Num(); i < n; ++i)
	{
		void* value_ptr = (void*)Helper.GetRawPtr(i);
		/*bool success = */AccessParameterCollectionValue(value_ptr, ArrayType->Inner, in_params, param_index);
		param_index++;
	}
}

// handle passing a collection of values into a struct
//
static void PopulateStructFromParameterCollection(FStructProperty* StructType, void* StructPtr, const Apparance::IParameterCollection* in_params)
{
	//reset structure
	StructType->ClearValue( StructPtr );

	//scan struct values
	UScriptStruct* pstruct = StructType->Struct;
	int param_index = 0;
	for (TFieldIterator<FProperty> It(pstruct); It; ++It)
	{
		FProperty* property = *It;

		check(property->ArrayDim == 1);
		void* value_ptr = property->ContainerPtrToValuePtr<void>(StructPtr);

		/*bool success = */AccessParameterCollectionValue( value_ptr, property, in_params, param_index );
		param_index++;
	}
}




// handles list parameter setting from members of any passed structure
//
void UApparanceBlueprintLibrary::SetEntityListParameter_Struct_Generic(AApparanceEntity* Entity, int ParameterID, FStructProperty* StructType, const void* StructPtr)
{
	//get/prep list we are setting
	Apparance::IParameterCollection* p = Entity->GetOverrideParameters();
	p->BeginEdit();
	//ensure fresh list
	p->RemoveParameter(ParameterID);
	p->AddParameter(Apparance::Parameter::List, ParameterID);
	//prep
	Apparance::IParameterCollection* list = p->ModifyList( ParameterID );
	list->BeginEdit();

	//defer to struct->paramcollection handler
	//  can in turn defer to array->paramcollection handler, etc.
	PopulateParameterCollectionFromStruct( list, StructType, StructPtr );

	//done, cleanup & apply
	list->EndEdit();
	p->EndEdit();
	Entity->RebuildDeferred(); //queue up rebuild (if nothing else triggers it in the meantime)
}

// handles list parameter setting from items in any passed array
//
void UApparanceBlueprintLibrary::SetEntityListParameter_Array_Generic(AApparanceEntity* Entity, int ParameterID, FArrayProperty* ArrayType, const void* ArrayPtr)
{
	//get/prep list we are setting
	Apparance::IParameterCollection* p = Entity->GetOverrideParameters();
	p->BeginEdit();
	//ensure fresh list
	p->RemoveParameter(ParameterID);
	p->AddParameter(Apparance::Parameter::List, ParameterID);
	//prep
	Apparance::IParameterCollection* list = p->ModifyList(ParameterID);
	list->BeginEdit();

	//defer to struct->paramcollection handler
	//  can in turn defer to array->paramcollection handler, etc.
	PopulateParameterCollectionFromArray(list, ArrayType, ArrayPtr);

	//done, cleanup & apply
	list->EndEdit();
	p->EndEdit();
	Entity->RebuildDeferred(); //queue up rebuild (if nothing else triggers it in the meantime)
}

// set entity parameters from a parameter struct
//
void UApparanceBlueprintLibrary::SetEntityParameters(AApparanceEntity* pentity, FApparanceParameters Parameters)
{
	//checks (entity input connected?)
	if(!pentity) { return; }

	Apparance::IParameterCollection* source_params = Parameters.Parameters.Get();
	Apparance::IParameterCollection* target_params = pentity->GetOverrideParameters();

	//apply source values to target
	target_params->Merge( source_params );

	pentity->RebuildDeferred(); //queue up rebuild (if nothing else triggers it in the meantime)
}

// request rebuild of content (when no parameter changes)
//
void UApparanceBlueprintLibrary::RebuildEntity( AApparanceEntity* pentity )
{
	//checks (entity input connected?)
	if(!pentity) { return; }

	//rebuild now
	pentity->RebuildDeferred();
}


//////////////////////////// PARAMETER-COLLECTION STRUCT PARAMETERS : SET /////////////////////////////

// set specific integer parameter value of a parameter collection
//
void UApparanceBlueprintLibrary::SetParamsIntParameter( FApparanceParameters Params, int parameter_id, int value )
{
	Apparance::IParameterCollection* p = Params.EnsureParameterCollection();
	p->BeginEdit();
	if(!p->ModifyInteger( parameter_id, value ))
	{
		int index = p->AddParameter( Apparance::Parameter::Integer, parameter_id );
		p->SetInteger( index, value );
	}
	p->EndEdit();
}

// set specific float parameter value of a parameter collection
//
void UApparanceBlueprintLibrary::SetParamsFloatParameter( FApparanceParameters Params, int parameter_id, float value )
{
	Apparance::IParameterCollection* p = Params.EnsureParameterCollection();
	p->BeginEdit();
	if(!p->ModifyFloat( parameter_id, value ))
	{
		int index = p->AddParameter( Apparance::Parameter::Float, parameter_id );
		p->SetFloat( index, value );
	}
	p->EndEdit();
}

// set specific bool parameter value of a parameter collection
//
void UApparanceBlueprintLibrary::SetParamsBoolParameter( FApparanceParameters Params, int parameter_id, bool value )
{
	Apparance::IParameterCollection* p = Params.EnsureParameterCollection();
	p->BeginEdit();
	if(!p->ModifyBool( parameter_id, value ))
	{
		int index = p->AddParameter( Apparance::Parameter::Bool, parameter_id );
		p->SetBool( index, value );
	}
	p->EndEdit();
}

// set specific string parameter value of a parameter collection
//
void UApparanceBlueprintLibrary::SetParamsStringParameter( FApparanceParameters Params, int parameter_id, FString value )
{
	Apparance::IParameterCollection* p = Params.EnsureParameterCollection();
	p->BeginEdit();
	if(!p->ModifyString( parameter_id, value.Len(), *value ))
	{
		int index = p->AddParameter( Apparance::Parameter::String, parameter_id );
		p->SetString( index, value.Len(), *value );
	}
	p->EndEdit();
}

// set specific colour parameter value of a parameter collection
//
void UApparanceBlueprintLibrary::SetParamsColourParameter( FApparanceParameters Params, int parameter_id, FLinearColor value )
{
	//convert
	Apparance::Colour colour_value = APPARANCECOLOUR_FROM_UNREALLINEARCOLOR(value);
	
	Apparance::IParameterCollection* p = Params.EnsureParameterCollection();
	p->BeginEdit();
	if(!p->ModifyColour( parameter_id, &colour_value ))
	{
		int index = p->AddParameter( Apparance::Parameter::Colour, parameter_id );
		p->SetColour( index, &colour_value );
	}
	p->EndEdit();
}

// set specific vector3 parameter value of a parameter collection
//
void UApparanceBlueprintLibrary::SetParamsVector3Parameter( FApparanceParameters Params, int parameter_id, FVector value )
{
	//convert?!?
	Apparance::Vector3 vector_value(value.X, value.Y, value.Z);
	
	Apparance::IParameterCollection* p = Params.EnsureParameterCollection();
	p->BeginEdit();
	if(!p->ModifyVector3( parameter_id, &vector_value ))
	{
		int index = p->AddParameter( Apparance::Parameter::Vector3, parameter_id );
		p->SetVector3( index, &vector_value );
	}
	p->EndEdit();
}

// set specific frame parameter value of a parameter collection
//
void UApparanceBlueprintLibrary::SetParamsFrameParameter( FApparanceParameters Params, int parameter_id, FApparanceFrame value )
{
	//convert
	Apparance::Frame frame_value;
	value.GetFrame(frame_value);
	
	Apparance::IParameterCollection* p = Params.EnsureParameterCollection();
	p->BeginEdit();
	if(!p->ModifyFrame( parameter_id, &frame_value ))
	{
		int index = p->AddParameter( Apparance::Parameter::Frame, parameter_id );
		p->SetFrame( index, &frame_value );
	}
	p->EndEdit();
}

// handles parameter collection list parameter setting from members of any passed structure
//
void UApparanceBlueprintLibrary::SetParamsListParameter_Struct_Generic(FApparanceParameters Params, int ParameterID, FStructProperty* StructType, const void* StructPtr)
{
	//get/prep list we are setting
	Apparance::IParameterCollection* p = Params.Parameters.Get();
	if (p)
	{
		p->BeginEdit();
		//ensure fresh list
		p->RemoveParameter(ParameterID);
		p->AddParameter(Apparance::Parameter::List, ParameterID);
		//prep
		Apparance::IParameterCollection* list = p->ModifyList(ParameterID);
		if (list)
		{
			list->BeginEdit();

			//defer to struct->paramcollection handler
			//  can in turn defer to array->paramcollection handler, etc.
			PopulateParameterCollectionFromStruct(list, StructType, StructPtr);

			//done, cleanup & apply
			list->EndEdit();
		}
		p->EndEdit();
	}
}

// handles parameter collection list parameter setting from items in any passed array
//
void UApparanceBlueprintLibrary::SetParamsListParameter_Array_Generic(FApparanceParameters Params, int ParameterID, FArrayProperty* ArrayType, const void* ArrayPtr)
{
	//get/prep list we are setting
	Apparance::IParameterCollection* p = Params.Parameters.Get();
	if (p)
	{
		p->BeginEdit();
		//ensure fresh list
		p->RemoveParameter(ParameterID);
		p->AddParameter(Apparance::Parameter::List, ParameterID);
		//prep
		Apparance::IParameterCollection* list = p->ModifyList(ParameterID);
		if (list)
		{
			list->BeginEdit();

			//defer to struct->paramcollection handler
			//  can in turn defer to array->paramcollection handler, etc.
			PopulateParameterCollectionFromArray(list, ArrayType, ArrayPtr);

			//done, cleanup & apply
			list->EndEdit();
		}
		p->EndEdit();
	}
}


//////////////////////////// PARAMETER-COLLECTION STRUCT PARAMETERS : GET /////////////////////////////

//get specific parameter collection parameter output
int UApparanceBlueprintLibrary::GetParamsIntParameter(FApparanceParameters Params, int ParameterID)
{
	int value = 0;
	Apparance::IParameterCollection* p = Params.Parameters.Get();
	if(p)
	{
		p->BeginAccess();
		p->FindInteger(ParameterID, &value);
		p->EndAccess();
	}
	return value;
}
float UApparanceBlueprintLibrary::GetParamsFloatParameter( FApparanceParameters Params, int ParameterID )
{
	float value = 0;
	Apparance::IParameterCollection* p = Params.Parameters.Get();
	if(p)
	{
		p->BeginAccess();
		p->FindFloat(ParameterID, &value);
		p->EndAccess();
	}
	return value;
}
bool UApparanceBlueprintLibrary::GetParamsBoolParameter( FApparanceParameters Params, int ParameterID )
{
	bool value = false;
	Apparance::IParameterCollection* p = Params.Parameters.Get();
	if(p)
	{
		p->BeginAccess();
		p->FindBool(ParameterID, &value);
		p->EndAccess();
	}
	return value;
}
FString UApparanceBlueprintLibrary::GetParamsStringParameter( FApparanceParameters Params, int ParameterID )
{
	FString value;
	Apparance::IParameterCollection* p = Params.Parameters.Get();
	if(p)
	{
		p->BeginAccess();
		int text_len = 0;
		if(p->FindString( ParameterID, 0, nullptr, &text_len ))
		{
			//retrieve string
			value = FString::ChrN( text_len, TCHAR(' ') );	
			TCHAR* pbuffer = value.GetCharArray().GetData();
			p->FindString( ParameterID, text_len, pbuffer );			
		}
		p->EndAccess();
	}
	return value;
}
FLinearColor UApparanceBlueprintLibrary::GetParamsColourParameter( FApparanceParameters Params, int ParameterID )
{
	Apparance::Colour value;
	Apparance::IParameterCollection* p = Params.Parameters.Get();
	if(p)
	{
		p->BeginAccess();
		p->FindColour(ParameterID, &value);
		p->EndAccess();
	}
	return UNREALLINEARCOLOR_FROM_APPARANCECOLOUR( value );
}
FVector UApparanceBlueprintLibrary::GetParamsVector3Parameter( FApparanceParameters Params, int ParameterID )
{
	Apparance::Vector3 value;
	Apparance::IParameterCollection* p = Params.Parameters.Get();
	if(p)
	{
		p->BeginAccess();
		p->FindVector3(ParameterID, &value);
		p->EndAccess();
	}
	return UNREALVECTOR_FROM_APPARANCEVECTOR3( value );
}
FApparanceFrame UApparanceBlueprintLibrary::GetParamsFrameParameter( FApparanceParameters Params, int ParameterID )
{
	Apparance::Frame value;
	Apparance::IParameterCollection* p = Params.Parameters.Get();
	if(p)
	{
		p->BeginAccess();
		p->FindFrame(ParameterID, &value);
		p->EndAccess();
	}

	FApparanceFrame fr;
	fr.SetFrame(value);
	return fr;
}

// handles parameter collection list parameter getting to members of any passed structure
//
void UApparanceBlueprintLibrary::GetParamsListParameter_Struct_Generic(FApparanceParameters Params, int ParameterID, FStructProperty* StructType, void* StructPtr)
{
	//get/prep list we are setting
	Apparance::IParameterCollection* p = Params.Parameters.Get();
	if (p)
	{
		p->BeginAccess();
		//access list
		const Apparance::IParameterCollection* list = p->FindList(ParameterID);
		if (list)
		{
			list->BeginAccess();

			//defer to paramcollection->struct handler
			//  can in turn defer to paramcollection->array handler, etc.
			PopulateStructFromParameterCollection(StructType, StructPtr, list);

			//done, cleanup & apply
			list->EndAccess();
		}
		p->EndAccess();
	}
}

// handles parameter collection list parameter getting to items in any passed array
//
void UApparanceBlueprintLibrary::GetParamsListParameter_Array_Generic(FApparanceParameters Params, int ParameterID, FArrayProperty* ArrayType, void* ArrayPtr)
{
	//get/prep list we are setting
	Apparance::IParameterCollection* p = Params.Parameters.Get();
	if (p)
	{
		p->BeginAccess();
		//access list
		const Apparance::IParameterCollection* list = p->FindList(ParameterID);
		if (list)
		{
			int list_size = list->BeginAccess();

			//defer to paramcollection->struct handler
			//  can in turn defer to paramcollection->array handler, etc
			PopulateArrayFromParameterCollection(ArrayType, ArrayPtr, list, list_size);

			//done, cleanup & apply
			list->EndAccess();
		}
		p->EndAccess();
	}
}


///////////////////////////////// FRAME TYPE BLUEPRINT SUPPORT //////////////////////////////////

FApparanceFrame::FApparanceFrame(Apparance::Frame& frame)
{
	SetFrame(frame);
}
void FApparanceFrame::SetFrame(Apparance::Frame& frame)
{
	Origin.X = frame.Origin.X; 
	Origin.Y = frame.Origin.Y; 
	Origin.Z = frame.Origin.Z;
	Size.X   = frame.Size.X  ;
	Size.Y   = frame.Size.Y  ;
	Size.Z   = frame.Size.Z  ;
	XAxis.X  = frame.Orientation.X.X; 
	XAxis.Y  = frame.Orientation.X.Y; 
	XAxis.Z  = frame.Orientation.X.Z;
	YAxis.X  = frame.Orientation.Y.X; 
	YAxis.Y  = frame.Orientation.Y.Y; 
	YAxis.Z  = frame.Orientation.Y.Z;
	ZAxis.X  = frame.Orientation.Z.X; 
	ZAxis.Y  = frame.Orientation.Z.Y; 
	ZAxis.Z  = frame.Orientation.Z.Z;
}
void FApparanceFrame::GetFrame(Apparance::Frame& frame_out)
{
	frame_out.Origin.X = Origin.X; 
	frame_out.Origin.Y = Origin.Y; 
	frame_out.Origin.Z = Origin.Z;
	frame_out.Size.X   = Size.X  ;
	frame_out.Size.Y   = Size.Y  ;
	frame_out.Size.Z   = Size.Z  ;
	frame_out.Orientation.X.X = XAxis.X ; 
	frame_out.Orientation.X.Y = XAxis.Y ; 
	frame_out.Orientation.X.Z = XAxis.Z ;
	frame_out.Orientation.Y.X = YAxis.X ; 
	frame_out.Orientation.Y.Y = YAxis.Y ; 
	frame_out.Orientation.Y.Z = YAxis.Z ;
	frame_out.Orientation.Z.X = ZAxis.X ; 
	frame_out.Orientation.Z.Y = ZAxis.Y ; 
	frame_out.Orientation.Z.Z = ZAxis.Z ;
}
FMatrix UApparanceBlueprintLibrary::GetFrameTransform(FApparanceFrame Frame)
{
	FMatrix rot = GetFrameOrientation(Frame);
	FTranslationMatrix tra(GetFrameOrigin(Frame));
	FScaleMatrix sca(GetFrameSize(Frame));
	return sca * rot * tra;
}
FVector UApparanceBlueprintLibrary::GetFrameOrigin( FApparanceFrame Frame )
{
	return FVector(Frame.Origin.X, Frame.Origin.Y, Frame.Origin.Z);
}
FVector UApparanceBlueprintLibrary::GetFrameSize( FApparanceFrame Frame )
{
	return FVector(Frame.Size.X, Frame.Size.Y, Frame.Size.Z);
}
FMatrix UApparanceBlueprintLibrary::GetFrameOrientation( FApparanceFrame Frame )
{
	FVector xaxis(Frame.XAxis.X, Frame.XAxis.Y, Frame.XAxis.Z);
	FVector yaxis(Frame.YAxis.X, Frame.YAxis.Y, Frame.YAxis.Z);
	FVector zaxis(Frame.ZAxis.X, Frame.ZAxis.Y, Frame.ZAxis.Z);
	FMatrix m;
	m.SetAxis(0, xaxis);
	m.SetAxis(1, yaxis);
	m.SetAxis(2, zaxis);
	return m;	//TODO: check, may need swapping
}
FRotator UApparanceBlueprintLibrary::GetFrameRotator( FApparanceFrame Frame )
{
	return GetFrameOrientation(Frame).Rotator();
}
void UApparanceBlueprintLibrary::GetFrameEuler(FApparanceFrame Frame, float& Roll, float& Pitch, float& Yaw)
{
	FRotator rot = GetFrameRotator(Frame);
	Roll = rot.Roll;
	Pitch = rot.Pitch;
	Yaw = rot.Yaw;
}
//use transform to maps unit cube into frame
FApparanceFrame UApparanceBlueprintLibrary::MakeFrameFromTransform(FMatrix Transform)
{
	FVector origin = Transform.TransformPosition(FVector());
	FVector alongx = Transform.TransformPosition(FVector(1,0,0))-origin;
	FVector alongy = Transform.TransformPosition(FVector(0,1,0))-origin;
	FVector alongz = Transform.TransformPosition(FVector(0,0,1))-origin;
	float lenx = alongx.Size();
	float leny = alongy.Size();
	float lenz = alongz.Size();

	FApparanceFrame f;
	f.Origin.X = origin.X;
	f.Origin.Y = origin.Y;
	f.Origin.Z = origin.Z;
	f.Size.X = lenx;
	f.Size.Y = leny;
	f.Size.Z = lenz;
	f.XAxis.X = alongx.X / lenx;
	f.XAxis.Y = alongx.Y / lenx;
	f.XAxis.Z = alongx.Z / lenx;
	f.YAxis.X = alongy.X / leny;
	f.YAxis.Y = alongy.Y / leny;
	f.YAxis.Z = alongy.Z / leny;
	f.ZAxis.X = alongz.X / lenz;
	f.ZAxis.Y = alongz.Y / lenz;
	f.ZAxis.Z = alongz.Z / lenz;
	return f;
}
FApparanceFrame UApparanceBlueprintLibrary::MakeFrameFromRotator(FVector Origin, FVector Size, FRotator Orientation )
{
	FApparanceFrame f;
	f.Origin.X = Origin.X;
	f.Origin.Y = Origin.Y;
	f.Origin.Z = Origin.Z;
	f.Size.X = Size.X;
	f.Size.Y = Size.Y;
	f.Size.Z = Size.Z;
	FRotationMatrix rot( Orientation );
	FVector xaxis = rot.TransformVector(FVector(1, 0, 0));
	FVector yaxis = rot.TransformVector(FVector(0, 1, 0));
	FVector zaxis = rot.TransformVector(FVector(0, 0, 1));
	f.XAxis.X = xaxis.X;
	f.XAxis.Y = xaxis.Y;
	f.XAxis.Z = xaxis.Z;
	f.YAxis.X = yaxis.X;
	f.YAxis.Y = yaxis.Y;
	f.YAxis.Z = yaxis.Z;
	f.ZAxis.X = zaxis.X;
	f.ZAxis.Y = zaxis.Y;
	f.ZAxis.Z = zaxis.Z;
	return f;
}
FApparanceFrame UApparanceBlueprintLibrary::MakeFrameFromEuler(FVector Origin, FVector Size, float Roll, float Pitch, float Yaw )
{
	FRotator Orientation(Roll, Pitch, Yaw);
	return MakeFrameFromRotator(Origin, Size, Orientation);
}
FApparanceFrame UApparanceBlueprintLibrary::MakeFrameFromBox( FBox Box )
{
	FApparanceFrame f;
	f.Origin.X = Box.Min.X;
	f.Origin.Y = Box.Min.Y;
	f.Origin.Z = Box.Min.Z;
	f.Size.X = Box.Max.X-Box.Min.X;
	f.Size.Y = Box.Max.Y-Box.Min.Y;
	f.Size.Z = Box.Max.Z-Box.Min.Z;
	f.XAxis = FVector(1, 0, 0);
	f.YAxis = FVector(0, 1, 0);
	f.ZAxis = FVector(0, 0, 1);
	return f;
}

///////////////////////////////// PARAMETER COLLECTION STRUCT //////////////////////////////////

// access helper, get contained parameter collection, init if not present
//
Apparance::IParameterCollection* FApparanceParameters::EnsureParameterCollection()
{
	//init on demand
	if (!Parameters.IsValid())
	{
		Parameters = MakeShareable( FApparanceUnrealModule::GetEngine()->CreateParameterCollection() );
	}
	return Parameters.Get();
}

// generate a new empty parameter collection
//
FApparanceParameters UApparanceBlueprintLibrary::MakeParameters()
{
	FApparanceParameters pc; 
	pc.EnsureParameterCollection();	//init
	return pc;
}

// generate a new empty parameter collection (bp callable version)
//
FApparanceParameters UApparanceBlueprintLibrary::NewParameters()
{
	return MakeParameters();
}

// Access any parameters an actor was procedurally placed with
//
FApparanceParameters UApparanceBlueprintLibrary::GetPlacementParameters(AActor* Target)
{
	FApparanceParameters pc; 
	Apparance::IParameterCollection* new_parm_collection = pc.EnsureParameterCollection();	//init

	//search actor for parameters component
	if(Target)
	{
		//locate entity (self or owner if placed by it)
		AApparanceEntity* pentity = nullptr;
		AActor* pactor = Target;
		while(pactor)
		{
			pentity = Cast<AApparanceEntity>( pactor );
			if(pentity)
			{
				//found entity
				break;
			}
			//search up hierarchy
			pactor = Cast<AActor>( pactor->GetOwner() );
		}

		//an entity! this will have parameters accessible
		if(pentity)
		{
			UApparanceParametersComponent* actor_params_component = pentity->FindComponentByClass<UApparanceParametersComponent>();
			if(actor_params_component)
			{
				const Apparance::IParameterCollection* actor_parameter_collection;
				if(pentity == Target)
				{
					//our own actor
					actor_parameter_collection = actor_params_component->GetActorParameters();
				}
				else
				{
					//a placed bp actor
					actor_parameter_collection = actor_params_component->GetBluepringActorParameters( Target );
				}
				if(actor_parameter_collection)
				{
					//store in output collection
					new_parm_collection->Merge( actor_parameter_collection );
				}
			}
		}
	}

	return pc;	
}

// Access the current parameters an actor has been built with
//
FApparanceParameters UApparanceBlueprintLibrary::GetCurrentParameters(AApparanceEntity* Target)
{
	FApparanceParameters pc; 
	Apparance::IParameterCollection* new_parm_collection = pc.EnsureParameterCollection();	//init
	
	//search actor for parameters component
	if(Target)
	{
		const Apparance::IParameterCollection* current_parameter_collection = Target->GetCurrentParameters();
		if(current_parameter_collection)
		{
			//store in output collection
			new_parm_collection->Merge( current_parameter_collection );
		}
	}
	
	return pc;	
}


//Access parameter count
int UApparanceBlueprintLibrary::GetParameterCount(FApparanceParameters Parameters)
{
	Apparance::IParameterCollection* params = Parameters.Parameters.Get();
	int num = params->BeginAccess();
	params->EndAccess();
	return num;
}

namespace ParameterTypeNames
{
	static const FName Frame("frame");
	static const FName Integer("integer");
	static const FName Float("float");
	static const FName Bool("bool");
	static const FName String("string");
	static const FName Colour("colour");
	static const FName Vector("vector");
}

//Access parameter type by index
FName UApparanceBlueprintLibrary::GetParameterType(FApparanceParameters Parameters, int Index, bool& Success)
{
	Apparance::IParameterCollection* params = Parameters.Parameters.Get();
	params->BeginAccess();
	FName type = NAME_None;
	switch (params->GetType(Index))
	{
		case Apparance::Parameter::Frame:   type = ParameterTypeNames::Frame; break;
		case Apparance::Parameter::Integer: type = ParameterTypeNames::Integer; break;
		case Apparance::Parameter::Float:   type = ParameterTypeNames::Float; break;
		case Apparance::Parameter::Bool:    type = ParameterTypeNames::Bool; break;
		case Apparance::Parameter::String:  type = ParameterTypeNames::String; break;
		case Apparance::Parameter::Colour:  type = ParameterTypeNames::Colour; break;
		case Apparance::Parameter::Vector3: type = ParameterTypeNames::Vector; break;
	}
	params->EndAccess();
	Success = !type.IsNone();
	return type;
}

//Access integer parameter value by index
int UApparanceBlueprintLibrary::GetIntegerParameter(FApparanceParameters Parameters, int Index, int Fallback, bool& Success)
{
	Apparance::IParameterCollection* params = Parameters.Parameters.Get();
	int value = Fallback;
	if(params)
	{
		params->BeginAccess();
		Success = false;
		if(params->GetInteger( Index, &value ))
		{
			Success = true;
		}
		params->EndAccess();
	}
	return value;
}

//Access float parameter value by index
float UApparanceBlueprintLibrary::GetFloatParameter(FApparanceParameters Parameters, int Index, float Fallback, bool& Success)
{
	Apparance::IParameterCollection* params = Parameters.Parameters.Get();
	float value = Fallback;
	if(params)
	{
		params->BeginAccess();
		Success = false;
		if(params->GetFloat( Index, &value ))
		{
			Success = true;
		}
		params->EndAccess();
	}
	return value;
}

//Access bool parameter value by index
bool UApparanceBlueprintLibrary::GetBoolParameter(FApparanceParameters Parameters, int Index, bool Fallback, bool& Success)
{
	Apparance::IParameterCollection* params = Parameters.Parameters.Get();
	bool value = Fallback;
	if(params)
	{
		params->BeginAccess();
		Success = false;
		if(params->GetBool( Index, &value ))
		{
			Success = true;
		}
		params->EndAccess();
	}
	return value;
}


//Access string parameter value by index
FString UApparanceBlueprintLibrary::GetStringParameter(FApparanceParameters Parameters, int Index, FString Fallback, bool& Success)
{
	Apparance::IParameterCollection* params = Parameters.Parameters.Get();
	FString value = Fallback;
	if(params)
	{
		params->BeginAccess();
		Success = false;
		value = ParameterCollectionGetFString( params, Index, &Fallback, &Success );
		params->EndAccess();
	}
	return value;
}

//Access colour parameter value by index
FLinearColor UApparanceBlueprintLibrary::GetColourParameter(FApparanceParameters Parameters, int Index, FLinearColor Fallback, bool& Success)
{
	Apparance::IParameterCollection* params = Parameters.Parameters.Get();
	FLinearColor value = Fallback;
	if(params)
	{
		params->BeginAccess();
		Success = false;
		Apparance::Colour colour;
		if(params->GetColour( Index, &colour ))
		{
			value = UNREALLINEARCOLOR_FROM_APPARANCECOLOUR( colour );
			Success = true;
		}
		params->EndAccess();
	}
	return value;
}

//Access vector parameter value by index
FVector UApparanceBlueprintLibrary::GetVectorParameter(FApparanceParameters Parameters, int Index, FVector Fallback, bool& Success)
{
	Apparance::IParameterCollection* params = Parameters.Parameters.Get();
	FVector value = Fallback;
	if(params)
	{
		params->BeginAccess();
		Success = false;
		Apparance::Vector3 vector;
		if(params->GetVector3( Index, &vector ))
		{
			value = UNREALVECTOR_FROM_APPARANCEVECTOR3( vector );
			Success = true;
		}
		params->EndAccess();
	}
	return value;
}

//Access frame parameter value by index
FApparanceFrame UApparanceBlueprintLibrary::GetFrameParameter( FApparanceParameters Parameters, int Index, FApparanceFrame Fallback, bool& Success )
{
	Apparance::IParameterCollection* params = Parameters.Parameters.Get();
	Apparance::Frame value;
	Fallback.GetFrame( value );
	if(params)
	{
		params->BeginAccess();
		Success = false;
		Apparance::Frame frame;
		if(params->GetFrame( Index, &frame ))
		{
			value = frame;
			Success = true;
		}
		params->EndAccess();
	}
	FApparanceFrame fr;
	fr.SetFrame( value );
	return fr;
}

// handles parameter collection list parameter getting to members of any passed structure
//
void UApparanceBlueprintLibrary::GetStructParameter_Generic(FApparanceParameters Params, int Index, FStructProperty* StructType, void* StructPtr)
{
	//get/prep list we are setting
	Apparance::IParameterCollection* p = Params.Parameters.Get();
	p->BeginAccess();
	//access list
	const Apparance::IParameterCollection* list = p->GetList(Index);
	if (list)
	{
		list->BeginAccess();

		//defer to paramcollection->struct handler
		//  can in turn defer to paramcollection->array handler, etc.
		PopulateStructFromParameterCollection(StructType, StructPtr, list);

		//done, cleanup & apply
		list->EndAccess();
	}
	p->EndAccess();
}

// handles parameter collection list parameter getting to items in any passed array
//
void UApparanceBlueprintLibrary::GetArrayParameter_Generic(FApparanceParameters Params, int Index, FArrayProperty* ArrayType, void* ArrayPtr)
{
	//get/prep list we are setting
	Apparance::IParameterCollection* p = Params.Parameters.Get();
	p->BeginAccess();
	//access list
	const Apparance::IParameterCollection* list = p->GetList(Index);
	if (list)
	{
		int list_size = list->BeginAccess();

		//defer to paramcollection->struct handler
		//  can in turn defer to paramcollection->array handler, etc
		PopulateArrayFromParameterCollection(ArrayType, ArrayPtr, list, list_size);

		//done, cleanup & apply
		list->EndAccess();
	}
	p->EndAccess();
}



//Modify integer parameter value by index
void UApparanceBlueprintLibrary::SetIntegerParameter(FApparanceParameters Parameters, int Index, int Value, bool& Success)
{
	Apparance::IParameterCollection* params = Parameters.Parameters.Get();
	Success = false;
	if(params)
	{
		params->BeginEdit();
		if(params->SetInteger( Index, Value ))
		{
			Success = true;
		}
		params->EndEdit();
	}
}

//Modify float parameter value by index
void UApparanceBlueprintLibrary::SetFloatParameter(FApparanceParameters Parameters, int Index, float Value, bool& Success)
{
	Apparance::IParameterCollection* params = Parameters.Parameters.Get();
	Success = false;
	if(params)
	{
		params->BeginEdit();
		if(params->SetFloat( Index, Value ))
		{
			Success = true;
		}
		params->EndEdit();
	}
}

//Modify bool parameter value by index
void UApparanceBlueprintLibrary::SetBoolParameter(FApparanceParameters Parameters, int Index, bool Value, bool& Success)
{
	Apparance::IParameterCollection* params = Parameters.Parameters.Get();
	Success = false;
	if(params)
	{
		params->BeginEdit();
		if(params->SetBool( Index, Value ))
		{
			Success = true;
		}
		params->EndEdit();
	}
}

//Modify string parameter value by index
void UApparanceBlueprintLibrary::SetStringParameter(FApparanceParameters Parameters, int Index, FString Value, bool& Success)
{
	Apparance::IParameterCollection* params = Parameters.Parameters.Get();
	Success = false;
	if(params)
	{
		params->BeginEdit();
		//pointer to text
		const TCHAR* pbuffer = Value.GetCharArray().GetData();
		int Len = Value.Len();
		//set
		if(params->SetString( Index, Len, pbuffer ))
		{
			Success = true;
		}
		params->EndEdit();
	}
}

//Modify colour parameter value by index
void UApparanceBlueprintLibrary::SetColourParameter(FApparanceParameters Parameters, int Index, FLinearColor Value, bool& Success)
{
	Apparance::IParameterCollection* params = Parameters.Parameters.Get();
	Success = false;
	if(params)
	{
		params->BeginEdit();
		Apparance::Colour colour = APPARANCECOLOUR_FROM_UNREALLINEARCOLOR( Value );
		if(params->SetColour( Index, &colour ))
		{
			Success = true;
		}
		params->EndEdit();
	}
}

//Modify vector parameter value by index
void UApparanceBlueprintLibrary::SetVectorParameter(FApparanceParameters Parameters, int Index, FVector Value, bool& Success)
{
	Apparance::IParameterCollection* params = Parameters.Parameters.Get();
	Success = false;
	if(params)
	{
		params->BeginEdit();
		Apparance::Vector3 vector = APPARANCEVECTOR3_FROM_UNREALVECTOR( Value );
		if(params->SetVector3( Index, &vector ))
		{
			Success = true;
		}
		params->EndEdit();
	}
}

//Modify frame parameter value by index
void UApparanceBlueprintLibrary::SetFrameParameter( FApparanceParameters Parameters, int Index, FApparanceFrame Value, bool& Success )
{
	Apparance::IParameterCollection* params = Parameters.Parameters.Get();
	Success = false;
	if(params)
	{
		params->BeginEdit();
		Apparance::Frame frame;
		Value.GetFrame( frame );
		if(params->SetFrame( Index, &frame ))
		{
			Success = true;
		}
		params->EndEdit();
	}
}


// handles parameter collection list parameter setting from members of any passed structure
//
void UApparanceBlueprintLibrary::SetStructParameter_Generic(FApparanceParameters Params, int Index, FStructProperty* StructType, void* StructPtr)
{
	//get/prep list we are setting
	Apparance::IParameterCollection* p = Params.Parameters.Get();
	if (p)
	{
		p->BeginEdit();
		//ensure fresh list
		p->DeleteParameter(Index);
		p->InsertParameter(Index, Apparance::Parameter::List, 0);
		//prep
		Apparance::IParameterCollection* list = p->SetList(Index);
		if (list)
		{
			list->BeginEdit();

			//defer to struct->paramcollection handler
			//  can in turn defer to array->paramcollection handler, etc.
			PopulateParameterCollectionFromStruct(list, StructType, StructPtr);

			//done, cleanup & apply
			list->EndEdit();
		}
		p->EndEdit();
	}
}

// handles parameter collection list parameter setting from items in any passed array
//
void UApparanceBlueprintLibrary::SetArrayParameter_Generic(FApparanceParameters Params, int Index, FArrayProperty* ArrayType, void* ArrayPtr)
{
	//get/prep list we are setting
	Apparance::IParameterCollection* p = Params.Parameters.Get();
	if (p)
	{
		p->BeginEdit();
		//ensure fresh list
		p->DeleteParameter(Index);
		p->InsertParameter(Index, Apparance::Parameter::List, 0);
		//prep
		Apparance::IParameterCollection* list = p->SetList(Index);
		if (list)
		{
			list->BeginEdit();

			//defer to struct->paramcollection handler
			//  can in turn defer to array->paramcollection handler, etc.
			PopulateParameterCollectionFromArray(list, ArrayType, ArrayPtr);

			//done, cleanup & apply
			list->EndEdit();
		}
		p->EndEdit();
	}
}



///////////////////////////////// GLOBAL UTILS //////////////////////////////////

// set specific integer parameter value of an entity
//
bool UApparanceBlueprintLibrary::IsGameRunning()
{
	return FApparanceUnrealModule::GetModule()->IsGameRunning();
}


bool UApparanceBlueprintLibrary::IsBusy()
{
#if 0 //not available yet
	if(FApparanceUnrealModule::GetModule()->GetEngine()->IsBusy())
	{
		return true;
	}
#endif
#if TIMESLICE_GEOMETRY_ADD_REMOVE
	if(Apparance_IsPendingGeometryAdd())
	{
		return true;
	}
#endif

	//not busy, or can't tell
	return false;
}

void UApparanceBlueprintLibrary::SetTimeslice( float MaxMilliseconds )
{
#if TIMESLICE_GEOMETRY_ADD_REMOVE
	Apparance_SetTimesliceLimit( MaxMilliseconds );
#endif
}

static int nPrevPendingCount = 0;
static int nPendingCounterAdd = 0;
static int nPendingCounterDone = 0;

void UApparanceBlueprintLibrary::ResetGenerationCounter()
{
	UE_LOG( LogApparance, Display, TEXT( "Resetting Generation Counter (which previously reached %i)" ), nPendingCounterAdd + nPendingCounterDone );

	nPrevPendingCount = 0;
	nPendingCounterAdd = 0;
	nPendingCounterDone = 0;
}

int UApparanceBlueprintLibrary::GetGenerationCounter()
{
#if TIMESLICE_GEOMETRY_ADD_REMOVE
	const int count = Apparance_GetPendingCount();
	//change?
	if(count != nPrevPendingCount)
	{
		const int change = count - nPrevPendingCount;
		nPrevPendingCount = count;

		if(change>0)
		{
			//newly pended content placement request
			nPendingCounterAdd += change;
		}
		else
		{
			//newly completed content placement request
			nPendingCounterDone += -change;
		}
		//UE_LOG( LogApparance, Display, TEXT( "PENDING: %i + %i = %i" ), nPendingCounterAdd, nPendingCounterDone, nPendingCounterAdd + nPendingCounterDone );

		//any add/done that occur at the same time we don't care about
	}

	return nPendingCounterAdd+nPendingCounterDone;
#else
	//no idea, use busy state
	return IsBusy()?0.0f:1.0f;
#endif
}

