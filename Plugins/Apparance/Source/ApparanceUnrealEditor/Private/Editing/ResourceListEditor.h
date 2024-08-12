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
#include "IPropertyTypeCustomization.h"
#include "IDetailCustomization.h"
#include "DragAndDrop/DecoratedDragDropOp.h"
#include "SDropTarget.h"

//module
#include "ApparanceUnrealVersioning.h"
//#include "RemoteEditing.h"

// view model for an resource list entry (the model)
//
struct FResourceListEditorTreeItem : public TSharedFromThis<FResourceListEditorTreeItem>
{
	//types
	typedef TSharedPtr<FResourceListEditorTreeItem> Ptr;
	typedef TSharedRef<FResourceListEditorTreeItem> Ref;
	/** Delegate for hooking up an inline editable text block to be notified that a rename is requested. */
	DECLARE_DELEGATE( FOnRenameRequest );
	
	//our containing editor panel
	class FResourceListEditor* Editor;

	//the model we are the view model for
	TWeakObjectPtr<class UApparanceResourceListEntry> Resource;

	//parent item in hierarchy
	FResourceListEditorTreeItem* Parent;	//(not TSharedPtr as existence is implied, don't want loops)
	int TreeParentIndex;

	//missing asset info (only used for missing asset hierarchy)
	FString NameOverride;
	bool bMissingAssetEntry;
	bool bMissingAssetIsCompatible;

	//hierarchy to match the models hierarchy
	TArray< FResourceListEditorTreeItem::Ptr > Children;

	//editing
	FOnRenameRequest RenameRequestEvent;

	//init
	FResourceListEditorTreeItem( class FResourceListEditor* peditor )
		: Editor( peditor )
		, Resource( nullptr )
		, Parent( nullptr )
		, TreeParentIndex( 0 )
		, bMissingAssetEntry( false )
		, bMissingAssetIsCompatible( false )
	{}
	FResourceListEditorTreeItem( class FResourceListEditor* peditor, UApparanceResourceListEntry* presource, FResourceListEditorTreeItem::Ptr parent, int tree_parent_index )
		: Editor( peditor )
		, Resource( presource )
		, Parent( parent.Get() )
		, TreeParentIndex( tree_parent_index )
		, bMissingAssetEntry( false )
		, bMissingAssetIsCompatible( false )
	{}

	//access
	FString GetName() const;
	bool IsDimmed() const { return IsVariant() || (IsMissingAsset() && !bMissingAssetIsCompatible); }
	FText GetAssetTypeName() const;
	const FSlateBrush* GetIcon(bool is_open) const;
	bool HasVariants() const;
	int GetVariantCount() const;
	bool IsVariant() const { return Parent && Parent->HasVariants(); }
	bool IsAsset() const;
	bool IsResourceList() const;
	UClass* GetResourceEntryType() const;
	UClass* GetAssetType() const;
	FResourceListEditorTreeItem::Ptr FindEntry( UObject* asset );
	bool IsReference() const;
	bool IsMissingAsset() const { return bMissingAssetEntry; }
		
	//editing
	UObject* GetResource();
	bool SetResource( UObject* passet );
	void RenameEntry( FString new_name );
	void Sync(class UApparanceResourceListEntry* proot );
	void Sync(class UApparanceResourceListEntry* pentry, FResourceListEditorTreeItem::Ptr parent, int tree_parent_index );
	void SyncMissing( const TArray<FString> missing_asset_descriptors, FString primary_category );
	void ToggleVariants();
	void ChangeEntryType( const UClass* resource_list_entry_type );


private:
	void Sort();
	static void Sort( TArray<UApparanceResourceListEntry*>& entries );

};

// encapsulated insertion operation info
//
struct FResourceListInsertion
{
	FResourceListEditorTreeItem::Ptr Container;
	int TreeParentIndex;
	
	//restrictions
	const UClass* AllowedType;
	
	FResourceListInsertion( FResourceListEditorTreeItem::Ptr container=nullptr, int tree_parent_index=-1, const UClass* allowed_type=nullptr )
		: Container( container )
		, TreeParentIndex( tree_parent_index )
		, AllowedType( allowed_type )
	{}
	
	FResourceListInsertion( const UClass* allowed_type )
	: Container( nullptr )
		, TreeParentIndex( -1 )
		, AllowedType( allowed_type )
	{}
	
