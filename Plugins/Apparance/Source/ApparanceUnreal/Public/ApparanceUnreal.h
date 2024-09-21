//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#pragma once

//module
#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "ApparanceUnrealVersioning.h"

//unreal
#include "Containers/Ticker.h"

//Apparance API
#include "Apparance.h"

// general declarations
DECLARE_LOG_CATEGORY_EXTERN(LogApparance, Log, All);

// Apparance runtime module
//
class APPARANCEUNREAL_API FApparanceUnrealModule 
	: public IModuleInterface
{
	//globals
	static FApparanceUnrealModule* m_pModule;

	// systems
	Apparance::IEngine*    m_pApparance;
	struct FAssetDatabase* m_pAssetDatabase;
	struct IApparanceUnrealEditorAPI* m_pEditorModule;
	
	// tick management
	FTickerDelegate TickDelegate;
	TICKER_HANDLE TickDelegateHandle;

	// state
	bool                   m_bApparanceEngineDeferredStart;

	// library cache
	TArray<const Apparance::ProcedureSpec*> m_ProcedureInventory;
	bool m_bInventoryValid;

	//config
	FString ProceduresPath;
	class UApparanceResourceList* ResourceRoot;

	//editing
	TMap<class UWorld* , class USmartEditingState*> m_EditingStates; //all active editing states and the world they are associated with

public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// setup
	void StartupModuleDeferred();
	void SetEditorModule( IApparanceUnrealEditorAPI* peditor_module ) { m_pEditorModule = peditor_module; }
	
	// systems
	static FApparanceUnrealModule* GetModule() { return m_pModule; }
	static Apparance::IEngine* GetEngine() { return m_pModule->m_pApparance; }
	static Apparance::ILibrary* GetLibrary() { return (m_pModule && m_pModule->m_pApparance)? m_pModule->m_pApparance->GetLibrary():nullptr; }
	static struct FAssetDatabase* GetAssetDatabase() { return m_pModule->m_pAssetDatabase; }
	
	// access
	bool IsLiveEditingEnabled() const;
	FString GetProceduresPath() const { return ProceduresPath; }
	bool IsGameRunning() const;
	class UApparanceResourceList* GetResourceRoot() { return ResourceRoot; }
	const TArray<const Apparance::ProcedureSpec*>& GetInventory();
	const FText GetProductText() const;
	
	// asset info
	class UMaterial* GetFallbackMaterial();
	class UTexture* GetFallbackTexture();
	class UStaticMesh* GetFallbackMesh();
	const TArray<FString> GetMissingAssets();

	// tick handler
	bool Tick( float DeltaTime );

	// editor notifications/access
	void 	NotifyExternalContentChanged();	//allowed to be called from runtime, but ignored
	void	Editor_NotifyProcedureDefaultsChange( Apparance::ProcedureID proc_id, const Apparance::IParameterCollection* params, Apparance::ValueID changed_param );
	void	Editor_NotifyEntityProcedureTypeChanged( Apparance::ProcedureID new_proc_id );
	void 	Editor_NotifyAssetDatabaseChanged();
	void 	Editor_NotifyResourceListStructureChanged( class UApparanceResourceList* pres_list, class UApparanceResourceListEntry* pentry, FName property );
	void	Editor_NotifyResourceListAssetChanged( UApparanceResourceList* pres_list, UApparanceResourceListEntry* pentry, UObject* pnewasset );
	void	Editor_NotifyPlayStopped();
	UWorld* Editor_GetTempWorld();
	FViewport* Editor_FindViewport() const;
	bool    Editor_UpdateInteraction( UWorld* world, FVector cursor_dir, bool bInteractionButton/*, iModifierKeyFlags */ );

	// entity parameter editing (editor will want to be involved)
	Apparance::IParameterCollection* BeginEntityParameterEdit( class IProceduralObject* pentity, bool editing_transaction );
	void    EndEntityParameterEdit( class IProceduralObject* pentity, bool editing_transaction, Apparance::ValueID input_id );

	// editing (smart objects/handles)
	class USmartEditingState* IsEditingEnabled( class UWorld* world ) const;
	class USmartEditingState* EnableEditing( class UWorld* world, bool enable, bool force=false );
	
	//helpers
	static FString MakeWorldIdentifier( AActor* pactor );
	static FString MakeWorldIdentifier( UWorld* pworld );

private:

	//init
	bool InitTimers();
	bool InitEngine();
	bool InitSynthesis();
	bool InitAssetDatabase();
	void InitPlacement();
	void InitProductInfo();
	
	// editing (smart objects/handles) state management
	USmartEditingState* CreateSmartEditingState( UWorld* world );
	void DestroySmartEditingState( USmartEditingState* state );
	void HandleRemoveWorld( class UWorld* world );

};
