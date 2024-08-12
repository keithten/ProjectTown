//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

//main
#include "ApparanceUnrealEditor.h"

//unreal
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Interfaces/IMainFrameModule.h"
#include "Editor.h"
#include "Editor/EditorPerformanceSettings.h"
#include "Engine/Selection.h"
#include "DrawDebugHelpers.h"
#include "IAssetTools.h"
#include "IAssetTypeActions.h"
#include "AssetToolsModule.h"
#include "Styling/SlateStyleRegistry.h"
#include "EngineUtils.h"
#include "LevelEditor.h"
#include "SLevelViewport.h"
#include "Components/ActorComponent.h"
#include "ISettingsModule.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/FileHelper.h"
#if UE_VERSION_AT_LEAST( 5, 0, 0 )
#include "AssetRegistry/AssetRegistryModule.h"
#else
#include "AssetRegistryModule.h"
#endif
#if UE_VERSION_AT_LEAST( 5, 1, 0 )
#include "EditorStyleSet.h"
#endif

//module
#include "ApparanceUnreal.h"
#include "ApparanceEntity.h"
#include "Utility/ApparanceConversion.h"
#include "ApparanceResourceList.h"
#include "ApparanceResourceListActions.h"
#include "ApparanceEngineSetup.h"
#include "ApparanceEntityPreset.h"
#include "ApparanceEntityPresetActions.h"
#include "Utility/RemoteEditing.h"
#include "EntityProperties.h"
#include "ResourceListEntryProperties.h"
#include "Blueprint/ApparanceRebuildNode.h"
#include "BlueprintNodeProperties.h"
#include "EntityPresetProperties.h"
#include "ApparanceEntityFactory.h"
#include "ApparanceEntityPresetActorFactory.h"
#include "ApparanceProcedureLibrary.h"
#include "Editing/EntityEditingMode.h"

//module editor
#include "Utility/ThrottleOverride.h"


DEFINE_LOG_CATEGORY(LogApparance);
#define LOCTEXT_NAMESPACE "FApparanceUnrealEditorModule"

// HELPERS 
#if UE_VERSION_OLDER_THAN(5,0,0)
bool IsValid( UObject* p ) { return p && !p->IsPendingKill(); }
bool IsValidChecked( UObject* p ) { check( p );  return !p->IsPendingKill(); }
#endif

//additional visualisations (show component transforms)
#define APPARANCE_UNREAL_DRAW_TRANSFORMS 0

FThrottleOverride g_CPUThrottleOverride;
FApparanceUnrealEditorModule* FApparanceUnrealEditorModule::EditorModule = nullptr;

static UPackage* g_ProcPackage = nullptr;
static UObject* g_DummyObject = nullptr;

// Editor module startup
//
void FApparanceUnrealEditorModule::StartupModule()
{
	EditorModule = this;
	PendingProcedure = 0;

	// we want to be updated regularly
	TickDelegate = FTickerDelegate::CreateRaw(this, &FApparanceUnrealEditorModule::Tick);
	TickDelegateHandle = TICKER_TYPE::GetCoreTicker().AddTicker(TickDelegate);
	ReleaseNotesTimer = 1.0f; //time to popup

	// we want notifications from the main apparance module
	MainModule = &FModuleManager::GetModuleChecked<FApparanceUnrealModule>( "ApparanceUnreal" );
	if(MainModule)
	{
		MainModule->SetEditorModule( this );
	}

	// monitors
	//   selection
	USelection::SelectionChangedEvent.AddRaw(this, &FApparanceUnrealEditorModule::OnEditorSelectionChanged);
	//   property edits
	FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this, &FApparanceUnrealEditorModule::OnObjectPropertyEdited);

	//set up various systems
	g_CPUThrottleOverride.Init();
	InitExtensibility();
	InitAssets();
	InitMenus();
	InitUI();
	InitEditorInterop();
	InitPlacement();
	InitSettings();
	InitEditingModes();
	//see: "LateInit"
	//InitWorkspace();

	//create package as proxy for native procedure data files so we have something to block exit when they need saving
	FString path = FApparanceUnrealModule::GetModule()->GetProceduresPath();
	path = path.RightChop( FPaths::ProjectContentDir().Len() );
	path = TEXT("/Game") / path / TEXT( "ApparanceProcedures" );
	//should be: "/Game/Apparance/Procedures/ApparanceProcedures" or similar
#if UE_VERSION_AT_LEAST(5,0,0)
	g_ProcPackage = CreatePackage( *path );
	g_ProcPackage->AddToRoot();
	g_ProcPackage->MarkAsFullyLoaded(); //fake loading, we're not using it to store anything
	//add dummy object into package (save ignores empty packages)
	g_DummyObject = NewObject<UApparanceProcedureLibrary>( g_ProcPackage, FName( "Apparance Procedures" ), RF_Public|RF_Standalone|RF_WasLoaded ); //type/name chosen to give it the appearance of a proc library in save dialog
	g_DummyObject->AddToRoot();
	FAssetRegistryModule::AssetCreated( g_DummyObject );
	UPackage::PreSavePackageWithContextEvent.AddRaw( this, &FApparanceUnrealEditorModule::OnPackageSave );
#else //4.2*
	g_ProcPackage = NewObject<UPackage>( nullptr, *path, RF_Public );
	g_ProcPackage->AddToRoot();
	g_ProcPackage->MarkAsFullyLoaded(); //fake loading, we're not using it to store anything
	//add dummy object into package (save ignores empty packages)
	g_DummyObject = NewObject<UApparanceProcedureLibrary>( g_ProcPackage, FName( "Apparance Procedures" ), RF_Public | RF_Standalone | RF_WasLoaded ); //type/name chosen to give it the appearance of a proc library in save dialog
	g_DummyObject->AddToRoot();
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>( TEXT( "AssetRegistry" ) ).Get();
	AssetRegistry.AssetCreated( g_DummyObject );
	UPackage::PreSavePackageEvent.AddRaw( this, &FApparanceUnrealEditorModule::OnPackageSave );
#endif

	//misc
	bWasInGameViewBeforeInteractiveEditingBegan = false;
}


// are there unsaved procedures?
//
void FApparanceUnrealEditorModule::NotifyProceduresDirty( bool are_dirty )
{
	//UE_LOG( LogApparance, Log, TEXT( "Procedures marked as %s" ), are_dirty ? TEXT( "DIRTY" ) : TEXT( "CLEAN" ) );
	g_ProcPackage->SetDirtyFlag( are_dirty );
}

// monitor package saves during shutdown (or save all) for our procedures proxy
//
void FApparanceUnrealEditorModule::OnPackageSave( UPackage* Package
#if UE_VERSION_AT_LEAST(5,0,0)
		, FObjectPreSaveContext ObjectSaveContext
#endif
	)
{
	if(Package == g_ProcPackage)
	{
		//this means there were unsaved procedures and the user wants them saved as we are exiting
		SaveProcedures();
	}
}

void FApparanceUnrealEditorModule::SaveProcedures()
{
	UE_LOG( LogApparance, Log, TEXT( "Saving Apparance procedures" ) );
	
	//note: blocking
	GetEditor()->Save();
}

