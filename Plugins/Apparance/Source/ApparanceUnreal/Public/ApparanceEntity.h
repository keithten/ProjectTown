//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#pragma once

// unreal
#include "GameFramework/Actor.h"
#include "Components/InstancedStaticMeshComponent.h"

// apparance
#include "Apparance.h"

// module
#include "IProceduralObject.h"
#include "ApparanceEntityPreset.h"
#include "Utility/ApparanceConversion.h"

// auto (last)
#include "ApparanceEntity.generated.h"

//nolonger support native unreal spatial mapping/scale
#define APPARANCE_ENABLE_SPATIAL_MAPPING 0


USTRUCT()
struct FMeshCacheEntry
{
	GENERATED_BODY()
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<class UStaticMeshComponent>> Meshes;
};

USTRUCT()
struct FActorComponentCacheEntry
{
	GENERATED_BODY()
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<class UActorComponent>> Components;
};

USTRUCT()
struct FActorCacheEntry
{
	GENERATED_BODY()
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<class AActor>> Actors;
};

USTRUCT()
struct FMeshInstanceHandle
{
	GENERATED_BODY()
	int MeshType;
	int InstanceIndex;

	FMeshInstanceHandle()
		: MeshType(0)
		, InstanceIndex(0)
	{
	}

	FMeshInstanceHandle(int mesh_type, int instance_index)
		: MeshType( mesh_type )
		, InstanceIndex( instance_index )
	{
	}
};

USTRUCT()
struct FMeshInstanceCacheEntry
{
	GENERATED_BODY()
	TSharedPtr<TArray<FMeshInstanceHandle>> Instances;
};


USTRUCT()
struct FInstancedMeshCacheEntry
{
	GENERATED_BODY()
	UPROPERTY( Transient )
	TWeakObjectPtr<UStaticMesh> Mesh;
	UPROPERTY( Transient )
	TWeakObjectPtr<UInstancedStaticMeshComponent> InstancedMeshes;
	TSharedPtr<TArray<FTransform>> PendingInstances;
};



// Apparance Entity
// An actor who's appearance is dynamically procedurally generated and populated
//
UCLASS()
class APPARANCEUNREAL_API AApparanceEntity
	: public AActor
	, public IProceduralObject
{
	GENERATED_BODY()

	// transient
	float AutoSeedTimer;
	bool bRebuildDeferred;
	bool bPostLoadInitRequired;
	bool bSuppressTransformUpdates;

	TArray<class UProceduralMeshComponent*> DeferredGeometryRemoval;
	//cached parameters	
	mutable TSharedPtr<Apparance::IParameterCollection>	InstanceParameters;
	TSharedPtr<Apparance::IParameterCollection> OverrideParameters;
	TSharedPtr<Apparance::IParameterCollection> GenerationParameters;
	//editing
	bool m_bSelected; //own selection state (independent of UObject selection state, which we won't rely on)
	
public:
	// persistent properties
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Apparance)
	int							ProcedureID;

	// inputs persisted as BLOB and have custom editing UI (since they are dynamic)
	UPROPERTY(BlueprintReadOnly, Category=Apparance ) //prevent replacement
	mutable TArray<uint8>		InputParameterData;
	
	UPROPERTY()
	UApparanceEntityPreset*		SourcePreset; //if was created from a preset

	// advanced persistent properties
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Apparance, AdvancedDisplay, meta = (ToolTip="Should content be generated?  Turn off to delay population (script requires SetPopulated call instead)"))
	bool						bPopulated = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Apparance, AdvancedDisplay, meta = (ToolTip="Should generated content be visible?  Turn off to hide generation (script requires SetShown call instead)"))
	bool 						bShown = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Apparance, AdvancedDisplay)
	bool 						bUseMeshInstancing;

	UPROPERTY( EditAnywhere, BlueprintReadOnly, Category = Apparance, AdvancedDisplay, meta = (ToolTip = "Show all the generated content (blueprint actors) under this entity in the scene outliner") )
	bool						bShowGeneratedContent;

	UPROPERTY(EditAnywhere, Category = "Apparance", AdvancedDisplay)
	bool						AutoSeed;
	UPROPERTY(EditAnywhere, Category = "Apparance", AdvancedDisplay)
	float						AutoSeedPeriod = 0.1f;

	/** Collision data */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Apparance", AdvancedDisplay )
	class UBodySetup* CollisionSetup;

	// generated content management
	TSharedPtr<class FEntityRendering>					m_pEntityRendering;
	
	// properties
	UPROPERTY(Transient)
	class USceneComponent*     DisplayRoot;
#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient)
	class UBillboardComponent* DisplayIcon;
