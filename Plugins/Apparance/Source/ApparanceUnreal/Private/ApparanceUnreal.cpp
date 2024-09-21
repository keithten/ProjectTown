//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

//local debugging help
#define APPARANCE_DEBUGGING_HELP_ApparanceUnreal 0
#if APPARANCE_DEBUGGING_HELP_ApparanceUnreal
PRAGMA_DISABLE_OPTIMIZATION_ACTUAL
#endif

#include "ApparanceUnreal.h"


//unreal
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "EngineUtils.h"

//module
#include "LoggingService.h"
#include "GeometryFactory.h"
#include "AssetDatabase.h"
#include "ApparanceEngineSetup.h"
#include "ApparanceEntity.h"
#include "ApparanceUnrealEditorAPI.h"
#include "Support/SmartEditingState.h"


// PROLOGUE
#define LOCTEXT_NAMESPACE "FApparanceUnrealModule"
DEFINE_LOG_CATEGORY(LogApparance);

// CONSTANTS & MACROS

// HELPERS 
#if UE_VERSION_OLDER_THAN(5,0,0)
bool IsValid( UObject* p ) { return p && !p->IsPendingKill(); }
bool IsValidChecked( UObject* p ) { check( p );  return !p->IsPendingKill(); }
#endif

// LOCAL CLASSES & TYPES


// GLOBAL STATE
FLoggingService g_ApparanceLogger;
FGeometryFactory g_ApparanceGeometryFactory;
FAssetDatabase g_ApparanceAssetDatabase;
FText g_ProductName;

// CLASS STATE
FApparanceUnrealModule* FApparanceUnrealModule::m_pModule = nullptr;

// DEBUGGING & TESTS
extern void Apparance_TickGenLog();
extern void Apparance_TickTimeslicedGeometryHandling();


// Init
//
// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
//
void FApparanceUnrealModule::StartupModule()
{
	m_pAssetDatabase = nullptr;
	m_pEditorModule = nullptr;
	m_pModule = this;
	m_bApparanceEngineDeferredStart = false;
	m_bInventoryValid = false;

	//need to start tick otherwise deferred startup can't trigger	
	if (!InitTimers())
	{
		UE_LOG(LogApparance, Error, TEXT("Failed to set up timers needed for the Apparance Engine"));
		return;
	}
	
	if(!InitEngine())
	{
		UE_LOG(LogApparance, Error, TEXT("Failed to start Apparance Engine check configuration and project setup"));
		return;
	}

	InitPlacement();
	InitProductInfo();
}

// set up regular ticks
//
bool FApparanceUnrealModule::InitTimers()
{
	// we want to be updated regularly
	TickDelegate = FTickerDelegate::CreateRaw(this, &FApparanceUnrealModule::Tick);
	TickDelegateHandle = TICKER_TYPE::GetCoreTicker().AddTicker(TickDelegate);

	return true;
}

// set up and launch apparance engine
//
bool FApparanceUnrealModule::InitEngine()
{
	m_pApparance = Apparance::Engine::Start();
	m_pAssetDatabase = &g_ApparanceAssetDatabase;
	
	//procedure location
	FString proc_subdir = UApparanceEngineSetup::GetProceduresDirectory();
	// procs are always located under game content folder
	FString content_dir_ws = FPaths::ProjectContentDir();
	content_dir_ws = FPaths::ConvertRelativePathToFull( content_dir_ws );	//content dir seems to come back relative when your project is in My Documents!
	ProceduresPath = FPaths::Combine(content_dir_ws, proc_subdir);
	FPaths::NormalizeDirectoryName(ProceduresPath);
	FPaths::CollapseRelativeDirectories(ProceduresPath);

	// build apparance configuration
	Apparance::Engine::SetupParameters sparams;
	memset(&sparams, 0, sizeof(sparams));

	FTCHARToUTF8 proc_dir(*ProceduresPath);
	sparams.ProcedureStoragePath = proc_dir.Get();

	sparams.LoggingService = &g_ApparanceLogger;
	sparams.GeometryFactory = &g_ApparanceGeometryFactory;
	sparams.AssetDatabase = &g_ApparanceAssetDatabase;

	// engine init
	g_ApparanceLogger.LogMessage("Setting up Apparance");
	if (!m_pApparance->Setup( sparams ))
	{
		g_ApparanceLogger.LogMessage( "Setup failed" );
	}

	return m_pApparance!=nullptr;
}