// Editor module shutdown
//
void FApparanceUnrealEditorModule::ShutdownModule()
{
	//release loading support objects objects
	if(g_ProcPackage) { g_ProcPackage->RemoveFromRoot(); g_ProcPackage = nullptr; }
	if(g_DummyObject) { g_DummyObject->RemoveFromRoot(); g_DummyObject = nullptr; }

	//shut down timers
	TICKER_TYPE::GetCoreTicker().RemoveTicker(TickDelegateHandle);
	TickDelegateHandle.Reset();
	TickDelegate = nullptr;

	//unhook unreal events
	USelection::SelectionChangedEvent.RemoveAll(this);
	FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);
	
	//shut down systems
	// See: InitWorkspace
	//ShutdownWorkspace(); - now on callback to happen earlier
	ShutdownUI();
	ShutdownMenus();
	ShutdownAssets();
	ShutdownExtensibility();
	ShutdownEditorInterop();
	ShutdownPlacement();
	ShutdownSettings();
	ShutdownEditingModes();
	g_CPUThrottleOverride.Shutdown();
	
	EditorModule = nullptr;
}

//remote editor API accessor
Apparance::IEditor* FApparanceUnrealEditorModule::GetEditor()
{
	if(FApparanceUnrealModule::GetEngine() != nullptr)
	{
		return FApparanceUnrealModule::GetEngine()->GetEditor();
	}
	return nullptr;
}

// helper
void DrawDebugBounds( const AApparanceEntity* pentity)
{
	float xy_basis = 0;
	float z_basis = 0;

	//proper bounds calc
	Apparance::Frame frame;
	bool is_worldspace = false;
	EApparanceFrameOrigin frame_origin = EApparanceFrameOrigin::Face;
	pentity->FindFirstFrame( frame, is_worldspace, frame_origin );

	//find unreal transform for this frame, relative to the containing entity
	FMatrix transform_rst;
	FMatrix transform_r;
	UnrealTransformsFromApparanceFrame( frame, pentity, &transform_rst, &transform_r, nullptr, nullptr, is_worldspace, frame_origin );

	//combine
	FVector origin = transform_rst.GetOrigin();
	FVector size = transform_rst.GetScaleVector();
	FVector extent = size * 0.5f;
	FVector extent_off( size.X*xy_basis, size.Y*xy_basis, size.Z*z_basis );
	FVector extent_rot = FVector(transform_r.TransformPosition(extent_off));
	FQuat rotation(transform_r);
	FColor c(64,64,64);	//dark grey

	UWorld* pworld = pentity->GetWorld();
	DrawDebugBox(pworld, origin + extent_rot, extent, rotation, c);

#if APPARANCE_UNREAL_DRAW_TRANSFORMS
	pentity->DrawDebugComponents();
#endif
}

// Selection Changed - tracking selected entities
//
void FApparanceUnrealEditorModule::OnEditorSelectionChanged(UObject* NewSelection)
{
	//any selected entities?
	USelection* sel = GEditor->GetSelectedActors();
	sel->GetSelectedObjects<AApparanceEntity>(SelectedEntities);
}

// an object has had it's properties changed
// NOTE: This is needed to spot changes to component templates used by resource list entries for component placement and the only way we can tell if we should invalidate any entities
//
void FApparanceUnrealEditorModule::OnObjectPropertyEdited(UObject* InObject, FPropertyChangedEvent& InPropertyChangedEvent)
{
	UApparanceResourceListEntry_Component* pcomponent_resource = Cast<UApparanceResourceListEntry_Component>(InObject);
	if (pcomponent_resource)
	{
		RebuildAllEntities( pcomponent_resource->ComponentTemplate );
	}
}


// helper to check for game mode in viewport
//
static bool IsMainViewportInGameViewMode()
{
	FLevelEditorModule& level_editor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	SLevelViewport* pviewport = level_editor.GetFirstActiveLevelViewport().Get();	
	return pviewport && pviewport->IsInGameView();
}

// regular Update/Tick
//
bool FApparanceUnrealEditorModule::Tick(float DeltaTime)
{
	//late init
	if(!bLateInitHappened)
	{
		LateInit();		
	}

	// view update helper
	g_CPUThrottleOverride.Tick(DeltaTime);

	// selected entity bounds
	if(!IsMainViewportInGameViewMode())
	{
		for (int i = 0; i < SelectedEntities.Num(); i++)
		{
			AApparanceEntity* pentity = SelectedEntities[i];
			if(IsValid( pentity ) 
				&& !MainModule->IsEditingEnabled())	//don't show bounds when using the Smart Editing system
			{
				DrawDebugBounds( pentity );
			}
		}
	}

	// check PiE/SiE state changes
	CheckPlayInEditor();

	// monitor for changes caused by external editor
	CheckExternalChanges();

	// editor installer
	TickInstallation();

	// release notes/setup popup
	if (ReleaseNotesTimer > 0)
	{
		ReleaseNotesTimer -= DeltaTime;
		if (ReleaseNotesTimer <= 0)
		{
			//setup still needed?
			if(SApparanceAbout::IsSetupNeeded())
			{
				OnOpenAboutWindow( EApparanceAboutPage::Setup );
			}
			//just release notes needed?
			else if (CheckForVersionChange())
			{
				OnOpenAboutWindow( EApparanceAboutPage::ReleaseNotes );
			}
		}
	}

	return true;
}

// icon resource helper
//
void FApparanceUnrealEditorModule::RegisterGeneralIcon( FString icon_path, FName name )
{
	FSlateImageBrush* icon = new FSlateImageBrush( AssetStyleSet->RootToContentDir( icon_path, TEXT( ".png" ) ), FVector2D( 32, 32 ), FLinearColor::White, ESlateBrushTileType::NoTile );
	AssetStyleSet->Set( name, icon );
}
void FApparanceUnrealEditorModule::CopyEditorIcon( FName source_name, FName name )
{
	const FSlateBrush* icon = APPARANCE_STYLE.GetBrush( source_name );	
	AssetStyleSet->Set( name, const_cast<FSlateBrush*>( icon ) );
}


