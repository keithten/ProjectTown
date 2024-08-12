//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

//main
#include "ResourceListEntryProperties.h"

#include "ApparanceUnrealVersioning.h"

//unreal
//#include "Core.h"
//unreal:editor
#include "Editor.h"
#include "SnappingUtils.h"
//unreal:ui
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IDetailChildrenBuilder.h"
//#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Colors/SColorBlock.h"
#include "WorkflowOrientedApp/SContentReference.h"
#include "ISinglePropertyView.h"
#include "IStructureDetailsView.h"
//unreal:misc
#include "Misc/EngineVersionComparison.h"
#include "HAL/PlatformApplicationMisc.h"
#include "PropertyCustomizationHelpers.h"
#if UE_VERSION_AT_LEAST( 5, 1, 0 )
#include "EditorStyleSet.h"
#endif

//module
#include "ApparanceUnreal.h"
#include "RemoteEditing.h"
#include "ApparanceUnrealEditor.h"
#include "Utility/ApparanceConversion.h"
#include "Utility/ApparanceUtility.h"
#include "ResourceList/ApparanceResourceListEntry_3DAsset.h"
#include "ResourceList/ApparanceResourceListEntry_Blueprint.h"
#include "ResourceList/ApparanceResourceListEntry_Component.h"
#include "ResourceList/ApparanceResourceListEntry_Material.h"
#include "ResourceList/ApparanceResourceListEntry_ResourceList.h"
#include "ResourceList/ApparanceResourceListEntry_StaticMesh.h"
#include "ResourceList/ApparanceResourceListEntry_Variants.h"
#include "ApparanceResourceList.h"
//module:ui
#include "../../../ApparanceUnreal/Public/ResourceList/ApparanceResourceListEntry_Material.h"
#include "UI/SResetToDefaultParameter.h"

#define LOCTEXT_NAMESPACE "ApparanceUnreal"

bool FResourcesListEntryPropertyCustomisation::s_bInlineHelpEnabled = false;
const float ValueContentWidth = 250.0f;		
const float ValueContentWidthTweak = 286.0f;
const float ValueContentWidthWide = 350.0f;

//brush names
namespace FApparanceEditorBrushNames
{
#if UE_VERSION_OLDER_THAN(5,0,0)
	static FName UpArrow( "BlueprintEditor.Details.ArgUpButton" );
	static FName DownArrow( "BlueprintEditor.Details.ArgDownButton" );
#else
	static FName UpArrow( "Icons.ChevronUp" );
	static FName DownArrow( "Icons.ChevronDown" );
#endif

}


/////////////////////////////////// EDITING HELPERS ///////////////////////////////////


static void BeginEdit( UObject* pobject )
{
	//we are going to commit change made to resource list entry
	//(edits are applied to the non-persistent, expanded version of the parameters, then applied)
	GEditor->BeginTransaction( LOCTEXT( "ResourceListEntryEdit", "Edit Resource List Entry" ) );
	
	//we are about to change the entity
	pobject->Modify();
}
static void EndEdit( UObject* pobject )
{
	//done with transaction
	GEditor->EndTransaction();
}
static void NotifyPropertyChanged( UObject* pobject )
{
	//search up for entry
	UApparanceResourceListEntry* pentry = Cast<UApparanceResourceListEntry>( pobject );
	if(!pentry)
	{
		pentry = pobject->GetTypedOuter<UApparanceResourceListEntry>();
	}
	
	//notify
	if(pentry)
	{
		pentry->NotifyPropertyChanged( pobject );
	}
}


/////////////////////////////////// PARAMETER EDITING SUPPORT ///////////////////////////////////

// display name for a type
// Used for: section heading
//
static FText GetTypeDisplayName( UClass* pentry_class )
{
	if(pentry_class == UApparanceResourceListEntry_Variants::StaticClass())
	{
		return LOCTEXT( "ResourceListEntryVariantsDisplayName", "Resource Variants" );
	}
	if(pentry_class == UApparanceResourceListEntry_StaticMesh::StaticClass())
	{
		return LOCTEXT( "ResourceListEntryStaticMeshDisplayName", "Static Mesh" );
	}
	if(pentry_class == UApparanceResourceListEntry_ResourceList::StaticClass())
	{
		return LOCTEXT( "ResourceListEntryResourceListDisplayName", "Resource List" );
	}
	if(pentry_class == UApparanceResourceListEntry_Material::StaticClass())
	{
		return LOCTEXT( "ResourceListEntryMaterialDisplayName", "Material" );
	}
	if(pentry_class == UApparanceResourceListEntry_Component::StaticClass())
	{
		return LOCTEXT( "ResourceListEntryComponentDisplayName", "Actor Component" );
	}
	if(pentry_class == UApparanceResourceListEntry_Blueprint::StaticClass())
	{
		return LOCTEXT( "ResourceListEntryBlueprintDisplayName", "Blueprint" );
	}
	if(pentry_class == UApparanceResourceListEntry_Blueprint::StaticClass())
	{
		return LOCTEXT( "ResourceListEntryBlueprintDisplayName", "Blueprint" );
	}
	if(pentry_class == UApparanceResourceListEntry_3DAsset::StaticClass())
	{
		return LOCTEXT( "ResourceListEntry3DAssetDisplayName", "3D Asset" );
	}
	if(pentry_class == UApparanceResourceListEntry_Asset::StaticClass())
	{
		return LOCTEXT( "ResourceListEntryAssetDisplayName", "Asset" );
	}
	return LOCTEXT( "ResourceListEntryDisplayName", "Resource" );
}

// value source editor helpers
static bool OnIsSourceType(UApparanceValueSource* psource, EApparanceValueSource type)
{
	return psource->Source == type;
}
static ECheckBoxState OnIsSourceTypeChecked(UApparanceValueSource* psource, EApparanceValueSource type)
{
	return OnIsSourceType(psource, type)?ECheckBoxState::Checked:ECheckBoxState::Unchecked;
}
static EVisibility OnIsSourceTypeVisible(UApparanceValueSource* psource, EApparanceValueSource type)
{
	return OnIsSourceType(psource, type)?EVisibility::Visible:EVisibility::Collapsed;
}
static FSlateColor OnGetSourceTypeTextColour(UApparanceValueSource* psource, EApparanceValueSource type)
{
	return OnIsSourceType(psource, type)?FSlateColor(FLinearColor(0, 0, 0)):FSlateColor(FLinearColor(0.72f, 0.72f, 0.72f, 1.f));
}
static void OnSourceTypeChange(ECheckBoxState NewCheckboxState, UApparanceValueSource* psource, EApparanceValueSource type )
{
	if (NewCheckboxState == ECheckBoxState::Checked)
	{
		BeginEdit(psource);
		psource->Source = type;
		EndEdit(psource);
		
		//explicit notify as not a property edit in the conventional sense
		NotifyPropertyChanged( psource );
	}
}
static FText OnGetValueSourceParameterText(UApparanceResourceListEntry_Asset* passet, UApparanceValueSource* psource)
{
	return passet->FindParameterNameText(psource->Parameter);
}
static FText OnGetValueSourceConstantText(UApparanceResourceListEntry_Asset* passet, UApparanceValueSource* psource)
{
	return FText::FromString(psource->DescribeValue(passet));
}
static void OnSetValueSourceParameter(UApparanceValueSource* psource, int parameter_id)
{
	BeginEdit(psource);
	psource->Parameter = parameter_id;
	EndEdit(psource);
	
	//explicit notify as not a property edit in the conventional sense
	NotifyPropertyChanged(psource);
}
static TSharedRef<class SWidget> BuildSourceParameterSelectMenu(UApparanceResourceListEntry_Asset* passet, UApparanceValueSource* psource)
{
	FMenuBuilder menu( true, nullptr );
	
	//none
	menu.AddMenuEntry( LOCTEXT("ParameterMenuNone","None"), FText(), FSlateIcon(), FUIAction( FExecuteAction::CreateStatic( &OnSetValueSourceParameter, psource, (int)0 ) ) );
	
	//parameters
	for (int i = 0; i < passet->ExpectedParameters.Num(); i++)
	{
		FApparancePlacementParameter& placement_param = passet->ExpectedParameters[i];
		menu.AddMenuEntry( FText::FromString( placement_param.Name ), FText(), FSlateIcon(), FUIAction( FExecuteAction::CreateStatic( &OnSetValueSourceParameter, psource, (int)placement_param.ID ) ) );
	}
	//textures (materials only)
	UApparanceResourceListEntry_Material* pmaterial = Cast<UApparanceResourceListEntry_Material>(passet);
	if (pmaterial)
	{
		for (int i = 0; i < pmaterial->ExpectedTextures.Num(); i++)
		{
			FApparancePlacementParameter& placement_param = pmaterial->ExpectedTextures[i];
			menu.AddMenuEntry(FText::FromString(placement_param.Name), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateStatic(&OnSetValueSourceParameter, psource, (int)placement_param.ID)));
		}
	}

	return menu.MakeWidget();
}


/////////////////////////////////// ENTITY PROPERTY CUSTOMISATION ///////////////////////////////////


FResourcesListEntryPropertyCustomisation::FResourcesListEntryPropertyCustomisation()
{
	//font to use (default textblock doesn't match property editor panel)
	DefaultFont = APPARANCE_STYLE.GetFontStyle( "PropertyWindow.NormalFont" );
}

