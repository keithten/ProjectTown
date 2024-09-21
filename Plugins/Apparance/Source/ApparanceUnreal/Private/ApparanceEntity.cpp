//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

//local debugging help
#define APPARANCE_DEBUGGING_HELP_ApparanceEntity 0
#if APPARANCE_DEBUGGING_HELP_ApparanceEntity
PRAGMA_DISABLE_OPTIMIZATION_ACTUAL
#endif

// main
#include "ApparanceEntity.h"


// unreal
#include "Components/BoxComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/DecalComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Texture2D.h"
#include "ProceduralMeshComponent.h"
#include "Engine/World.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/BlueprintGeneratedClass.h"

// module
#include "ApparanceUnreal.h"
#include "EntityRendering.h"
#include "Geometry.h"
#include "ApparanceEntityPreset.h"
#include "ApparanceParametersComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "ResourceList/ApparanceResourceListEntry_Component.h"
#include "Utility/ApparanceUtility.h"
#include "Geometry/ApparanceRootComponent.h"
#include "Support/SmartEditingState.h"

#define LOCTEXT_NAMESPACE "ApparanceUnreal"

//log some unreal events on entities, e.g. Destroyed, PostDuplicate, etc.
#if 0
#define LOG_ENTITY_EVENTS(CategoryName, Verbosity, Format, ...) UE_LOG(CategoryName, Verbosity, Format, __VA_ARGS__)
#else
#define LOG_ENTITY_EVENTS(...)
#endif

//testing effect of waiting to remove geometry until after new geometry added
#define APPARANCE_DEFERRED_REMOVAL 0
//show procedures being placed, destroyed, built, and their optionally their parameters
#define ENABLE_PROC_BUILD_DIAGNOSTICS 0
#define ENABLE_PROC_BUILD_PARAMETERS 0


/// <summary>
/// helper to save (clear) and restore overlapped generation flag state for an actors components
/// </summary>
class ScopedOverlapDisable
{
	AActor* m_pActor;
	TSet<UPrimitiveComponent*> m_Overlaps;
public:
	ScopedOverlapDisable( AActor* pactor, bool disable_only=false )
	{
		m_pActor = pactor;

		//cache prim overlap state, and disable
		for(auto it = m_pActor->GetComponents().CreateConstIterator() ; it ; ++it)
		{
			UPrimitiveComponent* pprim = Cast< UPrimitiveComponent>( *it );
			if(pprim)
			{
				bool does_overlaps = pprim->GetGenerateOverlapEvents();
				if(does_overlaps)
				{
					m_Overlaps.Add( pprim );
					pprim->SetGenerateOverlapEvents( false );
				}
			}
		}
		
		if(disable_only)
		{
			m_pActor = nullptr;
		}
	}

	~ScopedOverlapDisable()
	{
		if(m_pActor)
		{
			//cache prim overlap state, and disable
			for(auto it = m_pActor->GetComponents().CreateConstIterator(); it; ++it)
			{
				UPrimitiveComponent* pprim = Cast< UPrimitiveComponent>( *it );
				if(pprim)
				{
					if(m_Overlaps.Contains( pprim ))
					{
						pprim->SetGenerateOverlapEvents( true );
					}
				}
			}
		}
	}
};


//////////////////////////////////////////////////////////////////////////
// AApparanceEntity

AApparanceEntity::AApparanceEntity()
{
	m_pEntityRendering = MakeShareable( new FEntityRendering() );
#if 1
	//custom root (for transform notifications)
	DisplayRoot = CreateDefaultSubobject<UApparanceRootComponent>( TEXT( "Root" ) );
#else
	DisplayRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
#endif
	RootComponent = DisplayRoot;
	//DisplayRoot->SetMobility(EComponentMobility::Movable);

	//editing visuals
#if WITH_EDITORONLY_DATA
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinderOptional<UTexture2D> DecalTexture;
		FName ID_Decals;
		FText NAME_Decals;
		FConstructorStatics()
			: DecalTexture(TEXT("/ApparanceUnreal/Icons/ApparanceEntity"))
			, ID_Decals(TEXT("Misc"))
			, NAME_Decals(NSLOCTEXT("SpriteCategory", "Misc", "Misc"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;

	//dimensions bounding box
#if 0
	DisplayBounds = CreateEditorOnlyDefaultSubobject<UBoxComponent>(TEXT("Box"));
	if (DisplayBounds)
	{
		DisplayBounds->SetIsVisualizationComponent(true);
		DisplayBounds->SetBoxExtent(FVector(100.0f, 100.0f, 100.0f), false);
		DisplayBounds->SetupAttachment(DisplayRoot);
	}
#endif

	//actor type icon
	DisplayIcon = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	if (DisplayIcon)
	{
		DisplayIcon->SetIsVisualizationComponent(true);
		DisplayIcon->Sprite = ConstructorStatics.DecalTexture.Get();
		DisplayIcon->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.5f));
		DisplayIcon->SpriteInfo.Category = ConstructorStatics.ID_Decals;
		DisplayIcon->SpriteInfo.DisplayName = ConstructorStatics.NAME_Decals;
		DisplayIcon->bIsScreenSizeScaled = true;
		DisplayIcon->SetAbsolute(false, false, true);
		DisplayIcon->bReceivesDecals = false;
		DisplayIcon->SetupAttachment(DisplayRoot);
	}
#endif // WITH_EDITORONLY_DATA

	//parameter wrangler sub-object
	PlacementParameters = CreateDefaultSubobject<UApparanceParametersComponent>(FName("PlacementParameters"));
	PlacementParameters->ClearFlags(RF_Transactional);						//not persistent
	PlacementParameters->SetFlags(RF_Transient|RF_DuplicateTransient);		//only runtime

	//updates
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = false;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	//default state
	ProcedureID = Apparance::InvalidID;

	//we are static (otherwise we can't attach blueprints)
#if WITH_EDITOR
	//in editor everything complains if not movable
	RootComponent->SetMobility( EComponentMobility::Movable );
#else
	RootComponent->SetMobility( EComponentMobility::Static );
#endif

	//collision preset
	CollisionSetup = CreateDefaultSubobject<UBodySetup>(FName("ProcMeshCollisionSetup"));
	CollisionSetup->DefaultInstance.SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	//init transients
	bPostLoadInitRequired = false;
	bWasProcedurallyPlaced = false;
	bSuppressParameterMapping = false;
	m_bSelected = false;
	bSuppressTransformUpdates = false;
}

// now exists due to being added in editor or game
//
void AApparanceEntity::PostActorCreated()
{
	Super::PostActorCreated();
	LOG_ENTITY_EVENTS( LogApparance, Display, TEXT("*********** PostActorCreated %p ************"), this );
//	UE_LOG( LogApparance, Display, TEXT( "********** Created actor %p" ), this );

	//UnpackParameters();

	//on creation
	m_pEntityRendering->SetOwner(this);

	//pend gen
	if(bPopulated)
	{
		RebuildDeferred();
	}
}

// post construction script?
//
void AApparanceEntity::OnConstruction(const FTransform& Transform)
{
	LOG_ENTITY_EVENTS( LogApparance, Display, TEXT("*********** OnConstruction %p ************"), this );
	Super::OnConstruction(Transform);

	//verify transient status
	if(bWasProcedurallyPlaced)
	{
		if(HasAnyFlags( RF_Transactional ) || !HasAllFlags( RF_Transient | RF_DuplicateTransient ))
		{
			//has lost it's transient status, force back		
			//UE_LOG( LogApparance, Warning, TEXT( "Restoring transient status of entity that has lost it: %s" ), *GetName() );
			ClearFlags( RF_Transactional );
			SetFlags( RF_Transient | RF_DuplicateTransient );
		}
	}
}

#if WITH_EDITOR
// blueprint scripted actor setup, only available in editor now since construction scripts only run in game when spawning, but editor needs to use them during editing
//
void AApparanceEntity::RerunConstructionScripts()
{
	LOG_ENTITY_EVENTS( LogApparance, Display, TEXT("vvvvvvvvvvvv RerunConstructionScripts vvvvvvvvvvvv %p : %s"), this, *GetName() );

	//pre-script - outside-game build/prep modification by script needs to happen here, i.e. edit mode modification
	PreConstructionScript();

	//opportunity for script to read creation parameters and write override parameters
	Super::RerunConstructionScripts();
	
	//post-script
	PostConstructionScript();
	LOG_ENTITY_EVENTS( LogApparance, Display, TEXT("^^^^^^^^^^^^ RerunConstructionScripts ^^^^^^^^^^^^ %p : %s"), this, *GetName() );
}
#endif

// prepare parameters to be available as input/query for construction scripts
//
void AApparanceEntity::PreConstructionScript()
{
#if ENABLE_PROC_BUILD_DIAGNOSTICS 
	UE_LOG( LogApparance, Log, TEXT( "BEFORE Setup Script: %s" ), *this->GetName() );
#endif
	CompileCreationParameters();
}

// prepare parameters and trigger construction after construction script has had a chance to manupulate them
//
void AApparanceEntity::PostConstructionScript()
{
#if ENABLE_PROC_BUILD_DIAGNOSTICS 
	UE_LOG( LogApparance, Log, TEXT( "AFTER Setup Script: %s" ), *this->GetName() );
#endif
	CompileGenerationParameters();
	RebuildDeferred();
}


// now exists due to duplication of world for PIE or duplication in editor
//
void AApparanceEntity::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);
	LOG_ENTITY_EVENTS( LogApparance, Display, TEXT("*********** PostDuplicate %p ************"), this );

	//clean out proc-components copied during duplicate
	//NOTE: shouldn't RF_DuplicateTransient do this?
	for (int i = 0; i < ProceduralComponents.Num(); i++)
	{
		UActorComponent* pc = ProceduralComponents[i];
		if (pc)
		{
			pc->DestroyComponent();
		}
	}
	ProceduralComponents.Empty();

	//for actors, remove any that copied across that aren't in our new actor attachment list (which are new instances in the copy)
	TArray<AActor*> attached;
	GetAttachedActors( attached, true );
	for(int i = 0; i < ProceduralActors.Num();)
	{
		AActor* pa = ProceduralActors[i];
		if(pa && !attached.Contains( pa ))
		{
			ProceduralActors.RemoveAt( i );
		}
		else
		{
			i++;
		}
	}
	ProceduralActors.Empty();

	//params
	UnpackParameters();

	//mode
	if (bDuplicateForPIE)
	{
		//play in editor started (PostLoad will follow)

	}
	else
	{
		//duplication in editor (PostActorCreated already called)
		
		//need rebuild of proc content as can't use duplicated components and PostLoad not called on copy in editor
		RebuildDeferred();
	}
}

// now exists due to loading level from disk and in PiE
//
void AApparanceEntity::PostLoad()
{
	Super::PostLoad();
	LOG_ENTITY_EVENTS( LogApparance, Display, TEXT( "*********** PostLoad %p ************" ), this );

	//hold off init if loading a package at startup, when engine not ready
	if(FApparanceUnrealModule::GetLibrary())
	{
		PostLoadInit();
	}
	else
	{
		//request later
		bPostLoadInitRequired = true;
	}
}

// init required after loading
//
void AApparanceEntity::PostLoadInit()
{
	//param data is loaded as bin blob
	UnpackParameters();

	m_pEntityRendering->SetOwner( this );

	//on load/pie
	RebuildDeferred();
	
	TidyCaches();

	bPostLoadInitRequired = false;
}

// remove any dead cache entries
//
void AApparanceEntity::TidyCaches()
{
	//any dead objects that leave empty spaces behind	
	for(auto It = ActorCache.CreateIterator(); It; ++It)
	{
		FActorCacheEntry& e = It.Value();
		e.Actors.Remove( nullptr );
	}
	ProceduralComponents.Remove( nullptr );
	ProceduralActors.Remove( nullptr );

	//any transactional components (BP compile seems to convert some of them)
	for (int i = 0; i < ProceduralComponents.Num(); i++)
	{
		if (ProceduralComponents[i]->HasAnyFlags(RF_Transactional))
		{
			UActorComponent* pac = ProceduralComponents[i];
			ProceduralComponents.RemoveAt(i);
			i--;
			pac->DestroyComponent();
		}
	}
	for (int i = 0; i < ProceduralActors.Num(); i++)
	{
		if (ProceduralActors[i]->HasAnyFlags(RF_Transactional))
		{
			AActor* aa = ProceduralActors[i];
			ProceduralActors.RemoveAt(i);
			i--;
			aa->Destroy( false, false );
		}
	}
}

