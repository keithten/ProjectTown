//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#pragma once

#include "AssetTypeActions_Base.h"
//unreal:misc
#include "Misc/EngineVersionComparison.h"

class FApparanceUnrealEditorEntityPresetActions : public FAssetTypeActions_Base
{
	uint32 Category;
public:
	
	FApparanceUnrealEditorEntityPresetActions(uint32 InAssetCategory)
		: Category(InAssetCategory)
	{
#if UE_VERSION_OLDER_THAN(5,2,0) //nolonger needed, something somthing IAssetTools::IsAssetClassSupported
		SetSupported( true );
#endif
	}
	
	virtual FText GetName() const override
	{
		return FText::FromString("Apparance Entity Preset");
	}
	virtual FColor GetTypeColor() const override
	{
		return FColor::Emerald;
	}
	virtual UClass* GetSupportedClass() const override
	{
		return UApparanceEntityPreset::StaticClass();
	}
	virtual uint32 GetCategories() override
	{
		return Category;
	}
};
