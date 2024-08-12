//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

//main
#include "ResourceListEditor.h"
#include "ApparanceUnrealVersioning.h"

//unreal
//#include "Core.h"
#include "Editor.h"
//unreal:ui
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Layout/SScrollBox.h"
#include "WorkflowOrientedApp/SContentReference.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "SAssetDropTarget.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#if UE_VERSION_AT_LEAST(5,1,0)
#include "Misc/TransactionObjectEvent.h"
#endif
//module
#include "ApparanceUnreal.h"
#include "ApparanceResourceList.h"
#include "ApparanceUnrealEditor.h"
#include "Editing/ResourceListEntryProperties.h"

#define LOCTEXT_NAMESPACE "ApparanceUnreal"


const FName FResourceListEditor::ResourceListEditorAppIdentifier( TEXT( "ApparanceResourceListEditorApp" ) );
const FName FResourceListEditor::ResourceListHierarchyTabId("ApparanceResourceListEditor_Hierarchy");
const FName FResourceListEditor::ResourceListDetailsTabId("ApparanceResourceListEditor_Details");




//////////////////////////////////////////////////////////////////////////
// FResourceListEditorTreeItem

FString FResourceListEditorTreeItem::GetName() const
{
	if(!NameOverride.IsEmpty())
	{
		return NameOverride;
	}
	else
	{
		UApparanceResourceListEntry* pentry = Resource.Get();
		if(pentry)
		{
			return pentry->GetName();
		}
		
		return TEXT("New resource");
	}
}

FText FResourceListEditorTreeItem::GetAssetTypeName() const
{
	if(IsMissingAsset())
	{
		if(Parent)
		{
			return LOCTEXT("AssetNotFoundTypeColumnText","Missing");
		}
		else
		{
			return FText();	//don't describe type of missing asset root entry
		}
	}
	else
	{
		UApparanceResourceListEntry_Asset* pentry = Cast<UApparanceResourceListEntry_Asset>( Resource );
		if(pentry)
		{
			return pentry->GetAssetTypeName();
		}
		
		//only show anything for assets
		return FText::GetEmpty();
	}
}

const FSlateBrush* FResourceListEditorTreeItem::GetIcon( bool is_open ) const
{
	if(IsMissingAsset())
	{
		//return APPARANCE_STYLE.GetBrush("LevelEditor.Tabs.WorldBrowserDetails");
		return APPARANCE_STYLE.GetBrush("Icons.Warning");
	}
	else if(!NameOverride.IsEmpty())
	{
		//special for missing root
		return APPARANCE_STYLE.GetBrush("SceneOutliner.FolderClosed");
	}
	else
	{
		UApparanceResourceListEntry* pentry = Resource.Get();
		if(pentry)
		{
			FName icon_name;
			//ask resource for icon to use
			icon_name = pentry->GetIconName( is_open );
	
			//resolve
			const FSlateBrush* picon = APPARANCE_STYLE.GetBrush( icon_name );
			if(picon)
			{
				return picon;
			}
		}
	
		return APPARANCE_STYLE.GetBrush("ClassIcon.BoxComponent");
	}
}

bool FResourceListEditorTreeItem::HasVariants() const
{
	return Resource.IsValid() && Resource->IsA( UApparanceResourceListEntry_Variants::StaticClass() );
}

int FResourceListEditorTreeItem::GetVariantCount() const
{
	UApparanceResourceListEntry_Variants* pvariants = Cast<UApparanceResourceListEntry_Variants>( Resource );
	if(pvariants)
	{
		return pvariants->GetVariantCount();
	}

	//none or not an asset
	return 0;
}

bool FResourceListEditorTreeItem::IsAsset() const
{
	return Resource.IsValid() && Resource->IsA( UApparanceResourceListEntry_Asset::StaticClass() );
}

bool FResourceListEditorTreeItem::IsResourceList() const
{
	return Resource.IsValid() && Resource->IsA( UApparanceResourceListEntry_ResourceList::StaticClass() );
}

UClass* FResourceListEditorTreeItem::GetResourceEntryType() const
{
	if(Resource.IsValid())
	{
		return Resource->GetClass();
	}
	return nullptr;
}

UClass* FResourceListEditorTreeItem::GetAssetType() const
{
	UApparanceResourceListEntry_Asset* pentry = Cast<UApparanceResourceListEntry_Asset>( Resource );
	if(pentry)
	{
		return pentry->GetAssetClass();
	}
	//none or not an asset
	return nullptr;
}

// search the treeview hierarchy for the item acting for a specific asset (or variant thereof)
//
FResourceListEditorTreeItem::Ptr FResourceListEditorTreeItem::FindEntry( UObject* asset_or_entry )
{
	//check entry
	if(Resource.Get()==asset_or_entry) 
	{
		return AsShared();
	}

	//check for asset
	if(Resource.IsValid())
	{
		UApparanceResourceListEntry_Asset* pentry = Cast<UApparanceResourceListEntry_Asset>( Resource.Get() );
		if(pentry && pentry->GetAsset()==asset_or_entry)
		{
			return AsShared();
		}
	}

	//check children
	for(int i=0 ;i<Children.Num(); i++)
	{
		FResourceListEditorTreeItem::Ptr found = Children[i]->FindEntry( asset_or_entry );
		if(found.IsValid())
		{
			return found;
		}
	}

	return FResourceListEditorTreeItem::Ptr();
}

// is this a reference to other resource lists?
//
bool FResourceListEditorTreeItem::IsReference() const
{
	return Resource->IsA( UApparanceResourceListEntry_ResourceList::StaticClass() );
}

UObject* FResourceListEditorTreeItem::GetResource()
{
	UApparanceResourceListEntry_Asset* pentry = Cast<UApparanceResourceListEntry_Asset>( Resource );
	if(pentry)
	{
		return pentry->GetAsset();
	}
	return nullptr;
}

// assign an asset to a treeview item, may require convertion of resource list entry type
//
bool FResourceListEditorTreeItem::SetResource( UObject* passet )
{
	//do we need type conversion?
	UClass* source_type = passet?passet->GetClass():nullptr;
	UClass* target_type = GetAssetType();
	if(source_type!=nullptr && source_type!=target_type)
	{
		//convert type to support newly assigned asset type
		UApparanceResourceListEntry* parent_entry = Parent->Resource.Get();
		int tree_parent_index = TreeParentIndex;
		UApparanceResourceListEntry* pold_entry = Resource.Get();

		//what type of entry?
		const UClass* entry_type = FResourceListEditor::GetEntryTypeForAsset( source_type );
		if (entry_type) //discard unsupported asset types (can't filter on a sub-set of types in resource picker unfortunately)
		{
			UApparanceResourceListEntry_Asset* pnew_entry = Cast< UApparanceResourceListEntry_Asset>( Editor->CreateEntry( entry_type ) );
															//NewObject<UApparanceResourceListEntry_Asset>(parent_entry, entry_type, NAME_None, RF_Transactional);

			//transfer old state, apply new state
			pnew_entry->Editor_AssignFrom(pold_entry);
			pnew_entry->SetAsset(passet);

			//item needs to refer to new entry (rather than trigger hierarchy rebuild)
			Resource = pnew_entry;

			//need to know index of item in resource list (not same as IndexInParent which is for the tree items which are sorted)
			int resource_parent_index = 0;
			if(ensure( parent_entry->Children.Find( pold_entry, resource_parent_index ) ))
			{

				//undoable transaction
				{
					FScopedTransaction transaction( LOCTEXT( "AssignResourceListAsset", "Assign Resource List Asset" ) );

					//save pre-edit state
					parent_entry->Modify();

					//apply change
					parent_entry->Children[resource_parent_index] = pnew_entry;
				}
				//transaction complete
				return true;
			}
		}
		else
		{
			//report
			UE_LOG(LogApparance, Error, TEXT("Unsupported asset type '%s' assigned to resource list"), *source_type->GetFName().ToString() );
		}
	}
	else
	{
		//just assign new asset (of same type)
		UApparanceResourceListEntry_Asset* pentry = Cast<UApparanceResourceListEntry_Asset>( Resource );
		if(pentry)
		{
			//undoable transaction
			{
				FScopedTransaction transaction( LOCTEXT("AssignResourceListAsset","Assign Resource List Asset"));
				
				//save pre-edit state
				pentry->Modify();				
				
				//apply change
				pentry->SetAsset( passet );				
			}
			//transaction complete
			return true;
		}
	}

	//no transaction
	return false;
}

// change the type of resource entry
//
void FResourceListEditorTreeItem::ChangeEntryType( const UClass* resource_list_entry_type )
{
	//convert type to support newly assigned asset type
	UApparanceResourceListEntry* parent_entry = Parent->Resource.Get();
	int tree_parent_index = TreeParentIndex;
	UApparanceResourceListEntry* pold_entry = Resource.Get();
	
	UApparanceResourceListEntry_Asset* pnew_entry = Cast< UApparanceResourceListEntry_Asset>( Editor->CreateEntry( resource_list_entry_type ) );
													//NewObject<UApparanceResourceListEntry_Asset>(parent_entry, resource_list_entry_type, NAME_None, RF_Transactional);
		
	//transfer old state, apply new state
	pnew_entry->Editor_AssignFrom(pold_entry);
		
	//item needs to refer to new entry (rather than trigger hierarchy rebuild)
	Resource = pnew_entry;
	int resource_parent_index = 0;
	if(ensure( parent_entry->Children.Find( pold_entry, resource_parent_index ) ))
	{
		//undoable transaction
		{
			FScopedTransaction transaction( LOCTEXT( "AssignResourceListAsset", "Assign Resource List Asset" ) );

			//save pre-edit state
			parent_entry->Modify();

			//apply change
			parent_entry->Children[resource_parent_index] = pnew_entry;
		}
	}
}

// rename this entry
//
void FResourceListEditorTreeItem::RenameEntry( FString new_name )
{
	//undoable transaction
	{
		FScopedTransaction transaction( LOCTEXT("RenameResourceListEntry","Rename Resource List Entry"));
		
		//save pre-edit state
		Resource->Modify();
		
		//apply change
		Resource->SetName( new_name );
	}

	//rename affects caching
	Resource->Editor_NotifyCacheInvalidatingChange();	//need to manually invalidate since we are externally manipulating the entry	
	//rename can affect order	
	if(Parent)
	{
		Parent->Sort();		
	}
}


// swap an entry between single resource and variant list
//
void FResourceListEditorTreeItem::ToggleVariants()
{
	UApparanceResourceListEntry* parent_entry = Parent->Resource.Get();
	int tree_parent_index = TreeParentIndex;
	UApparanceResourceListEntry* pold_entry = Resource.Get();
	int resource_parent_index = 0;
	if(!ensure( parent_entry->Children.Find( pold_entry, resource_parent_index ) ))
	{
		return;
	}

	if(HasVariants())
	{
		//variant -> single
		UApparanceResourceListEntry_Variants* pold_variant_container = Cast<UApparanceResourceListEntry_Variants>( pold_entry );
		check(pold_variant_container);
		check(pold_variant_container->Children.Num()>=1);//assumption of being called is that we have at least one variant asset
		UApparanceResourceListEntry_Asset* pfirst_variant = Cast<UApparanceResourceListEntry_Asset>( pold_variant_container->Children[0] );
		check(pfirst_variant);

		//undoable transaction
		{
			FScopedTransaction transaction( LOCTEXT("AssignResourceListAsset","Convert Variant To Asset"));
			
			//save pre-edit state
			parent_entry->Modify();
			
			//apply change
			parent_entry->Children[resource_parent_index] = pfirst_variant;	//swap in our new resource	
			pfirst_variant->SetName( pold_entry->GetName() );			//transfer name
			
			//keep discarded sibling variants (in single asset)
			pfirst_variant->OldVariantSiblings.Empty();
			for(int i=1/*first used as main asset*/ ; i<pold_variant_container->Children.Num() ; i++)
			{
				pfirst_variant->OldVariantSiblings.Add( pold_variant_container->Children[i] );
			}
		}	
		parent_entry->Editor_NotifyCacheInvalidatingChange();
	}
	else if(IsAsset() && !IsVariant())
	{
		//single -> variant
		UApparanceResourceListEntry_Variants* pvariants = Cast< UApparanceResourceListEntry_Variants>( Editor->CreateEntry( UApparanceResourceListEntry_Variants::StaticClass() ) );
															//NewObject<UApparanceResourceListEntry_Variants>( parent_entry, UApparanceResourceListEntry_Variants::StaticClass(), NAME_None, RF_Transactional );
		UApparanceResourceListEntry_Asset* pold_asset = Cast<UApparanceResourceListEntry_Asset>( pold_entry );
		check(pold_asset);

		//undoable transaction
		{
			FScopedTransaction transaction( LOCTEXT("AssignResourceListAsset","Convert Asset To Variant"));

			//save pre-edit state
			parent_entry->Modify();
			
			//apply change
			parent_entry->Children[resource_parent_index] = pvariants; 	//swap in our new resource
			pvariants->Children.Add( pold_entry );					//original asset becomes variant
			pvariants->SetName( pold_entry->GetName() );			//transfer name

			//restore variant children (stored in single asset)
			for(int i=0 ; i<pold_asset->OldVariantSiblings.Num() ; i++)
			{
				pvariants->Children.Add( pold_asset->OldVariantSiblings[i] );
			}
			pold_asset->OldVariantSiblings.Empty();
		}		
		parent_entry->Editor_NotifyCacheInvalidatingChange();
	}
	else
	{
		//shouldn't have been called, can't toggle this
		check(false);
	}
}

