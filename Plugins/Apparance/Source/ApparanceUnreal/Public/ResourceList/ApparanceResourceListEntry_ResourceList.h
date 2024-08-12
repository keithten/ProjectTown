//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#pragma once

// unreal
#include "CoreMinimal.h"

// apparance
#include "Apparance.h"

// module
#include "ApparanceResourceListEntry_Asset.h"

// auto (last)
#include "ApparanceResourceListEntry_ResourceList.generated.h"


// entry for referencing other resource lists
//
UCLASS()
class APPARANCEUNREAL_API UApparanceResourceListEntry_ResourceList
	: public UApparanceResourceListEntry_Asset
{
	GENERATED_BODY()
	
public:
	class UApparanceResourceList* GetResourceList() const;

public:
	
	//editing
	virtual void SetName( FString name ) override;
	
	//access
	virtual bool IsNameEditable() const { return false; }
	virtual FString GetName() const override;
	virtual UClass* GetAssetClass() const override;
	virtual FText GetAssetTypeName() const override;
	virtual FName GetIconName( bool is_open=false ) override;

	static FText StaticAssetTypeName();
	static FName StaticIconName( bool is_open=false );
};

