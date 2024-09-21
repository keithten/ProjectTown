//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

//main
#include "SEntityEditor.h"
#include "ApparanceUnrealVersioning.h"
#include "EntityEditingMode.h"
#include "EntityEditingToolkit.h"

//unreal
//#include "Core.h"
#include "Editor.h"
#include "EditorModeManager.h"
#include "UnrealEdGlobals.h" //for GUnrealEd
#include "Editor/UnrealEdEngine.h" //for GUnrealEd
#include "Engine/Selection.h"
#include "EngineUtils.h"
//unreal:ui
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#if UE_VERSION_AT_LEAST( 5, 1, 0 )
#include "EditorStyleSet.h"
#endif

//module
#include "ApparanceUnreal.h"
#include "ApparanceUnrealEditor.h"
#include "ApparanceEntity.h"

#define LOCTEXT_NAMESPACE "ApparanceUnreal"



// main panel UI setup
//
void SEntityEditor::Construct( const FArguments& InArgs, TSharedRef<FEntityEditingToolkit> InParentToolkit )
{
	ParentToolkit = InParentToolkit;

	NoBorder = APPARANCE_STYLE.GetBrush( "LevelViewport.NoViewportBorder" );

	//main container
	TSharedRef<SVerticalBox> VerticalBox = SNew( SVerticalBox );
	ChildSlot
	[
		SNew( SBorder )
		.BorderImage( NoBorder )
		.BorderBackgroundColor( FLinearColor::Transparent )
		//.ShowEffectWhenDisabled( false )
		[
			VerticalBox
		]
	];

	//main edit mode controls
	VerticalBox->AddSlot()
	.Padding(FMargin(10, 5))
	.AutoHeight()
	[
		SNew( SCheckBox )	
		.Style( APPARANCE_STYLE, "ToggleButtonCheckbox" )
		//.ToolTipText( this, &SAssetPicker::GetShowOtherDevelopersToolTip )
		.OnCheckStateChanged( this, &SEntityEditor::HandleEditControlCheckStateChanged )
		.IsChecked( this, &SEntityEditor::GetEditControlCheckState )
		.HAlign( HAlign_Center )
		[
			SNew(SBox)
			.HeightOverride(30)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ApparanceEditingControl","Enable Editing"))			
				//SNew( SImage )
				//.Image( FEditorStyle::GetBrush( "ContentBrowser.ColumnViewDeveloperFolderIcon" ) )
			]
		]

	];

	//treeview
	VerticalBox->AddSlot()
	.FillHeight( 1.0f )
	[
		CreateEntityTreeUI()
	];

	// Populate our data set
	Populate();

	// track selection
	OnLevelSelectionChanged( NULL );
	USelection::SelectionChangedEvent.AddRaw( this, &SEntityEditor::OnLevelSelectionChanged );
	USelection::SelectObjectEvent.AddRaw( this, &SEntityEditor::OnLevelSelectionChanged );

	// track actor add/remove/etc
	FEditorDelegates::MapChange.AddSP( this, &SEntityEditor::OnMapChange );
	FEditorDelegates::NewCurrentLevel.AddSP( this, &SEntityEditor::OnNewCurrentLevel );
	GEngine->OnLevelActorListChanged().AddSP( this, &SEntityEditor::OnLevelActorListChanged );
	FWorldDelegates::LevelAddedToWorld.AddSP( this, &SEntityEditor::OnLevelAdded );
	FWorldDelegates::LevelRemovedFromWorld.AddSP( this, &SEntityEditor::OnLevelRemoved );
	FCoreUObjectDelegates::OnPackageReloaded.AddRaw( this, &SEntityEditor::OnAssetReloaded );
	GEngine->OnLevelActorAdded().AddSP( this, &SEntityEditor::OnLevelActorsAdded );
	GEngine->OnLevelActorDeleted().AddSP( this, &SEntityEditor::OnLevelActorsRemoved );
	//GEngine->OnLevelActorRequestRename().AddSP( this, &SEntityEditor::OnLevelActorsRequestRename );

	// track undo/redo
	GEditor->RegisterForUndo( this );

	// track renames
	FCoreDelegates::OnActorLabelChanged.AddRaw( this, &SEntityEditor::OnActorLabelChanged );

	// track cut/copy/paste/etc
	FEditorDelegates::OnEditCutActorsBegin.AddSP( this, &SEntityEditor::OnEditCutActorsBegin );
	FEditorDelegates::OnEditCutActorsEnd.AddSP( this, &SEntityEditor::OnEditCutActorsEnd );
	FEditorDelegates::OnEditCopyActorsBegin.AddSP( this, &SEntityEditor::OnEditCopyActorsBegin );
	FEditorDelegates::OnEditCopyActorsEnd.AddSP( this, &SEntityEditor::OnEditCopyActorsEnd );
	FEditorDelegates::OnEditPasteActorsBegin.AddSP( this, &SEntityEditor::OnEditPasteActorsBegin );
	FEditorDelegates::OnEditPasteActorsEnd.AddSP( this, &SEntityEditor::OnEditPasteActorsEnd );
	FEditorDelegates::OnDuplicateActorsBegin.AddSP( this, &SEntityEditor::OnDuplicateActorsBegin );
	FEditorDelegates::OnDuplicateActorsEnd.AddSP( this, &SEntityEditor::OnDuplicateActorsEnd );
	FEditorDelegates::OnDeleteActorsBegin.AddSP( this, &SEntityEditor::OnDeleteActorsBegin );
	FEditorDelegates::OnDeleteActorsEnd.AddSP( this, &SEntityEditor::OnDeleteActorsEnd );

	//other vars
	bNeedsRefresh = true;
	bFullRefresh = true;
	bIsReentrant = false;
	bResyncMode = false;
	bActorSelectionDirty = false;
}