// build UI
//
void FResourcesListEntryPropertyCustomisation::CustomizeDetails( IDetailLayoutBuilder& detail_builder )
{
	//hook edit event
	DetailBuilderPtr = &detail_builder;
	IDetailLayoutBuilder& DetailBuilder = detail_builder;
	DetailBuilder.GetDetailsView()->OnFinishedChangingProperties().AddSP( this, &FResourcesListEntryPropertyCustomisation::OnPropertyChanged );

	//what is being edited
	UApparanceResourceListEntry* pentry = GetEntry<UApparanceResourceListEntry>();
	if(!pentry)
	{
		return;
	}
	UApparanceResourceListEntry* pfolder = (pentry->GetClass()==UApparanceResourceListEntry::StaticClass())?pentry:nullptr;	//pure entry is used as folder
	UApparanceResourceListEntry_Asset* passet = Cast< UApparanceResourceListEntry_Asset>( pentry );
	UApparanceResourceListEntry_3DAsset* p3dasset = Cast< UApparanceResourceListEntry_3DAsset>( pentry );
	UApparanceResourceListEntry_Blueprint* pblueprint = Cast< UApparanceResourceListEntry_Blueprint>( pentry );
	UApparanceResourceListEntry_Component* pcomponent = Cast< UApparanceResourceListEntry_Component>( pentry );
	UApparanceResourceListEntry_Material* pmaterial = Cast< UApparanceResourceListEntry_Material>( pentry );
	UApparanceResourceListEntry_ResourceList* presourcelist = Cast< UApparanceResourceListEntry_ResourceList>( pentry );
	UApparanceResourceListEntry_StaticMesh* pstaticmesh = Cast< UApparanceResourceListEntry_StaticMesh>( pentry );
	UApparanceResourceListEntry_Variants* pvariants = Cast< UApparanceResourceListEntry_Variants>( pentry );

	//derived logic
	UApparanceResourceListEntry* parent = pentry->FindParent();
	bool is_variant = parent && parent->IsA(UApparanceResourceListEntry_Variants::StaticClass());
	bool has_name = pentry->IsNameEditable() && !is_variant;
	bool has_asset = passet && !pcomponent;
	bool is_container = pfolder || pvariants;
	bool has_children = pentry->Children.Num() > 0;
	bool is_parameterised = passet && !presourcelist;
	bool has_descriptor = has_name && !pfolder;
	
	//main property section 
	IDetailCategoryBuilder& res_category = DetailBuilder.EditCategory( TEXT( "ResourceListEntry" ), GetTypeDisplayName( pentry->GetClass() ) );

	//name property
	TSharedRef<IPropertyHandle> NameProp = DetailBuilder.GetProperty( GET_MEMBER_NAME_CHECKED( UApparanceResourceListEntry, Name ), UApparanceResourceListEntry::StaticClass() );
	if(has_name)
	{
		//info
		if (has_descriptor)
		{
			BuildDocumentationText(res_category, LOCTEXT("ResourceListNameHelp", "Resources are named and referenced by a dot delimited descriptor string formed from their location within a resource list asset."));
		}

		//name
		res_category.AddProperty( NameProp );
		
		//descriptor
		if(has_descriptor)
		{
			res_category.AddCustomRow(FText::FromString(TEXT("Name Descriptor")))
			.NameContent()
			[
				SNew( STextBlock )
			.Font(DefaultFont)
			.ToolTipText( LOCTEXT("ResourceListDescriptorLabelTooltip","Full descriptor you can refer to this resource with in procedures") )
			.Text( LOCTEXT("ResourceListDescriptorLabel", "Descriptor" ) )
			]
				.ValueContent()
			.VAlign(VAlign_Center)
			.MaxDesiredWidth(ValueContentWidth)
			.MinDesiredWidth(ValueContentWidth)
			[
				SNew(SEditableText)
			.IsReadOnly(true) //readonly, so copyable
			.Text( this, &FResourcesListEntryPropertyCustomisation::OnGetResourceDescriptor, pentry )
			.Font( DefaultFont )
			];
		}
	}
	else
	{
		DetailBuilder.HideProperty(NameProp);
	}
	//asset property
	TSharedRef<IPropertyHandle> AssetProp = DetailBuilder.GetProperty( GET_MEMBER_NAME_CHECKED( UApparanceResourceListEntry_Asset, Asset ), UApparanceResourceListEntry_Asset::StaticClass() );
	if(has_asset)
	{
		res_category.AddProperty( AssetProp );
	}
	else
	{
		DetailBuilder.HideProperty( AssetProp );
	}

	//bounds
	if(p3dasset)
	{
		IDetailCategoryBuilder& bounds_category = DetailBuilder.EditCategory( TEXT( "Bounds" ), LOCTEXT( "ResourceListEntryBounds", "Bounds" ) );

		BuildDocumentationText(bounds_category, LOCTEXT("ResourceListBoundsHelp", "3D assets need to expose a bounds to the procedural placement system, this is usually obtained from the asset itself but sometimes an alternate mesh can be used when the asset type doesn't allow it."));
		
		TSharedRef<IPropertyHandle> BoundsSourceProp = DetailBuilder.GetProperty( GET_MEMBER_NAME_CHECKED( UApparanceResourceListEntry_3DAsset, BoundsSource ), UApparanceResourceListEntry_3DAsset::StaticClass() );
		BoundsSourceEnumControl = MakeShared<FEnumCustomization>();
		BoundsSourceEnumControl->CreateEnumCustomization( bounds_category, BoundsSourceProp, StaticEnum<EApparanceResourceBoundsSource>(), LOCTEXT( "ResourceListEntryBoundsSourceName", "Source" ) );
		
		//bounds mesh
		TSharedRef<IPropertyHandle> BoundsMeshProp = DetailBuilder.GetProperty( GET_MEMBER_NAME_CHECKED( UApparanceResourceListEntry_3DAsset, BoundsMesh ), UApparanceResourceListEntry_3DAsset::StaticClass() );
		if(p3dasset->BoundsSource==EApparanceResourceBoundsSource::Mesh)
		{
			bounds_category.AddProperty( BoundsMeshProp );
		}
		else
		{
			DetailBuilder.HideProperty( BoundsMeshProp );
		}
		
		//bounds
		bool is_custom_bounds = p3dasset->BoundsSource == EApparanceResourceBoundsSource::Custom;
		TSharedRef<IPropertyHandle> BoundsCentreProp = DetailBuilder.GetProperty( GET_MEMBER_NAME_CHECKED( UApparanceResourceListEntry_3DAsset, BoundsCentre ), UApparanceResourceListEntry_3DAsset::StaticClass() );
		IDetailPropertyRow& centre_row = bounds_category.AddProperty( BoundsCentreProp );
		centre_row.IsEnabled( is_custom_bounds );
		TSharedRef<IPropertyHandle> BoundsExtentProp = DetailBuilder.GetProperty( GET_MEMBER_NAME_CHECKED( UApparanceResourceListEntry_3DAsset, BoundsExtent ), UApparanceResourceListEntry_3DAsset::StaticClass() );
		IDetailPropertyRow& extent_row = bounds_category.AddProperty( BoundsExtentProp );
		extent_row.IsEnabled( is_custom_bounds );
	}
	
	//bounds adjustments
	if(p3dasset || pvariants)
	{
		bool hide_placement = true;
		//both 3d assets and variants have fixup params, but they are distinct
		TSharedRef<IPropertyHandle> FixPlacementProp =
			pvariants
			?DetailBuilder.GetProperty( GET_MEMBER_NAME_CHECKED( UApparanceResourceListEntry_Variants, bFixPlacement ), UApparanceResourceListEntry_Variants::StaticClass() )
			:DetailBuilder.GetProperty( GET_MEMBER_NAME_CHECKED( UApparanceResourceListEntry_3DAsset, bFixPlacement ), UApparanceResourceListEntry_3DAsset::StaticClass() );
		TSharedRef<IPropertyHandle> FixBasisProp = DetailBuilder.GetProperty( GET_MEMBER_NAME_CHECKED( UApparanceResourceListEntry_3DAsset, Override ), UApparanceResourceListEntry_3DAsset::StaticClass() );
		TSharedRef<IPropertyHandle> FixOrientationProp =
			pvariants
			?DetailBuilder.GetProperty( GET_MEMBER_NAME_CHECKED( UApparanceResourceListEntry_Variants, Orientation ), UApparanceResourceListEntry_Variants::StaticClass() )
			:DetailBuilder.GetProperty( GET_MEMBER_NAME_CHECKED( UApparanceResourceListEntry_3DAsset, Orientation ), UApparanceResourceListEntry_3DAsset::StaticClass() );
		TSharedRef<IPropertyHandle> FixRotationProp =
			pvariants
			?DetailBuilder.GetProperty( GET_MEMBER_NAME_CHECKED( UApparanceResourceListEntry_Variants, Rotation ), UApparanceResourceListEntry_Variants::StaticClass() )
			:DetailBuilder.GetProperty( GET_MEMBER_NAME_CHECKED( UApparanceResourceListEntry_3DAsset, Rotation ), UApparanceResourceListEntry_3DAsset::StaticClass() );
		TSharedRef<IPropertyHandle> FixUniformScaleProp =
			pvariants
			?DetailBuilder.GetProperty( GET_MEMBER_NAME_CHECKED( UApparanceResourceListEntry_Variants, UniformScale ), UApparanceResourceListEntry_Variants::StaticClass() )
			:DetailBuilder.GetProperty( GET_MEMBER_NAME_CHECKED( UApparanceResourceListEntry_3DAsset, UniformScale ), UApparanceResourceListEntry_3DAsset::StaticClass() );
		TSharedRef<IPropertyHandle> FixScaleProp =
			pvariants
			?DetailBuilder.GetProperty( GET_MEMBER_NAME_CHECKED( UApparanceResourceListEntry_Variants, Scale ), UApparanceResourceListEntry_Variants::StaticClass() )
			:DetailBuilder.GetProperty( GET_MEMBER_NAME_CHECKED( UApparanceResourceListEntry_3DAsset, Scale ), UApparanceResourceListEntry_3DAsset::StaticClass() );

		IDetailCategoryBuilder& placement_category = DetailBuilder.EditCategory( TEXT( "Fix-ups" ), LOCTEXT( "ResourceListEntryBoundsAdjustmentSection", "Bounds Adjustments" ) );
		
		BuildDocumentationText(placement_category, LOCTEXT("ResourceListBoundsFixupsHelp", "If the asset isn't sized or oriented as expected you can correct for this here, enable fix-ups and select the orientation, rotation, and scaling adjustments needed."));

		//fix placement?
		IDetailPropertyRow& fix_placement_check = placement_category.AddProperty( FixPlacementProp );
		if(pvariants)
		{
			fix_placement_check.DisplayName( LOCTEXT( "ResourceListFixPlacementShared", "Fix All Variants" ) );
		}

		//in effect?
		bool is_active = p3dasset?p3dasset->bFixPlacement:pvariants->bFixPlacement;
		if(is_active)
		{
			//override control
			if(p3dasset)
			{
				FixOverrideEnumControl = MakeShared<FEnumCustomization>();
				FixOverrideEnumControl->CreateEnumCustomization( placement_category, FixBasisProp, StaticEnum<EApparanceResourceTransformOverride>(), LOCTEXT( "ResourceListEntryTransformOverrideName", "Override" ) );
			}
			//orientation control
			FixOrientationEnumControl = MakeShared<FEnumCustomization>();
			FixOrientationEnumControl->CreateEnumCustomization( placement_category, FixOrientationProp, StaticEnum<EApparanceResourceOrientation>(), LOCTEXT( "ResourceListEntryTransformOrientationName", "Orientation" ) );
			placement_category.AddProperty( FixRotationProp );
			placement_category.AddProperty( FixUniformScaleProp );
			placement_category.AddProperty( FixScaleProp );
			hide_placement = false;
		}

		//hide unused
		DetailBuilder.HideProperty( FixPlacementProp );
		if(hide_placement)
		{
			if(p3dasset)
			{
				DetailBuilder.HideProperty( FixBasisProp );
			}
			DetailBuilder.HideProperty( FixOrientationProp );
			DetailBuilder.HideProperty( FixRotationProp );
			DetailBuilder.HideProperty( FixUniformScaleProp );
			DetailBuilder.HideProperty( FixScaleProp );
		}
	}

	//parameters
	if(passet)
	{
		TSharedRef<IPropertyHandle> ExpectedParametersProp = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UApparanceResourceListEntry_Asset, ExpectedParameters), UApparanceResourceListEntry_Asset::StaticClass());
		TSharedRef<IPropertyHandle> ExpectedTexturesProp = DetailBuilder.GetProperty( GET_MEMBER_NAME_CHECKED( UApparanceResourceListEntry_Material, ExpectedTextures ), UApparanceResourceListEntry_Material::StaticClass() );
		DetailBuilder.HideProperty(ExpectedParametersProp);
		if(pmaterial)
		{
			DetailBuilder.HideProperty( ExpectedTexturesProp );
		}
		if (is_parameterised)
		{
			//heading
			IDetailCategoryBuilder& parameter_category = DetailBuilder.EditCategory( TEXT( "Parameters" ), LOCTEXT("ResourceListEntryParameters","Parameters") );
			if (pmaterial)
			{
				BuildDocumentationText(parameter_category, LOCTEXT("ResourceListParameterTextureHelp", "Material resources can be provided with parameters and textures, adding the expected parameters and textures here makes them available for binding to material parameters on the instanced material."));
			}
			else
			{
				BuildDocumentationText(parameter_category, LOCTEXT("ResourceListParameterHelp", "Placed resources can be provided with parameters, adding the expected parameters here makes them available for binding to properties and function call arguments on the instanced asset."));
			}
			parameter_category.AddCustomRow( FText::FromString(TEXT("Parameters")) )
				.NameContent()
				[
					SNew( STextBlock )
					.Font(DefaultFont)
					.ToolTipText( ExpectedParametersProp->GetToolTipText() )
					.Text( ExpectedParametersProp->GetPropertyDisplayName() )
				]
				.ValueContent()
				.VAlign(VAlign_Center)
				.MaxDesiredWidth(ValueContentWidth)
				.MinDesiredWidth(ValueContentWidth)
				[
					SNew(SHorizontalBox)
					//count
					+SHorizontalBox::Slot()
					.Padding(FMargin(3,0))
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Fill)
					.FillWidth(1)
					[
						SNew( STextBlock )
						.Font(DefaultFont)
						.Text( FText::Format( LOCTEXT("ResourceListExpectedParametersCount","{0} parameter(s)"), passet->ExpectedParameters.Num() ) )
					]
					//add
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew( SComboButton )
						//.ComboButtonStyle(APPARANCE_STYLE, "ToolbarComboButton")
						//.ForegroundColor(FLinearColor::White)
						.ContentPadding(FMargin(5,1))
						.ButtonContent()
						[
							SNew(STextBlock)
							//.TextStyle(APPARANCE_STYLE, "Launcher.Filters.Text")
							.Text(LOCTEXT("ResourceListAddParameterButton","+ Add Parameter"))
						]
						.MenuContent()
						[
							BuildParameterTypeSelectMenu( passet )
						]				
					]
				];

			for (int i = 0; i < passet->ExpectedParameters.Num(); i++)
			{
				BuildParameterRow(parameter_category, FText::FromString(TEXT("Parameters")), passet, i);
			}

			//materials have a texture parameter list too?
			if(pmaterial)
			{
				//heading
				parameter_category.AddCustomRow( FText::FromString(TEXT("Textures")) )
					.NameContent()
					[
						SNew( STextBlock )
						.Font(DefaultFont)
						.ToolTipText( ExpectedTexturesProp->GetToolTipText() )
						.Text( ExpectedTexturesProp->GetPropertyDisplayName() )
					]
					.ValueContent()
					.VAlign(VAlign_Center)
					.MaxDesiredWidth(ValueContentWidth)
					.MinDesiredWidth(ValueContentWidth)
					[
						SNew(SHorizontalBox)
						//count
						+SHorizontalBox::Slot()
						.Padding(FMargin(3,0))
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Fill)
						.FillWidth(1)
						[
							SNew( STextBlock )
							.Font(DefaultFont)
							.Text( FText::Format( LOCTEXT("ResourceListExpectedParametersCount","{0} textures(s)"), pmaterial->ExpectedTextures.Num() ) )
						]
						//add
						+SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew( SButton )
							.ContentPadding(FMargin(5,1))
							.Text(LOCTEXT("ResourceListAddTextureButton","+ Add Texture"))
							.OnClicked(this, &FResourcesListEntryPropertyCustomisation::OnAddTexture, pmaterial )
						]
					];

				for (int i = 0; i < pmaterial->ExpectedTextures.Num(); i++)
				{
					BuildTextureRow(parameter_category, FText::FromString(TEXT("Textures")), pmaterial, i);
				}
			}
		}
	}

	//component
	if(pcomponent)
	{
		IDetailCategoryBuilder& component_category = DetailBuilder.EditCategory( TEXT( "Component" ), LOCTEXT("ResourceListEntryComponent","Component") );
		BuildDocumentationText(component_category, LOCTEXT("ResourceListComponentHelp", "Components can be procedurally placed and get added as instances of the provided component template in the Entity Actor. Actions can be performed on the placed component such as setting properties and calling functions to set them up as needed."));
		
		TSharedRef<IPropertyHandle> TemplateTypeProp = DetailBuilder.GetProperty( GET_MEMBER_NAME_CHECKED( UApparanceResourceListEntry_Component, TemplateType ), UApparanceResourceListEntry_Component::StaticClass() );
		TemplateTypeEnumControl = MakeShared<FEnumCustomization>();
		TemplateTypeEnumControl->CreateEnumCustomization( component_category, TemplateTypeProp, StaticEnum<EComponentTemplateType>(), LOCTEXT( "ResourceListEntryComponentTemplateTypeName", "Template Type" ) );

		//component class selection
		TSharedRef<IPropertyHandle> TemplateProp = DetailBuilder.GetProperty( GET_MEMBER_NAME_CHECKED( UApparanceResourceListEntry_Component, ComponentTemplate ), UApparanceResourceListEntry_Component::StaticClass() );
		TSharedRef<IPropertyHandle> BlueprintProp = DetailBuilder.GetProperty( GET_MEMBER_NAME_CHECKED( UApparanceResourceListEntry_Component, BlueprintTemplate ), UApparanceResourceListEntry_Component::StaticClass() );
		switch(pcomponent->TemplateType)
		{
			// Component instance as template
			case EComponentTemplateType::Component:
			{
				component_category.AddProperty( TemplateProp );
				//hide the other
				DetailBuilder.HideProperty( BlueprintProp );
				break;
			}
			// Blueprint asset as template
			case EComponentTemplateType::Blueprint:
			{
				component_category.AddProperty( BlueprintProp );
				//hide the other
				DetailBuilder.HideProperty( TemplateProp );
				break;
			}
		}

		//component setup
		TSharedRef<IPropertyHandle> SetupProp = DetailBuilder.GetProperty( GET_MEMBER_NAME_CHECKED( UApparanceResourceListEntry_Component, ComponentSetup ), UApparanceResourceListEntry_Component::StaticClass() );
		DetailBuilder.HideProperty(SetupProp);
		const UClass* pcomponent_class = pcomponent->GetTemplateClass();
		if(pcomponent_class) //has a class set?
		{
			//lookup info about how we can setup the component
			const FApparanceComponentInfo* pcomponent_info = FApparanceComponentInventory::GetComponentInfo(pcomponent_class);

			//setup UI
			component_category.AddCustomRow( FText::FromString(TEXT("Component")) )
				.NameContent()
				[
					SNew( STextBlock )
					.Font(DefaultFont)
					.ToolTipText( SetupProp->GetToolTipText() )
					.Text( SetupProp->GetPropertyDisplayName() )
				]
				.ValueContent()
				.VAlign(VAlign_Center)
				.MaxDesiredWidth(ValueContentWidth)
				.MinDesiredWidth(ValueContentWidth)
				[
					SNew(SHorizontalBox)
					//count
					+SHorizontalBox::Slot()
					.Padding(FMargin(3,0))
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Fill)
					.FillWidth(1)
					[
						SNew( STextBlock )
						.Font(DefaultFont)
						.Text( FText::Format( LOCTEXT("ResourceListComponentActionCount","{0} action(s)"), pcomponent->ComponentSetup.Num() ) )
					]
					//add
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew( SComboButton )
						//.ComboButtonStyle(APPARANCE_STYLE, "ToolbarComboButton")
						//.ForegroundColor(FLinearColor::White)
						.ContentPadding(FMargin(5,1))
						.ButtonContent()
						[
							SNew(STextBlock)
							//.TextStyle(APPARANCE_STYLE, "Launcher.Filters.Text")
							.Text(LOCTEXT("ResourceListAddActionButton","+ Add Action"))
						]
						.MenuContent()
						[
							BuildAddSetupActionMenu( pcomponent_info, pcomponent )
						]
					]
				];
	
			for (int i = 0; i < pcomponent->ComponentSetup.Num(); i++)
			{
				BuildSetupActionRow(component_category, FText::FromString(TEXT("Setup Actions")), pcomponent, i);
			}
		}
	}
	
	//material (parameter binding)
	if(pmaterial)
	{
		IDetailCategoryBuilder& material_category = DetailBuilder.EditCategory( TEXT( "Material" ), LOCTEXT("ResourceListEntryMaterial","Material") );
		BuildDocumentationText(material_category, LOCTEXT("ResourceListMaterialHelp", "Materials can be procedurally parameterised by binding incoming parameters and textures to parameters exposed on the material."));

		//component setup
		TSharedRef<IPropertyHandle> BindingProp = DetailBuilder.GetProperty( GET_MEMBER_NAME_CHECKED( UApparanceResourceListEntry_Material, MaterialBindings ), UApparanceResourceListEntry_Material::StaticClass() );
		DetailBuilder.HideProperty(BindingProp);

		//setup UI
		material_category.AddCustomRow( FText::FromString(TEXT("Bindings")) )
			.NameContent()
			[
				SNew( STextBlock )
				.Font(DefaultFont)
				.ToolTipText( BindingProp->GetToolTipText() )
				.Text( BindingProp->GetPropertyDisplayName() )
			]
			.ValueContent()
			.VAlign(VAlign_Center)
			.MaxDesiredWidth(ValueContentWidthWide)
			.MinDesiredWidth(ValueContentWidthWide)
			[
				SNew(SHorizontalBox)
				//count
				+SHorizontalBox::Slot()
				.Padding(FMargin(3,0))
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Fill)
				.FillWidth(1)
				[
					SNew( STextBlock )
					.Font(DefaultFont)
					.Text( FText::Format( LOCTEXT("ResourceListMaterialBindingCount","{0} bindings(s)"), pmaterial->MaterialBindings.Num() ) )
				]
				//add
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew( SComboButton )
					.ContentPadding(FMargin(5,1))
					.ButtonContent()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ResourceListAddMaterialBindingButton","+ Add Binding"))
					]
					.MenuContent()
					[
						BuildAddMaterialBindingMenu( pmaterial )
					]
				]
			];
	
		for (int i = 0; i < pmaterial->MaterialBindings.Num(); i++)
		{
			BuildMaterialBindingRow(material_category, FText::FromString(TEXT("Material Parameter Binding")), pmaterial, i);
		}		
	}

	//transform
	if(p3dasset)
	{
		//translation
		TSharedRef<IPropertyHandle> TransformTranslationParamProp = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UApparanceResourceListEntry_3DAsset, PlacementOffsetParameter), UApparanceResourceListEntry_3DAsset::StaticClass());
		TSharedRef<IPropertyHandle> TransformCustomTranslationProp = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UApparanceResourceListEntry_3DAsset, CustomPlacementOffset), UApparanceResourceListEntry_3DAsset::StaticClass());
		//scale
		TSharedRef<IPropertyHandle> TransformSizeModeProp = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UApparanceResourceListEntry_3DAsset, PlacementSizeMode), UApparanceResourceListEntry_3DAsset::StaticClass());
		TSharedRef<IPropertyHandle> TransformScaleParamProp = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UApparanceResourceListEntry_3DAsset, PlacementScaleParameter), UApparanceResourceListEntry_3DAsset::StaticClass());
		TSharedRef<IPropertyHandle> TransformCustomScaleProp = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UApparanceResourceListEntry_3DAsset, CustomPlacementScale), UApparanceResourceListEntry_3DAsset::StaticClass());
		//rotation
		TSharedRef<IPropertyHandle> TransformRotationParamProp = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UApparanceResourceListEntry_3DAsset, PlacementRotationParameter), UApparanceResourceListEntry_3DAsset::StaticClass());
		TSharedRef<IPropertyHandle> TransformCustomRotationProp = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UApparanceResourceListEntry_3DAsset, CustomPlacementRotation), UApparanceResourceListEntry_3DAsset::StaticClass());
		
		IDetailCategoryBuilder& transform_category = DetailBuilder.EditCategory(TEXT("Transform"), LOCTEXT("ResourceListEntryPlacementTransformSection", "Transform"));
		BuildDocumentationText(transform_category, LOCTEXT("ResourceListTransformHelp", "Here you can choose how the placed asset offset, scale and rotation are obtained from the placement parameters."));
		
		//preset transform settings (for the below configuration)
		transform_category.AddCustomRow( FText::FromString(TEXT("Transform Presets")) )
			.NameContent()
			[
				SNew( STextBlock )
				.Font(DefaultFont)
				.ToolTipText( LOCTEXT("ResourceListTransformPresetTooltip","Pre-configured transform settings"))
				.Text( LOCTEXT( "ResourceListTransformPresetName", "Presets" ) )
			]
			.ValueContent()
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				//Full
				+SHorizontalBox::Slot()
				.Padding(FMargin(3,0))
				.AutoWidth()
				[
					SNew( SButton )
					.ButtonStyle( APPARANCE_STYLE, "FlatButton.DarkGrey")
					.ForegroundColor(FLinearColor::Gray)
					.ContentPadding(FMargin(5,1))
					.Text( LOCTEXT("ApparanceResourceTransformPresetFull","Full") )
					.ToolTipText( LOCTEXT("ApparanceResourceTransformPresetFullTooltip","Fully positioned, scaled and oriented. The asset completely fills the placement frame.") )
					.OnClicked(this, &FResourcesListEntryPropertyCustomisation::OnTransformPresetSelected, p3dasset, 2 )
				]
				//Non-scaled
				+SHorizontalBox::Slot()
				.Padding(FMargin(3,0))
				.AutoWidth()
				[
					SNew( SButton )
					.ButtonStyle( APPARANCE_STYLE, "FlatButton.DarkGrey")
					.ForegroundColor( FLinearColor::Gray )
					.ContentPadding(FMargin(5,1))
					.Text( LOCTEXT("ApparanceResourceTransformPresetUnscaled","Unscaled") )
					.ToolTipText( LOCTEXT("ApparanceResourceTransformPresetUnscaledTooltip","Positioned and oriented but left at original size. The asset is placed unchanged at the centre of the frame.") )
					.OnClicked(this, &FResourcesListEntryPropertyCustomisation::OnTransformPresetSelected, p3dasset, 1 )
				]
				//None
				+SHorizontalBox::Slot()
				.Padding(FMargin(3,0))
				.AutoWidth()
				[
					SNew( SButton )
					.ButtonStyle( APPARANCE_STYLE, "FlatButton.DarkGrey")
					.ForegroundColor( FLinearColor::Gray )
					.ContentPadding(FMargin(5,1))
					.Text( LOCTEXT("ApparanceResourceTransformPresetNone","None") )
					.ToolTipText( LOCTEXT("ApparanceResourceTransformPresetNoneTooltip","No transform applied.  The asset is left at the actor position.") )
					.OnClicked(this, &FResourcesListEntryPropertyCustomisation::OnTransformPresetSelected, p3dasset, 0 )
				]
			];

		//placement translation
		if(BuildTransformParameterSelect(transform_category, FText::FromString("Transform Parameter Offset"), passet, TransformTranslationParamProp, TransformCustomTranslationProp))
		{
			//hide custom if we are using parameter
			DetailBuilder.HideProperty( TransformCustomTranslationProp );
		}
		//placement scaling
		SizeModeEnumControl = MakeShared<FEnumCustomization>();
		SizeModeEnumControl->CreateEnumCustomization( transform_category, TransformSizeModeProp, StaticEnum<EApparanceSizeMode>(), LOCTEXT( "ResourceListEntrySizeModeName", "Size Mode" ) );
		if(BuildTransformParameterSelect( transform_category, FText::FromString( "Transform Parameter Scale" ), passet, TransformScaleParamProp, TransformCustomScaleProp ))
		{
			//hide custom if we are using parameter
			DetailBuilder.HideProperty( TransformCustomScaleProp );
		}
		//placement rotation
		if(BuildTransformParameterSelect(transform_category, FText::FromString("Transform Parameter Rotation"), passet, TransformRotationParamProp, TransformCustomRotationProp))
		{
			//hide custom if we are using parameter
			DetailBuilder.HideProperty( TransformCustomRotationProp );
		}

		//hide the param select props explicitly as we are replacing with custom UI
		DetailBuilder.HideProperty( TransformTranslationParamProp );
		DetailBuilder.HideProperty( TransformScaleParamProp );
		DetailBuilder.HideProperty( TransformRotationParamProp );
	}
}