// asset management setup
//
void FApparanceUnrealEditorModule::InitAssets()
{
	// gather
	FString ContentDir = IPluginManager::Get().FindPlugin("ApparanceUnreal")->GetBaseDir() / "Content";
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	//use custom category for procedural assets
	EAssetTypeCategories::Type procedural_category = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("Procedural")), LOCTEXT("Procedural", "Procedural"));

	//register our asset types
	AssetTools.RegisterAssetTypeActions( MakeShareable( new FApparanceUnrealEditorResourceListActions(procedural_category) ));
	AssetTools.RegisterAssetTypeActions( MakeShareable( new FApparanceUnrealEditorEntityPresetActions(procedural_category) ));

	//asset factories
	auto PresetFactory = NewObject<UApparanceEntityPresetActorFactory>();
	GEditor->ActorFactories.Add( PresetFactory );

	//style setup for asset appearance
	AssetStyleSet = MakeShareable(new FSlateStyleSet("Apparance"));
	AssetStyleSet->SetContentRoot(ContentDir);
	//entity icon
	FSlateImageBrush* EntityIcon = new FSlateImageBrush( AssetStyleSet->RootToContentDir( TEXT( "Icons/Apparance_64x" ), TEXT( ".png" ) ), FVector2D( 64.f, 64.f ) );
	if(EntityIcon){	AssetStyleSet->Set( "Apparance.ApparanceEntity_64x", EntityIcon );}
	//asset icons
	FSlateImageBrush* ResourceListThumbnail = new FSlateImageBrush(AssetStyleSet->RootToContentDir( TEXT("Icons/ApparanceResourceList"), TEXT(".png") ), FVector2D(128.f, 128.f) );
	if (ResourceListThumbnail){	AssetStyleSet->Set("ClassThumbnail.ApparanceResourceList", ResourceListThumbnail);}
	FSlateImageBrush* EntityPresetThumbnail = new FSlateImageBrush(AssetStyleSet->RootToContentDir( TEXT("Icons/ApparanceEntityPreset"), TEXT(".png") ), FVector2D(128.f, 128.f) );
	if (EntityPresetThumbnail) { AssetStyleSet->Set("ClassThumbnail.ApparanceEntityPreset", EntityPresetThumbnail); }
	//about screen and general branding
	FSlateImageBrush* ApparanceLogo80 = new FSlateImageBrush( AssetStyleSet->RootToContentDir( TEXT("Icons/ApparanceLogo_80x"), TEXT(".png") ), FVector2D(275,80), FLinearColor::White, ESlateBrushTileType::NoTile );
	AssetStyleSet->Set( "Apparance.Logo.Large", ApparanceLogo80 );
	FSlateImageBrush* ApparanceLogo50 = new FSlateImageBrush( AssetStyleSet->RootToContentDir( TEXT( "Icons/ApparanceLogo_50x" ), TEXT( ".png" ) ), FVector2D(172,50), FLinearColor::White, ESlateBrushTileType::NoTile );
	AssetStyleSet->Set( "Apparance.Logo.Small", ApparanceLogo50 );

	//setup/about window icons
	RegisterGeneralIcon( TEXT( "Icons/Icon_Info_32x" ), "Apparance.Icon.Info" );
	//CopyEditorIcon( "Icons.Info", "Apparance.Icon.Info" );
	RegisterGeneralIcon( TEXT( "Icons/Icon_Warning_32x" ), "Apparance.Icon.Warning" );
	//CopyEditorIcon( "Icons.Warning", "Apparance.Icon.Warning" );
	RegisterGeneralIcon( TEXT( "Icons/Icon_Error_32x" ), "Apparance.Icon.Error" );
	//CopyEditorIcon( "Icons.Error", "Apparance.Icon.Error" );
	RegisterGeneralIcon( TEXT( "Icons/Icon_Important_32x" ), "Apparance.Icon.Important" );
	//CopyEditorIcon( "NotificationList.DefaultMessage", "Apparance.Icon.Important" );
	RegisterGeneralIcon( TEXT( "Icons/Icon_Web_32x" ), "Apparance.Icon.Web" );
	//CopyEditorIcon( "LevelEditor.WorldProperties", "Apparance.Icon.Web" );
	RegisterGeneralIcon( TEXT( "Icons/Icon_Setup_32x" ), "Apparance.Icon.Setup" );	
	//CopyEditorIcon( "LevelEditor.GameSettings", "Apparance.Icon.Setup" );
	RegisterGeneralIcon( TEXT( "Icons/Icon_Stats_32x" ), "Apparance.Icon.Stats" );
	//CopyEditorIcon( "MaterialEditor.ToggleMaterialStats", "Apparance.Icon.Stats" );	

	//tool icons
	FSlateImageBrush* ToolkitIcon = new FSlateImageBrush( AssetStyleSet->RootToContentDir( TEXT( "Icons/Apparance_40x" ), TEXT( ".png" ) ), FVector2D( 40.f, 40.f ) );
	if(ToolkitIcon) { AssetStyleSet->Set( "Apparance.ApparanceEntity_40x", ToolkitIcon ); }
	FSlateImageBrush* ToolkitIconSm = new FSlateImageBrush( AssetStyleSet->RootToContentDir( TEXT( "Icons/Apparance_20x" ), TEXT( ".png" ) ), FVector2D( 20.f, 20.f ) );
	if(ToolkitIconSm) { AssetStyleSet->Set( "Apparance.ApparanceEntity_20x", ToolkitIconSm ); }

	//misc text
	const FTextBlockStyle NormalText = APPARANCE_STYLE.GetWidgetStyle<FTextBlockStyle>("NormalText");
	AssetStyleSet->Set( "Apparance.AboutText", FTextBlockStyle(NormalText)
		.SetFont( FCoreStyle::GetDefaultFontStyle( "Mono", 10.0f ) ));
	
	FSlateStyleRegistry::RegisterSlateStyle(*AssetStyleSet);
}

void FApparanceUnrealEditorModule::ShutdownAssets()
{
	//release our custom style
	FSlateStyleRegistry::UnRegisterSlateStyle( *AssetStyleSet.Get() );
	ensure(AssetStyleSet.IsUnique());
	AssetStyleSet.Reset();
}







#include "Framework/Commands/Commands.h"
#include "EditorStyleSet.h"

class FApparanceCommands : public TCommands<FApparanceCommands>
{
	
public:
	FApparanceCommands() : TCommands<FApparanceCommands>
		(
		"Apparance",
		NSLOCTEXT("Apparance", "Apparance", "Apparance"),
		NAME_None,
		APPARANCE_STYLE.GetStyleSetName()
	)
	{}
	
	/** Launch external editor */
	TSharedPtr< FUICommandInfo > OpenEditor;

	/** Open URL (website) */
	TSharedPtr< FUICommandInfo > OpenQuickstart;
	/** Open URL (documentation) */
	TSharedPtr< FUICommandInfo > OpenDocumentation;
	/** Open URL (website) */
	TSharedPtr< FUICommandInfo > OpenWebsite;

	/** Open About window (Setup page) */
	TSharedPtr< FUICommandInfo > OpenSetup;
	/** Open About window (About page) */
	TSharedPtr< FUICommandInfo > OpenAbout;
	/** Open About window (Release Notes page) */
	TSharedPtr< FUICommandInfo > OpenReleaseNotes;
	/** Open About window (Status page) */
	TSharedPtr< FUICommandInfo > OpenStatus;

	/** Open project settings for this plugin */
	TSharedPtr< FUICommandInfo > OpenProjectSettings;