#if WITH_EDITOR
// details panel edit response
//
void AApparanceEntity::PostEditChangeProperty(struct FPropertyChangedEvent& e)
{
	//LOG_ENTITY_EVENTS( LogApparance, Display, TEXT("*********** PostEditChangeProperty %p ************"), this );
	FName PropertyName = (e.Property != NULL) ? e.Property->GetFName() : NAME_None;
	FName MemberPropertyName = (e.Property != NULL) ? e.MemberProperty->GetFName() : NAME_None;

	//handle prop effects
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AApparanceEntity, ProcedureID))
	{
		//TODO: check if we are a preset, if so, warn/remove preset link
		if (SourcePreset)
		{
			//changing procedure type invalidates preset status
			SourcePreset = nullptr;
		}

		//change of proc often invalidates stored instance parameters
		CleanParameters();

		//proc type changed
		RebuildDeferred();
		//ensure details panel updates if we change procedure type
		FApparanceUnrealModule::GetModule()->Editor_NotifyEntityProcedureTypeChanged( ProcedureID );
	}
	else if(PropertyName == GET_MEMBER_NAME_CHECKED(AApparanceEntity, bUseMeshInstancing))
	{
		//entity config changed
		RebuildDeferred();
	}
	else if(PropertyName == GET_MEMBER_NAME_CHECKED(AApparanceEntity, bPopulated))
	{
		bPopulated = !bPopulated;	//ensure it is applied by undoing effect and then toggling
		SetPopulated( !bPopulated );
	}
	else if(PropertyName == GET_MEMBER_NAME_CHECKED(AApparanceEntity, bShown))
	{
		bShown = !bShown;	//ensure it is applied by undoing effect and then toggling
		SetShown( !bShown );
	}
	else if(PropertyName == GET_MEMBER_NAME_CHECKED( AApparanceEntity, bShowGeneratedContent ))
	{
		//affects generated content
		RebuildDeferred();
	}
	
	Super::PostEditChangeProperty(e);
}

// clear content before undo
//
void AApparanceEntity::PreEditUndo()
{
	LOG_ENTITY_EVENTS( LogApparance, Display, TEXT("*********** PreEditUndo %p ************"), this );	

	//need to clear generated content as very hard to manage
	ClearContent();

	Super::PreEditUndo();
}

// rebuild content after undo
//
void AApparanceEntity::PostEditUndo()
{
	LOG_ENTITY_EVENTS( LogApparance, Display, TEXT("*********** PostEditUndo %p ************"), this );

	//params may have changed
	UnpackParameters();

	//content is removed during undo, re-build now
	RebuildDeferred();

	//changes to base actor should cause instances to update
	if(!HasAnyFlags( RF_ClassDefaultObject ) && IsValidChecked(this))
	{
		FApparanceUnrealModule::GetModule()->Editor_NotifyProcedureDefaultsChange(ProcedureID, nullptr, Apparance::InvalidID);
	}

	//default
	Super::PostEditUndo();
}

#endif //WITH_EDITOR

// find parameters needed for our current procedures inputs from it's specification
//
const Apparance::IParameterCollection* AApparanceEntity::GetSpecParameters() const
{
	//checks
	auto* pengine = FApparanceUnrealModule::GetEngine();
	auto* plibrary = pengine?pengine->GetLibrary():nullptr;
	if (!plibrary)
	{
		return nullptr;
	}
	
	//procedure inputs
	Apparance::ProcedureID proc_id = (Apparance::ProcedureID)ProcedureID;
	const Apparance::ProcedureSpec* proc_spec = plibrary->FindProcedureSpec( proc_id );
	return proc_spec?proc_spec->Inputs:nullptr;
}

// look for worldspace/origin metadata flag on any properties and apply mapping as appropriate
//
void AApparanceEntity::ApplyParameterMappings(Apparance::IParameterCollection* params, const Apparance::IParameterCollection* spec)
{
	//suppressed?
	if(bSuppressParameterMapping)
	{
		return;
	}

	//prep
	int num_params = spec->BeginAccess();
	params->BeginAccess();

	//scan
	for (int i = 0; i < num_params; i++)
	{
		//needs mapping?
		Apparance::Parameter::Type type = spec->GetType( i );
		const char* name = spec->GetName(i);
#if APPARANCE_ENABLE_SPATIAL_MAPPING
		bool ws_value;
		bool is_spatial = ApparanceParameterTypeIsSpatial( type );
		bool map_worldspace = is_spatial && FApparanceUtility::TryGetMetadataValue( "Worldspace", name, ws_value ) && ws_value;	//(has worldspace metadata flag and it is true)
#endif
		int origin_value = (int)EApparanceFrameOrigin::Face; //default is face
		FApparanceUtility::TryGetMetadataValue( "Origin", name, origin_value );
		bool map_origin = origin_value != (int)EApparanceFrameOrigin::Corner; //no remapping needed for corner

		//apply mapping if needed by type
		Apparance::ValueID id = spec->GetID(i);
		switch (type)
		{
			case Apparance::Parameter::Float:
			{
#if APPARANCE_ENABLE_SPATIAL_MAPPING
				if(map_worldspace)
				{
					float value;
					if(params->FindFloat( id, &value ))
					{
						//map
						value = APPARANCESCALE_FROM_UNREALSCALE( value );
						params->EndAccess();
						params->BeginEdit();
						params->ModifyFloat( id, value );
						params->EndEdit();
						params->BeginAccess();
					}
				}
#endif
				break;
			}
			case Apparance::Parameter::Integer:
			{
#if APPARANCE_ENABLE_SPATIAL_MAPPING
				if(map_worldspace)
				{
					int value;
					if(params->FindInteger( id, &value ))
					{
						//map
						value = (int)APPARANCESCALE_FROM_UNREALSCALE( (float)value );
						params->EndAccess();
						params->BeginEdit();
						params->ModifyInteger( id, value );
						params->EndEdit();
						params->BeginAccess();
					}
				}
#endif
				break;
			}
			case Apparance::Parameter::Frame:
			{
				if(
#if APPARANCE_ENABLE_SPATIAL_MAPPING
					map_worldspace ||
#endif
					map_origin)
				{
					Apparance::Frame frame;
					if(params->FindFrame( id, &frame ))
					{
						//map							
#if APPARANCE_ENABLE_SPATIAL_MAPPING
						if(map_worldspace)
						{
							ApparanceFrameFromUnrealWorldspaceFrame( frame );
						}
#endif
						if(map_origin)
						{
							EApparanceFrameOrigin origin = (EApparanceFrameOrigin)origin_value;
							ApparanceFrameOriginAdjust( frame, origin );
						}
						params->EndAccess();
						params->BeginEdit();
						params->ModifyFrame( id, &frame );
						params->EndEdit();
						params->BeginAccess();
					}
				}
				break;
			}
			case Apparance::Parameter::Vector3:
			{
#if APPARANCE_ENABLE_SPATIAL_MAPPING
				if(map_worldspace)
				{
					Apparance::Vector3 vector;
					if(params->FindVector3( id, &vector ))
					{
						//map							
						vector = VECTOR3_APPARANCESPACE_FROM_UNREALSPACE( vector );
						params->EndAccess();
						params->BeginEdit();
						params->ModifyVector3( id, &vector );
						params->EndEdit();
						params->BeginAccess();
					}
				}
#endif
				break;
			}
		}
	}

	//done
	params->EndAccess();
	spec->EndAccess();
}

// our parameter spec has changed (editing by attached editor)
//
void AApparanceEntity::NotifyProcedureSpecChanged()
{
	CleanParameters();
}

// engine entity access
//
Apparance::IEntity* AApparanceEntity::GetEntityAPI() const
{
	auto* per = GetEntityRendering();
	if(per)
	{
		return per->GetEntityAPI();
	}
	return nullptr;
}

bool AApparanceEntity::IsSmartEditingSupported() const
{
	Apparance::IEntity* p = AApparanceEntity::GetEntityAPI();
	if(p)
	{ 
		return p->SupportsSmartEditing();
	}
	return false;
}  

void AApparanceEntity::EnableSmartEditing()
{
	Apparance::IEntity* p = GetEntityAPI(); 
	if(p)
	{
		p->EnableSmartEditing();
	}
}

void AApparanceEntity::SetSmartEditingSelected(bool is_selected)
{
	Apparance::IEntity* p = GetEntityAPI();
	if (p)
	{
		//select
		m_bSelected = is_selected;
		p->SetSelected(is_selected);

		//maintain smart editing state to match
		if (USmartEditingState* pse = FApparanceUnrealModule::GetModule()->IsEditingEnabled( GetWorld() ))
		{
			pse->NotifyEntitySelection( this, is_selected );
		}
	}
}

bool AApparanceEntity::IsSmartEditingSelected() const
{
	return m_bSelected;
}


// smart editing BP support
void AApparanceEntity::SetEditingSelected(bool bSelected)
{
	SetSmartEditingSelected(bSelected);
}
bool AApparanceEntity::GetEditingSelected()
{
	return IsSmartEditingSelected();
}


// clear out any bad parameters
//
void AApparanceEntity::CleanParameters()
{
	//ensure ready
	if(InstanceParameters.IsValid())
	{
		//update as best to match the spec
		const Apparance::IParameterCollection* spec_params = GetSpecParameters();
		if(spec_params) //(may be set to None)
		{
			InstanceParameters->Sanitise( spec_params );

			//ensure stored away with fixed up values
			PackParameters();
		}
	}
}

// get the entities parameters collection for editing
//
Apparance::IParameterCollection* AApparanceEntity::BeginEditingParameters()
{
	//init on demand (class default doesn't get load/create path)
	if(!InstanceParameters.IsValid())
	{
		UnpackParameters();
	}

	Apparance::IParameterCollection* inst_params = InstanceParameters.Get();
	inst_params->BeginEdit();
	return inst_params;
}
// done editing parameters
//
const Apparance::IParameterCollection* AApparanceEntity::EndEditingParameters( bool suppress_pack )
{
	InstanceParameters->EndEdit();

	if(!suppress_pack)
	{
		//ensure stored away so that if a BP is recompiled we don't lose them (placed proc objects)
		PackParameters();
	}

	return InstanceParameters.Get();
}

// get just the entity parameters collection
//
const Apparance::IParameterCollection* AApparanceEntity::GetParameters() const
{
	//init on demand (class default doesn't get load/create path)
	if(!InstanceParameters.IsValid())
	{
		UnpackParameters();
	}
	return InstanceParameters.Get();
}

// get list of places to look for parameter, allowing fallback to other sources
// e.g. Entity Instance parameters -> Blueprint Class parameters -> Procedure Spec inputs
// can ask for Generation, Override, Creation, Instance, Blueprint, and Specification contribution parameter sets
// or request ForCreation, ForGeneration, or ForRebuild composite collections (the sources for each respective stage)
void AApparanceEntity::GetParameters( TArray<const Apparance::IParameterCollection*>& parameter_cascade, EParametersRole needed_parameters ) const
{
	//final results
	if(((int)needed_parameters&(int)EParametersRole::Generation)!=0 && GenerationParameters.IsValid())
	{
		parameter_cascade.Add(GenerationParameters.Get());
	}
	
	//any overrides set on this instance
	if(((int)needed_parameters&(int)EParametersRole::Override)!=0 && OverrideParameters.IsValid())
	{
		parameter_cascade.Add(OverrideParameters.Get());
	}

	//previously generated creation parameters
	if (((int)needed_parameters&(int)EParametersRole::Creation) != 0 && PlacementParameters!=nullptr)
	{
		const Apparance::IParameterCollection* creation_parameters = PlacementParameters->GetActorParameters();
		if (creation_parameters)
		{
			//creation parameters are cached in the PlacementParameters component with other proc-placement parameter sets
			parameter_cascade.Add(creation_parameters);
		}
	}

	//this instance (assuming not CDO)
	if(((int)needed_parameters&(int)EParametersRole::Instance) != 0)
	{
		auto pinstance_parameters = GetParameters();
		if (pinstance_parameters)
		{
			parameter_cascade.Add(pinstance_parameters);
		}
	}

	//CDO defaults (when we aren't the CDO)
	if (((int)needed_parameters&(int)EParametersRole::Blueprint) != 0)
	{
		//presets behave like a blueprint (note: can't be both preset based and blueprint based)
		if (SourcePreset)
		{
			auto pclass_default_parameters = SourcePreset->GetParameters();
			if (pclass_default_parameters)
			{
				parameter_cascade.Add(pclass_default_parameters);
			}
		}
		else
		{
			//AApparanceEntity* ptemplate_entity = Cast<AApparanceEntity>( pbaseblueprint->GeneratedClass->GetDefaultObject() );		
			AApparanceEntity* pclass_default = Cast<AApparanceEntity>(GetClass()->GetDefaultObject());
			if (pclass_default != this)
			{
				auto pclass_default_parameters = pclass_default->GetParameters();
				if (pclass_default_parameters)
				{
					parameter_cascade.Add(pclass_default_parameters);
				}
			}
		}
	}

	//procedure spec defaults
	if (((int)needed_parameters&(int)EParametersRole::Specification) != 0)
	{
		const Apparance::ProcedureSpec* proc_spec = FApparanceUnrealModule::GetEngine()->GetLibrary()->FindProcedureSpec((Apparance::ProcedureID)ProcedureID);
		const Apparance::IParameterCollection* spec_params = proc_spec?proc_spec->Inputs:nullptr;
		if (spec_params)
		{
			parameter_cascade.Add(spec_params);
		}
	}
}