void FApparanceUnrealModule::StartupModuleDeferred()
{
	//want to start using Apparance
	m_bApparanceEngineDeferredStart = true;

	if(GIsEditor)
	{
		//boot remote editor access
		ensure( GetEngine()->GetEditor() );
	}
	if(!InitAssetDatabase())
	{
		//info provided in call when fails
		return;
	}
	if(!InitSynthesis())
	{
		UE_LOG( LogApparance, Error, TEXT( "Failed to initialise Apparance synthesis system, check ini files and default assets are available" ) );
		return;
	}
}

// asset database setup
// NOTE: has to be deferred until engine up and running (loading of the settings asset triggers loading of blueprints which need access to the procedure library catalogue)
//
bool FApparanceUnrealModule::InitAssetDatabase()
{
	//resource root
	ResourceRoot = UApparanceEngineSetup::GetResourceRoot();
	if (ResourceRoot)
	{
		UE_LOG(LogApparance, Log, TEXT("Using engine resource root: %s"), *ResourceRoot->GetFullName());
	}
	else
	{
		UE_LOG(LogApparance, Error, TEXT("No root Resource List specified, plugin configuration set up appropriately.") );
		return false;
	}
	
	if(ResourceRoot)
	{
		ResourceRoot->AddToRoot();//need to pin as we aren't a UObject
	}

	// init asset support
	g_ApparanceAssetDatabase.SetRootResourceList( ResourceRoot );

	return true;
}

// set up synthesis engine
//
bool FApparanceUnrealModule::InitSynthesis()
{
	//engine setup context
	UE_LOG(LogApparance, Log, TEXT("Apparance running in '%s' configuration"), *UApparanceEngineSetup::GetSetupContext().ToString());

	//prep engine config
	Apparance::Engine::ConfigParameters cparams;
	memset(&cparams, 0, sizeof(cparams));

	cparams.SynthesiserCount = UApparanceEngineSetup::GetThreadCount();
	cparams.BufferSizeMB = UApparanceEngineSetup::GetBufferSize();
	cparams.LiveEditing = UApparanceEngineSetup::GetEnableLiveEditing();
	
	//start synthesis
	g_ApparanceLogger.LogMessage("Configuring Apparance Synthesis Engine");
	if (!m_pApparance->Configure( cparams ))
	{
		g_ApparanceLogger.LogMessage( "Configuration failed" );
		return false;
	}	

	return true;
}


