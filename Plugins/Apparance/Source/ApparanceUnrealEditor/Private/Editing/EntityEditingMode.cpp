//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

//main
#include "EntityEditingMode.h"

//unreal
#include "Editor.h"
#include "Toolkits/ToolkitManager.h"
//unreal:editor
#include "Engine/Selection.h"
//unreal:ui

//module
#include "ApparanceUnreal.h"
#include "EntityEditingToolkit.h"
#include "Utility/ApparanceConversion.h"

#define LOCTEXT_NAMESPACE "ApparanceUnreal"


const FEditorModeID FEntityEditingMode::EM_Main( TEXT("ApparanceEntityEditingMode") );

bool FEntityEditingMode::bEditingIsActive = false;
bool FEntityEditingMode::bEnabledOnRequest = false;


//////////////////////////////////////////////////////////////////////////
// FEntityEditingMode

//global tool open request
//optionally enable editing
//
void FEntityEditingMode::RequestShow(bool auto_enable)
{
	//show tool?
	FEntityEditingMode* mode_tools = (FEntityEditingMode*)GLevelEditorModeTools().GetActiveMode( FEntityEditingMode::EM_Main );
	if (mode_tools)
	{
		//already open, just enable
		if (auto_enable)
		{
			mode_tools->SetEntityEditingEnable( true );
		}
	}
	else
	{
		//want to enable when opened
		if (auto_enable)
		{
			bEditingIsActive = true;
			bEnabledOnRequest = true;	//it was a request that caused it to open
		}

		//not active, activate it
		GLevelEditorModeTools().ActivateMode( FEntityEditingMode::EM_Main );
	}
}

//global tool close request
//disables editing
//
void FEntityEditingMode::RequestHide(bool force_hide)
{
	FEntityEditingMode* mode_tools = (FEntityEditingMode*)GLevelEditorModeTools().GetActiveMode(FEntityEditingMode::EM_Main);
	if(mode_tools)
	{
		if (bEnabledOnRequest || force_hide)
		{
			//was opened by a request (or being forced)
			GLevelEditorModeTools().DeactivateMode(FEntityEditingMode::EM_Main);
		}
		else
		{
			//need to turn off editing at least
			mode_tools->SetEntityEditingEnable(false);
		}
	}
	
	bEnabledOnRequest = false;
}


// mode activated
//
void FEntityEditingMode::Enter()
{
	FEdMode::Enter();

	if(!Toolkit.IsValid())
	{
		EntityToolkit = MakeShareable( new FEntityEditingToolkit );
		Toolkit = EntityToolkit;
		Toolkit->Init( Owner->GetToolkitHost() );
	}

	ViewPoint = FVector::ZeroVector;
	ViewDirection = FVector::ForwardVector;
	CursorRayDirection = FVector::ForwardVector;
	bInteractionButton = false;
	iModifierKeyFlags = 0;

	//restore mode
	SetEntityEditingEnable( bEditingIsActive );
}

//mode deactivated
//
void FEntityEditingMode::Exit()
{
	//save mode while we turn it off
	bool editing_active = bEditingIsActive;	//keep around disable
	SetEntityEditingEnable( false );
	bEditingIsActive = editing_active; //store original state

	FToolkitManager::Get().CloseToolkit( Toolkit.ToSharedRef() );
	Toolkit.Reset();
	EntityToolkit.Reset();

	FEdMode::Exit();
}

void FEntityEditingMode::Render( const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI )
{
}

void FEntityEditingMode::Tick( FEditorViewportClient* ViewportClient, float DeltaTime )
{
	UpdateViewportInfo( ViewportClient );
}

bool FEntityEditingMode::ShouldDrawWidget() const
{
	return !bEditingIsActive;
}


bool FEntityEditingMode::MouseMove( FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y )
{
	//calculate relevant view info
	FViewportCursorLocation vcl = ViewportClient->GetCursorWorldLocationFromMousePos();
	FVector dir = vcl.GetDirection();

	//check for change (synthesised mouse events fire this constantly otherwise)
	if(dir!= CursorRayDirection)
	{
		CursorRayDirection = dir;
		return UpdateInteraction();
	}

	//no change
	return false;
}

bool FEntityEditingMode::CapturedMouseMove( FEditorViewportClient* InViewportClient, FViewport* InViewport, int32 InMouseX, int32 InMouseY )
{
	return MouseMove( InViewportClient, InViewport, InMouseX, InMouseY );
}

