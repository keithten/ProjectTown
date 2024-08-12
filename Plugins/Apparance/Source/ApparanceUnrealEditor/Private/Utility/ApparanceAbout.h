//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
//#include "Slate.h"
#include "InfoSlot.h"

class ITableRow;
class SButton;
class STableViewBase;
struct FSlateBrush;

enum class EApparanceAboutPage
{
	About,
	ReleaseNotes,
	Setup,
	Status,
	License,
	Credits,
	Attribution
};

// about screen widget
//
class SApparanceAbout : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SApparanceAbout )
	{}
	SLATE_END_ARGS()

	//timing regular checks
	float TickTimer;
	
	//slots that make up the page (keep alive with page)
	TArray<TSharedPtr<FInfoSlot>> Slots;

	//switchable page of text
	TSharedPtr<SVerticalBox> Page;

	//the active page
	EApparanceAboutPage CurrentPage;

	//container
	TSharedPtr<SWindow> MyWindow;

	/**
	* Constructs the about screen widgets                   
	*/
	void Construct(const FArguments& InArgs);
	void SetWindow( TSharedPtr<SWindow> InWindow )
	{
		MyWindow = InWindow;
	}
	
	void SetPage( EApparanceAboutPage page );

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;
	virtual FReply OnKeyUp( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;

	virtual bool SupportsKeyboardFocus() const override { return true; }

	//setup check
	static bool IsSetupNeeded();

private:
	TArray<TSharedPtr<FInfoSlot>> LoadPage(FString filename);
	TArray<TSharedPtr<FInfoSlot>> BuildSetupPage();
	TArray<TSharedPtr<FInfoSlot>> BuildStatusPage();

	
	void ParseSlots( const TArray<FString>& lines, TArray<TSharedPtr<FInfoSlot>>& slots_out );
	TSharedPtr<FInfoSlot> ParseSlot( FString line );
	void ParseParameter( const FString& line, FName& name_out, FString& value_out );

	void AddEntityStats();
	
	EVisibility HandlePageLinkImageVisibility( EApparanceAboutPage page) const;
	
	FReply OnClose();

	void CheckPageRefresh();

};