// update our whole hierarchy to match the provided resource list
//
void FResourceListEditorTreeItem::Sync(UApparanceResourceListEntry* proot )
{
	Sync( proot, nullptr, 0 );
}

// update our sub-structure to match the provided resource list entry sub-structure
void FResourceListEditorTreeItem::Sync(UApparanceResourceListEntry* pentry, FResourceListEditorTreeItem::Ptr parent, int tree_parent_index )
{
	//ensure we are in sync (no need to check+update, just set)
	Resource = pentry;
	Parent = parent.Get();
	TreeParentIndex = tree_parent_index;

	//source is normal entry hierarchy
	int num_children = Resource->Children.Num();
	TArray<UApparanceResourceListEntry*> ordered_resource_entries( Resource->Children );
	//variants are implicitly numbered, don't try and sort
	if(!HasVariants())
	{
		Sort( ordered_resource_entries );
	}

	//sync children to source
	Children.SetNum( num_children, false );
	for(int i=0 ; i<num_children ; i++)
	{
		if(!Children[i].IsValid())
		{
			Children[i] = MakeShareable( new FResourceListEditorTreeItem( Editor ) );
		}
		Children[i]->Sync( ordered_resource_entries[i], AsShared(), i ); //sub-entries
	}
}

// update the missing assets section of the hierarchy
// shows any assets that were requested by procedures but not found, allows easy promotion to resource list entries
//
void FResourceListEditorTreeItem::SyncMissing( const TArray<FString> missing_asset_descriptors, FString primary_category )
{
	//prep
	FString primary_prefix = primary_category + TEXT(".");
	int num_missing = missing_asset_descriptors.Num();
	Children.SetNum( num_missing, false );

	//root appearance
	NameOverride = FText::Format( LOCTEXT("MissingAssetRootFolderName", "Unresolved({0})"), num_missing ).ToString();
	bMissingAssetEntry = true; //root
	bMissingAssetIsCompatible = num_missing>0; //dim when none missing
	
	//ones for us first, then 'other'
	int tree_parent_index = 0;
	for(int pass=0 ; pass<2 ; pass++)
	{
		bool want_primary = pass==0;

		//process descriptors
		for(int i=0 ; i<missing_asset_descriptors.Num() ; i++)
		{
			//filter
			bool is_primary = missing_asset_descriptors[i].StartsWith( primary_prefix );
			if(is_primary==want_primary)
			{
				FResourceListEditorTreeItem::Ptr item = MakeShareable( new FResourceListEditorTreeItem( Editor ) );
				Children[i] = item;
				//unused elements
				item->Resource = nullptr;
				item->Parent = this;
				//used elements
				item->TreeParentIndex = tree_parent_index++;
				item->bMissingAssetEntry = true;
				item->NameOverride = missing_asset_descriptors[i];
				item->bMissingAssetIsCompatible = is_primary;
			}
		}
	}
	Sort();
}

// sort our child list alphabettically
//
void FResourceListEditorTreeItem::Sort()
{
	//variants are implicitly numbered, don't try and sort
	if(HasVariants())
	{
		return;
	}

	// lexical order comparison functor
	static struct
	{
		FORCEINLINE bool operator()(const FResourceListEditorTreeItem::Ptr A, const FResourceListEditorTreeItem::Ptr B) const
		{
			return A->GetName().Compare( B->GetName() ) < 0;
		}
	} lexical_compare;

	// perform lexical sort
	Children.Sort( lexical_compare );
}

// sort a list of entries alphabettically
//
void FResourceListEditorTreeItem::Sort( TArray<UApparanceResourceListEntry*>& entries )
{
	// lexical order comparison functor
	static struct
	{
		FORCEINLINE bool operator()( const UApparanceResourceListEntry& A, const UApparanceResourceListEntry& B ) const
		{
			return A.GetName().Compare( B.GetName() ) < 0;
		}
	} lexical_compare;

	// perform lexical sort
	entries.Sort( lexical_compare );
}



//////////////////////////////////////////////////////////////////////////
// FResourceListEditor

// raw init, maybe a better place for editor opening
//
FResourceListEditor::FResourceListEditor()
{
	// hook apparance editor events
	FApparanceUnrealEditorModule::GetModule()->ResourceListStructuralChangeEvent.AddRaw( this, &FResourceListEditor::OnResourceListStructuralChange );
	FApparanceUnrealEditorModule::GetModule()->ResourceListAssetChangeEvent.AddRaw( this, &FResourceListEditor::OnResourceListAssetChange );
	
	// we want to be updated regularly
	TickDelegate = FTickerDelegate::CreateRaw(this, &FResourceListEditor::Tick);
	TickDelegateHandle = TICKER_TYPE::GetCoreTicker().AddTicker(TickDelegate);
}

// main setup
//
void FResourceListEditor::InitResourceListEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UApparanceResourceList* res_list )
{
	//initial layout?
	TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout( "Standalone_CompositeResourceListEditor_new3_Layout" )
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation( Orient_Vertical )
#if UE_VERSION_OLDER_THAN(5,0,0) //used to need to manually add toolbar
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient( 0.1f )
				->SetHideTabWell( true )
				->AddTab( GetToolbarTabId(), ETabState::OpenedTab )
			)
#endif
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation( Orient_Horizontal )
				->Split
				(
					FTabManager::NewStack()
					->AddTab( ResourceListHierarchyTabId, ETabState::OpenedTab )
					->SetForegroundTab( ResourceListHierarchyTabId )
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient( 0.4f )
					->SetHideTabWell( true )
					->AddTab( ResourceListDetailsTabId, ETabState::OpenedTab )
				)
			)
		);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor( Mode, InitToolkitHost, ResourceListEditorAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, res_list );

	// menu customisation
//	FDataTableEditorModule& DataTableEditorModule = FModuleManager::LoadModuleChecked<FDataTableEditorModule>("DataTableEditor");
//	AddMenuExtender(DataTableEditorModule.GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	// toolbar customisation
	TSharedPtr<FExtender> ToolbarExtender = FApparanceUnrealEditorModule::GetModule()->GetToolBarExtensibilityManager()->GetAllExtenders( GetToolkitCommands(), GetEditingObjects() );
	ExtendToolbar( ToolbarExtender );
	AddToolbarExtender( ToolbarExtender );
	RegenerateMenusAndToolbars();

	// Support undo/redo
	GEditor->RegisterForUndo( this );

	// ensure resource list set up for editing (e.g. if brand new)
	res_list->Editor_InitForEditing();

	// prep for UI
	RebuildEntryViewModel();

	if(DetailsView.IsValid())
	{
		//initially nothing
		DetailsView->SetObject( nullptr );
	}
}

// raw shutdown, maybe a better place for editor closing
//
FResourceListEditor::~FResourceListEditor()
{
	//shut down timers
	TICKER_TYPE::GetCoreTicker().RemoveTicker(TickDelegateHandle);
	TickDelegateHandle.Reset();
	TickDelegate = nullptr;

	//done with events
	FApparanceUnrealEditorModule::GetModule()->ResourceListStructuralChangeEvent.RemoveAll( this );
	FApparanceUnrealEditorModule::GetModule()->ResourceListAssetChangeEvent.RemoveAll( this );

	//undo system
	GEditor->UnregisterForUndo( this );
}

// target object access
//
UApparanceResourceList* FResourceListEditor::GetEditableResourceList() const
{
	return Cast<UApparanceResourceList>( GetEditingObject() );
}

// search the hierarchy for the item wrapping a particular resource list entry
//
FResourceListEditorTreeItem::Ptr FResourceListEditor::FindHierarchyEntry( UObject* entry_or_asset )
{
	for(int i=0 ; i<TreeRoot.Num() ; i++)
	{
		FResourceListEditorTreeItem::Ptr found = TreeRoot[i]->FindEntry( entry_or_asset );
		if(found.IsValid())
		{
			return found;
		}
	}
	return FResourceListEditorTreeItem::Ptr();
}



// opportunity to add buttons to our toolbar
//
void FResourceListEditor::ExtendToolbar(TSharedPtr<FExtender> Extender)
{
	Extender->AddToolBarExtension(
		"Asset", EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateSP(this, &FResourceListEditor::FillToolbar)
	);	
}

// populate our toolbar buttons
//
void FResourceListEditor::FillToolbar(FToolBarBuilder& ToolbarBuilder)
{
	ToolbarBuilder.BeginSection("ResourceListCommands");
	{
		ToolbarBuilder.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateSP(this, &FResourceListEditor::OnAddFolderClicked),
				FCanExecuteAction::CreateSP(this, &FResourceListEditor::CheckAddFolderAllowed)),
			NAME_None,
			LOCTEXT("AddResourceFolderText", "Folder"),
			LOCTEXT("AddResourceFolderToolTip", "Add a new folder to the resource list"),
			FSlateIcon( APPARANCE_STYLE.GetStyleSetName(), "CurveEditor.CreateTab"));
		ToolbarBuilder.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateSP(this, &FResourceListEditor::OnAddResourceClicked),
				FCanExecuteAction::CreateSP(this, &FResourceListEditor::CheckAddResourceAllowed)),
			NAME_None,
			LOCTEXT("AddResourceEntryText", "Resource"),
			LOCTEXT("AddResourceEntryToolTip", "Add a new resource entry to the resource list"),
			FSlateIcon( APPARANCE_STYLE.GetStyleSetName(), "DataTableEditor.Add"));
		ToolbarBuilder.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateSP(this, &FResourceListEditor::OnRemoveItemClicked),
				FCanExecuteAction::CreateSP(this, &FResourceListEditor::CheckRemoveItemAllowed)),
			NAME_None,
			LOCTEXT("RemoveResourceEntryText", "Remove"),
			LOCTEXT("RemoveResourceEntryToolTip", "Remove the currently selected resource entry from the resource list"),
			FSlateIcon( APPARANCE_STYLE.GetStyleSetName(), "DataTableEditor.Remove"));
		ToolbarBuilder.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateSP(this, &FResourceListEditor::OnToggleVariantsClicked),
				FCanExecuteAction::CreateSP(this, &FResourceListEditor::CheckToggleVariantsAllowed)),
			NAME_None,
			LOCTEXT("VariantsToggleResourceEntryText", "Variants?"),
			LOCTEXT("VariantsToggleResourceEntryToolTip", "Toggle between single resource and list of variants"),
			FSlateIcon( APPARANCE_STYLE.GetStyleSetName(), "LevelEditor.OpenContentBrowser"));
			//alt icon?: Matinee.PlayLoop
		ToolbarBuilder.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateSP(this, &FResourceListEditor::OnToggleHelpClicked),
				FCanExecuteAction::CreateSP(this, &FResourceListEditor::CheckToggleHelpAllowed)),
			NAME_None,
			LOCTEXT("HelpResourceEntryText", "Help?"),
			LOCTEXT("HelpResourceEntryToolTip", "Toggle inline help information on the details panel"),
			FSlateIcon("Apparance","Apparance.Icon.Info"));
	}
	ToolbarBuilder.EndSection();	
}

