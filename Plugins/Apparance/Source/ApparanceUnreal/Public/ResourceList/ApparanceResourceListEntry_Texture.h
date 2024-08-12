//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2021 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#pragma once

// unreal
#include "CoreMinimal.h"
#include "Engine/Texture.h"

// apparance
#include "Apparance.h"

// module
#include "ApparanceResourceListEntry_Asset.h"

// auto (last)
#include "ApparanceResourceListEntry_Texture.generated.h"


// entry for mapping resource name to texture asset
//
UCLASS()
class APPARANCEUNREAL_API UApparanceResourceListEntry_Texture
	: public UApparanceResourceListEntry_Asset
{
	GENERATED_BODY()

public:

public:
	UTexture* GetTexture() const { return Cast<UTexture>( GetAsset() ); };

public:
	
	//access
	virtual UClass* GetAssetClass() const override { return UTexture::StaticClass(); }
	virtual FText GetAssetTypeName() const override;
	virtual FName GetIconName( bool is_open=false ) override;

	static FText StaticAssetTypeName();
	static FName StaticIconName( bool is_open=false );
	
};

