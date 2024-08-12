//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#pragma once

#include "Apparance.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Templates/Casts.h"

#include "ApparanceBlueprintLibrary.generated.h"

// wrapper type for Apparance::Frame
USTRUCT(BlueprintType, BlueprintInternalUseOnly, Category="Apparance|Frame")
struct FApparanceFrame
{
	GENERATED_BODY()
	
	UPROPERTY()
	FVector Origin;
	UPROPERTY()
	FVector Size;
	//orientation
	UPROPERTY()
	FVector XAxis;
	UPROPERTY()
	FVector YAxis;
	UPROPERTY()
	FVector ZAxis;

	FApparanceFrame()
		: Origin(FVector(0,0,0))
		, Size(FVector(1,1,1))
		, XAxis(FVector(1,0,0))
		, YAxis(FVector(0,1,0))
		, ZAxis(FVector(0,0,1)) {}

	//conversion/access
	FApparanceFrame(Apparance::Frame& frame );
	void SetFrame(Apparance::Frame& frame );
	void GetFrame(Apparance::Frame& frame_out );	
};

// wrapper for Apparance::IParameterCollection
USTRUCT(BlueprintType, BlueprintInternalUseOnly, Category = "Apparance|Parameters")
struct FApparanceParameters
{
	GENERATED_BODY()
	TSharedPtr<Apparance::IParameterCollection> Parameters;

	//access helper, get contained parameter collection, init if not present
	Apparance::IParameterCollection* EnsureParameterCollection();
};