// regular updates
// NOTE: used to handle deferred details panel change
//
bool FResourceListEditor::Tick(float DeltaTime)
{
	//deferred selection (by entry)
	if(DeferredSelectionEntry.IsValid())
	{
		//find and apply
		FResourceListEditorTreeItem::Ptr found = FindHierarchyEntry( DeferredSelectionEntry.Get() );
		if(found.IsValid())
		{
			SetSelection( found );
		}

		//done
		DeferredSelectionEntry.Reset();
	}
	//deferred selection (by item)
	if(DeferredSelectionItem.IsValid())
	{
		SetSelection( DeferredSelectionItem );
		DeferredSelectionItem.Reset();
	}

	//sync details panel with our selected item
	if(DetailsView.IsValid())
	{
		const TArray<TWeakObjectPtr<UObject>>& cur_sel = DetailsView->GetSelectedObjects();
		int num_selected = cur_sel.Num();
		int num_wanted = (SelectedItem.IsValid() && SelectedItem->Resource.IsValid()) ? 1 : 0;
		if(num_selected != num_wanted
			|| (num_selected > 0 && cur_sel[0] != SelectedItem->Resource)
			|| bForceApplySelection)
		{
			bForceApplySelection = false;

			//change selected item
			if(num_wanted > 0)
			{
				DetailsView->SetObject( SelectedItem->Resource.Get() );
			}
			else
			{
				DetailsView->SetObject( nullptr );
			}
			DetailsView->ForceRefresh();

			//treeview too
			TreeView->SetSelection( SelectedItem );
		}
	}

	//track missing assets
	int num_missing = FApparanceUnrealModule::GetModule()->GetMissingAssets().Num();
	if(num_missing!=CurrentMissingCount)
	{
		CurrentMissingCount = num_missing;
		RebuildEntryViewModel();
	}

	return true; //keep on tickin
}

// can we currently add a new folder?
//
bool FResourceListEditor::CheckAddFolderAllowed() const
{
	FResourceListInsertion insertion = EvaluateAdd( SelectedItem.Get(), UApparanceResourceListEntry::StaticClass() );
	return insertion.IsValid();
}
// can we currently add a new resource?
bool FResourceListEditor::CheckAddResourceAllowed() const
{
	FResourceListInsertion insertion = EvaluateAdd( SelectedItem.Get(), UApparanceResourceListEntry_Asset::StaticClass() );
	return insertion.IsValid();
}
// can we currently remove an item?
bool FResourceListEditor::CheckRemoveItemAllowed() const
{
	return SelectedItem.IsValid() //can only remove if user has selected something
			&& SelectedItem!=ResourcesRoot  //can never remove the roots
			&& SelectedItem!=ReferencesRoot
			&& SelectedItem!=MissingRoot && !IsUnderMissingRoot( SelectedItem.Get() );
}
// can we toggle variants support of an item?
bool FResourceListEditor::CheckToggleVariantsAllowed() const
{
	FResourceListEditorTreeItem* current_item = SelectedItem.Get();
	if(current_item)
	{	
		return (current_item->HasVariants() && current_item->GetVariantCount()>0) 	//variant (with an asset), can become single
			|| (current_item->IsAsset() && !current_item->IsVariant()); 			//single, can become variant
	}
	return false;
}
void FResourceListEditor::OnToggleHelpClicked()
{
	FResourcesListEntryPropertyCustomisation::ToggleInlineHelpText();
	DetailsView->ForceRefresh();
}
bool FResourceListEditor::CheckToggleHelpAllowed()
{
	return true;
}


// add a new resource folder to the hierarchy at the current position
//
void FResourceListEditor::OnAddFolderClicked()
{
	//work out where
	UClass* folder_type = UApparanceResourceListEntry::StaticClass();
	FResourceListInsertion insertion = EvaluateAdd( SelectedItem.Get(), folder_type );
	if(insertion.IsValid())
	{
		//insert
		InsertEntry( insertion );
	}
}

// prompt user to select new resource entry type to add
//
void FResourceListEditor::OnAddResourceClicked()
{
	FResourceListInsertion insertion = EvaluateAdd( SelectedItem.Get(), nullptr/*general add*/ );
	if (insertion.IsTypeValid())
	{
		if(insertion.IsInsertionValid())
		{
			//was type implied by operation?
			//yes, insert now
			InsertEntry( insertion );
		}
		else
		{
			//no, need more info, ask user
			ShowAssetTypeContextMenu();
		}
	}
}

// add new resource entry to the hierarchy at the current position
//
void FResourceListEditor::AddChosenResource( const UClass* entry_type )
{
	//now we have type, re-eval and apply
	FResourceListInsertion insertion = EvaluateAdd( SelectedItem.Get(), entry_type );
	if(insertion.IsValid())
	{
		//insert now
		InsertEntry( insertion );
	}
}

// remove the currently selected entry in the hierarchy
//
void FResourceListEditor::OnRemoveItemClicked()
{
	FResourceListEditorTreeItem::Ptr current_item = SelectedItem;
	if(current_item)
	{
		RemoveEntry( current_item->Parent->AsShared(), current_item->TreeParentIndex );
	}	
}

// toggle between single resource and a variant list
//
void FResourceListEditor::OnToggleVariantsClicked()
{
	FResourceListEditorTreeItem::Ptr current_item = SelectedItem;
	if(current_item)
	{				
		current_item->ToggleVariants();
		RebuildEntryViewModel( true/*because we are changing the target object but keeping the item the same*/ );
	}
}


// helper to ensure new entries are set up properly
//
UApparanceResourceListEntry* FResourceListEditor::CreateEntry( const UClass* entry_type ) const
{
	UApparanceResourceList* res_list = GetEditableResourceList();

	//NOTE: only place we should be creating entries, and they all must have the owning resource list as their outer so that they can be moved about within the hierarchy
	UApparanceResourceListEntry* new_entry = NewObject<UApparanceResourceListEntry>( res_list, entry_type, NAME_None, RF_Transactional );

	//default name for new entry is just class desc for now
	if (new_entry->IsNameEditable())
	{
		new_entry->SetName("unnamed");
	}

	return new_entry;
}

// edit: perform insert operation
//
void FResourceListEditor::InsertEntry( FResourceListInsertion insertion, UObject* explicit_entry_or_asset )
{
	check(insertion.IsInsertionValid());
	
	//insertion context
	FResourceListEditorTreeItem::Ptr container = insertion.Container;
	UApparanceResourceListEntry* parent_entry = container->Resource.Get();
	int tree_parent_index = insertion.TreeParentIndex;
	UApparanceResourceListEntry* explicit_entry = Cast<UApparanceResourceListEntry>( explicit_entry_or_asset );
	UObject* explicit_asset = explicit_entry?nullptr:explicit_entry_or_asset;
	UApparanceResourceListEntry* entry = explicit_entry;
	
	//undo support
	{
		FScopedTransaction transaction( LOCTEXT("InsertResourceListEntry","Insert resource list entry") );
		
		//save pre-edit state
		parent_entry->Modify();
		
		//need entry to insert
		if(!entry)
		{
			if(explicit_asset)
			{
				//it's an asset, wrap in entry
				entry = CreateAssetEntry( explicit_asset );
			}
			else
			{
				//required to create an empty entry or variant list
				check(insertion.IsTypeValid());		//wasn't provided with a usable insertion
				entry = CreateEntry( insertion.AllowedType );
			}
		}
		
		//entry insertion
		parent_entry->Children.Insert( CastChecked<UApparanceResourceListEntry>( entry ), tree_parent_index ); //(tree index only makes sense for variants which are unsorted, don't care where this added for non-variants, as will sort anyway)
		if(explicit_asset)
		{
			//need to re-trigger asset setup as transform could be affected by new parent (e.g. if variant) which we didn't have when the asset entry was created above
			UApparanceResourceListEntry_Asset* passet_entry = Cast< UApparanceResourceListEntry_Asset>( entry );
			passet_entry->SetAsset( explicit_asset );
		}
		parent_entry->Editor_NotifyCacheInvalidatingChange();	//need to manually invalidate since we are externally manipulating the entry
	}

	//refresh our VM to match straight away
	RebuildEntryViewModel(); 

	//new item is left selected
	DeferredSelect( entry );
}

// edit: perform a move operation
//
void FResourceListEditor::MoveEntry( FResourceListMovement movement )
{
	//gather op info
	UApparanceResourceListEntry* source_container_entry = movement.SourceContainer->Resource.Get();
	UApparanceResourceListEntry* dest_container_entry = movement.DestinationContainer->Resource.Get();
	int source_tree_parent_index = movement.SourceTreeParentIndex;
	int dest_tree_parent_index = movement.DestinationTreeParentIndex;
	FResourceListEditorTreeItem::Ptr source_entry = movement.SourceContainer->Children[source_tree_parent_index];
	int source_resource_parent_index = 0;
	int dest_resource_parent_index = dest_tree_parent_index;	//there isn't an entry yet so insert wherever (variants will use this, others are sorted anyway)
	if(!ensure( source_container_entry->Children.Find( source_entry->Resource.Get(), source_resource_parent_index ) ))
	{
		return;
	}

	//checks
	if(!source_container_entry || !dest_container_entry)
	{
		return;
	}

	//derived info
	UApparanceResourceListEntry_Variants* source_from_variants = Cast<UApparanceResourceListEntry_Variants>( source_container_entry );
	UApparanceResourceListEntry_Variants* dest_to_variants = Cast<UApparanceResourceListEntry_Variants>( dest_container_entry );
	bool promote_to_variant = !source_from_variants && dest_to_variants;
	bool demote_from_variant = source_from_variants && !dest_to_variants;
	
	//undoable transaction
	{
		FScopedTransaction transaction( LOCTEXT("RemoveResourceListEntry","Remove resource list entry") );
		
		source_container_entry->Modify();	//save pre-edit state
		dest_container_entry->Modify();
		
		//entry removal
		UApparanceResourceListEntry* entry_being_moved = source_container_entry->Children[source_resource_parent_index];
		source_container_entry->Children.RemoveAt( source_resource_parent_index );
		source_container_entry->Editor_NotifyCacheInvalidatingChange();	//need to manually invalidate since we are externally manipulating the entry
		
		//entry insertion
		dest_container_entry->Children.Insert( entry_being_moved, dest_resource_parent_index );
		dest_container_entry->Editor_NotifyCacheInvalidatingChange();	//need to manually invalidate since we are externally manipulating the entry

		//special behaviours
		if(demote_from_variant)
		{
			//variants don't have names (although you can set them, they aren't used)
			//so may need to generate one (although may have one from previous promotion)
			if(entry_being_moved->GetName().IsEmpty())
			{
				entry_being_moved->SetName( FText::Format( LOCTEXT("ConvertedVariantDescription","Variant {0}"), source_tree_parent_index+1 ).ToString() );
			}
		}
	}
	
	//refresh our VM to match straight away
	RebuildEntryViewModel();
}

// edit: perform remove operation
//
UObject* FResourceListEditor::RemoveEntry( FResourceListEditorTreeItem::Ptr container, int tree_parent_index )
{
	//gather entry info
	UApparanceResourceListEntry* parent_entry = container->Resource.Get();
	UObject* old_entry = nullptr;
	bool deleting_selected = SelectedItem == container->Children[tree_parent_index];
	UApparanceResourceListEntry* pentry = container->Children[tree_parent_index]->Resource.Get();
	
	//find index in parent resource
	int resource_parent_index = 0;
	if(!ensure( parent_entry->Children.Find( pentry, resource_parent_index ) ))
	{
		return nullptr;
	}

	//undoable transaction
	{
		FScopedTransaction transaction( LOCTEXT("RemoveResourceListEntry","Remove resource list entry") );
		
		parent_entry->Modify();	//save pre-edit state
		
		//entry removal
		old_entry = parent_entry->Children[resource_parent_index];
		parent_entry->Children.RemoveAt( resource_parent_index );
		parent_entry->Editor_NotifyCacheInvalidatingChange();	//need to manually invalidate since we are externally manipulating the entry
	}

	//always deselect the deleted item immediately
	if (deleting_selected)
	{
		SetSelection(nullptr);
	}

	//refresh our VM to match straight away
	RebuildEntryViewModel();

	//selection, any left to leave selected?
	UApparanceResourceListEntry* new_sel = parent_entry;
	if(parent_entry->Children.Num()>0)
	{
		//leave a element/variant selected
		int new_index = FMath::Min( tree_parent_index, parent_entry->Children.Num()-1 );	//clamp to end (e.g. for deleting last one)
		new_sel = parent_entry->Children[new_index];
	}
	DeferredSelect(new_sel);

	return old_entry;
}