	/**
	* Initialize commands
	*/
	virtual void RegisterCommands() override
	{
		UI_COMMAND( OpenSetup, "Set-up", "Open helper window to guide you through getting your project set up", EUserInterfaceActionType::Button, FInputChord() );
		UI_COMMAND( OpenEditor, "Open Editor", "Launch the external Apparance editor", EUserInterfaceActionType::Button, FInputChord() );
		UI_COMMAND( OpenQuickstart, "Getting Started", "Online quick-start guide for your first steps in using Apparance", EUserInterfaceActionType::Button, FInputChord() );
		UI_COMMAND( OpenDocumentation, "Open Documentation", "Open the Apparance documentation pages in a browser", EUserInterfaceActionType::Button, FInputChord() );
		UI_COMMAND( OpenWebsite, "Open Website", "Open the Apparance website in a browser", EUserInterfaceActionType::Button, FInputChord() );
		UI_COMMAND( OpenAbout, "About", "Show information about the Apparance plugin", EUserInterfaceActionType::Button, FInputChord() );
		UI_COMMAND( OpenReleaseNotes, "Release Notes", "Show latest features, improvements and fixes", EUserInterfaceActionType::Button, FInputChord() );
		UI_COMMAND( OpenStatus, "Status", "Show stats about the Apparance plugin and the current scene", EUserInterfaceActionType::Button, FInputChord() );
		UI_COMMAND( OpenProjectSettings, "Project Settings", "Open project settings for the Apparance plugin", EUserInterfaceActionType::Button, FInputChord() );
	}
};


// allow placement of actor (by drag and drop) from Place Actor panel
// NOTE: By default this should already happen, but I think if you start customising with factories/etc too much it stops working
//
void FApparanceUnrealEditorModule::InitPlacement()
{
	//register for placement refresh events
	check( IPlacementModeModule::IsAvailable() );
	IPlacementModeModule& PlacementModeModule = IPlacementModeModule::Get(); //(NOTE: requires editor plugin to be in PostEngineInit startup phase for the placementmodule to be available)
	PlacementModeModule.OnPlacementModeCategoryRefreshed().AddRaw( this, &FApparanceUnrealEditorModule::OnPlacementModeRefresh );
	
	//initial registration
	OnPlacementModeRefresh( FBuiltInPlacementCategories::Basic() );
	OnPlacementModeRefresh( FBuiltInPlacementCategories::AllClasses() );
}

// unhook placement event handler
//
void FApparanceUnrealEditorModule::ShutdownPlacement()
{
	if(IPlacementModeModule::IsAvailable())
	{
		IPlacementModeModule::Get().OnPlacementModeCategoryRefreshed().RemoveAll( this );
	}
}

// (re)register actor entity for placement
//
void FApparanceUnrealEditorModule::OnPlacementModeRefresh( FName CategoryName )
{
	if(CategoryName == FBuiltInPlacementCategories::Basic() || CategoryName== FBuiltInPlacementCategories::AllClasses())
	{
		IPlacementModeModule& PlacementModeModule = IPlacementModeModule::Get();

/* 4.27 version
		FPlaceableItem(
			UClass& InAssetClass,
			const FAssetData& InAssetData,
			FName InClassThumbnailBrushOverride = NAME_None,
			TOptional<FLinearColor> InAssetTypeColorOverride = TOptional<FLinearColor>(),
			TOptional<int32> InSortOrder = TOptional<int32>(),
			TOptional<FText> InDisplayName = TOptional<FText>()
		)
*/
/* 5.0EA version
		FPlaceableItem(
			UClass & InAssetClass,
			const FAssetData & InAssetData,
			FName InClassThumbnailBrushOverride = NAME_None,
			FName InClassIconBrushOverride = NAME_None,										<<<< EXTRA PARAMETER
			TOptional<FLinearColor> InAssetTypeColorOverride = TOptional<FLinearColor>(),
			TOptional<int32> InSortOrder = TOptional<int32>(),
			TOptional<FText> InDisplayName = TOptional<FText>()
		)
*/

		//our placeable apparance entity appearance
		FPlaceableItem* Placement = new FPlaceableItem(
			*UApparanceEntityFactory::StaticClass(),
			FAssetData( AApparanceEntity::StaticClass() ),
			FName( "Apparance.ApparanceEntity_64x" ),
#if !UE_VERSION_OLDER_THAN(5,0,0)
			FName( "Apparance.ApparanceEntity_64x" ),
#endif
			TOptional<FLinearColor>( FColor::Emerald.ReinterpretAsLinear() ),
			TOptional<int32>(),
			LOCTEXT( "ApparanceEntityActorName", "Apparance Entity" )
		);
		Placement->bAlwaysUseGenericThumbnail = false;

		//re-register
		TOptional<FPlacementModeID>& handle = (CategoryName == FBuiltInPlacementCategories::Basic()) ? EntityPlacementHandle_Basic : EntityPlacementHandle_AllClasses;
		if(handle.IsSet())
		{
			PlacementModeModule.UnregisterPlaceableItem( handle.GetValue() );
		}
		handle = PlacementModeModule.RegisterPlaceableItem( CategoryName, MakeShareable( Placement ) ).GetValue();
	}

}

// project settings integration
//
void FApparanceUnrealEditorModule::InitSettings()
{
	if(ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>( "Settings" ))
	{
		SettingsModule->RegisterSettings( "Project", "Plugins", "Apparance",
			LOCTEXT( "ApparanceSettingsName", "Apparance" ),
			LOCTEXT( "ApparanteSettingsDescription", "Configure the Apparance plugin" ),
			GetMutableDefault<UApparanceEngineSetup>() );
	}
}

// project settings integration
//
void FApparanceUnrealEditorModule::ShutdownSettings()
{
	if(ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>( "Settings" ))
	{
		SettingsModule->UnregisterSettings( "Project", "Plugins", "Apparance" );
	}
}

// editing modes are go
//
void FApparanceUnrealEditorModule::InitEditingModes()
{
	FEditorModeRegistry::Get().RegisterMode<FEntityEditingMode>(
		FEntityEditingMode::EM_Main,
		FText::FromString( "Entity Editor" ),
		FSlateIcon( AssetStyleSet->GetStyleSetName(), "Apparance.ApparanceEntity_40x", "Apparance.ApparanceEntity_20x" ),
		true, 1000
		);
}

// editing modes done
//
void FApparanceUnrealEditorModule::ShutdownEditingModes()
{
	FEditorModeRegistry::Get().UnregisterMode( FEntityEditingMode::EM_Main );
}



