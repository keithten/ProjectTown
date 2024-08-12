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
#include "Widgets/SWidget.h"

//module
#include "EntityEditingToolkit.h"


//------------------------------------------------------------------------
// view model for an entity list entry (the model)
//
struct FEntityEditorTreeItem : public TSharedFromThis<FEntityEditorTreeItem>
{
	//types
	typedef TSharedPtr<FEntityEditorTreeItem> Ptr;
	typedef TSharedRef<FEntityEditorTreeItem> Ref;

	//our containing editor panel
	class SEntityEditor* Editor;

	//the model we are the view model for
	TWeakObjectPtr<class AApparanceEntity> Entity;

	//parent item in hierarchy
	FEntityEditorTreeItem* Parent;	//(not TSharedPtr as existence is implied, don't want loops)
	int TreeParentIndex;

	//hierarchy, when we support it
	TArray<FEntityEditorTreeItem::Ptr > Children;

	//init
	FEntityEditorTreeItem( class SEntityEditor* peditor )
		: Editor( peditor )
		, Entity( nullptr )
		, Parent( nullptr )
		, TreeParentIndex( 0 )
	{}
	FEntityEditorTreeItem( class SEntityEditor* peditor, AApparanceEntity* pentity, FEntityEditorTreeItem::Ptr parent, int tree_parent_index )
		: Editor( peditor )
		, Entity( pentity )
		, Parent( parent.Get() )
		, TreeParentIndex( tree_parent_index )
	{}

	//access
	FString GetName() const;
	const FSlateBrush* GetIcon( bool is_open ) const;
	FEntityEditorTreeItem::Ptr FindEntry( UObject* actor );
	bool IsDimmed() const;
	bool IsEditingAvailable() const;

	//editing
	class AApparanceEntity* GetEntity() const { return Entity.Get(); }
	bool SetEntity( AApparanceEntity* pentity );

private:

};


//------------------------------------------------------------------------
// UI for entity editing mode panel and tooling
//
class SEntityEditor 
	: public SCompoundWidget
	, public FEditorUndoClient
{
private:
	//setup
	TWeakPtr<FEntityEditingToolkit> ParentToolkit;

	//the world being shown
	UWorld* RepresentingWorld;

	//state
	/** True if the outliner needs to be repopulated at the next appropriate opportunity, usually because our
		actor set has changed in some way. */
	uint8 bNeedsRefresh : 1;
	/** true if the Scene Outliner should do a full refresh. */
	uint8 bFullRefresh : 1;
	/** true when the actor selection state in the world does not match the selection state of the tree */
	uint8 bActorSelectionDirty : 1;
	// resync needed to make sure all entities are in correct mode
	uint8 bResyncMode : 1;
	/** Reentrancy guard */
	bool bIsReentrant;

	//tree
	TSharedPtr<STreeView<FEntityEditorTreeItem::Ptr>> TreeView;
	TArray<FEntityEditorTreeItem::Ptr> TreeRoots;
	TMap<AApparanceEntity*, FEntityEditorTreeItem::Ptr> TreeItemMap;

	//UI
	const FSlateBrush* NoBorder;


public:
	SLATE_BEGIN_ARGS( SEntityEditor ) {}
	SLATE_END_ARGS();

	void Construct( const FArguments& InArgs, TSharedRef<FEntityEditingToolkit> InParentToolkit );
	void Populate();
	~SEntityEditor();

	// SWidget interface
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

	//utility
	// Check that we are reflecting a valid world
	bool CheckWorld() const { return RepresentingWorld!=nullptr; }
	//sub-modes
	class FEntityEditingMode* GetEdMode() const;

	//FEditorUndoClient
	virtual void PostUndo( bool bSuccess ) override {};
	virtual void PostRedo( bool bSuccess ) override {};

private:
	//tree
	void EmptyTreeItems();
	void RepopulateEntireTree();
	void OnTreeViewSelect( FEntityEditorTreeItem::Ptr TreeItem, ESelectInfo::Type SelectInfo );
	void SynchronizeActorSelection();

	/** Level, editor and other global event hooks required to keep the outliner up to date */
	void OnLevelSelectionChanged( UObject* Obj );
	void OnMapChange( uint32 MapFlags );
	void OnNewCurrentLevel();
	void OnLevelActorListChanged();
	void OnLevelAdded( ULevel* InLevel, UWorld* InWorld );
	void OnLevelRemoved( ULevel* InLevel, UWorld* InWorld );
	void OnLevelActorsAdded( AActor* InActor );
	void OnLevelActorsRemoved( AActor* InActor );
	//void OnLevelActorsRequestRename( const AActor* InActor );
	void OnActorLabelChanged( AActor* ChangedActor );
	void OnAssetReloaded( const EPackageReloadPhase InPackageReloadPhase, FPackageReloadedEvent* InPackageReloadedEvent );
	void OnEditCutActorsBegin();
	void OnEditCutActorsEnd();
	void OnEditCopyActorsBegin();
	void OnEditCopyActorsEnd();
	void OnEditPasteActorsBegin();
	void OnEditPasteActorsEnd();
	void OnDuplicateActorsBegin();
	void OnDuplicateActorsEnd();
	void OnDeleteActorsBegin();
	void OnDeleteActorsEnd();

	//UI
	TSharedRef<SVerticalBox> CreateEntityTreeUI();
	void GetChildrenForTree( TSharedPtr< FEntityEditorTreeItem > TreeItem, TArray< FEntityEditorTreeItem::Ptr >& OutChildren );
	TSharedRef<ITableRow> GenerateTreeRow(FEntityEditorTreeItem::Ptr TreeItem, const TSharedRef<STableViewBase>& OwnerTable );
	//interaction
	void HandleEditControlCheckStateChanged( ECheckBoxState NewState );
	ECheckBoxState GetEditControlCheckState() const;
};