// ask the user immediately for a type of resource (e.g. mesh, bp, material)
//
void FResourceListEditor::ShowAssetTypeContextMenu()
{
	//setup spontaneous context menu
	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, nullptr);
	const FText SelectResourceTypeHeaderString = LOCTEXT("SelectResourceType", "Select Resource Type");
	MenuBuilder.BeginSection("SelectResourceType", SelectResourceTypeHeaderString);
	{
#if 0//folders have own button :/
		MenuBuilder.AddMenuEntry(
			LOCTEXT("AddFolderCommand","Add Folder"),
			FText(),
			FSlateIcon( APPARANCE_STYLE.GetStyleSetName(), UApparanceResourceListEntry::StaticIconName() ),
			FUIAction(FExecuteAction::CreateLambda([this]() { AddChosenResource( UApparanceResourceListEntry::StaticClass() ); }))
		);

		MenuBuilder.AddMenuSeparator();
#endif
		MenuBuilder.AddMenuEntry(
			UApparanceResourceListEntry_StaticMesh::StaticAssetTypeName(),
			FText(),
			FSlateIcon( APPARANCE_STYLE.GetStyleSetName(), UApparanceResourceListEntry_StaticMesh::StaticIconName() ),
			FUIAction(FExecuteAction::CreateLambda([this]() { AddChosenResource( UApparanceResourceListEntry_StaticMesh::StaticClass() ); }))
		);
		MenuBuilder.AddMenuEntry(
			UApparanceResourceListEntry_Blueprint::StaticAssetTypeName(),
			FText(),
			FSlateIcon( APPARANCE_STYLE.GetStyleSetName(), UApparanceResourceListEntry_Blueprint::StaticIconName() ),
			FUIAction(FExecuteAction::CreateLambda([this]() { AddChosenResource( UApparanceResourceListEntry_Blueprint::StaticClass() ); }))
		);
		MenuBuilder.AddMenuEntry(
			UApparanceResourceListEntry_Material::StaticAssetTypeName(),
			FText(),
			FSlateIcon( APPARANCE_STYLE.GetStyleSetName(), UApparanceResourceListEntry_Material::StaticIconName() ),
			FUIAction(FExecuteAction::CreateLambda([this]() { AddChosenResource( UApparanceResourceListEntry_Material::StaticClass() ); }))
		);
		MenuBuilder.AddMenuEntry(
			UApparanceResourceListEntry_Texture::StaticAssetTypeName(),
			FText(),
			FSlateIcon( APPARANCE_STYLE.GetStyleSetName(), UApparanceResourceListEntry_Texture::StaticIconName() ),
			FUIAction(FExecuteAction::CreateLambda([this]() { AddChosenResource( UApparanceResourceListEntry_Texture::StaticClass() ); }))
		);
		MenuBuilder.AddMenuEntry(
			UApparanceResourceListEntry_Component::StaticAssetTypeName(),
			FText(),
			FSlateIcon( APPARANCE_STYLE.GetStyleSetName(), UApparanceResourceListEntry_Component::StaticIconName() ),
			FUIAction(FExecuteAction::CreateLambda([this]() { AddChosenResource( UApparanceResourceListEntry_Component::StaticClass() ); }))
		);
	}
	MenuBuilder.EndSection();
	
	//present menu
	FSlateApplication::Get().PushMenu(
		GetToolkitHost()->GetParentWidget(),
		FWidgetPath(),
		MenuBuilder.MakeWidget(),
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu)
	);
}

// these resources notify of any structural change
void FResourceListEditor::OnResourceListStructuralChange( UApparanceResourceList* pres_list, UApparanceResourceListEntry* pentry, FName property )
{
	if(GetEditableResourceList()==pres_list)
	{
		//it was ours, panic, rebuild everything
		RebuildEntryViewModel();
	}
}

// these resources notify of any structural change
void FResourceListEditor::OnResourceListAssetChange( UApparanceResourceList* pres_list, UApparanceResourceListEntry* pentry, UObject* pnew_asset )
{
	if(GetEditableResourceList() == pres_list)
	{
		//it was ours
		UApparanceResourceListEntry_Asset* passet_entry = Cast<UApparanceResourceListEntry_Asset>( pentry );
		if(passet_entry)
		{
			//find tree entry for it
			FResourceListEditorTreeItem::Ptr tree_item = FindHierarchyEntry( passet_entry );
			if(tree_item.IsValid())
			{
				//do full resource change
				if(tree_item->SetResource( pnew_asset ))
				{
					//update treeview (in case assignment changed semantics of controls, e.g. not want specific type filter)
					RebuildEntryViewModel();
				}
			}
		}
	}
}



// refresh internal representation of current resource list state
//
void FResourceListEditor::RebuildEntryViewModel( bool track_item_instead_of_entry )
{
	//should always only be root
	if(TreeRoot.Num()!=3)
	{
		TreeRoot.Empty();

		ResourcesRoot = MakeShareable( new FResourceListEditorTreeItem( this ) );
		TreeRoot.Add( ResourcesRoot );

		ReferencesRoot = MakeShareable( new FResourceListEditorTreeItem( this ) );
		TreeRoot.Add( ReferencesRoot );

		MissingRoot = MakeShareable( new FResourceListEditorTreeItem( this ) );
		TreeRoot.Add( MissingRoot );
	}

	//store selection state
	if(SelectedItem.IsValid())
	{
		if(track_item_instead_of_entry)
		{
			DeferredSelect( SelectedItem );
		}
		else
		{
			TWeakObjectPtr<UApparanceResourceListEntry> selected_entry = SelectedItem.Get()->Resource;
			DeferredSelect( selected_entry );
		}
		bForceApplySelection = true; //likely re-applying same object, need force to update details and toolbar		
	}
	SetSelection( FResourceListEditorTreeItem::Ptr() ); //clear actual selection for rebuild
	
	//incremental update
	UApparanceResourceList* pres_list = GetEditableResourceList();
	ResourcesRoot->Sync( pres_list->Resources );
	ReferencesRoot->Sync( pres_list->References );
	MissingRoot->SyncMissing( FApparanceUnrealModule::GetModule()->GetMissingAssets(), ResourcesRoot->GetName() );
	
	RefreshTreeview();
}

// update Slate-side when our viewmodel has chnaged
//
void FResourceListEditor::RefreshTreeview()
{
	//trigger UI update to match
	if(TreeView.IsValid())
	{
		TreeView->RebuildList(); //ensure any item presentation changes are applied
		TreeView->RequestTreeRefresh(); //update tree UI structure from hierarchy
	}
}


/** IToolkit interface */

FName FResourceListEditor::GetToolkitFName() const
{
	return FName("ApparanceResourceListEditor");
}

FText FResourceListEditor::GetBaseToolkitName() const
{
	return LOCTEXT( "ResourceListEditorLabel", "Resource List Editor" );
}

FString FResourceListEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("ResourceListEditorTabPrefix", "Resource List ").ToString();
}

FLinearColor FResourceListEditor::GetWorldCentricTabColorScale() const
{
	return FColor::Emerald.ReinterpretAsLinear();
}

/** FEditorUndoClient */

bool FResourceListEditor::MatchesContext( const FTransactionContext& InContext, const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjectContexts ) const
{
	//check if this transaction affects us
	for (const TPair<UObject*, FTransactionObjectEvent>& TransactionObjectPair : TransactionObjectContexts)
	{	
		UApparanceResourceList* our_res_list = GetEditableResourceList();

		//walk up object hierarchy looking for root resource list
		UObject* Object = TransactionObjectPair.Key;
		while (Object != nullptr)
		{
			//is a class we're interested in and within the resource list we are an editor for?
			UApparanceResourceList* pres_list_object = Cast<UApparanceResourceList>( Object );
			if(pres_list_object==our_res_list)
			{
				//request that PostUndo/PostRedo below to be called as appropriate
				return true;
			}
			Object = Object->GetOuter();
		}
	}
	return false;
}
void FResourceListEditor::PostUndo( bool bSuccess )
{
	OnUndoRedo();
}
void FResourceListEditor::PostRedo( bool bSuccess )
{
	OnUndoRedo();
}

// action required after an editor-wide undo or a redo
//
void FResourceListEditor::OnUndoRedo()
{
	//invalidates view model
	RebuildEntryViewModel();

	//potentially invalidates all entities
	FApparanceUnrealModule::GetModule()->Editor_NotifyAssetDatabaseChanged();
}


/** FAssetEditorToolkit interface */

void FResourceListEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_Resource List Editor", "Resource List Editor"));
	
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
	
	CreateAndRegisterResourceListHierarchyTab(InTabManager);
	CreateAndRegisterResourceListDetailsTab(InTabManager);
}

void FResourceListEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
	
	InTabManager->UnregisterTabSpawner(ResourceListHierarchyTabId);
	InTabManager->UnregisterTabSpawner(ResourceListDetailsTabId);
	
	ResourceListHierarchyTabWidget.Reset();
}

// hierarchy tab type
//
void FResourceListEditor::CreateAndRegisterResourceListHierarchyTab(const TSharedRef<class FTabManager>& InTabManager)
{
	ResourceListHierarchyTabWidget = CreateResourceListHierarchyUI();
	
	InTabManager->RegisterTabSpawner(ResourceListHierarchyTabId, FOnSpawnTab::CreateSP(this, &FResourceListEditor::SpawnTab_ResourceListHierarchy))
		.SetDisplayName(LOCTEXT("ResourceListHierarchyTab", "Resources"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
}

// details tab type
//
void FResourceListEditor::CreateAndRegisterResourceListDetailsTab(const TSharedRef<class FTabManager>& InTabManager)
{
	//access standard property panel
	FPropertyEditorModule & EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.bLockable = false;
	DetailsViewArgs.bAllowSearch = true;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsView = EditModule.CreateDetailView(DetailsViewArgs);
	
	InTabManager->RegisterTabSpawner(ResourceListDetailsTabId, FOnSpawnTab::CreateSP(this, &FResourceListEditor::SpawnTab_ResourceListDetails))
		.SetDisplayName(LOCTEXT("ResourceListDetailsTab", "Details"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
}

// hierarchy tab creation
//
TSharedRef<SDockTab> FResourceListEditor::SpawnTab_ResourceListHierarchy( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId().TabType == ResourceListHierarchyTabId );

	UApparanceResourceList* res_list = GetEditableResourceList();

	// Support undo/redo
	if (res_list)
	{
		res_list->SetFlags(RF_Transactional);
	}

	TSharedRef<SDockTab> tab = SNew(SDockTab)
		.Label( LOCTEXT("ResourceListHierarcyTitle", "Resources") )
		.TabColorScale( GetTabColorScale() )
		//.OnTabActivated( this, &FResourceListEditor::OnHierarchyTabActivated )	//this doesn't appear to work, see below
		[
			SNew(SBorder)
			.Padding(2)
			.BorderImage( APPARANCE_STYLE.GetBrush( "ToolPanel.GroupBorder" ) )
			[
				ResourceListHierarchyTabWidget.ToSharedRef()
			]
		];

	//set explicitly for now to avoid warning (supposed to be handled by spawner)
	tab->SetTabIcon( APPARANCE_STYLE.GetBrush( "LevelEditor.Tabs.ContentBrowser" ) );

	//BUG?: the imperitive event setter above doesn't appear to work, direct setter does however
	tab->SetOnTabActivated( SDockTab::FOnTabActivatedCallback::CreateSP( this, &FResourceListEditor::OnHierarchyTabActivated ) );

	return tab;
}

// property tab creation
//
TSharedRef<SDockTab> FResourceListEditor::SpawnTab_ResourceListDetails(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == ResourceListDetailsTabId);

	TSharedRef<SDockTab> tab = SNew(SDockTab)
		.Label(LOCTEXT("ResourceListDetailsTitle", "Details"))
		.TabColorScale(GetTabColorScale())
		[
			SNew(SBorder)
			.Padding(2)
			.BorderImage( APPARANCE_STYLE.GetBrush("ToolPanel.GroupBorder"))
			[
				DetailsView.ToSharedRef()
			]
		];

	//set explicitly for now to avoid warning (supposed to be handled by spawner)
	tab->SetTabIcon( APPARANCE_STYLE.GetBrush( "GenericEditor.Tabs.Properties" ) );

	return tab;
}

// hierarchy tab active, ensure property panel follows selected item
//
void FResourceListEditor::OnHierarchyTabActivated(TSharedRef<SDockTab> tab, ETabActivationCause cause)
{
}

// hierarchy UI creation
//
TSharedRef<SVerticalBox> FResourceListEditor::CreateResourceListHierarchyUI()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SAssignNew(TreeView, SResourcesListTreeView, SharedThis(this))
			.TreeItemsSource(&TreeRoot)
			.OnGenerateRow(this, &FResourceListEditor::GenerateTreeRow)
//			.OnItemScrolledIntoView(TreeView, &SResourcesListTreeView::OnTreeItemScrolledIntoView)
			.ItemHeight(21)
//			.SelectionMode(InArgs._SelectionMode)
			.OnSelectionChanged(this, &FResourceListEditor::OnTreeViewSelect)
//			.OnExpansionChanged(this, &SPathView::TreeExpansionChanged)
			.OnGetChildren(this, &FResourceListEditor::GetChildrenForTree)
//			.OnSetExpansionRecursive(this, &SPathView::SetTreeItemExpansionRecursive)
			.OnContextMenuOpening(this, &FResourceListEditor::GenerateTreeContextMenu)
			.ClearSelectionOnClick(false)
			.HighlightParentNodesForSelection(true)				
			.HeaderRow
			(
				SNew(SHeaderRow)
				+ SHeaderRow::Column( SResourcesListTreeView::ColumnTypes::Path() )
				.FillWidth(0.4f)
				[
					SNew(STextBlock).Text( LOCTEXT("ResourcePathColumn","Resource Path") )
					.Margin(4.0f)
				]
				+ SHeaderRow::Column( SResourcesListTreeView::ColumnTypes::Type() )
				.FillWidth(0.2f)
				[
					SNew(STextBlock).Text( LOCTEXT("ResourceType", "Type" ) )
					.Margin(4.0f)
				]
				+ SHeaderRow::Column( SResourcesListTreeView::ColumnTypes::Asset() )
				.FillWidth(0.4f)
				[
					SNew(STextBlock).Text( LOCTEXT("ResourceColumn", "Asset" ) )
					.Margin(4.0f)
				]
			)
		];
}