// main panel shutdown
//
SEntityEditor::~SEntityEditor()
{
	// stop tracking selection
	USelection::SelectionChangedEvent.RemoveAll( this );
	USelection::SelectObjectEvent.RemoveAll( this );

	// stop tracking actor add/remove/etc
	FEditorDelegates::MapChange.RemoveAll( this );
	FEditorDelegates::NewCurrentLevel.RemoveAll( this );
	if(GEngine)
	{
		GEngine->OnLevelActorListChanged().RemoveAll( this );
	}
	FWorldDelegates::LevelAddedToWorld.RemoveAll( this );
	FWorldDelegates::LevelRemovedFromWorld.RemoveAll( this );
	FCoreUObjectDelegates::OnPackageReloaded.RemoveAll( this );
	if(GEngine)
	{	
		GEngine->OnLevelActorAdded().RemoveAll( this );
		GEngine->OnLevelActorDeleted().RemoveAll( this );
		//GEngine->OnLevelActorRequestRename().RemoveAll( this );

		// stop tracking undo/redo
		GEditor->UnregisterForUndo( this );
	}

	// stop tracking renames
	FCoreDelegates::OnActorLabelChanged.RemoveAll( this );

	// stop tracking cut/copy/paste/etc
	FEditorDelegates::OnEditCutActorsBegin.RemoveAll( this );
	FEditorDelegates::OnEditCutActorsEnd.RemoveAll( this );
	FEditorDelegates::OnEditCopyActorsBegin.RemoveAll( this );
	FEditorDelegates::OnEditCopyActorsEnd.RemoveAll( this );
	FEditorDelegates::OnEditPasteActorsBegin.RemoveAll( this );
	FEditorDelegates::OnEditPasteActorsEnd.RemoveAll( this );
	FEditorDelegates::OnDuplicateActorsBegin.RemoveAll( this );
	FEditorDelegates::OnDuplicateActorsEnd.RemoveAll( this );
	FEditorDelegates::OnDeleteActorsBegin.RemoveAll( this );
	FEditorDelegates::OnDeleteActorsEnd.RemoveAll( this );
}