// info row to explain the UI
//
void FResourcesListEntryPropertyCustomisation::BuildDocumentationText(IDetailCategoryBuilder& category, FText help_text)
{
	if(s_bInlineHelpEnabled)
	{
		category.AddCustomRow( FText::GetEmpty() )
			.WholeRowContent()
			[
				SNew( SBorder )
				.BorderImage( APPARANCE_STYLE.GetBrush( "ToolPanel.DarkGroupBorder" ) )
				.VAlign( EVerticalAlignment::VAlign_Top )
				.Padding( 5 )
				[
					SNew(SHorizontalBox)
					//icon
					+SHorizontalBox::Slot()
					.Padding( FMargin( 0, 0, 5, 0 ) )
					.AutoWidth()
					.VAlign( EVerticalAlignment::VAlign_Top )
					[
						SNew(SBox)
						.WidthOverride(32)
						.HeightOverride(32)
						[
							SNew( SImage ).Image( FApparanceUnrealEditorModule::GetStyle()->GetBrush( TEXT( "Apparance.Icon.Info" ) ) )
						]
					]			
					//text
					+SHorizontalBox::Slot()
					.FillWidth(1)
					[
						SNew( STextBlock )
						.Text( help_text )
						.Justification( ETextJustify::Left )
						.AutoWrapText( true )
					]
				]
			];
		}
}