#endif
	UPROPERTY(Transient)
	class UApparanceParametersComponent* PlacementParameters;
	

	//components added due to procedural generation (so we know which ones to kill on duplicate)
	UPROPERTY(Transient)//, VisibleAnywhere, Category = "Apparance Internals")
	TArray<UActorComponent*>   ProceduralComponents;
	UPROPERTY(Transient)//, VisibleAnywhere, Category = "Apparance Internals")
	TArray<AActor*>            ProceduralActors;	
	UPROPERTY()
	bool bSuppressParameterMapping;	//placed BP actors shouldn't do space mapping

public:
	// geometry/component caching (by generated content id)
	UPROPERTY(Transient)
	TMap<int, TWeakObjectPtr<class UProceduralMeshComponent>> GeometryCache;
	UPROPERTY( Transient )
	TMap<int, TWeakObjectPtr<class UProceduralMeshComponent>> CollisionCache;
	UPROPERTY(Transient)
	TMap<int, TWeakObjectPtr<class UMaterialInstanceDynamic>> MaterialCache;
	UPROPERTY(Transient)
	TMap<int, FMeshCacheEntry> MeshCache;
	UPROPERTY(Transient)
	TMap<int, FActorCacheEntry> ActorCache;
	UPROPERTY(Transient)
	TMap<int, FMeshInstanceCacheEntry> InstanceCache;
	UPROPERTY(Transient)
	TMap<int, FActorComponentCacheEntry> ActorComponentCache;

private:
	// internal cache (instanced meshes)
	UPROPERTY( Transient )
	TArray<FInstancedMeshCacheEntry> InstanceMeshCache;
	UPROPERTY(Transient)
	TMap<class UStaticMesh*, int> InstancedMeshCacheLookup;

	//integrity check to spot actors that should definitely not be persisted
	//NOTE: specific case - bp based proc placed actors get turned from transient to transactional by a bp compile
	//SEE: https://answers.unrealengine.com/questions/1029158/view.html
	UPROPERTY()
	bool bWasProcedurallyPlaced;

public:
	AApparanceEntity();

	//access
	bool UseMeshInstancing() const { return bUseMeshInstancing; }
	bool FindFirstFrame( Apparance::Frame& frame_out, bool& worldspace_out, EApparanceFrameOrigin& frameorigin_out ) const;
	bool HasComponentOfType(const UClass* pcomponent_class) const;

	//control
	void MarkAsProcedurallyPlaced() { bWasProcedurallyPlaced = true; }
	void ApplyPreset(class UApparanceEntityPreset* preset);	//set proc/defaults according to a preset
	void DisableParameterMapping() { bSuppressParameterMapping = true; } //don't want world/frame remapping to occur
	void NotifyTransformChanged();

	//editing
	Apparance::IParameterCollection*       BeginEditingParameters();
	const Apparance::IParameterCollection* EndEditingParameters( bool suppress_pack=false );
	const Apparance::IParameterCollection* GetParameters() const;
	void GetParameters(TArray<const Apparance::IParameterCollection*>& parameter_cascade, EParametersRole needed_parameters=EParametersRole::All ) const;
	Apparance::IParameterCollection* GetOverrideParameters();
	bool IsParameterOverridden( Apparance::ValueID param_id ) const;
	void PostParameterEdit( const Apparance::IParameterCollection* params, Apparance::ValueID changed_param );
	void InteractiveParameterEdit( const Apparance::IParameterCollection* params, Apparance::ValueID changed_param );
	bool IsParameterDefault( Apparance::ValueID param_id ) const;
	FText GenerateParameterLabel( Apparance::ValueID param_id ) const;
	FText GenerateParameterTooltip(Apparance::ValueID param_id) const;
	const Apparance::IParameterCollection* GetCurrentParameters() const { return GenerationParameters.Get(); }
	void NotifyProcedureSpecChanged();
	Apparance::IEntity* GetEntityAPI() const;
	bool IsSmartEditingSupported() const; //currently available for this entities procedure?
	void EnableSmartEditing(); //for use at run-time/play-mode (always on in edit mode)
	void SetSmartEditingSelected(bool is_selected);
	bool IsSmartEditingSelected() const;

	// persistent properties
	UFUNCTION(BlueprintCallable, Category="Apparance|Entity")
	void SetProcedureID(int procedure_id);
	UFUNCTION(BlueprintCallable, Category="Apparance|Entity")
	void SetPopulated( bool is_populated );
	UFUNCTION(BlueprintCallable, Category="Apparance|Entity")
	void SetShown( bool is_shown );
	UFUNCTION(BlueprintCallable, Category="Apparance|Entity")
	void SetUseMeshInstancing( bool use_mesh_instancing );
	UFUNCTION(BlueprintCallable, Category="Apparance|Entity")
	void Rebuild();
	UFUNCTION(BlueprintCallable, Category="Apparance|Entity")
	void RebuildDeferred();

	// smart editing
	UFUNCTION(BlueprintCallable, Category = "Apparance|Entity")
	void SetEditingSelected(bool bSelected);
	UFUNCTION(BlueprintCallable, Category = "Apparance|Entity")
	bool GetEditingSelected();

	// set parmeters

	// events
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName = "GenerationComplete"), Category="Apparance|Entity")
	void ReceiveGenerationComplete();

