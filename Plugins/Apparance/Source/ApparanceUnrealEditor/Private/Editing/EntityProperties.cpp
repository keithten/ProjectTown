//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

//main
#include "EntityProperties.h"

#include "ApparanceUnrealVersioning.h"

//unreal
//#include "Core.h"
//unreal:editor
#include "Editor.h"
#include "SnappingUtils.h"
//unreal:ui
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
//#include "Slate.h"
//#include "Widgets/Input/SNumericEntryBox.h"
#include "SNumericEntryBoxFix.h"
//#include "Widgets/Input/SVectorInputBox.h"
#include "SVectorInputBoxFix.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
//unreal:misc
#include "Misc/EngineVersionComparison.h"
#include "HAL/PlatformApplicationMisc.h"
#if UE_VERSION_AT_LEAST( 5, 1, 0 )
#include "EditorStyleSet.h"
#endif

//module
#include "ApparanceUnreal.h"
#include "RemoteEditing.h"
#include "ApparanceUnrealEditor.h"
//#include "ApparanceEntity.h"
#include "Utility/ApparanceConversion.h"
#include "Utility/ApparanceUtility.h"
#include "Editing/EntityEditingMode.h"
//module:ui
#include "UI/SResetToDefaultParameter.h"

#define LOCTEXT_NAMESPACE "ApparanceUnreal"


// MAIN ENTRY POINT: 
//		FEntityPropertyCustomisation::CustomizeDetails


/////////////////////////////////// PARAMETER EDITING SUPPORT ///////////////////////////////////

static float GetVectorComponent( Apparance::Vector3 vector, EApparanceFrameAxis axis )
{
	switch(axis)
	{
		case EApparanceFrameAxis::X: return vector.X;
		case EApparanceFrameAxis::Y: return vector.Y;
		case EApparanceFrameAxis::Z: return vector.Z;
		default: return 0;
	}
}


static EApparanceFrameAxis SwapXY(EApparanceFrameAxis a)
{
	switch (a)
	{
		case EApparanceFrameAxis::X : return EApparanceFrameAxis::Y;
		case EApparanceFrameAxis::Y : return EApparanceFrameAxis::X;
		default:
		case EApparanceFrameAxis::Z : return EApparanceFrameAxis::Z;
	}
}
static float GetFrameComponent( Apparance::Frame& frame, EApparanceFrameComponent component, EApparanceFrameAxis axis, bool is_worldspace )
{
	switch(component)
	{
		case EApparanceFrameComponent::Size:
			return GetVectorComponent( frame.Size, axis );
		case EApparanceFrameComponent::Offset:
			return GetVectorComponent( frame.Origin, axis );
		case EApparanceFrameComponent::Orientation:
		{
			float sign = is_worldspace ? 1.0f : -1.0f;

			//derive orientation Euler rotations from axes and return those
			FVector xaxis = UNREALVECTOR_FROM_APPARANCEVECTOR3(frame.Orientation.X);
			FVector yaxis = UNREALVECTOR_FROM_APPARANCEVECTOR3(frame.Orientation.Y);
			FVector zaxis = UNREALVECTOR_FROM_APPARANCEVECTOR3(frame.Orientation.Z);
			FMatrix m;
			m.SetAxes(&xaxis, &yaxis, &zaxis);
			FRotator r = m.Rotator();
			FVector euler = r.Euler();
			Apparance::Vector3 e = APPARANCEVECTOR3_FROM_UNREALVECTOR(euler);
			return GetVectorComponent(e, axis) * sign;
		}
		default:
			return 0;
	}
}

static void SetVectorComponent( Apparance::Vector3& vector, float value, EApparanceFrameAxis axis )
{
	switch(axis)
	{
		case EApparanceFrameAxis::X: vector.X = value; break;
		case EApparanceFrameAxis::Y: vector.Y = value; break;
		case EApparanceFrameAxis::Z: vector.Z = value; break;
		default: break;
	}
}

static void SetFrameComponent( Apparance::Frame& frame, float value, EApparanceFrameComponent component, EApparanceFrameAxis axis, bool is_worldspace )
{
	switch(component)
	{
		case EApparanceFrameComponent::Size:
			SetVectorComponent( frame.Size, value, axis );
			break;
		case EApparanceFrameComponent::Offset:
			SetVectorComponent( frame.Origin, value, axis );
			break;
		case EApparanceFrameComponent::Orientation:
		{
			float sign = is_worldspace ? 1.0f : -1.0f;

			//derive orientation Euler rotations from axes and return those
			FVector xaxis = UNREALVECTOR_FROM_APPARANCEVECTOR3(frame.Orientation.X);
			FVector yaxis = UNREALVECTOR_FROM_APPARANCEVECTOR3(frame.Orientation.Y);
			FVector zaxis = UNREALVECTOR_FROM_APPARANCEVECTOR3(frame.Orientation.Z);
			FMatrix m;
			m.SetAxes(&xaxis, &yaxis, &zaxis);
			FRotator r = m.Rotator();
			FVector euler = r.Euler();
			Apparance::Vector3 e = APPARANCEVECTOR3_FROM_UNREALVECTOR(euler);
			//apply change
			SetVectorComponent(e, value*sign, axis);
			//work back to basis
			euler = UNREALVECTOR_FROM_APPARANCEVECTOR3(e);
			r = FRotator::MakeFromEuler(euler);

			//snapping
			if (FSnappingUtils::IsRotationSnapEnabled())
			{
				FSnappingUtils::SnapRotatorToGrid(r);
			}

			m = FRotationMatrix(r);
			xaxis = m.GetUnitAxis(EAxis::X);
			yaxis = m.GetUnitAxis(EAxis::Y);
			zaxis = m.GetUnitAxis(EAxis::Z);
			frame.Orientation.X = APPARANCEVECTOR3_FROM_UNREALVECTOR(xaxis);
			frame.Orientation.Y = APPARANCEVECTOR3_FROM_UNREALVECTOR(yaxis);
			frame.Orientation.Z = APPARANCEVECTOR3_FROM_UNREALVECTOR(zaxis);
			break;
		}
		default: break;
	}
}

static int BeginParameterAccess( IProceduralObject* pentity, TArray<const Apparance::IParameterCollection*>& parameter_cascade )
{
	//entity provides list of parameter collections to build parameter set from
	pentity->IPO_GetParameters( parameter_cascade, EParametersRole::ForDetails );
	
	//prep params for read
	int num = 0;
	for (int i = 0; i < parameter_cascade.Num(); i++)
	{
		int n = parameter_cascade[i]->BeginAccess();
		if (n > num)
		{
			num = n;
		}
	}
	
	//how many params?
	return num;
}

static void EndParameterAccess( IProceduralObject* pentity, TArray<const Apparance::IParameterCollection*>& parameter_cascade )
{
	//finished param reads
	for (int i = 0; i < parameter_cascade.Num(); i++)
	{
		parameter_cascade[i]->EndAccess();
	}
}

static Apparance::IParameterCollection* BeginParameterEdit( IProceduralObject* pentity, bool editing_transaction )
{
	return FApparanceUnrealEditorModule::GetModule()->BeginParameterEdit( pentity, editing_transaction );
}

static void EndParameterEdit( IProceduralObject* pentity, bool editing_transaction, Apparance::ValueID input_id )
{
	FApparanceUnrealEditorModule::GetModule()->EndParameterEdit( pentity, editing_transaction, input_id );
}

//slider min/max
const float c_DefaultLimitMinF = TNumericLimits<float>::Lowest();
const float c_DefaultLimitMaxF = TNumericLimits<float>::Max();
const float c_DefaultSliderMinF = 0;
const float c_DefaultSliderMaxF = 100;
const float c_DefaultLimitMinI = TNumericLimits<int>::Lowest();
const float c_DefaultLimitMaxI = TNumericLimits<int>::Max();
const float c_DefaultSliderMinI = 0;
const float c_DefaultSliderMaxI = 100;


/////////////////////////////////// ENTITY PROPERTY CUSTOMISATION ///////////////////////////////////


FEntityPropertyCustomisation::FEntityPropertyCustomisation()
	: bColourPickingInteractive( false )
{
	//font to use (default textblock doesn't match property editor panel)
	DefaultFont = APPARANCE_STYLE.GetFontStyle( "PropertyWindow.NormalFont" );
}


// live procedure name
FText FEntityPropertyCustomisation::GetLogoOverlayText() const
{
	FText product = FApparanceUnrealModule::GetModule()->GetProductText();

	//hide Commercial text, full version doesn't need a splash
	FString product_check = product.ToString();
	if(product_check.Equals( TEXT( "COMMERCIAL" ) ))
	{
		product = FText::GetEmpty();
	}

	return product;
}

// live procedure name
FText FEntityPropertyCustomisation::GetProcedureName() const
{
	//find list
	const TArray<const Apparance::ProcedureSpec*>& procedures = FApparanceUnrealModule::GetModule()->GetInventory();

	//current value
	int current_proc_id = 0;
	ProcedureIDProperty->GetValue( current_proc_id );

	FString current_proc_name;
	if(current_proc_id == 0)
	{
		current_proc_name = "None";
	}
	else
	{
		//search
		current_proc_name = "Unknown";
		for(int i = 0; i < procedures.Num(); i++)
		{
			if(procedures[i]->ID == current_proc_id)
			{
				current_proc_name = APPARANCE_UTF8_TO_TCHAR( (UTF8CHAR*)procedures[i]->Category );
				current_proc_name.Append( TEXT(".") );
				current_proc_name.Append( APPARANCE_UTF8_TO_TCHAR( (UTF8CHAR*)procedures[i]->Name ) );
				break;
			}
		}
	}

	return FText::FromString( current_proc_name );
}

// live procedure info/description
FText FEntityPropertyCustomisation::GetProcedureInformation() const
{
	int current_proc_id = 0;
	ProcedureIDProperty->GetValue( current_proc_id );
	const Apparance::ProcedureSpec* proc_spec = FApparanceUnrealModule::GetLibrary()->FindProcedureSpec( current_proc_id );
	if(!proc_spec)
	{
		return FText();
	}
	
	//desc?
	FString info = APPARANCE_UTF8_TO_TCHAR( (UTF8CHAR*)proc_spec->Description );
	if(!info.IsEmpty())
	{
		return FText::FromString( info );
	}

	//use param count
	int num_parameters = proc_spec->Inputs->BeginAccess();
	proc_spec->Inputs->EndAccess();
	return FText::Format( (num_parameters == 1) ? LOCTEXT( "ProcedureParameterCountSingular", "{0} parameter" ) : LOCTEXT( "ProcedureParameterCountPlural", "{0} parameters" ), num_parameters );
}

