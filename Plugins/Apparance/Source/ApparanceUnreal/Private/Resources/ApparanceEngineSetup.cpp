//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

// main
#include "ApparanceEngineSetup.h"


// unreal
//#include "Components/BoxComponent.h"
//#include "Components/BillboardComponent.h"
//#include "Components/DecalComponent.h"
//#include "UObject/ConstructorHelpers.h"
//#include "Engine/Texture2D.h"
//#include "ProceduralMeshComponent.h"
//#include "Engine/World.h"
//#include "Components/InstancedStaticMeshComponent.h"
#include "Misc/Paths.h"

// module
#include "ApparanceUnreal.h"
#include "AssetDatabase.h"
//#include "Geometry.h"

#define LOCTEXT_NAMESPACE "ApparanceUnreal"

//////////////////////////////////////////////////////////////////////////
// UApparanceEngineSetup

UApparanceEngineSetup::UApparanceEngineSetup()
{
	//defaults
	ProceduresDirectory.Path = "Apparance/Procedures/";	
	Editor_MissingMaterial = FSoftObjectPath("/ApparanceUnreal/Materials/ApparanceMissingMaterial.ApparanceMissingMaterial");
	Standalone_MissingMaterial = FSoftObjectPath("/ApparanceUnreal/Materials/ApparanceFallbackMaterial.ApparanceFallbackMaterial");
	Editor_MissingTexture = FSoftObjectPath("/ApparanceUnreal/Textures/ApparanceMissingTexture.ApparanceMissingTexture");
	Standalone_MissingTexture = FSoftObjectPath("/ApparanceUnreal/Textures/ApparanceFallbackTexture.ApparanceFallbackTexture");
	Editor_MissingObject = FSoftObjectPath("/ApparanceUnreal/Meshes/ApparanceMissingObject.ApparanceMissingObject");
	Standalone_MissingObject = FSoftObjectPath("/ApparanceUnreal/Meshes/ApparanceFallbackObject.ApparanceFallbackObject");	
}

//context dependent var selection
#if WITH_EDITOR
#define APPARANCESETUPVAR(x) GetDefault<UApparanceEngineSetup>()->Editor_##x
#else
#define APPARANCESETUPVAR(x) GetDefault<UApparanceEngineSetup>()->Standalone_##x
#endif

FText UApparanceEngineSetup::GetSetupContext()
{
#if WITH_EDITOR
	return LOCTEXT("SetupContextEditor", "Editor");
#else
	return LOCTEXT("SetupContextStandalone", "Standalone");
#endif
}

FString UApparanceEngineSetup::GetProceduresDirectory()
{
	return GetDefault<UApparanceEngineSetup>()->ProceduresDirectory.Path;	//not context dependant
}
bool UApparanceEngineSetup::DoesProceduresDirectoryExist()
{
	FString proc_path = FPaths::Combine( FPaths::ProjectContentDir(), GetProceduresDirectory() );
	FPaths::NormalizeDirectoryName( proc_path );
	FPaths::CollapseRelativeDirectories( proc_path );
	return FPaths::DirectoryExists( proc_path );
}
void UApparanceEngineSetup::SetProceduresDirectory( FString path )
{
	UApparanceEngineSetup* pes = GetMutableDefault<UApparanceEngineSetup>();

	pes->ProceduresDirectory.Path = path;

#if UE_VERSION_AT_LEAST(5,4,0)
	auto* pprop = pes->GetClass()->FindPropertyByName(FName("ProceduresDirectory"));
	pes->UpdateSinglePropertyInConfigFile(pprop, pes->GetDefaultConfigFilename());
#else
	pes->SaveConfig();
#endif
}
UApparanceResourceList* UApparanceEngineSetup::GetResourceRoot()
{
	return GetDefault<UApparanceEngineSetup>()->ResourceRoot.LoadSynchronous();
}
void UApparanceEngineSetup::SetResourceRoot( UApparanceResourceList* root )
{
	UApparanceEngineSetup* pes = GetMutableDefault<UApparanceEngineSetup>();

	pes->ResourceRoot = root;

#if UE_VERSION_AT_LEAST(5,4,0)
	auto* pprop = pes->GetClass()->FindPropertyByName(FName("ResourceRoot"));
	pes->UpdateSinglePropertyInConfigFile(pprop, pes->GetDefaultConfigFilename());
#else
	pes->SaveConfig();
#endif
}

int UApparanceEngineSetup::GetThreadCount()
{
	return APPARANCESETUPVAR(ThreadCount);
}
int UApparanceEngineSetup::GetBufferSize()
{
	return APPARANCESETUPVAR(BufferSize);
}
bool UApparanceEngineSetup::GetEnableLiveEditing()
{
	return APPARANCESETUPVAR(bEnableLiveEditing);
}
EApparanceInstancingMode UApparanceEngineSetup::GetInstancedRenderingMode()
{
	return APPARANCESETUPVAR(EnableInstancedRendering);
}
UMaterial* UApparanceEngineSetup::GetMissingMaterial()
{
	return APPARANCESETUPVAR( MissingMaterial.LoadSynchronous());
}
UTexture* UApparanceEngineSetup::GetMissingTexture()
{
	return APPARANCESETUPVAR( MissingTexture.LoadSynchronous());
}
UStaticMesh* UApparanceEngineSetup::GetMissingObject()
{
	return APPARANCESETUPVAR(MissingObject.LoadSynchronous());
}



#if WITH_EDITOR
void UApparanceEngineSetup::PostEditChangeProperty(struct FPropertyChangedEvent& e)
{
	FName PropertyName = (e.Property != NULL) ? e.Property->GetFName() : NAME_None;
	FName MemberPropertyName = (e.Property != NULL) ? e.MemberProperty->GetFName() : NAME_None;
	
	//changes to these settings invalidates built objects
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UApparanceEngineSetup, Editor_EnableInstancedRendering)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UApparanceEngineSetup, Editor_MissingMaterial)		
		|| PropertyName == GET_MEMBER_NAME_CHECKED(UApparanceEngineSetup, Editor_MissingObject))
	{
		//need to:
		//1. clear any cached versions of these assets (for material changes)
		//2. cause rebuild (for all changes)
		auto db = FApparanceUnrealModule::GetModule()->GetAssetDatabase();
		if (db)
		{
			db->Invalidate();
		}
	}

	//default
	Super::PostEditChangeProperty(e);
}
#endif

#undef APPARANCESETUPVAR
#undef LOCTEXT_NAMESPACE