// script access for temporarily overriding parameters instead of actually editing the entities parameter data
//
Apparance::IParameterCollection* AApparanceEntity::GetOverrideParameters()
{
	if(!OverrideParameters.IsValid())
	{
		OverrideParameters = MakeShareable(FApparanceUnrealModule::GetEngine()->CreateParameterCollection());
	}
	return OverrideParameters.Get();
}

// has a parameter been overridden in script?
//
bool AApparanceEntity::IsParameterOverridden( Apparance::ValueID param_id ) const
{
	if (OverrideParameters.IsValid())
	{
		Apparance::IParameterCollection* params = OverrideParameters.Get();
		
		//do we have this param in the overrides?
		params->BeginAccess();
		bool is_overridden = params->FindType(param_id) != Apparance::Parameter::None;
		params->EndAccess();

		return is_overridden;
	}

	//no, not overridden
	return false;
}

// is this parameter going to use the class/spec default?
// i.e. not changed for this instance, nor overridden by script
bool AApparanceEntity::IsParameterDefault(Apparance::ValueID param_id) const
{
	//overridden?
	if (IsParameterOverridden(param_id))
	{
		return false;
	}

	//check our actual params
	const Apparance::IParameterCollection* params = GetParameters();
	params->BeginAccess();
	bool is_set = params->FindType(param_id) != Apparance::Parameter::None;
	params->EndAccess();
	if (is_set)
	{
		return false;
	}
	
	//ok, will fall back to default then
	return true;
}

// generate label, i.e. name of parameter
//
FText AApparanceEntity::GenerateParameterLabel( Apparance::ValueID input_id ) const
{
	FString s;
	FString units;
	
	//type/name
	const Apparance::IParameterCollection* spec_params = GetSpecParameters();
	if(spec_params)
	{
		//get name
		spec_params->BeginAccess();
		UTF8CHAR* raw_name = (UTF8CHAR*)spec_params->FindName( input_id );
		s = APPARANCE_UTF8_TO_TCHAR( raw_name );
		spec_params->EndAccess();

#if APPARANCE_ENABLE_SPATIAL_MAPPING
		//while we're here check for worldspace metadata (for units)
		bool ws_value;
		if (FApparanceUtility::TryGetMetadataValue("Worldspace", raw_name, ws_value) && ws_value)
		{
			units = LOCTEXT("WorldSpaceUnitHintSuffix", " (cm)").ToString();
		}
#endif

		//extract name part (remove desc/meta)
		int isep = FApparanceUtility::NextMetadataSeparator( s, 0 );
		if(isep != -1)
		{
			s = s.Left( isep );
		}
	}
	if(!s.IsEmpty())
	{
		return FText::FromString( s + units );
	}

	//empty
	return FText::Format( LOCTEXT( "UnnamedParameterLabelFormat", "Parameter #{0}" ), input_id );
}

// generate tooltip, explaining state of parameter (e.g. overridden, etc)
//
FText AApparanceEntity::GenerateParameterTooltip( Apparance::ValueID input_id ) const
{
	FString s;

	//type/name
	const Apparance::IParameterCollection* spec_params = GetSpecParameters();
	if (spec_params)
	{
		//name info
		spec_params->BeginAccess();
		FString raw_name = APPARANCE_UTF8_TO_TCHAR( (UTF8CHAR*)spec_params->FindName( input_id ) );

		//extract description
		FString desc = FApparanceUtility::GetMetadataSection(raw_name,1);
		if (!desc.IsEmpty())
		{
			s = desc;
		}
		else
		{
			Apparance::Parameter::Type param_type = spec_params->FindType(input_id);
			s.Append(ApparanceParameterTypeName(param_type));
			s.Append(TEXT(" parameter"));
		}

		spec_params->EndAccess();
	}
	else
	{
		s.Append( LOCTEXT("EntityParamUnknown", "Unknown entity parameter!").ToString() );
	}

	//state
	s.Append(TEXT("\n"));
	if(!IsParameterOverridden( input_id ))
	{
		if (IsParameterDefault(input_id))
		{
			//default
			s.Append(LOCTEXT("EntityParamIsDefault", "Parameter is set to its default value").ToString());
		}
		else
		{
			//specified
			s.Append(LOCTEXT("EntityParamIsEdited", "Parameter has been edited").ToString());
		}
	}
	else
	{
		//overridden by script
		s.Append(LOCTEXT("EntityParamIsOverridden", "Parameter has been overridden by a script").ToString());
	}

	return FText::FromString(s);
}



// our own internal editing support notification
//
void AApparanceEntity::PostParameterEdit( const Apparance::IParameterCollection* params, Apparance::ValueID changed_param )
{
	//move changes to persistent store
	PackParameters();
	
	//trigger rebuild
	RebuildDeferred();

	//changes to base actor should cause instances to update
	//(if change is on parameter we are interested in)
	if(HasAnyFlags( RF_ClassDefaultObject|RF_ArchetypeObject ) && IsValidChecked(this))
	{
		FApparanceUnrealModule::GetModule()->Editor_NotifyProcedureDefaultsChange(ProcedureID, params, changed_param);
	}
}

// our own internal editing support, during interactive edits
// don't commit, just do what's needed to update the visuals
//
void AApparanceEntity::InteractiveParameterEdit(const Apparance::IParameterCollection* params, Apparance::ValueID changed_param)
{
	//trigger rebuild
	RebuildDeferred();
	
	//changes to base actor should cause instances to update
	//(if change is on parameter we are interested in)
	if(HasAnyFlags( RF_ClassDefaultObject | RF_ArchetypeObject ) && IsValidChecked( this ))
	{
		FApparanceUnrealModule::GetModule()->Editor_NotifyProcedureDefaultsChange(ProcedureID, params, changed_param);
	}
}

// we've been moved
//
void AApparanceEntity::NotifyTransformChanged()
{
	//ignore spurious notifications, e.g. during sub-actor attach/detach
	if (bSuppressTransformUpdates )
	{
		return;
	}
	
	//notify apparance of entity location
	FVector origin = GetActorLocation();
	FVector right = GetActorRightVector();
	FVector forward = GetActorForwardVector();
	if(auto pentity = GetEntityAPI())
	{
		pentity->NotifyWorldTransform( VECTOR3_APPARANCESPACE_FROM_UNREALSPACE( origin ), VECTOR3_APPARANCEHANDEDNESS_FROM_UNREALHANDEDNESS( right ), VECTOR3_APPARANCEHANDEDNESS_FROM_UNREALHANDEDNESS( forward ) );
	}
}



// soon to nolonger exist due to delete from level, end of PIE or level/game shutdown
//
void AApparanceEntity::BeginDestroy()
{
	//PiE seems to skip Destroyed and just call this sometimes, so make sure
	DestroyContent();

	Super::BeginDestroy();
	LOG_ENTITY_EVENTS( LogApparance, Display, TEXT("*********** BeginDestroy %p ************"), this );
}

// being destroyed in game or in editor (not during unload)
//
void AApparanceEntity::Destroyed()
{
	//normal destruction path
	DestroyContent();

	LOG_ENTITY_EVENTS( LogApparance, Display, TEXT("*********** Destroyed %p ************"), this );
//	UE_LOG( LogApparance, Display, TEXT( "********** Destroyed actor %p" ), this );

	Super::Destroyed();
}

// remove any generated actors
//
void AApparanceEntity::ClearContent()
{
	//reset cached state in er tracker
	m_pEntityRendering->Clear();
}

// fully dump content and entity rendering
//
void AApparanceEntity::DestroyContent()
{
	//need to ensure cleared up
	ClearContent();
	//discard internal entity
	m_pEntityRendering->SetOwner( nullptr );
}

// start of PIE or standalone gameplay after everything loaded
//
void AApparanceEntity::BeginPlay()
{
	LOG_ENTITY_EVENTS( LogApparance, Display, TEXT( "vvvvvvvvvvvvv BeginPlay %p vvvvvvvvvvvvv" ), this );

	//pre-script - in-game build/prep modification by script needs to happen here (no construction scripts called outside editor for loaded actors)
	PreConstructionScript();

	//opportunity for script to read creation parameters and write override parameters
	Super::BeginPlay();

	//post-script
	PostConstructionScript();
	
	LOG_ENTITY_EVENTS( LogApparance, Display, TEXT("^^^^^^^^^^^^ BeginPlay %p ^^^^^^^^^^^^"), this );
}

// ensure ticks in editor
//
bool AApparanceEntity::ShouldTickIfViewportsOnly() const
{
	return true;
}

// Actor update
//
void AApparanceEntity::Tick(float DeltaSeconds)
{
	//deferred init
	if(bPostLoadInitRequired)
	{
		PostLoadInit();
	}

	//deferred update
	if(bRebuildDeferred)
	{
		Rebuild();
	}

	//potential geometry change
	m_pEntityRendering->Tick(DeltaSeconds);

	//autoseed
	if(AutoSeed)
	{
		AutoSeedTimer -= DeltaSeconds;
		if(AutoSeedTimer<=0)
		{
			AutoSeedTimer = AutoSeedPeriod;
			
			//find and modify seed (first int parameter)
			const Apparance::IParameterCollection* ent_params = GetParameters();
			int num = ent_params->BeginAccess();
			int ifirstint = -1;
			int seed_value = 0;
			int param_id = 0;
			for(int i=0 ; i<num ; i++)
			{
				if(ent_params->GetType( i )==Apparance::Parameter::Integer)
				{
					ifirstint = i;
					ent_params->GetInteger( i, &seed_value );
					param_id = ent_params->GetID(i);
					break;
				}
			}
			ent_params->EndAccess();
			if(ifirstint!=-1)
			{
				seed_value++; //advance
				Apparance::IParameterCollection* ent_params_editable = BeginEditingParameters();
				ent_params_editable->SetInteger( ifirstint, seed_value );
				EndEditingParameters();
			}
			PostParameterEdit( ent_params, param_id );
		}
	}

#if APPARANCE_DEFERRED_REMOVAL
	//clear any deferred removal left over from circumstantial loss of generated content
	ClearDeferredRemovals();
#endif

	Super::Tick(DeltaSeconds);
}


//~ Begin IProceduralObject Interface
void AApparanceEntity::IPO_GetParameters(TArray<const Apparance::IParameterCollection*>& parameter_cascade, EParametersRole needed_parameters ) const
{
	GetParameters(parameter_cascade, needed_parameters);
}
bool AApparanceEntity::IPO_IsParameterOverridden( Apparance::ValueID input_id ) const
{
	return IsParameterOverridden( input_id );
}
bool AApparanceEntity::IPO_IsParameterDefault( Apparance::ValueID param_id ) const
{
	return IsParameterDefault( param_id );
}
FText AApparanceEntity::IPO_GenerateParameterLabel( Apparance::ValueID input_id ) const
{
	return GenerateParameterLabel( input_id );
}
FText AApparanceEntity::IPO_GenerateParameterTooltip( Apparance::ValueID input_id ) const
{
	return GenerateParameterTooltip( input_id );
}
void AApparanceEntity::IPO_SetProcedureID(int proc_id)
{
	SetProcedureID(proc_id);
}
Apparance::IParameterCollection* AApparanceEntity::IPO_BeginEditingParameters()
{
	return BeginEditingParameters();
}
const Apparance::IParameterCollection* AApparanceEntity::IPO_EndEditingParameters(bool dont_pack_interactive_edits)
{
	return EndEditingParameters(dont_pack_interactive_edits);
}
void AApparanceEntity::IPO_PostParameterEdit(const Apparance::IParameterCollection* params, Apparance::ValueID changed_param)
{
	PostParameterEdit(params, changed_param);
}
void AApparanceEntity::IPO_InteractiveParameterEdit(const Apparance::IParameterCollection* params, Apparance::ValueID changed_param)
{
	InteractiveParameterEdit(params, changed_param);
}

//~ End IProceduralObject Interface

// set proc/defaults according to a preset
//
void AApparanceEntity::ApplyPreset(class UApparanceEntityPreset* preset)
{
	//take on preset proc
	ProcedureID = preset->ProcedureID;
	SourcePreset = preset;

	//update	
	RebuildDeferred();
}

void AApparanceEntity::SetProcedureID(int procedure_id)
{
	if(procedure_id!=ProcedureID)
	{
		ProcedureID = procedure_id;
		RebuildDeferred();
	}
}
void AApparanceEntity::SetUseMeshInstancing( bool use_mesh_instancing )
{
	if(use_mesh_instancing!=bUseMeshInstancing)
	{
		bUseMeshInstancing = use_mesh_instancing;
		RebuildDeferred();
	}
}

// build or clear generated content
//
void AApparanceEntity::SetPopulated( bool is_populated )
{
	if(is_populated!=bPopulated)
	{
		bPopulated = is_populated;
		if(is_populated)
		{
			//trigger generation
			RebuildDeferred();
		}
		else
		{
			//dump content
			ClearContent();
		}
	}
}