// debug structure export requested
//
FReply  FEntityPropertyCustomisation::Editor_ExportEntityStructure()
{
	TArray<UObject*> objects;
	ProcedureIDProperty->GetOuterObjects( objects );
	if(objects.Num()==1)
	{
		AApparanceEntity* pentity = Cast<AApparanceEntity>( objects[0] );
		if(pentity)
		{
			FString s = pentity->Debug_GetStructure();
			FPlatformApplicationMisc::ClipboardCopy( *s );
		}
	}

	return FReply::Handled();
}

// open this procedure in the apparance editor
//
FReply  FEntityPropertyCustomisation::Editor_OpenProcedure()
{
	TArray<UObject*> objects;
	ProcedureIDProperty->GetOuterObjects( objects );
	if(objects.Num()==1)
	{
		AApparanceEntity* pentity = Cast<AApparanceEntity>( objects[0] );
		if(pentity)
		{
			//request open in external editor
			FApparanceUnrealEditorModule::GetModule()->OpenProcedure( pentity->ProcedureID );
		}
	}
	
	return FReply::Handled();
}


// is smart editing enabled?
static ECheckBoxState OnIsSmartEditingEnabled()
{
	bool enable = FApparanceUnrealModule::GetModule()->IsEditingEnabled();
	return enable?ECheckBoxState::Checked:ECheckBoxState::Unchecked;
}

static void OnSmartEditingChanged(ECheckBoxState NewCheckboxState)
{
	bool enable = NewCheckboxState == ECheckBoxState::Checked;
	if (enable)
	{
		FEntityEditingMode::RequestShow(true);
	}
	else
	{
		FEntityEditingMode::RequestHide();
	}
}

// build UI
//
void FEntityPropertyCustomisation::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	//our Apparance category
	IDetailCategoryBuilder& proc_category = DetailBuilder.EditCategory( TEXT("Apparance"), LOCTEXT("ApparancePropertySection","Apparance") );	

	//is it an actual entity?
	AApparanceEntity* pentityactor = nullptr;
	TArray<TWeakObjectPtr<UObject>> edited_objects;
	DetailBuilder.GetObjectsBeingCustomized( edited_objects );	
	for (int32 i = 0; i < edited_objects.Num(); i++)
	{
		pentityactor = Cast<AApparanceEntity>( edited_objects[i].Get() );
		if (pentityactor)
		{
			break;
		}
	}

	//what are we showing?
	bool show_procedure_select = ShowProcedureSelection();
	bool show_entity_mode_select = false; //show_procedure_select;//aliased for now
	bool show_smart_editing_button = pentityactor!=nullptr && pentityactor->IsSmartEditingSupported();//aliased for now
	bool show_proc_parameters = ShowProcedureParameters();
	bool show_pin_enable_ui = ShowPinEnableUI();
	bool show_diagnostics = ShowDiagnostics();

	//where does nice separator line go?
	bool separator_after_mode_buttons = show_smart_editing_button || show_entity_mode_select;
	bool separator_after_proc_select = show_procedure_select && !separator_after_mode_buttons;

	//ProcedureID property
	if(show_procedure_select)
	{
		//cache because needed to yield live values on panel display
		ProcedureIDProperty = DetailBuilder.GetProperty( GetProcedureIDPropertyName(), GetProcedureIDPropertyClass() );

		proc_category.AddCustomRow( FText::FromString( "Procedure" ) )
			.WholeRowContent()
			[
				SNew(SVerticalBox)
				//logo + proc select
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0,2,0,separator_after_proc_select?2:0))
				[
					SNew( SHorizontalBox)
					//logo
					 + SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew( SOverlay )
						//logo
						+ SOverlay::Slot()
						[
							SNew( SBox )
							.WidthOverride( 172.0f )
							.HeightOverride( 50.0f )
							[
								SNew( SImage ).Image( FApparanceUnrealEditorModule::GetStyle()->GetBrush( TEXT( "Apparance.Logo.Small" ) ) )
							]
						]
						//splash
						+ SOverlay::Slot()
						.VAlign( EVerticalAlignment::VAlign_Bottom )
						.HAlign( EHorizontalAlignment::HAlign_Right )
						[
							SNew( STextBlock )
							.Text_Raw( this, &FEntityPropertyCustomisation::GetLogoOverlayText )
							.ColorAndOpacity( FLinearColor::Red )
							.TextStyle( APPARANCE_STYLE, "NormalText.Important" )
						]
					]
					//proc selection
					+ SHorizontalBox::Slot()
					.FillWidth(1.0)
					.Padding(FMargin(5.0f,0,0,0))
					[
						SNew( SVerticalBox )
						//label
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew( STextBlock )
							.Font( DefaultFont )
							.Text( LOCTEXT("ProcedureSelectLabel", "Procedure"))
						]
						//proc select/open
						+ SVerticalBox::Slot()
						.AutoHeight()