// context menu on items
//
TSharedPtr<SWidget> FResourceListEditor::GenerateTreeContextMenu()
{
	//anything selected?
	if(!SelectedItem.IsValid())
	{
		return nullptr;
	}
	
	//can only set type of ones not already assigned
	if (SelectedItem->GetResourceEntryType()!=UApparanceResourceListEntry_Asset::StaticClass())
	{
		return nullptr;
	}

	//setup spontaneous context menu
	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection=*/true, nullptr);
	const FText SelectResourceTypeHeaderString = LOCTEXT("SelectResourceType", "Select Resource Type");
	MenuBuilder.BeginSection("SelectResourceType", SelectResourceTypeHeaderString);
	{
		MenuBuilder.AddMenuEntry(
			UApparanceResourceListEntry_StaticMesh::StaticAssetTypeName(),
			FText(),
			FSlateIcon( APPARANCE_STYLE.GetStyleSetName(), UApparanceResourceListEntry_StaticMesh::StaticIconName() ),
			FUIAction(FExecuteAction::CreateLambda([this]() { ChangeEntryType( UApparanceResourceListEntry_StaticMesh::StaticClass() ); }))
		);
		MenuBuilder.AddMenuEntry(
			UApparanceResourceListEntry_Blueprint::StaticAssetTypeName(),
			FText(),
			FSlateIcon( APPARANCE_STYLE.GetStyleSetName(), UApparanceResourceListEntry_Blueprint::StaticIconName() ),
			FUIAction(FExecuteAction::CreateLambda([this]() { ChangeEntryType( UApparanceResourceListEntry_Blueprint::StaticClass() ); }))
		);
		MenuBuilder.AddMenuEntry(
			UApparanceResourceListEntry_Material::StaticAssetTypeName(),
			FText(),
			FSlateIcon( APPARANCE_STYLE.GetStyleSetName(), UApparanceResourceListEntry_Material::StaticIconName() ),
			FUIAction(FExecuteAction::CreateLambda([this]() { ChangeEntryType( UApparanceResourceListEntry_Material::StaticClass() ); }))
		);
		MenuBuilder.AddMenuEntry(
			UApparanceResourceListEntry_Texture::StaticAssetTypeName(),
			FText(),
			FSlateIcon( APPARANCE_STYLE.GetStyleSetName(), UApparanceResourceListEntry_Texture::StaticIconName() ),
			FUIAction(FExecuteAction::CreateLambda([this]() { ChangeEntryType( UApparanceResourceListEntry_Texture::StaticClass() ); }))
		);
		MenuBuilder.AddMenuEntry(
			UApparanceResourceListEntry_Component::StaticAssetTypeName(),
			FText(),
			FSlateIcon( APPARANCE_STYLE.GetStyleSetName(), UApparanceResourceListEntry_Component::StaticIconName() ),
			FUIAction(FExecuteAction::CreateLambda([this]() { ChangeEntryType( UApparanceResourceListEntry_Component::StaticClass() ); }))
		);
	}
	MenuBuilder.EndSection();
	
	return MenuBuilder.MakeWidget();
}

// change an existing type
//
void FResourceListEditor::ChangeEntryType(const UClass* resource_list_entry_type)
{
	check(resource_list_entry_type);
	check(SelectedItem.IsValid());
	FResourceListEditorTreeItem::Ptr entry = FindHierarchyEntry( SelectedItem.Get()->Resource.Get() );
	check(entry);

	entry->ChangeEntryType(resource_list_entry_type);
}

// hierarchy item selected
//
void FResourceListEditor::OnTreeViewSelect( FResourceListEditorTreeItem::Ptr Item, ESelectInfo::Type SelectInfo )
{
	SetSelection( Item );
}

// request a select by entry/variant, but not immediately
//
void FResourceListEditor::DeferredSelect( TWeakObjectPtr<UApparanceResourceListEntry> selected_entry )
{
	DeferredSelectionEntry = selected_entry;
}

// request a select by tree item, but not immediately
//
void FResourceListEditor::DeferredSelect( FResourceListEditorTreeItem::Ptr selected_item )
{
	DeferredSelectionItem = selected_item;
}

// update our editor local selection state, show in Details tab
//
void FResourceListEditor::SetSelection( FResourceListEditorTreeItem::Ptr Item )
{
	SelectedItem = Item;
}

// is there a selected item and is it within the resource hierarchy?
//
bool FResourceListEditor::IsUnderResourceRoot( FResourceListEditorTreeItem* item ) const
{
	while(item)
	{
		if(item==ResourcesRoot.Get())
		{
			return true;
		}
		item = item->Parent;
	}
	return false;
}

// is there a selected item and is it within the references hierarchy?
//
bool FResourceListEditor::IsUnderReferencesRoot(FResourceListEditorTreeItem* item) const
{
	while(item)
	{
		if(item==ReferencesRoot.Get())
		{
			return true;
		}
		item = item->Parent;
	}
	return false;
}

// is there a selected item and is it within the missing hierarchy?
//
bool FResourceListEditor::IsUnderMissingRoot(FResourceListEditorTreeItem* item) const
{
	while(item)
	{
		if(item==MissingRoot.Get())
		{
			return true;
		}
		item = item->Parent;
	}
	return false;
}

// is selected item within the resource hierarchy or is nothing selected?
//
bool FResourceListEditor::IsUnderResourceRootOrUnselected( FResourceListEditorTreeItem* item ) const
{
	return item==nullptr || IsUnderResourceRoot( item );
}

// is selected item within the reference hierarchy or is nothing selected?
//
bool FResourceListEditor::IsUnderReferencesRootOrUnselected( FResourceListEditorTreeItem* item ) const
{
	return item==nullptr || !IsUnderResourceRoot( item );
}

// is selected item within the missinghierarchy or is nothing selected?
//
bool FResourceListEditor::IsUnderMissingRootOrUnselected( FResourceListEditorTreeItem* item ) const
{
	return item==nullptr || !IsUnderMissingRoot( item );
}

// is user allowed to rename this item?
//
bool FResourceListEditor::CanRenameItem( FResourceListEditorTreeItem* item ) const
{
	//can rename any normal resource entry, including root
	//(except variants which are implicitly named)
	return IsUnderResourceRoot( item )
		&& !item->IsVariant();
}

// is user allowed to delete this item?
//
bool FResourceListEditor::CanDeleteItem( FResourceListEditorTreeItem* item ) const
{
	//can delete any normal resource entry, not root though
	return IsUnderResourceRoot( item ) && item!=ResourcesRoot.Get();
}
	
// can we start a drag and drop operation from this item?
//
bool FResourceListEditor::IsItemValidDragSource( FResourceListEditorTreeItem* item ) const
{
	//can move any sub-entry of the resource hierarchy
	return IsUnderResourceRoot( item ) && item!=ResourcesRoot.Get();
}

	

// build a row on demand
//
TSharedRef<ITableRow> FResourceListEditor::GenerateTreeRow( TSharedPtr<FResourceListEditorTreeItem> TreeItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	check(TreeItem.IsValid());
	return SNew(SResourcesListTreeRow, OwnerTable)
		.Item( TreeItem )
		.ResourceListEditor(SharedThis(this));
}

// hierarchy access
//
void FResourceListEditor::GetChildrenForTree( TSharedPtr< FResourceListEditorTreeItem > TreeItem, TArray< TSharedPtr<FResourceListEditorTreeItem> >& OutChildren )
{
	//TreeItem->SortChildrenIfNeeded();
	OutChildren = TreeItem->Children;
}

// what resource entry wrapper object type is needed for this actual asset type?
// e.g. a UStaticMesh is wrapped by a UApparanceResourceListEntry_StaticMesh
//
const UClass* FResourceListEditor::GetEntryTypeForAsset( const UClass* passet_class )
{
	//TODO: maybe table search based lookup?
	if(passet_class->IsChildOf( UStaticMesh::StaticClass() ))
	{
		return UApparanceResourceListEntry_StaticMesh::StaticClass();
	}
	else if(passet_class->IsChildOf( UBlueprint::StaticClass() ))
	{
		return UApparanceResourceListEntry_Blueprint::StaticClass();
	}
	else if(passet_class->IsChildOf( UMaterialInterface::StaticClass() ))
	{
		return UApparanceResourceListEntry_Material::StaticClass();
	}
	else if(passet_class->IsChildOf( UTexture::StaticClass() ))
	{
		return UApparanceResourceListEntry_Texture::StaticClass();
	}
	else if(passet_class->IsChildOf( UActorComponent::StaticClass() ))
	{
		return UApparanceResourceListEntry_Component::StaticClass();
	}
	else if(passet_class->IsChildOf( UApparanceResourceList::StaticClass() ))
	{
		return UApparanceResourceListEntry_ResourceList::StaticClass();
	}
	return nullptr;
}

// wrap an asset object in the appropriate resource entry for it
// e.g. a UStaticMesh is wrapped by a UApparanceResourceListEntry_StaticMesh
//
UApparanceResourceListEntry* FResourceListEditor::CreateAssetEntry( UObject* passet ) const
{
	const UClass* pentry_type = GetEntryTypeForAsset( passet->GetClass() );
	if(pentry_type)
	{
		UApparanceResourceListEntry* pentry = CreateEntry( pentry_type );
		
		//assign asset
		UApparanceResourceListEntry_Asset* passet_entry = CastChecked<UApparanceResourceListEntry_Asset>( pentry );
		passet_entry->SetAsset( passet );
		
		return pentry;
	}
	else
	{
		return nullptr;
	}
}