// show or hide generated content
//
void AApparanceEntity::SetShown( bool is_shown )
{
	if(is_shown!=bShown)
	{
		bShown = is_shown;

		//apply to components
		RootComponent->SetVisibility( is_shown, true );
		
		//except for any collision		
		for(TMap<int, TWeakObjectPtr<class UProceduralMeshComponent>>::TIterator It( CollisionCache ); It; ++It)
		{
			if(It.Value().IsValid())
			{
				It.Value()->SetVisibility( false, false );
			}
		}
	}
}

// start re-geneneration of content
//
void AApparanceEntity::Rebuild()
{ 
	if(IsRunningUserConstructionScript())
	{
		//in the middle of potentially changing rebuild parameters
		//can't trigger yet, but want to ensure that we def build eventually
		RebuildDeferred();
	}
	else
	{
		//finilise and trigger the rebuild
		CompileUpdateParameters();
		TriggerGeneration();
	}
}

// request re-geneneration of content (next Tick)
//
void AApparanceEntity::RebuildDeferred()
{
	//UE_LOG(LogApparance, Log, TEXT("RebuileDeferred()"));

	bRebuildDeferred = true;
}

// update parameter object from persisted bin blob
//
void AApparanceEntity::UnpackParameters() const
{
	bool new_params = false;;
	if(!InstanceParameters.IsValid())
	{
		InstanceParameters = MakeShareable( FApparanceUnrealModule::GetEngine()->CreateParameterCollection() );
		new_params = true;
	}

	//fill in
	int num_bytes = InputParameterData.Num();
	unsigned char* pbytes = InputParameterData.GetData();
	InstanceParameters->SetBytes( num_bytes, pbytes );

	//validate against spec
	if(!new_params && num_bytes>0) //(only validate if already have data otherwise we can auto-override all preset params)
	{
		auto* plibrary = FApparanceUnrealModule::GetEngine()->GetLibrary();
		if(plibrary)
		{
			const Apparance::ProcedureSpec* proc_spec = plibrary->FindProcedureSpec( (Apparance::ProcedureID)ProcedureID );
			const Apparance::IParameterCollection* spec_params = proc_spec ? proc_spec->Inputs : nullptr;
			if(spec_params)
			{
				Apparance::IParameterCollection* ent_params = InstanceParameters.Get();
				if(ent_params)
				{
					//remap/tidy if proc inputs changed since last loaded
					ent_params->Sanitise( spec_params );
				}
			}
		}
	}
}

// store parameter object into bin blob for persistence
//
void AApparanceEntity::PackParameters()
{
	//update
	if(InstanceParameters.IsValid())
	{
		//get
		int byte_count = 0;
		const unsigned char* pdata = InstanceParameters->GetBytes( byte_count );

		//store
		InputParameterData.SetNum( byte_count );
		unsigned char* pstore = InputParameterData.GetData();
		FMemory::Memcpy( (void*)pstore, (void*)pdata, byte_count );
	}
	else
	{
		//none
		InputParameterData.Empty();
	}
}

// helper to find first frame for bounds calc
//
bool AApparanceEntity::FindFirstFrame( Apparance::Frame& frame_out, bool& worldspace_out, EApparanceFrameOrigin& frameorigin_out ) const
{
	bool found = false;

	//use final generation params for bounds
	const Apparance::IParameterCollection* ent_params = GenerationParameters.Get();
	Apparance::ValueID param_id = 0;
	if (ent_params)
	{
		int num = ent_params->BeginAccess();
		for (int i = 0; i < num; i++)
		{
			if (ent_params->GetType(i) == Apparance::Parameter::Frame)
			{
				//bounds
				ent_params->GetFrame(i, &frame_out);
				param_id = ent_params->GetID( i );
			
				found = true;
				break;
			}
		}
		ent_params->EndAccess();
	}

	//metadata
	if(found)
	{
		//need to search properly as don't know source of metadata (usually proc spec)
		TArray<const Apparance::IParameterCollection*> param_hierarchy;
		GetParameters( param_hierarchy, EParametersRole::ForDetails );
		for(int i = 0; i < param_hierarchy.Num(); i++)
		{
			param_hierarchy[i]->BeginAccess();
		}
#if APPARANCE_ENABLE_SPATIAL_MAPPING
		worldspace_out = FApparanceUtility::FindMetadataBool( &param_hierarchy, param_id, "Worldspace" );
#else
		worldspace_out = false;
#endif
		frameorigin_out = (EApparanceFrameOrigin)FApparanceUtility::FindMetadataInteger( &param_hierarchy, param_id, "Origin", (int)EApparanceFrameOrigin::Face );
		for(int i = 0; i < param_hierarchy.Num(); i++)
		{
			param_hierarchy[i]->EndAccess();
		}
	}

	return found;
}

// has this entity had a component of a particular type added procedurally?
//
bool AApparanceEntity::HasComponentOfType(const UClass* pcomponent_class) const
{
	for (int i = 0; i < ProceduralComponents.Num(); i++)
	{
		UActorComponent* pac = ProceduralComponents[i];
		if (pac && pac->GetClass() == pcomponent_class)
		{
			return true;
		}
	}
	return false;
}




// compose creation parameters from preceding parameter collections (Spec + Blueprint + Instance)
//
void AApparanceEntity::CompileCreationParameters()
{
	//proc inputs
	const Apparance::IParameterCollection* spec_params = GetSpecParameters();
	if (!spec_params) { return; }
	
	//list of contributing parameter sources (spec, BP, instance)
	TArray<const Apparance::IParameterCollection*> creation_param_cascade;
	GetParameters( creation_param_cascade, EParametersRole::ForCreation );
	
	//compile paramters	
	Apparance::IParameterCollection* out_params = FApparanceUnrealModule::GetEngine()->CreateParameterCollection();
	CollapseParameterCascade( creation_param_cascade, spec_params, out_params );
	
	//store for reference and blueprint access
	PlacementParameters->SetActorParameters( out_params );
	delete out_params;
}
void AApparanceEntity::CompileGenerationParameters()
{
	//proc inputs
	const Apparance::IParameterCollection* spec_params = GetSpecParameters();
	if (!spec_params) { return; }
	
	//list of contributing parameter sources (creation, overrides)
	TArray<const Apparance::IParameterCollection*> generation_param_cascade;
	GetParameters( generation_param_cascade, EParametersRole::ForGeneration );

	//ensure place to store (we now keep the final generation parameters around
	if (!GenerationParameters.IsValid())
	{
		GenerationParameters = MakeShareable( FApparanceUnrealModule::GetEngine()->CreateParameterCollection() );
	}
	
	//compile paramters	
	Apparance::IParameterCollection* out_params = GenerationParameters.Get();
	CollapseParameterCascade( generation_param_cascade, spec_params, out_params );
}
void AApparanceEntity::CompileUpdateParameters()
{
	//proc inputs
	const Apparance::IParameterCollection* spec_params = GetSpecParameters();
	if (!spec_params) { return; }
	
	//list of contributing parameter sources (creation, overrides)
	TArray<const Apparance::IParameterCollection*> update_param_cascade;
	GetParameters( update_param_cascade, EParametersRole::ForUpdate );
	
	//ensure place to store (we now keep the final generation parameters around)
	if (!GenerationParameters.IsValid())
	{
		GenerationParameters = MakeShareable( FApparanceUnrealModule::GetEngine()->CreateParameterCollection() );
	}
	
	//compile paramters	
	Apparance::IParameterCollection* out_params = GenerationParameters.Get();
	CollapseParameterCascade( update_param_cascade, spec_params, out_params );
}

#if ENABLE_PROC_BUILD_PARAMETERS
static void DumpParameters( const Apparance::IParameterCollection* p )
{
	int num_params = p->BeginAccess();
	for(int i = 0; i < num_params; i++)
	{
		//apply mapping if needed by type
		Apparance::Parameter::Type type = p->GetType( i );
		FString type_name;
		FString value;
		switch(type)
		{
			case Apparance::Parameter::Float:
			{
				type_name = TEXT( "Float" );
				float v;
				if(p->GetFloat( i, &v ))
				{
					value = FString::Printf( TEXT( "%f" ), v );
				}
				break;
			}
			case Apparance::Parameter::Integer:
			{
				type_name = TEXT( "Int" );
				int v;
				if(p->GetInteger( i, &v ))
				{
					value = FString::Printf( TEXT( "%i" ), v );
				}
				break;
			}
			case Apparance::Parameter::String:
			{
				type_name = TEXT( "String" );
				wchar_t str_bfr[2048];
				int str_len;
				if(p->GetString( i, 1024, str_bfr, &str_len ))
				{
					value = WCHAR_TO_TCHAR( str_bfr );
				}
				break;
			}
			case Apparance::Parameter::Bool: type_name = TEXT( "Bool" ); break;
			case Apparance::Parameter::Colour: type_name = TEXT( "Colour" ); break;
			case Apparance::Parameter::Frame: type_name = TEXT( "Frame" ); break;
			case Apparance::Parameter::List: type_name = TEXT( "List" ); break;
			case Apparance::Parameter::Vector2: type_name = TEXT( "Vector2" ); break;
			case Apparance::Parameter::Vector3: type_name = TEXT( "Vector3" ); break;
			case Apparance::Parameter::Vector4: type_name = TEXT( "Vector4" ); break;
			default: type_name = TEXT( "None" ); break;
		}
		UE_LOG( LogApparance, Log, TEXT( "    [%i] %s %s %s" ), i, *type_name, value.IsEmpty() ? TEXT( "" ) : TEXT( "=" ), *value );
	}

	//done
	p->EndAccess();
}
#endif

void AApparanceEntity::TriggerGeneration()
{
	//handled, or not needed now
	bRebuildDeferred = false;

	//checks	
	auto* pengine = FApparanceUnrealModule::GetEngine();
	auto* plibrary = pengine?pengine->GetLibrary():nullptr;
	if ((!pengine || !plibrary) //no apparance (yet)
		|| HasAnyFlags( RF_ClassDefaultObject ) //default objects are abstract, don't build
		|| !IsValidChecked(this) //dead object
		|| !bPopulated  //doesn't want to be populated at this time
		|| ProcedureID==Apparance::InvalidID //no procedure set
		|| !GenerationParameters.IsValid()) //no generation params calculated
	{
		return;
	}

	//assemble what the generation process needs
	Apparance::IClosure* proc = pengine->CreateClosure( ProcedureID );
	Apparance::IParameterCollection* out_params = proc->GetParameters();
	out_params->Sync( GenerationParameters.Get() );

	//remap origin/etc parameters
	const Apparance::IParameterCollection* spec_params = GetSpecParameters();
	ApplyParameterMappings( out_params, spec_params );
	
#if ENABLE_PROC_BUILD_DIAGNOSTICS
	int proc_id = (int)proc->GetID();
	if(proc_id==0x6f6cd891)
	{	
		UE_LOG( LogApparance, Log, TEXT( "%s @ frame %i : BUILDING MAZE........." ), *GetName(), GFrameCounter );
	}
	const Apparance::ProcedureSpec* proc_spec = plibrary->FindProcedureSpec( proc_id );
	FString proc_name;
	if(proc_spec)
	{
		proc_name = proc_spec->Name;
	}
#if WITH_EDITOR
	FString actor_label = GetActorLabel( false );
#else
	FString actor_label;
#endif
	UE_LOG( LogApparance, Log, TEXT( "%s : TRIGGER GENERATION OF %08x \"%s\" (%s) with parameters:" ), *GetName(), proc_id, *proc_name, actor_label.IsEmpty()?TEXT(""): *actor_label );
#if ENABLE_PROC_BUILD_PARAMETERS
	DumpParameters( out_params );
#endif
#endif

	//trigger generation
	m_pEntityRendering->SetProcedure( proc );
}


