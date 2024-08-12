//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

//module
#include "ApparanceAbout.h"
#include "ApparanceUnreal.h"
#include "ApparanceUnrealEditor.h"
#include "Apparance.h"
#include "ApparanceEngineSetup.h"
#include "ApparanceResourceList.h"
#include "ApparanceEntity.h"
#include "ApparanceResourceListFactory.h"
#include "ResourceList/ApparanceResourceListEntry.h"
#include "ResourceList/ApparanceResourceListEntry_ResourceList.h"
#include "ApparanceUnrealVersioning.h"

//unreal
#include "Misc/Paths.h"
#include "Misc/EngineVersion.h"
#include "Interfaces/IPluginManager.h"
#include "Components/ActorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "UObject/SavePackage.h"
//unreal: slate
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "EditorStyleSet.h"
#include "Fonts/SlateFontInfo.h"
//unreal: ui
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SWindow.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
//unreal: misc
#include "UnrealEdMisc.h"
#include "IDocumentation.h"
#include "Misc/FileHelper.h"
#include "EngineUtils.h"
#include "Misc/EngineVersionComparison.h"
//setup
#include "ISettingsModule.h"
#include "IAssetTools.h"
#include "IAssetTypeActions.h"
#include "AssetToolsModule.h"
#if UE_VERSION_AT_LEAST( 5, 0, 0 )
#include "AssetRegistry/AssetRegistryModule.h"
#else
#include "AssetRegistryModule.h"
#endif

#define LOCTEXT_NAMESPACE "ApparanceUnreal"

static FTextBlockStyle* overlay_style = nullptr;
const float RefreshPeriod = 1.0f;	//refresh check every second
static bool g_bNeedsRestart = false;