// build descriptor for entry
//
FText FResourcesListEntryPropertyCustomisation::OnGetResourceDescriptor(UApparanceResourceListEntry* pentry) const
{
	return FText::FromString(pentry->BuildDescriptor());
}

// row for expected parameter list
//
void FResourcesListEntryPropertyCustomisation::BuildParameterRow( 
	IDetailCategoryBuilder& parameter_category, 
	FText filter_string,
	UApparanceResourceListEntry_Asset* passetentry,
	int parameter_index )
{
	int ilast = passetentry->ExpectedParameters.Num()-1;
	bool has_fixed_first = passetentry->IsPlaceable();
	bool is_first = parameter_index == (has_fixed_first?1:0);
	bool is_last = parameter_index == ilast;
	bool is_fixed_parameter = parameter_index==0 && has_fixed_first;
	bool is_editable_type = !is_fixed_parameter;
	const FApparancePlacementParameter* param_info = &passetentry->ExpectedParameters[parameter_index];
	const FText type_name = StaticEnum<EApparanceParameterType>()->GetDisplayNameTextByValue( (int64)param_info->Type );

	FDetailWidgetRow& row = parameter_category.AddCustomRow( filter_string )
		.NameContent()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text( type_name )
				.Font( DefaultFont )
			]
		];

	if(is_editable_type)
	{
		row.ValueContent()
		.MaxDesiredWidth(ValueContentWidth)
		.MinDesiredWidth(ValueContentWidth)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(SEditableTextBox)
				.Text( this, &FResourcesListEntryPropertyCustomisation::OnGetParameterNameText, passetentry, parameter_index )
				.OnTextCommitted( this, &FResourcesListEntryPropertyCustomisation::OnParameterNameTextCommitted, passetentry, parameter_index )
				.Font( DefaultFont )
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ContentPadding(0)
				.OnClicked(this, &FResourcesListEntryPropertyCustomisation::OnMoveParameterUp, passetentry, parameter_index)
				.IsEnabled(!is_first && is_editable_type)
				[
					SNew(SImage)
					.Image( APPARANCE_STYLE.GetBrush( FApparanceEditorBrushNames::UpArrow ))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ContentPadding(0)
				.OnClicked(this, &FResourcesListEntryPropertyCustomisation::OnMoveParameterDown, passetentry, parameter_index)
				.IsEnabled(!is_last)
				[
					SNew(SImage)
					.Image( APPARANCE_STYLE.GetBrush( FApparanceEditorBrushNames::DownArrow ))
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				PropertyCustomizationHelpers::MakeClearButton(
				FSimpleDelegate::CreateSP(this, &FResourcesListEntryPropertyCustomisation::OnRemoveParameter, passetentry, parameter_index),
				LOCTEXT("ResourceListRemoveParameter", "Remove parameter"),
				!is_fixed_parameter )
			]
		];
	}
	else
	{
		row.ValueContent()
		.MaxDesiredWidth(ValueContentWidth)
		.MinDesiredWidth(ValueContentWidth)
		.VAlign(VAlign_Center)
		[
			SNew(SEditableTextBox)
			.Text( this, &FResourcesListEntryPropertyCustomisation::OnGetParameterNameText, passetentry, 0 )
			.OnTextCommitted( this, &FResourcesListEntryPropertyCustomisation::OnParameterNameTextCommitted, passetentry, 0 )
			.Font( DefaultFont )
		];
	}
}

// menu to select parameter type from valid types
//
TSharedRef<class SWidget> FResourcesListEntryPropertyCustomisation::BuildParameterTypeSelectMenu( UApparanceResourceListEntry_Asset* passetentry )
{
	FMenuBuilder menu( true, nullptr );
	
	const UEnum* param_types = StaticEnum<EApparanceParameterType>();
	int num_types = param_types->NumEnums();
	if (param_types->ContainsExistingMax())
	{
		num_types--;
	}
	
	//types
	for (int i = 0; i < num_types; i++)
	{
		FText type_name = param_types->GetDisplayNameTextByIndex( i );
		EApparanceParameterType parameter_type = (EApparanceParameterType)param_types->GetValueByIndex(i);
		menu.AddMenuEntry( type_name, FText(), FSlateIcon(), 
			FUIAction( FExecuteAction::CreateRaw( 
				this, 
				&FResourcesListEntryPropertyCustomisation::OnAddParameter, 
				passetentry, 
				parameter_type 
			) )
		);
	}
	
	return menu.MakeWidget();	
}

//live summary of value
static FText GetActionSourceSummary(UApparanceResourceListEntry_Component* pcomponent, UApparanceComponentSetupAction* action)
{
	return FText::FromString( action->GetSourceSummary( pcomponent ) );
}


