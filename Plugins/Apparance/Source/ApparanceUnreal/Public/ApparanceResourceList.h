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
#include "ResourceList/ApparanceResourceListEntry.h"
#include "ResourceList/ApparanceResourceListEntry_Asset.h"
#include "ResourceList/ApparanceResourceListEntry_3DAsset.h"
#include "ResourceList/ApparanceResourceListEntry_StaticMesh.h"
#include "ResourceList/ApparanceResourceListEntry_Material.h"
#include "ResourceList/ApparanceResourceListEntry_Texture.h"
#include "ResourceList/ApparanceResourceListEntry_Component.h"
#include "ResourceList/ApparanceResourceListEntry_Blueprint.h"
#include "ResourceList/ApparanceResourceListEntry_ResourceList.h"
#include "ResourceList/ApparanceResourceListEntry_Variants.h"

// auto (last)
#include "ApparanceResourceList.generated.h"

// Resource List asset
//
UCLASS(config = Engine)
class APPARANCEUNREAL_API UApparanceResourceList
	: public UObject
{
	GENERATED_BODY()

public:
	//root of resource tree (root provides category)
	UPROPERTY(EditAnywhere, Category = Apparance )
	class UApparanceResourceListEntry*  Resources;

	//other resource lists to also search
	UPROPERTY(EditAnywhere, Category = Apparance )
	class UApparanceResourceListEntry*	References;


	//FAssetDatabase access
	const class UApparanceResourceListEntry* FindResourceEntry(const FString& asset_descriptor) const;

	//~ Begin UObject Interface
	virtual void PostLoad() override;
	//~ End UObject Interface

public:
	//editing
	void Editor_InitForEditing();
	void Editor_NotifyStructuralChange( class UApparanceResourceListEntry* pobject, FName property_name );

private:
	
	static UApparanceResourceListEntry* SearchHierarchy( UApparanceResourceListEntry* pentry, const FString& asset_descriptor, int descriptor_index );

	void FixupOwnership( class UApparanceResourceListEntry* p );

};