// menu items setup
//
void FApparanceUnrealEditorModule::InitMenus()
{
	FApparanceCommands::Register();

	//commands for menus
	TSharedPtr<FUICommandList> commands = MakeShareable(new FUICommandList);
	commands->MapAction(
		FApparanceCommands::Get().OpenSetup,
		FExecuteAction::CreateRaw( this, &FApparanceUnrealEditorModule::OnOpenAboutWindow, EApparanceAboutPage::Setup ) );
	commands->MapAction(
		FApparanceCommands::Get().OpenEditor,
		FExecuteAction::CreateRaw( this, &FApparanceUnrealEditorModule::LaunchApparanceEditor, (Apparance::ProcedureID)0 ),
		FCanExecuteAction::CreateRaw( this, &FApparanceUnrealEditorModule::CanLaunchApparanceEditor ) );
	commands->MapAction(
		FApparanceCommands::Get().OpenQuickstart,
		FExecuteAction::CreateRaw( this, &FApparanceUnrealEditorModule::OnOpenURL, TEXT( "https://apparance.uk/tutorial-unreal-plugin.htm" ) ) );
	commands->MapAction(
		FApparanceCommands::Get().OpenDocumentation,
		FExecuteAction::CreateRaw( this, &FApparanceUnrealEditorModule::OnOpenURL, TEXT( "https://apparance.uk/manual-unreal.htm" ) ) );
	commands->MapAction(
		FApparanceCommands::Get().OpenWebsite,
		FExecuteAction::CreateRaw( this, &FApparanceUnrealEditorModule::OnOpenURL, TEXT("https://apparance.uk") ) );
	commands->MapAction(
		FApparanceCommands::Get().OpenAbout,
		FExecuteAction::CreateRaw( this, &FApparanceUnrealEditorModule::OnOpenAboutWindow, EApparanceAboutPage::About ) );
	commands->MapAction(
		FApparanceCommands::Get().OpenReleaseNotes,
		FExecuteAction::CreateRaw( this, &FApparanceUnrealEditorModule::OnOpenAboutWindow, EApparanceAboutPage::ReleaseNotes ) );
	commands->MapAction(
		FApparanceCommands::Get().OpenStatus,
		FExecuteAction::CreateRaw( this, &FApparanceUnrealEditorModule::OnOpenAboutWindow, EApparanceAboutPage::Status ) );
	commands->MapAction(
		FApparanceCommands::Get().OpenProjectSettings,
		FExecuteAction::CreateRaw( this, &FApparanceUnrealEditorModule::OnOpenProjectSettings ) );
	
	//extender for menus
	MenuExtension = MakeShareable(new FExtender);
	MenuExtension->AddMenuBarExtension(
		"Window",
		EExtensionHook::After,
		commands,
		FMenuBarExtensionDelegate::CreateRaw(this, &FApparanceUnrealEditorModule::AddPullDownMenu)
	);
	
	//register with editor
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtension);
}

void FApparanceUnrealEditorModule::ShutdownMenus()
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	LevelEditorModule.GetMenuExtensibilityManager()->RemoveExtender(MenuExtension);
	
	FApparanceCommands::Unregister();	
}


void FApparanceUnrealEditorModule::AddPullDownMenu(FMenuBarBuilder &menuBuilder)
{
	menuBuilder.AddPullDownMenu(
		LOCTEXT("MenuHeading", "Apparance"),
		LOCTEXT("MenuHeadingTooltip", "Apparance procedural tools"),
		FNewMenuDelegate::CreateRaw(this, &FApparanceUnrealEditorModule::FillMenu),
		"Apparance",
		FName(TEXT("Apparance"))
	);
}

void FApparanceUnrealEditorModule::FillMenu(FMenuBuilder& MenuBuilder)
{
	//Tools
	MenuBuilder.BeginSection("Tools",LOCTEXT("MenuSectionTools", "Tools"));
	{
		//    Open Editor
		MenuBuilder.AddMenuEntry(
			FApparanceCommands::Get().OpenEditor, NAME_None,
			LOCTEXT( "MenuOpenEditor", "Open Editor" ),
			LOCTEXT( "MenuOpenEditorTooltip", "Launch the Apparance procedure editor" ),
			FSlateIcon()
		);
		//    Open Project Settings
		MenuBuilder.AddMenuEntry(
			FApparanceCommands::Get().OpenProjectSettings, NAME_None,
			LOCTEXT( "MenuProjectSettings", "Project Settings" ),
			LOCTEXT( "MenuProjectSettingsTooltip", "Open the project settings for configuring the Apparance plugin" ),
			FSlateIcon()
		);
		//    Setup
		MenuBuilder.AddMenuEntry(
			FApparanceCommands::Get().OpenSetup, NAME_None,
			LOCTEXT( "MenuOpenSetup", "Setup Wizard" ),
			LOCTEXT( "MenuOpenSetupTooltip", "Open helper window to guide you through getting your project set up" ),
			FSlateIcon()
		);
	}
	MenuBuilder.EndSection();
	//Help
	MenuBuilder.BeginSection("Help",LOCTEXT("MenuSectionHelp", "Help"));
	{
		//    URL Quickstart
		MenuBuilder.AddMenuEntry(
			FApparanceCommands::Get().OpenQuickstart, NAME_None,
			LOCTEXT( "MenuOpenQuickstart", "Getting Started" ),
			LOCTEXT( "MenuOpenQuickstartTooltip", "Online quick-start guide for your first steps with Apparance" ),
			FSlateIcon()
		);
		//    URL Documentation
		MenuBuilder.AddMenuEntry(
			FApparanceCommands::Get().OpenDocumentation, NAME_None,
			LOCTEXT("MenuOpenDocumentation","Documentation"),
			LOCTEXT("MenuOpenDocumentationTooltip","Open the Apparance documentation pages"),
			FSlateIcon()
		);		
		//    URL Website
		MenuBuilder.AddMenuEntry(
			FApparanceCommands::Get().OpenWebsite, NAME_None,
			LOCTEXT( "MenuOpenWebsite", "Web site" ),
			LOCTEXT( "MenuOpenWebsiteTooltip", "Visit the Apparance website" ),
			FSlateIcon()
		);
	}
	MenuBuilder.EndSection();
	//Information
	MenuBuilder.BeginSection("Information",LOCTEXT("MenuSectionInformation", "Information"));
	{
		//    Open Release Notes
		MenuBuilder.AddMenuEntry(
			FApparanceCommands::Get().OpenReleaseNotes, NAME_None,
			LOCTEXT("MenuOpenReleaseNotes","Release Notes" ),
			LOCTEXT("MenuOpenReleaseNotesTooltip","Latest changes and fixes" ),
			FSlateIcon()
		);
		//    Open Status
		MenuBuilder.AddMenuEntry(
			FApparanceCommands::Get().OpenStatus, NAME_None,
			LOCTEXT("MenuOpenStatus","Status" ),
			LOCTEXT("MenuOpenStatusTooltip", "Apparance status and scene information" ),
			FSlateIcon()
		);
		//    Open About
		MenuBuilder.AddMenuEntry(
			FApparanceCommands::Get().OpenAbout, NAME_None,
			LOCTEXT( "MenuOpenAbout", "About Apparance" ),
			LOCTEXT( "MenuOpenAboutTooltip", "Information about the Apparance plugin" ),
			FSlateIcon()
		);

		// Entities ------
		//entity tools
	}
	MenuBuilder.EndSection();	
}