	bool IsInsertionValid() { return Container.IsValid(); }
	bool IsTypeValid() { return AllowedType!=nullptr; }
	bool IsValid() { return IsInsertionValid() && IsTypeValid(); }
};

// encapsulated movement operation info
//
struct FResourceListMovement
{
	FResourceListEditorTreeItem::Ptr SourceContainer;
	int SourceTreeParentIndex;
	FResourceListEditorTreeItem::Ptr DestinationContainer;
	int DestinationTreeParentIndex;
	
	FResourceListMovement( FResourceListEditorTreeItem::Ptr source_container=nullptr, int source_tree_parent_index=-1, FResourceListEditorTreeItem::Ptr destination_container=nullptr, int destination_tree_parent_index=-1 )
		: SourceContainer( source_container )
		, SourceTreeParentIndex( source_tree_parent_index )
		, DestinationContainer( destination_container )
		, DestinationTreeParentIndex( destination_tree_parent_index )
	{}
	
	bool IsSourceValid() { return SourceContainer.IsValid() && SourceTreeParentIndex!=-1; }
	bool IsDestinationValid() { return DestinationContainer.IsValid() && DestinationTreeParentIndex!=-1; }
	bool IsValid() { return IsSourceValid() && IsDestinationValid(); }
};
	
/// <summary>
/// Fully custom editing panel for resource lists
/// </summary>
class FResourceListEditor
	: public FAssetEditorToolkit
	, public FEditorUndoClient
{
	
	/** ResourceList Editor app identifier string */
	static const FName ResourceListEditorAppIdentifier;
	
	/**	The tab id for the resource hierarchy tab */
	static const FName ResourceListHierarchyTabId;

	/**	The tab id for the resource properties tab */
	static const FName ResourceListDetailsTabId;
	
	/** The workspace menu category of this toolkit */
	TSharedPtr<FWorkspaceItem> WorkspaceMenuCategory;
	
	/** Property viewing widget */
	TSharedPtr<class IDetailsView> DetailsView;

	/** UI for the "Resource List Hierarchy" tab */
	TSharedPtr<SWidget> ResourceListHierarchyTabWidget;
	
	// UI for treeview
	TSharedPtr< STreeView< TSharedPtr<FResourceListEditorTreeItem>> > TreeView;

	// root of view model
	TArray<FResourceListEditorTreeItem::Ptr> TreeRoot;

	// root of the resource hierarchy view model
	FResourceListEditorTreeItem::Ptr ResourcesRoot;
	
	// root of the references hierarchy view model
	FResourceListEditorTreeItem::Ptr ReferencesRoot;
	
	// root of the missing assets hierarchy view model
	FResourceListEditorTreeItem::Ptr MissingRoot;
	
	// currently selected item
	FResourceListEditorTreeItem::Ptr SelectedItem;

	// track count of missing assets to spot changes
	int CurrentMissingCount;

	// tick management
	FTickerDelegate TickDelegate;
	TICKER_HANDLE TickDelegateHandle;

	// deferred selection (by entry or item)
	TWeakObjectPtr<UApparanceResourceListEntry> DeferredSelectionEntry;
	FResourceListEditorTreeItem::Ptr DeferredSelectionItem;
	bool bForceApplySelection;
	
public:
	//setup
	FResourceListEditor();
	virtual ~FResourceListEditor();
	void InitResourceListEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, class UApparanceResourceList* res_list );

	//access
	FResourceListEditorTreeItem::Ptr GetSelectedItem() const { return SelectedItem; }
	
	/** FAssetEditorToolkit interface */
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	
	/** IToolkit interface */
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;

	/** FEditorUndoClient */
	virtual bool MatchesContext( const FTransactionContext& InContext, const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjectContexts ) const;
	virtual void PostUndo( bool bSuccess ) override;
	virtual void PostRedo( bool bSuccess ) override;

	//utility
	static const UClass* GetEntryTypeForAsset( const UClass* passet_class );
	UApparanceResourceListEntry* CreateEntry( const UClass* entry_type ) const;