//						.Padding(FMargin(0,2,0,0))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							[
								//proc drop-select
								SNew( SComboButton )
								.ContentPadding( 2.0f )
								.ButtonContent()
								[
									SNew( STextBlock )
										.Font(DefaultFont)
										.Text_Raw( this, &FEntityPropertyCustomisation::GetProcedureName )
								]
								.MenuContent()
								[
									BuildProceduresMenu()
								]
							]
							+SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(6,0,0,0)
							.VAlign(VAlign_Center)
							[
								//open button
								SNew( SButton )
								.ContentPadding( FMargin( 10.0f, 2.0f ) )
								.Text( LOCTEXT("ApparanceProcedureOpenButton","Open") )
								.ToolTipText( LOCTEXT("ApparanceProcedureOpenButtonTooltip", "Open the current procedure in the Apparance Editor") )
								.OnClicked( this, &FEntityPropertyCustomisation::Editor_OpenProcedure )
							]
						]
						//info
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew( STextBlock )
							.Font( DefaultFont )
							.Text_Raw( this, &FEntityPropertyCustomisation::GetProcedureInformation )
						]
					]
				]
				//separator
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew( SSeparator )
					.Visibility( separator_after_proc_select?EVisibility::Visible:EVisibility::Collapsed )
				]
			];
	}

	//entity editing controls and mode selection
	if (show_smart_editing_button || show_entity_mode_select)
	{
		proc_category.AddCustomRow( FText::FromString( "Smart Editing" ) )
			.WholeRowContent()
			[
				SNew(SVerticalBox)
				//logo + proc select
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0,2))
				[
					SNew(SHorizontalBox)
					
					//smart edit button
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SCheckBox)
						.Style( APPARANCE_STYLE, "Property.ToggleButton")
						.IsChecked_Static( &OnIsSmartEditingEnabled )
						.OnCheckStateChanged_Static( &OnSmartEditingChanged )
						.ToolTipText( LOCTEXT("EntitySmartEditingButtonTooltip","Toggle the Apparance editing tools panel to enable smart editing features") )
						.Visibility( show_smart_editing_button?EVisibility::Visible:EVisibility::Hidden )
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(3, 2)							
							[
								SNew(SImage)
								.Image( APPARANCE_STYLE.GetBrush("LevelEditor.Tabs.Modes") )
							]
							+SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Center)
							.Padding(6, 2)
							[
								SNew(STextBlock)
								.Text( LOCTEXT("EntitySmartEditingButton", "Smart Editing" ) )
								.Font(DefaultFont)
								//.ColorAndOpacity_Static(&OnGetSourceTypeTextColour, psource, EApparanceValueSource::Constant )
							]			
						]
					]

					//filler
					+SHorizontalBox::Slot()
					.FillWidth(1.0f)

					//mode buttons
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SCheckBox)
						.Style( APPARANCE_STYLE, "Property.ToggleButton.Start")
						.IsChecked(ECheckBoxState::Checked)
						//.IsChecked_Static( &OnIsSourceTypeChecked, psource, EApparanceValueSource::Constant )
						//.OnCheckStateChanged_Static( &OnSourceTypeChange, psource, EApparanceValueSource::Constant )
						.ToolTipText( LOCTEXT("EntityDynamicModeButtonTooltip","Fully dynamic entity, parameters can be changed at run-time.") )
						.Visibility( show_entity_mode_select?EVisibility::Visible:EVisibility::Hidden )
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(3, 2)							
							[
								SNew(SImage)
								.Image( APPARANCE_STYLE.GetBrush("Mobility.Stationary") )
							]
							+SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Center)
							.Padding(6, 2)
							[
								SNew(STextBlock)
								.Text( LOCTEXT("EntityDynamicModeButton", "Dynamic" ) )
								.Font(DefaultFont)
								//.ColorAndOpacity_Static(&OnGetSourceTypeTextColour, psource, EApparanceValueSource::Constant )
							]			
						]
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SCheckBox)
						.Style( APPARANCE_STYLE, "Property.ToggleButton.Middle")
						.IsChecked(ECheckBoxState::Unchecked)
						//.IsChecked_Static( &OnIsSourceTypeChecked, psource, EApparanceValueSource::Constant )
						//.OnCheckStateChanged_Static( &OnSourceTypeChange, psource, EApparanceValueSource::Constant )
						.ToolTipText( LOCTEXT("EntityCompactModeButtonTooltip","Sub-entities are compacted into paramter lists and build by shared generation.") )
						.Visibility( show_entity_mode_select?EVisibility::Visible:EVisibility::Hidden )
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(3, 2)							
							[
								SNew(SImage)
								.Image( APPARANCE_STYLE.GetBrush("GraphEditor.MakeArray_16x") )
							]
							+SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Center)
							.Padding(6, 2)
							[
								SNew(STextBlock)
								.Text( LOCTEXT("EntityCompactModeButton", "Compact" ) )
								.Font(DefaultFont)
								//.ColorAndOpacity_Static(&OnGetSourceTypeTextColour, psource, EApparanceValueSource::Constant )
							]			
						]
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SCheckBox)
						.Style( APPARANCE_STYLE, "Property.ToggleButton.Middle")
						.IsChecked(ECheckBoxState::Unchecked)
						//.IsChecked_Static( &OnIsSourceTypeChecked, psource, EApparanceValueSource::Constant )
						//.OnCheckStateChanged_Static( &OnSourceTypeChange, psource, EApparanceValueSource::Constant )
						.ToolTipText( LOCTEXT("EntityStaticModeButtonTooltip","Generation is only performed once, on load. No further changes allowed (optimisation).") )
						.Visibility( show_entity_mode_select?EVisibility::Visible:EVisibility::Hidden )
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(3, 2)							
							[
								SNew(SImage)
								.Image( APPARANCE_STYLE.GetBrush("Mobility.Static") )
							]
							+SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Center)
							.Padding(6, 2)
							[
								SNew(STextBlock)
								.Text( LOCTEXT("EntityStaticModeButton", "Static" ) )
								.Font(DefaultFont)
								//.ColorAndOpacity_Static(&OnGetSourceTypeTextColour, psource, EApparanceValueSource::Constant )
							]			
						]
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SCheckBox)
						.Style( APPARANCE_STYLE, "Property.ToggleButton.End")
						.IsChecked(ECheckBoxState::Unchecked)
						//.IsChecked_Static( &OnIsSourceTypeChecked, psource, EApparanceValueSource::Constant )
						//.OnCheckStateChanged_Static( &OnSourceTypeChange, psource, EApparanceValueSource::Constant )
						.ToolTipText( LOCTEXT("EntityBakedModeButtonTooltip","Content is captured, stored with actor, and used for rendering. No more generation performed.") )
						.Visibility( show_entity_mode_select?EVisibility::Visible:EVisibility::Hidden )
						[
							SNew(SHorizontalBox)
							+SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(3, 2)							
							[
								SNew(SImage)
								.Image( APPARANCE_STYLE.GetBrush("Level.LockedIcon16x") )
							]
							+SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Center)
							.Padding(6, 2)
							[
								SNew(STextBlock)
								.Text( LOCTEXT("EntityBakedModeButton", "Baked" ) )
								.Font(DefaultFont)
								//.ColorAndOpacity_Static(&OnGetSourceTypeTextColour, psource, EApparanceValueSource::Constant )
							]			
						]
					]
		
			
				]
				//separator
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew( SSeparator )
					.Visibility( separator_after_mode_buttons?EVisibility::Visible:EVisibility::Collapsed )
				]
			];
	}

	//procedure parameters based on proc spec
	if(show_proc_parameters)
	{	
		//what is being edited
		IProceduralObject* pentity = nullptr;
		TArray<TWeakObjectPtr<UObject>> EditedObjects;
		DetailBuilder.GetObjectsBeingCustomized( EditedObjects );	
		for (int32 i = 0; i < EditedObjects.Num(); i++)
		{
			pentity = Cast<IProceduralObject>( EditedObjects[i].Get() );
			if (pentity)
			{
				break;
			}
		}
		if(!pentity)
		{
			return;
		}

		//eumerate params
		int current_proc_id = 0;
		ProcedureIDProperty->GetValue( current_proc_id );
		const Apparance::ProcedureSpec* proc_spec = FApparanceUnrealModule::GetLibrary()->FindProcedureSpec( current_proc_id );
		if(proc_spec)
		{
			//hold parameter hierarchy while generating for easy metadata access
			TArray<const Apparance::IParameterCollection*> prop_hierarchy;
			int num_parameters = BeginParameterAccess( pentity, prop_hierarchy ); //assumes proc_spec is in cascade somewhere
			for(int i=0 ; i<num_parameters ; i++)
			{
				Apparance::ValueID input_id = proc_spec->Inputs->GetID( i );

				//skip old entries
				Apparance::Parameter::Type input_type = proc_spec->Inputs->GetType( i );
				if(input_type == Apparance::Parameter::None)
				{
					continue;
				}
	
				//build UI for this proc input
				TSharedPtr<STextBlock> textblock;
				proc_category.AddCustomRow(FText::FromString("Procedure"))
					.NameContent()
					[
						SAssignNew( textblock, STextBlock )
							.Font(DefaultFont)
							.ToolTipText( this, &FEntityPropertyCustomisation::GetParameterTooltip, pentity, input_id, proc_spec->Inputs )
							.Text( this, &FEntityPropertyCustomisation::GetParameterLabel, pentity, input_id )
					]
					.ValueContent()
					.MaxDesiredWidth(375.0f) //like the Unreal transform edit controls
					.MinDesiredWidth(375.0f)
					.VAlign(VAlign_Center)
					[
						GenerateParameterEditControl( pentity, i, proc_spec->Inputs, &prop_hierarchy )
					];

				//attach context menu functionality
#if 0 //not using context menu yet
				textblock->SetOnMouseButtonUp(FPointerEventHandler::CreateSP(this, &FEntityPropertyCustomisation::HandleMouseButtonUp, pentity, i, proc_spec->Inputs ));
#endif
			}
			EndParameterAccess( pentity, prop_hierarchy );
		}
	}

	//hide normal proc id field (property needs to be exposed for property handle access used to get/set values)
	if(show_procedure_select)
	{
		IDetailPropertyRow& proc_row = proc_category.AddProperty( GetProcedureIDPropertyName(), GetProcedureIDPropertyClass() );
		proc_row.Visibility( EVisibility::Hidden );
	}

	//per-pin enable/disable interface
	if(show_pin_enable_ui)
	{
		//eumerate params
		int current_proc_id = 0;
		ProcedureIDProperty->GetValue( current_proc_id );
		const Apparance::ProcedureSpec* proc_spec = FApparanceUnrealModule::GetLibrary()->FindProcedureSpec( current_proc_id );
		if(proc_spec)
		{
			int num_parameters = proc_spec->Inputs->BeginAccess();		
			for(int i=0 ; i<num_parameters ; i++)
			{
				FString full_input_name = FString( APPARANCE_UTF8_TO_TCHAR( (UTF8CHAR*)proc_spec->Inputs->GetName( i ) ) );
				FString input_name = FApparanceUtility::GetMetadataSection( full_input_name, 0 );
				FString input_desc = FApparanceUtility::GetMetadataSection( full_input_name, 1 );

				Apparance::ValueID input_id = proc_spec->Inputs->GetID( i );
				
				//build UI for this proc input
				proc_category.AddCustomRow(FText::FromString("Procedure"))
					.NameContent()
					[
					SNew( STextBlock )
					.Font(DefaultFont)
					.Text( FText::FromString( input_name ) )
					.ToolTipText( FText::FromString( input_desc ) )
					]
					.ValueContent()
					.MaxDesiredWidth(375.0f) //like the Unreal transform edit controls
					.MinDesiredWidth(375.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SCheckBox)
						.IsChecked( this, &FEntityPropertyCustomisation::CurrentPinEnabled, input_id )
						.OnCheckStateChanged( this, &FEntityPropertyCustomisation::ChangePinEnable, input_id )
					];
			}
			proc_spec->Inputs->EndAccess();
		}
	}

	//diags
	if(show_diagnostics)
	{
		//add a new row for export of structure
		proc_category.AddCustomRow(FText::FromString("Debug"), true/*advanced*/)
			.NameContent()
			[
				SNew( STextBlock )
					.Font(DefaultFont)
					.Text(LOCTEXT("ApparanceDebugLabel","Debug") )
			]
			.ValueContent()
			.MaxDesiredWidth(0)
			.VAlign(VAlign_Center)
			[
				SNew( SButton )
				.ContentPadding( FMargin(4.0f,0) )
				.Text( LOCTEXT("ApparanceProcedureExportStructure","Copy Structure To Clipboard") )
				.ToolTipText( LOCTEXT( "ApparanceProcedureExportStructureTooltip", "Information on the actor components and internal entity data structures" ) )
				.OnClicked( this, &FEntityPropertyCustomisation::Editor_ExportEntityStructure )
			];
	}
}

// open context menu on property
//
FReply FEntityPropertyCustomisation::HandleMouseButtonUp(
	const FGeometry& InGeometry, 
	const FPointerEvent& InPointerEvent, 
	IProceduralObject* pentity,
	int parameter_index,
	const Apparance::IParameterCollection* spec_params )
{
	if( InPointerEvent.GetEffectingButton() == EKeys::RightMouseButton )
	{
		//parameter access
		TArray<const Apparance::IParameterCollection*> prop_hierarchy;
		int num_parameters = BeginParameterAccess( pentity, prop_hierarchy ); //assumes proc_spec is in cascade somewhere

		//entity info
		Apparance::Parameter::Type type = spec_params->GetType(parameter_index);		
		const TCHAR* pname = ApparanceParameterTypeName(type);

		//build menu for it
		FMenuBuilder MenuBuilder( true, nullptr, nullptr, true );	
		//FUIAction ExpandAllAction( FExecuteAction::CreateSP( this, &SDetailTableRowBase::OnExpandAllClicked ) );
		//FUIAction CollapseAllAction( FExecuteAction::CreateSP( this, &SDetailTableRowBase::OnCollapseAllClicked ) );

		//parameter information		
		MenuBuilder.BeginSection( NAME_None, FText::FromString(TEXT("Parameter")) );
		MenuBuilder.AddWidget( GenerateTextWidget( FString( pname ) ), LOCTEXT("ParameterContextType","Type") );
		MenuBuilder.EndSection();

		//metadata display/editing
		MenuBuilder.BeginSection( NAME_None, FText::FromString(TEXT("Metadata")) );
		AddFlagMetadataMenuItems(MenuBuilder, pentity, parameter_index, spec_params, &prop_hierarchy);
		if (ApparanceParameterTypeIsNumeric(type))
		{
			AddNumericMetadataMenuItems(MenuBuilder, pentity, parameter_index, spec_params, &prop_hierarchy);
		}	
		if (ApparanceParameterTypeIsSpatial(type))
		{
			AddSpatialMetadataMenuItems(MenuBuilder, pentity, parameter_index, spec_params, &prop_hierarchy);
		}
		MenuBuilder.EndSection();

		//invoke menu		
		FWidgetPath WidgetPath = InPointerEvent.GetEventPath() != nullptr ? *InPointerEvent.GetEventPath() : FWidgetPath();		
		FSlateApplication::Get().PushMenu(WidgetPath.GetLastWidget(), WidgetPath, MenuBuilder.MakeWidget(), InPointerEvent.GetScreenSpacePosition(), FPopupTransitionEffect::ContextMenu);

		EndParameterAccess( pentity, prop_hierarchy );
	}
	
	return FReply::Handled();
}