// work out what insertion would be performed if an asset of the given type was added to a particular entry, if allowed
// 
FResourceListInsertion FResourceListEditor::EvaluateAdd( FResourceListEditorTreeItem* item, const UClass* asset_class ) const
{
	//target
	bool is_valid = item!=nullptr;
	bool is_no_item = item==nullptr;
	bool is_under_resources = IsUnderResourceRoot( item );
	bool is_under_references = IsUnderReferencesRoot( item );
	bool is_under_missing = IsUnderMissingRoot( item );
	bool is_actual_res_root = item==ResourcesRoot.Get();
	bool is_actual_ref_root = item==ReferencesRoot.Get();
	bool is_actual_missing_root = item==MissingRoot.Get();
	bool is_root = is_actual_res_root || is_actual_ref_root;
	bool is_variant = is_valid && item->IsVariant();
	bool is_variant_container = is_valid && item->HasVariants();
	bool is_asset = is_valid && item->IsAsset();
	bool is_resourcelist = is_valid && item->IsResourceList();
	bool is_folder = is_valid && !is_asset && !is_resourcelist;
	
	//source
	bool want_unspecified = asset_class==nullptr;
	bool want_reslist = !want_unspecified && asset_class->IsChildOf(UApparanceResourceListEntry_ResourceList::StaticClass());
	bool want_asset = !want_unspecified && !want_reslist && asset_class->IsChildOf( UApparanceResourceListEntry_Asset::StaticClass() );
	bool want_variants = !want_unspecified && !want_reslist && asset_class->IsChildOf( UApparanceResourceListEntry_Variants::StaticClass() );
	bool want_general_asset = want_asset && asset_class==UApparanceResourceListEntry_Asset::StaticClass();
	bool want_folder = !want_unspecified && !want_asset && !want_reslist && asset_class->IsChildOf( UApparanceResourceListEntry::StaticClass() );

	//Missing
	if (is_under_missing || is_actual_missing_root)
	{
		//nothing yet
		return FResourceListInsertion();
	}
	
	//GENERAL
	if(want_unspecified)
	{
		//nothing|root - prompt user for type, add asset into root
		//folder - prompt user for type, add asset into folder
		if(is_no_item || is_actual_res_root || is_folder)		
		{
			if(is_no_item || is_under_resources)
			{
				//need choice of asset type for insertion, can't determine insertion until we do

				//general type yielded to indicate choice needed
				//return FResourceListInsertion( FResourceListEditorTreeItem::Ptr(), -1, UApparanceResourceListEntry::StaticClass() );

				//now just spawn ambiguous slot as will promote to concrete slot when asset chosen
				asset_class = UApparanceResourceListEntry_Asset::StaticClass();
				want_unspecified = false;
				want_asset = true;
				//carry on eval below...				
			}
			else
			{
				//references then
				asset_class = UApparanceResourceListEntry_ResourceList::StaticClass();
				want_unspecified = false;
				want_reslist = true;
				//carry on eval below...
			}
		}
		else
		{
			//base on targetted entry, insert directly
			asset_class = item->GetResourceEntryType();
			want_unspecified = false;
			want_asset = true;
			//carry on eval below...
		}
	}

	//FOLDER
	if(want_folder)
	{
		const UClass* folder_type = UApparanceResourceListEntry::StaticClass();

		if(is_under_references)
		{
			//reference hierarchy - no folders allowed at the moment
			return FResourceListInsertion();
		}
		else if(is_no_item || is_actual_res_root)
		{
			//root - add into root
			return FResourceListInsertion( ResourcesRoot, ResourcesRoot->Children.Num()/*end*/, folder_type );
		}
		else if(is_variant)
		{	
			//variant - add folder after the owning asset
			return FResourceListInsertion( item->Parent->Parent->AsShared(), item->Parent->TreeParentIndex+1, folder_type );
		}
		else if(is_asset)
		{
			//asset - add folder after asset
			return FResourceListInsertion( item->Parent->AsShared(), item->TreeParentIndex +1, folder_type );
		}
		else if(is_folder)
		{
			//folder - add into folder
			return FResourceListInsertion( item->AsShared(), item->Children.Num()/*end*/, folder_type );
		}
		else
		{
			//not valid choice
			return FResourceListInsertion();
		}
	}
	
	//RESOURCE LIST
	if(want_reslist
		|| (want_general_asset 
			&& (is_actual_ref_root || is_under_references)))
	{
		if(want_general_asset)
		{
			//testing addition into references (e.g. toolbar button)
			const UClass* reference_class = UApparanceResourceListEntry_ResourceList::StaticClass();
			if(is_no_item || is_actual_ref_root)
			{
				//nothing|ref root - add ref asset into root
				return FResourceListInsertion( ReferencesRoot, ReferencesRoot.Get()->Children.Num()/*end*/, reference_class );
			}
			else if(is_under_references)
			{
				//ref - insert after ref
				return FResourceListInsertion( ReferencesRoot, item->TreeParentIndex+1, reference_class );
			}
			else
			{
				//not allowed
				return FResourceListInsertion();
			}
		}
		else if(is_no_item || is_actual_ref_root)
		{
			//nothing|root - add asset into root
			return FResourceListInsertion( ReferencesRoot, ReferencesRoot.Get()->Children.Num()/*end*/, asset_class );
		}
		else if(!is_under_references)
		{
			//only allowed in reference hierarchy
			return FResourceListInsertion();
		}
		else if(is_resourcelist)
		{
			//reslist - add after
			return FResourceListInsertion( item->Parent->AsShared(), item->TreeParentIndex +1, asset_class );
		}
		else
		{
			//not allowed anywhere else
			return FResourceListInsertion();
		}
	}
	
	//ASSET
	if(want_asset)
	{
		if(is_no_item || is_actual_res_root)
		{	
			//nothing|root - add asset into root
			return FResourceListInsertion( ResourcesRoot, ResourcesRoot.Get()->Children.Num()/*end*/, asset_class );
		}
		else if(!is_under_resources)
		{
			//specific assets are only allowed in resource hierarchy
			return FResourceListInsertion();
		}
		else if(!is_asset)
		{
			//folder - add asset into folder
			return FResourceListInsertion( item->AsShared(), item->Children.Num()/*end*/, asset_class );
		}
		else if(is_variant)
		{
			//add another variant immediately after
			return FResourceListInsertion( item->Parent->AsShared(), item->TreeParentIndex +1, asset_class );
		}
		else if(is_variant_container)
		{
			//add another variant to end of variant container
			return FResourceListInsertion( item->AsShared(), item->GetVariantCount()/*end*/, asset_class );
		}
		else //asset
		{
			//asset(single) - add asset of same after
			return FResourceListInsertion( item->Parent->AsShared(), item->TreeParentIndex +1, asset_class );
		}
	}
	
	//no idea
	return FResourceListInsertion();
}

// work out what movement would be performed if an item was dropped onto another item, if allowed
//
FResourceListMovement FResourceListEditor::EvaluateMove( FResourceListEditorTreeItem* source_item, FResourceListEditorTreeItem* destination_item ) const
{
	//source
	bool source_valid = source_item!=nullptr;
	bool source_no_item = source_item==nullptr;
	bool source_under_resources = IsUnderResourceRoot( source_item );
	bool source_actual_res_root = source_item==ResourcesRoot.Get();
	bool source_is_root = source_actual_res_root;
	bool source_is_variant = source_valid && source_item->IsVariant();
	bool source_is_variant_container = source_valid && source_item->HasVariants();
	bool source_is_asset = source_valid && source_item->IsAsset();
	bool source_is_folder = source_valid && !source_is_asset;

	//destination
	bool dest_valid = destination_item!=nullptr;
	bool dest_no_item = destination_item==nullptr;
	bool dest_under_resources = IsUnderResourceRoot( destination_item );
	bool dest_actual_res_root = destination_item==ResourcesRoot.Get();
	bool dest_is_root = dest_actual_res_root;
	bool dest_is_variant = dest_valid && destination_item->IsVariant();
	bool dest_is_variant_container = dest_valid && destination_item->HasVariants();
	bool dest_is_asset = dest_valid && destination_item->IsAsset();
	bool dest_is_folder = dest_valid && !dest_is_asset;

	//further analysis
	bool dest_is_same = source_valid && source_item->Parent==destination_item;
	bool dest_is_within_source = false;
	FResourceListEditorTreeItem* p = destination_item;
	while(p)
	{
		if(p==source_item)
		{
			dest_is_within_source = true;
			break;
		}
		p = p->Parent;
	}

	//valid items?
	if(!source_valid || !dest_valid)
	{
		return FResourceListMovement();
	}

	//anything to do?
	if(dest_is_same) //not moving
	{
		return FResourceListMovement();
	}

	//pathological cases
	if(dest_is_within_source) //can't move to within self
	{
		return FResourceListMovement();
	}

	//within resource hierarhcy?
	if(!source_under_resources || !dest_under_resources //can't move in-out of resource hierarchy
		|| source_actual_res_root)	//can't move the root
	{
		return FResourceListMovement();
	}
	
	//moving a folder?
	if(source_is_folder)
	{
		//can only move to another folder
		if(dest_is_folder)
		{
			//move folder to another folder
			return FResourceListMovement( source_item->Parent->AsShared(), source_item->TreeParentIndex, destination_item->AsShared(), destination_item->Children.Num()/*end*/ );
		}
		else
		{
			return FResourceListMovement();
		}
	}

	//moving an asset? (promote to variant?)
	if(source_is_asset)
	{
		if(dest_is_variant_container)
		{
			//promote asset to variant
			//(just a move, but separate incase needs specialisation)
			return FResourceListMovement( source_item->Parent->AsShared(), source_item->TreeParentIndex, destination_item->AsShared(), destination_item->Children.Num()/*end*/ );
		}
		else
		{
			//otherwise assets can only be moved to folders
			if(dest_is_folder)
			{
				//move
				return FResourceListMovement( source_item->Parent->AsShared(), source_item->TreeParentIndex, destination_item->AsShared(), destination_item->Children.Num()/*end*/ );
			}
			else
			{
				return FResourceListMovement();
			}
		}
	}

	//moving a variant? (demote to single asset?)
	if(source_is_variant)
	{
		if(dest_is_variant_container)
		{
			//move from one container to another
			//(just a move, but separate incase needs specialisation)
			return FResourceListMovement( source_item->Parent->AsShared(), source_item->TreeParentIndex, destination_item->AsShared(), destination_item->Children.Num()/*end*/ );
		}
		else
		{
			//otherwise variants can only be moved (demoted) to folders
			if(dest_is_folder)
			{
				//move and demote from variant to asset
				return FResourceListMovement( source_item->Parent->AsShared(), source_item->TreeParentIndex, destination_item->AsShared(), destination_item->Children.Num()/*end*/ );
			}
			else
			{
				return FResourceListMovement();
			}
		}
	}

	//TODO: deal with any unforseen moves
	check(false);
	return FResourceListMovement();
}


//------------------------------------------------------------------------
// SResourceListTreeView

void SResourcesListTreeView::Construct(const FArguments& InArgs, TSharedRef<FResourceListEditor> Owner)
{
	ResourceListEditorWeak = Owner;
	STreeView::Construct( InArgs );
}

// key handling
//	F2 - rename
//	Del - delete
//
FReply SResourcesListTreeView::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	//our editor
	TSharedPtr<FResourceListEditor> res_list_editor = ResourceListEditorWeak.Pin();
	if(res_list_editor.IsValid())
	{	
		//F2 to rename entry
		if( InKeyEvent.GetKey() == EKeys::F2 )
		{
			FResourceListEditorTreeItem::Ptr item = res_list_editor.IsValid()?res_list_editor->SelectedItem:nullptr;
			if (item.IsValid())
			{
				if(res_list_editor->CanRenameItem( item.Get() ))
				{
					//trigger
					item->RenameRequestEvent.ExecuteIfBound();
				}
				
				return FReply::Handled();
			}
		}
		
		//insert to add entries	
		else if ( InKeyEvent.GetKey() == EKeys::Insert )
		{
			if(res_list_editor->CheckAddResourceAllowed())
			{
				//pass to editor to perform delete
				res_list_editor->OnAddResourceClicked();
			}		
			
			return FReply::Handled();
		}
	
		//delete/bs to remove entries	
		else if ( InKeyEvent.GetKey() == EKeys::Delete || InKeyEvent.GetKey() == EKeys::BackSpace )
		{
			FResourceListEditorTreeItem::Ptr item = res_list_editor.IsValid()?res_list_editor->SelectedItem:nullptr;
			if (item.IsValid())
			{
				if(res_list_editor->CanDeleteItem( item.Get() ))
				{
					//pass to editor to perform delete
					res_list_editor->RemoveEntry( item->Parent->AsShared(), item->TreeParentIndex );
				}
				
				return FReply::Handled();
			}
			
			return FReply::Handled();
		}
	}


	return STreeView<TSharedPtr<FResourceListEditorTreeItem>>::OnKeyDown( MyGeometry, InKeyEvent );
}