// Flatten a cascade of parameter collections into an output collection such that:
// 1. output contains all the parameters in an example template
// 2. output paramaters take the value of the first occurrance of that parameter in the cascade (first to last)
//
void AApparanceEntity::CollapseParameterCascade( 
	TArray<const Apparance::IParameterCollection*>& parameters_cascade, //list of parameter collections to scan looking for overrides
	const Apparance::IParameterCollection* template_params,	//full list of parameter types to cover
	Apparance::IParameterCollection* out_params )		//list expected to contain all template parameters from first occurrance in cascade
{
	//sync to template first
	int num_parameters = 0;
	if(template_params)
	{
		num_parameters = out_params->Sync( template_params );	//init to expected set, also gives us the param count
	}
	
	//open param collections
	bool template_ready = false;
	for (int i = 0; i < parameters_cascade.Num(); i++)
	{
		parameters_cascade[i]->BeginAccess();
		if(parameters_cascade[i] == template_params)
		{
			template_ready = true;
		}
	}
	out_params->BeginEdit();
	if(!template_ready)
	{
		template_params->BeginAccess();
	}
	
	//params scan
	for(int param_index=0 ; param_index<num_parameters ; param_index++)
	{
		//find input spec
		Apparance::Parameter::Type input_type = template_params->GetType( param_index );
		Apparance::ValueID input_id = template_params->GetID( param_index );
		const char* input_name = template_params->GetName( param_index );
		
		//set value
		bool ok = false;
		switch (input_type)
		{
			case Apparance::Parameter::Bool:
			{
				bool v = false;
				for (int i = 0; i < parameters_cascade.Num(); i++)
				{
					if (parameters_cascade[i]->FindBool(input_id, &v))
					{
						break; //found
					}
				}
				ok = out_params->SetBool( param_index, v );
				break;
			}
			case Apparance::Parameter::Colour:
			{
				Apparance::Colour c = { 0,0,0,0 };
				for (int i = 0; i < parameters_cascade.Num(); i++)
				{
					if (parameters_cascade[i]->FindColour(input_id, &c))
					{
						break; //found
					}
				}
				ok = out_params->SetColour( param_index, &c );
				break;
			}
			case Apparance::Parameter::Float:
			{
				float v = 0;
				for (int i = 0; i < parameters_cascade.Num(); i++)
				{
					if (parameters_cascade[i]->FindFloat(input_id, &v))
					{
						break; //found
					}
				}
				ok = out_params->SetFloat( param_index, v );
				break;
			}
			case Apparance::Parameter::Frame:
			{
				Apparance::Frame f;
				for (int i = 0; i < parameters_cascade.Num(); i++)
				{
					if (parameters_cascade[i]->FindFrame(input_id, &f))
					{
						break; //found
					}
				}
				ValidateFrame( f );
				ok = out_params->SetFrame( param_index, &f );
				break;
			}
			case Apparance::Parameter::Integer:
			{
				int v = 0;
				for (int i = 0; i < parameters_cascade.Num(); i++)
				{
					if (parameters_cascade[i]->FindInteger(input_id, &v))
					{
						break; //found
					}
				}
				ok = out_params->SetInteger( param_index, v );
				break;
			}
			case Apparance::Parameter::String:
			{
				FString str_val;
				TCHAR* pbuffer = nullptr;
				int str_len = 0;
				for (int i = 0; i < parameters_cascade.Num(); i++)
				{
					if(parameters_cascade[i]->FindString( input_id, 0, nullptr, &str_len ))
					{
						//retrieve string
						str_val = FString::ChrN( str_len, TCHAR(' ') );	
						pbuffer = str_val.GetCharArray().GetData();
						parameters_cascade[i]->FindString( input_id, str_len, pbuffer );				
						break; //found
					}
				}
				if(pbuffer)
				{
					ok = out_params->SetString(param_index, str_len, pbuffer);
				}
				break;
			}
			/*case Apparance::Parameter::Vector2:
			{
			Apparance::Vector2 v = { 0,0 };
			proc->SetValue( input_id, &v );
			break;
			}*/
			case Apparance::Parameter::Vector3:
			{
				Apparance::Vector3 v = { 0,0,0 };
				for (int i = 0; i < parameters_cascade.Num(); i++)
				{
					if (parameters_cascade[i]->FindVector3(input_id, &v))
					{
						break; //found
					}
				}
				ok = out_params->SetVector3( param_index, &v );
				break;
			}
			/*case Apparance::Parameter::Vector4:
			{
			Apparance::Vector4 v = { 0,0,0,0 };
			proc->SetValue( input_id, &v );
			break;
			}*/
			case Apparance::Parameter::List:
			{
				const Apparance::IParameterCollection* src_list = nullptr;
				for(int i = 0; i < parameters_cascade.Num(); i++)
				{
					src_list = parameters_cascade[i]->FindList( input_id );
					if(src_list)
					{
						break; //found
					}
				}
				Apparance::IParameterCollection* dst_list = out_params->SetList( param_index );
				if(dst_list)
				{
					dst_list->Merge( src_list );
					ok = true;
				}
				break;
			}
			default:
			{
				UE_LOG(LogApparance,Warning,TEXT("Unknown parameter type %i (id %i, name %i)"), input_type, input_id, input_name );
				break;
			}
		}
		if(!ok)
		{
			UE_LOG(LogApparance,Warning,TEXT("Failed to set parameter %i (id %i, name %i)"), param_index, input_id, input_name );
			break;
		}
	}
	
	//close param collections
	if(!template_ready)
	{
		template_params->EndAccess();
	}
	out_params->EndEdit();
	for (int i = 0; i < parameters_cascade.Num(); i++)
	{
		parameters_cascade[i]->EndAccess();
	}
}

// check frame over for issues, and fix them
//
void AApparanceEntity::ValidateFrame( Apparance::Frame& frame )
{
	//restore axes that have become empty
	if(frame.Orientation.X.X == 0 && frame.Orientation.X.Y == 0 && frame.Orientation.X.Z == 0)
	{
		frame.Orientation.X.X = 1;
	}
	if(frame.Orientation.Y.X == 0 && frame.Orientation.Y.Y == 0 && frame.Orientation.Y.Z == 0)
	{
		frame.Orientation.Y.Y = 1;
	}
	if(frame.Orientation.Z.X == 0 && frame.Orientation.Z.Y == 0 && frame.Orientation.Z.Z == 0)
	{
		frame.Orientation.Z.Z = 1;
	}

	//others...
}



// find out if visual or collision geometry needed?
//
void AssessGeometry( AApparanceEntity* pactor, Apparance::Host::IGeometry* geometry, int tier_index, bool& geometry_present, bool& collision_present )
{
	FEntityRendering* pentityrendering = pactor->GetEntityRendering();
	geometry_present = false;
	collision_present = false;

	//convert parts to meshes
	FApparanceGeometry* pgeom = (FApparanceGeometry*)geometry;	//upcast to known internal type
	const TArray<class FApparanceGeometryPart*>& parts = pgeom->GetParts();
	for(int i = 0; i < parts.Num(); i++)
	{
		FApparanceGeometryPart* part = parts[i];

		//material info
		bool want_collision = false;
		class UMaterialInterface* material_instance = pentityrendering->GetMaterial( part->Material, part->Parameters, part->Textures, tier_index, &want_collision );

		//geometry	
		if(want_collision && !material_instance)
		{
			//just collision? doesn't render
			collision_present = true;
			if(geometry_present) //both, don't need to keep checking
			{
				break;
			}
		}
		else
		{
			//geometry, possibley collision
			geometry_present = true;
			collision_present = want_collision;
			if(collision_present) //both, don't need to keep checking
			{
				break;
			}
		}
	}
}


#if LOG_GEOMETRY_ADD_STATS
extern int nGenLogParts;
extern int nGenLogTriangles;
extern int nGenLogVertices;
extern int nGenLogCollisionParts;
extern int nGenLogCollisionTriangles;
extern int nGenLogCollisionVertices;
#endif


// set up from geometry content
// tier is needed for corrent material instance setup and tracking
//
void SetGeometry(UProceduralMeshComponent* pmc_geometry, UProceduralMeshComponent* pmc_collision, Apparance::Host::IGeometry* geometry, int tier_index)
{
	AApparanceEntity* pactor = CastChecked<AApparanceEntity>( pmc_geometry?pmc_geometry->GetOwner():pmc_collision->GetOwner() );
	FEntityRendering* pentityrendering = pactor->GetEntityRendering();

	//convert parts to meshes
	FApparanceGeometry* pgeom = (FApparanceGeometry*)geometry;	//upcast to known internal type
	const TArray<class FApparanceGeometryPart*>& parts = pgeom->GetParts();
	for (int i = 0; i < parts.Num(); i++)
	{
		FApparanceGeometryPart* part = parts[i];		

		//conversion
		int num_v = part->Positions.Num();
		TArray<FColor> int_colours;
		int_colours.AddZeroed( num_v );
		TArray<FProcMeshTangent> tangents;
		tangents.AddZeroed( num_v );
		for (int v = 0; v < num_v ; v++)
		{
			//3D space transform
			part->Positions[v] = UNREALHANDEDNESS_FROM_APPARANCEHANDEDNESS(part->Positions[v]);
			part->Normals[v] = UNREALHANDEDNESS_FROM_APPARANCEHANDEDNESS(part->Normals[v]);

			//type conversions
			tangents[v].TangentX = UNREALHANDEDNESS_FROM_APPARANCEHANDEDNESS( part->Tangents[v] );
			tangents[v].bFlipTangentY = true;	//apparance maps bottom up?
			int_colours[v] = part->Colours[v].QuantizeRound();
		}

		//material info
		bool want_collision = false;
		class UMaterialInterface* material_instance = pentityrendering->GetMaterial( part->Material, part->Parameters, part->Textures, tier_index, &want_collision );

		//geometry	
		if(want_collision && !material_instance)
		{
			//just collision? doesn't render
			if(pmc_collision)
			{
				pmc_collision->CreateMeshSection( i, part->Positions, part->Triangles, part->Normals, part->UVs[0], part->UVs[1], part->UVs[2], part->UVs[2/*only three available*/], int_colours, tangents, true );
				GENLOG_ACC( nGenLogCollisionVertices, num_v )
				GENLOG_INC( nGenLogCollisionParts )
				GENLOG_ACC( nGenLogCollisionTriangles, part->Triangles.Num() / 3 )
			}
			else
			{
				UE_LOG( LogApparance, Warning, TEXT( "SetGeometry inconsistent collision presence logic in %s" ), *pactor->GetName() );
			}
		}
		else
		{
			if(pmc_geometry)
			{
				//geometry, maybe collision
				pmc_geometry->CreateMeshSection( i, part->Positions, part->Triangles, part->Normals, part->UVs[0], part->UVs[1], part->UVs[2], part->UVs[2/*only three available*/], int_colours, tangents, want_collision );
				GENLOG_INC( nGenLogParts )
				GENLOG_ACC( nGenLogVertices, num_v )
				GENLOG_ACC( nGenLogTriangles, part->Triangles.Num() / 3 )
				if(want_collision)
				{
					GENLOG_INC( nGenLogCollisionParts )
					GENLOG_ACC( nGenLogCollisionVertices, num_v )
					GENLOG_ACC( nGenLogCollisionTriangles, part->Triangles.Num() / 3 )
				}
				//materials
				pmc_geometry->SetMaterial( i, material_instance );
			}
			else
			{
				UE_LOG( LogApparance, Warning, TEXT( "SetGeometry inconsistent geometry presence logic in %s" ), *pactor->GetName() );
			}
		}
	}
}

//start of geometry update phase
//
void AApparanceEntity::BeginGeometryUpdate()
{

}

//end of geometry update phase
void AApparanceEntity::EndGeometryUpdate(int tier_index)
{
	if (tier_index >= 0)	//workaround to avoid instance mesh thrashing by editing content changes (we don't currently distinguish between main, tiers, or editor content))
	{
	//update instanced static meshes with accumulated mesh state
		if (bUseMeshInstancing)
	{
		for (int i = 0; i < InstanceMeshCache.Num(); i++)
		{
			//for each mesh
			FInstancedMeshCacheEntry& cache_entry = InstanceMeshCache[i];
			UInstancedStaticMeshComponent* pismc = cache_entry.InstancedMeshes.Get();
			TArray<FTransform>* pending = cache_entry.PendingInstances.Get();
			if (pismc)
			{
				//change required?
				int num_current = pismc->GetInstanceCount();
				int num_needed = pending ? pending->Num() : 0;
				int to_add = num_needed - num_current;
				int to_remove = -to_add;
				
				//resize
					if (to_add > 0)
				{
					//add
#if UE_VERSION_AT_LEAST(5,1,0)
						pismc->PreAllocateInstancesMemory(to_add);
					TArray<FTransform> adding;
						adding.AddDefaulted(to_add);
						pismc->AddInstances(adding, false, false);
#else
					while (num_current < num_needed)
					{
						pismc->AddInstance(FTransform());
						num_current++;
					}
#endif
				}
					else if (to_remove > 0)
				{
					if (num_needed == 0)
					{
						pismc->ClearInstances();
					}
					else
					{
						//shrink
#if UE_VERSION_AT_LEAST(5,1,0)
						TArray<int32> removing;
							for (int idx = num_current - 1; idx >= num_needed; idx--)
						{
								removing.Add(idx);
						}
							pismc->RemoveInstances(removing);
#else
							while (num_current > num_needed)
						{
							num_current--;
								pismc->RemoveInstance(num_current);
						}
#endif
					}
				}
				
				//apply transforms, in bulk
				const auto* transforms = pending;
				if(!transforms) //protect against previously dumped list
				{
					static TArray<FTransform> empty;
					transforms = &empty;
				}
				if (pending)
				{				
					pismc->BatchUpdateInstancesTransforms(0, *transforms, false, true, true/*teleport to avoid blur*/);
				}

				//done with them now, dump
				cache_entry.PendingInstances.Reset();
			}
		}
	}
	else
	{
		//not using, ensure any prev use is cleared out
		RemoveAllInstancedMeshes();
	}
	}

	//fire completion event (for BPs)
	ReceiveGenerationComplete();
}

// entity rendering geometry access
class UProceduralMeshComponent* AApparanceEntity::AddGeometry(Apparance::Host::IGeometry* geometry, int tier_index, FVector unreal_offset, UProceduralMeshComponent*& pcollision_mesh_out )
{
	FApparanceGeometry* pag = (FApparanceGeometry*)geometry;	//upcast to known internal type