// allow placement of actor (by drag and drop) from Place Actor panel
// NOTE: By default this should already happen, but I think if you start customising with factories/etc too much it stops working
//
void FApparanceUnrealModule::InitPlacement()
{
#if 0
	UBlueprint* CompositePlane = Cast<UBlueprint>( FSoftObjectPath( TEXT( "/CompositePlane/BP_CineCameraProj.BP_CineCameraProj" ) ).TryLoad() );
	if(CompositePlane == nullptr)
	{
		return;
	}

	IPlacementModeModule& PlacementModeModule = IPlacementModeModule::Get();
	const FPlacementCategoryInfo* Info = PlacementModeModule.GetRegisteredPlacementCategory( FName( "Cinematic" ) );
	if(Info == nullptr)
	{
		return;
	}

	FPlaceableItem* BPPlacement = new FPlaceableItem(
		*UActorFactoryBlueprint::StaticClass(),
		FAssetData( CompositePlane, true ),
		FName( "" ),
		TOptional<FLinearColor>(),
		TOptional<int32>(),
		NSLOCTEXT( "PlacementMode", "Composite Plane", "Composite Plane" )
	);

	IPlacementModeModule::Get().RegisterPlaceableItem( Info->UniqueHandle, MakeShareable( BPPlacement ) );

#endif

#if 0
	check( IPlacementModeModule::IsAvailable() );
	IPlacementModeModule& PlacementModeModule = IPlacementModeModule::Get();
	const FPlacementCategoryInfo* Info = PlacementModeModule.GetRegisteredPlacementCategory( FBuiltInPlacementCategories::Basic() );
	if(Info == nullptr)
	{
		return;
	}

	FPlaceableItem* Placement = new FPlaceableItem( *AApparanceEntity::StaticClass() );
	IPlacementModeModule::Get().RegisterPlaceableItem( Info->UniqueHandle, MakeShareable( Placement ) );
#endif

#if 0
	check( IPlacementModeModule::IsAvailable() );
	IPlacementModeModule& PlacementModeModule = IPlacementModeModule::Get();
	const FPlacementCategoryInfo* Info = PlacementModeModule.GetRegisteredPlacementCategory( FBuiltInPlacementCategories::Basic() );
	if(Info == nullptr)
	{
		return;
	}

	FPlaceableItem* Placement = new FPlaceableItem(
		*UActorFactoryApparanceEntity::StaticClass(),
		FAssetData( AApparanceEntity::StaticClass() ),
		FName( "" ),
		TOptional<FLinearColor>(),
		TOptional<int32>(),
		LOCTEXT( "ApparanceEntityActorName", "Apparance Entity" )
	);

	PlacementModeModule.RegisterPlaceableItem( Info->UniqueHandle, MakeShareable( Placement ) );
	// new FPlaceableItem( nullptr, FAssetData( AApparanceEntity::StaticClass() ) ) ) );
#endif
}

// Exit
//
// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
// we call this function before unloading the module.
//
void FApparanceUnrealModule::ShutdownModule()
{
	//release objects
	TArray<class UWorld*> worlds;
	m_EditingStates.GetKeys( worlds );
	for (int i = 0; i < worlds.Num(); i++ )
	{
		DestroySmartEditingState( m_EditingStates[worlds[i]] );
	}

	//release pinned assets
	if(ResourceRoot)
	{
		ResourceRoot->RemoveFromRoot();
		ResourceRoot = nullptr;
	}	
	g_ApparanceAssetDatabase.Shutdown();

	//stop engine
	g_ApparanceLogger.LogMessage("Stopping Apparance Engine");	
	TICKER_TYPE::GetCoreTicker().RemoveTicker(TickDelegateHandle);

	//done with Apparance
	Apparance::Engine::Stop();
}




// Update/Tick
//
bool FApparanceUnrealModule::Tick( float DeltaTime )
{
	//setup phase
	if (!m_bApparanceEngineDeferredStart)
	{
		StartupModuleDeferred();
	}

	//smart editing
	for (TMap<UWorld*, USmartEditingState*>::TIterator It(m_EditingStates); It; ++It)	
	{
		USmartEditingState* state = It.Value();
		state->Tick();
	}

	//ongoing operation
	if (m_pApparance)
	{
		m_pApparance->Update(DeltaTime);
	}

	//adb updates
	GetAssetDatabase()->Tick();

	//debug/tests
	Apparance_TickTimeslicedGeometryHandling();
	Apparance_TickGenLog();

	return true;
}

/// <summary>
/// external change to procedural assets has occurred, e.g. apparance editor triggered change
/// NOTE: can be called during runtime, but will be ignored, hence no Editor_ prefix
/// </summary>
void FApparanceUnrealModule::NotifyExternalContentChanged()
{
	//editing systems needs to know
	if(m_pEditorModule)
	{
		m_pEditorModule->NotifyExternalContentChanged();
	}	
}

// CDO/BP default proc param changes need to be propagated to all instances of them
//
void FApparanceUnrealModule::Editor_NotifyProcedureDefaultsChange(Apparance::ProcedureID proc_id, const Apparance::IParameterCollection* params, Apparance::ValueID changed_param)
{
	//assume inventory out of date
	m_bInventoryValid = false;

	//editing systems needs to know
	if(m_pEditorModule)
	{
		m_pEditorModule->NotifyProcedureDefaultsChange(proc_id, params, changed_param);
	}
}