// hierarchy UI creation
//
TSharedRef<SVerticalBox> SEntityEditor::CreateEntityTreeUI()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SAssignNew(TreeView, SEntityEditorTreeView, SharedThis(this))
			.TreeItemsSource(&TreeRoots)
			.OnGenerateRow(this, &SEntityEditor::GenerateTreeRow)
//			.OnItemScrolledIntoView(TreeView, &SResourcesListTreeView::OnTreeItemScrolledIntoView)
			.ItemHeight(21)
//			.SelectionMode(InArgs._SelectionMode)
			.OnSelectionChanged(this, &SEntityEditor::OnTreeViewSelect)
//			.OnExpansionChanged(this, &SPathView::TreeExpansionChanged)
			.OnGetChildren(this, &SEntityEditor::GetChildrenForTree)
//			.OnSetExpansionRecursive(this, &SPathView::SetTreeItemExpansionRecursive)
			//.OnContextMenuOpening(this, &FResourceListEditor::GenerateTreeContextMenu)
			.ClearSelectionOnClick(true)
			.HighlightParentNodesForSelection(true)				
			.HeaderRow
			(
				SNew(SHeaderRow)
				+ SHeaderRow::Column( SEntityEditorTreeView::ColumnTypes::Actors() )
				.FillWidth(0.4f)
				[
					SNew(STextBlock).Text( LOCTEXT("EntityTreeEntityColumn","Entities") )
					.Margin(4.0f)
				]
			
			)
		];
}

// hierarchy access
//
void SEntityEditor::GetChildrenForTree(FEntityEditorTreeItem::Ptr TreeItem, TArray< FEntityEditorTreeItem::Ptr >& OutChildren )
{
	//no hierarchy (yet)
	OutChildren.Empty();
}

TSharedRef<ITableRow> SEntityEditor::GenerateTreeRow(FEntityEditorTreeItem::Ptr TreeItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	check( TreeItem.IsValid() );
	return SNew( SEntityEditorTreeRow, OwnerTable )
		.Item( TreeItem )
		.EntityEditor( SharedThis( this ) );
}

void SEntityEditor::HandleEditControlCheckStateChanged( ECheckBoxState NewState )
{
	GetEdMode()->SetEntityEditingEnable( NewState == ECheckBoxState::Checked );
}

