//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

//module
#include "InfoSlot.h"
#include "ApparanceUnrealEditor.h"

//unreal

#define LOCTEXT_NAMESPACE "ApparanceUnreal"


static FTextBlockStyle* Style_TitleText()
{
	static FTextBlockStyle* text = nullptr;
	if(!text)
	{
		text = new FTextBlockStyle( APPARANCE_STYLE.GetWidgetStyle<FTextBlockStyle>( "NormalText.Important" ) );
		text->SetFontSize( 14.0f );
	}
	return text;
}

static FTextBlockStyle* Style_HeadingText()
{
	static FTextBlockStyle* text = nullptr;
	if(!text)
	{
		//text = new FTextBlockStyle( FEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>( "NormalText.Important" ) );
		text = new FTextBlockStyle( APPARANCE_STYLE.GetWidgetStyle<FTextBlockStyle>( "RichTextBlock.Bold" ) );
		//text->SetFontSize( 12.0f );
	}
	return text;
}



TSharedRef<SWidget> FInfoSlot::MakeWidget()
{
	TSharedPtr<SVerticalBox> box;
	TSharedRef<SWidget> widget = SNew( SBorder )
		.BorderImage( APPARANCE_STYLE.GetBrush( "ToolPanel.DarkGroupBorder" ) )
		.VAlign( EVerticalAlignment::VAlign_Top )
	[
		SAssignNew( box, SVerticalBox )
	];

	//heading
	if(!Heading.IsEmpty())
	{
		TSharedPtr<SHorizontalBox> row;
		box->AddSlot()
		.AutoHeight()
		.VAlign( EVerticalAlignment::VAlign_Top )
		.Padding(4)
		[
			SAssignNew(row,SHorizontalBox)
		];

		//icon
		FString icon = GetParameter( FName( "Icon" ), TEXT( "" ) );
		bool has_icon = !icon.IsEmpty();
		if(has_icon)
		{
			FName icon_asset = FName( TEXT( "Apparance.Icon." ) + icon );
			row->AddSlot()
				.Padding( FMargin( 0, 0, 5, 0 ) )
				.AutoWidth()
				[
					SNew( SImage ).Image( FApparanceUnrealEditorModule::GetStyle()->GetBrush( icon_asset ) )
				];
		}

		//heading
		FTextBlockStyle* heading_style = has_icon ? Style_HeadingText() : Style_TitleText();
		ETextJustify::Type justification = ETextJustify::Left;// has_icon ? ETextJustify::Left : ETextJustify::Center;
		row->AddSlot()
			.VAlign( VAlign_Center )
			.Padding( FMargin(6,0) )
			.FillWidth(1.0f)
			[
				SNew( STextBlock )
				.Text( FText::FromString( Heading ) )
				.Justification( justification )
				.TextStyle( heading_style )
			];

		//derived additions
		AddToHeading( row );
	}

	//content
	if(!Content.IsEmpty())
	{
		box->AddSlot()
		.AutoHeight()
		.VAlign( EVerticalAlignment::VAlign_Top )
		.Padding(10)
		[
			SNew( SEditableText )
			.IsReadOnly( true )
			.Text( FText::FromString( Content ) )
		];
	}

	return widget;
}

// common parameters
//
void FInfoSlot::SetParameter( FName name, FString value )
{
	Parameters.Add( name, value );
}

// parameter lookup helper
//
FString FInfoSlot::GetParameter( FName name, FString fallback )
{
	FString* value= Parameters.Find( name );
	if(value)
	{
		return *value;
	}
	return fallback;
}




///////////////////////// ACTIONS //////////////////////////

// add action buttons
//
void FInfoSlotActions::AddToHeading( TSharedPtr<SHorizontalBox> heading_row )
{
	int num = GetNumActions();
	for(int i=0 ; i<num ; i++)
	{
		heading_row->AddSlot()
		.VAlign( VAlign_Center )
		.AutoWidth()
		.Padding(FMargin((i==0)?0:4,0,0,0))
		[
			SNew( SButton )
			.Content()
			[
				SNew( STextBlock )
				.Text( GetActionName( i ) )
			]
			.OnClicked( this, &FInfoSlotActions::OnClick, i )
		];
	}
}

// activate action
//
FReply FInfoSlotActions::OnClick( int action_index )
{
	ExecuteAction( action_index );
	return FReply::Handled();
}


//////////////////////////// LINK //////////////////////////////

FText FInfoSlot_Link::GetActionName( int index )
{
	return LOCTEXT( "AboutOpenLink", "Open in web browser" );
}
void FInfoSlot_Link::ExecuteAction( int index )
{
	FString url = GetParameter( FName( "URL" ) );
	if(!url.IsEmpty())
	{
		FPlatformProcess::LaunchURL( *url, NULL, NULL );
	}
}






#undef LOCTEXT_NAMESPACE