// open the Apparance about window popup (to a specific page)
//
void FApparanceUnrealEditorModule::OnOpenAboutWindow( EApparanceAboutPage page )
{
	if(AboutWindow.IsValid())
	{
		AboutWindow->BringToFront();
		return;
	}

	const FText AboutWindowTitle = LOCTEXT( "AboutApparancePluginTitle", "The Apparance Plugin" );
	
	//create panel
	AboutPanel = SNew(SApparanceAbout);
	AboutPanel->SetPage( page );

	//wrap in window
	AboutWindow = 
		SNew(SWindow)
		.Title( AboutWindowTitle )
		.ClientSize(FVector2D(800.f, 500.f))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.SizingRule( ESizingRule::FixedSize )
		[
			AboutPanel.ToSharedRef()
		];
	AboutWindow->SetOnWindowClosed( FOnWindowClosed::CreateRaw( this, &FApparanceUnrealEditorModule::OnAboutWindowClosed ) );
	AboutPanel->SetWindow( AboutWindow );

	IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>( "MainFrame" );
	TSharedPtr<SWindow> ParentWindow = MainFrame.GetParentWindow();

#if false
	if ( ParentWindow.IsValid() )
	{
		FSlateApplication::Get().AddModalWindow(AboutWindow.ToSharedRef(), ParentWindow.ToSharedRef());
	}
	else
#endif
	{
		FSlateApplication::Get().AddWindow( AboutWindow.ToSharedRef() );
	}
}

// respond to window closing
void FApparanceUnrealEditorModule::OnAboutWindowClosed( const TSharedRef<SWindow>& window )
{
	//close the window
	AboutWindow.Reset();
	AboutPanel.Reset();
}

// open a website in the users browser
//
void FApparanceUnrealEditorModule::OnOpenURL(const TCHAR* url)
{
	FPlatformProcess::LaunchURL( url, NULL, NULL);
}

// open project settings on the page for ourselves
//
void FApparanceUnrealEditorModule::OnOpenProjectSettings()
{
	//open project settings
	FModuleManager::LoadModuleChecked<ISettingsModule>( "Settings" ).ShowViewer( FName( "Project" ), FName( "Plugins" ), FName( "Apparance" ) );
}

// open a procedure in the editor
//
void FApparanceUnrealEditorModule::OpenProcedure( Apparance::ProcedureID proc_id )
{
	LaunchApparanceEditor( proc_id );
}

void FApparanceUnrealEditorModule::LaunchApparanceEditor( Apparance::ProcedureID proc_id )
{
	if(!GetEditor()->IsInstalled())
	{
		//needs installing, opens when complete
		if(GetEditor()->RequestInstallation())
		{
			InstallingProgress = MakeShareable( new FScopedSlowTask( 100.0f, LOCTEXT( "BeginEditorInstall", "Installing Apparance Editor" ), true ) );
			InstallingProgress->MakeDialog();
			ProgressPercentage = 0;
			CurrentProgressMessage = FText();
			PendingProcedure = proc_id;

			//editor is also opened after install
		}
	}
	else
	{
		//already installed, can open straight away
		GetEditor()->Open( proc_id );
	}
}

// can we launch the editor?
//
bool FApparanceUnrealEditorModule::CanLaunchApparanceEditor() const
{
	return !IsInstalling();
}

// installation process
void FApparanceUnrealEditorModule::TickInstallation()
{
	if(IsInstalling())
	{
		//check messages
		bool failed = false;
		while(true)
		{
			wchar_t message_buffer[1024];
			int message_type = GetEditor()->GetInstallationMessage( 1023, message_buffer );
			if(message_type == 0)
			{
				//no (more) messages
				break;
			}
			//display
			switch(message_type)
			{
			case 1: //Info/status
			{
				CurrentProgressMessage = FText::FromString( FString( message_buffer ) );
				UE_LOG( LogApparance, Log, TEXT("%s"), message_buffer);
				break;
			}
			case 2: //Warning
				UE_LOG( LogApparance, Warning, TEXT("%s"), message_buffer );
				break;
			case 3: //Error
				UE_LOG( LogApparance, Error, TEXT("%s"), message_buffer );
				failed = true;
				break;
			}
		}

		//check %
		float progress = GetEditor()->GetInstallationProgress();
		int iprogress = (int)(progress * 100.0f);
		if(iprogress > ProgressPercentage)
		{
			//advanced
			float advance = iprogress - ProgressPercentage;
			ProgressPercentage = iprogress;

			//update progress UI
			InstallingProgress->EnterProgressFrame( (float)advance );
		}

		//message
		if(!CurrentProgressMessage.IsEmpty())
		{
			InstallingProgress->FrameMessage = CurrentProgressMessage;
		}

		//check completion
		if(GetEditor()->IsInstalled() || failed)
		{
			//complete
			InstallingProgress.Reset();
			if(failed)
			{
				UE_LOG( LogApparance, Error, TEXT( "Apparance Editor installation failed. See above for error details. You may need to re-install or update the plugin and try again." ) );
			}
			else
			{
				UE_LOG( LogApparance, Log, TEXT( "Apparance Editor installed successfully." ) );
				
				//open as requested
				GetEditor()->Open( PendingProcedure );
				PendingProcedure = 0;
			}
		}
	}
}

// has the plugin version changed since we last ran?
//
bool FApparanceUnrealEditorModule::CheckForVersionChange()
{
	//load version text
	const FString& save_dir = FPaths::ProjectSavedDir();
	static TCHAR version_file_name[] = TEXT("apparance_version.ini");
	FString version_path = FPaths::Combine(save_dir,version_file_name);
	FString file_version_text;
	FFileHelper::LoadFileToString(file_version_text, *version_path);
	
	//get our version
	FString live_version_text;
	Apparance::IParameterCollection* version_info = FApparanceUnrealModule::GetEngine()->GetVersionInformation();
	version_info->BeginAccess();
	int num_ver_chars = 0;
	const Apparance::ValueID version_number_id = (Apparance::ValueID)1;
	if (version_info->FindString(version_number_id, 0, nullptr, &num_ver_chars))
	{
		live_version_text = FString::ChrN(num_ver_chars, ' ');
		wchar_t* pbuffer = &live_version_text[0];
		version_info->FindString(version_number_id, num_ver_chars, pbuffer);
	}
	version_info->EndAccess();
	delete version_info;

	//compare
	bool changed = file_version_text.Compare(live_version_text) != 0;

	//update
	if (changed)
	{
		FFileHelper::SaveStringToFile(live_version_text, *version_path);
	}

	return changed;
}

// setup extensibility systems for extending editor and building custom panels
//
void FApparanceUnrealEditorModule::InitExtensibility()
{
	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
	ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);
	
}

// clean up
//
void FApparanceUnrealEditorModule::ShutdownExtensibility()
{
	MenuExtensibilityManager.Reset();
	ToolBarExtensibilityManager.Reset();
}

// setup UI customisations
//
void FApparanceUnrealEditorModule::InitUI()
{
	//register prop customiser
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");	
	//entity (and entity like objects)
	PropertyModule.RegisterCustomClassLayout( AApparanceEntity::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic( &FEntityPropertyCustomisation::MakeInstance ) );
	PropertyModule.RegisterCustomClassLayout( UApparanceEntityPreset::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic( &FEntityPresetPropertyCustomisation::MakeInstance ) );
	//blueprint nodes with parameters
	PropertyModule.RegisterCustomClassLayout( UApparanceRebuildNode::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic( &FNodePropertyCustomisation_Rebuild::MakeInstance ) );
	PropertyModule.RegisterCustomClassLayout( UApparanceMakeParametersNode::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic( &FNodePropertyCustomisation_MakeParameters::MakeInstance ) );
	PropertyModule.RegisterCustomClassLayout( UApparanceBreakParametersNode::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic( &FNodePropertyCustomisation_BreakParameters::MakeInstance ) );
	//resource list entry
	PropertyModule.RegisterCustomClassLayout( UApparanceResourceListEntry::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic( &FResourcesListEntryPropertyCustomisation::MakeInstance ) );

}