bool FEntityEditingMode::InputKey( FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event )
{ 
	bool changed = false;
	bool absorbed = false;

	//check mouse change
	if(Key==EKeys::LeftMouseButton)
	{
		const bool pressed = Event==EInputEvent::IE_Pressed;
		const bool pressed2 = Event == EInputEvent::IE_DoubleClick;
		const bool released = Event==EInputEvent::IE_Released;
		if(pressed || pressed2 || released)
		{
			bool is_a_press = pressed || pressed2;
			if(is_a_press != bInteractionButton)
			{
				bInteractionButton = is_a_press;
				changed = true;
			}
		}
		//also block all double-clicks from Unreal as this interferes with object selction
		if(pressed2)
		{
			absorbed = true;
		}
	}

	//check relevant modifier changed
	const FModifierKeysState KeyState = FSlateApplication::Get().GetModifierKeys();
	const bool bShiftDown = KeyState.IsShiftDown();
	const bool bCtrlDown = KeyState.IsControlDown();
	const bool bAltDown = KeyState.IsAltDown();
	const int new_modifiers = (bShiftDown ? 1 : 0) | (bCtrlDown ? 2 : 0) | (bAltDown ? 4 : 0);
	if(new_modifiers!= iModifierKeyFlags)
	{
		iModifierKeyFlags = new_modifiers; 
		changed = true;
	}

	//apply if useful change
	if(changed) 
	{
		changed = UpdateInteraction();
	}

#if 0
	FText event_type = UEnum::GetDisplayValueAsText<EInputEvent>( Event );
	UE_LOG( LogApparance, Display, TEXT("INTERACTION KEY: %s :%s%s"), *event_type.ToString(), changed ? TEXT(" CHANGED") : TEXT(""), absorbed ? TEXT(" ABSORBED") : TEXT("") );
#endif
	return changed || absorbed;
}

void FEntityEditingMode::UpdateViewportInfo( FEditorViewportClient* ViewportClient )
{
	//gather
	FVector view_point = ViewportClient->GetViewLocation();
	FRotator view_rotation = ViewportClient->GetViewRotation();
	FVector view_direction = view_rotation.Vector();
	float fov_angle = ViewportClient->FOVAngle;
	FIntPoint origin, dimensions;
	ViewportClient->GetViewportDimensions( origin, dimensions );

	//changed?
	if(view_point!=ViewPoint || view_direction!=ViewDirection || fov_angle!=ViewFOV || dimensions.X!=ViewPixelWidth)
	{
		//track
		ViewPoint = view_point;
		ViewFOV = fov_angle;
		ViewPixelWidth = dimensions.X;
		ViewDirection = view_direction;

		//further calcs
		float pixel_angle = fov_angle / (float)dimensions.X;	//degrees assigned to pixel

		//apply
		Apparance::Vector3 pos = VECTOR3_APPARANCESPACE_FROM_UNREALSPACE( ViewPoint );
		Apparance::Vector3 dir = APPARANCEHANDEDNESS_FROM_UNREALHANDEDNESS( ViewDirection );
		FApparanceUnrealModule::GetEngine()->UpdateInteractiveViewport( pos, dir, fov_angle, pixel_angle );
	}
}

// send current state to editing tools
//
bool FEntityEditingMode::UpdateInteraction()
{
	//pass into Apparance engine
	Apparance::Vector3 dir = VECTOR3_APPARANCEHANDEDNESS_FROM_UNREALHANDEDNESS( CursorRayDirection );
	return FApparanceUnrealModule::GetEngine()->UpdateInteractiveEditing( dir, bInteractionButton, iModifierKeyFlags );
}

void FEntityEditingMode::SetEntityEditingEnable( bool enable )
{
	//update editor with mode state
	FApparanceUnrealModule::GetModule()->EnableEditing( enable );

	//restore widget location as seems not to track actor automatically when shown again
	if(!enable)
	{
		AActor* selected_actor = GEditor->GetSelectedActors()->GetTop<AActor>();
		if(selected_actor)
		{
			Owner->SetPivotLocation( selected_actor->GetActorLocation(), false );
		}
	}

	bEditingIsActive = enable;
}

bool FEntityEditingMode::IsEntityEditingEnabled() const
{
	return bEditingIsActive;
}




#undef LOCTEXT_NAMESPACE
