//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#pragma once

// unreal
#include "Engine/EngineTypes.h"
#include "Materials/Material.h"
#include "Engine/StaticMesh.h"

// apparance

// module

// auto (last)
#include "ApparanceEngineSetup.generated.h"

//instancing options for the project
UENUM()
enum class EApparanceInstancingMode
{
	Never,
	PerEntity,
	Always,
};


// Engine setup definition
//
UCLASS(config=Engine, defaultconfig)
class APPARANCEUNREAL_API UApparanceEngineSetup
	: public UObject
{
	GENERATED_BODY()

public:
	UApparanceEngineSetup();

	//------------------------------------------------------------------------
	// General/default setup
	
	UPROPERTY(EditAnywhere, config, Category=Default, meta = (ContentDir, Tooltip = "Path to store procedures in, relative to project Content folder. Changing this requires manual movement of any existing procedure files.", ConfigRestartRequired=true));
	FDirectoryPath ProceduresDirectory;

	UPROPERTY(EditAnywhere, config, Category=Default, meta = (Tooltip = "Asset to use as root resource list needed for asset lookup", ConfigRestartRequired=true));
	TSoftObjectPtr<class UApparanceResourceList> ResourceRoot;

	//------------------------------------------------------------------------
	// Editor setup

	UPROPERTY(EditAnywhere, config, Category=Editor, meta = (DisplayName="Thread Count", Tooltip = "Number of cores to make available for background procedural generation (0 automatic)", ConfigRestartRequired=true));
	int Editor_ThreadCount = 4;
	
	UPROPERTY(EditAnywhere, config, Category=Editor, meta = (DisplayName="Buffer Size", Tooltip = "Synthesis buffer size for each thread (MB), more space is needed for more complex procedures", ConfigRestartRequired=true));
	int Editor_BufferSize = 100;
	
	UPROPERTY(EditAnywhere, config, Category=Editor, meta = (DisplayName="Enable Live Editing", Tooltip = "Run the engine as remote edit server so the Apparance Editor can connect for live interactive editing", ConfigRestartRequired=true));
	bool Editor_bEnableLiveEditing = true;
	
	UPROPERTY(EditAnywhere, config, Category=Editor, meta = (DisplayName="Enable Instanced Rendering", Tooltip = "Whether to allow the use of instanced rendering to display generated content (may need to be enabled per Entity too)."));
	EApparanceInstancingMode Editor_EnableInstancedRendering = EApparanceInstancingMode::PerEntity;
	
	UPROPERTY(EditAnywhere, config, Category=Editor, meta = (DisplayName="Missing Material", Tooltip = "Material to place on objects when the material resource requested isn't assigned, doesn't have a Resource List entry, or is missing."));
	TSoftObjectPtr<UMaterial> Editor_MissingMaterial;

	UPROPERTY(EditAnywhere, config, Category=Editor, meta = (DisplayName="Missing Texture", Tooltip = "Texture to bind to materials when the material resource requested isn't assigned, doesn't have a Resource List entry, or is missing."));
	TSoftObjectPtr<UTexture> Editor_MissingTexture;
	
	UPROPERTY(EditAnywhere, config, Category=Editor, meta = (DisplayName="Missing Object", Tooltip = "Object to place when a mesh or blueprint resource requested isn't assigned, doesn't have a Resource List entry, or is missing."));
	TSoftObjectPtr<UStaticMesh> Editor_MissingObject;

	//------------------------------------------------------------------------
	// Standalone setup

	UPROPERTY(EditAnywhere, config, Category=Standalone, meta = (DisplayName="Thread Count", Tooltip = "Number of cores to make available for background procedural generation (0 automatic)", ConfigRestartRequired=true));
	int Standalone_ThreadCount = 4;
	
	UPROPERTY(EditAnywhere, config, Category=Standalone, meta = (DisplayName="Buffer Size", Tooltip = "Synthesis buffer size for each thread (MB), more space is needed for more complex procedures", ConfigRestartRequired=true));
	int Standalone_BufferSize = 100;
	
	UPROPERTY(EditAnywhere, config, Category=Standalone, meta = (DisplayName="Enable Live Editing", Tooltip = "Run the engine as remote edit server so the Apparance Editor can connect for live interactive editing", ConfigRestartRequired=true));
	bool Standalone_bEnableLiveEditing = false;
	
	UPROPERTY(EditAnywhere, config, Category=Standalone, meta = (DisplayName="Enable Instanced Rendering", Tooltip = "Whether to allow the use of instanced rendering to display generated content (may need to be enabled per Entity too)."));
	EApparanceInstancingMode Standalone_EnableInstancedRendering = EApparanceInstancingMode::PerEntity;
	
	UPROPERTY(EditAnywhere, config, Category=Standalone, meta = (DisplayName="Missing Material", Tooltip = "Material to place on objects when the material resource requested isn't assigned, doesn't have a Resource List entry, or is missing."));
	TSoftObjectPtr<UMaterial> Standalone_MissingMaterial;

	UPROPERTY(EditAnywhere, config, Category=Standalone, meta = (DisplayName="Missing Texture", Tooltip = "Texture to bind to materials when the material resource requested isn't assigned, doesn't have a Resource List entry, or is missing."));
	TSoftObjectPtr<UTexture> Standalone_MissingTexture;
	
	UPROPERTY(EditAnywhere, config, Category=Standalone, meta = (DisplayName="Missing Object", Tooltip = "Object to place when a mesh or blueprint resource requested isn't assigned, doesn't have a Resource List entry, or is missing."));
	TSoftObjectPtr<UStaticMesh> Standalone_MissingObject;
	

	// access
	static FText GetSetupContext();
	static FString GetProceduresDirectory();
	static void SetProceduresDirectory( FString path );
	static bool DoesProceduresDirectoryExist();
	static UApparanceResourceList* GetResourceRoot();
	static void SetResourceRoot( UApparanceResourceList* root );
	// engine
	static int GetThreadCount();
	static int GetBufferSize();
	static bool GetEnableLiveEditing();
	static EApparanceInstancingMode GetInstancedRenderingMode();
	static UMaterial* GetMissingMaterial();
	static UTexture* GetMissingTexture();
	static UStaticMesh* GetMissingObject();
	
public:
#if WITH_EDITOR
	void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent);
#endif
	
};