	//create container for this geometry
	FName geometry_name = NAME_None;
	FName collision_name = NAME_None;
#if WITH_EDITOR
	//need to disambiguate name by tier to avoid re-use between tiers (same name means same object in Unreal)
	geometry_name = FName( "ProceduralGeometry", tier_index );
	collision_name = FName( "ProceduralCollision", tier_index );
#endif

	//pre-scan to work out what component(s) we may need
	bool geometry_present, collision_present;
	AssessGeometry( this, geometry, tier_index, geometry_present, collision_present );

	UProceduralMeshComponent* pgeometry = nullptr;
	if(geometry_present)
	{
		pgeometry = NewObject<UProceduralMeshComponent>( this, geometry_name, RF_Transient | RF_DuplicateTransient);
		check( pgeometry );
	}
	UProceduralMeshComponent* pcollision = nullptr;
	if(collision_present)
	{
		pcollision = NewObject<UProceduralMeshComponent>( this, collision_name, RF_Transient | RF_DuplicateTransient);
		check( pcollision );
	}
	//	UE_LOG( LogApparance, Display, TEXT( "********** New PMC %p (entity %p)" ), pcomponent, this );

	//disable (costly) overlaps and synchronous physics calcs during setup
	bool geom_does_overlaps = false;
	if(pgeometry)
	{
		pgeometry->bUseAsyncCooking = true; //dynamic content, we can accept delay instead of hitching game thread
		geom_does_overlaps = pgeometry->GetGenerateOverlapEvents();
		pgeometry->SetGenerateOverlapEvents( false );
		//but provide collision info (when adding sections)
		pgeometry->BodyInstance.UseExternalCollisionProfile(CollisionSetup); //needed in case geom is used for collision as well
	}
	bool coll_does_overlaps = false;
	if(pcollision)
	{
		pcollision->bUseAsyncCooking = true; //dynamic content, we can accept delay instead of hitching game thread
		coll_does_overlaps = pcollision->GetGenerateOverlapEvents();
		pcollision->SetGenerateOverlapEvents( false );
		//but provide collision info (when adding sections)
		pcollision->BodyInstance.UseExternalCollisionProfile(CollisionSetup);
	}

	//geometry
	LOG_ENTITY_EVENTS( LogApparance, Display, TEXT( "ADD GEOMETRY" ) );
	if(pgeometry || pcollision)
	{
		check( geometry_present == !!pgeometry );
		check( collision_present == !!pcollision );
		SetGeometry( pgeometry, pcollision, geometry, tier_index );
	}

	//transform/etc
	FAttachmentTransformRules KeepRelativeTransformWeld( EAttachmentRule::KeepRelative, true );
	if(pgeometry)
	{
#if defined(PROCEDURAL_MESH_COMPONENT_SUPPORTS_FULL_PRECISION_UV_PROPERTY) //custom build needed to support this fix
		pgeometry->bUseFullPrecisionUVs = true;	//fix distortion on very thin/long triangles (such as generated by polygon clipper on curves)
#endif
		pgeometry->SetRelativeLocation( unreal_offset );
		pgeometry->SetRelativeScale3D( UNREALSCALE_FROM_APPARANCESCALE3( FVector::OneVector ) );
		pgeometry->AttachToComponent( GetRootComponent(), KeepRelativeTransformWeld );
		pgeometry->SetVisibility( bShown, true );
		if(FApparanceUnrealModule::GetModule()->IsGameRunning())
		{
			pgeometry->SetMobility( RootComponent->Mobility );
		}
	}
	if(pcollision)
	{
		pcollision->SetRelativeLocation( unreal_offset );
		pcollision->SetRelativeScale3D( UNREALSCALE_FROM_APPARANCESCALE3( FVector::OneVector ) );
		pcollision->AttachToComponent( GetRootComponent(), KeepRelativeTransformWeld );
		pcollision->SetVisibility( false, true );	//collision not rendered
		pcollision->CanCharacterStepUpOn = ECB_Yes;
		if(FApparanceUnrealModule::GetModule()->IsGameRunning())
		{
			pcollision->SetMobility( RootComponent->Mobility );
		}
	}

	//pcollision->bUseComplexAsSimpleCollision = true;
	//pcollision->SetSimulatePhysics( true );
	//SetActorEnableCollision( true );

	//CDO doesn't always have a world :/
//	AActor* MyOwner = pcomponent->GetOwner(); //(test taken from UActorComponent::RegisterComponent)
//	UWorld* MyOwnerWorld = MyOwner?MyOwner->GetWorld():nullptr;
//	if(MyOwnerWorld)
	{
		//now register (after all the messing about above)
		// * prevents double proxy creation
		// * prevents movement/scale artifacts triggering motion blur fx
		if(pgeometry)
		{
			pgeometry->RegisterComponent();
		}
		if(pcollision)
		{
			pcollision->RegisterComponent();
		}
	}

	if(pgeometry)
	{
		pgeometry->SetGenerateOverlapEvents( geom_does_overlaps );
	}
	if(pcollision)
	{
		pcollision->SetGenerateOverlapEvents( coll_does_overlaps );
	}

#if APPARANCE_DEFERRED_REMOVAL
	//clear any deferred removal now we have replacement geometry
	ClearDeferredRemovals();
#endif

	if(pgeometry && pgeometry->SceneProxy)
	{
//		UE_LOG( LogApparance, Display, TEXT( "                    Scene Proxy %p" ), pcomponent->SceneProxy );
	}
	if(pcollision && pcollision->SceneProxy)
	{
//		UE_LOG( LogApparance, Display, TEXT( "                    Scene Proxy %p" ), pcomponent->SceneProxy );
	}

	if(pgeometry)
	{
		ProceduralComponents.AddUnique( pgeometry );
	}
	if(pcollision)
	{
		ProceduralComponents.AddUnique( pcollision );
	}
	pcollision_mesh_out = pcollision;
	return pgeometry;
}


void AApparanceEntity::RemoveGeometry(class UProceduralMeshComponent* pcomponent)
{
	if(pcomponent)
	{
#if APPARANCE_DEFERRED_REMOVAL
		DeferredGeometryRemoval.Add( pcomponent );
#else
		RemoveGeometryInternal( pcomponent );
#endif
	}
}

void AApparanceEntity::RemoveGeometryInternal( class UProceduralMeshComponent* pcomponent )
{
	//UE_LOG( LogApparance, Display, TEXT( "********** Destroy PMC %p (entity %p)" ), pcomponent, this );
	if(pcomponent->SceneProxy)
	{
//		UE_LOG( LogApparance, Display, TEXT( "                    Scene Proxy %p" ), pcomponent->SceneProxy );
	}
	ProceduralComponents.Remove( pcomponent );

	pcomponent->SetGenerateOverlapEvents( false );	//prevent cleanup events
	pcomponent->DestroyComponent();
}

void AApparanceEntity::ClearDeferredRemovals()
{
	for(int i = 0; i < DeferredGeometryRemoval.Num(); i++)
	{
		RemoveGeometryInternal( DeferredGeometryRemoval[i] );
	}
	DeferredGeometryRemoval.Empty();
}

// general actor component adding
class UActorComponent* AApparanceEntity::AddActorComponent(const class UApparanceResourceListEntry_Component* psource, FMatrix& local_placement)
{
	UActorComponent* pcomp = nullptr;
	
	//testing mesh placement
	if (psource && psource->ComponentTemplate)
	{
		pcomp = DuplicateObject<UActorComponent>( psource->ComponentTemplate, this );

		//disable (costly) overlaps during setup
		UPrimitiveComponent* pprim_comp = Cast<UPrimitiveComponent>( pcomp );
		bool does_overlaps = false;
		if(pprim_comp)
		{
			does_overlaps = pprim_comp->GetGenerateOverlapEvents();
			pprim_comp->SetGenerateOverlapEvents( false );	
		}

		//other setup
		pcomp->SetFlags(RF_Transient | RF_DuplicateTransient);
		pcomp->RegisterComponent();

		//3d rendering component?
		USceneComponent* pscene_comp = Cast<USceneComponent>(pcomp);
		if (pscene_comp)
		{
			pscene_comp->SetVisibility(bShown, true);
			pscene_comp->SetRelativeTransform( FTransform( local_placement ), false, nullptr, ETeleportType::TeleportPhysics );
			pscene_comp->AttachToComponent( GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform );
		}

		if(pprim_comp)
		{
			pprim_comp->SetGenerateOverlapEvents( does_overlaps );
		}
	}
	
	if(pcomp)
	{
		ProceduralComponents.Add( pcomp );
	}
	return pcomp;
}

// removal of general components
void AApparanceEntity::RemoveActorComponent(class UActorComponent* pcomponent)
{
	if (pcomponent)
	{
		//try to prevent costly overlap updates
		UPrimitiveComponent* pprim_comp = Cast<UPrimitiveComponent>( pcomponent );
		if(pprim_comp)
		{
			pprim_comp->SetGenerateOverlapEvents( false );
		}

		ProceduralComponents.Remove(pcomponent);
		pcomponent->DestroyComponent();
	}	
}


// entity rendering mesh access
class UStaticMeshComponent* AApparanceEntity::AddMesh( UStaticMesh* psource, FMatrix& local_placement)
{
	UStaticMeshComponent* pmesh = nullptr;

	//testing mesh placement
	if (psource)
	{
		FBoxSphereBounds bounds = psource->GetBounds();
		FVector extent = bounds.BoxExtent;
		FVector centre = bounds.Origin;

		pmesh = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient | RF_DuplicateTransient);

		//disable (costly) overlaps during setup
		bool does_overlaps = pmesh->GetGenerateOverlapEvents();
		pmesh->SetGenerateOverlapEvents( false );

		//other setup
		pmesh->RegisterComponent();
		pmesh->SetStaticMesh(psource);
#if WITH_EDITOR
		pmesh->SetVisibility( bShown, true ); //hide if required
		pmesh->SetGenerateOverlapEvents( does_overlaps );
		pmesh->SetRelativeTransform( FTransform( local_placement ), false, nullptr, ETeleportType::ResetPhysics );
		if(FApparanceUnrealModule::GetModule()->IsGameRunning())	//before move (ironically) to prevent motion blur
		{
			pmesh->SetMobility( RootComponent->Mobility );
		}
		pmesh->AttachToComponent( GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform );
#else //standalone
		pmesh->SetVisibility( bShown, true ); //hide if required
		pmesh->SetGenerateOverlapEvents( does_overlaps );
		pmesh->SetRelativeTransform( FTransform( local_placement ), false, nullptr, ETeleportType::ResetPhysics );
		pmesh->SetMobility( RootComponent->Mobility );
		pmesh->AttachToComponent( GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform );
#endif
	}

	ProceduralComponents.Add( pmesh );
	return pmesh;
}

static int meshHandleGenerator = 0;
static TSet<UStaticMesh*> once_only;

// entity rendering instanced mesh access
FMeshInstanceHandle AApparanceEntity::AddInstancedMesh(UStaticMesh* psource, FMatrix& local_placement)
{
	FMeshInstanceHandle mesh_handle;

	if (psource)
	{
		//find instanced mesh for this mesh
		int cache_index = 0;
		FInstancedMeshCacheEntry cache_entry;
		const int* pcache_index = InstancedMeshCacheLookup.Find(psource);
		if (pcache_index)
		{
			//refer to existing
			cache_index = *pcache_index;
			cache_entry = InstanceMeshCache[cache_index];
			if (!cache_entry.PendingInstances)
			{
				 cache_entry.PendingInstances = MakeShareable( new TArray<FTransform>() );
				 InstanceMeshCache[cache_index] = cache_entry;
			}
		}
		else
		{
			//create on demand
			cache_entry.Mesh = psource;
			UInstancedStaticMeshComponent* pism = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transient | RF_DuplicateTransient);

			//disable (costly) overlaps during setup
			bool does_overlaps = pism->GetGenerateOverlapEvents();
			pism->SetGenerateOverlapEvents( false );

			pism->RegisterComponent();
			pism->SetStaticMesh(psource);

#if UE_VERSION_AT_LEAST(5,1,0)
			bool need_collision_query = psource->IsNavigationRelevant(); //optim by completely disabling physics for e.g. foliage
			pism->GetBodyInstance()->SetCollisionEnabled( need_collision_query ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision );
			if(!once_only.Contains( psource ))
			{
				once_only.Add( psource );
				//UE_LOG( LogApparance, Display, TEXT( "STATIC MESH NAV RELEVANT: %s : %s" ), *psource->GetName(), need_collision_query ? TEXT( "YES" ) : TEXT( "NO" ), *psource->GetPathName() );
			}
#endif

			pism->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
			pism->SetVisibility(bShown, true);

			pism->SetGenerateOverlapEvents( does_overlaps );
			if(FApparanceUnrealModule::GetModule()->IsGameRunning())
			{
				pism->SetMobility( RootComponent->Mobility );
			}
			//navmesh support of instanced meshes needs this. TODO: make optional, per resource, later if cost implications
			pism->bNavigationRelevant = true;

			//record
			cache_entry.InstancedMeshes = pism;
			cache_index = InstanceMeshCache.Num();
			cache_entry.PendingInstances = MakeShareable( new TArray<FTransform>() );
			InstanceMeshCache.Add(cache_entry);
			InstancedMeshCacheLookup.Add(psource, cache_index);

			ProceduralComponents.Add( pism );			
		}
		mesh_handle.MeshType = cache_index;

		//add entry for it
		if (cache_entry.InstancedMeshes.IsValid())
		{
			//add (pend)
			cache_entry.PendingInstances->Add( FTransform( local_placement ) );
			mesh_handle.InstanceIndex = meshHandleGenerator;
			meshHandleGenerator++;
		}
	}

	return mesh_handle;
}