// apparance blueprint support functions
UCLASS()
class APPARANCEUNREAL_API UApparanceBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:

	//set specific entity parameter input
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly)
	static void SetEntityIntParameter( AApparanceEntity* Entity, int ParameterID, int Value );
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly)
	static void SetEntityFloatParameter( AApparanceEntity* Entity, int ParameterID, float Value );
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly)
	static void SetEntityBoolParameter( AApparanceEntity* Entity, int ParameterID, bool Value );
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly)
	static void SetEntityStringParameter( AApparanceEntity* Entity, int ParameterID, FString Value );
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly)
	static void SetEntityColourParameter( AApparanceEntity* Entity, int ParameterID, FLinearColor Value );
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly)
	static void SetEntityVector3Parameter( AApparanceEntity* Entity, int ParameterID, FVector Value );
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly)
	static void SetEntityFrameParameter( AApparanceEntity* Entity, int ParameterID, FApparanceFrame Value );
	
	//list support : for struct
	static void SetEntityListParameter_Struct_Generic(AApparanceEntity* Entity, int ParameterID, FStructProperty* StructType, const void* StructPtr);
	UFUNCTION(BlueprintCallable, CustomThunk, meta = (BlueprintInternalUseOnly = "true", CustomStructureParam = "Value", AutoCreateRefTerm = "Value"))
	static void SetEntityListParameter_Struct(AApparanceEntity* Entity, int ParameterID, const FGenericStruct& Value);
	//custom thunk to convert arbitrary struct to type+ptr
	DECLARE_FUNCTION(execSetEntityListParameter_Struct)
	{
		P_GET_OBJECT(AApparanceEntity, Entity);
		P_GET_PROPERTY(FIntProperty, ParameterID);

		Stack.StepCompiledIn<FStructProperty>(NULL);
		FStructProperty* StructProperty = CastField<FStructProperty>(Stack.MostRecentProperty);
		void* SrcStructAddr = Stack.MostRecentPropertyAddress;
		if (!StructProperty || !SrcStructAddr)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		P_FINISH;
		P_NATIVE_BEGIN;
		SetEntityListParameter_Struct_Generic(Entity, ParameterID, StructProperty, SrcStructAddr);
		P_NATIVE_END;
	}

	//list support : for array
	static void SetEntityListParameter_Array_Generic(AApparanceEntity* Entity, int ParameterID, FArrayProperty* ArrayType, const void* ArrayPtr);
	UFUNCTION(BlueprintCallable, CustomThunk, meta = (BlueprintInternalUseOnly = "true", CustomStructureParam = "Value", AutoCreateRefTerm = "Value"))
	static void SetEntityListParameter_Array(AApparanceEntity* Entity, int ParameterID, const FGenericStruct& Value);
	//custom thunk to convert arbitrary array to type+ptr
	DECLARE_FUNCTION(execSetEntityListParameter_Array)
	{
		P_GET_OBJECT(AApparanceEntity, Entity);
		P_GET_PROPERTY(FIntProperty, ParameterID);

		Stack.StepCompiledIn<FArrayProperty>(NULL);
		FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Stack.MostRecentProperty);
		void* SrcArrayAddr = Stack.MostRecentPropertyAddress;
		if (!ArrayProperty || !SrcArrayAddr)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		P_FINISH;
		P_NATIVE_BEGIN;
		SetEntityListParameter_Array_Generic(Entity, ParameterID, ArrayProperty, SrcArrayAddr);
		P_NATIVE_END;
	}

	//set all entity parameters
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly)
	static void SetEntityParameters( AApparanceEntity* Entity, FApparanceParameters Parameters );

	//request rebuild of content (when no parameter changes)
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly)
	static void RebuildEntity( AApparanceEntity* Entity );	
	
	//set specific parameter collection parameter input
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly)
	static void SetParamsIntParameter( FApparanceParameters Params, int ParameterID, int Value );
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly)
	static void SetParamsFloatParameter( FApparanceParameters Params, int ParameterID, float Value );
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly)
	static void SetParamsBoolParameter( FApparanceParameters Params, int ParameterID, bool Value );
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly)
	static void SetParamsStringParameter( FApparanceParameters Params, int ParameterID, FString Value );
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly)
	static void SetParamsColourParameter( FApparanceParameters Params, int ParameterID, FLinearColor Value );
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly)
	static void SetParamsVector3Parameter( FApparanceParameters Params, int ParameterID, FVector Value );
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly)
	static void SetParamsFrameParameter( FApparanceParameters Params, int ParameterID, FApparanceFrame Value );

	//param list support : for struct
	static void SetParamsListParameter_Struct_Generic(FApparanceParameters Params, int ParameterID, FStructProperty* StructType, const void* StructPtr);
	UFUNCTION(BlueprintCallable, CustomThunk, meta = (BlueprintInternalUseOnly = "true", CustomStructureParam = "Value", AutoCreateRefTerm = "Value"))
	static void SetParamsListParameter_Struct(FApparanceParameters Params, int ParameterID, const FGenericStruct& Value);
	//custom thunk to convert arbitrary struct to type+ptr
	DECLARE_FUNCTION(execSetParamsListParameter_Struct)
	{
		P_GET_STRUCT(FApparanceParameters, Params);
		P_GET_PROPERTY(FIntProperty, ParameterID);

		Stack.StepCompiledIn<FStructProperty>(NULL);
		FStructProperty* StructProperty = CastField<FStructProperty>(Stack.MostRecentProperty);
		void* SrcStructAddr = Stack.MostRecentPropertyAddress;
		if (!StructProperty || !SrcStructAddr)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		P_FINISH;
		P_NATIVE_BEGIN;
		SetParamsListParameter_Struct_Generic(Params, ParameterID, StructProperty, SrcStructAddr);
		P_NATIVE_END;
	}

	//param list support : for array
	static void SetParamsListParameter_Array_Generic(FApparanceParameters Params, int ParameterID, FArrayProperty* ArrayType, const void* ArrayPtr);
	UFUNCTION(BlueprintCallable, CustomThunk, meta = (BlueprintInternalUseOnly = "true", CustomStructureParam = "Value", AutoCreateRefTerm = "Value"))
	static void SetParamsListParameter_Array(FApparanceParameters Params, int ParameterID, const FGenericStruct& Value);
	//custom thunk to convert arbitrary array to type+ptr
	DECLARE_FUNCTION(execSetParamsListParameter_Array)
	{
		P_GET_STRUCT(FApparanceParameters, Params);
		P_GET_PROPERTY(FIntProperty, ParameterID);

		Stack.StepCompiledIn<FArrayProperty>(NULL);
		FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Stack.MostRecentProperty);
		void* SrcArrayAddr = Stack.MostRecentPropertyAddress;
		if (!ArrayProperty || !SrcArrayAddr)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		P_FINISH;
		P_NATIVE_BEGIN;
		SetParamsListParameter_Array_Generic(Params, ParameterID, ArrayProperty, SrcArrayAddr);
		P_NATIVE_END;
	}

	//get specific parameter collection parameter output
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly)
	static int GetParamsIntParameter( FApparanceParameters Params, int ParameterID );
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly)
	static float GetParamsFloatParameter( FApparanceParameters Params, int ParameterID );
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly)
	static bool GetParamsBoolParameter( FApparanceParameters Params, int ParameterID );
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly)
	static FString GetParamsStringParameter( FApparanceParameters Params, int ParameterID );
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly)
	static FLinearColor GetParamsColourParameter( FApparanceParameters Params, int ParameterID );
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly)
	static FVector GetParamsVector3Parameter( FApparanceParameters Params, int ParameterID );
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly)
	static FApparanceFrame GetParamsFrameParameter( FApparanceParameters Params, int ParameterID );
	//param get list support : for struct
	static void GetParamsListParameter_Struct_Generic(FApparanceParameters Params, int ParameterID, FStructProperty* StructType, void* StructPtr);
	UFUNCTION(BlueprintCallable, CustomThunk, meta = (BlueprintInternalUseOnly = "true", CustomStructureParam = "Value", AutoCreateRefTerm = "Value"))
	static void GetParamsListParameter_Struct(FApparanceParameters Params, int ParameterID, FGenericStruct& Value);
	//custom thunk to convert arbitrary struct to type+ptr
	DECLARE_FUNCTION(execGetParamsListParameter_Struct)
	{
		P_GET_STRUCT(FApparanceParameters, Params);
		P_GET_PROPERTY(FIntProperty, ParameterID);

		Stack.StepCompiledIn<FStructProperty>(NULL);
		FStructProperty* StructProperty = CastField<FStructProperty>(Stack.MostRecentProperty);
		void* SrcStructAddr = Stack.MostRecentPropertyAddress;
		if (!StructProperty || !SrcStructAddr)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		P_FINISH;
		P_NATIVE_BEGIN;
		GetParamsListParameter_Struct_Generic(Params, ParameterID, StructProperty, SrcStructAddr);
		P_NATIVE_END;
	}

	//param get list support : for array
	static void GetParamsListParameter_Array_Generic(FApparanceParameters Params, int ParameterID, FArrayProperty* ArrayType, void* ArrayPtr);
	UFUNCTION(BlueprintCallable, CustomThunk, meta = (BlueprintInternalUseOnly = "true", CustomStructureParam = "Value", AutoCreateRefTerm = "Value"))
	static void GetParamsListParameter_Array(FApparanceParameters Params, int ParameterID, FGenericStruct& Value);
	//custom thunk to convert arbitrary array to type+ptr
	DECLARE_FUNCTION(execGetParamsListParameter_Array)
	{
		P_GET_STRUCT(FApparanceParameters, Params);
		P_GET_PROPERTY(FIntProperty, ParameterID);

		Stack.StepCompiledIn<FArrayProperty>(NULL);
		FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Stack.MostRecentProperty);
		void* SrcArrayAddr = Stack.MostRecentPropertyAddress;
		if (!ArrayProperty || !SrcArrayAddr)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		P_FINISH;
		P_NATIVE_BEGIN;
		GetParamsListParameter_Array_Generic(Params, ParameterID, ArrayProperty, SrcArrayAddr);
		P_NATIVE_END;
	}


	//frame support
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Apparance|Frame", meta=(CompactNodeTitle="Frame Transform"))
	static FMatrix GetFrameTransform(FApparanceFrame Frame );
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Apparance|Frame", meta=(CompactNodeTitle="Frame Origin"))
	static FVector GetFrameOrigin( FApparanceFrame Frame );
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Apparance|Frame", meta=(CompactNodeTitle="Frame Size"))
	static FVector GetFrameSize( FApparanceFrame Frame );
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Apparance|Frame", meta=(CompactNodeTitle="Frame Orientation"))
	static FMatrix GetFrameOrientation( FApparanceFrame Frame );
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Apparance|Frame", meta=(CompactNodeTitle="Frame Rotator"))
	static FRotator GetFrameRotator( FApparanceFrame Frame );
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Apparance|Frame", meta=(CompactNodeTitle="Frame Euler"))
	static void GetFrameEuler( FApparanceFrame Frame, float& Roll, float& Pitch, float& Yaw );
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Apparance|Frame", meta=(CompactNodeTitle="Frame"))
	static FApparanceFrame MakeFrameFromTransform(FMatrix Transform );
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Apparance|Frame", meta=(CompactNodeTitle="Frame"))
	static FApparanceFrame MakeFrameFromRotator(FVector Origin, FVector Size, FRotator Orientation );
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Apparance|Frame", meta=(CompactNodeTitle="Frame"))
	static FApparanceFrame MakeFrameFromEuler(FVector Origin, FVector Size, float Roll, float Pitch, float Yaw );
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Apparance|Frame", meta=(CompactNodeTitle="Frame"))
	static FApparanceFrame MakeFrameFromBox( FBox Box );
	
	//parameter list support
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly)
	static FApparanceParameters MakeParameters();
	
	//Create an empty parameter set
	UFUNCTION(BlueprintCallable, Category = "Apparance|Entity", meta = (CompactNodeTitle = "Parameters", ToolTip = "Create an empty parameter collection"))
	static FApparanceParameters NewParameters();

	//Access any parameters an actor was procedurally placed with
	UFUNCTION(BlueprintCallable, Category="Apparance|Entity")
	static FApparanceParameters GetPlacementParameters( AActor* Target );
	
	//Access the latest parameters an actor generated its content with
	UFUNCTION(BlueprintCallable, Category="Apparance|Entity")
	static FApparanceParameters GetCurrentParameters(AApparanceEntity* Target);
		
	//Access parameter count
	UFUNCTION(BlueprintCallable, Category="Apparance|Parameters")
	static int GetParameterCount(FApparanceParameters Parameters);
	//Access parameter type by index
	UFUNCTION(BlueprintCallable, Category="Apparance|Parameters")
	static FName GetParameterType(FApparanceParameters Parameters, int Index, bool& Success);

	//Access integer parameter value by index
	UFUNCTION(BlueprintCallable, Category="Apparance|Parameters")
	static int GetIntegerParameter(FApparanceParameters Parameters, int Index, int Fallback, bool& Success);
	//Access float parameter value by index
	UFUNCTION(BlueprintCallable, Category="Apparance|Parameters")
	static float GetFloatParameter(FApparanceParameters Parameters, int Index, float Fallback, bool& Success);
	//Access bool parameter value by index
	UFUNCTION(BlueprintCallable, Category="Apparance|Parameters")
	static bool GetBoolParameter(FApparanceParameters Parameters, int Index, bool Fallback, bool& Success);
	//Access string parameter value by index
	UFUNCTION(BlueprintCallable, Category="Apparance|Parameters")
	static FString GetStringParameter(FApparanceParameters Parameters, int Index, FString Fallback, bool& Success);
	//Access colour parameter value by index
	UFUNCTION(BlueprintCallable, Category="Apparance|Parameters")
	static FLinearColor GetColourParameter(FApparanceParameters Parameters, int Index, FLinearColor Fallback, bool& Success);
	//Access vector parameter value by index
	UFUNCTION(BlueprintCallable, Category="Apparance|Parameters")
	static FVector GetVectorParameter(FApparanceParameters Parameters, int Index, FVector Fallback, bool& Success);
	//Access frame parameter value by index
	UFUNCTION( BlueprintCallable, Category = "Apparance|Parameters" )
	static FApparanceFrame GetFrameParameter( FApparanceParameters Parameters, int Index, FApparanceFrame Fallback, bool& Success );
	//Access struct (list) parameter value by index
	static void GetStructParameter_Generic(FApparanceParameters Params, int Index, FStructProperty* StructType, void* StructPtr);
	UFUNCTION(BlueprintCallable, CustomThunk, meta = (CustomStructureParam = "Value", AutoCreateRefTerm = "Value"), Category = "Apparance|Parameters")
	static void GetStructParameter(FApparanceParameters Params, int Index, FGenericStruct& Value);
	//custom thunk to convert arbitrary struct to type+ptr
	DECLARE_FUNCTION(execGetStructParameter)
	{
		P_GET_STRUCT(FApparanceParameters, Params);
		P_GET_PROPERTY(FIntProperty, ParameterIndex);

		Stack.StepCompiledIn<FStructProperty>(NULL);
		FStructProperty* StructProperty = CastField<FStructProperty>(Stack.MostRecentProperty);
		void* SrcStructAddr = Stack.MostRecentPropertyAddress;
		if (!StructProperty || !SrcStructAddr)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		P_FINISH;
		P_NATIVE_BEGIN;
		GetStructParameter_Generic(Params, ParameterIndex, StructProperty, SrcStructAddr);
		P_NATIVE_END;
	}
	//Access array (list) parameter value by index
	static void GetArrayParameter_Generic(FApparanceParameters Params, int Index, FArrayProperty* ArrayType, void* ArrayPtr);
	UFUNCTION(BlueprintCallable, CustomThunk, meta = (CustomStructureParam = "Value", AutoCreateRefTerm = "Value"), Category = "Apparance|Parameters")
	static void GetArrayParameter(FApparanceParameters Params, int Index, FGenericStruct& Value);
	//custom thunk to convert arbitrary array to type+ptr
	DECLARE_FUNCTION(execGetArrayParameter)
	{
		P_GET_STRUCT(FApparanceParameters, Params);
		P_GET_PROPERTY(FIntProperty, ParameterIndex);

		Stack.StepCompiledIn<FArrayProperty>(NULL);
		FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Stack.MostRecentProperty);
		void* SrcArrayAddr = Stack.MostRecentPropertyAddress;
		if (!ArrayProperty || !SrcArrayAddr)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		P_FINISH;
		P_NATIVE_BEGIN;
		GetArrayParameter_Generic(Params, ParameterIndex, ArrayProperty, SrcArrayAddr);
		P_NATIVE_END;
	}


	//Modify integer parameter value by index
	UFUNCTION(BlueprintCallable, Category="Apparance|Parameters")
	static void SetIntegerParameter(FApparanceParameters Parameters, int Index, int Value, bool& Success);
	//Modify float parameter value by index
	UFUNCTION(BlueprintCallable, Category="Apparance|Parameters")
	static void SetFloatParameter(FApparanceParameters Parameters, int Index, float Value, bool& Success);
	//Modify bool parameter value by index
	UFUNCTION(BlueprintCallable, Category="Apparance|Parameters")
	static void SetBoolParameter(FApparanceParameters Parameters, int Index, bool Value, bool& Success);
	//Modify string parameter value by index
	UFUNCTION(BlueprintCallable, Category="Apparance|Parameters")
	static void SetStringParameter(FApparanceParameters Parameters, int Index, FString Value, bool& Success);
	//Modify colour parameter value by index
	UFUNCTION(BlueprintCallable, Category="Apparance|Parameters")
	static void SetColourParameter(FApparanceParameters Parameters, int Index, FLinearColor Value, bool& Success);
	//Modify vector parameter value by index
	UFUNCTION(BlueprintCallable, Category="Apparance|Parameters")
	static void SetVectorParameter(FApparanceParameters Parameters, int Index, FVector Value, bool& Success);
	//Modify frame parameter value by index
	UFUNCTION( BlueprintCallable, Category = "Apparance|Parameters" )
	static void SetFrameParameter( FApparanceParameters Parameters, int Index, FApparanceFrame Value, bool& Success );
	//Modify struct (list) parameter value by index
	static void SetStructParameter_Generic(FApparanceParameters Params, int Index, FStructProperty* StructType, void* StructPtr);
	UFUNCTION(BlueprintCallable, CustomThunk, meta = (CustomStructureParam = "Value", AutoCreateRefTerm = "Value", ToolTip = "Sets by index or appends if not enough parameters present"), Category = "Apparance|Parameters")
	static void SetStructParameter(FApparanceParameters Params, int Index, const FGenericStruct& Value);
	//custom thunk to convert arbitrary struct to type+ptr
	DECLARE_FUNCTION(execSetStructParameter)
	{
		P_GET_STRUCT(FApparanceParameters, Params);
		P_GET_PROPERTY(FIntProperty, ParameterIndex);

		Stack.StepCompiledIn<FStructProperty>(NULL);
		FStructProperty* StructProperty = CastField<FStructProperty>(Stack.MostRecentProperty);
		void* SrcStructAddr = Stack.MostRecentPropertyAddress;
		if (!StructProperty || !SrcStructAddr)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		P_FINISH;
		P_NATIVE_BEGIN;
		SetStructParameter_Generic(Params, ParameterIndex, StructProperty, SrcStructAddr);
		P_NATIVE_END;
	}
	//Modify array (list) parameter value by index
	static void SetArrayParameter_Generic(FApparanceParameters Params, int Index, FArrayProperty* ArrayType, void* ArrayPtr);
	UFUNCTION(BlueprintCallable, CustomThunk, meta = (CustomStructureParam = "Value", AutoCreateRefTerm = "Value", ToolTip = "Sets by index or appends if not enough parameters present"), Category = "Apparance|Parameters")
	static void SetArrayParameter(FApparanceParameters Params, int Index, const FGenericStruct& Value);
	//custom thunk to convert arbitrary array to type+ptr
	DECLARE_FUNCTION(execSetArrayParameter)
	{
		P_GET_STRUCT(FApparanceParameters, Params);
		P_GET_PROPERTY(FIntProperty, ParameterIndex);

		Stack.StepCompiledIn<FArrayProperty>(NULL);
		FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Stack.MostRecentProperty);
		void* SrcArrayAddr = Stack.MostRecentPropertyAddress;
		if (!ArrayProperty || !SrcArrayAddr)
		{
			Stack.bArrayContextFailed = true;
			return;
		}

		P_FINISH;
		P_NATIVE_BEGIN;
		SetArrayParameter_Generic(Params, ParameterIndex, ArrayProperty, SrcArrayAddr);
		P_NATIVE_END;
	}

	//global utils
	UFUNCTION(BlueprintCallable, Category="Apparance|Engine", meta = (ToolTip = "Are we playing a game?") )
	static bool IsGameRunning();

	UFUNCTION( BlueprintCallable, Category = "Apparance|Engine", meta = (ToolTip = "Is Apparance busy generating or placing content?") )
	static bool IsBusy();

	UFUNCTION( BlueprintCallable, Category = "Apparance|Engine", meta = (ToolTip = "Control time limit for content placement in a single frame. Set to zero for one-at-a-time. Increase during loading screen for boost.") )
	static void SetTimeslice( float MaxMilliseconds );

	UFUNCTION( BlueprintCallable, Category = "Apparance|Engine", meta = (ToolTip = "Prepare for loading progress calculations. Call before loading main level.") )
	static void ResetGenerationCounter();

	UFUNCTION( BlueprintCallable, Category = "Apparance|Engine", meta = (ToolTip = "Get generation job and pending placement count since last ResetGenerationCounter.") )
	static int GetGenerationCounter();


};
