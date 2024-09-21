//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#pragma once


// unreal
#include "CoreMinimal.h"

//module
#include "ApparanceEntity.h"

// auto (last)
#include "SmartEditingState.generated.h"


// Per-world state tracking for smart editing system, when enabled
//
UCLASS()
class USmartEditingState : public UObject
{
	GENERATED_BODY()

	//- static state -
	class UWorld* m_pWorld;
	std::string m_WorldName;

	//engine access
	struct Apparance::IEngine* m_pEngine;
	class FApparanceUnrealModule* m_pModule;

	//- dynamic state -
	UPROPERTY()
	TArray<AApparanceEntity*> SelectedEntities;

	bool m_IsActive;

	//viewport
	FVector ViewPoint;
	FVector ViewDirection;
	float ViewFOV;
	int ViewPixelWidth;
	
public:
	//setup
	USmartEditingState();
	void Init(class UWorld* world);

	//smart editing started (again)
	void Start();
	//smart editing stopped (for now)
	void Stop();

	//access
	bool IsActive() const;
	int GetSelectedEntities(TArray<class AApparanceEntity*>& Out_Entities);
	class UWorld* GetWorld() const { return m_pWorld; }

	//control
	void NotifyEntitySelection(class AApparanceEntity* Entity, bool IsSelected);
	void Tick();

	//update
	bool UpdateInteractiveEditing(FVector CursorDirection, bool Interact);

private:

	const char* GetWorldNameAnsi() const { return m_WorldName.c_str(); }
	void UpdateViewportInfo();
	void HandleRemoveWorld(class UWorld* InWorld);

};