// row for expected texture list
//
void FResourcesListEntryPropertyCustomisation::BuildTextureRow( 
	IDetailCategoryBuilder& parameter_category, 
	FText filter_string,
	UApparanceResourceListEntry_Material* pmaterial,
	int texture_index )
{
	int ilast = pmaterial->ExpectedTextures.Num()-1;
	bool is_first = texture_index == 0;
	bool is_last = texture_index == ilast;
	const FApparancePlacementParameter* param_info = &pmaterial->ExpectedTextures[texture_index];

	FDetailWidgetRow& row = parameter_category.AddCustomRow( filter_string )
		.NameContent()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("ResourceListTextureHeader","Texture {0}"), texture_index+1 ))
				.Font( DefaultFont )
			]
		]
		.ValueContent()
		.MaxDesiredWidth(ValueContentWidth)
		.MinDesiredWidth(ValueContentWidth)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(SEditableTextBox)
				.Text( this, &FResourcesListEntryPropertyCustomisation::OnGetTextureNameText, pmaterial, texture_index )
				.OnTextCommitted( this, &FResourcesListEntryPropertyCustomisation::OnTextureNameTextCommitted, pmaterial, texture_index )
				.Font( DefaultFont )
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ContentPadding(0)
				.OnClicked(this, &FResourcesListEntryPropertyCustomisation::OnMoveTextureUp, pmaterial, texture_index)
				.IsEnabled(!is_first)
				[
					SNew(SImage)
					.Image( APPARANCE_STYLE.GetBrush( FApparanceEditorBrushNames::UpArrow ))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ContentPadding(0)
				.OnClicked(this, &FResourcesListEntryPropertyCustomisation::OnMoveTextureDown, pmaterial, texture_index)
				.IsEnabled(!is_last)
				[
					SNew(SImage)
					.Image( APPARANCE_STYLE.GetBrush( FApparanceEditorBrushNames::DownArrow ))
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				PropertyCustomizationHelpers::MakeClearButton(
				FSimpleDelegate::CreateSP(this, &FResourcesListEntryPropertyCustomisation::OnRemoveTexture, pmaterial, texture_index),
				LOCTEXT("ResourceListRemoveTexture", "Remove texture"),
				true )
			]
		];	
}

void FResourcesListEntryPropertyCustomisation::BuildMaterialBindingRow(
	IDetailCategoryBuilder& material_category, 
	FText filter_string,
	UApparanceResourceListEntry_Material* pmaterial,
	int binding_index)
{
	//gather
	FApparanceMaterialParameterBinding& binding = pmaterial->MaterialBindings[binding_index];
	const FMaterialParameterInfo* pinfo = pmaterial->FindMaterialParameter(binding.ParameterID, binding.ParameterName);
	UApparanceValueSource* psource = binding.Value;
	FText binding_desc = pinfo?FText::FromName(pinfo->Name):LOCTEXT("ResourceListMissingMaterialParameter","Unknown");
	bool is_first = binding_index == 0;
	bool is_last = binding_index == (pmaterial->MaterialBindings.Num()-1);

	//ui
	material_category.AddCustomRow(filter_string)
		.NameContent()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1)
			.VAlign(VAlign_Center)
			.Padding(FMargin(10,0,0,0)) //slight indent
			[
				SNew(STextBlock)
				.Text( binding_desc )
				.Font( DefaultFont )
			]
		]
		.ValueContent()
		.MaxDesiredWidth(ValueContentWidthWide)
		.MinDesiredWidth(ValueContentWidthWide)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				//select: parameter
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0,0,0,0)
				[
					SNew(SCheckBox)
					.Style( APPARANCE_STYLE, "Property.ToggleButton.Start")
					.IsChecked_Static( &OnIsSourceTypeChecked, psource, EApparanceValueSource::Parameter )
					.OnCheckStateChanged_Static( &OnSourceTypeChange, psource, EApparanceValueSource::Parameter )
					.ToolTipText( LOCTEXT("ResourceListValueSourceParameterTooltip","Use an incoming parameter to drive this value") )
					[
						SNew(STextBlock)
						.Text( LOCTEXT("ResourceListValueSourceParameter", "Param" ) )
						.Font(DefaultFont)
						.ColorAndOpacity_Static(&OnGetSourceTypeTextColour, psource, EApparanceValueSource::Parameter )
						.Margin(FMargin(3, 2))
					]
				]
				//select: constant
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0,0,4,0)
				[
					SNew(SCheckBox)
					.Style( APPARANCE_STYLE, "Property.ToggleButton.End")
					.IsChecked_Static( &OnIsSourceTypeChecked, psource, EApparanceValueSource::Constant )
					.OnCheckStateChanged_Static( &OnSourceTypeChange, psource, EApparanceValueSource::Constant )
					.ToolTipText( LOCTEXT("ResourceListValueSourceParameterTooltip","Use a constant to drive this value") )
					[
						SNew(STextBlock)
						.Text( LOCTEXT("ResourceListValueSourceConstant", "Const" ) )
						.Font(DefaultFont)
						.ColorAndOpacity_Static(&OnGetSourceTypeTextColour, psource, EApparanceValueSource::Constant )
						.Margin(FMargin(3, 2))
					]
				]
				//parameter display/select section
				+SHorizontalBox::Slot()
				.FillWidth(1)
				[
					SNew( SHorizontalBox)
					.Visibility_Static(&OnIsSourceTypeVisible, psource, EApparanceValueSource::Parameter )
					+SHorizontalBox::Slot()
					[
						SNew( SComboButton )
						//.ComboButtonStyle(APPARANCE_STYLE.Get(), "ToolbarComboButton")
						//.ForegroundColor(FLinearColor::White)
						.ContentPadding(FMargin(5,0))				
						.ButtonContent()
						[
							SNew(STextBlock)
							//.TextStyle(APPARANCE_STYLE.Get(), "Launcher.Filters.Text")
							.Text_Static( &OnGetValueSourceParameterText, (UApparanceResourceListEntry_Asset*)pmaterial, psource )
						]
						.MenuContent()
						[
							BuildSourceParameterSelectMenu( pmaterial, psource )
						]
					]
				]
				//constant display/edit section
				+SHorizontalBox::Slot()
				.FillWidth(1)
				[
					SNew(SHorizontalBox)
					.Visibility_Static(&OnIsSourceTypeVisible, psource, EApparanceValueSource::Constant )
					+SHorizontalBox::Slot()
					[
						BuildConstantValueEditControl( psource )
					]
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ContentPadding(0)
				.OnClicked(this, &FResourcesListEntryPropertyCustomisation::OnMoveMaterialBindingUp, pmaterial, binding_index)
				.IsEnabled(!is_first)
				[
					SNew(SImage)
					.Image( APPARANCE_STYLE.GetBrush( FApparanceEditorBrushNames::UpArrow ))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ContentPadding(0)
				.OnClicked(this, &FResourcesListEntryPropertyCustomisation::OnMoveMaterialBindingDown, pmaterial, binding_index)
				.IsEnabled(!is_last)
				[
					SNew(SImage)
					.Image( APPARANCE_STYLE.GetBrush(FApparanceEditorBrushNames::DownArrow))
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				PropertyCustomizationHelpers::MakeClearButton(
				FSimpleDelegate::CreateSP(this, &FResourcesListEntryPropertyCustomisation::OnRemoveMaterialBinding, pmaterial, binding_index),
				LOCTEXT("ResourceListRemoveMaterialBinding", "Remove binding"),
				true )
			]
		];
}


// custom builder so we can have collapsible action parameters (fn args)
//
class FComponentSetupActionBuilder : public IDetailCustomNodeBuilder, public TSharedFromThis<FComponentSetupActionBuilder>
{
	UApparanceResourceListEntry_Component* ComponentEntry;
	FResourcesListEntryPropertyCustomisation* PropertyCustomisation;
	int ActionIndex;

public:
	FComponentSetupActionBuilder(FResourcesListEntryPropertyCustomisation* prop_cust, UApparanceResourceListEntry_Component* pcomponent, int action_index)
	{
		PropertyCustomisation = prop_cust;
		ComponentEntry = pcomponent;
		ActionIndex = action_index;
	}
	
	/** IDetailCustomNodeBuilder interface */
	virtual void SetOnRebuildChildren( FSimpleDelegate InOnRebuildChildren  ) override { } 
	virtual bool RequiresTick() const override { return false; }
	virtual void Tick(float DeltaTime) override {}
	virtual bool InitiallyCollapsed() const override { return true; };
	virtual FName GetName() const override { return FName(TEXT("SetupAction")); }
	virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow) override
	{
		UApparanceComponentSetupAction* action = ComponentEntry->ComponentSetup[ActionIndex];
		const FText action_desc = FText::FromString(action->GetDescription());
		const FText action_target = FText::FromString(action->GetTargetName());
		int ilast = ComponentEntry->ComponentSetup.Num()-1;
		bool is_first = ActionIndex == 0;
		bool is_last = ActionIndex == ilast;
		
		TSharedPtr<SHorizontalBox> value_box;
		NodeRow.NameContent()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.FillWidth(1)
				.VAlign(VAlign_Center)
				.Padding(FMargin(10,0,0,0)) //slight indent
				[
					SNew(STextBlock)
					.Text( action_desc )
					.Font( PropertyCustomisation->DefaultFont )
				]
			]
			.ValueContent()
			.MaxDesiredWidth(ValueContentWidthWide)
			.MinDesiredWidth(ValueContentWidthWide)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SAssignNew(value_box,SHorizontalBox)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ContentPadding(0)
					.OnClicked(PropertyCustomisation, &FResourcesListEntryPropertyCustomisation::OnMoveSetupActionUp, ComponentEntry, ActionIndex)
					.IsEnabled(!is_first)
					[
						SNew(SImage)
						.Image( APPARANCE_STYLE.GetBrush( FApparanceEditorBrushNames::UpArrow ))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ContentPadding(0)
					.OnClicked(PropertyCustomisation, &FResourcesListEntryPropertyCustomisation::OnMoveSetupActionDown, ComponentEntry, ActionIndex)
					.IsEnabled(!is_last)
					[
						SNew(SImage)
						.Image( APPARANCE_STYLE.GetBrush(FApparanceEditorBrushNames::DownArrow))
					]
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				[
					PropertyCustomizationHelpers::MakeClearButton(
					FSimpleDelegate::CreateSP(PropertyCustomisation, &FResourcesListEntryPropertyCustomisation::OnRemoveSetupAction, ComponentEntry, ActionIndex),
					LOCTEXT("ResourceListRemoveParameter", "Remove parameter"),
					true )
				]
			];

		//value
		if (action->SourcesAsChildren())
		{
			//summary 
			value_box->AddSlot()
			[
				SNew(STextBlock)
				.Font(PropertyCustomisation->DefaultFont)
				.Text_Static( &GetActionSourceSummary, ComponentEntry, action )
			];
		}
		else
		{
			//first source value only (i.e. for property setter)
#if 0
			//with cute '=' prefix
			value_box->AddSlot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(STextBlock)
			.Text(FText::FromString(TEXT("=")))
			];
#endif
			value_box->AddSlot()
			[
				PropertyCustomisation->BuildValueSourceEditor(ComponentEntry, action, 0)
			];
		}
	}
	virtual void GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder) override
	{
		UApparanceComponentSetupAction* action = ComponentEntry->ComponentSetup[ActionIndex];

		//child info
		if (action->SourcesAsChildren())
		{
			for (int i = 0; i < action->GetSourceCount(); i++)
			{			
				ChildrenBuilder.AddCustomRow(FText::FromString(TEXT("Row")))
				.NameContent()
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.FillWidth(1)
					.VAlign(VAlign_Center)
					.Padding(FMargin(10,0,0,0)) //slight indent
					[
						SNew(STextBlock)
						.Text( FText::FromString( action->DescribeSource( i ) ) )
						.Font( PropertyCustomisation->DefaultFont )
					]
				]
				.ValueContent()
				.MaxDesiredWidth(ValueContentWidthTweak)
				.MinDesiredWidth(ValueContentWidthTweak)
				[
					PropertyCustomisation->BuildValueSourceEditor( ComponentEntry, action, i )
				];
			}
		}
	}
};
void FResourcesListEntryPropertyCustomisation::BuildSetupActionRow(
	IDetailCategoryBuilder& component_category,
	FText filter_string,
	UApparanceResourceListEntry_Component* pcomponent,
	int action_index)
{
	const TSharedRef<FComponentSetupActionBuilder> ActionBuilder = MakeShared<FComponentSetupActionBuilder>( this, pcomponent, action_index );
	component_category.AddCustomBuilder(ActionBuilder);
}