void FEntityPropertyCustomisation::AddFlagMetadataMenuItems(FMenuBuilder& MenuBuilder, IProceduralObject* pentity,	int parameter_index, const Apparance::IParameterCollection* spec_params,	TArray<const Apparance::IParameterCollection*>* prop_hierarchy)
{
}
void FEntityPropertyCustomisation::AddNumericMetadataMenuItems(FMenuBuilder& MenuBuilder, IProceduralObject* pentity,	int parameter_index, const Apparance::IParameterCollection* spec_params,	TArray<const Apparance::IParameterCollection*>* prop_hierarchy)
{
	switch (spec_params->GetType(parameter_index))
	{
		case Apparance::Parameter::Integer:
		{
			//min
			bool found;
			int min_value = FApparanceUtility::FindMetadataInteger(prop_hierarchy, parameter_index, "Min", 0, &found);
			if (found)
			{
				TSharedRef<SWidget> min_text = GenerateTextWidget(FString::Format(TEXT("{0}"), { min_value }));
				MenuBuilder.AddWidget(min_text, LOCTEXT("MetadataLabelMinValue", "Minimum"));
			}
			//max
			int max_value = FApparanceUtility::FindMetadataInteger(prop_hierarchy, parameter_index, "Max", 0, &found);
			if (found)
			{
				TSharedRef<SWidget> max_text = GenerateTextWidget(FString::Format(TEXT("{0}"), { max_value }));
				MenuBuilder.AddWidget(max_text, LOCTEXT("MetadataLabelMaxValue", "Maximum"));
			}
			break;
		}
		case Apparance::Parameter::Float:
		{
			//min
			bool found;
			float min_value = FApparanceUtility::FindMetadataFloat(prop_hierarchy, parameter_index, "Min", 0, &found);
			if (found)
			{
				TSharedRef<SWidget> min_text = GenerateTextWidget(FString::Format(TEXT("{0}"), { min_value }));
				MenuBuilder.AddWidget(min_text, LOCTEXT("MetadataLabelMinValue", "Minimum"));
			}
			//max
			float max_value = FApparanceUtility::FindMetadataFloat(prop_hierarchy, parameter_index, "Max", 0, &found);
			if (found)
			{
				TSharedRef<SWidget> max_text = GenerateTextWidget(FString::Format(TEXT("{0}"), { max_value }));
				MenuBuilder.AddWidget(max_text, LOCTEXT("MetadataLabelMaxValue", "Maximum"));
			}
			break;
		}
		case Apparance::Parameter::Vector3:
			break;
	}
}
void FEntityPropertyCustomisation::AddSpatialMetadataMenuItems(FMenuBuilder& MenuBuilder, IProceduralObject* pentity,	int parameter_index, const Apparance::IParameterCollection* spec_params,	TArray<const Apparance::IParameterCollection*>* prop_hierarchy)
{
#if APPARANCE_ENABLE_SPATIAL_MAPPING
	bool worldspace = FApparanceUtility::FindMetadataBool(prop_hierarchy, parameter_index, "Worldspace");
	MenuBuilder.AddWidget( 
		SNew( STextBlock )
		.Text( worldspace?LOCTEXT("WorldspaceTrue","Yes"):LOCTEXT("WorldspaceFalse","No") ),
		LOCTEXT("MetadataLabelWorldspace", "World-space"));			
#endif
}




// plain text widget for information context menu entries
//
TSharedRef<SWidget> FEntityPropertyCustomisation::GenerateTextWidget( FString text ) const
{
	return
		SNew( SBox )
		.HAlign( HAlign_Right )
		[
			SNew( SBox )
			.Padding( FMargin(4.0f, 0.0f, 0.0f, 0.0f) )
			.WidthOverride( 100.0f )
			[
				SNew(STextBlock)
				.Font( APPARANCE_STYLE.GetFontStyle( TEXT( "MenuItem.Font" ) ) )
				.Text( FText::FromString( text ) )
			]
		];
}


// build appropriate control for editing a specific procedure parameter
//
TSharedRef<SWidget> FEntityPropertyCustomisation::GenerateParameterEditControl( 
	IProceduralObject* pentity,
	int parameter_index,
	const Apparance::IParameterCollection* spec_params,
	TArray<const Apparance::IParameterCollection*>* prop_hierarchy )
{
	//find input details
	Apparance::Parameter::Type input_type = spec_params->GetType( parameter_index );
	Apparance::ValueID input_id = spec_params->GetID( parameter_index );
	const char* input_name = spec_params->GetName( parameter_index );

	//handle
	FText parameter_value_as_text;
	TSharedPtr<SWidget> parameter_control;
	switch (input_type)
	{
		case Apparance::Parameter::Integer:
		{
			parameter_control = GenerateIntegerEditControl( pentity, input_id, prop_hierarchy, spec_params );
			break;
		}
		case Apparance::Parameter::Frame:
		{
			parameter_control = GenerateFrameEditControl( pentity, input_id, prop_hierarchy );
			break;
		}
		case Apparance::Parameter::Float:
		{
			parameter_control = GenerateFloatEditControl( pentity, input_id, prop_hierarchy );
			break;
		}
		case Apparance::Parameter::Bool:
		{
			parameter_control = GenerateBoolEditControl( pentity, input_id, prop_hierarchy );
			break;
		}
		case Apparance::Parameter::Colour:
		{
			parameter_control = GenerateColourEditControl( pentity, input_id, prop_hierarchy );
			break;
		}
		case Apparance::Parameter::String:
		{
			parameter_control = GenerateStringEditControl( pentity, input_id, prop_hierarchy );
			break;
		}
		/*case Apparance::Parameter::Vector2:
		{
		Apparance::Vector2 v = { 0,0 };
		proc->SetValue( input_id, &v );
		break;
		}*/
		case Apparance::Parameter::Vector3:
		{
			parameter_control = GenerateVectorEditControl( pentity, input_id, prop_hierarchy );
			break;
		}
		/*case Apparance::Parameter::Vector4:
		{
		Apparance::Vector4 v = { 0,0,0,0 };
		proc->SetValue( input_id, &v );
		break;
		}*/
		case Apparance::Parameter::List:
		{
			parameter_control = SNew(STextBlock)
				.Font(DefaultFont)
				.Text(this, &FEntityPropertyCustomisation::CurrentListParameterDesc, pentity, input_id);
			break;
		}
		default:
		{
			UE_LOG(LogApparance,Error,TEXT("Unknown parameter type %i (id %i, name %i)"), input_type, input_id, input_name ); 
			return SNullWidget::NullWidget;
		}
	}

	//UI?
	if(parameter_control)
	{
		return parameter_control.ToSharedRef();
	}
	else
	{
		//default widget, just show value as text
		return SNew( STextBlock )
			.Font(DefaultFont)
			.Text( parameter_value_as_text );
	}
}


FText FEntityPropertyCustomisation::CurrentListParameterDesc(IProceduralObject* pentity, Apparance::ValueID input_id) const
{
	TArray<const Apparance::IParameterCollection*> parameter_cascade;
	BeginParameterAccess(pentity, parameter_cascade);

	//attempt to obtain current value
	FText value;
	int text_len = 0;
	for (int i = 0; i < parameter_cascade.Num(); i++)
	{
		const Apparance::IParameterCollection* params = parameter_cascade[i];
		const Apparance::IParameterCollection* plist = params->FindList(input_id);
		if (plist)
		{
			int count = plist->BeginAccess();
			value = FText::FromString(FString::Format(TEXT("{0} Items"), { count }));
			plist->EndAccess();

			break; //found
		}
	}

	EndParameterAccess(pentity, parameter_cascade);
	return value;
}


// parameters overridden by script aren't editable in the details panel
//
bool FEntityPropertyCustomisation::CheckParameterEditable(IProceduralObject* pentity, Apparance::ValueID input_id) const
{
	return !pentity->IPO_IsParameterOverridden(input_id);
}

// UI label request
//
FText FEntityPropertyCustomisation::GetParameterLabel( IProceduralObject* pentity, Apparance::ValueID input_id ) const
{
	return pentity->IPO_GenerateParameterLabel( input_id );
}

// UI tooltip request
//
FText FEntityPropertyCustomisation::GetParameterTooltip(IProceduralObject* pentity, Apparance::ValueID input_id, const Apparance::IParameterCollection* spec_params) const
{
	FString main = pentity->IPO_GenerateParameterTooltip(input_id).ToString();

	//extra information from metadata
	TArray<const Apparance::IParameterCollection*> prop_hierarchy;
	int num_parameters = BeginParameterAccess( pentity, prop_hierarchy ); //assumes proc_spec is in cascade somewhere

	FText units;
#if APPARANCE_ENABLE_SPATIAL_MAPPING
	//units check (via worldspace flag)
	bool found_ws;
	bool ws_value = FApparanceUtility::FindMetadataBool(&prop_hierarchy, input_id, "Worldspace", false, false, &found_ws);
	if (found_ws && ws_value)
	{
		units = LOCTEXT("WorldSpaceUnitHintSuffix", " (cm)");
	}
#endif
	
	//numerical metadata
	Apparance::Parameter::Type type = spec_params->FindType( input_id );
	switch (type)
	{
		case Apparance::Parameter::Integer:
		{
			//range
			bool found_min;
			int min_value = FApparanceUtility::FindMetadataInteger(&prop_hierarchy, input_id, "Min", 0, &found_min );
			bool found_max;
			int max_value = FApparanceUtility::FindMetadataInteger(&prop_hierarchy, input_id, "Max", 100, &found_max);
			if (found_min || found_max)
			{
				main.Append(TEXT("\n"));
				main.Append(FText::Format(LOCTEXT("ParameterMetadataLabel_Range", "Range: {0} to {1}{2}"), { min_value, max_value, units }).ToString());
			}
			//clamp
			bool found_minlim;
			int minlim_value = FApparanceUtility::FindMetadataInteger(&prop_hierarchy, input_id, "MinLimit", 0, &found_minlim );
			bool found_maxlim;
			int maxlim_value = FApparanceUtility::FindMetadataInteger(&prop_hierarchy, input_id, "MaxLimit", 100, &found_maxlim);
			if (found_minlim || found_maxlim)
			{
				main.Append(TEXT("\n"));
				main.Append(FText::Format(LOCTEXT("ParameterMetadataLabel_Clamp", "Clamped: {0} to {1}{2}"), { minlim_value, maxlim_value, units }).ToString());
			}
			break;
		}
		case Apparance::Parameter::Float:
		{
			//range
			bool found_min;
			float min_value = FApparanceUtility::FindMetadataFloat(&prop_hierarchy, input_id, "Min", 0, &found_min );
			bool found_max;
			float max_value = FApparanceUtility::FindMetadataFloat(&prop_hierarchy, input_id, "Max", 100, &found_max);
			if (found_min || found_max)
			{
				main.Append(TEXT("\n"));
				main.Append( FText::Format(LOCTEXT("ParameterMetadataFormat_Range", "Range: {0} to {1}{2}"),{ min_value, max_value, units }).ToString());
			}
			//clamp
			bool found_minlim;
			float minlim_value = FApparanceUtility::FindMetadataFloat(&prop_hierarchy, input_id, "MinLimit", 0, &found_minlim );
			bool found_maxlim;
			float maxlim_value = FApparanceUtility::FindMetadataFloat(&prop_hierarchy, input_id, "MaxLimit", 100, &found_maxlim);
			if (found_minlim || found_maxlim)
			{
				main.Append(TEXT("\n"));
				main.Append(FText::Format(LOCTEXT("ParameterMetadataLabel_Clamp", "Clamped: {0} to {1}{2}"), { minlim_value, maxlim_value, units }).ToString());
			}
			break;
		}
	}

#if APPARANCE_ENABLE_SPATIAL_MAPPING
	//spatial metadata
	if (found_ws)
	{
		main.Append(TEXT("\n"));
		main.Append(FText::Format(LOCTEXT("ParameterMetadataFormat_Worldspace", "Worldspace: {0}"), { ws_value?LOCTEXT("ParameterMetadataTrue", "Yes"):LOCTEXT("ParameterMetadataFalse", "No") } ).ToString() );
	}
#endif

	//origin metadata
	if(type == Apparance::Parameter::Frame)
	{
		int origin_value = FApparanceUtility::FindMetadataInteger( &prop_hierarchy, input_id, "Origin", (int)EApparanceFrameOrigin::Face/*default*/ );
		if(origin_value >= 0 && origin_value <= 2)
		{
			FText origin_text = FText();
			switch((EApparanceFrameOrigin)origin_value)
			{
				//FrameOrigin type (editor internal)
				case EApparanceFrameOrigin::Face:   origin_text = LOCTEXT( "ParameterMetadataFrameOrigin_Face", "Face" ); break;
				case EApparanceFrameOrigin::Corner: origin_text = LOCTEXT( "ParameterMetadataFrameOrigin_Corner", "Corner" ); break;
				case EApparanceFrameOrigin::Centre: origin_text = LOCTEXT( "ParameterMetadataFrameOrigin_Centre", "Centre" ); break;
			}
			main.Append( TEXT( "\n" ) );
			main.Append( FText::Format( LOCTEXT( "ParameterMetadataFormat_FrameOrigin", "Origin: {0}" ), { origin_text } ).ToString() );
		}
	}

	EndParameterAccess( pentity, prop_hierarchy );

	return FText::FromString( main );
}

