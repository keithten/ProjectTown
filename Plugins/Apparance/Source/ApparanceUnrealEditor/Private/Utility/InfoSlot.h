//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#pragma once

#include "CoreMinimal.h"
//#include "Slate.h"


// Information slot view model
//
class FInfoSlot : public TSharedFromThis<FInfoSlot>
{
	TMap<FName, FString> Parameters;
public:
	FString Heading;
	FString Content;

	FInfoSlot( FString heading, FString content, FString icon=TEXT("") )
		: Heading( heading )
		, Content( content )
	{
		if(!icon.IsEmpty())
		{
			SetParameter( FName( "Icon" ), icon );
		}
	}
	FInfoSlot( FText heading, FText content, FString icon = TEXT( "" ) )
		: FInfoSlot( heading.ToString(), content.ToString(), icon )
	{
	}

	virtual ~FInfoSlot() {}

	//specialised parameters
	void SetParameter( FName name, FString value );

	//construct widget for this slot
	virtual TSharedRef<SWidget> MakeWidget();

	//still relevant, refresh triggered if nolonger relevant
	virtual bool IsRelevant() { return true; }

protected:
	virtual void AddToHeading( TSharedPtr<SHorizontalBox> heading_row ) {}
	FString GetParameter( FName name, FString fallback=TEXT("") );

};


// info slot with action button(s)
//
class FInfoSlotActions : public FInfoSlot
{
public:
	FInfoSlotActions( FString heading, FString content )
		: FInfoSlot( heading, content )
	{
	}

protected:
	virtual int GetNumActions() { return 1;/*always at least one, helpful default*/ }
	virtual FText GetActionName( int index ) = 0;
	virtual void ExecuteAction( int index ) = 0;
	
	//impl
	virtual void AddToHeading( TSharedPtr<SHorizontalBox> heading_row ) override;

private:
	FReply OnClick( int action_index );
};


// open a URL
//
class FInfoSlot_Link : public FInfoSlotActions
{
public:
	FInfoSlot_Link( FString heading, FString content, FString url=TEXT("") )
		: FInfoSlotActions( heading, content )
	{
		SetParameter( FName( "Icon" ), TEXT( "Web" ) );
		if(!url.IsEmpty())
		{
			SetParameter( FName( "URL" ), url );
		}
	}

protected:
	virtual FText GetActionName( int index ) override;
	virtual void ExecuteAction( int index ) override;
};