private:

	//access
	class UApparanceResourceList* GetEditableResourceList() const;
	FResourceListEditorTreeItem::Ptr FindHierarchyEntry( UObject* entry_or_asset );
	FResourceListInsertion EvaluateAdd( FResourceListEditorTreeItem* item, const UClass* asset_class ) const;
	FResourceListMovement EvaluateMove( FResourceListEditorTreeItem* source_item, FResourceListEditorTreeItem* target_item ) const;
	
	//ui setup
	void CreateAndRegisterResourceListHierarchyTab(const TSharedRef<class FTabManager>& InTabManager);
	void CreateAndRegisterResourceListDetailsTab(const TSharedRef<class FTabManager>& InTabManager);
	TSharedRef<SDockTab> SpawnTab_ResourceListHierarchy( const FSpawnTabArgs& Args );
	TSharedRef<SDockTab> SpawnTab_ResourceListDetails(const FSpawnTabArgs& Args);
	TSharedRef<SVerticalBox> CreateResourceListHierarchyUI();
	void ExtendToolbar(TSharedPtr<FExtender> Extender);	
	void FillToolbar(FToolBarBuilder& ToolbarBuilder);

	//idle/deferred updates
	bool Tick(float DeltaTime);
	
	//editing via ui
	bool CheckAddFolderAllowed() const;
	void OnAddFolderClicked();
	bool CheckAddResourceAllowed() const;
	void OnAddResourceClicked();
	void AddChosenResource( const UClass* entry_type );
	void ChangeEntryType( const UClass* entry_type);
	bool CheckRemoveItemAllowed() const;
	void OnRemoveItemClicked();
	bool CheckToggleVariantsAllowed() const;
	void OnToggleVariantsClicked();
	bool CheckToggleHelpAllowed();
	void OnToggleHelpClicked();
	void ShowAssetTypeContextMenu();
	void SetSelection( FResourceListEditorTreeItem::Ptr Item );
	bool IsUnderResourceRoot( FResourceListEditorTreeItem* item ) const;
	bool IsUnderReferencesRoot( FResourceListEditorTreeItem* item ) const;
	bool IsUnderMissingRoot( FResourceListEditorTreeItem* item ) const;
	bool IsUnderResourceRootOrUnselected( FResourceListEditorTreeItem* item ) const;
	bool IsUnderReferencesRootOrUnselected( FResourceListEditorTreeItem* item ) const;
	bool IsUnderMissingRootOrUnselected( FResourceListEditorTreeItem* item ) const;
	bool CanRenameItem( FResourceListEditorTreeItem* item ) const;
	bool CanDeleteItem( FResourceListEditorTreeItem* item ) const;
	bool IsItemValidDragSource( FResourceListEditorTreeItem* item ) const;

	//editing data
	UApparanceResourceListEntry* CreateAssetEntry( UObject* passet ) const;
	void InsertEntry( FResourceListInsertion insertion, UObject* explicit_entry_or_asset=nullptr );
	void MoveEntry( FResourceListMovement movement );
	UObject* RemoveEntry( FResourceListEditorTreeItem::Ptr container, int tree_parent_index );
	void OnUndoRedo();
	void InvalidateAssetCache();
	
	//treeview
	TSharedRef<ITableRow> GenerateTreeRow( TSharedPtr<FResourceListEditorTreeItem> TreeItem, const TSharedRef<STableViewBase>& OwnerTable );
	void GetChildrenForTree( TSharedPtr< FResourceListEditorTreeItem > TreeItem, TArray< TSharedPtr<FResourceListEditorTreeItem> >& OutChildren );
	void RebuildEntryViewModel( bool track_item_instead_of_entry=false );
	void RefreshTreeview(); //update Slate-side when our viewmodel has chnaged
	FResourceListEditorTreeItem::Ptr BuildEntryViewModel( UApparanceResourceListEntry* pentry );
	void OnTreeViewSelect( FResourceListEditorTreeItem::Ptr Item, ESelectInfo::Type SelectInfo );
	void DeferredSelect( TWeakObjectPtr<UApparanceResourceListEntry> selected_entry );
	void DeferredSelect( FResourceListEditorTreeItem::Ptr selected_item );
	TSharedPtr<SWidget> GenerateTreeContextMenu();
		
	//notifications
	void OnHierarchyTabActivated(TSharedRef<SDockTab> tab, ETabActivationCause cause);
	void OnResourceListStructuralChange( UApparanceResourceList* pres_list, UApparanceResourceListEntry* pentry, FName property );
	void OnResourceListAssetChange( UApparanceResourceList* pres_list, UApparanceResourceListEntry* pentry, UObject* pnew_asset );

	friend class SResourcesListTreeView;
	friend class SResourcesListTreeRow;
};