// proc type changes
//
void FApparanceUnrealModule::Editor_NotifyEntityProcedureTypeChanged( Apparance::ProcedureID new_proc_id )
{
	//editing systems needs to know
	if(m_pEditorModule)
	{
		m_pEditorModule->NotifyEntityProcedureTypeChanged( new_proc_id );
	}
	
}

// asset database changed
//
void FApparanceUnrealModule::Editor_NotifyAssetDatabaseChanged()
{
	//editing systems needs to know
	if(m_pEditorModule)
	{
		m_pEditorModule->NotifyAssetDatabaseChanged();
	}
}

// resource list structure changed
//
void FApparanceUnrealModule::Editor_NotifyResourceListStructureChanged( UApparanceResourceList* pres_list, UApparanceResourceListEntry* pentry, FName property )
{
	//editing systems needs to know
	if(m_pEditorModule)
	{
		m_pEditorModule->NotifyResourceListStructureChanged( pres_list, pentry, property );
	}
}

// resource list entry asset changed
//
void FApparanceUnrealModule::Editor_NotifyResourceListAssetChanged( UApparanceResourceList* pres_list, UApparanceResourceListEntry* pentry, UObject* pnewasset )
{
	//editing systems needs to know
	if(m_pEditorModule)
	{
		m_pEditorModule->NotifyResourceListAssetChanged( pres_list, pentry, pnewasset );
	}
}

// Play in Editor just stopped
//
void FApparanceUnrealModule::Editor_NotifyPlayStopped()
{
}

// forward to state associated with world
//
bool FApparanceUnrealModule::Editor_UpdateInteraction(UWorld* world, FVector cursor_ray_direction, bool interaction_button/*, int modifiers*/)
{
	if (USmartEditingState* state = IsEditingEnabled( world ) )
	{
		return state->UpdateInteractiveEditing( cursor_ray_direction, interaction_button );
	}
	return false;
}

// some runtime module code includes editing functions that need a world to do temp things with
//
UWorld* FApparanceUnrealModule::Editor_GetTempWorld()
{
	if(m_pEditorModule)
	{
		return m_pEditorModule->GetTempWorld();
	}
	return nullptr;
}

// viewport access when in editor only
//
FViewport* FApparanceUnrealModule::Editor_FindViewport() const
{
	if (m_pEditorModule)
	{
		return m_pEditorModule->FindEditorViewport();
	}
	return nullptr;
}

// smart interactive editing of entities
//
Apparance::IParameterCollection* FApparanceUnrealModule::BeginEntityParameterEdit( class IProceduralObject* pentity, bool editing_transaction )
{
	if(m_pEditorModule)
	{
		//editor build, indirect, editor module will handle it
		return m_pEditorModule->BeginParameterEdit( pentity, editing_transaction );
	}
	else
	{
		//stand-alone build, direct to entity
		AApparanceEntity* p = Cast< AApparanceEntity>( pentity );
		if(p)
		{
			return p->BeginEditingParameters();
		}
	}
	return nullptr;
}

// smart interactive editing of entities
//
void FApparanceUnrealModule::EndEntityParameterEdit( class IProceduralObject* pentity, bool editing_transaction, Apparance::ValueID input_id )
{
	if(m_pEditorModule)
	{
		//editor build, indirect, editor module will handle it
		m_pEditorModule->EndParameterEdit( pentity, editing_transaction, input_id );
	}
	else
	{
		//stand-alone build, direct to entity
		AApparanceEntity* p = Cast< AApparanceEntity>( pentity );
		if(p)
		{
			p->EndEditingParameters( false );
		}
	}
}




// can we use live editing features?
//
bool FApparanceUnrealModule::IsLiveEditingEnabled() const
{ 
	return UApparanceEngineSetup::GetEnableLiveEditing();
}

// are we within a run of the game (or sim?)
//
bool FApparanceUnrealModule::IsGameRunning() const
{
	if (m_pEditorModule)
	{
		return m_pEditorModule->IsGameRunning();
	}
	else
	{
		//no editor, must be game, i.e. always running
		return true;
	}
}