void SApparanceAbout::Construct(const FArguments& InArgs)
{
	TickTimer = 0;

	//version info
	FString s;
	const Apparance::IParameterCollection* version = FApparanceUnrealModule::GetEngine()->GetVersionInformation();
	if (version)
	{
		int count = version->BeginAccess();
		for (int i = 0; i < count; i++)
		{
			if (version->GetType(i) == Apparance::Parameter::String)	//all should be strings
			{
				//name
				const char* pname = version->GetName(i);
				FString name(UTF8_TO_TCHAR(pname));

				//value
				FString value;
				int num_chars = 0;
				TCHAR* pbuffer = nullptr;
				if (version->GetString( i, 0, nullptr, &num_chars))
				{
					value = FString::ChrN(num_chars, TCHAR(' '));
					pbuffer = value.GetCharArray().GetData();
					version->GetString( i, num_chars, pbuffer);
				}

				//append
				s.Appendf(TEXT("%s: %s\n"), *name, *value);
			}
		}
		version->EndAccess();
		delete version;
	}
	FText Version = FText::FromString( s );
	FText product = FApparanceUnrealModule::GetModule()->GetProductText();

	//hide Commercial text, full version doesn't need a splash
	FString product_check = product.ToString();
	if(product_check.Equals( TEXT("COMMERCIAL")))
	{
		product = FText::GetEmpty();
	}

	if(!overlay_style)
	{
		overlay_style = new FTextBlockStyle( APPARANCE_STYLE.GetWidgetStyle<FTextBlockStyle>( "NormalText.Important" ) );
		overlay_style->SetFontSize( 16.0f );
	}

	//layout
	ChildSlot
	[
		SNew(SVerticalBox)

		//logo space (background above) and version info
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)
			.HeightOverride(85.0f)
			[
				SNew(SHorizontalBox)
				//logo
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(5.0f, 10.0f, 5.0f, 0.0f))
				[
					SNew( SOverlay )
					//logo
					+ SOverlay::Slot()
					[
						SNew( SBox )
						.WidthOverride( 275.0f )
						.HeightOverride( 80.0f )
						.VAlign( VAlign_Center )
						[
							SNew( SImage ).Image( FApparanceUnrealEditorModule::GetStyle()->GetBrush( TEXT( "Apparance.Logo.Large" ) ) )
						]
					]
					//splash
					+ SOverlay::Slot()
					.VAlign( EVerticalAlignment::VAlign_Bottom )
					.HAlign( EHorizontalAlignment::HAlign_Right )
					[
						SNew( STextBlock )
						.Text( product )
						.ColorAndOpacity( FLinearColor::Red )
						.TextStyle( overlay_style )
					]

				]
				//info
				+SHorizontalBox::Slot()
				.FillWidth(1.f)
				.Padding(FMargin(5.0f, 5.0f, 5.0f, 0.0f))
				[
					SNew(SBorder)
					.BorderImage( APPARANCE_STYLE.GetBrush("ToolPanel.DarkGroupBorder"))
					.Padding(FMargin(10.f))
					[
						SNew( SRichTextBlock)//SMultiLineEditableText)//SEditableText)
						//.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
						//.IsReadOnly(true)
						.Text( Version )
//						.Style( FEditorStyle::Get(), "MessageLog" )
						.AutoWrapText(true)
					]
				]
			]
		]

		//categories
		+SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(10.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				// categories menu
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SVerticalBox)
					//----------------- Setup -------------------
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FMargin(0,0,0,7.5f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.0f, 0.0f, 4.0f, 0.0f)
						.VAlign(VAlign_Center)
						[
							SNew(SImage)
								.Image( APPARANCE_STYLE.GetBrush("TreeArrow_Collapsed_Hovered"))
								.Visibility(this, &SApparanceAbout::HandlePageLinkImageVisibility, EApparanceAboutPage::Setup)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SHyperlink)
							.Text(FText::FromString(TEXT("Setup Wizard")))
							.OnNavigate(this, &SApparanceAbout::SetPage, EApparanceAboutPage::Setup)
						]
					]
					//----------------- About -------------------
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FMargin(0,0,0,7.5f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.0f, 0.0f, 4.0f, 0.0f)
						.VAlign(VAlign_Center)
						[
							SNew(SImage)
								.Image( APPARANCE_STYLE.GetBrush("TreeArrow_Collapsed_Hovered"))
								.Visibility(this, &SApparanceAbout::HandlePageLinkImageVisibility, EApparanceAboutPage::About )
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SHyperlink)
							.Text(FText::FromString(TEXT("About Apparance")))
							.OnNavigate(this, &SApparanceAbout::SetPage, EApparanceAboutPage::About )
						]
					]
					//----------------- Release Notes -------------------
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FMargin(0,0,0,7.5f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.0f, 0.0f, 4.0f, 0.0f)
						.VAlign(VAlign_Center)
						[
							SNew(SImage)
								.Image( APPARANCE_STYLE.GetBrush("TreeArrow_Collapsed_Hovered"))
								.Visibility(this, &SApparanceAbout::HandlePageLinkImageVisibility, EApparanceAboutPage::ReleaseNotes )
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SHyperlink)
							.Text(FText::FromString(TEXT("Release Notes")))
							.OnNavigate(this, &SApparanceAbout::SetPage, EApparanceAboutPage::ReleaseNotes )
						]
					]
					//----------------- Status -------------------
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FMargin(0,0,0,7.5f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.0f, 0.0f, 4.0f, 0.0f)
						.VAlign(VAlign_Center)
						[
							SNew(SImage)
								.Image( APPARANCE_STYLE.GetBrush("TreeArrow_Collapsed_Hovered"))
								.Visibility(this, &SApparanceAbout::HandlePageLinkImageVisibility, EApparanceAboutPage::Status)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SHyperlink)
							.Text(FText::FromString(TEXT("Status")))
							.OnNavigate(this, &SApparanceAbout::SetPage, EApparanceAboutPage::Status)
						]
					]
					//----------------- License -------------------
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FMargin(0,0,0,7.5f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.0f, 0.0f, 4.0f, 0.0f)
						.VAlign(VAlign_Center)
						[
							SNew(SImage)
								.Image( APPARANCE_STYLE.GetBrush("TreeArrow_Collapsed_Hovered"))
								.Visibility(this, &SApparanceAbout::HandlePageLinkImageVisibility, EApparanceAboutPage::License)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SHyperlink)
							.Text(FText::FromString(TEXT("License & Usage")))
							.OnNavigate(this, &SApparanceAbout::SetPage, EApparanceAboutPage::License)
						]
					]
					//----------------- Credits -------------------
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FMargin(0,0,0,7.5f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.0f, 0.0f, 4.0f, 0.0f)
						.VAlign(VAlign_Center)
						[
							SNew(SImage)
								.Image( APPARANCE_STYLE.GetBrush("TreeArrow_Collapsed_Hovered"))
								.Visibility(this, &SApparanceAbout::HandlePageLinkImageVisibility, EApparanceAboutPage::Credits)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SHyperlink)
							.Text(FText::FromString(TEXT("Credits")))
							.OnNavigate(this, &SApparanceAbout::SetPage, EApparanceAboutPage::Credits)
						]
					]
					//----------------- Attribution -------------------
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FMargin(0,0,0,7.5f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.0f, 0.0f, 4.0f, 0.0f)
						.VAlign(VAlign_Center)
						[
							SNew(SImage)
								.Image( APPARANCE_STYLE.GetBrush("TreeArrow_Collapsed_Hovered"))
								.Visibility(this, &SApparanceAbout::HandlePageLinkImageVisibility, EApparanceAboutPage::Attribution)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SHyperlink)
							.Text(FText::FromString(TEXT("Attribution")))
							.OnNavigate(this, &SApparanceAbout::SetPage, EApparanceAboutPage::Attribution)
						]
					]
					//------------------------------------------------
					+SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						SNew(SSpacer)
					]
					+SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.Text(LOCTEXT("Close", "Close"))
						.ButtonColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
						.OnClicked(this, &SApparanceAbout::OnClose)
					]

				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SSpacer)
					.Size(FVector2D(10.0f, 0.0f))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(10.0f, 0.0f, 10.0f, 0.0f))
			[
				SNew(SSeparator)
				.Orientation(Orient_Vertical)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(5.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SNew(SBorder)
					//.BorderImage(FEditorStyle::GetBrush("ToolPanel.DarkGroupBorder"))
					.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
					.Padding(FMargin(4.f))
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						[
							SAssignNew(Page, SVerticalBox)
						]
					]
				]
			]
		]
	];
}