ECheckBoxState SEntityEditor::GetEditControlCheckState() const
{
	return GetEdMode()->IsEntityEditingEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

// accessor for mode object
//
class FEntityEditingMode* SEntityEditor::GetEdMode() const
{
	return (FEntityEditingMode*)GLevelEditorModeTools().GetActiveMode( FEntityEditingMode::EM_Main );
}


void SEntityEditor::Populate()
{
	// Block events while we clear out the list.  We don't want actors in the level to become deselected
	// while we doing this
	TGuardValue<bool> ReentrantGuard( bIsReentrant, true );

	//find the world to show
	RepresentingWorld = nullptr;

	if(!RepresentingWorld)
	{
		// try to pick the most suitable world context

		// ideally we want a PIE world that is standalone or the first client
		for(const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if(World && Context.WorldType == EWorldType::PIE)
			{
				if(World->GetNetMode() == NM_Standalone)
				{
					RepresentingWorld = World;
					break;
				}
				else if(World->GetNetMode() == NM_Client && Context.PIEInstance == 2)	// Slightly dangerous: assumes server is always PIEInstance = 1;
				{
					RepresentingWorld = World;
					break;
				}
			}
		}
	}

	if(!RepresentingWorld)
	{
		// still not world so fallback to old logic where we just prefer PIE over Editor
		for(const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if(Context.WorldType == EWorldType::PIE)
			{
				RepresentingWorld = Context.World();
				break;
			}
			else if(Context.WorldType == EWorldType::Editor)
			{
				RepresentingWorld = Context.World();
			}
		}
	}

	if(!CheckWorld())
	{
		return;
	}

	if(bFullRefresh)
	{
		// Clear the selection here - RepopulateEntireTree will reconstruct it.
		TreeView->ClearSelection();

		RepopulateEntireTree();

		//bMadeAnySignificantChanges = true;
		bFullRefresh = false;
	}
}


void SEntityEditor::EmptyTreeItems()
{
	TreeRoots.Empty();
	TreeItemMap.Empty();
}

void SEntityEditor::RepopulateEntireTree()
{
	EmptyTreeItems();

	// Iterate over every actor in memory. WARNING: This is potentially very expensive!
	for(FActorIterator ActorIt( RepresentingWorld ); ActorIt; ++ActorIt)
	{
		AActor* Actor = *ActorIt;
		if(Actor)
		{
			AApparanceEntity* pentity = Cast<AApparanceEntity>( Actor );
			if(pentity)
			{
				auto p = MakeShared<FEntityEditorTreeItem>( this, pentity, nullptr, -1 );
				TreeRoots.Add( p );
				TreeItemMap.Add( pentity, p );
			}
		}
	}

	TreeView->RequestTreeRefresh();
}



// background updates/etc
//
void SEntityEditor::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	//refreshing : entity list
	if(bNeedsRefresh)
	{
		if(!bIsReentrant)
		{
			Populate();
		}
	}

	//refreshing : edit mode control
	if(bResyncMode)
	{
		FApparanceUnrealModule::GetModule()->EnableEditing( RepresentingWorld, GetEdMode()->IsEntityEditingEnabled(), true/*force*/ );
		bResyncMode = false;
	}

	//refreshing : selection state
	if(bActorSelectionDirty)
	{
		SynchronizeActorSelection();
		bActorSelectionDirty = false;
	}

	//rough local cache for spotting changes to selected actors (false +ve aren't an issue)
	USelection* SelectedActors = GEditor->GetSelectedActors();
	static TArray<bool> track_editing;
	if (track_editing.Num() < SelectedActors->Num())
	{
		track_editing.SetNum(SelectedActors->Num());
	}

	//refreshing : monitor state of selected entities for relevant changes
#if 0
	int i = 0;
	for (FSelectionIterator SelectionIt(*SelectedActors); SelectionIt; ++SelectionIt)
	{
		AApparanceEntity* entity_actor = Cast<AApparanceEntity>(*SelectionIt);
		if (entity_actor)
		{
			//discover support
			bool now_supports_editing = entity_actor->IsSmartEditingSupported();

			//changed?
			bool did_support_editing = track_editing[i];
			if (now_supports_editing != did_support_editing)
			{
				track_editing[i] = now_supports_editing;

				//changed, update edit mode
				UE_LOG(LogApparance, Display, TEXT("Edit support: %s"), now_supports_editing?TEXT("YES"):TEXT("NO") );
				entity_actor->SetEditingMode( GetEdMode()->IsEntityEditingEnabled()?Apparance::EntityMode::Selected:Apparance::EntityMode::Normal );
			}
		}
		i++;
	}
#endif
}


//helper to gather actors from tree selection set
struct FItemSelection
{
	TSet<AActor*> Actors;
	TArray<FEntityEditorTreeItem::Ptr> Items;

	FItemSelection( STreeView<FEntityEditorTreeItem::Ptr>& treeview )
	{
		//fetch selection		
		treeview.GetSelectedItems( Items );

		//filter
		for(int i = 0; i < Items.Num(); i++)
		{
			AApparanceEntity* pentity = Items[i]->GetEntity();
			Actors.Add( pentity );
		}
	}
};