/////////////////////////////////// INTEGER ///////////////////////////////////

// integer parameter getter
//
int32 FEntityPropertyCustomisation::GetIntegerParameter( IProceduralObject* pentity, Apparance::ValueID input_id ) const
{
	TArray<const Apparance::IParameterCollection*> parameter_cascade;
	BeginParameterAccess( pentity, parameter_cascade );
	
	//attempt to obtain current value
	int value = 0;
	for (int i = 0; i < parameter_cascade.Num(); i++)
	{
		const Apparance::IParameterCollection* params = parameter_cascade[i];
		if (params->FindInteger(input_id, &value))
		{
			break; //found
		}
	}
	
	EndParameterAccess( pentity, parameter_cascade );
	return value;	
}
// integer parameter setter (either full editing modification, or just pre-edit preview)
//
void FEntityPropertyCustomisation::SetIntegerParameter( int32 new_value, IProceduralObject* pentity, Apparance::ValueID input_id, bool editing_transaction )
{
	Apparance::IParameterCollection* ent_params = BeginParameterEdit( pentity, editing_transaction );	
	
	//set parameter (in-case outstanding change)
	if(!ent_params->ModifyInteger( input_id, new_value ))
	{
		//if not present, needs adding
		int index = ent_params->AddParameter( Apparance::Parameter::Integer, input_id );
		ent_params->SetInteger( index, new_value );
	}

	EndParameterEdit( pentity, editing_transaction, input_id );
}

// integer parameter editing
//
TSharedPtr<SWidget> FEntityPropertyCustomisation::GenerateIntegerEditControl( IProceduralObject* pentity, Apparance::ValueID input_id, const TArray<const Apparance::IParameterCollection*>* prop_hierarchy, const Apparance::IParameterCollection* spec_params )
{
	return SNew( SHorizontalBox )
		+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.FillWidth(1.0f)
			.MaxWidth(105.0f)
			[	
				SNew( SNumericEntryBoxFix<int32> )
				.AllowSpin( true )
				.ToolTipText( this, &FEntityPropertyCustomisation::GetParameterTooltip, pentity, input_id, spec_params )
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Value( this, &FEntityPropertyCustomisation::CurrentIntegerParameter, pentity, input_id )
				.OnValueChanged( this, &FEntityPropertyCustomisation::ChangeIntegerParameter, pentity, input_id )
				.OnValueCommitted( this, &FEntityPropertyCustomisation::CommitIntegerParameter, pentity, input_id )
				.IsEnabled( this, &FEntityPropertyCustomisation::CheckParameterEditable, pentity, input_id )
				.MinValue( FApparanceUtility::FindMetadataInteger( prop_hierarchy, input_id, "MinLimit", c_DefaultLimitMinI ) )
				.MaxValue( FApparanceUtility::FindMetadataInteger( prop_hierarchy, input_id, "MaxLimit", c_DefaultLimitMaxI ) )
				.MinSliderValue( FApparanceUtility::FindMetadataInteger( prop_hierarchy, input_id, "Min", c_DefaultSliderMinI ) )
				.MaxSliderValue( FApparanceUtility::FindMetadataInteger( prop_hierarchy, input_id, "Max", c_DefaultSliderMaxI ) )
		]
		+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(6, 0, 0, 0)
			[
				SNew( SResetToDefaultParameter, pentity, input_id )
				.IsEnabled( this, &FEntityPropertyCustomisation::CheckParameterEditable, pentity, input_id )
		];
}


/////////////////////////////////// FLOAT ///////////////////////////////////

// float parameter getter
//
float FEntityPropertyCustomisation::GetFloatParameter( IProceduralObject* pentity, Apparance::ValueID input_id ) const
{
	TArray<const Apparance::IParameterCollection*> parameter_cascade;
	BeginParameterAccess( pentity, parameter_cascade );
	
	//attempt to obtain current value
	float value = 0;
	for (int i = 0; i < parameter_cascade.Num(); i++)
	{
		const Apparance::IParameterCollection* params = parameter_cascade[i];
		if (params->FindFloat(input_id, &value))
		{
			break; //found
		}
	}
	
	EndParameterAccess( pentity, parameter_cascade );
	return value;	
}
// float parameter setter (either full editing modification, or just pre-edit preview)
//
void FEntityPropertyCustomisation::SetFloatParameter( float new_value, IProceduralObject* pentity, Apparance::ValueID input_id, bool editing_transaction )
{
	Apparance::IParameterCollection* ent_params = BeginParameterEdit( pentity, editing_transaction );	
	
	//set parameter (in-case outstanding change)
	if(!ent_params->ModifyFloat( input_id, new_value ))
	{
		//if not present, needs adding
		int index = ent_params->AddParameter( Apparance::Parameter::Float, input_id );
		ent_params->SetFloat( index, new_value );
	}

	EndParameterEdit( pentity, editing_transaction, input_id );
}

// float parameter editing
//
TSharedPtr<SWidget> FEntityPropertyCustomisation::GenerateFloatEditControl( 
	IProceduralObject* pentity, 
	Apparance::ValueID input_id, 
	const TArray<const Apparance::IParameterCollection*>* prop_hierarchy )
{
	bool is_worldspace = IsWorldspace(input_id, prop_hierarchy);
	float spin_speed = CalculateSpinSpeed(is_worldspace);

	return SNew( SHorizontalBox )
		+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.FillWidth(1.0f)
			.MaxWidth(105.0f)
			[	
				SNew( SNumericEntryBoxFix<float> )
				.AllowSpin( true )
				.Delta( spin_speed )
				//.ToolTipText()
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Value( this, &FEntityPropertyCustomisation::CurrentFloatParameter, pentity, input_id )
				.OnValueChanged( this, &FEntityPropertyCustomisation::ChangeFloatParameter, pentity, input_id )
				.OnValueCommitted( this, &FEntityPropertyCustomisation::CommitFloatParameter, pentity, input_id )
				.IsEnabled( this, &FEntityPropertyCustomisation::CheckParameterEditable, pentity, input_id )
				.MinValue( FApparanceUtility::FindMetadataFloat( prop_hierarchy, input_id, "MinLimit", c_DefaultLimitMinF ) )
				.MaxValue( FApparanceUtility::FindMetadataFloat( prop_hierarchy, input_id, "MaxLimit", c_DefaultLimitMaxF ) )
				.MinSliderValue( FApparanceUtility::FindMetadataFloat( prop_hierarchy, input_id, "Min", c_DefaultSliderMinF ) )
				.MaxSliderValue( FApparanceUtility::FindMetadataFloat( prop_hierarchy, input_id, "Max", c_DefaultSliderMaxF ) )
		]
		+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(6, 0, 0, 0)
			[
				SNew( SResetToDefaultParameter, pentity, input_id )
				.IsEnabled( this, &FEntityPropertyCustomisation::CheckParameterEditable, pentity, input_id )
		];
}

/////////////////////////////////// BOOL ///////////////////////////////////

// bool parameter getter
//
bool FEntityPropertyCustomisation::GetBoolParameter( IProceduralObject* pentity, Apparance::ValueID input_id ) const
{
	TArray<const Apparance::IParameterCollection*> parameter_cascade;
	BeginParameterAccess( pentity, parameter_cascade );
	
	//attempt to obtain current value
	bool value = false;
	for (int i = 0; i < parameter_cascade.Num(); i++)
	{
		const Apparance::IParameterCollection* params = parameter_cascade[i];
		if (params->FindBool(input_id, &value))
		{
			break; //found
		}
	}
	
	EndParameterAccess( pentity, parameter_cascade );
	return value;	
}
// bool parameter setter (either full editing modification, or just pre-edit preview)
//
void FEntityPropertyCustomisation::SetBoolParameter( bool new_value, IProceduralObject* pentity, Apparance::ValueID input_id, bool editing_transaction )
{
	Apparance::IParameterCollection* ent_params = BeginParameterEdit( pentity, editing_transaction );	
	
	//set parameter (in-case outstanding change)
	if(!ent_params->ModifyBool( input_id, new_value ))
	{
		//if not present, needs adding
		int index = ent_params->AddParameter( Apparance::Parameter::Bool, input_id );
		ent_params->SetBool( index, new_value );
	}

	EndParameterEdit( pentity, editing_transaction, input_id );
}

// bool parameter editing
//
TSharedPtr<SWidget> FEntityPropertyCustomisation::GenerateBoolEditControl( IProceduralObject* pentity, Apparance::ValueID input_id, const TArray<const Apparance::IParameterCollection*>* prop_hierarchy )
{
	return SNew( SHorizontalBox )
		+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.FillWidth(1.0f)
			.MaxWidth(105.0f)
			[	
				SNew( SCheckBox )
				.Type(ESlateCheckBoxType::CheckBox)
				//.ToolTipText()
				.IsChecked( this, &FEntityPropertyCustomisation::CurrentBoolParameter, pentity, input_id )
				.OnCheckStateChanged( this, &FEntityPropertyCustomisation::ChangeBoolParameter, pentity, input_id )
				.IsEnabled( this, &FEntityPropertyCustomisation::CheckParameterEditable, pentity, input_id )
		]
		+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(6, 0, 0, 0)
			[
				SNew( SResetToDefaultParameter, pentity, input_id )
				.IsEnabled( this, &FEntityPropertyCustomisation::CheckParameterEditable, pentity, input_id )
			];
}