EVisibility SApparanceAbout::HandlePageLinkImageVisibility( EApparanceAboutPage page ) const
{
	if (page==CurrentPage)
	{
		return EVisibility::Visible;
	}
	
	return EVisibility::Hidden;
}

// close our containing window
FReply SApparanceAbout::OnClose()
{
	MyWindow->RequestDestroyWindow();
	return FReply::Handled();
}

// want to catch Esc
//
FReply SApparanceAbout::OnKeyUp( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent )
{
	if(InKeyEvent.GetKey() == EKeys::Escape)
	{
		OnClose();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

// regular ticks
//
void SApparanceAbout::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	TickTimer += InDeltaTime;
	if(TickTimer > RefreshPeriod)
	{
		TickTimer = 0;

		//updates
		CheckPageRefresh();
	}
}

// see if any page slots are out of date
//
void SApparanceAbout::CheckPageRefresh()
{
	//evaluate slots
	bool refresh = false;
	for(int i = 0; i < Slots.Num(); i++)
	{
		//any become irrellevant?
		if(!Slots[i]->IsRelevant())
		{
			refresh = true;
			break;
		}
	}

	//refresh
	if(refresh)
	{
		SetPage( CurrentPage );
		
		//bring to front
		TSharedRef<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow( AsShared() ).ToSharedRef();
		ParentWindow->BringToFront();
	}

}

// turn to and build a specific page
//
void SApparanceAbout::SetPage( EApparanceAboutPage page)
{
	CurrentPage = page;
	Slots.Reset();
	switch (page)
	{
		case EApparanceAboutPage::About :
			Slots = LoadPage( "ApparanceAbout.txt");
			break;
		case EApparanceAboutPage::ReleaseNotes:
			Slots = LoadPage("ApparanceReleaseNotes.txt");
			break;
		case EApparanceAboutPage::Setup:
			Slots = BuildSetupPage();
			break;
		case EApparanceAboutPage::Status:
			Slots = BuildStatusPage();
			break;
		case EApparanceAboutPage::License:
			Slots = LoadPage("ApparanceLicense.txt");
			break;
		case EApparanceAboutPage::Credits:
			Slots = LoadPage("ApparanceCredits.txt");
			break;		
		case EApparanceAboutPage::Attribution:
			Slots = LoadPage("ApparanceAttribution.txt");
			break;		
	}

	//update UI
	Page->ClearChildren();
	for(int i = 0; i < Slots.Num(); i++)
	{
		TSharedRef<SWidget> widget = Slots[i]->MakeWidget();
		Page->AddSlot()
			.AutoHeight()
			[
				SNew( SBox )
				.Padding( FMargin( 0, 0, 0, 2 ) )
				[
					widget
				]
			];
	}
	//Page->Invalidate( EInvalidateWidgetReason::Layout );
}

// text file based page
//
TArray<TSharedPtr<FInfoSlot>> SApparanceAbout::LoadPage(FString filename)
{
	// text docs located in Resources folder next to Content folder (of the plugin)
	FString plugin_content = IPluginManager::Get().FindPlugin(TEXT("ApparanceUnreal"))->GetContentDir();
	FString resources_dir = FPaths::Combine(plugin_content , TEXT("../Resources"));
	FPaths::NormalizeDirectoryName(resources_dir);
	FPaths::CollapseRelativeDirectories(resources_dir);

	//file path
	FString file_path = FPaths::Combine(resources_dir, filename);

	//load text
	TArray<FString> lines;
	const bool bReadFile = FFileHelper::LoadFileToStringArray(lines, *file_path);

	//parse into info slots
	TArray<TSharedPtr<FInfoSlot>> slots;
	ParseSlots( lines, slots );

	//set
	return slots;
}

// turn lines of text into slots
//
void SApparanceAbout::ParseSlots( const TArray<FString>& lines, TArray<TSharedPtr<FInfoSlot>>& slots_out )
{
	//state machine
	int iline = 0;
	int state = 0;
	int count_blanks = 0;
	int count_content = 0;
	bool done = false;
	TSharedPtr<FInfoSlot> slot;
	while(!done)
	{
		bool is_more = iline < lines.Num();
		FString line = is_more?lines[iline]:TEXT("");
		bool is_slot = line.StartsWith( TEXT( "----" ) );
		bool is_param = line.StartsWith( TEXT( "#" ) );
		bool is_content = !is_slot && !is_param;
		bool is_blank = line.IsEmpty();
		if(!is_more)
		{
			state = 99;
		}

		switch(state)
		{
			//---- looking for a new slot ----
			case 0: 
			{
				if(is_slot)
				{
					//explicit slot
					slot = ParseSlot( line );
				}
				else
				{
					//no initial slot, all content
					slot = MakeShareable( new FInfoSlot( line, TEXT("") ) );
				}
				slots_out.Add( slot );
				state = 1;
				count_content = 0;
				iline++;
				break;
			}

			//---- new slot found ----
			case 1:
			{
				if(is_slot)
				{
					state = 0;
				}
				else if(is_param)
				{
					FName name;
					FString value;
					ParseParameter( line, name, value );
					if(!name.IsNone())
					{
						slot->SetParameter( name, value );
					}
					iline++;
				}
				else if(is_blank)
				{
					count_blanks = 1;
					state = 2;
					iline++;
				}
				else
				{
					//content!
					count_content++;
					if(!slot->Content.IsEmpty())
					{
						slot->Content.Append( TEXT( "\n" ) );
					}
					slot->Content.Append( line );
					iline++;
				}
				break;
			}

			//---- counting blank lines ----
			case 2:
			{
				if(is_blank)
				{
					count_blanks++;
					iline++;
				}
				else
				{
					if(is_slot)
					{
						//discard trailing blanks (i.e. after content)
						state = 0;
					}
					else if(count_content == 0)
					{
						//discard leading blanks (i.e. before content)
						state = 1;
					}
					else
					{
						//part of the content
						for(int i = 0; i < count_blanks; i++)
						{
							slot->Content.Append( TEXT( "\n" ) );
						}
						state = 1;
					}
				}
				break;
			}

			case 99: //no more lines
			{
				done = true;
				break;
			}
		}
	}
}

// turn an explicit slot line into slot object
// e.g.
// ---- WARNING: Something went wrong!
//
TSharedPtr<FInfoSlot> SApparanceAbout::ParseSlot( FString line )
{
	//start of content
	int istart = 0;
	while(istart < line.Len())
	{
		if(line[istart] != '-' && line[istart] != ' ')
		{
			break;
		}
		istart++;
	}

	//has category?
	FString type;
	FString heading;
	int isep = 0;
	if(line.FindChar( ':', isep ))
	{
		type = line.Mid( istart, isep - istart ).TrimStartAndEnd();
		heading = line.Mid( isep + 1 ).TrimStartAndEnd();
	}
	else
	{
		heading = line.Mid( istart ).TrimStartAndEnd();
	}

	//make info slot
	FInfoSlot* pslot;
	if(type == TEXT( "Link" ))
	{
		pslot = new FInfoSlot_Link( heading, TEXT( "" ) );
	}
	else
	{
		pslot = new FInfoSlot( heading, TEXT( "" ), type );
	}

	//interpret
	return MakeShareable( pslot );
}

// turn a parmeter line into name/value
// e.g.
// # Name = Value
void SApparanceAbout::ParseParameter( const FString& line, FName& name_out, FString& value_out )
{
	int ihash, iequal;
	if(line.FindChar( '#', ihash ) && line.FindChar( '=', iequal ))
	{
		FString sname = line.Mid( ihash + 1, iequal - ihash - 1 ).TrimStartAndEnd();
		name_out = FName( sname );
		value_out = line.Mid( iequal + 1 ).TrimStartAndEnd();
	}
	else
	{
		UE_LOG( LogApparance, Error, TEXT( "Mal-formed info slot parameter: %s" ), *line );
		name_out = NAME_None;
		value_out = TEXT( "" );
	}
}


static FString status_text;
static void AddStatusLine(int indent_level, const TCHAR* name, const TCHAR* property)
{
	if(!status_text.IsEmpty())
	{
		status_text.Append( "\n" );
	}
	status_text.Appendf(TEXT("%s%s: %s"), *FString::ChrN(indent_level,'\t'), name, property);
}
static void AddStatusLine(int indent_level, const TCHAR* name, int value)
{
	if(!status_text.IsEmpty())
	{
		status_text.Append( "\n" );
	}
	status_text.Appendf(TEXT("%s%s: %i"), *FString::ChrN(indent_level,'\t'), name, value);
}
static void AddStatusLine(int indent_level, const TCHAR* name, bool value)
{
	if(!status_text.IsEmpty())
	{
		status_text.Append( "\n" );
	}
	status_text.Appendf(TEXT("%s%s: %s"), *FString::ChrN(indent_level,'\t'), name, value?TEXT("Enabled"):TEXT("Disabled"));
}
static void AddStatusLine(int indent_level, const TCHAR* name, const UObject* value)
{
	if(!status_text.IsEmpty())
	{
		status_text.Append( "\n" );
	}
	if (IsValid( value ))
	{
		status_text.Appendf(TEXT("%s%s: %s"), *FString::ChrN(indent_level, '\t'), name, *value->GetName());
	}
	else
	{
		status_text.Appendf(TEXT("%s%s: Not set up"), *FString::ChrN(indent_level, '\t'), name);
	}
}

// a live status page
//
TArray<TSharedPtr<FInfoSlot>> SApparanceAbout::BuildStatusPage()
{
	TArray<TSharedPtr<FInfoSlot>> slots;

	//runtime
	status_text.Reset();
	FApparanceUnrealModule* runtime = FApparanceUnrealModule::GetModule();
	AddStatusLine(0, TEXT("Procedures Path"), *runtime->GetProceduresPath());
	AddStatusLine(0, TEXT("Procedure Count"), runtime->GetLibrary()->GetProcedureCount());
	AddStatusLine(0, TEXT("Resource Root"), runtime->GetResourceRoot());
	slots.Add( MakeShareable( new FInfoSlot( "Runtime", status_text, "Stats" ) ) );
	
	//editor
	status_text.Reset();
	FApparanceUnrealEditorModule* editor = FApparanceUnrealEditorModule::GetModule();
	AddStatusLine(0, TEXT("Editor Installed"), editor->GetEditor()->IsInstalled());
	slots.Add( MakeShareable( new FInfoSlot( "Editor", status_text, "Stats" ) ) );

	//entities in the current world...
	status_text.Reset();
	AddEntityStats();
	slots.Add( MakeShareable( new FInfoSlot( "World", status_text, "Stats" ) ) );

	//done
	status_text.Reset();
	return slots;
}

// stats about entites in the scene
//
void SApparanceAbout::AddEntityStats()
{
	int entity_count = 0;
	int component_count = 0;
	int static_mesh_count = 0;
	int mesh_types = 0;
	int instanced_static_mesh_count = 0;
	int instanced_mesh_count = 0;
	int instanced_mesh_types = 0;
	int blueprint_count = 0;
	int procedural_mesh_count = 0;
	int proc_mesh_sections = 0;
	int triangle_count = 0;
	int vertex_count = 0;
	int material_types = 0;
	int other_component_count = 0;

	UWorld* pworld = GEditor->GetEditorWorldContext().World();
	if (pworld)
	{
		TSet<UMaterialInterface*> materials;
		TSet<UStaticMesh*> meshes;
		TSet<UStaticMesh*> instancedmeshes;
		TMap<const UClass*, int> others;
		//all the entities
		for (TActorIterator<AApparanceEntity> aeit(pworld); aeit; ++aeit)
		{
			entity_count++;

			//all the added components
			const TSet<UActorComponent*>& components = aeit->GetComponents();
			for (UActorComponent* component : components)
			{
				component_count++;
				if (component->IsA(UInstancedStaticMeshComponent::StaticClass()))
				{
					instanced_static_mesh_count++;
					UInstancedStaticMeshComponent* pismc = Cast<UInstancedStaticMeshComponent>(component);
					instanced_mesh_count += pismc->GetInstanceCount();
					if(pismc->GetStaticMesh())
					{
						instancedmeshes.Add( pismc->GetStaticMesh() );
					}
				}
				else if (component->IsA(UStaticMeshComponent::StaticClass()))
				{
					static_mesh_count++;
					UStaticMeshComponent* smc = Cast<UStaticMeshComponent>(component);
					if (smc->GetStaticMesh())
					{
						meshes.Add(smc->GetStaticMesh());
					}
				}
				//blueprints, how?
				//else if(component->IsA(UActor
				//{
				//}
				else if (component->IsA(UProceduralMeshComponent::StaticClass()))
				{
					procedural_mesh_count++;
					UProceduralMeshComponent* pmc = Cast<UProceduralMeshComponent>(component);
					int num_sections = pmc->GetNumSections();
					proc_mesh_sections += num_sections;
					for (int i = 0; i < num_sections; i++)
					{
						FProcMeshSection* pms = pmc->GetProcMeshSection(i);
						int num_verts = pms->ProcVertexBuffer.Num();
						int num_indices = pms->ProcIndexBuffer.Num();
						int num_tris = num_indices / 3;
						triangle_count += num_tris;
						vertex_count += num_verts;						
					}
					TArray<UMaterialInterface*> mats = pmc->GetMaterials();
					for (int i = 0; i < mats.Num(); i++)
					{
						if (mats[i])
						{
							materials.Add(mats[i]);
						}
					}
				}
				else
				{
					other_component_count++;
					const UClass* c = component->GetClass();
					int* pcount = others.Find( c );
					if(pcount)
					{
						others[c] = *pcount + 1; //more of the same
					}
					else
					{
						others.Add( c, 1 ); //first one
					}
				}
			}
		}
		material_types = materials.Num();
		mesh_types = meshes.Num();
		instanced_mesh_types = instancedmeshes.Num();

		//add stats
		AddStatusLine(0, TEXT("World"), *pworld->GetName());
		AddStatusLine(1, TEXT("Entities"), entity_count);
		AddStatusLine(1, TEXT("Components"), component_count);
		AddStatusLine(2, TEXT("Static Meshes"), static_mesh_count);
		AddStatusLine(3, TEXT("Mesh Types"), mesh_types);
		AddStatusLine(2, TEXT("Instanced Static Meshes"), instanced_static_mesh_count );
		AddStatusLine(3, TEXT("Mesh Instances"), instanced_mesh_count);
		AddStatusLine(3, TEXT("Mesh Types" ), instanced_mesh_types );
		AddStatusLine(2, TEXT("Blueprint Actors"), blueprint_count);
		AddStatusLine(2, TEXT("Procedural Meshes"), procedural_mesh_count);
		AddStatusLine(3, TEXT("Sections"), proc_mesh_sections );
		AddStatusLine(3, TEXT("Triangles"), triangle_count);
		AddStatusLine(3, TEXT("Vertices"), vertex_count);
		AddStatusLine(3, TEXT("Material Types"), material_types);
		AddStatusLine(2, TEXT("Other Components"), other_component_count);
		for(auto it : others)
		{
			const UClass* c = it.Key;
			int count = it.Value;
			AddStatusLine( 3, *c->GetName(), count );
		}
	}	
}



// base for setup info slots, icon, buttons, etc.
//
class FInfoSlotSetup : public FInfoSlotActions
{
	TArray<FText> Actions;
public:
	FInfoSlotSetup( FText heading, FText content, FString icon=TEXT("Setup") )
		: FInfoSlotActions( heading.ToString(), content.ToString() )
	{
		SetParameter( FName( "Icon" ), icon );
	}
protected:
	void AddAction( FText name )
	{
		Actions.Add( name );
	}

	virtual int GetNumActions() override
	{
		return Actions.Num();
	}
	virtual FText GetActionName( int index ) override
	{
		return Actions[index];
	}
	virtual void ExecuteAction( int index )
	{
		switch(index + 1)
		{
		case 1: Action1(); break;
		case 2: Action2(); break;
		case 3: Action3(); break;
		}
	}

public:
	virtual void Action1() = 0;
	virtual void Action2() {}
	virtual void Action3() {}
};

//---------- Open Project Settings -----------
class FInfoSlot_OpenProjectSettings : public FInfoSlotSetup
{
public:
	FInfoSlot_OpenProjectSettings()
		: FInfoSlotSetup(
			LOCTEXT( "SetupOpenProjectSettings", "Manually configure the Apparance plugin in the project settings panel" ),
			FText()
		)
	{
		AddAction( LOCTEXT( "SetupOpenProjectSettings", "Open Project Settings" ) );
	}
	virtual void Action1() override
	{
		//open project settings
		FModuleManager::LoadModuleChecked<ISettingsModule>( "Settings" ).ShowViewer( FName( "Project" ), FName( "Plugins" ), FName( "Apparance" ) );
	}
};

//---------- Open Root Resource List -----------
class FInfoSlot_OpenRootResourceList : public FInfoSlotSetup
{
public:
	FInfoSlot_OpenRootResourceList()
		: FInfoSlotSetup(
			LOCTEXT( "SetupOpenRootResourceList", "Manually configure the project root Resource List" ),
			FText()
		)
	{
		AddAction( LOCTEXT( "SetupOpenRootResourceList", "Open Root Resource List" ) );
	}
	virtual void Action1() override
	{
		//open root resource list
		UApparanceResourceList* preslist = UApparanceEngineSetup::GetResourceRoot();
		if(preslist)
		{
			//open
			TArray<FName> assets;
			assets.Add( preslist->GetFName() );
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorsForAssets( assets );
		}
	}
};

//---------- Restart needed -----------
class FInfoSlot_RestartNeeded : public FInfoSlotSetup
{
public:
	FInfoSlot_RestartNeeded()
		: FInfoSlotSetup(
			LOCTEXT( "SetupRestartNeeded", "Editor needs to be restarted for changes to take effect" ),
			FText(),
			TEXT("Warning")
		)
	{
		AddAction( LOCTEXT( "SetupRestartEditorButton", "Restart Editor" ) );
	}
	virtual void Action1() override
	{
		FUnrealEdMisc::Get().RestartEditor( false );
	}
};

//---------- Procedure Directory -------------
//
class FInfoSlot_AutomaticProcedureLocation : public FInfoSlotSetup
{
public:
	virtual bool IsRelevant() override { return IsNeeded(); }
	static bool IsNeeded()
	{
		return !HasPath() || !DirectoryExists();
	}

	//info
	static bool HasPath()
	{
		return !UApparanceEngineSetup::GetProceduresDirectory().IsEmpty();
	}
	static bool DirectoryExists()
	{
		return UApparanceEngineSetup::DoesProceduresDirectoryExist();
	}
	FInfoSlot_AutomaticProcedureLocation()
		: FInfoSlotSetup(
			LOCTEXT( "SetupProcedureLocationWarning", "Procedures directory not set" ),
			LOCTEXT( "SetupProcedureLocationWarningDetails", "The storage location for Procedures need to be set.\nThis needs to be a directory within the project Content folder\nYou can either use the default location and set this up automatically, or\nopen the project settings panel and configure it manually." ),
			TEXT( "Error" )
		)
	{
		AddAction( LOCTEXT( "SetupProcedureLocationDefaultButton", "Configure Automatically" ) );
	}
	virtual void Action1() override
	{
		//point project settings at it
		FString proc_dir = UApparanceEngineSetup::GetProceduresDirectory();
		if(proc_dir.IsEmpty())
		{
			//default
			proc_dir = "Apparance/Procedures/";
			UApparanceEngineSetup::SetProceduresDirectory( proc_dir );
		}
			
		//create directory
		FString proc_path = FPaths::Combine( FPaths::ProjectContentDir(), proc_dir );
		FPaths::NormalizeDirectoryName( proc_path );
		FPaths::CollapseRelativeDirectories( proc_path );
		if(!FPaths::DirectoryExists( proc_path ))
		{
			FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree( *proc_path );
		}
		
		g_bNeedsRestart = true;
	}
};

//---------- Project Resources -------------
//
class FInfoSlot_AutomaticProjectResources : public FInfoSlotSetup
{
public:
	virtual bool IsRelevant() override { return IsNeeded(); }
	static bool IsNeeded()
	{
		return !ProjectResourceIsSet();
	}
	static bool ProjectResourceIsSet()
	{
		return UApparanceEngineSetup::GetResourceRoot() != nullptr;
	}

	FInfoSlot_AutomaticProjectResources()
		: FInfoSlotSetup(
				LOCTEXT( "SetupProjectResourcesWarning", "Root project Resources not set up" ),
				LOCTEXT( "SetupProjectResourcesWarningDetails", "A root Resource List is needed to provide access to Unreal assets for procedures.\nThis project level Resource List can reference other Resource Lists as well\nas containing it's own asset references.\nEither manually create a project Resource List and point the project settings at it,\nor have one created for you automatically." ),
				TEXT( "Error" )	
		)
	{
		AddAction( LOCTEXT( "SetupProjectResourcesDefaultButton", "Configure Automatically" ) );
	}
	virtual void Action1() override
	{
		//ensure the default directory exists to put it in
		FString apparance_path = FPaths::Combine( FPaths::ProjectContentDir(), TEXT("Apparance") );
		FPaths::NormalizeDirectoryName( apparance_path );
		FPaths::CollapseRelativeDirectories( apparance_path );
		if(!FPaths::DirectoryExists( apparance_path ))
		{
			FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree( *apparance_path );
		}

		//create the asset
		FString BaseName = TEXT("ProjectResources");
		FString PackageName = TEXT("/Game/Apparance/");
		PackageName += BaseName;
#if UE_VERSION_OLDER_THAN(4,26,0)
		UPackage* Package = CreatePackage( nullptr, *PackageName );
#else
		UPackage* Package = CreatePackage( *PackageName );
#endif
		UApparanceResourceListFactory* Factory = GetMutableDefault<UApparanceResourceListFactory>();
		UApparanceResourceList* Asset = (UApparanceResourceList*)Factory->FactoryCreateNew( UApparanceResourceList::StaticClass(), Package, *BaseName, RF_Standalone | RF_Public, nullptr, GWarn );
		if(Asset)
		{
			//created
			FAssetRegistryModule::AssetCreated( Asset );
			Package->FullyLoad();
			Package->SetDirtyFlag( true );

			//specific setup needed
			Asset->Editor_InitForEditing();
			Asset->Resources->SetName( TEXT( "Project" ) ); //root of project resources is just called "Project" by default

			//save
			FString PackageFileName = FPackageName::LongPackageNameToFilename( PackageName, FPackageName::GetAssetPackageExtension() );
#if UE_VERSION_AT_LEAST(5,0,0)
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = EObjectFlags::RF_Public | EObjectFlags::RF_Standalone;
			SaveArgs.SaveFlags = SAVE_None;// SAVE_NoError;
			SaveArgs.bForceByteSwapping = true;
			SaveArgs.bWarnOfLongFilename = true;
			if(!UPackage::SavePackage( Package, Asset, *PackageFileName, SaveArgs ))
#else
			if(!UPackage::SavePackage( Package, Asset, EObjectFlags::RF_Public | EObjectFlags::RF_Standalone, *PackageFileName, GError, nullptr, true, true, SAVE_NoError ))
#endif
			{
				UE_LOG( LogApparance, Error, TEXT( "Failed to save new asset %s" ), *PackageFileName );
			}

			//assign to root in project settings
			UApparanceEngineSetup::SetResourceRoot( Asset );

			g_bNeedsRestart = true;
		}
	}

};

//---------- Apparance Root Ref -------------
//
class FInfoSlot_AutomaticRootReference : public FInfoSlotSetup
{
public:
	virtual bool IsRelevant() override { return IsNeeded(); }
	static bool IsNeeded()
	{
		return HasRoot() && !RootReferencesApparance();
	}
	static bool HasRoot()
	{
		return UApparanceEngineSetup::GetResourceRoot() != nullptr;
	}
	static bool RootReferencesApparance()
	{
		//does the project root resource reference the apparance resources included with the plugin?
		UApparanceResourceList* apparance_resources = GetApparanceResources();
		if(apparance_resources)
		{
			bool reference_found = false;
			UApparanceResourceList* root = UApparanceEngineSetup::GetResourceRoot();
			if(root->References)
			{
				for(int i = 0; i < root->References->Children.Num(); i++)
				{
					UApparanceResourceListEntry_ResourceList* res_list_ref = Cast< UApparanceResourceListEntry_ResourceList>( root->References->Children[i] );
					if(res_list_ref)
					{
						if(res_list_ref->GetResourceList() == apparance_resources)
						{
							reference_found = true;
							break;
						}
					}
				}
			}
			if(reference_found)
			{
				return true;
			}
		}
		return false;
	}
	static UApparanceResourceList* GetApparanceResources()
	{
#if UE_VERSION_AT_LEAST(5,0,0)
		FSoftObjectPath apparance_resources_path( "/ApparanceUnreal/Resources/ApparanceEngineResources.ApparanceEngineResources" );
#else
		FStringAssetReference apparance_resources_path( "/ApparanceUnreal/Resources/ApparanceEngineResources.ApparanceEngineResources" );
#endif
		UApparanceResourceList* apparance_resources = Cast<UApparanceResourceList>( apparance_resources_path.TryLoad() );
		return apparance_resources;
	}

	FInfoSlot_AutomaticRootReference()
		: FInfoSlotSetup(
			LOCTEXT( "SetupRootReferencesWarning", "Project should reference Apparance Plugin assets" ),
			LOCTEXT( "SetupRootReferencesWarningDetails", "The projects root Resource List should reference the Resource List included with the Apparance Plugin\n(in the Plugins Content folder).\nEither manually add to the root project Resource List,\nor have this done for you automatically." ),
			TEXT( "Warning" )
		)
	{
		AddAction( LOCTEXT( "SetupRootReferencesDefaultButton", "Configure Automatically" ) );
	}
	virtual void Action1() override
	{
		UApparanceResourceList* apparance_resources = GetApparanceResources();
		UApparanceResourceList* root = UApparanceEngineSetup::GetResourceRoot();
		if(root && apparance_resources)
		{
			root->Modify();

			//edit
			UApparanceResourceListEntry_ResourceList* ref_entry = NewObject<UApparanceResourceListEntry_ResourceList>( root, UApparanceResourceListEntry_ResourceList::StaticClass(), NAME_None, RF_Transactional );		
			ref_entry->SetAsset( apparance_resources );
			root->Editor_InitForEditing(); //ensure ready (may be new)
			root->References->Children.Add( ref_entry );

			root->MarkPackageDirty();

			//save
			UPackage* Package = root->GetOutermost();
			FString const PackageName = Package->GetName();
			FString const PackageFileName = FPackageName::LongPackageNameToFilename( PackageName, FPackageName::GetAssetPackageExtension() );
#if UE_VERSION_AT_LEAST(5,0,0)
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = EObjectFlags::RF_Public | EObjectFlags::RF_Standalone;
			SaveArgs.SaveFlags = SAVE_NoError;
			SaveArgs.bForceByteSwapping = true;
			SaveArgs.bWarnOfLongFilename = true;
			if(!UPackage::SavePackage( root->GetOutermost(), root, *PackageFileName, SaveArgs ))
#else
			if(!UPackage::SavePackage( root->GetOutermost(), root, EObjectFlags::RF_Public | EObjectFlags::RF_Standalone, *PackageFileName, GError, nullptr, true, true, SAVE_NoError ))
#endif
			{
				UE_LOG( LogApparance, Error, TEXT( "Failed to save edited asset %s" ), *PackageFileName );
			}
		}
	}

};


// static to check for setup needs
//
bool SApparanceAbout::IsSetupNeeded()
{
	return FInfoSlot_AutomaticProcedureLocation::IsNeeded()
		|| FInfoSlot_AutomaticProjectResources::IsNeeded()
		|| FInfoSlot_AutomaticRootReference::IsNeeded();
}

//============ Setup Wizard ============
//
TArray<TSharedPtr<FInfoSlot>> SApparanceAbout::BuildSetupPage()
{
	TArray<TSharedPtr<FInfoSlot>> slots;
	bool open_project_settings = false;
	bool open_root_resource_list = false;

	//title
	auto* title = new FInfoSlot( TEXT("Plugin Setup"), TEXT("") );
	slots.Add( MakeShareable( title ) );

	//procedure location
	if(FInfoSlot_AutomaticProcedureLocation::IsNeeded())
	{
		slots.Add( MakeShareable( new FInfoSlot_AutomaticProcedureLocation() ) );
		open_project_settings = true;
	}
	//project resources
	if(FInfoSlot_AutomaticProjectResources::IsNeeded())
	{
		slots.Add( MakeShareable( new FInfoSlot_AutomaticProjectResources() ) );
		open_project_settings = true;
	}
	//apparance resource refs
	if(FInfoSlot_AutomaticRootReference::IsNeeded())
	{
		slots.Add( MakeShareable( new FInfoSlot_AutomaticRootReference() ) );
		open_root_resource_list = true;
	}

	//show an open project settings button
	if(open_project_settings)
	{
		slots.Add( MakeShareable( new FInfoSlot_OpenProjectSettings() ) );
	}
	if(open_root_resource_list)
	{
		slots.Add( MakeShareable( new FInfoSlot_OpenRootResourceList() ) );
	}

	//restart if needed and nothing else to do
	if(g_bNeedsRestart && slots.Num()==1)
	{
		slots.Add( MakeShareable( new FInfoSlot_RestartNeeded() ) );
	}

	//all good?
	if(slots.Num()==1)
	{
		//replace with standard ready page
		slots = LoadPage( "ApparanceReady.txt" );
	}

	//done
	return slots;
}







#undef LOCTEXT_NAMESPACE