// vector editing helpers
//
static float GetVectorComponent( FVector vector, int axis )
{
	switch(axis)
	{
		case 0: return vector.X;
		case 1: return vector.Y;
		case 2: return vector.Z;
		default: return 0;
	}
}
static void SetVectorComponent( FVector& vector, float value, int axis )
{
	switch(axis)
	{
		case 0: vector.X = value; break;
		case 1: vector.Y = value; break;
		case 2: vector.Z = value; break;
		default: break;
	}
}
static float GetVectorParameterComponent( UApparanceValueConstant_Vector* pvector, int axis )
{
	return GetVectorComponent( pvector->Value, axis );
}
static void SetVectorParameterComponent( float new_value, UApparanceValueConstant_Vector* pvector, int axis, bool editing_transaction )
{
	if(editing_transaction)
	{
		GEditor->BeginTransaction( LOCTEXT("ParameterEditVector", "Edit Vector Parameter") );
		pvector->Modify();
	}

	//apply
	SetVectorComponent( pvector->Value, new_value, axis );

	if(editing_transaction)
	{
		GEditor->EndTransaction();

		//explicit notify as not a property edit in the conventional sense
		NotifyPropertyChanged( pvector );
	}
}
static TOptional<float> CurrentVectorParameterComponent( UApparanceValueConstant_Vector* pvector, int axis ) { return GetVectorParameterComponent( pvector, axis ); }
static void ChangeVectorParameterComponent( float new_value, UApparanceValueConstant_Vector* pvector, int axis ) { SetVectorParameterComponent( new_value, pvector, axis, false ); }
static void CommitVectorParameterComponent( float new_value, ETextCommit::Type commit_info, UApparanceValueConstant_Vector* pvector, int axis ) { SetVectorParameterComponent( new_value, pvector, axis, true ); }	

// colour editing helpers
//
static FLinearColor GetColourParameter(UApparanceValueConstant_Colour* psource_colour)
{
	return psource_colour->Value;
}
void SetColourParameter(FLinearColor new_value, UApparanceValueConstant_Colour* psource_colour, bool editing_transaction)
{
	if(editing_transaction)
	{
		GEditor->BeginTransaction( LOCTEXT("ParameterEditColour", "Edit Colour Parameter") );
		psource_colour->Modify();
	}
	
	//apply
	psource_colour->Value = new_value;
	
	if(editing_transaction)
	{
		GEditor->EndTransaction();

		//explicit notify as not a property edit in the conventional sense
		NotifyPropertyChanged( psource_colour );
		FPropertyChangedEvent e(nullptr);
		psource_colour->PostEditChangeProperty(e);	
	}	
}
static FLinearColor CurrentColourParameter( UApparanceValueConstant_Colour* psource_colour ) { return GetColourParameter( psource_colour ); }
static FLinearColor LastColourValue;
static bool bColourPickingInteractive = false;
static void OnColorPickerInteractiveBegin()
{
	bColourPickingInteractive = true; 
}
static void OnColorPickerInteractiveEnd(UApparanceValueConstant_Colour* psource_colour)
{
	bColourPickingInteractive = false; 
	
	//final value apply
	SetColourParameter( LastColourValue, psource_colour, true ); //transaction
}
static void OnSetColorFromColorPicker(FLinearColor NewColor, UApparanceValueConstant_Colour* psource_colour)
{
	LastColourValue = NewColor;
	SetColourParameter( NewColor, psource_colour, !bColourPickingInteractive ); //transaction
}
static FReply OnClickColorBlock(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, UApparanceValueConstant_Colour* psource_colour)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}
	
	FColorPickerArgs PickerArgs;
	{
		PickerArgs.bOnlyRefreshOnOk = false;
		PickerArgs.DisplayGamma = TAttribute<float>::Create(TAttribute<float>::FGetter::CreateUObject(GEngine, &UEngine::GetDisplayGamma));
		PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateStatic(&OnSetColorFromColorPicker, psource_colour);
		PickerArgs.OnInteractivePickBegin = FSimpleDelegate::CreateStatic( &OnColorPickerInteractiveBegin );
		PickerArgs.OnInteractivePickEnd = FSimpleDelegate::CreateStatic( &OnColorPickerInteractiveEnd, psource_colour );
#if UE_VERSION_OLDER_THAN(5,2,0)
		PickerArgs.InitialColorOverride = GetColourParameter( psource_colour );
#else //5.2+
		PickerArgs.InitialColor = GetColourParameter( psource_colour );
#endif
	}
	
	OpenColorPicker(PickerArgs);
	
	return FReply::Handled();
}

// creation of a general property editor control for constant values
//
TSharedRef<class SWidget> FResourcesListEntryPropertyCustomisation::BuildConstantValueEditControl(UApparanceValueSource* psource)
{
	if (psource && psource->Constant)
	{
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		FSinglePropertyParams SinglePropertyArgs;
		SinglePropertyArgs.NamePlacement = EPropertyNamePlacement::Hidden; //just want the edit control
		
		//simple or complex editing UI?
		const UClass* source_class = psource->Constant->GetClass();
		if(source_class==UApparanceValueConstant_Vector::StaticClass())
		{
			//vectors
			UApparanceValueConstant_Vector* psource_vector = Cast<UApparanceValueConstant_Vector>(psource->Constant);
			return SNew( SVectorInputBox )
				.X_Static( &CurrentVectorParameterComponent, psource_vector, 0 )
				.Y_Static( &CurrentVectorParameterComponent, psource_vector, 1 )
				.Z_Static( &CurrentVectorParameterComponent, psource_vector, 2 )
				.bColorAxisLabels( true )
#if UE_VERSION_OLDER_THAN(5,0,0)
				.AllowResponsiveLayout( true )
#endif
				//.IsEnabled( this, &FComponentTransformDetails::GetIsEnabled )
				.OnXChanged_Static( &ChangeVectorParameterComponent, psource_vector, 0 )
				.OnYChanged_Static( &ChangeVectorParameterComponent, psource_vector, 1 )
				.OnZChanged_Static( &ChangeVectorParameterComponent, psource_vector, 2 )
				.OnXCommitted_Static( &CommitVectorParameterComponent, psource_vector, 0 )
				.OnYCommitted_Static( &CommitVectorParameterComponent, psource_vector, 1 )
				.OnZCommitted_Static( &CommitVectorParameterComponent, psource_vector, 2 )
				//.Font( FontInfo )
				.AllowSpin( true )
				//.SpinDelta( spin_speed )
				//.OnBeginSliderMovement( this, &FComponentTransformDetails::OnBeginScaleSlider )
				//.OnEndSliderMovement( this, &FComponentTransformDetails::OnEndScaleSlider )
				//.IsEnabled( this, &FEntityPropertyCustomisation::CheckParameterEditable, pentity, input_id )
			;
		}
		else if (source_class == UApparanceValueConstant_Colour::StaticClass())
		{
			//colours
			UApparanceValueConstant_Colour* psource_colour = Cast<UApparanceValueConstant_Colour>(psource->Constant);
			return SNew( SColorBlock )
				.Color_Static(&CurrentColourParameter, psource_colour )
				.ShowBackgroundForAlpha(true)
#if UE_VERSION_OLDER_THAN(5,0,0)
				.IgnoreAlpha(false)
#else
				.AlphaDisplayMode( EColorBlockAlphaDisplayMode::Ignore )
#endif
				.OnMouseButtonDown_Static(&OnClickColorBlock, psource_colour )
				//.IsEnabled( this, &FEntityPropertyCustomisation::CheckParameterEditable, pentity, input_id )
				//.Size(FVector2D(EditColorWidth, EditColorHeight))
			;
		}
		else
		{
			//fallback to default property edit control
			TSharedPtr<ISinglePropertyView> prop_view = PropertyEditorModule.CreateSingleProperty( psource->Constant, FName( "Value" ), SinglePropertyArgs );
			if (prop_view.IsValid())
			{
				return prop_view.ToSharedRef();
			}
		}		
	}
	return SNullWidget::NullWidget;
}

// build source editor UI
//
TSharedRef<class SWidget> FResourcesListEntryPropertyCustomisation::BuildValueSourceEditor(UApparanceResourceListEntry_Component* pcomponent, UApparanceComponentSetupAction* paction, int source_index)
{
	UApparanceValueSource* psource = paction->GetSource(source_index);

	return SNew(SHorizontalBox)
		//select: parameter
		+SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0,0,0,0)
		[
			SNew(SCheckBox)
			.Style( APPARANCE_STYLE, "Property.ToggleButton.Start")
			.IsChecked_Static( &OnIsSourceTypeChecked, psource, EApparanceValueSource::Parameter )
			.OnCheckStateChanged_Static( &OnSourceTypeChange, psource, EApparanceValueSource::Parameter )
			.ToolTipText( LOCTEXT("ResourceListValueSourceParameterTooltip","Use an incoming parameter to drive this value") )
			[
				SNew(STextBlock)
				.Text( LOCTEXT("ResourceListValueSourceParameter", "Param" ) )
				.Font(DefaultFont)
				.ColorAndOpacity_Static(&OnGetSourceTypeTextColour, psource, EApparanceValueSource::Parameter )
				.Margin(FMargin(3, 2))
			]
		]
		//select: constant
		+SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0,0,4,0)
		[
			SNew(SCheckBox)
			.Style( APPARANCE_STYLE, "Property.ToggleButton.End")
			.IsChecked_Static( &OnIsSourceTypeChecked, psource, EApparanceValueSource::Constant )
			.OnCheckStateChanged_Static( &OnSourceTypeChange, psource, EApparanceValueSource::Constant )
			.ToolTipText( LOCTEXT("ResourceListValueSourceParameterTooltip","Use a constant to drive this value") )
			[
				SNew(STextBlock)
				.Text( LOCTEXT("ResourceListValueSourceConstant", "Const" ) )
				.Font(DefaultFont)
				.ColorAndOpacity_Static(&OnGetSourceTypeTextColour, psource, EApparanceValueSource::Constant )
				.Margin(FMargin(3, 2))
			]
		]
		//parameter display/select section
		+SHorizontalBox::Slot()
		.FillWidth(1)
		[
			SNew( SHorizontalBox)
			.Visibility_Static(&OnIsSourceTypeVisible, psource, EApparanceValueSource::Parameter )
			+SHorizontalBox::Slot()
			[
				SNew( SComboButton )
				//.ComboButtonStyle(APPARANCE_STYLE, "ToolbarComboButton")
				//.ForegroundColor(FLinearColor::White)
				.ContentPadding(FMargin(5,0))				
				.ButtonContent()
				[
					SNew(STextBlock)
					//.TextStyle(APPARANCE_STYLE, "Launcher.Filters.Text")
					.Text_Static( &OnGetValueSourceParameterText, (UApparanceResourceListEntry_Asset*)pcomponent, psource )
				]
				.MenuContent()
				[
					BuildSourceParameterSelectMenu( pcomponent, psource )
				]
			]
		]
		//constant display/edit section
		+SHorizontalBox::Slot()
		.FillWidth(1)
		[
			SNew(SHorizontalBox)
			.Visibility_Static(&OnIsSourceTypeVisible, psource, EApparanceValueSource::Constant )
			+SHorizontalBox::Slot()
			[
				BuildConstantValueEditControl( psource )
			]
		]
	;		
}

