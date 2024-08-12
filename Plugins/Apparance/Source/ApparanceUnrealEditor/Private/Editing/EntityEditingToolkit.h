//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#pragma once

//unreal
#include "Toolkits/BaseToolkit.h"
#include "EditorModeManager.h"

//module
#include "EntityEditingMode.h"

//module

// toolkit intermediary for editing mode
//
class FEntityEditingToolkit : public FModeToolkit
{
public:

	/** Initializes the geometry mode toolkit */
	virtual void Init( const TSharedPtr< class IToolkitHost >& InitToolkitHost ) override;

	/** IToolkit interface */
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual class FEdMode* GetEditorMode() const override { return GLevelEditorModeTools().GetActiveMode( FEntityEditingMode::EM_Main ); }
	virtual TSharedPtr<class SWidget> GetInlineContent() const override;

private:
	TSharedPtr<class SEntityEditor> EntityEditor;
};