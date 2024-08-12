//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

//main
#include "EntityEditingToolkit.h"
#include "SEntityEditor.h"

//unreal
//#include "Core.h"
#include "Editor.h"
//unreal:ui

//module
#include "EntityEditingToolkit.h"

#define LOCTEXT_NAMESPACE "ApparanceUnreal"


//////////////////////////////////////////////////////////////////////////
// FEntityEditingToolkit

void FEntityEditingToolkit::Init( const TSharedPtr< class IToolkitHost >& InitToolkitHost )
{
	EntityEditor = SNew( SEntityEditor, SharedThis( this ) );

	FModeToolkit::Init( InitToolkitHost );
}

// tool identity
//
FName FEntityEditingToolkit::GetToolkitFName() const { return FName( "ApparanceEntityEditingMode" ); }
FText FEntityEditingToolkit::GetBaseToolkitName() const { return LOCTEXT( "DisplayName", "Entity Editor" ); }

// tool Ui access
//
TSharedPtr<class SWidget> FEntityEditingToolkit::GetInlineContent() const
{
	return EntityEditor;
}






#undef LOCTEXT_NAMESPACE