/////////////////////////////////// COLOUR ///////////////////////////////////

// colour parameter getter
//
FLinearColor FEntityPropertyCustomisation::GetColourParameter( IProceduralObject* pentity, Apparance::ValueID input_id ) const
{
	TArray<const Apparance::IParameterCollection*> parameter_cascade;
	BeginParameterAccess( pentity, parameter_cascade );
	
	//attempt to obtain current value
	Apparance::Colour value;
	value.A = 1.0f;
	for (int i = 0; i < parameter_cascade.Num(); i++)
	{
		const Apparance::IParameterCollection* params = parameter_cascade[i];
		if (params->FindColour(input_id, &value))
		{
			break; //found
		}
	}
	
	EndParameterAccess( pentity, parameter_cascade );
	return UNREALLINEARCOLOR_FROM_APPARANCECOLOUR( value );
}
// colour parameter setter (either full editing modification, or just pre-edit preview)
//
void FEntityPropertyCustomisation::SetColourParameter( FLinearColor new_value, IProceduralObject* pentity, Apparance::ValueID input_id, bool editing_transaction )
{
	Apparance::IParameterCollection* ent_params = BeginParameterEdit( pentity, editing_transaction );	
	
	//set parameter (in-case outstanding change)
	Apparance::Colour new_colour = APPARANCECOLOUR_FROM_UNREALLINEARCOLOR( new_value );
	if(!ent_params->ModifyColour( input_id, &new_colour ))
	{
		//if not present, needs adding
		int index = ent_params->AddParameter( Apparance::Parameter::Colour, input_id );
		ent_params->SetColour( index, &new_colour );
	}

	EndParameterEdit( pentity, editing_transaction, input_id );
}

FReply FEntityPropertyCustomisation::OnClickColorBlock(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, IProceduralObject* pentity, Apparance::ValueID input_id)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}
	
	FColorPickerArgs PickerArgs;
	{
		PickerArgs.bOnlyRefreshOnOk = false;
		PickerArgs.DisplayGamma = TAttribute<float>::Create(TAttribute<float>::FGetter::CreateUObject(GEngine, &UEngine::GetDisplayGamma));
		PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateSP(this, &FEntityPropertyCustomisation::OnSetColorFromColorPicker, pentity, input_id);
		PickerArgs.OnInteractivePickBegin = FSimpleDelegate::CreateSP( this, &FEntityPropertyCustomisation::OnColorPickerInteractiveBegin );
		PickerArgs.OnInteractivePickEnd = FSimpleDelegate::CreateSP( this, &FEntityPropertyCustomisation::OnColorPickerInteractiveEnd, pentity, input_id );
#if UE_VERSION_OLDER_THAN(5,2,0)
		PickerArgs.InitialColorOverride = GetColourParameter( pentity, input_id );
#else //5.2+
		PickerArgs.InitialColor = GetColourParameter( pentity, input_id );
#endif
	}
	
	OpenColorPicker(PickerArgs);
	
	return FReply::Handled();
}

void FEntityPropertyCustomisation::OnSetColorFromColorPicker(FLinearColor NewColor, IProceduralObject* pentity, Apparance::ValueID input_id)
{
	LastColourValue = NewColor;
	SetColourParameter( NewColor, pentity, input_id, !bColourPickingInteractive ); //transaction
}

void FEntityPropertyCustomisation::OnColorPickerInteractiveBegin()
{ 
	bColourPickingInteractive = true; 
}

void FEntityPropertyCustomisation::OnColorPickerInteractiveEnd( IProceduralObject* pentity, Apparance::ValueID input_id )
{ 
	bColourPickingInteractive = false; 

	//final value apply
	SetColourParameter( LastColourValue, pentity, input_id, true ); //transaction
}


// colour parameter editing
//
TSharedPtr<SWidget> FEntityPropertyCustomisation::GenerateColourEditControl( IProceduralObject* pentity, Apparance::ValueID input_id, const TArray<const Apparance::IParameterCollection*>* prop_hierarchy )
{
	return SNew( SHorizontalBox )
		+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.FillWidth(1.0f)
			.MaxWidth(105.0f)
			[	
				SNew( SColorBlock )
				.Color(this, &FEntityPropertyCustomisation::CurrentColourParameter, pentity, input_id )
				.ShowBackgroundForAlpha(true)
#if UE_VERSION_OLDER_THAN(5,0,0)
				.IgnoreAlpha(false)
#else
				.AlphaDisplayMode( EColorBlockAlphaDisplayMode::Ignore )
#endif
				.OnMouseButtonDown(this, &FEntityPropertyCustomisation::OnClickColorBlock, pentity, input_id )
				.IsEnabled( this, &FEntityPropertyCustomisation::CheckParameterEditable, pentity, input_id )
				//.Size(FVector2D(EditColorWidth, EditColorHeight))
			]
		+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(6, 0, 0, 0)
			[
				SNew( SResetToDefaultParameter, pentity, input_id )
				.IsEnabled( this, &FEntityPropertyCustomisation::CheckParameterEditable, pentity, input_id )
			];
}

/////////////////////////////////// STRING ///////////////////////////////////

// string parameter getter
//
FText FEntityPropertyCustomisation::GetStringParameter( IProceduralObject* pentity, Apparance::ValueID input_id ) const
{
	TArray<const Apparance::IParameterCollection*> parameter_cascade;
	BeginParameterAccess( pentity, parameter_cascade );
	
	//attempt to obtain current value
	FText value;
	int text_len = 0;
	for (int i = 0; i < parameter_cascade.Num(); i++)
	{
		const Apparance::IParameterCollection* params = parameter_cascade[i];
		if(params->FindString( input_id, 0, nullptr, &text_len ))
		{
			//retrieve string
			FString text_val = FString::ChrN( text_len, TCHAR(' ') );	
			TCHAR* pbuffer = text_val.GetCharArray().GetData();
			params->FindString( input_id, text_len, pbuffer );
			value = FText::FromString( text_val );		

			break; //found
		}
	}
	
	EndParameterAccess( pentity, parameter_cascade );
	return value;	
}
// string parameter setter (either full editing modification, or just pre-edit preview)
//
void FEntityPropertyCustomisation::SetStringParameter( FText new_value, IProceduralObject* pentity, Apparance::ValueID input_id, bool editing_transaction )
{
	Apparance::IParameterCollection* ent_params = BeginParameterEdit( pentity, editing_transaction );	
	
	//pointer to text
	const FString& string_value = new_value.ToString();
	const TCHAR* pbuffer = string_value.GetCharArray().GetData();
	int string_len = string_value.Len();

	//set parameter (in-case outstanding change)
	if(!ent_params->ModifyString( input_id, string_len, pbuffer ))
	{
		//if not present, needs adding
		int index = ent_params->AddParameter( Apparance::Parameter::String, input_id );
		ent_params->SetString( index, string_len, pbuffer );
	}

	EndParameterEdit( pentity, editing_transaction, input_id );
}

// string parameter editing
//
TSharedPtr<SWidget> FEntityPropertyCustomisation::GenerateStringEditControl( IProceduralObject* pentity, Apparance::ValueID input_id, const TArray<const Apparance::IParameterCollection*>* prop_hierarchy )
{
	return SNew( SHorizontalBox )
		+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.FillWidth(1.0f)
			[	
				SNew(SVerticalBox)				
				+SVerticalBox::Slot()
				.AutoHeight()
				.MaxHeight(500)
				[
					SNew( SMultiLineEditableTextBox )
					//.ToolTipText()
					//.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text( this, &FEntityPropertyCustomisation::CurrentStringParameter, pentity, input_id )
					.OnTextChanged( this, &FEntityPropertyCustomisation::ChangeStringParameter, pentity, input_id )
					.OnTextCommitted( this, &FEntityPropertyCustomisation::CommitStringParameter, pentity, input_id )
					.IsEnabled( this, &FEntityPropertyCustomisation::CheckParameterEditable, pentity, input_id )
				]
			]
		+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(6, 0, 0, 0)
			[
				SNew( SResetToDefaultParameter, pentity, input_id )
				.IsEnabled( this, &FEntityPropertyCustomisation::CheckParameterEditable, pentity, input_id )
			];
}


/////////////////////////////////// VECTOR ///////////////////////////////////

float FEntityPropertyCustomisation::GetVectorParameterComponent( IProceduralObject* pentity, Apparance::ValueID input_id, EApparanceFrameAxis axis ) const
{
	TArray<const Apparance::IParameterCollection*> parameter_cascade;
	BeginParameterAccess( pentity, parameter_cascade );
	
	//attempt to obtain current value
	Apparance::Vector3 vector;
	for (int i = 0; i < parameter_cascade.Num(); i++)
	{
		const Apparance::IParameterCollection* params = parameter_cascade[i];
		if (params->FindVector3(input_id, &vector))
		{
			break; //found
		}
	}
	
	EndParameterAccess( pentity, parameter_cascade );
	return GetVectorComponent( vector, axis );
}
void FEntityPropertyCustomisation::SetVectorParameterComponent( float new_value, IProceduralObject* pentity, Apparance::ValueID input_id, EApparanceFrameAxis axis, bool editing_transaction )
{
	//get previous parameter value (we are only changing one component of it)
	TArray<const Apparance::IParameterCollection*> parameter_cascade;
	BeginParameterAccess( pentity, parameter_cascade );	
	//attempt to obtain current value
	Apparance::Vector3 vector;
	for (int i = 0; i < parameter_cascade.Num(); i++)
	{
		const Apparance::IParameterCollection* params = parameter_cascade[i];
		if (params->FindVector3(input_id, &vector))
		{
			break; //found
		}
	}	
	EndParameterAccess( pentity, parameter_cascade );
	
	//modify
	Apparance::IParameterCollection* ent_params = BeginParameterEdit( pentity, editing_transaction );
	
	Apparance::Vector3 new_vector = vector;
	SetVectorComponent( new_vector, new_value, axis );

	//set parameter (in-case outstanding change)
	if(!ent_params->ModifyVector3( input_id, &new_vector ))
	{
		//if not present, needs adding
		int index = ent_params->AddParameter( Apparance::Parameter::Vector3, input_id );
		ent_params->SetVector3( index, &new_vector );
	}	
	
	EndParameterEdit( pentity, editing_transaction, input_id );
}