void AApparanceEntity::RemoveInstancedMesh(FMeshInstanceHandle mesh_handle)
{
#if false //only one, is recyled
	//find instanced mesh for this mesh
	FInstancedMeshCacheEntry& cache_entry = InstanceMeshCache[mesh_handle.MeshType];
	if (cache_entry.InstancedMeshes && cache_entry.InstancedMeshes->GetInstanceCount()>0)	//(clear only once)
	{
		//request to remove any removes all (because we can't rely on the indices to remain consistent as they are removed)		
		cache_entry.InstancedMeshes->ClearInstances();
	}
#endif
}

// proper clearout of ISMC
//
void AApparanceEntity::RemoveAllInstancedMeshes()
{
	//instances
	for(int i=0 ; i<InstanceMeshCache.Num() ; i++)
	{
		UInstancedStaticMeshComponent* pismc = InstanceMeshCache[i].InstancedMeshes.Get();
		if(pismc)
		{
			ProceduralComponents.Remove( pismc );
			pismc->SetGenerateOverlapEvents( false ); //try to prevent costly overlap updates
			pismc->DestroyComponent();
		}
	}
	InstanceMeshCache.Empty();

	//other tracking structures
	InstancedMeshCacheLookup.Empty();
	InstanceCache.Empty();
}



void AApparanceEntity::RemoveMesh(class UStaticMeshComponent* pcomponent)
{
	if (pcomponent)
	{
		ProceduralComponents.Remove(pcomponent);
		pcomponent->SetGenerateOverlapEvents( false ); //try to prevent costly overlap updates
		pcomponent->DestroyComponent();
	}
}

// entity rendering blueprint access
class AActor* AApparanceEntity::AddBlueprint_Begin( UBlueprintGeneratedClass* ptemplateclass, FMatrix& local_placement)
{
	AActor* pactor = nullptr;

//	UE_LOG( LogApparance, Log, TEXT( "%s : BEGIN Placing Blueprint, Template: %s" ), *GetName(), ptemplateclass ? *ptemplateclass->GetName() : TEXT( "null" ) );

	//testing mesh placement
	if (ptemplateclass)
	{
		//spawn
		FActorSpawnParameters asp;
		asp.ObjectFlags = RF_Transient | RF_DuplicateTransient;
		asp.Owner = this;
		asp.bNoFail = true;
#if WITH_EDITOR
		asp.bHideFromSceneOutliner = !bShowGeneratedContent;
#endif
		asp.bDeferConstruction = true; //manual construction script manually called later (to allow parameter injection), via UGameplayStatics::FinishSpawningActor
		FTransform t(local_placement);
		//NOTE: can't use SpawnActorDeferred because we need some custom spawn parameters but this just uses the deferred flag
		pactor = GetWorld()->SpawnActor( ptemplateclass, &t, asp );	//ensure transform set on spawn as can't move Static stuff after
		
#if ENABLE_PROC_BUILD_DIAGNOSTICS
		UE_LOG( LogApparance, Log, TEXT( "%s : ADD Blueprint, Template: %s -> Actor: %s" ), *GetName(), ptemplateclass ? *ptemplateclass->GetName() : TEXT( "null" ), pactor?*pactor->GetName():TEXT("failed") );
#endif
//		UE_LOG( LogApparance, Display, TEXT("********** Spawned actor %p"), pactor );

		//track
		ProceduralActors.Add( pactor );

		//NOTE: now require AddBlueprint_Attach to be called separately since construction script is run later and is needed to happen before
	}

	return pactor;
}
// second stage of adding a bluerint
void AApparanceEntity::AddBlueprint_End( AActor* pactor, FMatrix& local_placement, const Apparance::IParameterCollection* placement_parameters )
{
	//disable overlaps
	ScopedOverlapDisable disable_overlaps_temporarily( pactor );

//	UE_LOG( LogApparance, Log, TEXT( "%s : END Placing Blueprint, Actor: %s" ), *GetName(), pactor ? *pactor->GetName() : TEXT( "null" ) );

	//store parameters for bp access
	PlacementParameters->AddBlueprintActorParameters( pactor, placement_parameters );	//follow up to SpawnActorDeferred

#if !WITH_EDITOR
	//can't intercept rerunconstruction scripts to handle parameter setup in-game, but in-game this is the only place it's needed
	AApparanceEntity* pentity = Cast<AApparanceEntity>(pactor);
	if (pentity)
	{
		pentity->PreConstructionScript();
	}
#endif

	//finish spawn, including running construction scripts
	FTransform t( local_placement );
	UGameplayStatics::FinishSpawningActor( pactor, t );
	
#if !WITH_EDITOR
	//can't intercept rerunconstruction scripts to handle parameter setup in-game, but in-game this is the only place it's needed
	if (pentity)
	{
		pentity->PostConstructionScript();
	}
#endif

	//attach
	bSuppressTransformUpdates = true;
	pactor->AttachToActor( this, FAttachmentTransformRules::KeepRelativeTransform );
	bSuppressTransformUpdates = false;
	USceneComponent* proot = pactor->GetRootComponent();
	proot->SetFlags( RF_Transient );	//HACK: ensure later detach doesn't dirty the parent
	if(proot)
	{
		proot->SetVisibility( bShown, true );
	}

	//HACK: Not sure why they aren't running properly as part of the deferred spawning above
	//force construction scripts
	//pactor->RerunConstructionScripts();

	//hide editor icons on placed blueprints
	TArray<UBillboardComponent*> billboards;
	pactor->GetComponents<UBillboardComponent>( billboards );
	for(int i = 0; i < billboards.Num(); i++)
	{
		if(billboards[i]->IsEditorOnly())
		{
			billboards[i]->SetVisibility( false, false );
		}
	}
}


void AApparanceEntity::RemoveBlueprint(class AActor* pactor)
{
	if (pactor)
	{
		ScopedOverlapDisable disable_overlaps( pactor, true );
	
#if ENABLE_PROC_BUILD_DIAGNOSTICS
		UE_LOG( LogApparance, Log, TEXT( "%s : REMOVE Blueprint, Actor: %s" ), *GetName(), *pactor->GetName() );
#endif

//		UE_LOG( LogApparance, Display, TEXT( "********** Destroying actor %p" ), pactor );

		//HACK: workaround because Destroy on it's own of a transient actor dirties the parent actor and the scene/package
		//SEE: 
		bSuppressTransformUpdates = true;
		pactor->DetachFromActor( FDetachmentTransformRules::KeepWorldTransform/*don't care*/ );
		bSuppressTransformUpdates = false;

		//remove from world and discard
		pactor->Destroy( false, false );

		ProceduralActors.Remove( pactor );

		//clearn stored parameters
		PlacementParameters->RemoveBlueprintActorParameters( pactor );
	}
}






/////////////////////////////////////////// DIAGNOSTICS TOOLS ////////////////////////////////////////////////

struct FObjectFlagsNames
{
	int32 Value;
	const TCHAR* Name;
}
ObjectFlagNames[]
{
	(int32)RF_Public					 				,TEXT("RF_Public"),
	(int32)RF_Standalone				 				,TEXT("RF_Standalone"),
	(int32)RF_MarkAsNative				 				,TEXT("RF_MarkAsNative"),
	(int32)RF_Transactional			 					,TEXT("RF_Transactional"),
	(int32)RF_ClassDefaultObject		 				,TEXT("RF_ClassDefaultObject"),
	(int32)RF_ArchetypeObject			 				,TEXT("RF_ArchetypeObject"),
	(int32)RF_Transient				 					,TEXT("RF_Transient"),
	(int32)RF_MarkAsRootSet			 					,TEXT("RF_MarkAsRootSet"),
	(int32)RF_TagGarbageTemp			 				,TEXT("RF_TagGarbageTemp"),
	(int32)RF_NeedInitialization		 				,TEXT("RF_NeedInitialization"),
	(int32)RF_NeedLoad					 				,TEXT("RF_NeedLoad"),
	(int32)RF_KeepForCooker			 					,TEXT("RF_KeepForCooker"),
	(int32)RF_NeedPostLoad				 				,TEXT("RF_NeedPostLoad"),
	(int32)RF_NeedPostLoadSubobjects	 				,TEXT("RF_NeedPostLoadSubobjects"),
	(int32)RF_NewerVersionExists		 				,TEXT("RF_NewerVersionExists"),
	(int32)RF_BeginDestroyed			 				,TEXT("RF_BeginDestroyed"),
	(int32)RF_FinishDestroyed			 				,TEXT("RF_FinishDestroyed"),
	(int32)RF_BeingRegenerated			 				,TEXT("RF_BeingRegenerated"),
	(int32)RF_DefaultSubObject			 				,TEXT("RF_DefaultSubObject"),
	(int32)RF_WasLoaded				 					,TEXT("RF_WasLoaded"),
	(int32)RF_TextExportTransient		 				,TEXT("RF_TextExportTransient"),
	(int32)RF_LoadCompleted			 					,TEXT("RF_LoadCompleted"),
	(int32)RF_InheritableComponentTemplate				,TEXT("RF_InheritableComponentTemplate"),
	(int32)RF_DuplicateTransient						,TEXT("RF_DuplicateTransient"),
	(int32)RF_StrongRefOnFrame							,TEXT("RF_StrongRefOnFrame"),
	(int32)RF_NonPIEDuplicateTransient					,TEXT("RF_NonPIEDuplicateTransient"),
	(int32)RF_WillBeLoaded								,TEXT("RF_WillBeLoaded"),
	(int32)EInternalObjectFlags::ReachableInCluster		,TEXT("ReachableInCluster"),
	(int32)EInternalObjectFlags::ClusterRoot			,TEXT("ClusterRoot"),
	(int32)EInternalObjectFlags::Native					,TEXT("Native"),
	(int32)EInternalObjectFlags::Async					,TEXT("Async"),
	(int32)EInternalObjectFlags::AsyncLoading			,TEXT("AsyncLoading"),
#if UE_VERSION_AT_LEAST(5,4,0)
	(int32)UE::GC::GUnreachableObjectFlag				,TEXT("Unreachable"),
#else
	(int32)EInternalObjectFlags::Unreachable			,TEXT("Unreachable"),
#endif
	(int32)EInternalObjectFlags::RootSet				,TEXT("RootSet"),
#if UE_VERSION_OLDER_THAN(5,0,0)
	(int32)RF_Dynamic									,TEXT( "RF_Dynamic" ),
	(int32)EInternalObjectFlags::PendingKill			,TEXT( "PendingKill" ),
#endif
};
	

// helper to visualise object flags
//
FString DescribeObjectFlags( EObjectFlags flags )
{
	FString s;
	int num = sizeof(ObjectFlagNames) / sizeof(FObjectFlagsNames);
	for (int i = 0; i < num; i++)
	{
		int value = ObjectFlagNames[i].Value;
		FString name = ObjectFlagNames[i].Name;
		
		if((flags&value)!=0)
		{
			if (s.Len() > 0)
			{
				s.AppendChar(TCHAR('|'));
			}
			s.Append(name);
		}
	}
	
	return s;
}
FString DescribeObjectFlags(const UObject* object)
{
	if (object)
	{
		FString s = DescribeObjectFlags(object->GetFlags());

		//additional actor flags
		const AActor* pactor = Cast<AActor>( object );
		if(pactor)
		{
#if WITH_EDITOR
			if(!pactor->IsListedInSceneOutliner())
			{
				s.Append( TEXT( "|hidden" ) );
			}
			if(pactor->IsSelectable())
			{
				s.Append( TEXT( "|selectable" ) );
			}
#endif
			if(pactor->IsRootComponentMovable())
			{
				s.Append( TEXT( "|movable" ) );
			}
			if(pactor->IsRootComponentStatic())
			{
				s.Append( TEXT( "|static" ) );
			}
			if(pactor->IsRootComponentStationary())
			{
				s.Append( TEXT( "|stationary" ) );
			}
			if(pactor->IsChildActor())
			{
				s.Append( TEXT( "|child" ) );
			}
			if(pactor->GetParentActor()!=nullptr)
			{
				s.Append( TEXT( "|parent" ) );
			}
		}

		return s;
	}
	else
	{
		return FString();
	}
}
	
