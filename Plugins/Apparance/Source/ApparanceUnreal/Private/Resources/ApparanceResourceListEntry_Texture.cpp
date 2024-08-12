//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2021 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

// main
#include "ResourceList/ApparanceResourceListEntry_Texture.h"


// unreal
//#include "Components/BoxComponent.h"
//#include "Components/BillboardComponent.h"
//#include "Components/DecalComponent.h"
//#include "UObject/ConstructorHelpers.h"
//#include "Engine/Texture2D.h"
//#include "ProceduralMeshComponent.h"
//#include "Engine/World.h"
//#include "Components/InstancedStaticMeshComponent.h"

// module
#include "ApparanceUnreal.h"
#include "AssetDatabase.h"
//#include "Geometry.h"


#define LOCTEXT_NAMESPACE "ApparanceUnreal"

//////////////////////////////////////////////////////////////////////////
// UApparanceResourceListEntry_Texture

FName UApparanceResourceListEntry_Texture::GetIconName( bool is_open )
{
	return StaticIconName( is_open );
}

FName UApparanceResourceListEntry_Texture::StaticIconName( bool is_open )
{
	return FName( "ClassIcon.Texture2D" );
}

FText UApparanceResourceListEntry_Texture::GetAssetTypeName() const
{
	return UApparanceResourceListEntry_Texture::StaticAssetTypeName();
}

FText UApparanceResourceListEntry_Texture::StaticAssetTypeName()
{
	return LOCTEXT( "TextureAssetTypeName", "Texture");
}

#undef LOCTEXT_NAMESPACE