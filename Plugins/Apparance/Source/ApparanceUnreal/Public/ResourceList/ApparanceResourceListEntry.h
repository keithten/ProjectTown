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

// apparance
#include "Apparance.h"

// module

// auto (last)
#include "ApparanceResourceListEntry.generated.h"


// base entry for asset entries and forming naming hierarchy
//
UCLASS()
class APPARANCEUNREAL_API UApparanceResourceListEntry 
	: public UObject
{
	GENERATED_BODY()

	//transient
	bool bIsNameEditable = true;

public:
	UPROPERTY(EditAnywhere,Category=Apparance)
	FString Name;
	
	UPROPERTY()
	TArray<UApparanceResourceListEntry*> Children;

	//keep old variant siblings around so they aren't lost when toggling between single/variant mode
	UPROPERTY()
	TArray<UApparanceResourceListEntry*> OldVariantSiblings;
	
public:
	//editing
	virtual void SetName( FString name ) { Name = name; }
	virtual void Editor_AssignFrom( UApparanceResourceListEntry* psource );
	void Editor_NotifyCacheInvalidatingChange();
	void NotifyPropertyChanged( UObject* pobject );
	
	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
#endif
	//~ End UObject Interface

	//access
	virtual bool IsNameEditable() const { return bIsNameEditable; }
	virtual FString GetName() const { return Name; }
	virtual UClass* GetAssetClass() const { return nullptr; }
	virtual FName GetIconName( bool is_open=false );
	static FName StaticIconName( bool is_open=false );
	virtual bool GetBounds( FVector& centre_out, FVector& extent_out ) const { return false; }
	void SetNameEditable( bool enable ) { bIsNameEditable = enable; }
	FString BuildDescriptor() const;
	const UApparanceResourceListEntry* FindParent() const;
	UApparanceResourceListEntry* FindParent();
		
protected:
	//editing
	void Editor_NotifyStructuralChange( struct FPropertyChangedEvent& PropertyChangedEvent );
	void Editor_NotifyStructuralChange( FName property );	

	//events
	virtual void Editor_OnAssetChanged();

};

