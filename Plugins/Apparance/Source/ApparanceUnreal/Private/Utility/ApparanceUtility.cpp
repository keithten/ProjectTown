//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

//main
#include "Utility/ApparanceUtility.h"
#include "ApparanceUnrealVersioning.h"

//unreal
//#include "Core.h"
//#include "Editor.h"
//#include "Editor/EditorPerformanceSettings.h"

//module


DEFINE_LOG_CATEGORY(LogUtility);

///////////// METADATA UTILS ////////////

// separator search util
//
int FApparanceUtility::NextMetadataSeparator(const FString& s, int start)
{
	if(start >= 0 && start < s.Len())
	{
		int i = start;
		while(i < s.Len())
		{
			TCHAR c = s[i];
			if(c == '|'		//is separator
				&& (i == 0 || s[i - 1] != '\\'))	//skip escaped separators
			{
				return i;
			}
			i++;
		}
	}
	return -1;
}

// get one of the separated sections by index
// 0 = name
// 1 = description
// 2+ = metadata fields
//
FString FApparanceUtility::GetMetadataSection(const FString& s, int index)
{
	int isection = 0;
	int icur = 0;
	while (icur < s.Len())
	{
		//span of section
		int inext = NextMetadataSeparator(s, icur);
		if (inext == -1)
		{
			inext = s.Len();
		}

		//want this one?
		if (isection == index)
		{
			return s.Mid(icur, inext-icur);
		}

		//next
		icur = inext + 1;
		isection++;
	}

	//not found
	return "";
}

// search metadata text for a key/value pair
//
bool FApparanceUtility::FindMetadataValue(const FString& metadata, const FString& key, FString& out_value)
{
	int isection = 2;	//skip name/desc
	while (true)
	{
		FString section = GetMetadataSection(metadata, isection);

		//run out?
		if (section.IsEmpty())
		{
			break;
		}

		//right one?
		int index;
		if (section.FindChar(TCHAR('='), index))
		{
			//this our key?
			FString k = section.Left(index);
			if (k.Equals(key, ESearchCase::IgnoreCase))
			{
				//extract value
				out_value = section.RightChop(index + 1);
				return true;
			}
		}

		isection++;
	}

	//not found
	return false;
}


// attempt to find a float metadata value encoded in the name of the parameter
bool FApparanceUtility::TryGetMetadataValue(const char* name, const char* metadata, float& out_value)
{
	//contains it at all?
	const char* pn = FCStringAnsi::Strifind(metadata, name, true);
	if (pn)
	{
		//prep for proper extraction
		FString fsname = APPARANCE_UTF8_TO_TCHAR( (UTF8CHAR*)name );
		FString fsmeta = APPARANCE_UTF8_TO_TCHAR( (UTF8CHAR*)metadata );
		
		FString fsvalue;
		if (FindMetadataValue(fsmeta, fsname, fsvalue))
		{
			//convert
			out_value = FCString::Atof(*fsvalue);
			return true;
		}
	}
	return false;
}

// attempt to find a int metadata value encoded in the name of the parameter
bool FApparanceUtility::TryGetMetadataValue(const char* name, const char* metadata, int& out_value)
{
	//contains it at all?
	const char* pn = FCStringAnsi::Strifind(metadata, name, true);
	if (pn)
	{
		//prep for proper extraction
		FString fsname = APPARANCE_UTF8_TO_TCHAR( (UTF8CHAR*)name );
		FString fsmeta = APPARANCE_UTF8_TO_TCHAR( (UTF8CHAR*)metadata );
		
		FString fsvalue;
		if (FindMetadataValue(fsmeta, fsname, fsvalue))
		{
			//convert
			out_value = FCString::Atoi(*fsvalue);
			return true;
		}
	}
	return false;
}

// attempt to find a bool metadata value encoded in the name of the parameter
bool FApparanceUtility::TryGetMetadataValue(const char* name, const char* metadata, bool& out_value)
{
	//contains it at all?
	const char* pn = FCStringAnsi::Strifind(metadata, name, true);
	if (pn)
	{
		//prep for proper extraction
		FString fsname = APPARANCE_UTF8_TO_TCHAR( (UTF8CHAR*)name );
		FString fsmeta = APPARANCE_UTF8_TO_TCHAR( (UTF8CHAR*)metadata );
		
		FString fsvalue;
		if (FindMetadataValue(fsmeta, fsname, fsvalue))
		{
			//convert
			out_value = FCString::Stricmp(*fsvalue, TEXT("true")) == 0;
			return true;
		}
	}
	return false;
}


// search param hierarchy for a metadata value for a given parameter
//
float FApparanceUtility::FindMetadataFloat( 
	const TArray<const Apparance::IParameterCollection*>* prop_hierarchy,
	Apparance::ValueID input_id,
	const char* metadata_name, 
	float fallback_value,
	bool* is_found )
{
	float value = fallback_value;
	bool found = false;
	
	//attempt to obtain current value
	for (int i = 0; i < prop_hierarchy->Num(); i++)
	{
		const Apparance::IParameterCollection* params = prop_hierarchy->operator[]( i );
		const char* full_param_name_and_desc = params->FindName(input_id);
		if(full_param_name_and_desc)
		{
			//check
			if (TryGetMetadataValue( metadata_name, full_param_name_and_desc, value))
			{
				//found
				found = true;
				break;
			}
		}
	}
	
	if (is_found)
	{
		*is_found = found;
	}	
	return value;	
}

// search param hierarchy for a metadata value for a given parameter
//
int FApparanceUtility::FindMetadataInteger( 
	const TArray<const Apparance::IParameterCollection*>* prop_hierarchy,
	Apparance::ValueID input_id,
	const char* metadata_name, 
	int fallback_value,
	bool* is_found )
{
	int value = fallback_value;
	bool found = false;
	
	//attempt to obtain current value
	for (int i = 0; i < prop_hierarchy->Num(); i++)
	{
		const Apparance::IParameterCollection* params = prop_hierarchy->operator[]( i );
		const char* full_param_name_and_desc = params->FindName(input_id);
		if(full_param_name_and_desc)
		{
			//check
			if (TryGetMetadataValue( metadata_name, full_param_name_and_desc, value))
			{
				//found
				found = true;
				break;
			}
		}
	}
	
	if (is_found)
	{
		*is_found = found;
	}	
	return value;	
}

// search param hierarchy for a metadata value for a given parameter
//
bool FApparanceUtility::FindMetadataBool( 
	const TArray<const Apparance::IParameterCollection*>* prop_hierarchy,
	Apparance::ValueID input_id,
	const char* metadata_name, 
	bool fallback_value ,
	bool count_missing_as_false ,
	bool* is_found )
{
	bool value = fallback_value;
	bool found = false;
	
	//attempt to obtain current value
	for (int i = 0; i < prop_hierarchy->Num(); i++)
	{
		const Apparance::IParameterCollection* params = prop_hierarchy->operator[]( i );
		const char* full_param_name_and_desc = params->FindName(input_id);
		if(full_param_name_and_desc)
		{
			//check
			if (TryGetMetadataValue( metadata_name, full_param_name_and_desc, value))
			{
				//found
				found = true;
				break;
			}
		}
	}
	
	if(!found && count_missing_as_false)
	{
		found = true;
		value = false;
	}	
	
	if (is_found)
	{
		*is_found = found;
	}
	return value;	
}