public:
	//~ Begin UObject Interface
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PreEditUndo() override;
	virtual void PostEditUndo() override;
#endif
	virtual void BeginDestroy();
	//~ End UObject Interface

	//~ Begin AActor Interface
	virtual void Destroyed() override;
	virtual void PostActorCreated() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override;
	virtual void OnConstruction(const FTransform& Transform) override;
#if WITH_EDITOR
	virtual void RerunConstructionScripts() override;
#endif
protected:

	virtual void BeginPlay();
public:
	//~ End AActor Interface

	//~ Begin IProceduralObject Interface
	//query
	virtual int IPO_GetProcedureID() const  override { return ProcedureID; }
	virtual void IPO_GetParameters(TArray<const Apparance::IParameterCollection*>& parameter_cascade, EParametersRole needed_parameters) const override;
	virtual bool IPO_IsParameterOverridden(Apparance::ValueID input_id) const;
	virtual bool IPO_IsParameterDefault(Apparance::ValueID param_id) const;
	virtual FText IPO_GenerateParameterLabel( Apparance::ValueID input_id ) const;
	virtual FText IPO_GenerateParameterTooltip(Apparance::ValueID input_id) const;
	//editing
	virtual void IPO_Modify() override { Modify(); }
	virtual void IPO_SetProcedureID(int proc_id) override;
	virtual Apparance::IParameterCollection* IPO_BeginEditingParameters() override;
	virtual const Apparance::IParameterCollection* IPO_EndEditingParameters(bool dont_pack_interactive_edits) override;
	virtual void IPO_PostParameterEdit(const Apparance::IParameterCollection* params, Apparance::ValueID changed_param) override;
	virtual void IPO_InteractiveParameterEdit( const Apparance::IParameterCollection* params, Apparance::ValueID changed_param ) override;
	//~ End IProceduralObject Interface

	// entity rendering access
	class FEntityRendering* GetEntityRendering() const { return m_pEntityRendering.Get(); }
	void BeginGeometryUpdate();
	void EndGeometryUpdate( int tier_index );
	class UProceduralMeshComponent* AddGeometry(Apparance::Host::IGeometry* geometry, int tier_index, FVector unreal_offset, UProceduralMeshComponent*& pcollision_mesh_out );
	void                            RemoveGeometry(class UProceduralMeshComponent* pcomponent);
	class UStaticMeshComponent*     AddMesh(class UStaticMesh* psource, FMatrix& local_placement);
	void                            RemoveMesh(class UStaticMeshComponent* pcomponent);
	class AActor*					AddBlueprint_Begin(class UBlueprintGeneratedClass* pclasstemplate, FMatrix& local_placement);
	void                            AddBlueprint_End( AActor* pactor, FMatrix& local_placement, const Apparance::IParameterCollection* placement_parameters );
	void                            RemoveBlueprint(class AActor* pactor);
	FMeshInstanceHandle				AddInstancedMesh(class UStaticMesh* psource,FMatrix& local_placement);
	void							RemoveInstancedMesh(FMeshInstanceHandle mesh_handle);
	void							RemoveAllInstancedMeshes();
	class UActorComponent*          AddActorComponent(const class UApparanceResourceListEntry_Component* psource, FMatrix& local_placement);
	void                            RemoveActorComponent(class UActorComponent* pcomponent);
	
#if WITH_EDITOR
	// debug stuff
	FString Debug_GetStructure() const;
#endif //WITH_EDITOR

	// utils
	static void ValidateFrame( Apparance::Frame& frame );	//check frame and fix issues

private:
	//setup
	void PostLoadInit();
	void PreConstructionScript();
	void PostConstructionScript();

	//parameters
	void UnpackParameters() const;
	void PackParameters();
	const Apparance::IParameterCollection* GetSpecParameters() const;
	void ApplyParameterMappings( Apparance::IParameterCollection* params, const Apparance::IParameterCollection* spec );
	void CleanParameters();

	//proc generation
	void CompileCreationParameters();
	void CompileGenerationParameters();
	void CompileUpdateParameters();
	void TriggerGeneration();

	//content management
	void ClearContent();
	void DestroyContent();
	void RemoveGeometryInternal( class UProceduralMeshComponent* pcomponent );
	void ClearDeferredRemovals();
	void TidyCaches();

	//helpers
	static void CollapseParameterCascade( TArray<const Apparance::IParameterCollection*>& parameters_cascade, const Apparance::IParameterCollection* template_params, Apparance::IParameterCollection* out_params );

	//debug/diags support
	void Debug_CheckAttachedActors();
};