void SEntityEditor::OnTreeViewSelect( FEntityEditorTreeItem::Ptr TreeItem, ESelectInfo::Type SelectInfo )
{
	if(SelectInfo == ESelectInfo::Direct)
	{
		return;
	}

	if(!bIsReentrant)
	{
		TGuardValue<bool> ReentrantGuard( bIsReentrant, true );

		// @todo outliner: Can be called from non-interactive selection

		// The tree let us know that selection has changed, but wasn't able to tell us
		// what changed.  So we'll perform a full difference check and update the editor's
		// selected actors to match the control's selection set.

		// Make a list of all the actors that should now be selected in the world.
		FItemSelection Selection( *TreeView );

		bool bChanged = false;
		bool bAnyInPIE = false;

		//any of our tree selection not actually selected in the world?
		for(auto* Actor : Selection.Actors)
		{
			if(!bAnyInPIE && Actor && Actor->GetOutermost()->HasAnyPackageFlags( PKG_PlayInEditor ))
			{
				bAnyInPIE = true;
			}
			if(!GEditor->GetSelectedActors()->IsSelected( Actor ))
			{
				bChanged = true;
				break;
			}
		}

		//any of the world actors not actually selected in the tree?
		for(FSelectionIterator SelectionIt( *GEditor->GetSelectedActors() ); SelectionIt && !bChanged; ++SelectionIt)
		{
			AActor* Actor = CastChecked< AActor >( *SelectionIt );
			if(!bAnyInPIE && Actor->GetOutermost()->HasAnyPackageFlags( PKG_PlayInEditor ))
			{
				bAnyInPIE = true;
			}
			if(!Selection.Actors.Contains( Actor ))
			{
				// Actor has been deselected
				bChanged = true;
			}
		}

		// If there's a discrepancy, update the selected actors to reflect this list.
		if(bChanged)
		{
			const FScopedTransaction Transaction( NSLOCTEXT( "UnrealEd", "ClickingOnActors", "Clicking on Actors" ), !bAnyInPIE );
			GEditor->GetSelectedActors()->Modify();

			// Clear the selection.
			GEditor->SelectNone( false, true, true );

			// We'll batch selection changes instead by using BeginBatchSelectOperation()
			GEditor->GetSelectedActors()->BeginBatchSelectOperation();

			const bool bShouldSelect = true;
			const bool bNotifyAfterSelect = false;
			const bool bSelectEvenIfHidden = true;	// @todo outliner: Is this actually OK?
			for(auto* Actor : Selection.Actors)
			{
				UE_LOG( LogApparance, Verbose, TEXT( "Clicking on Actor (entity editor): %s (%s)" ), *Actor->GetClass()->GetName(), *Actor->GetActorLabel() );
				GEditor->SelectActor( Actor, bShouldSelect, bNotifyAfterSelect, bSelectEvenIfHidden );
			}

			// Commit selection changes
			GEditor->GetSelectedActors()->EndBatchSelectOperation(/*bNotify*/false );

			// Fire selection changed event
			GEditor->NoteSelectionChange();

			//sync editing visualisation state for all affected actors
			if(GetEdMode()->IsEntityEditingEnabled())
			{
				for(auto& entry : TreeItemMap)
				{					
					AApparanceEntity* pentity = entry.Key;
					bool should_be_selected = Selection.Actors.Contains( pentity );					
					pentity->SetSmartEditingSelected( should_be_selected );
				}
			}
		}

		bActorSelectionDirty = true;
	}
}

