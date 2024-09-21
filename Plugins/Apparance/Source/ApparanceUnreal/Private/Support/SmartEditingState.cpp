//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----


// main
#include "SmartEditingState.h"

// unreal
#include "EngineUtils.h"
#include "Engine/World.h"
#include "UnrealClient.h"
#include "Camera/PlayerCameraManager.h"

// apparance
#include "ApparanceUnrealVersioning.h"
#include "ApparanceEntity.h"
#include "Apparance.h"

// module
#include "ApparanceUnreal.h"




//////////////////////////////////////////////////////////////////////////
// USmartEditingState

// ctor, raw field init
//
USmartEditingState::USmartEditingState()
	: m_pWorld(nullptr)
	, m_pEngine(nullptr)
	, m_pModule(nullptr)
	, m_IsActive(false)
	, ViewPoint(FVector(0,0,0))
	, ViewDirection(FVector(0,0,0))
	, ViewFOV(0)
	, ViewPixelWidth(0)
{
}

// setup
//
void USmartEditingState::Init(UWorld* world)
{
	check(m_pWorld == nullptr);
	m_pWorld = world;
	m_WorldName = StringCast<ANSICHAR>( *FApparanceUnrealModule::MakeWorldIdentifier(m_pWorld) ).Get();

	//gather dependencies
	m_pEngine = FApparanceUnrealModule::GetEngine();
	m_pModule = FApparanceUnrealModule::GetModule();
}

// enable, make active, prep for action
//
void USmartEditingState::Start()
{
	if(!m_IsActive)
	{
		//update smart editing of all entities in world
		if (IsValid( m_pWorld ))
		{
			for (FActorIterator ActorIt(m_pWorld); ActorIt; ++ActorIt)
			{
				AActor* Actor = *ActorIt;
				if (Actor)
				{
					AApparanceEntity* pentity = Cast<AApparanceEntity>(Actor);
					if (pentity)
					{
						//on (straight to selected view mode if is selected in editor)
						bool unreal_selected = false;
						if (!m_pModule->IsGameRunning())
						{
							unreal_selected = pentity->IsSelected();
						}
						
						//need to ensure all entities are smart editing enabled
						pentity->EnableSmartEditing();

						//NOTE: will call back through to NotifyEntitySelection
						pentity->SetSmartEditingSelected(unreal_selected);
					}
				}
			}
		}

		ViewPoint = FVector::ZeroVector;
		ViewDirection = FVector::ForwardVector;
		m_IsActive = true;
	}
}

// disable, make inactive, pause the action
//
void USmartEditingState::Stop()
{
	if (m_IsActive)
	{
		//update smart editing of all entities in world
		if (IsValid( m_pWorld )) //(if world still in use)
		{
			for (FActorIterator ActorIt( m_pWorld ); ActorIt; ++ActorIt )
			{
				AActor* Actor = *ActorIt;
				if (Actor)
				{
					AApparanceEntity* pentity = Cast<AApparanceEntity>( Actor );
					if (pentity)
					{
						//off

						//NOTE: will call back through to NotifyEntitySelection
						pentity->SetSmartEditingSelected( false );
					}
				}
			}
		}
		m_IsActive = false;
	}
}

// active in world, editing going on?
// NOTE: Also needs to check world still valid as can be slight gap between PIE stopping and state object being stopped
//
bool USmartEditingState::IsActive() const
{
	return m_IsActive && IsValid(m_pWorld);
}


// accumulate list of selected entities
//
int USmartEditingState::GetSelectedEntities( TArray<class AApparanceEntity*>& Out_Entities )
{	
	Out_Entities.Append( SelectedEntities );
	return SelectedEntities.Num();
}

// update selection list due to entity selection state changing
// i.e. Smart editing selection is applied to each entity, which tracks this selection state and when changed notifies the smart editing system (here)
//
void USmartEditingState::NotifyEntitySelection(class AApparanceEntity* Entity, bool IsSelected)
{
	if (IsSelected)
	{
		//ensure known to be selected
		if (!SelectedEntities.Contains( Entity ))
		{
			SelectedEntities.Add( Entity );
		}
	}
	else
	{
		//ensure known to be unselected
		if (SelectedEntities.Contains( Entity ))
		{
			SelectedEntities.Remove( Entity );
		}
	}
}

// regular updates
//
void USmartEditingState::Tick()
{
	if (IsActive())
	{
		UpdateViewportInfo();
	}
}

// push viewport state changes to interaction system in Apparance engine
//
void USmartEditingState::UpdateViewportInfo()
{
	//gather
	FViewport* viewport = nullptr;
	if (GEngine->GameViewport)
	{
		viewport = GEngine->GameViewport->Viewport;
	}
	if (!viewport)
	{
		viewport = m_pModule->Editor_FindViewport();
	}
	APlayerController* player_controller = m_pWorld ? (m_pWorld->GetFirstPlayerController()) : nullptr;
	APlayerCameraManager* camera = player_controller ? (player_controller->PlayerCameraManager) : nullptr;
	if (!viewport || !m_pWorld || !player_controller || !camera)
	{
		UE_LOG( LogApparance, Error, TEXT( "Unable to update interaction system, no viewport found" ) );
		return;
	}

	//view point
	FIntPoint dimensions = viewport->GetSizeXY();
	const FVector2D view_size = FVector2D(dimensions);
	float fov_angle = camera->GetFOVAngle();
	FVector view_point;
	FRotator view_rotation;
	camera->GetCameraViewPoint(view_point, view_rotation);
	FVector view_direction = view_rotation.Vector();

	//changed?
	if (view_point != ViewPoint || view_direction != ViewDirection || fov_angle != ViewFOV || dimensions.X != ViewPixelWidth)
	{
		//track
		ViewPoint = view_point;
		ViewFOV = fov_angle;
		ViewPixelWidth = dimensions.X;
		ViewDirection = view_direction;

		//further calcs
		float pixel_angle = fov_angle / (float)dimensions.X;	//degrees assigned to pixel

		//apply
		Apparance::Vector3 pos = VECTOR3_APPARANCESPACE_FROM_UNREALSPACE(ViewPoint);
		Apparance::Vector3 dir = APPARANCEHANDEDNESS_FROM_UNREALHANDEDNESS(ViewDirection);
		m_pEngine->UpdateInteractiveViewport( GetWorldNameAnsi(), pos, dir, fov_angle, pixel_angle);
	}
}

// push interaction info changes to interaction system in Apparance engine
//
bool USmartEditingState::UpdateInteractiveEditing( FVector CursorDirection, bool Interact )
{
	//convert
	Apparance::Vector3 dir = VECTOR3_APPARANCEHANDEDNESS_FROM_UNREALHANDEDNESS( CursorDirection );
	
	//forward to engine
	return m_pEngine->UpdateInteractiveEditing( GetWorldNameAnsi(), dir, Interact, 0 );	
}

