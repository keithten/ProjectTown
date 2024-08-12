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
#include "EdMode.h"

//module
//#include "ApparanceUnrealVersioning.h"

// entity editing mode functionality
//
class FEntityEditingMode : public FEdMode
{
	TSharedPtr<class FEntityEditingToolkit> EntityToolkit;

	//viewport
	FVector ViewPoint;
	FVector ViewDirection;
	float ViewFOV;
	int ViewPixelWidth;
	//interaction
	FVector CursorRayDirection;
	bool bInteractionButton;
	int iModifierKeyFlags;
	static bool bEditingIsActive;
	static bool bEnabledOnRequest;

public:
	const static FEditorModeID EM_Main;

	// FEdMode interface
	virtual void Enter() override;
	virtual void Exit() override;
	virtual bool UsesToolkits() const override { return true; }
	virtual void Render( const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI ) override;
	virtual bool ShouldDrawWidget() const;
	virtual void Tick( FEditorViewportClient* ViewportClient, float DeltaTime );

	virtual bool MouseMove( FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y ) override;
	virtual bool CapturedMouseMove( FEditorViewportClient* InViewportClient, FViewport* InViewport, int32 InMouseX, int32 InMouseY ) override;
	virtual bool InputKey( FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event ) override;

	//plugin editing mode state
	void SetEntityEditingEnable( bool enable );
	bool IsEntityEditingEnabled() const;

	//global tool request
	static void RequestShow(bool auto_enable=false);
	static void RequestHide(bool force_hide=false);

private:
	void UpdateViewportInfo( FEditorViewportClient* ViewportClient );
	bool UpdateInteraction();

};