// is this parameter in worldspace?
//
bool FEntityPropertyCustomisation::IsWorldspace(Apparance::ValueID input_id, const TArray<const Apparance::IParameterCollection*>* prop_hierarchy) const
{
#if APPARANCE_ENABLE_SPATIAL_MAPPING
	return FApparanceUtility::FindMetadataBool(prop_hierarchy, input_id, "Worldspace");
#else
	return false;
#endif
}

// suitable spin speed for scale of object
// NOTE: currently based on worldspace or not
// TODO: maybe base on current magnitude too?
//
float FEntityPropertyCustomisation::CalculateSpinSpeed(bool is_worldspace) const
{
	return is_worldspace?0.25f:0.0025f;
}

// vector parameter editing
//
TSharedPtr<SWidget> FEntityPropertyCustomisation::GenerateVectorEditControl( IProceduralObject* pentity, Apparance::ValueID input_id, const TArray<const Apparance::IParameterCollection*>* prop_hierarchy )
{
	bool is_worldspace = IsWorldspace(input_id, prop_hierarchy);
	float spin_speed = CalculateSpinSpeed(is_worldspace);
	
	return SNew( SHorizontalBox )
		+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.FillWidth(1.0f)
			[	
				SNew( SVectorInputBoxFix )
				.X( this, &FEntityPropertyCustomisation::CurrentVectorParameterComponent, pentity, input_id, EApparanceFrameAxis::X )
				.Y( this, &FEntityPropertyCustomisation::CurrentVectorParameterComponent, pentity, input_id, EApparanceFrameAxis::Y )
				.Z( this, &FEntityPropertyCustomisation::CurrentVectorParameterComponent, pentity, input_id, EApparanceFrameAxis::Z )
				.bColorAxisLabels( true )
#if UE_VERSION_OLDER_THAN(5,0,0)
				.AllowResponsiveLayout( true )
#endif
				//.IsEnabled( this, &FComponentTransformDetails::GetIsEnabled )
				.OnXChanged( this, &FEntityPropertyCustomisation::ChangeVectorParameterComponent, pentity, input_id, EApparanceFrameAxis::X )
				.OnYChanged( this, &FEntityPropertyCustomisation::ChangeVectorParameterComponent, pentity, input_id, EApparanceFrameAxis::Y )
				.OnZChanged( this, &FEntityPropertyCustomisation::ChangeVectorParameterComponent, pentity, input_id, EApparanceFrameAxis::Z )
				.OnXCommitted( this, &FEntityPropertyCustomisation::CommitVectorParameterComponent, pentity, input_id, EApparanceFrameAxis::X )
				.OnYCommitted( this, &FEntityPropertyCustomisation::CommitVectorParameterComponent, pentity, input_id, EApparanceFrameAxis::Y )
				.OnZCommitted( this, &FEntityPropertyCustomisation::CommitVectorParameterComponent, pentity, input_id, EApparanceFrameAxis::Z )
				//.ContextMenuExtenderX( this, &FComponentTransformDetails::ExtendXScaleContextMenu )
				//.ContextMenuExtenderY( this, &FComponentTransformDetails::ExtendYScaleContextMenu )
				//.ContextMenuExtenderZ( this, &FComponentTransformDetails::ExtendZScaleContextMenu )
				//.Font( FontInfo )
				.AllowSpin( true )
				.SpinDelta( spin_speed )
				//.OnBeginSliderMovement( this, &FComponentTransformDetails::OnBeginScaleSlider )
				//.OnEndSliderMovement( this, &FComponentTransformDetails::OnEndScaleSlider )
				.IsEnabled( this, &FEntityPropertyCustomisation::CheckParameterEditable, pentity, input_id )
			]
		+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(6, 0, 0, 0)
			[
				SNew( SResetToDefaultParameter, pentity, input_id )
				.IsEnabled( this, &FEntityPropertyCustomisation::CheckParameterEditable, pentity, input_id )
			];
}


/////////////////////////////////// FRAME ///////////////////////////////////

float FEntityPropertyCustomisation::GetFrameParameterComponent( IProceduralObject* pentity, Apparance::ValueID input_id, EApparanceFrameComponent component, EApparanceFrameAxis axis, bool is_worldspace ) const
{
	TArray<const Apparance::IParameterCollection*> parameter_cascade;
	BeginParameterAccess( pentity, parameter_cascade );
	
	//attempt to obtain current value
	Apparance::Frame frame;
	for (int i = 0; i < parameter_cascade.Num(); i++)
	{
		const Apparance::IParameterCollection* params = parameter_cascade[i];
		if (params->FindFrame(input_id, &frame))
		{
			break; //found
		}
	}
	
	EndParameterAccess( pentity, parameter_cascade );
	return GetFrameComponent( frame, component, axis, is_worldspace );
}
void FEntityPropertyCustomisation::SetFrameParameterComponent( float new_value, IProceduralObject* pentity, Apparance::ValueID input_id, EApparanceFrameComponent component, EApparanceFrameAxis axis, bool editing_transaction, bool is_worldspace )
{
	//get previous parameter value (we are only changing one component of it)
	TArray<const Apparance::IParameterCollection*> parameter_cascade;
	BeginParameterAccess( pentity, parameter_cascade );	
	//attempt to obtain current value
	Apparance::Frame frame;
	for (int i = 0; i < parameter_cascade.Num(); i++)
	{
		const Apparance::IParameterCollection* params = parameter_cascade[i];
		if (params->FindFrame(input_id, &frame))
		{
			break; //found
		}
	}
	EndParameterAccess( pentity, parameter_cascade );
	
	//modify
	Apparance::IParameterCollection* ent_params = BeginParameterEdit( pentity, editing_transaction );
	
	Apparance::Frame new_frame = frame;
	SetFrameComponent( new_frame, new_value, component, axis, is_worldspace );
	AApparanceEntity::ValidateFrame( new_frame );

	//set parameter (in-case outstanding change)
	if(!ent_params->ModifyFrame( input_id, &new_frame ))
	{
		//if not present, needs adding
		int index = ent_params->AddParameter( Apparance::Parameter::Frame, input_id );
		ent_params->SetFrame( index, &new_frame );
	}	
	
	EndParameterEdit( pentity, editing_transaction, input_id );
}

