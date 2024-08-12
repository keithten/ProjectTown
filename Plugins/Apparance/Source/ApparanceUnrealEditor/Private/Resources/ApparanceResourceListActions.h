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

class FApparanceUnrealEditorResourceListActions : public FAssetTypeActions_Base
{
	uint32 RegisteredCategoryHandle;
public:
	
	FApparanceUnrealEditorResourceListActions(uint32 InAssetCategory)
		: RegisteredCategoryHandle(InAssetCategory)
	{
#if UE_VERSION_OLDER_THAN(5,2,0) //nolonger needed, something somthing IAssetTools::IsAssetClassSupported
		SetSupported(true);
#endif
	}
	
	// IAssetTypeActions Implementation
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override;
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
	// End IAssetTypeActions
	
};
