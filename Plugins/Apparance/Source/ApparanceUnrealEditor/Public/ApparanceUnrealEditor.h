//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#pragma once

//unreal
#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Styling/SlateStyle.h"
#include "IPlacementModeModule.h"

//unreal version differences
#include "ApparanceUnrealVersioning.h"
#if UE_VERSION_AT_LEAST(5,0,0)
#include "UObject/ObjectSaveContext.h"
#endif
#if UE_VERSION_AT_LEAST(5,1,0)
# include "EditorStyleSet.h"
# define APPARANCE_STYLE FAppStyle::Get()
#else
# define APPARANCE_STYLE FEditorStyle::Get()
#endif

//module
#include "ApparanceUnrealEditorAPI.h"
#include "ApparanceAbout.h"

//apparance
#include "Apparance.h"

// Apparance editor module
//
class FApparanceUnrealEditorModule 
	: public IModuleInterface
	, public IApparanceUnrealEditorAPI
	, public IHasMenuExtensibility
	, public IHasToolBarExtensibility
{
	// tick management
	FTickerDelegate TickDelegate;
	TICKER_HANDLE TickDelegateHandle;
	bool bLateInitHappened;
	
	/** Extensibility managers */
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;

	// tracking
	TArray<class AApparanceEntity*> SelectedEntities;
	bool bPlaySessionActive;
	float ReleaseNotesTimer;

	// asset appearance
	TSharedPtr<FSlateStyleSet> AssetStyleSet;
	TOptional<FPlacementModeID> EntityPlacementHandle_Basic;
	TOptional<FPlacementModeID> EntityPlacementHandle_AllClasses;

	// menus/windows
	TSharedPtr<FExtender> MenuExtension;
	TSharedPtr<SWindow> AboutWindow;
	TSharedPtr<SApparanceAbout> AboutPanel;

	// systems
	class FApparanceUnrealModule* MainModule;
	static class FApparanceUnrealEditorModule* EditorModule;

	// workspace
	UWorld* WorkspaceWorld;

	// external editor installation
	TSharedPtr<FScopedSlowTask> InstallingProgress;
	int ProgressPercentage;
	FText CurrentProgressMessage;
	Apparance::ProcedureID PendingProcedure;

	// interactive editing
	bool bWasInGameViewBeforeInteractiveEditingBegan;

public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/* IApparanceUnrealEditorAPI implementation */
	virtual void NotifyAssetDatabaseChanged() override;
	virtual void NotifyExternalContentChanged() override;
	virtual void NotifyResourceListStructureChanged( UApparanceResourceList* pres_list, UApparanceResourceListEntry* pentry, FName property ) override;
	virtual void NotifyResourceListAssetChanged( UApparanceResourceList* pres_list, UApparanceResourceListEntry* pentry, UObject* pnewasset ) override;
	virtual void NotifyProcedureDefaultsChange(Apparance::ProcedureID proc_id, const Apparance::IParameterCollection* params, Apparance::ValueID changed_param) override;
	virtual void NotifyEntityProcedureTypeChanged(Apparance::ProcedureID new_proc_id) override;
	virtual UWorld* GetTempWorld() override;
	virtual bool IsGameRunning() const override;
	virtual Apparance::IParameterCollection* BeginParameterEdit( class IProceduralObject* pentity, bool editing_transaction ) override;
	virtual void EndParameterEdit( class IProceduralObject* pentity, bool editing_transaction, Apparance::ValueID input_id ) override;
	virtual FViewport* FindEditorViewport() const override;

	/** IHasMenuExtensibility interface */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() override { return MenuExtensibilityManager; }
	
	/** IHasToolBarExtensibility interface */
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() override { return ToolBarExtensibilityManager; }	

	// systems access
	static class FApparanceUnrealEditorModule* GetModule() { return EditorModule; }
	static Apparance::IEditor* GetEditor();
	UFactory* GetResourceListFactory();

	// event handlers
	bool Tick(float DeltaTime);
	void OnEditorSelectionChanged(UObject* NewSelection);
	void OnPackageSave( UPackage* Package
#if UE_VERSION_AT_LEAST(5,0,0)
			, FObjectPreSaveContext ObjectSaveContext
#endif
		);

	void OnPlacementModeRefresh( FName CategoryName );
	void OnObjectPropertyEdited(UObject* InObject, FPropertyChangedEvent& InPropertyChangedEvent);
		
	// events
	DECLARE_EVENT_ThreeParams(FApparanceUnrealEditorModule, FOnResourceListStructuralChange, class UApparanceResourceList*, class UApparanceResourceListEntry*, FName );	
	FOnResourceListStructuralChange ResourceListStructuralChangeEvent;
	DECLARE_EVENT_ThreeParams( FApparanceUnrealEditorModule, FOnResourceListAssetChange, class UApparanceResourceList*, class UApparanceResourceListEntry*, UObject* );
	FOnResourceListAssetChange ResourceListAssetChangeEvent;

	//style
	static const ISlateStyle* GetStyle() { return GetModule()->AssetStyleSet.Get(); }

	//external editor operations
	void OpenProcedure( Apparance::ProcedureID proc_id );	//open a procedure in the editor

private:
	// sub-system setup/shutdown
	void LateInit();
	void InitAssets();
	void ShutdownAssets();
	void InitExtensibility();
	void ShutdownExtensibility();
	void InitMenus();
	void ShutdownMenus();
	void InitUI();
	void ShutdownUI();
	void InitWorkspace();
	void ShutdownWorkspace();
	void InitEditorInterop();
	void ShutdownEditorInterop();
	void InitPlacement();
	void ShutdownPlacement();
	void InitSettings();
	void ShutdownSettings();
	void InitEditingModes();
	void ShutdownEditingModes();

	//setup helpers
	void RegisterGeneralIcon( FString icon_path, FName name );
	void CopyEditorIcon( FName source_name, FName name );

	//menu setup
	void AddPullDownMenu(FMenuBarBuilder& MenuBuilder);
	void FillMenu(FMenuBuilder& MenuBuilder);
	
	//menu actions
	void LaunchApparanceEditor( Apparance::ProcedureID proc_id=0 );
	bool CanLaunchApparanceEditor() const;
	void OnOpenAboutWindow( EApparanceAboutPage page=EApparanceAboutPage::About );
	void OnOpenURL(const TCHAR* url);
	void OnOpenProjectSettings();
		
	//editor installation
	bool IsInstalling() const { return InstallingProgress.IsValid(); }
	void TickInstallation();
	bool CheckForVersionChange();
	
	//changes
	void CheckPlayInEditor();
	void OnPlayInEditorStarting();
	void OnPlayInEditorStopped();
	void CheckExternalChanges();
	void OnProcedureExternalChange( Apparance::ProcedureID changed_proc );
	void NotifyProceduresDirty( bool are_dirty );
	void SaveProcedures();

	//rebuild
	void RebuildAllEntities( const UActorComponent* optional_only_this_component_affected=nullptr );
	void RebuildAffectedEntities(Apparance::ProcedureID proc_id, Apparance::ValueID changed_param);
	
	//misc
	void OnAboutWindowClosed( const TSharedRef<SWindow>& window );

};