//------------------------------------------------------------------------
// tree view
//
class SResourcesListTreeView : public STreeView< TSharedPtr<FResourceListEditorTreeItem> >
{
	FResourceListEditorTreeItem::Ptr PendingRenameItem;

protected:
	
	/** Weak reference to the outliner widget that owns this list */
	TWeakPtr<FResourceListEditor> ResourceListEditorWeak;
	
public:
	//static info
	struct ColumnTypes
	{	
		static const FName& Path()
		{
			static FName PathCol("Path");
			return PathCol;
		}
		
		static FName& Type()
		{
			static FName TypeCol("Type");
			return TypeCol;
		}
		
		static FName& Asset()
		{
			static FName AssetCol("Asset");
			return AssetCol;
		}
	};

public:
	
	/** Construct this widget */
	void Construct(const FArguments& InArgs, TSharedRef<FResourceListEditor> Owner);	

	/** STreeView */
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;
		
	//access
	TWeakPtr<FResourceListEditor> GetResourceListEditor() { return ResourceListEditorWeak; }

	
};

//------------------------------------------------------------------------
// drag and drop payload
//
class FResourceListDragDrop : public FDecoratedDragDropOp
{
public:
	FResourceListEditorTreeItem::Ptr Item;

public:
	DRAG_DROP_OPERATOR_TYPE(FResourceListDragDrop, FDragDropOperation)	
	
	FResourceListDragDrop( FResourceListEditorTreeItem::Ptr dragged_item );
	virtual void Construct() override;
	virtual ~FResourceListDragDrop() {}
		
	// FDragDropOperation interface
#if 0
	virtual void OnDrop( bool bDropWasHandled, const FPointerEvent& MouseEvent ) override { Impl->OnDrop(bDropWasHandled, MouseEvent); }
	virtual void OnDragged( const class FDragDropEvent& DragDropEvent ) override { Impl->OnDragged(DragDropEvent); }
	virtual FCursorReply OnCursorQuery() override { return Impl->OnCursorQuery(); }
	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override { return Impl->GetDefaultDecorator(); }
	virtual FVector2D GetDecoratorPosition() const override { return Impl->GetDecoratorPosition(); }
	virtual void SetDecoratorVisibility(bool bVisible) override { Impl->SetDecoratorVisibility(bVisible); }
	virtual bool IsExternalOperation() const override { return Impl->IsExternalOperation(); }
	virtual bool IsWindowlessOperation() const override { return Impl->IsWindowlessOperation(); }
#endif
	// End of FDragDropOperation interface
		
private:
	//TSharedPtr<FResourceListDragDrop> Impl;
};


/**
* A widget that displays a hover cue and handles dropping assets of allowed types onto this widget
*/
class SResourceListItemDropTarget : public SDropTarget
{
public:
	/** Called when a valid asset is dropped */
	DECLARE_DELEGATE_OneParam( FOnAssetDropped, UObject* );
	
	/** Called when we need to check if an asset type is valid for dropping */
	DECLARE_DELEGATE_RetVal_OneParam( bool, FIsAssetAcceptableForDrop, const UObject* );

	/** Called when a valid item is dropped */
	DECLARE_DELEGATE_OneParam( FOnItemDropped, FResourceListEditorTreeItem::Ptr );
	
	/** Called when we need to check if an asset type is valid for dropping */
	DECLARE_DELEGATE_RetVal_OneParam( bool, FIsItemAcceptableForDrop, FResourceListEditorTreeItem::Ptr );
	
	SLATE_BEGIN_ARGS(SResourceListItemDropTarget)
	{ }
	/* Content to display for the in the drop target */
	SLATE_DEFAULT_SLOT( FArguments, Content )
	/** Called when a valid asset is dropped */
	SLATE_EVENT( FOnAssetDropped, OnAssetDropped )
	/** Called to check if an asset is acceptible for dropping */
	SLATE_EVENT( FIsAssetAcceptableForDrop, OnIsAssetAcceptableForDrop )
	/** Called when a valid item is dropped */
	SLATE_EVENT( FOnItemDropped, OnItemDropped )
	/** Called to check if an item is acceptible for dropping */
	SLATE_EVENT( FIsItemAcceptableForDrop, OnIsItemAcceptableForDrop )
	SLATE_END_ARGS()
		