// obtain up-to-date procedure inventory
//
const TArray<const Apparance::ProcedureSpec*>& FApparanceUnrealModule::GetInventory()
{
	//rebuild list on demand
	if(!m_bInventoryValid)
	{
		//scan lib for latest spec
		m_ProcedureInventory.Reset();
		int count = GetLibrary()->GetProcedureCount();
		for(int i = 0; i < count; i++)
		{
			const Apparance::ProcedureSpec* pspec = GetLibrary()->GetProcedureSpec( i );
			//check( pspec );
			if(pspec) //some weird setup issue cause new procedures to be present but not 
			{
				m_ProcedureInventory.Add( pspec );
			}
		}

		//sort alphabetically
		m_ProcedureInventory.Sort( []( const Apparance::ProcedureSpec& A, const Apparance::ProcedureSpec& B )->bool
			{
				int cat = FCStringAnsi::Strcmp( A.Category, B.Category );
				if(cat == 0)
				{
					return FCStringAnsi::Strcmp( A.Name, B.Name ) < 0;
				}
				return cat < 0;
			} );

		m_bInventoryValid = true;
	}

	return m_ProcedureInventory;
}

// what product variant is this, e.g. Demo/Beta
//
const FText FApparanceUnrealModule::GetProductText() const
{
	return g_ProductName;
}

// gather init product information
void FApparanceUnrealModule::InitProductInfo()
{
	//gather
	FString phase;
	const Apparance::IParameterCollection* version = FApparanceUnrealModule::GetEngine()->GetVersionInformation();
	if(version)
	{
		int count = version->BeginAccess();
		for(int i = 0; i < count; i++)
		{
			if(version->GetType( i ) == Apparance::Parameter::String)	//all should be strings
			{
				//name
				const char* pname = version->GetName( i );
				FString name( UTF8_TO_TCHAR( pname ) );
				//value
				FString value;
				int num_chars = 0;
				TCHAR* pbuffer = nullptr;
				if(version->GetString( i, 0, nullptr, &num_chars ))
				{
					value = FString::ChrN( num_chars, TCHAR( ' ' ) );
					pbuffer = value.GetCharArray().GetData();
					version->GetString( i, num_chars, pbuffer );
				}

				//use?
				if(name == TEXT( "Phase" ))
				{
					phase = value;
				}
//				else if(name== TEXT(""))
			}
		}
		version->EndAccess();
		delete version;
	}

	//interpret
	g_ProductName = FText::FromString( phase.ToUpper() );
}

// fallback material provided by current engine settings
//
class UMaterial* FApparanceUnrealModule::GetFallbackMaterial()
{
	return UApparanceEngineSetup::GetMissingMaterial();
}

// fallback texture provided by current engine settings
//
class UTexture* FApparanceUnrealModule::GetFallbackTexture()
{
	return UApparanceEngineSetup::GetMissingTexture();
}

// list of missing asset descriptors requested by proc-gen
//
const TArray<FString> FApparanceUnrealModule::GetMissingAssets()
{
	return g_ApparanceAssetDatabase.MissingAssets;
}

// fallback mesh provided by current engine settings
//
class UStaticMesh* FApparanceUnrealModule::GetFallbackMesh()
{
	return UApparanceEngineSetup::GetMissingObject();
}


// does this world have smart editing enabled?
//
class USmartEditingState* FApparanceUnrealModule::IsEditingEnabled( UWorld* world ) const
{
	if(USmartEditingState* const* p = m_EditingStates.Find( world ))
	{
		USmartEditingState* state = *p;
		if(state && state->IsActive())
		{
			return state;
		}
	}
	return nullptr;
}

// set up, start, or pause smart editing for this world
//
class USmartEditingState* FApparanceUnrealModule::EnableEditing( UWorld* world, bool enable, bool force )
{
	//existing state?
	USmartEditingState* state = nullptr;
	if (USmartEditingState** p = m_EditingStates.Find( world ) )
	{
		state = *p;
		if (state->IsActive()==enable && !force )
		{
			//no change
			return state;
		}
	}
	
	//first time, set up state
	if(enable && !state)
	{
		state = CreateSmartEditingState( world );	
	}