// clean up
//
void FApparanceUnrealEditorModule::ShutdownUI()
{
	//unregister prop customiser
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.UnregisterCustomClassLayout( AApparanceEntity::StaticClass()->GetFName() );
	PropertyModule.UnregisterCustomClassLayout( UApparanceRebuildNode::StaticClass()->GetFName() );
	PropertyModule.UnregisterCustomClassLayout( UApparanceMakeParametersNode::StaticClass()->GetFName() );
	PropertyModule.UnregisterCustomClassLayout( UApparanceBreakParametersNode::StaticClass()->GetFName() );
	
}

// some stuff can't be initialised until engine fully up and running
// these init's are done on first active tick/frame of module
void FApparanceUnrealEditorModule::LateInit()
{
	bLateInitHappened = true;

	InitWorkspace();
}

// set up some objects for apparance tooling use
// NOTE: Can't be called in module init (no engine)
//
void FApparanceUnrealEditorModule::InitWorkspace()
{
	//maybe there's a better way to provide a world for temp operations to occur in?
	check(GEngine);
	check(!WorkspaceWorld);

	WorkspaceWorld = UWorld::CreateWorld( EWorldType::None, false, FName("ApparanceWorkspace"), GetTransientPackage() );
	WorkspaceWorld->AddToRoot(); //keep around
	FWorldContext& WorkspaceWorldContext = GEngine->CreateNewWorldContext( EWorldType::None );
	WorkspaceWorldContext.SetCurrentWorld( WorkspaceWorld );

	//monitor shutdown early enough to clean up worlds we created (was being destroyed automatically before module shutdown and crashing because of "NetworkSubsystem")
	FCoreDelegates::OnEnginePreExit.AddRaw( this, &FApparanceUnrealEditorModule::ShutdownWorkspace );
}

// clean up
//
void FApparanceUnrealEditorModule::ShutdownWorkspace()
{
	//clean up temp objects
	if(WorkspaceWorld)
	{
		//world cleanup
		GEngine->DestroyWorldContext( WorkspaceWorld );
		WorkspaceWorld->CleanupWorld();
		WorkspaceWorld->DestroyWorld( false );

		//object cleanup
		WorkspaceWorld->RemoveFromRoot();
		WorkspaceWorld = nullptr;
	}
}

// Need to init Apparance edtior interop
//
void FApparanceUnrealEditorModule::InitEditorInterop()
{
	TSharedPtr<IPlugin> plugin = IPluginManager::Get().FindPlugin( "ApparanceUnreal" );
	check( plugin );

	//install location (Binaries, next to Content folder)
	FString BinDir = plugin->GetBaseDir() / "Binaries";
	if(plugin->GetType() == EPluginType::Engine)
	{
		//running as engine plugin
		BinDir = FPlatformProcess::BaseDir() / BinDir;
	}
	FString install_dir = BinDir / "Apparance";
	//FPaths::ConvertRelativePathToFull(install_dir);  fails for some reason, reproduced here instead
	{
		FString platform_base_dir = FPlatformProcess::BaseDir();
		FString FullyPathed;
		if (FPaths::IsRelative(install_dir))
		{
			FullyPathed = MoveTemp(platform_base_dir);
			FullyPathed /= MoveTemp(install_dir);
		}
		else
		{
			FullyPathed = MoveTemp(install_dir);
		}

		FPaths::NormalizeFilename(FullyPathed);
		FPaths::CollapseRelativeDirectories(FullyPathed);

		if (FullyPathed.Len() == 0)
		{
			// Empty path is not absolute, and '/' is the best guess across all the platforms.
			// This substituion is not valid for Windows of course; however CollapseRelativeDirectories() will not produce an empty
			// absolute path on Windows as it takes care not to remove the drive letter.
			FullyPathed = TEXT("/");
		}
		install_dir = FullyPathed;
	}

	//ensure exists
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	PlatformFile.CreateDirectoryTree( *install_dir );

	//init editor api
	UE_LOG( LogApparance, Log, TEXT( "Apparance Editor install location: %s" ), *install_dir );
	GetEditor()->SetInstallDirectory( *install_dir );
}

// Done with Apparance edtior interop
//
void FApparanceUnrealEditorModule::ShutdownEditorInterop()
{
}


/* IApparanceUnrealEditorAPI implementation */
void FApparanceUnrealEditorModule::NotifyExternalContentChanged()
{
	//external changes usually need UnrealEd responsiveness boost to respond interactively
	g_CPUThrottleOverride.Boost( 10.0f );
}

void FApparanceUnrealEditorModule::NotifyAssetDatabaseChanged()
{
	RebuildAllEntities();
}

void FApparanceUnrealEditorModule::NotifyProcedureDefaultsChange( Apparance::ProcedureID proc_id, const Apparance::IParameterCollection* params, Apparance::ValueID changed_param )
{
	RebuildAffectedEntities(proc_id, changed_param);
}

void FApparanceUnrealEditorModule::NotifyEntityProcedureTypeChanged( Apparance::ProcedureID new_proc_id )
{
	OnProcedureExternalChange( new_proc_id );
}

void FApparanceUnrealEditorModule::NotifyResourceListStructureChanged( UApparanceResourceList* pres_list, UApparanceResourceListEntry* pentry, FName property )
{
	//notify any registered interested parties
	ResourceListStructuralChangeEvent.Broadcast( pres_list, pentry, property );
}

void FApparanceUnrealEditorModule::NotifyResourceListAssetChanged( UApparanceResourceList* pres_list, UApparanceResourceListEntry* pentry, UObject* pnewasset )
{
	//notify any registered interested parties
	ResourceListAssetChangeEvent.Broadcast( pres_list, pentry, pnewasset );
}

UWorld* FApparanceUnrealEditorModule::GetTempWorld()
{
	return WorkspaceWorld;
}

Apparance::IParameterCollection* FApparanceUnrealEditorModule::BeginParameterEdit( IProceduralObject* pentity, bool editing_transaction )
{
	//	UE_LOG(LogApparance,Display,TEXT("******** BEGIN param edit (%i) %p"), editing_transaction?1:0, pentity );

	if(editing_transaction)
	{
		//we are going to commit change made to parameter collection
		//(edits are applied to the non-persistent, expanded version of the parameters, then applied)
		GEditor->BeginTransaction( LOCTEXT( "ParameterEdit", "Edit Procedure Parameter" ) );

		//we are about to change the entity
		pentity->IPO_Modify();

		//assume interactive editing, hide scene widgets
		bWasInGameViewBeforeInteractiveEditingBegan = GCurrentLevelEditingViewportClient->IsInGameView();
		GCurrentLevelEditingViewportClient->SetGameView( true );
	}

	//prep params for edit
	Apparance::IParameterCollection* ent_params = pentity->IPO_BeginEditingParameters();
	return ent_params;
}

