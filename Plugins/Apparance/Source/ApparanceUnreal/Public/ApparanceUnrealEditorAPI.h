//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#pragma once

// unreal

// apparance
#include "Apparance.h"

// module

// auto (last)



// Apparance plugin access back to editor module (e.g. notifications when running in editor)
//
struct IApparanceUnrealEditorAPI
{
	// procedural content has been changed externally (e.g. by remote edit)
	virtual void NotifyExternalContentChanged() = 0;

	// the asset database has changed or been invalidated
	virtual void NotifyAssetDatabaseChanged() = 0;

	// a resource list structure has changed
	virtual void NotifyResourceListStructureChanged( class UApparanceResourceList* pres_list, class UApparanceResourceListEntry* pentry, FName property ) = 0;

	// a resource list asset has chnaged
	virtual void NotifyResourceListAssetChanged( UApparanceResourceList* pres_list, UApparanceResourceListEntry* pentry, UObject* pnewasset ) = 0;

	// base entity properties changed, affects bp instances, etc
	virtual void NotifyProcedureDefaultsChange( Apparance::ProcedureID proc_id, const Apparance::IParameterCollection* params, Apparance::ValueID changed_param) = 0;

	// details panels need to refresh when procedure types change
	virtual void NotifyEntityProcedureTypeChanged( Apparance::ProcedureID new_proc_id ) = 0;
		
	// provide a temp world to game class editor functionality that needs one (see UApparanceResourceListEntry_Blueprint)
	virtual UWorld* GetTempWorld() = 0;

	// query whether game is running (would only be false in editor, outside of sim/pie)
	virtual bool IsGameRunning() const = 0;

	// prepare an entity/etc for editing (e.g. save undo state, begin transaction)
	virtual Apparance::IParameterCollection* BeginParameterEdit( class IProceduralObject* pentity, bool editing_transaction ) = 0;

	// finish up an entity/etc edit (e.g. commit transaction)
	virtual void EndParameterEdit( class IProceduralObject* pentity, bool editing_transaction, Apparance::ValueID input_id ) = 0;

	// viewport access
	virtual FViewport* FindEditorViewport() const = 0;
};