// menu to select setup action from list available
//
TSharedRef<class SWidget> FResourcesListEntryPropertyCustomisation::BuildAddSetupActionMenu( const class FApparanceComponentInfo* pcomponent_info, UApparanceResourceListEntry_Component* pcomponent )
{
	FMenuBuilder menu( true, nullptr );
	
	//properties
	if (pcomponent_info->Properties.Num() > 0)
	{
		menu.BeginSection(NAME_None, LOCTEXT("ResourceListSetPropertyMenuSection", "Set Property"));
		for (int i = 0; i < pcomponent_info->Properties.Num(); i++)
		{
			const FApparancePropertyInfo* prop_info = pcomponent_info->Properties[i];
			menu.AddMenuEntry(FText::FromName(prop_info->Name), prop_info->Description, FSlateIcon(), FUIAction(FExecuteAction::CreateRaw(this, &FResourcesListEntryPropertyCustomisation::OnAddSetupAction_Property, pcomponent, prop_info)));
		}
		menu.EndSection();
	}

	//functions
	if (pcomponent_info->Methods.Num() > 0)
	{
		menu.BeginSection(NAME_None, LOCTEXT("ResourceListCallMethodMenuSection", "Call Function"));
		for (int i = 0; i < pcomponent_info->Methods.Num(); i++)
		{
			const FApparanceMethodInfo* method_info = pcomponent_info->Methods[i];
			menu.AddMenuEntry(FText::FromName(method_info->Name), method_info->Description, FSlateIcon(), FUIAction(FExecuteAction::CreateRaw(this, &FResourcesListEntryPropertyCustomisation::OnAddSetupAction_Method, pcomponent, method_info)));
		}
		menu.EndSection();
	}

	return menu.MakeWidget();
}

// menu to select new material parameter to bind
//
TSharedRef<class SWidget> FResourcesListEntryPropertyCustomisation::BuildAddMaterialBindingMenu(class UApparanceResourceListEntry_Material* pmaterial)
{
	FMenuBuilder menu( true, nullptr );

	//sync all
	menu.AddMenuEntry(LOCTEXT("ResourceListSyncMaterialParametersMenuItem","Sync All Parameters"), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateRaw(this, &FResourcesListEntryPropertyCustomisation::OnSyncMaterialBinding, pmaterial)));
		
	//scalars
	if (pmaterial->ScalarParameters.Num() > 0)
	{
		menu.BeginSection(NAME_None, LOCTEXT("ResourceListScalarParameterMenuSection", "Float/Scalar Parameters"));
		for (int i = 0; i < pmaterial->ScalarParameters.Num(); i++)
		{
			const FMaterialParameterInfo& mpi = pmaterial->ScalarParameters[i];
			const FGuid& guid = pmaterial->ScalarParameterIDs[i];		
			menu.AddMenuEntry(FText::FromName(mpi.Name), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateRaw(this, &FResourcesListEntryPropertyCustomisation::OnAddMaterialBinding, pmaterial, guid)));
		}
		menu.EndSection();
	}
	//vectors
	if (pmaterial->VectorParameters.Num() > 0)
	{
		menu.BeginSection(NAME_None, LOCTEXT("ResourceListVectorParameterMenuSection", "Colour/Vector Parameters"));
		for (int i = 0; i < pmaterial->VectorParameters.Num(); i++)
		{
			const FMaterialParameterInfo& mpi = pmaterial->VectorParameters[i];
			const FGuid& guid = pmaterial->VectorParameterIDs[i];		
			menu.AddMenuEntry(FText::FromName(mpi.Name), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateRaw(this, &FResourcesListEntryPropertyCustomisation::OnAddMaterialBinding, pmaterial, guid)));
		}
		menu.EndSection();
	}
	//textures
	if (pmaterial->TextureParameters.Num() > 0)
	{
		menu.BeginSection(NAME_None, LOCTEXT("ResourceListTextureParameterMenuSection", "Texture Parameters"));
		for (int i = 0; i < pmaterial->TextureParameters.Num(); i++)
		{
			const FMaterialParameterInfo& mpi = pmaterial->TextureParameters[i];
			const FGuid& guid = pmaterial->TextureParameterIDs[i];		
			menu.AddMenuEntry(FText::FromName(mpi.Name), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateRaw(this, &FResourcesListEntryPropertyCustomisation::OnAddMaterialBinding, pmaterial, guid)));
		}
		menu.EndSection();
	}
	
	return menu.MakeWidget();
}


// apply a transform preset
FReply FResourcesListEntryPropertyCustomisation::OnTransformPresetSelected( UApparanceResourceListEntry_3DAsset* p3dasset, int preset_index )
{
	const FApparancePlacementParameter* frame_param = p3dasset->FindFirstFrameParameterInfo();
	int frame_param_id = frame_param?frame_param->ID:0;
	
	BeginEdit(p3dasset);
	switch (preset_index)
	{
		case 0: //None
			p3dasset->PlacementOffsetParameter = 0;
			p3dasset->CustomPlacementOffset = FVector(0, 0, 0);
			p3dasset->PlacementSizeMode = EApparanceSizeMode::Scale;
			p3dasset->PlacementScaleParameter = 0;
			p3dasset->CustomPlacementScale = FVector(1, 1, 1);
			p3dasset->PlacementRotationParameter = 0;
			p3dasset->CustomPlacementRotation = FVector(0, 0, 0);
			break;
		case 1: //Unscaled
			if (frame_param_id != 0)
			{
				p3dasset->PlacementOffsetParameter = frame_param_id;
				p3dasset->PlacementScaleParameter = 0;
				p3dasset->PlacementSizeMode = EApparanceSizeMode::Scale;
				p3dasset->CustomPlacementScale = FVector(1, 1, 1);
				p3dasset->PlacementRotationParameter = frame_param_id;
			}
			break;
		case 2: //Full
			if (frame_param_id != 0)
			{
				p3dasset->PlacementOffsetParameter = frame_param_id;
				p3dasset->PlacementSizeMode = EApparanceSizeMode::Size;
				p3dasset->PlacementScaleParameter = frame_param_id;
				p3dasset->PlacementRotationParameter = frame_param_id;
			}
			break;
	}
	EndEdit(p3dasset);
	Rebuild();
	NotifyPropertyChanged(p3dasset);	//need explicit notification
	return FReply::Handled();
}

// build UI for selecting transform parameter to use
// used for translation, scale, and rotation
//
bool FResourcesListEntryPropertyCustomisation::BuildTransformParameterSelect(
	IDetailCategoryBuilder& parameter_category, 
	FText filter_string,
	UApparanceResourceListEntry_Asset* passetentry,
	TSharedPtr<IPropertyHandle> id_property,
	TSharedPtr<IPropertyHandle> custom_value_property
)
{
	//current value
	int current_param_id = 0;
	id_property->GetValue( current_param_id );
	const FApparancePlacementParameter* param_info = passetentry->FindParameterInfo( current_param_id);
	bool is_param_used = param_info != nullptr;
	FText param_name = param_info?FText::FromString(param_info->Name):LOCTEXT("ApparanceParameterNone","None");
	FText prop_tooltip = id_property->GetToolTipText();
	FText prop_name = id_property->GetPropertyDisplayName();
	parameter_category.AddCustomRow(filter_string)
		.NameContent()
		[
			SNew( STextBlock )
			.Font(DefaultFont)
			.ToolTipText( prop_tooltip )
			.Text( prop_name )
		]
		.ValueContent()
		.MaxDesiredWidth(ValueContentWidth)
		.MinDesiredWidth(ValueContentWidth)
		.VAlign(VAlign_Center)
		[
			SNew( SComboButton )
			//.ComboButtonStyle(APPARANCE_STYLE, "ToolbarComboButton")
			//.ForegroundColor(FLinearColor::White)
			.ContentPadding(FMargin(5,1))
			.ButtonContent()
			[
				SNew(STextBlock)
				//.TextStyle(APPARANCE_STYLE, "Launcher.Filters.Text")
				.Text( param_name )
			]
			.MenuContent()
			[
				BuildParameterSelectMenu(passetentry, id_property)
			]
		];

	//custom value edit
	if (!is_param_used)
	{
		parameter_category.AddProperty( custom_value_property );
	}

	return is_param_used;
}

// helper to act on parameter choice menu item selection
//
static void ParamaterPropertySelect( TSharedPtr<IPropertyHandle> proc_property, int32 id )
{
	proc_property->SetValue( id );
}

// menu to select parameter id from the expected parameter list
//
TSharedRef<class SWidget> FResourcesListEntryPropertyCustomisation::BuildParameterSelectMenu( UApparanceResourceListEntry_Asset* passetentry, TSharedPtr<IPropertyHandle> id_property )
{
	FMenuBuilder menu( true, nullptr );
	
	//none
	menu.AddMenuEntry( LOCTEXT("ParameterMenuNone","None"), FText(), FSlateIcon(), FUIAction( FExecuteAction::CreateStatic( &ParamaterPropertySelect, id_property, (int)0 ) ) );
	
	//current value
	int current_param_id = 0;
	id_property->GetValue( current_param_id );
	
	//parameters
	for (int i = 0; i < passetentry->ExpectedParameters.Num(); i++)
	{
		FApparancePlacementParameter& placement_param = passetentry->ExpectedParameters[i];
		menu.AddMenuEntry( FText::FromString( placement_param.Name ), FText(), FSlateIcon(), FUIAction( FExecuteAction::CreateStatic( &ParamaterPropertySelect, id_property, (int)placement_param.ID ) ) );
	}
	
	return menu.MakeWidget();	
}

// handle property changes that affect UI layout
//
void FResourcesListEntryPropertyCustomisation::OnPropertyChanged( const FPropertyChangedEvent& e )
{
	FName prop_name = e.GetPropertyName();
	FName struct_name = NAME_None;
	if(e.MemberProperty)
	{
		struct_name = e.MemberProperty->GetFName();
	}

	if(prop_name == GET_MEMBER_NAME_CHECKED( UApparanceResourceListEntry_Component, TemplateType )
		|| prop_name == GET_MEMBER_NAME_CHECKED( UApparanceResourceListEntry_3DAsset, bFixPlacement )
		|| prop_name == GET_MEMBER_NAME_CHECKED( UApparanceResourceListEntry_3DAsset, PlacementOffsetParameter )
		|| prop_name == GET_MEMBER_NAME_CHECKED( UApparanceResourceListEntry_3DAsset, PlacementScaleParameter )
		|| prop_name == GET_MEMBER_NAME_CHECKED( UApparanceResourceListEntry_3DAsset, PlacementRotationParameter )
		|| prop_name == GET_MEMBER_NAME_CHECKED( UApparanceResourceListEntry_3DAsset, BoundsSource )
	)
	{
		Rebuild();
	}

	//changes within complex member properties
	if(struct_name == GET_MEMBER_NAME_CHECKED( UApparanceResourceListEntry_Asset, ExpectedParameters )
		&& prop_name != GET_MEMBER_NAME_CHECKED( FApparancePlacementParameter, Type ))	//don't respond Type as it changes as soon as you go to open the combo (grrrr)
	{
		Rebuild();
	}

}