//------------------------------------------------------------------------
// tree view
//
class SEntityEditorTreeView : public STreeView< FEntityEditorTreeItem::Ptr >
{

protected:

	/** Weak reference to the editor widget that owns this list */
	TWeakPtr<SEntityEditor> EntityEditorWeak;

public:
	//static info
	struct ColumnTypes
	{
		static const FName& Actors()
		{
			static FName ActorCol( "Entities" );
			return ActorCol;
		}

	};

public:

	/** Construct this widget */
	void Construct( const FArguments& InArgs, TSharedRef<SEntityEditor> Owner );

	//access
	TWeakPtr<SEntityEditor> GetEntityEditor() { return EntityEditorWeak; }


};



//------------------------------------------------------------------------
// tree view row
//
/** Widget that represents a row in the outliner's tree control.  Generates widgets for each column on demand. */
class SEntityEditorTreeRow
	: public SMultiColumnTableRow< FEntityEditorTreeItem::Ptr >
{

public:

	SLATE_BEGIN_ARGS( SEntityEditorTreeRow ) {}

	/** The list item for this row */
	SLATE_ARGUMENT( FEntityEditorTreeItem::Ptr, Item )

	/** The owning object. This allows us access to the actual data table being edited as well as some other API functions. */
	SLATE_ARGUMENT( TSharedPtr<SEntityEditor>, EntityEditor )

	SLATE_END_ARGS()


	/** Construct function for this widget */
	void Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& treeview );

	/** Overridden from SMultiColumnTableRow.  Generates a widget for this column of the tree row. */
	virtual TSharedRef<SWidget> GenerateWidgetForColumn( const FName& ColumnName ) override;

	// ui state
	FSlateColor GetRowColourAndOpacity( FEntityEditorTreeItem::Ptr item ) const;


protected:


private:

	/** Weak reference to the outliner widget that owns our list */
	TWeakPtr<SEntityEditor> EntityEditorWeak;

	/** The item associated with this row of data */
	TWeakPtr<FEntityEditorTreeItem> Item;

	//test
	AApparanceEntity* GetRowEntity( FEntityEditorTreeItem::Ptr item ) const;
	void SetRowEntity( AApparanceEntity* new_value, FEntityEditorTreeItem::Ptr item );
	FText GetRowName( FEntityEditorTreeItem::Ptr item ) const;
	//const FSlateBrush* GetRowIcon( FResourceListEditorTreeItem::Ptr item ) const;
	//FSlateColor GetRowColourAndOpacity( FResourceListEditorTreeItem::Ptr item ) const;


protected:

private:

};
