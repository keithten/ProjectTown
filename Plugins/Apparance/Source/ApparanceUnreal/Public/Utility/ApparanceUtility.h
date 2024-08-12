//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#pragma once

//shared header for utility

//unreal
#include "CoreMinimal.h"

//module
#include "Apparance.h"

// general declarations
DECLARE_LOG_CATEGORY_EXTERN(LogUtility, Log, All);

//utility functions
struct APPARANCEUNREAL_API FApparanceUtility
{
	//metadata string parsing helpers
	static int NextMetadataSeparator(const FString& s, int start);
	static FString GetMetadataSection(const FString& s, int index);
	static bool FindMetadataValue(const FString& metadata, const FString& key, FString& out_value);
	
	//attempt known value extraction
	static bool TryGetMetadataValue(const char* name, const char* metadata, float& out_value);
	static bool TryGetMetadataValue(const char* name, const char* metadata, int& out_value);
	static bool TryGetMetadataValue(const char* name, const char* metadata, bool& out_value);
		
	//extract known type of metadata with fallback
	static float FindMetadataFloat( 
		const TArray<const Apparance::IParameterCollection*>* prop_hierarchy,
		Apparance::ValueID input_id,
		const char* metadata_name, 
		float fallback_value,
		bool* is_found = nullptr);
	static int FindMetadataInteger( 
		const TArray<const Apparance::IParameterCollection*>* prop_hierarchy,
		Apparance::ValueID input_id,
		const char* metadata_name, 
		int fallback_value,
		bool* is_found = nullptr);
	static bool FindMetadataBool( 
		const TArray<const Apparance::IParameterCollection*>* prop_hierarchy,
		Apparance::ValueID input_id,
		const char* metadata_name, 
		bool fallback_value = false,
		bool count_missing_as_false = true,
		bool* is_found = nullptr);
};