//------------------------------------------------------------------------
// FResourceListDragDrop setup

FResourceListDragDrop::FResourceListDragDrop( FResourceListEditorTreeItem::Ptr dragged_item )
	: Item( dragged_item )
{
}

void FResourceListDragDrop::Construct()
{
	FDecoratedDragDropOp::Construct();

	CurrentHoverText = FText::FromString( Item->GetName() );
	CurrentIconBrush = Item->GetIcon( false );
	SetDecoratorVisibility( true );
}

//------------------------------------------------------------------------
// SResourceListItemDropTarget

void SResourceListItemDropTarget::Construct(const FArguments& InArgs )
{
	OnAssetDropped = InArgs._OnAssetDropped;
	OnIsAssetAcceptableForDrop = InArgs._OnIsAssetAcceptableForDrop;

	OnItemDropped = InArgs._OnItemDropped;
	OnIsItemAcceptableForDrop = InArgs._OnIsItemAcceptableForDrop;
	
	SDropTarget::Construct(
		SDropTarget::FArguments()
#if UE_VERSION_OLDER_THAN(5,0,0) //used to only support single object
		.OnDrop( this, &SResourceListItemDropTarget::OnDropped )
#else
		.OnDropped(this, &SResourceListItemDropTarget::OnDropped2)
#endif
		[
		InArgs._Content.Widget
		]);
}

// drop operation entry point
//
FReply SResourceListItemDropTarget::OnDropped2( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	return OnDropped( DragDropEvent.GetOperation() );
}
FReply SResourceListItemDropTarget::OnDropped(TSharedPtr<FDragDropOperation> DragDropOperation)
{
	bool bUnused;

	// asset drop case
	UObject* Object = GetDroppedObject(DragDropOperation, bUnused);	
	if ( Object )
	{
		OnAssetDropped.ExecuteIfBound(Object);
	}

	// item drop case
	FResourceListEditorTreeItem::Ptr Item = GetDroppedItem(DragDropOperation, bUnused);
	if( Item.IsValid())
	{
		OnItemDropped.ExecuteIfBound(Item);
	}
	
	return FReply::Handled();
}

// is this an allowed drop? (even if it's a valid type)
//
bool SResourceListItemDropTarget::OnAllowDrop(TSharedPtr<FDragDropOperation> DragDropOperation) const
{
	bool bUnused = false;

	// asset drop case	
	UObject* Object = GetDroppedObject(DragDropOperation, bUnused);
	if ( Object )
	{
		// Check and see if its valid to drop this object
		if ( OnIsAssetAcceptableForDrop.IsBound() )
		{
			return OnIsAssetAcceptableForDrop.Execute(Object);
		}
		else
		{
			// If no delegate is bound assume its always valid to drop this object
			return true;
		}
	}

	// item drop case
	FResourceListEditorTreeItem::Ptr Item = GetDroppedItem(DragDropOperation, bUnused);
	if( Item.IsValid())
	{
		// Check and see if its valid to drop this object
		if ( OnIsItemAcceptableForDrop.IsBound() )
		{
			return OnIsItemAcceptableForDrop.Execute(Item);
		}
		else
		{
			// If no delegate is bound assume its always valid to drop this object
			return true;
		}
	}
	
	return false;
}

// is the drop payload something we are interested in?
//
bool SResourceListItemDropTarget::OnIsRecognized(TSharedPtr<FDragDropOperation> DragDropOperation) const
{
	bool bRecognizedEventO = false;
	bool bRecognizedEventI = false;

	UObject* Object = GetDroppedObject(DragDropOperation, bRecognizedEventO);
	FResourceListEditorTreeItem::Ptr Item = GetDroppedItem(DragDropOperation, bRecognizedEventI);
	
	return bRecognizedEventO || bRecognizedEventI;
}

// extract the asset being dropped (if it is one)
//
UObject* SResourceListItemDropTarget::GetDroppedObject(TSharedPtr<FDragDropOperation> DragDropOperation, bool& bOutRecognizedEvent) const
{
	bOutRecognizedEvent = false;
	UObject* DroppedObject = NULL;
	
	// Asset being dragged from content browser
	if ( DragDropOperation->IsOfType<FAssetDragDropOp>() )
	{
		bOutRecognizedEvent = true;
		TSharedPtr<FAssetDragDropOp> DragDropOp = StaticCastSharedPtr<FAssetDragDropOp>(DragDropOperation);
		const TArray<FAssetData>& DroppedAssets = DragDropOp->GetAssets();
		
		bool bCanDrop = DroppedAssets.Num() == 1;
		
		if( bCanDrop )
		{
			const FAssetData& AssetData = DroppedAssets[0];
			
			// Make sure the asset is loaded
			DroppedObject = AssetData.GetAsset();
		}
	}

	return DroppedObject;
}

// extract the item being dropped (if it is one)
//
FResourceListEditorTreeItem::Ptr SResourceListItemDropTarget::GetDroppedItem(TSharedPtr<FDragDropOperation> DragDropOperation, bool& bOutRecognizedEvent) const
{
	bOutRecognizedEvent = false;
	FResourceListEditorTreeItem::Ptr DroppedItem;
	
	// Item being dragged from elsewhere in resource list editor
	if ( DragDropOperation->IsOfType<FResourceListDragDrop>() )
	{
		TSharedPtr<FResourceListDragDrop> DragDropOp = StaticCastSharedPtr<FResourceListDragDrop>(DragDropOperation);
		if(DragDropOp->Item.IsValid())
		{
			bOutRecognizedEvent = true;

			//extract item from 
			DroppedItem = DragDropOp->Item;
		}
	}
	
	return DroppedItem;
}


//------------------------------------------------------------------------
// SResourceListTreeRow

void SResourcesListTreeRow::Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& TreeView )
{
	Item = InArgs._Item->AsShared();
	ResourceListEditorWeak = InArgs._ResourceListEditor;

	auto Args = FSuperRowType::FArguments()
		.Style(&APPARANCE_STYLE.GetWidgetStyle<FTableRowStyle>("SceneOutliner.TableViewRow"));
	Args.OnDragDetected( this, &SResourcesListTreeRow::OnDragDetected );
	
	SMultiColumnTableRow< TSharedPtr<FResourceListEditorTreeItem>>::Construct(Args, TreeView);
}

// watch for mouse down so we can detect drag operations beginning
//
FReply SResourcesListTreeRow::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	auto ItemPtr = Item.Pin();
	if (ItemPtr.IsValid())
	{
		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			FReply Reply = SMultiColumnTableRow<TSharedPtr<FResourceListEditorTreeItem>>::OnMouseButtonDown( MyGeometry, MouseEvent );
			
			if (ResourceListEditorWeak.Pin()->IsItemValidDragSource( ItemPtr.Get() ))
			{
				return Reply.DetectDrag( SharedThis(this) , EKeys::LeftMouseButton );
			}
			
			return Reply.PreventThrottling();
		}
	}
	
	return FReply::Handled();
}

// kick off a drag operation
//
FReply SResourcesListTreeRow::OnDragDetected( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	auto ItemPtr = Item.Pin();
	if(MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton )
		&& ResourceListEditorWeak.Pin()->IsItemValidDragSource( ItemPtr.Get() ))
	{
		//build drag payload
		TSharedPtr<FResourceListDragDrop> dnd = MakeShareable( new FResourceListDragDrop( ItemPtr ) );
		return FReply::Handled().BeginDragDrop( dnd.ToSharedRef() );
	}

	return FReply::Unhandled();
}



FReply SResourcesListTreeRow::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	return FReply::Handled();
}

FReply SResourcesListTreeRow::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	//target (this)
	auto pdestination_item = Item.Pin();

	//operation
	TSharedPtr<FDragDropOperation> op = DragDropEvent.GetOperation();
	if(op->IsOfType<FResourceListDragDrop>())
	{
		FResourceListDragDrop* psource = (FResourceListDragDrop*)op.Get();
		if(psource)
		{
			UE_LOG(LogApparance,Display,TEXT("DROPPED ITEM '%s' ONTO '%s'"), *psource->Item->GetName(), *pdestination_item->GetName() );
		}
	}
	return FReply::Handled();
}

// Row accessor handler: get name
//
FText SResourcesListTreeRow::GetRowName( FResourceListEditorTreeItem::Ptr item ) const
{
	if(item->IsVariant())
	{
		return FText::Format( LOCTEXT("ResourceVariantNumber","#{0}"), item->TreeParentIndex + 1 );
	}
	else
	{
		return FText::FromString( item->GetName() );
	}
}

// Row accessor handler: get type
//
FText SResourcesListTreeRow::GetRowType( FResourceListEditorTreeItem::Ptr item ) const
{
	return item->GetAssetTypeName();
}

// Row accessor handler: get icon
//
const FSlateBrush* SResourcesListTreeRow::GetRowIcon( FResourceListEditorTreeItem::Ptr item ) const
{
	return item->GetIcon( false );//item->Flags.bIsExpanded );
}

// Row content dimming for variants
//
FSlateColor SResourcesListTreeRow::GetRowColourAndOpacity( FResourceListEditorTreeItem::Ptr item ) const
{
	TSharedPtr<FResourceListEditor> res_list_editor = ResourceListEditorWeak.Pin();
	if(item->IsDimmed() && item!=res_list_editor->SelectedItem)	//(interferes with selection row highlight)
	{
		return FSlateColor::UseSubduedForeground(); //like in scene outliner
	}
	else
	{
		//TODO: work out why this causes icons to go black on selected rows
		return FSlateColor::UseForeground();
	}	
}

// check new label for a row is allowed
//
bool SResourcesListTreeRow::OnVerifyItemLabelChange( const FText& InLabel, FText& OutErrorMessage, FResourceListEditorTreeItem::Ptr item )
{
	FString text = InLabel.ToString();
	text.TrimStartAndEndInline();

	//empty
	if(text.Len()==0)
	{
		OutErrorMessage = LOCTEXT( "RenameFailed_LeftBlank", "Names cannot be left blank" );
		return false;
	}

	//chars
	if(text.Contains(TEXT("#")))
	{
		OutErrorMessage = LOCTEXT("RenameFailed_UnsupportedCharacters","Unsupported resource descritor characters, '#' is not permitted.");
		return false;
	}

	return true;
}

// perform rename of item
//
void SResourcesListTreeRow::OnItemLabelCommitted(const FText& InLabel, ETextCommit::Type InCommitInfo, FResourceListEditorTreeItem::Ptr item)
{
	item->RenameEntry( InLabel.ToString() );	

	//rename can affect order	
	TSharedPtr<FResourceListEditor> plisteditor = ResourceListEditorWeak.Pin();
	plisteditor->RebuildEntryViewModel(); //ordering needs to be reassigned
}

// Row accessor handler: get description of variants under this entry
//
FText SResourcesListTreeRow::GetRowVariantDescription( FResourceListEditorTreeItem::Ptr item ) const
{
	int variant_count = item->GetVariantCount();
	return FText::Format( LOCTEXT("ResourceVariantEntryDescription", "{0} variant(s)"), variant_count );
}

// Row accessor handler: get description of variants under this entry
//
FText SResourcesListTreeRow::GetRowComponentTypeName( FResourceListEditorTreeItem::Ptr item ) const
{
	UApparanceResourceListEntry_Component* pcrle = Cast< UApparanceResourceListEntry_Component>( item->Resource );
	if(pcrle)
	{
		if(pcrle->ComponentTemplate)
		{
			return FText::FromString( pcrle->ComponentTemplate->GetClass()->GetName() );
		}
	}
	return FText();
}