// get tree selection to match world selection
//
void SEntityEditor::SynchronizeActorSelection()
{
	TGuardValue<bool> ReentrantGuard( bIsReentrant, true );

	USelection* SelectedActors = GEditor->GetSelectedActors();

	// Deselect actors in the tree that are no longer selected in the world
	FItemSelection Selection( *TreeView );
	if(Selection.Actors.Num())
	{
		TArray<FEntityEditorTreeItem::Ptr> ActorItems;
		for(FEntityEditorTreeItem::Ptr ActorItem : Selection.Items)
		{
			if(!ActorItem->Entity.IsValid() || !ActorItem->Entity.Get()->IsSelected())
			{
				ActorItems.Add( ActorItem->AsShared() );

				if(GetEdMode()->IsEntityEditingEnabled())
				{
					//these are not selected, so editable
					ActorItem->Entity->SetSmartEditingSelected( false );
				}
			}
		}

		TreeView->SetItemSelection( ActorItems, false );
	}

	// See if the tree view selector is pointing at a selected item
	bool bSelectorInSelectionSet = false;

	TArray<FEntityEditorTreeItem::Ptr> ActorItems;
	for(FSelectionIterator SelectionIt( *SelectedActors ); SelectionIt; ++SelectionIt)
	{
		AApparanceEntity* Actor = Cast< AApparanceEntity >( *SelectionIt );
		if(Actor)
		{
			if(FEntityEditorTreeItem::Ptr* ActorItem = TreeItemMap.Find( Actor ))
			{
				if(!bSelectorInSelectionSet && TreeView->Private_HasSelectorFocus( *ActorItem ))
				{
					bSelectorInSelectionSet = true;
				}

				ActorItems.Add( *ActorItem );
			}

			if(GetEdMode()->IsEntityEditingEnabled())
			{
				//now selected, ensure in select mode
				Actor->SetSmartEditingSelected( true );
			}
		}
	}

	// If NOT bSelectorInSelectionSet then we want to just move the selector to the first selected item.
	ESelectInfo::Type SelectInfo = bSelectorInSelectionSet ? ESelectInfo::Direct : ESelectInfo::OnMouseClick;
	TreeView->SetItemSelection( ActorItems, true, SelectInfo );

	//also sync edit mode for current selection to make sure correct (e.g. new instance, dupe, etc)
	
}