// frame parameter editing
//
TSharedPtr<SWidget> FEntityPropertyCustomisation::GenerateFrameEditControl( IProceduralObject* pentity, Apparance::ValueID input_id, const TArray<const Apparance::IParameterCollection*>* prop_hierarchy )
{
	bool is_worldspace = IsWorldspace( input_id, prop_hierarchy );
	float spin_speed = CalculateSpinSpeed(is_worldspace);
	
	return SNew( SHorizontalBox )	

		//component labels
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding( 0, 0, 6, 0 )
		.VAlign( VAlign_Center )
		[
			SNew( SVerticalBox )
			+SVerticalBox::Slot()
			.VAlign( VAlign_Center )
			.Padding( 0, 3, 0, 3 )
			[
				// size
				SNew(STextBlock).Text(LOCTEXT("FrameSizeLabel","Size:"))
			]
			+SVerticalBox::Slot()
			.VAlign( VAlign_Center )
			.Padding( 0, 3, 0, 3 )
			[
				// origin
				SNew(STextBlock).Text(LOCTEXT("FrameOriginLabel","Origin:"))
			]
			// orientation
			+SVerticalBox::Slot()
			.VAlign( VAlign_Center )
			.Padding( 0, 3, 0, 3 )
			[
				SNew( STextBlock ).Text( LOCTEXT( "FrameOrientationLabel", "Rotation:" ) )
			]
		]
		
		//component edit boxes
		+SHorizontalBox::Slot()
		.VAlign( VAlign_Center )
		.FillWidth( 1.0f )
		[
			SNew( SVerticalBox )
			+SVerticalBox::Slot()
			[
				// size
				SNew( SVectorInputBoxFix )
				.X( this, &FEntityPropertyCustomisation::CurrentFrameParameterComponent, pentity, input_id, EApparanceFrameElement( EApparanceFrameComponent::Size, EApparanceFrameAxis::X ), is_worldspace )
				.Y( this, &FEntityPropertyCustomisation::CurrentFrameParameterComponent, pentity, input_id, EApparanceFrameElement( EApparanceFrameComponent::Size, EApparanceFrameAxis::Y ), is_worldspace )
				.Z( this, &FEntityPropertyCustomisation::CurrentFrameParameterComponent, pentity, input_id, EApparanceFrameElement( EApparanceFrameComponent::Size, EApparanceFrameAxis::Z ), is_worldspace )
				.bColorAxisLabels( true )
#if UE_VERSION_OLDER_THAN(5,0,0)
				.AllowResponsiveLayout( true )
#endif
				//.IsEnabled( this, &FComponentTransformDetails::GetIsEnabled )
				.OnXChanged( this, &FEntityPropertyCustomisation::ChangeFrameParameterComponent, pentity, input_id, EApparanceFrameElement( EApparanceFrameComponent::Size, EApparanceFrameAxis::X ), is_worldspace )
				.OnYChanged( this, &FEntityPropertyCustomisation::ChangeFrameParameterComponent, pentity, input_id, EApparanceFrameElement( EApparanceFrameComponent::Size, EApparanceFrameAxis::Y ), is_worldspace )
				.OnZChanged( this, &FEntityPropertyCustomisation::ChangeFrameParameterComponent, pentity, input_id, EApparanceFrameElement( EApparanceFrameComponent::Size, EApparanceFrameAxis::Z ), is_worldspace )
				.OnXCommitted( this, &FEntityPropertyCustomisation::CommitFrameParameterComponent, pentity, input_id, EApparanceFrameElement( EApparanceFrameComponent::Size, EApparanceFrameAxis::X ), is_worldspace )
				.OnYCommitted( this, &FEntityPropertyCustomisation::CommitFrameParameterComponent, pentity, input_id, EApparanceFrameElement( EApparanceFrameComponent::Size, EApparanceFrameAxis::Y ), is_worldspace )
				.OnZCommitted( this, &FEntityPropertyCustomisation::CommitFrameParameterComponent, pentity, input_id, EApparanceFrameElement( EApparanceFrameComponent::Size, EApparanceFrameAxis::Z ), is_worldspace )
				//.ContextMenuExtenderX( this, &FComponentTransformDetails::ExtendXScaleContextMenu )
				//.ContextMenuExtenderY( this, &FComponentTransformDetails::ExtendYScaleContextMenu )
				//.ContextMenuExtenderZ( this, &FComponentTransformDetails::ExtendZScaleContextMenu )
				//.Font( FontInfo )
				.AllowSpin( true )
				.SpinDelta( spin_speed )
				//.OnBeginSliderMovement( this, &FComponentTransformDetails::OnBeginScaleSlider )
				//.OnEndSliderMovement( this, &FComponentTransformDetails::OnEndScaleSlider )
				.IsEnabled( this, &FEntityPropertyCustomisation::CheckParameterEditable, pentity, input_id )
			]
			+ SVerticalBox::Slot()
			[
				// origin
				SNew( SVectorInputBoxFix )
				.X( this, &FEntityPropertyCustomisation::CurrentFrameParameterComponent, pentity, input_id, EApparanceFrameElement( EApparanceFrameComponent::Offset, EApparanceFrameAxis::X ), is_worldspace )
				.Y( this, &FEntityPropertyCustomisation::CurrentFrameParameterComponent, pentity, input_id, EApparanceFrameElement( EApparanceFrameComponent::Offset, EApparanceFrameAxis::Y ), is_worldspace )
				.Z( this, &FEntityPropertyCustomisation::CurrentFrameParameterComponent, pentity, input_id, EApparanceFrameElement( EApparanceFrameComponent::Offset, EApparanceFrameAxis::Z ), is_worldspace )
				.bColorAxisLabels( true )
#if UE_VERSION_OLDER_THAN(5,0,0)
				.AllowResponsiveLayout( true )
#endif
				//.IsEnabled( this, &FComponentTransformDetails::GetIsEnabled )
				.OnXChanged( this, &FEntityPropertyCustomisation::ChangeFrameParameterComponent, pentity, input_id, EApparanceFrameElement( EApparanceFrameComponent::Offset, EApparanceFrameAxis::X ), is_worldspace )
				.OnYChanged( this, &FEntityPropertyCustomisation::ChangeFrameParameterComponent, pentity, input_id, EApparanceFrameElement( EApparanceFrameComponent::Offset, EApparanceFrameAxis::Y ), is_worldspace )
				.OnZChanged( this, &FEntityPropertyCustomisation::ChangeFrameParameterComponent, pentity, input_id, EApparanceFrameElement( EApparanceFrameComponent::Offset, EApparanceFrameAxis::Z ), is_worldspace )
				.OnXCommitted( this, &FEntityPropertyCustomisation::CommitFrameParameterComponent, pentity, input_id, EApparanceFrameElement( EApparanceFrameComponent::Offset, EApparanceFrameAxis::X ), is_worldspace )
				.OnYCommitted( this, &FEntityPropertyCustomisation::CommitFrameParameterComponent, pentity, input_id, EApparanceFrameElement( EApparanceFrameComponent::Offset, EApparanceFrameAxis::Y ), is_worldspace )
				.OnZCommitted( this, &FEntityPropertyCustomisation::CommitFrameParameterComponent, pentity, input_id, EApparanceFrameElement( EApparanceFrameComponent::Offset, EApparanceFrameAxis::Z ), is_worldspace )
				//.ContextMenuExtenderX( this, &FComponentTransformDetails::ExtendXScaleContextMenu )
				//.ContextMenuExtenderY( this, &FComponentTransformDetails::ExtendYScaleContextMenu )
				//.ContextMenuExtenderZ( this, &FComponentTransformDetails::ExtendZScaleContextMenu )
				//.Font( FontInfo )
				.AllowSpin( true )
				.SpinDelta( spin_speed )
				//.OnBeginSliderMovement( this, &FComponentTransformDetails::OnBeginScaleSlider )
				//.OnEndSliderMovement( this, &FComponentTransformDetails::OnEndScaleSlider )
				.IsEnabled( this, &FEntityPropertyCustomisation::CheckParameterEditable, pentity, input_id )			
			]
			+ SVerticalBox::Slot()
			[
				// rotation
				SNew( SVectorInputBoxFix )
				.X( this, &FEntityPropertyCustomisation::CurrentFrameParameterComponent, pentity, input_id, EApparanceFrameElement( EApparanceFrameComponent::Orientation, EApparanceFrameAxis::X ), is_worldspace )
				.Y( this, &FEntityPropertyCustomisation::CurrentFrameParameterComponent, pentity, input_id, EApparanceFrameElement( EApparanceFrameComponent::Orientation, EApparanceFrameAxis::Y ), is_worldspace )
				.Z( this, &FEntityPropertyCustomisation::CurrentFrameParameterComponent, pentity, input_id, EApparanceFrameElement( EApparanceFrameComponent::Orientation, EApparanceFrameAxis::Z ), is_worldspace )
				.bColorAxisLabels( true )
#if UE_VERSION_OLDER_THAN(5,0,0)
				.AllowResponsiveLayout( true )
#endif
				//.IsEnabled( this, &FComponentTransformDetails::GetIsEnabled )
				.OnXChanged( this, &FEntityPropertyCustomisation::ChangeFrameParameterComponent, pentity, input_id, EApparanceFrameElement( EApparanceFrameComponent::Orientation, EApparanceFrameAxis::X ), is_worldspace )
				.OnYChanged( this, &FEntityPropertyCustomisation::ChangeFrameParameterComponent, pentity, input_id, EApparanceFrameElement( EApparanceFrameComponent::Orientation, EApparanceFrameAxis::Y ), is_worldspace )
				.OnZChanged( this, &FEntityPropertyCustomisation::ChangeFrameParameterComponent, pentity, input_id, EApparanceFrameElement( EApparanceFrameComponent::Orientation, EApparanceFrameAxis::Z ), is_worldspace )
				.OnXCommitted( this, &FEntityPropertyCustomisation::CommitFrameParameterComponent, pentity, input_id, EApparanceFrameElement( EApparanceFrameComponent::Orientation, EApparanceFrameAxis::X ), is_worldspace )
				.OnYCommitted( this, &FEntityPropertyCustomisation::CommitFrameParameterComponent, pentity, input_id, EApparanceFrameElement( EApparanceFrameComponent::Orientation, EApparanceFrameAxis::Y ), is_worldspace )
				.OnZCommitted( this, &FEntityPropertyCustomisation::CommitFrameParameterComponent, pentity, input_id, EApparanceFrameElement( EApparanceFrameComponent::Orientation, EApparanceFrameAxis::Z ), is_worldspace )
				//.ContextMenuExtenderX( this, &FComponentTransformDetails::ExtendXScaleContextMenu )
				//.ContextMenuExtenderY( this, &FComponentTransformDetails::ExtendYScaleContextMenu )
				//.ContextMenuExtenderZ( this, &FComponentTransformDetails::ExtendZScaleContextMenu )
				//.Font( FontInfo )
				.AllowSpin( true )
				.SpinDelta(0.25f) //angles, not length/scale/distance
				//.OnBeginSliderMovement( this, &FComponentTransformDetails::OnBeginScaleSlider )
				//.OnEndSliderMovement( this, &FComponentTransformDetails::OnEndScaleSlider )
				.IsEnabled( this, &FEntityPropertyCustomisation::CheckParameterEditable, pentity, input_id )
			]
		
		]

		// reset to default
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.Padding(6, 0, 0, 0)
		[
			SNew( SResetToDefaultParameter, pentity, input_id )
			.IsEnabled( this, &FEntityPropertyCustomisation::CheckParameterEditable, pentity, input_id )
		];
}


// start (or during) interactive parameter editing (e.g. slider drag)
//
void FEntityPropertyCustomisation::BeginInteraction()
{
#if 0 //DIDN'T FIX AS INTENDED
	if(!IsInteracting())
	{
		UE_LOG( LogApparance, Display, TEXT( "STARTING INTERACTION" ) );
		//force interactive scene updates (due to a bug in the numericentrybox's use of spinbox we can't fix the issue of uninitialised bAlwaysThrottle sometimes breaking interaction
		InteractivePropertyResponsivnessThrottle = FSlateThrottleManager::Get().EnterResponsiveMode();
	}
#endif
}

// end of interactive parameter editing
//
void FEntityPropertyCustomisation::EndInteraction()
{
#if 0 //DIDN'T FIX AS INTENDED
	if(IsInteracting())
	{
		UE_LOG( LogApparance, Display, TEXT( "ENDING INTERACTION" ) );
		FSlateThrottleManager::Get().LeaveResponsiveMode( InteractivePropertyResponsivnessThrottle );
	}
#endif
}


static void EntityProcedureSelect( TSharedPtr<IPropertyHandle> proc_property, int32 id )
{
	UE_LOG(LogApparance,Display,TEXT("Setting ProcedureID property value %08X"),id );

	proc_property->SetValue( id );

	//affects details panel structure
	FApparanceUnrealEditorModule::GetModule()->NotifyEntityProcedureTypeChanged( id );
}



TSharedRef<class SWidget> FEntityPropertyCustomisation::BuildProceduresMenu()
{
	FMenuBuilder menu( true, nullptr );
	
	//none
	menu.AddMenuEntry( LOCTEXT("ProcedureMenuNone","None"), FText(), FSlateIcon(), FUIAction( FExecuteAction::CreateStatic( &EntityProcedureSelect, ProcedureIDProperty, (int)0 ) ) );

	//current value
	int current_proc_id = 0;
	ProcedureIDProperty->GetValue( current_proc_id );

	//procedures
	auto procedures = FApparanceUnrealModule::GetModule()->GetInventory();
	FString current_cat;
	for(int i=0 ; i<procedures.Num() ; i++)
	{
		//extract
		FString cat = APPARANCE_UTF8_TO_TCHAR( (UTF8CHAR*)procedures[i]->Category );
		FString name = APPARANCE_UTF8_TO_TCHAR( (UTF8CHAR*)procedures[i]->Name );
		if(cat.IsEmpty())
		{
			cat = TEXT( "uncategorised" );
		}
		if(name.IsEmpty())
		{
			name = TEXT( "unnamed" );
		}

		//separators
		if(cat != current_cat) //(change of cat)
		{
			if(!current_cat.IsEmpty()) { menu.EndSection();	}
			menu.BeginSection( NAME_None, FText::FromString( cat ) );
			current_cat = cat;
		}

		//current?
		int id = (int)procedures[i]->ID;
		bool is_checked = id == current_proc_id;

		//entry
		FSlateIcon icon;
		if(is_checked)
		{
#if 0 //HACK: can't work out to check an item nicely
			icon = FSlateIcon( "EditorStyle", "Menu.Button.Checked" );
#else
			name = TEXT( "* " ) + name + TEXT( " *" );
#endif
		}
		menu.AddMenuEntry( FText::FromString( name ), FText(), icon, FUIAction( FExecuteAction::CreateStatic( &EntityProcedureSelect, ProcedureIDProperty, (int)procedures[i]->ID ) ) );
	}
	if(!current_cat.IsEmpty()) { menu.EndSection(); }

	return menu.MakeWidget();	
}



#undef LOCTEXT_NAMESPACE