// Row accessor handler: get resource
//
UObject* SResourcesListTreeRow::GetRowResource( FResourceListEditorTreeItem::Ptr item ) const
{
	return item->GetResource();
}

// Row accessor handler: set resources
void SResourcesListTreeRow::SetRowResource( UObject* new_value, FResourceListEditorTreeItem::Ptr item )
{
	if(item->SetResource( new_value ))
	{
		//update treeview (in case assignment changed semantics of controls, e.g. not want specific type filter)
		TSharedPtr<FResourceListEditor> plisteditor = ResourceListEditorWeak.Pin();
		plisteditor->RebuildEntryViewModel();
	}
}

// Build UI for a specific column of the hierarchy
//
TSharedRef<SWidget> SResourcesListTreeRow::GenerateWidgetForColumn( const FName& ColumnName )
{
	FResourceListEditorTreeItem::Ptr ItemPtr = Item.Pin();
	if (!ItemPtr.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	//------------------------------------------------------------------------
	// names and path
	if( ColumnName == SResourcesListTreeView::ColumnTypes::Path() )
	{
		TSharedPtr<FResourceListEditor> plisteditor = ResourceListEditorWeak.Pin();

		//editable text (to support rename)
		TSharedPtr<SInlineEditableTextBlock> InlineTextBlock = SNew(SInlineEditableTextBlock)
			.Text(this, &SResourcesListTreeRow::GetRowName, ItemPtr )
//			.HighlightText( SceneOutliner.GetFilterHighlightText() )
			.ColorAndOpacity(this, &SResourcesListTreeRow::GetRowColourAndOpacity, ItemPtr)
			.OnTextCommitted(this, &SResourcesListTreeRow::OnItemLabelCommitted, ItemPtr)
			.OnVerifyTextChanged(this, &SResourcesListTreeRow::OnVerifyItemLabelChange, ItemPtr)
//			.IsSelected(FIsSelected::CreateSP(&InRow, &SResourceListTreeRow::IsSelectedExclusively))
			.IsReadOnly_Lambda([Editor = ResourceListEditorWeak, Item = ItemPtr, this]()
			{
				TSharedPtr<FResourceListEditor> ple = Editor.Pin();
				return !ple || !ple->CanRenameItem( Item.Get() );
			})
			;		
		if(plisteditor->CanRenameItem( ItemPtr.Get() ))
		{
			ItemPtr->RenameRequestEvent.BindSP( InlineTextBlock.Get(), &SInlineEditableTextBlock::EnterEditingMode );
		}			

		// The first column gets the tree expansion arrow for this row
		auto ui =
			SNew( SHorizontalBox )
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(6, 0, 0, 0)
			[
				SNew( SExpanderArrow, SharedThis(this) ).IndentAmount(12)
			]
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.FillWidth(1.0f)
			[
				SNew( SResourceListItemDropTarget )
				.OnIsAssetAcceptableForDrop( this, &SResourcesListTreeRow::OnAssetDraggedOverName )
				.OnAssetDropped( this, &SResourcesListTreeRow::OnAssetDroppedName )
				.OnIsItemAcceptableForDrop( this, &SResourcesListTreeRow::OnItemDraggedOverName )
				.OnItemDropped( this, &SResourcesListTreeRow::OnItemDroppedName )
				[
					SNew( SHorizontalBox )
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoWidth()
					[
						SNew(SImage)
						.Image(this, &SResourcesListTreeRow::GetRowIcon, ItemPtr )
						.ColorAndOpacity(this, &SResourcesListTreeRow::GetRowColourAndOpacity, ItemPtr )
					]
					+SHorizontalBox::Slot()
					.Padding(6,0,0,0)
					.VAlign(VAlign_Center)
					.FillWidth(1.0f)
					[ 
						InlineTextBlock.ToSharedRef()
//						SNew( STextBlock )
//						.Text( this, &SResourcesListTreeRow::GetRowName, ItemPtr )
//						.ColorAndOpacity(this, &SResourcesListTreeRow::GetRowColourAndOpacity, ItemPtr )
					]
				]
			];

			return ui;
	}
	//------------------------------------------------------------------------
	// type
	if( ColumnName == SResourcesListTreeView::ColumnTypes::Type() )
	{
		// The second column displays the resource type
		return
			SNew( SHorizontalBox )
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			[
				SNew( STextBlock )
				.Text( this, &SResourcesListTreeRow::GetRowType, ItemPtr )
				.ColorAndOpacity( FSlateColor::UseSubduedForeground() )	//like in scene outliner
			];
	}	
	//------------------------------------------------------------------------
	// asset information for this resource
	else if( ColumnName == SResourcesListTreeView::ColumnTypes::Asset())
	{
		//has asset?
		if(ItemPtr->Resource.IsValid())
		{		
			if(ItemPtr->Resource->IsA( UApparanceResourceListEntry_Component::StaticClass() ))
			{
				//show component type
				return SNew( SHorizontalBox )
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					[
						SNew( STextBlock )
						.Text( this, &SResourcesListTreeRow::GetRowComponentTypeName, ItemPtr )
//						.ColorAndOpacity( FSlateColor::UseSubduedForeground() )
					];
			}
			else if(ItemPtr->Resource->IsA( UApparanceResourceListEntry_Asset::StaticClass() ))
			{
				// asset
				if (ItemPtr->HasVariants())
				{
					//show variant counter
					return SNew( SHorizontalBox )
						+SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						[
							SNew( STextBlock )
							.Text( this, &SResourcesListTreeRow::GetRowVariantDescription, ItemPtr )
							.ColorAndOpacity( FSlateColor::UseSubduedForeground() )
						];
				}
				else
				{
					//full asset editor
					return SNew( SAssetDropTarget )
#if UE_VERSION_OLDER_THAN(5,0,0) //used to only support single object
						.OnIsAssetAcceptableForDrop( this, &SResourcesListTreeRow::OnAssetDraggedOverSlot )
						.OnAssetDropped( this, &SResourcesListTreeRow::OnAssetDroppedSlot )
#else
						.OnAreAssetsAcceptableForDrop( this, &SResourcesListTreeRow::OnAssetsDraggedOverSlot )
						.OnAssetsDropped( this, &SResourcesListTreeRow::OnAssetsDroppedSlot )
#endif
						[
							SNew( SHorizontalBox )
							+SHorizontalBox::Slot()
							.VAlign(VAlign_Center)
							.FillWidth(1.0f)
							[
								SNew( SContentReference )
								.AllowSelectingNewAsset(true)
								.AllowClearingReference(true)
								.AllowedClass( ItemPtr->Resource->GetAssetClass() )
								.AssetReference( this, &SResourcesListTreeRow::GetRowResource, ItemPtr )
								.OnSetReference( this, &SResourcesListTreeRow::SetRowResource, ItemPtr )
							]
						];
				}		
			}
		}
		else
		{
			//no editing UI needed for folders/root
			return SNullWidget::NullWidget;
		}
	}

	return SNullWidget::NullWidget;
}

// asset dragged over hierarchy entry
//
bool SResourcesListTreeRow::OnAssetDraggedOverName( const UObject* asset )
{
	//source
	const UClass* asset_class = asset->GetClass();

	//target
	FResourceListEditorTreeItem::Ptr item = Item.Pin();

	TSharedPtr<FResourceListEditor> res_list_editor = ResourceListEditorWeak.Pin();
	const UClass* entry_class = res_list_editor->GetEntryTypeForAsset( asset_class );
	if(entry_class) //only supported types
	{
		FResourceListInsertion insertion = res_list_editor->EvaluateAdd( item.Get(), entry_class );
		if(insertion.IsValid())
		{
			//UE_LOG(LogApparance,Display,TEXT("ASSET %s DRAGGED OVER %s"), *asset->GetName(), *item->GetName() );
			return true;
		}
	}
	return false;
}

// asset dropped on hierarchy entry
//
void SResourcesListTreeRow::OnAssetDroppedName( UObject* asset )
{
	//source
	const UClass* asset_class = asset->GetClass();

	//target
	FResourceListEditorTreeItem::Ptr item = Item.Pin();

	TSharedPtr<FResourceListEditor> res_list_editor = ResourceListEditorWeak.Pin();
	const UClass* entry_class = res_list_editor->GetEntryTypeForAsset( asset_class );
	if(entry_class) //only supported types
	{
		FResourceListInsertion insertion = res_list_editor->EvaluateAdd( item.Get(), entry_class );
		if(insertion.IsValid())
		{
			UE_LOG(LogApparance,Display,TEXT("ASSET %s DROPPED ON %s ENTRY"), *asset->GetName(), *item->GetName() );
			res_list_editor->InsertEntry( insertion, asset );
		}
	}
}

// item dragged over entry slot from somewhere else in the hierarchy
//
bool SResourcesListTreeRow::OnItemDraggedOverName( FResourceListEditorTreeItem::Ptr source_item )
{
	//target
	FResourceListEditorTreeItem::Ptr target_item = Item.Pin();

	TSharedPtr<FResourceListEditor> res_list_editor = ResourceListEditorWeak.Pin();
	FResourceListMovement movement = res_list_editor->EvaluateMove( source_item.Get(), target_item.Get() );
	if(movement.IsValid())
	{
		//UE_LOG(LogApparance,Display,TEXT("ASSET %s DRAGGED OVER %s"), *asset->GetName(), *item->GetName() );
		return true;
	}
	return false;
}

// item dropped on entry slot from somewhere else in the hierarchy
//
void SResourcesListTreeRow::OnItemDroppedName( FResourceListEditorTreeItem::Ptr source_item )
{
	//target
	FResourceListEditorTreeItem::Ptr target_item = Item.Pin();
	
	TSharedPtr<FResourceListEditor> res_list_editor = ResourceListEditorWeak.Pin();
	FResourceListMovement movement = res_list_editor->EvaluateMove( source_item.Get(), target_item.Get() );
	if(movement.IsValid())
	{
		UE_LOG(LogApparance,Display,TEXT("ITEM %s DROPPED ON %s ENTRY"), *source_item->GetName(), *target_item->GetName() );
		res_list_editor->MoveEntry( movement );
	}
}

// assets dragged over entry slot
//
bool SResourcesListTreeRow::OnAssetsDraggedOverSlot( TArrayView<FAssetData> assets )
{
	for(FAssetData fad : assets)
	{
		//any invalid? fail the drop
		if(!OnAssetDraggedOverSlot( fad.GetAsset() ))
		{
			return false;
		}
	}
	return true;
}

// asset dragged over entry slot
//
bool SResourcesListTreeRow::OnAssetDraggedOverSlot( const UObject* asset )
{
	//Fix: had null asset just after renaming it and dragging in
	if(!asset)
	{
		return false;
	}

	TSharedPtr<FResourceListEditor> res_list_editor = ResourceListEditorWeak.Pin();

	//source
	const UClass* asset_class = asset->GetClass();

	//source must be one we can support
	if(res_list_editor->GetEntryTypeForAsset( asset_class )==nullptr)
	{
		return false;
	}
	
	//target
	FResourceListEditorTreeItem::Ptr item = Item.Pin();
	if(item->GetAssetType()==nullptr)
	{
		return false;
	}

	//yup, we can drop, even if it means converting the type
	return true;
}

// assets dropped on entry slot
//
void SResourcesListTreeRow::OnAssetsDroppedSlot( const FDragDropEvent& Event, TArrayView<FAssetData> Assets )
{
	//TODO: support drop of more than one asset at a time
	if(Assets.Num() > 0)
	{
		OnAssetDroppedSlot( Assets[0].GetAsset() );
	}
}

// asset dropped on entry slot
//
void SResourcesListTreeRow::OnAssetDroppedSlot( UObject* asset )
{
	//target
	FResourceListEditorTreeItem::Ptr item = Item.Pin();
	
//	UE_LOG(LogApparance,Display,TEXT("ASSET %s DROPPED ON %s SLOT"), *asset->GetName(), *item->GetName() );
	if(item->SetResource( asset ))
	{
		//update treeview (in case assignment changed semantics of controls, e.g. not want specific type filter)
		TSharedPtr<FResourceListEditor> plisteditor = ResourceListEditorWeak.Pin();
		plisteditor->RebuildEntryViewModel();
	}
}