void SEntityEditor::OnLevelSelectionChanged( UObject* Obj )
{
	UE_LOG(LogApparance, Display, TEXT("OnLevelSelectionChanged %s"), Obj?(*Obj->GetName()):TEXT("NONE") );
	if(!bIsReentrant)
	{
		//TreeView->ClearSelection();
		bActorSelectionDirty = true;

#if 0
		// Scroll last item into view - this means if we are multi-selecting, we show newest selection. @TODO Not perfect though
		if(AActor* LastSelectedActor = GEditor->GetSelectedActors()->GetBottom<AActor>())
		{
			FEntityeditorItem::Ptr TreeItem = TreeItemMap.FindRef( LastSelectedActor );
			if(TreeItem.IsValid())
			{
				if(!TreeView->IsItemVisible( TreeItem ))
				{
					ScrollItemIntoView( TreeItem );
				}
			}
			else
			{
				OnItemAdded( LastSelectedActor, ENewItemAction::ScrollIntoView );
			}
		}
#endif
	}
}
void SEntityEditor::OnMapChange( uint32 MapFlags )
{
	UE_LOG(LogApparance, Display, TEXT("OnMapChange"));
	bNeedsRefresh = true;
	bFullRefresh = true;
}
void SEntityEditor::OnNewCurrentLevel()
{
	UE_LOG(LogApparance, Display, TEXT("OnNewCurrentLevel"));
	bFullRefresh = true;
	bActorSelectionDirty = true;
}
void SEntityEditor::OnLevelActorListChanged()
{
	UE_LOG(LogApparance, Display, TEXT("OnLevelActorListChanged"));
	bNeedsRefresh = true;
	bFullRefresh = true;
	bActorSelectionDirty = true;
	bResyncMode = true;
}
void SEntityEditor::OnLevelAdded( ULevel* InLevel, UWorld* InWorld )
{
	UE_LOG(LogApparance, Display, TEXT("OnLevelAdded"));
	bNeedsRefresh = true;
	bFullRefresh = true;
	bActorSelectionDirty = true;
}
void SEntityEditor::OnLevelRemoved( ULevel* InLevel, UWorld* InWorld )
{
	UE_LOG(LogApparance, Display, TEXT("OnLevelRemoved"));
	bNeedsRefresh = true;
	bFullRefresh = true;
	bActorSelectionDirty = true;
}
void SEntityEditor::OnLevelActorsAdded( AActor* InActor )
{
	//ignore actor spamming due to placement
	if (InActor->HasAnyFlags(RF_DuplicateTransient))
	{
		return;
	}

	UE_LOG(LogApparance, Display, TEXT("OnLevelActorsAdded"));
	bNeedsRefresh = true;
	bFullRefresh = true;
	bActorSelectionDirty = true;
	bResyncMode = true;
}
void SEntityEditor::OnLevelActorsRemoved( AActor* InActor )
{
	UE_LOG(LogApparance, Display, TEXT("OnLevelActorsRemoved"));
	bNeedsRefresh = true;
	bFullRefresh = true;
	bActorSelectionDirty = true;
}
//void SEntityEditor::OnLevelActorsRequestRename( const AActor* InActor )
void SEntityEditor::OnActorLabelChanged( AActor* ChangedActor )
{

}
void SEntityEditor::OnAssetReloaded( const EPackageReloadPhase InPackageReloadPhase, FPackageReloadedEvent* InPackageReloadedEvent )
{

}
void SEntityEditor::OnEditCutActorsBegin()
{

}
void SEntityEditor::OnEditCutActorsEnd()
{
	UE_LOG(LogApparance, Display, TEXT("OnEditCutActorsEnd"));
	bActorSelectionDirty = true;
}
void SEntityEditor::OnEditCopyActorsBegin()
{

}
void SEntityEditor::OnEditCopyActorsEnd()
{

}
void SEntityEditor::OnEditPasteActorsBegin()
{

}
void SEntityEditor::OnEditPasteActorsEnd()
{
	UE_LOG(LogApparance, Display, TEXT("OnEditPasteActorsEnd"));
	bNeedsRefresh = true;
	bFullRefresh = true;
	bResyncMode = true;
	bActorSelectionDirty = true;
}
void SEntityEditor::OnDuplicateActorsBegin()
{

}
void SEntityEditor::OnDuplicateActorsEnd()
{
	UE_LOG(LogApparance, Display, TEXT("OnDuplicateActorsEnd"));
	bNeedsRefresh = true;
	bFullRefresh = true;
	bResyncMode = true;
	bActorSelectionDirty = true;
}
void SEntityEditor::OnDeleteActorsBegin()
{

}
void SEntityEditor::OnDeleteActorsEnd()
{
	UE_LOG(LogApparance, Display, TEXT("OnDeleteActorsEnd"));
	bActorSelectionDirty = true;
}


//------------------------------------------------------------------------
// SEntityEditorTreeView

void SEntityEditorTreeView::Construct( const FArguments& InArgs, TSharedRef<SEntityEditor> Owner )
{
	EntityEditorWeak = Owner;
	STreeView::Construct( InArgs );
}


//------------------------------------------------------------------------
// SEntityEditorTreeRow

void SEntityEditorTreeRow::Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& TreeView )
{
	Item = InArgs._Item->AsShared();
	EntityEditorWeak = InArgs._EntityEditor;

	auto Args = FSuperRowType::FArguments()
		.Style( &APPARANCE_STYLE.GetWidgetStyle<FTableRowStyle>( "SceneOutliner.TableViewRow" ) );
//	Args.OnDragDetected( this, &SResourcesListTreeRow::OnDragDetected );

	SMultiColumnTableRow< FEntityEditorTreeItem::Ptr >::Construct( Args, TreeView );
}