// need to update UI layout/structure
//
void FResourcesListEntryPropertyCustomisation::Rebuild()
{
	DetailBuilderPtr->ForceRefreshDetails();
}


//parameter list editing
FText FResourcesListEntryPropertyCustomisation::OnGetParameterNameText(UApparanceResourceListEntry_Asset* passetentry, int parameter_index) const
{
	return FText::FromString(passetentry->ExpectedParameters[parameter_index].Name);
}
void FResourcesListEntryPropertyCustomisation::OnParameterNameTextCommitted(const FText& new_name, ETextCommit::Type operation, UApparanceResourceListEntry_Asset* passetentry, int parameter_index)
{
	BeginEdit(passetentry);
	passetentry->ExpectedParameters[parameter_index].Name = new_name.ToString();
	EndEdit(passetentry);
	Rebuild();
}
void FResourcesListEntryPropertyCustomisation::OnAddParameter(UApparanceResourceListEntry_Asset* passetentry, EApparanceParameterType parameter_type)
{
	BeginEdit(passetentry);
	FApparancePlacementParameter new_param;
	new_param.Type = parameter_type;
	new_param.Name = TEXT("Parameter");
	passetentry->ExpectedParameters.Add(new_param);
	passetentry->CheckParameterList();
	EndEdit( passetentry );
	Rebuild();
}
void FResourcesListEntryPropertyCustomisation::OnRemoveParameter(UApparanceResourceListEntry_Asset* passetentry, int parameter_index)
{
	BeginEdit(passetentry);
	passetentry->ExpectedParameters.RemoveAt(parameter_index);
	EndEdit( passetentry );
	Rebuild();
}
FReply FResourcesListEntryPropertyCustomisation::OnMoveParameterDown(UApparanceResourceListEntry_Asset* passetentry, int parameter_index)
{
	BeginEdit(passetentry);
	auto moving = passetentry->ExpectedParameters[parameter_index];
	passetentry->ExpectedParameters.RemoveAt(parameter_index);
	passetentry->ExpectedParameters.Insert( moving, parameter_index+1 );
	EndEdit( passetentry );
	//explicit notify as not a property edit in the conventional sense
	NotifyPropertyChanged(passetentry);
	Rebuild();
	return FReply::Handled();
}
FReply FResourcesListEntryPropertyCustomisation::OnMoveParameterUp(UApparanceResourceListEntry_Asset* passetentry, int parameter_index)
{
	BeginEdit(passetentry);
	auto moving = passetentry->ExpectedParameters[parameter_index];
	passetentry->ExpectedParameters.RemoveAt(parameter_index);
	passetentry->ExpectedParameters.Insert( moving, parameter_index-1 );
	EndEdit( passetentry );
	//explicit notify as not a property edit in the conventional sense
	NotifyPropertyChanged(passetentry);
	Rebuild();
	return FReply::Handled();
}


//material texture list editing
FText FResourcesListEntryPropertyCustomisation::OnGetTextureNameText(UApparanceResourceListEntry_Material* pmaterial, int texture_index) const
{
	return FText::FromString(pmaterial->ExpectedTextures[texture_index].Name);
}
void FResourcesListEntryPropertyCustomisation::OnTextureNameTextCommitted(const FText& new_name, ETextCommit::Type operation, UApparanceResourceListEntry_Material* pmaterial, int texture_index)
{
	BeginEdit(pmaterial);
	pmaterial->ExpectedTextures[texture_index].Name = new_name.ToString();
	EndEdit(pmaterial);
	Rebuild();
}
FReply FResourcesListEntryPropertyCustomisation::OnAddTexture(UApparanceResourceListEntry_Material* pmaterial)
{
	BeginEdit(pmaterial);
	FApparancePlacementParameter new_param;
	new_param.Type = EApparanceParameterType::Texture;
	new_param.Name = TEXT("Texture");
	pmaterial->ExpectedTextures.Add(new_param);
	pmaterial->CheckParameterList();
	EndEdit( pmaterial );
	Rebuild();
	return FReply::Handled();
}
void FResourcesListEntryPropertyCustomisation::OnRemoveTexture(UApparanceResourceListEntry_Material* pmaterial, int texture_index)
{
	BeginEdit(pmaterial);
	pmaterial->ExpectedTextures.RemoveAt(texture_index);
	EndEdit( pmaterial );
	Rebuild();
}
FReply FResourcesListEntryPropertyCustomisation::OnMoveTextureDown(UApparanceResourceListEntry_Material* pmaterial, int texture_index)
{
	BeginEdit(pmaterial);
	auto moving = pmaterial->ExpectedTextures[texture_index];
	pmaterial->ExpectedTextures.RemoveAt(texture_index);
	pmaterial->ExpectedTextures.Insert( moving, texture_index+1 );
	EndEdit( pmaterial );
	//explicit notify as not a property edit in the conventional sense
	NotifyPropertyChanged(pmaterial);
	Rebuild();
	return FReply::Handled();
}
FReply FResourcesListEntryPropertyCustomisation::OnMoveTextureUp(UApparanceResourceListEntry_Material* pmaterial, int texture_index)
{
	BeginEdit(pmaterial);
	auto moving = pmaterial->ExpectedTextures[texture_index];
	pmaterial->ExpectedTextures.RemoveAt(texture_index);
	pmaterial->ExpectedTextures.Insert( moving, texture_index-1 );
	EndEdit( pmaterial );
	//explicit notify as not a property edit in the conventional sense
	NotifyPropertyChanged(pmaterial);
	Rebuild();
	return FReply::Handled();
}

//component setup action list editing
void FResourcesListEntryPropertyCustomisation::OnAddSetupAction_Property(class UApparanceResourceListEntry_Component* pcomponent, const FApparancePropertyInfo* prop_info)
{	
	pcomponent->AddAction_SetProperty( prop_info );
	//explicit notify as not a property edit in the conventional sense
	NotifyPropertyChanged( pcomponent );
	Rebuild();
}
void FResourcesListEntryPropertyCustomisation::OnAddSetupAction_Method(class UApparanceResourceListEntry_Component* pcomponent, const FApparanceMethodInfo* method_info)
{
	pcomponent->AddAction_CallMethod(method_info);
	//explicit notify as not a property edit in the conventional sense
	NotifyPropertyChanged( pcomponent );
	Rebuild();
}
void FResourcesListEntryPropertyCustomisation::OnRemoveSetupAction(UApparanceResourceListEntry_Component* pcomponent, int action_index)
{
	BeginEdit(pcomponent);
	pcomponent->ComponentSetup.RemoveAt( action_index );
	EndEdit(pcomponent);
	//explicit notify as not a property edit in the conventional sense
	NotifyPropertyChanged( pcomponent );
	Rebuild();
}
FReply FResourcesListEntryPropertyCustomisation::OnMoveSetupActionDown(UApparanceResourceListEntry_Component* pcomponent, int action_index)
{
	BeginEdit(pcomponent);
	auto moving = pcomponent->ComponentSetup[action_index];
	pcomponent->ComponentSetup.RemoveAt(action_index);
	pcomponent->ComponentSetup.Insert( moving, action_index+1 );
	EndEdit( pcomponent );
	//explicit notify as not a property edit in the conventional sense
	NotifyPropertyChanged( pcomponent );
	Rebuild();
	return FReply::Handled();
}
FReply FResourcesListEntryPropertyCustomisation::OnMoveSetupActionUp(UApparanceResourceListEntry_Component* pcomponent, int action_index)
{
	BeginEdit(pcomponent);
	auto moving = pcomponent->ComponentSetup[action_index];
	pcomponent->ComponentSetup.RemoveAt(action_index);
	pcomponent->ComponentSetup.Insert( moving, action_index-1 );
	EndEdit( pcomponent );
	//explicit notify as not a property edit in the conventional sense
	NotifyPropertyChanged( pcomponent );
	Rebuild();
	return FReply::Handled();
}

//material binding list editing
void FResourcesListEntryPropertyCustomisation::OnSyncMaterialBinding(class UApparanceResourceListEntry_Material* pmaterial)
{	
	//sync by attempting to add all parameters
	BeginEdit(pmaterial);
	for (int i = 0; i < pmaterial->ScalarParameterIDs.Num(); i++)
	{
		pmaterial->AddBinding( pmaterial->ScalarParameterIDs[i] );
	}
	for (int i = 0; i < pmaterial->VectorParameterIDs.Num(); i++)
	{
		pmaterial->AddBinding( pmaterial->VectorParameterIDs[i] );
	}
	for (int i = 0; i < pmaterial->TextureParameterIDs.Num(); i++)
	{
		pmaterial->AddBinding( pmaterial->TextureParameterIDs[i] );
	}	
	EndEdit(pmaterial);
	//explicit notify as not a property edit in the conventional sense
	NotifyPropertyChanged( pmaterial );
	Rebuild();
}
void FResourcesListEntryPropertyCustomisation::OnAddMaterialBinding(class UApparanceResourceListEntry_Material* pmaterial,FGuid material_parameter_id)
{
	BeginEdit(pmaterial);
	pmaterial->AddBinding( material_parameter_id );
	EndEdit(pmaterial);
	//explicit notify as not a property edit in the conventional sense
	NotifyPropertyChanged( pmaterial );
	Rebuild();
}
void FResourcesListEntryPropertyCustomisation::OnRemoveMaterialBinding(class UApparanceResourceListEntry_Material* pmaterial, int binding_index)
{
	BeginEdit(pmaterial);
	pmaterial->MaterialBindings.RemoveAt( binding_index );
	EndEdit(pmaterial);
	//explicit notify as not a property edit in the conventional sense
	NotifyPropertyChanged( pmaterial );
	Rebuild();
}
FReply FResourcesListEntryPropertyCustomisation::OnMoveMaterialBindingDown(class UApparanceResourceListEntry_Material* pmaterial, int binding_index)
{
	BeginEdit(pmaterial);
	auto moving = pmaterial->MaterialBindings[binding_index];
	pmaterial->MaterialBindings.RemoveAt(binding_index);
	pmaterial->MaterialBindings.Insert( moving, binding_index+1 );
	EndEdit( pmaterial );
	//explicit notify as not a property edit in the conventional sense
	NotifyPropertyChanged( pmaterial );
	Rebuild();
	return FReply::Handled();
}
FReply FResourcesListEntryPropertyCustomisation::OnMoveMaterialBindingUp(class UApparanceResourceListEntry_Material* pmaterial, int binding_index)
{
	BeginEdit(pmaterial);
	auto moving = pmaterial->MaterialBindings[binding_index];
	pmaterial->MaterialBindings.RemoveAt(binding_index);
	pmaterial->MaterialBindings.Insert( moving, binding_index-1 );
	EndEdit( pmaterial );
	//explicit notify as not a property edit in the conventional sense
	NotifyPropertyChanged( pmaterial );
	Rebuild();
	return FReply::Handled();
}


#undef LOCTEXT_NAMESPACE