void FApparanceUnrealEditorModule::EndParameterEdit( IProceduralObject* pentity, bool editing_transaction, Apparance::ValueID input_id )
{
	//	UE_LOG(LogApparance,Display,TEXT("******** END param edit (%i) %p"), editing_transaction?1:0, pentity );

	//finished param edits
	bool dont_pack_interactive_edits = !editing_transaction;
	const Apparance::IParameterCollection* ent_params = pentity->IPO_EndEditingParameters( dont_pack_interactive_edits );

	//how to apply?
	if(editing_transaction)
	{
		//commit edit
		pentity->IPO_PostParameterEdit( ent_params, input_id );	//point at which we actually affect the persistent state of entity
		GEditor->EndTransaction();

		//restore game mode
		GCurrentLevelEditingViewportClient->SetGameView( bWasInGameViewBeforeInteractiveEditingBegan );		
	}
	else
	{
		//just interactively update
		pentity->IPO_InteractiveParameterEdit( ent_params, input_id );
	}
}



// watch for PiE and SiE session changes
//
void FApparanceUnrealEditorModule::CheckPlayInEditor()
{
	bool currently_playing = GEditor->IsPlayingSessionInEditor();
	if(currently_playing!=bPlaySessionActive)
	{
		bPlaySessionActive = currently_playing;
		if(currently_playing)
		{
			OnPlayInEditorStarting();
		}
		else
		{
			OnPlayInEditorStopped();
		}
	}
}

// PiE/SiE session is starting
// Response: Clear any editor proc-gen objects
//
void FApparanceUnrealEditorModule::OnPlayInEditorStarting()
{
	//TODO: may need to clear objects, possible efficiency/memory saving for PiE of large scenes, although may incur re-build cost, testing required
}

// PiE/SiE session has stopped
// Response: Rebuild any editor proc-gen objects
//
void FApparanceUnrealEditorModule::OnPlayInEditorStopped()
{
	RebuildAllEntities();
}

// monitor for changes caused by external editor
//
void FApparanceUnrealEditorModule::CheckExternalChanges()
{
	Apparance::ILibrary* plibrary = FApparanceUnrealModule::GetLibrary();
	if(plibrary) //main plugin ready?
	{
		//handle any changes to procedures external signiture/etc
		while(true)
		{
			Apparance::ProcedureID changed_proc = plibrary->CheckSpecNotification();
			if(changed_proc == Apparance::InvalidID)
			{
				break;
			}
			OnProcedureExternalChange( changed_proc );
		};
	}

	//sync dirtyness with our proxy
	bool are_unsaved = GetEditor()->IsUnsaved();
	if(are_unsaved != g_ProcPackage->IsDirty())
	{
		NotifyProceduresDirty( are_unsaved );
	}
}

// respond to a procedure's signature changing
//
void FApparanceUnrealEditorModule::OnProcedureExternalChange( Apparance::ProcedureID changed_proc )
{
	FPropertyEditorModule& PropPlugin = FModuleManager::LoadModuleChecked<FPropertyEditorModule>( "PropertyEditor" );

	//rebuild any affected (e.g. default value changes)
	FApparanceUnrealModule::GetModule()->Editor_NotifyProcedureDefaultsChange( changed_proc, nullptr, Apparance::InvalidID );

	//need to invalidate all details panels showing procedures of this type
#if 1
	//force a full refresh of all panels
	//HACK: This is done because there is no way to enumerate views, you can only FindDetailView, and then only if you know the identifier it goes by!
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>( "PropertyEditor" );
	EditModule.NotifyCustomizationModuleChanged(); //not what this is meant for, but it does do a full refresh
#else

	//scan all detail panels
	static const FName DetailsTabIdentifiers[] = { "LevelEditorSelectionDetails", "LevelEditorSelectionDetails2", "LevelEditorSelectionDetails3", "LevelEditorSelectionDetails4" };
	for(const FName& DetailsTabIdentifier : DetailsTabIdentifiers)
	{
		FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		TSharedPtr<IDetailsView> DetailsView = EditModule.FindDetailView( DetailsTabIdentifier );
		if(DetailsView.IsValid())
		{
			//scan the objects being inspected
			TArray<TWeakObjectPtr<UObject>> objects = DetailsView->GetSelectedObjects();
			for(int i=0 ; i<objects.Num() ;i++)
			{
				//is it looking at an entity?
				IProceduralObject* pentity = Cast<IProceduralObject>( objects[i] );
				if(pentity)
				{
					//is that entity using the changed procedure?
					//NOTE: It might not have actually changed, but it's probably the easiest way to be sure
					if(pentity->IPO_GetProcedureID()==changed_proc)
					{
						//yes!, refresh to reflect changes
						DetailsView->ForceRefresh();
					}
				}
			}
		}
	}
#endif
}


// trigger re-generation of all active procedural content
//
void FApparanceUnrealEditorModule::RebuildAllEntities( const UActorComponent* optional_only_this_component_affected )
{
	//database change invalidates generated state of all actors
	if (GEditor) //(can be triggered during shutdown)
	{
		UWorld* pworld = GEditor->GetEditorWorldContext().World();
		if (pworld)
		{
			for (TActorIterator<AApparanceEntity> aeit(pworld); aeit; ++aeit)
			{
				//check filtering
				bool rebuild = true;
				if (optional_only_this_component_affected)
				{
					aeit->HasComponentOfType(optional_only_this_component_affected->GetClass());
				}

				//apply?
				if (rebuild)
				{
					aeit->RebuildDeferred();
				}
			}
		}
	}
}


// trigger re-generation of affected entities
//
void FApparanceUnrealEditorModule::RebuildAffectedEntities(Apparance::ProcedureID proc_id, Apparance::ValueID changed_param)
{
	bool due_to_spec_change = true;	//for now always the case

	//default changes invalidates generated state of actors using this proc and this default parameter actors
	if (GEditor) //(can be triggered during shutdown)
	{
		UWorld* pworld = GEditor->GetEditorWorldContext().World();
		if (pworld)
		{
			for (TActorIterator<AApparanceEntity> aeit(pworld); aeit; ++aeit)
			{
				//only affected if this proc 
				//and this param is going to use the changed default
				//(or none specified in which case assume all)
				if (aeit->ProcedureID==proc_id 
					&& (changed_param==Apparance::InvalidID || aeit->IsParameterDefault(changed_param)))
				{
					if(due_to_spec_change)
					{
						aeit->NotifyProcedureSpecChanged();
					}
					aeit->RebuildDeferred();
				}
			}
		}
	}
}


// query whether game is running (would only be false in editor, outside of sim/pie)
bool FApparanceUnrealEditorModule::IsGameRunning() const
{
	return GEditor->IsPlaySessionInProgress();	
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FApparanceUnrealEditorModule, ApparanceUnrealEditor)