#if WITH_EDITOR
// copy element/component/actor/group/owner/attachment hierarchy of this actor to clipboard
//
FString AApparanceEntity::Debug_GetStructure() const
{
	TArray<FString> lines;

	//header	
	lines.Add( FString::Printf( TEXT("Actor: %s [%p] %s"), *GetDebugName(this), (void*)this, *DescribeObjectFlags(this) ) );
	
	//components
	lines.Add( FString::Printf( TEXT("-------- UNREAL --------") ) );
	TInlineComponentArray<UActorComponent*> components( this );
	lines.Add( FString::Printf( TEXT("Owned Components (%i):"), components.Num() ) );
	for(int i=0 ; i<components.Num() ; i++)
	{
		UActorComponent* pc = components[i];
		if(pc)
		{
			lines.Add( FString::Printf( TEXT("\t%i : %s [%p] %s"), i, *pc->GetReadableName(), (void*)pc, *DescribeObjectFlags(pc) ) );

			//sub-component details
			UProceduralMeshComponent* pmc = Cast<UProceduralMeshComponent>( pc );
			if(pmc)
			{
				int mats = pmc->GetNumMaterials();
				for(int m = 0; m < mats; m++)
				{
					FProcMeshSection* fpmc = pmc->GetProcMeshSection( m );
					int verts = fpmc->ProcVertexBuffer.Num();
					int indices = fpmc->ProcIndexBuffer.Num();
					int tris = indices / 3;
					lines.Add( FString::Printf( TEXT( "\t\t[%i] Mesh Section (%i vertices, %i triangles)" ), m, verts, tris ) );
				}
			}
		}
		else
		{
			lines.Add( FString::Printf( TEXT("\t[%i] : null"), i ) );
		}
	}

	//actors
	TArray<AActor*> actors;
	GetAttachedActors( actors );
	lines.Add( FString::Printf( TEXT("Attached Actors (%i):"), actors.Num() ) );
	for(int i=0 ; i<actors.Num() ; i++)
	{
		AActor* pa = actors[i];
		if(pa)
		{
			lines.Add( FString::Printf( TEXT("\t%i : %s [%p] %s"), i, *pa->GetHumanReadableName(), (void*)pa, *DescribeObjectFlags(pa) ) );
		}
		else
		{
			lines.Add( FString::Printf( TEXT("\t[%i] : null"), i ) );
		}
	}

	lines.Add( FString::Printf( TEXT("-------- APPARANCE --------") ) );	
	//caches
	//TMap<int, class UProceduralMeshComponent*> GeometryCache;
	lines.Add( FString::Printf( TEXT("Geometry Cache (%i):"), GeometryCache.Num() ) );
	for (auto It = GeometryCache.CreateConstIterator(); It; ++It)
	{
		int id = It.Key();
		UProceduralMeshComponent* p = It.Value().Get();
		lines.Add( FString::Printf( TEXT("\t#%i : %s [%p] %s"), id, p?(*p->GetReadableName()):TEXT("null"), (void*)p, *DescribeObjectFlags(p) ) );
	}
	//TMap<int, class UProceduralMeshComponent*> CollisionCache;
	lines.Add( FString::Printf( TEXT( "Collision Cache (%i):" ), CollisionCache.Num() ) );
	for(auto It = CollisionCache.CreateConstIterator(); It; ++It)
	{
		int id = It.Key();
		UProceduralMeshComponent* p = It.Value().Get();
		lines.Add( FString::Printf( TEXT( "\t#%i : %s [%p] %s" ), id, p ? (*p->GetReadableName()) : TEXT( "null" ), (void*)p, *DescribeObjectFlags( p ) ) );
	}
	//TMap<int, class UMaterialInstanceDynamic*> MaterialCache;
	lines.Add( FString::Printf( TEXT("Material Cache (%i):"), MaterialCache.Num() ) );
	for (auto It = MaterialCache.CreateConstIterator(); It; ++It)
	{
		int id = It.Key();
		UMaterialInstanceDynamic* p = It.Value().Get();
		UMaterial* pmat = p?p->GetMaterial():nullptr;
		lines.Add( FString::Printf( TEXT("\t#%i : %s (%s) [%p] %s"), id, p?(*p->GetFullName()):TEXT("null"), pmat?(*pmat->GetFullName()):TEXT("null"), (void*)p, *DescribeObjectFlags(p) ) );
	}
	//TMap<int, FMeshCacheEntry> MeshCache;
	lines.Add( FString::Printf( TEXT("Mesh Cache (%i):"), MeshCache.Num() ) );
	for (auto It = MeshCache.CreateConstIterator(); It; ++It)
	{
		int id = It.Key();
		const FMeshCacheEntry& e = It.Value();
		int count = e.Meshes.Num();
		lines.Add( FString::Printf( TEXT("\t#%i : Mesh Groups (%i):"), id, count) );
		for(int i=0 ; i<count ; i++)
		{
			UStaticMeshComponent* pm = e.Meshes.operator[]( i ).Get();
			lines.Add( FString::Printf( TEXT("\t\t%i : %s [%p] %s"), i, pm ?(*pm->GetReadableName()):TEXT("null"), (void*)pm, *DescribeObjectFlags(pm) ) );
		}
	}
	//TMap<int, FActorComponentCacheEntry> ActorComponentCache;
	lines.Add( FString::Printf( TEXT("Actor Component Cache (%i):"), ActorComponentCache.Num() ) );
	for (auto It = ActorComponentCache.CreateConstIterator(); It; ++It)
	{
		int id = It.Key();
		const FActorComponentCacheEntry& e = It.Value();
		int count = e.Components.Num();
		lines.Add( FString::Printf( TEXT("\t#%i : Component Groups (%i):"), id, count) );
		for(int i=0 ; i<count ; i++)
		{
			UActorComponent* pac = e.Components.operator[]( i ).Get();
			lines.Add( FString::Printf( TEXT("\t\t%i : %s [%p] %s"), i, pac ?(*pac->GetReadableName()):TEXT("null"), (void*)pac, *DescribeObjectFlags(pac) ) );
		}
	}
	//TMap<int, FActorCacheEntry> ActorCache;
	lines.Add( FString::Printf( TEXT("Actor Cache (%i):"), ActorCache.Num() ) );
	for (auto It = ActorCache.CreateConstIterator(); It; ++It)
	{
		int id = It.Key();
		const FActorCacheEntry& e = It.Value();
		int count = e.Actors.Num();
		lines.Add( FString::Printf( TEXT("\t#%i : Actor Groups (%i):"), id, count ) );
		for(int i=0 ; i< count; i++)
		{
			AActor* pa = e.Actors[i].Get();
			if(pa)
			{
				lines.Add( FString::Printf( TEXT("\t\t%i : %s [%p] %s"), i, *pa->GetHumanReadableName(), (void*)pa, *DescribeObjectFlags(pa) ) );
			}
			else
			{
				lines.Add( FString::Printf( TEXT("\t\t[%i] : nullptr"), i ) );
			}
		}
	}
	//TMap<int, FMeshInstanceCacheEntry> InstanceCache;
	lines.Add( FString::Printf( TEXT("Instance Cache (%i):"), InstanceCache.Num() ) );
	for (auto It = InstanceCache.CreateConstIterator(); It; ++It)
	{
		int id = It.Key();
		const FMeshInstanceCacheEntry e = It.Value();
		int count = e.Instances ? e.Instances->Num() : 0;
		lines.Add( FString::Printf( TEXT("\t#%i : Instance Groups (%i):"), id, count ) );
		for(int i=0 ; i<count ; i++)
		{
			FMeshInstanceHandle mih = e.Instances->operator[]( i );
			lines.Add( FString::Printf( TEXT("\t\t%i : FMeshInstanceCacheEntry, MeshType=%i, InstanceIndex=%i"), i, mih.MeshType, mih.InstanceIndex ) );	
		}
	}
	//TArray<FInstancedMeshCacheEntry> InstanceMeshCache;
	lines.Add( FString::Printf( TEXT("Instance Mesh Cache (%i):"), InstanceMeshCache.Num() ) );
	for (int i=0 ; i<InstanceMeshCache.Num() ; i++)
	{
		UStaticMesh* pm = InstanceMeshCache[i].Mesh.Get();
		UInstancedStaticMeshComponent* pim = InstanceMeshCache[i].InstancedMeshes.Get();
		int pending = InstanceMeshCache[i].PendingInstances.IsValid()?InstanceMeshCache[i].PendingInstances->Num():0;
		lines.Add( FString::Printf( TEXT("\t%i : %s (%s) (%i pending) [%p] %s"), i, pim?(*pim->GetReadableName()):TEXT("null"), pm?(*pm->GetFullName()):TEXT("null"), pending, (void*)pim, *DescribeObjectFlags(pim) ) );
	}
	//TMap<class UStaticMesh*, int> InstancedMeshCacheLookup;
	lines.Add( FString::Printf( TEXT("Mesh Cache Lookup (%i):"), InstancedMeshCacheLookup.Num() ) );
	for (auto It = InstancedMeshCacheLookup.CreateConstIterator(); It; ++It)
	{
		UStaticMesh* pm = It.Key();
		int cache_index = It.Value();
		lines.Add( FString::Printf( TEXT("\t%i : %s [%p] %s"), cache_index, pm?(*pm->GetFullName()):TEXT(""), pm, *DescribeObjectFlags(pm) ) );
	}

	//entity rendering
	if(m_pEntityRendering.IsValid())
	{
		int tier_count = m_pEntityRendering->m_Tiers.Num();
		lines.Add( FString::Printf( TEXT("Entity Rendering (%i tiers):"), tier_count ) );
		for(int t=0 ; t<tier_count+1/*extra one for editing tier*/ ; t++)
		{
			FDetailTier* ptier;
			int tier_id = t;
			if(t < tier_count)
			{
				//normal tier
				ptier = m_pEntityRendering->GetTier( t );
			}
			else
		{
				//special editing tier
				ptier = m_pEntityRendering->GetTier( -1 );
				tier_id = -1;
			}
			if(ptier)
			{
				lines.Add( FString::Printf( TEXT("\tTier %i: [%p]"), tier_id, ptier ) );
				
				//materials
				lines.Add( FString::Printf( TEXT("\t\tMaterials (%i):"), ptier->Materials.Num() ) );
				for(int m=0 ; m<ptier->Materials.Num() ; m++)
				{
					FParameterisedMaterial* ppm = ptier->Materials[m].Get();
					Apparance::MaterialID id = ppm->ID;
					UMaterialInstanceDynamic* p = ppm->MaterialInstance.Get();
					UMaterial* pmat = p?p->GetMaterial():nullptr;
					lines.Add( FString::Printf( TEXT("\t\t\t#%i: %s (%s) [%p]"), id, p?(*p->GetFullName()):TEXT("null"), pmat?(*pmat->GetFullName()):TEXT("null"), (void*)p ) );
				}
				
				//components
				lines.Add( FString::Printf( TEXT("\t\tComponents (%i):"), ptier->Components.Num() ) );
				for (auto It = ptier->Components.CreateConstIterator(); It; ++It)
				{
					int id = It.Key();
					UProceduralMeshComponent* p = It.Value().Get();
					lines.Add( FString::Printf( TEXT("\t\t\t#%i: %s [%p] %s"), id, p?(*p->GetReadableName()):TEXT("null"), (void*)p, *DescribeObjectFlags(p) ) );
				}
			}
			else
			{
				lines.Add( FString::Printf( TEXT("\t%i : null"), t ) );
			}
		}
	}
	
	//general proc-component tracking
	lines.Add( FString::Printf( TEXT("All Procedural Components (%i):"), ProceduralComponents.Num()+ProceduralActors.Num() ) );
	for(int i=0 ; i<ProceduralComponents.Num() ; i++)
	{
		UActorComponent* p = ProceduralComponents[i];
		lines.Add( FString::Printf( TEXT("\t%i : %s [%p] %s"), i, p?(*p->GetReadableName()):TEXT("null"), (void*)p, *DescribeObjectFlags(p) ) );
	}
	for(int i = 0; i < ProceduralActors.Num(); i++)
	{
		AActor* p = ProceduralActors[i];
		lines.Add( FString::Printf( TEXT( "\t%i : %s [%p] %s" ), ProceduralComponents.Num()+i, p ? (*p->GetName()) : TEXT( "null" ), (void*)p, *DescribeObjectFlags( p ) ) );
	}

#if false
	//to temp file
	FFileHelper::SaveStringArrayToFile( lines, TEXT("C:\\Temp\\ApparanceUnreal\\apparance_entity.txt") );
#endif

	//to clipboard
	FString s;
	for(int i = 0; i < lines.Num(); i++)
	{
		s.Append( lines[i] ).Append( TEXT( "\n" ) );
	}
	return s;
}
#endif //WITH_EDITOR

#undef LOCTEXT_NAMESPACE

#if APPARANCE_DEBUGGING_HELP_ApparanceEntity
PRAGMA_ENABLE_OPTIMIZATION_ACTUAL
#endif