	//switch active/inactive
	if(enable)
	{
		state->Start();
	}
	else
	{
		state->Stop();
	}

	return state;
}

// respond to a world being removed, e.g. end of PIE session
//
void FApparanceUnrealModule::HandleRemoveWorld( class UWorld* world )
{
	//associated state?
	USmartEditingState* state = nullptr;
	if (USmartEditingState** p = m_EditingStates.Find( world ) )
	{
		state = *p;
		DestroySmartEditingState( state );
	}
}


// new smart editing state tracker needed for a world
//
USmartEditingState* FApparanceUnrealModule::CreateSmartEditingState( UWorld* world )
{
	//new state object
	USmartEditingState* state = NewObject<USmartEditingState>();
	state->AddToRoot();
	state->Init( world );

	//associate with world	
	m_EditingStates.Add( world, state );

	//first time, need to watch world destruction	
	if (m_EditingStates.Num() == 1 )
	{
		FWorldDelegates::OnPreWorldFinishDestroy.AddRaw( this, &FApparanceUnrealModule::HandleRemoveWorld );
	}
	
	return state;
}

// we're done with a smart editing state tracker
//
void FApparanceUnrealModule::DestroySmartEditingState( USmartEditingState* state )
{
	//NOTE: don't stop state, we only destroy these when world is removed anyway
	
	//remove from tracking
	if(UWorld * const * p = m_EditingStates.FindKey(state))
	{
		UWorld* world = *p;
		m_EditingStates.Remove( world );
	}

	//release
	if (IsValid(state))
	{
		state->RemoveFromRoot();
	}

	//last time, nolonger need to watch world destruction	
	if (m_EditingStates.Num() == 0 )
	{
		FWorldDelegates::OnPreWorldFinishDestroy.RemoveAll( this );
	}
}

// build an identifying name for a given actors world
//
FString FApparanceUnrealModule::MakeWorldIdentifier( AActor* pactor )
{
	FString world_name;	
	if (auto* world = pactor->GetWorld())
	{
		//has world, partition entities by world
		world_name = MakeWorldIdentifier(world);
	}
	else if(pactor->HasAnyFlags( RF_ClassDefaultObject ))
	{
		//CDO objects seem to be getting entities
		//TODO: stop this, they don't need them as they have no visual presence
		world_name = "<Default Objects>";
	}
	else
	{
		//not sure where this entity is
		world_name = "Unknown";
	}
	return world_name;
}

// build an identifying name for a given actors world
//
FString FApparanceUnrealModule::MakeWorldIdentifier(UWorld* pworld)
{
	FString world_name;
	world_name = LexToString( pworld->WorldType );
	world_name.Append(TEXT(": "));
	world_name.Append(pworld->GetName());
	return world_name;
}

#if 0
//////////////////////////////////////////////////////////////////////////
// Helpers

// find the world we are working with
//
UWorld* FApparanceUnrealModule::FindWorld()
{
	UWorld* world = nullptr;
	if (!world)
	{
		// try to pick the most suitable world context

		// ideally we want a PIE world that is standalone or the first client
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (World && Context.WorldType == EWorldType::PIE)
			{
				if (World->GetNetMode() == NM_Standalone)
				{
					world = World;
					break;
				}
				else if (World->GetNetMode() == NM_Client && Context.PIEInstance == 2)	// Slightly dangerous: assumes server is always PIEInstance = 1;
				{
					world = World;
					break;
				}
			}
		}
	}

	if (!world)
	{
		// still not world so fallback to old logic where we just prefer PIE over Editor
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE)
			{
				world = Context.World();
				break;
			}
			else if (Context.WorldType == EWorldType::Editor)
			{
				world = Context.World();
			}
		}
	}

	if (!world)
	{
		//fall back to default
		world = GetWorld();
	}

	return world;
}
#endif

// EPILOGUE
#undef LOCTEXT_NAMESPACE
IMPLEMENT_MODULE(FApparanceUnrealModule, ApparanceUnreal)


#if APPARANCE_DEBUGGING_HELP_ApparanceUnreal
PRAGMA_ENABLE_OPTIMIZATION_ACTUAL
#endif