// Build UI for a specific column of the hierarchy
//
TSharedRef<SWidget> SEntityEditorTreeRow::GenerateWidgetForColumn( const FName& ColumnName )
{
	FEntityEditorTreeItem::Ptr ItemPtr = Item.Pin();
	if (!ItemPtr.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	//------------------------------------------------------------------------
	// names and path
	if( ColumnName == SEntityEditorTreeView::ColumnTypes::Actors() )
	{
		TSharedPtr<SEntityEditor> peditor = EntityEditorWeak.Pin();

		//editable text (to support rename)
		TSharedPtr<SInlineEditableTextBlock> InlineTextBlock = SNew(SInlineEditableTextBlock)
			.Text(this, &SEntityEditorTreeRow::GetRowName, ItemPtr )
//			.HighlightText( SceneOutliner.GetFilterHighlightText() )
			.ColorAndOpacity(this, &SEntityEditorTreeRow::GetRowColourAndOpacity, ItemPtr)
#if 0
			.OnTextCommitted(this, &SResourcesListTreeRow::OnItemLabelCommitted, ItemPtr)
			.OnVerifyTextChanged(this, &SResourcesListTreeRow::OnVerifyItemLabelChange, ItemPtr)
//			.IsSelected(FIsSelected::CreateSP(&InRow, &SResourceListTreeRow::IsSelectedExclusively))
			.IsReadOnly_Lambda([Editor = ResourceListEditorWeak, Item = ItemPtr, this]()
			{
				TSharedPtr<FResourceListEditor> ple = Editor.Pin();
				return !ple || !ple->CanRenameItem( Item.Get() );
			})
#endif
			.IsReadOnly(true)
			;

		// The first column gets the tree expansion arrow for this row
		auto ui =
			SNew( SHorizontalBox )
#if 0
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(6, 0, 0, 0)
			[
				SNew( SExpanderArrow, SharedThis(this) ).IndentAmount(12)
			]
#endif
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.FillWidth(1.0f)
			[
#if 0
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
#endif
						InlineTextBlock.ToSharedRef()
#if 0
					]
				]
#endif
			];

			return ui;
	}


	return SNullWidget::NullWidget;
}

// Row content dimming for variants
//
FSlateColor SEntityEditorTreeRow::GetRowColourAndOpacity( FEntityEditorTreeItem::Ptr item ) const
{
	TSharedPtr<SEntityEditor> entity_editor = EntityEditorWeak.Pin();
	if(!entity_editor->GetEdMode()->IsEntityEditingEnabled() || item->IsDimmed())// && item != res_list_editor->SelectedItem)	//(interferes with selection row highlight)
	{
		return FSlateColor::UseSubduedForeground(); //like in scene outliner
	}
	else
	{
		//TODO: work out why this causes icons to go black on selected rows
		return FSlateColor::UseForeground();
	}
}

AApparanceEntity* SEntityEditorTreeRow::GetRowEntity( FEntityEditorTreeItem::Ptr item ) const
{
	return nullptr;
}
void SEntityEditorTreeRow::SetRowEntity( AApparanceEntity* new_value, FEntityEditorTreeItem::Ptr item )
{
}
FText SEntityEditorTreeRow::GetRowName( FEntityEditorTreeItem::Ptr item ) const
{
	if(item.IsValid())
	{
		return FText::FromString( item->GetName() );
	}
	return FText();
}


//------------------------------------------------------------------------
// FEntityEditorTreeItem

FString FEntityEditorTreeItem::GetName() const
{
	if(Entity.IsValid())
	{
		return Entity->GetName();
	}
	return FString();
}

bool FEntityEditorTreeItem::IsDimmed() const
{
	return !IsEditingAvailable();
}

bool FEntityEditorTreeItem::IsEditingAvailable() const
{
	auto e = GetEntity();
	if(e)
	{
		return e->IsSmartEditingSupported();
	}
	//unable to find out, assume no
	return false;
}



#undef LOCTEXT_NAMESPACE