	void Construct(const FArguments& InArgs );
					
protected:
	FReply OnDropped(TSharedPtr<FDragDropOperation> DragDropOperation);
	FReply OnDropped2( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent );
	virtual bool OnAllowDrop(TSharedPtr<FDragDropOperation> DragDropOperation) const override;
	virtual bool OnIsRecognized(TSharedPtr<FDragDropOperation> DragDropOperation) const override;
	
private:
	UObject* GetDroppedObject(TSharedPtr<FDragDropOperation> DragDropOperation, bool& bOutRecognizedEvent) const;
	FResourceListEditorTreeItem::Ptr GetDroppedItem(TSharedPtr<FDragDropOperation> DragDropOperation, bool& bOutRecognizedEvent) const;
	
private:
	/** Delegate to call when an asset is dropped */
	FOnAssetDropped OnAssetDropped;
	/** Delegate to call to check validity of the asset */
	FIsAssetAcceptableForDrop OnIsAssetAcceptableForDrop;
	/** Delegate to call when an item is dropped */
	FOnItemDropped OnItemDropped;
	/** Delegate to call to check validity of the item */
	FIsItemAcceptableForDrop OnIsItemAcceptableForDrop;
};


//------------------------------------------------------------------------
// tree view row
//
/** Widget that represents a row in the outliner's tree control.  Generates widgets for each column on demand. */
class SResourcesListTreeRow
	: public SMultiColumnTableRow< TSharedPtr<FResourceListEditorTreeItem> >
{
	
public:
	
	SLATE_BEGIN_ARGS( SResourcesListTreeRow ) {}
	
	/** The list item for this row */
	SLATE_ARGUMENT( FResourceListEditorTreeItem::Ptr, Item )

	/** The owning object. This allows us access to the actual data table being edited as well as some other API functions. */
	SLATE_ARGUMENT(TSharedPtr<FResourceListEditor>, ResourceListEditor)
			
	SLATE_END_ARGS()
			
			
	/** Construct function for this widget */
	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& treeview );
	
	/** Overridden from SMultiColumnTableRow.  Generates a widget for this column of the tree row. */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override;
			
public:
	
	//editing
	bool OnVerifyItemLabelChange( const FText& InLabel, FText& OutErrorMessage, FResourceListEditorTreeItem::Ptr item );
	void OnItemLabelCommitted(const FText& InLabel, ETextCommit::Type InCommitInfo, FResourceListEditorTreeItem::Ptr item);
	
protected:
	

	/** SWidget interface */
	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;	
	//drag handlers
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;
	FReply OnDragDetected( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent );
		
	//drop handlers
	bool OnAssetDraggedOverName( const UObject* asset );
	void OnAssetDroppedName( UObject* asset );
	bool OnItemDraggedOverName( FResourceListEditorTreeItem::Ptr item );
	void OnItemDroppedName( FResourceListEditorTreeItem::Ptr item );
	bool OnAssetDraggedOverSlot( const UObject* asset );
	bool OnAssetsDraggedOverSlot( TArrayView<FAssetData> assets );
	void OnAssetDroppedSlot( UObject* asset );
	void OnAssetsDroppedSlot( const FDragDropEvent& Event, TArrayView<FAssetData> Assets );

private:
	
	/** Weak reference to the outliner widget that owns our list */
	TWeakPtr< FResourceListEditor > ResourceListEditorWeak;
	
	/** The item associated with this row of data */
	TWeakPtr<FResourceListEditorTreeItem> Item;
	
	//test
	UObject* GetRowResource( FResourceListEditorTreeItem::Ptr item ) const;
	void SetRowResource( UObject* new_value, FResourceListEditorTreeItem::Ptr item );
	FText GetRowName( FResourceListEditorTreeItem::Ptr item ) const;
	FText GetRowType( FResourceListEditorTreeItem::Ptr item ) const;
	const FSlateBrush* GetRowIcon( FResourceListEditorTreeItem::Ptr item ) const;
	FSlateColor GetRowColourAndOpacity( FResourceListEditorTreeItem::Ptr item ) const;
	FText GetRowVariantDescription( FResourceListEditorTreeItem::Ptr item ) const;
	FText GetRowComponentTypeName( FResourceListEditorTreeItem::Ptr item ) const;

			
protected:
	
private:
	
};

