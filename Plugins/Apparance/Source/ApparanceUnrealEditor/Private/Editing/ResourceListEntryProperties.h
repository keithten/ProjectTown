//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#pragma once

//unreal
#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "IDetailCustomization.h"
#include "DetailCategoryBuilder.h" //IDetailCategoryBuilder
#include "DetailLayoutBuilder.h" //IDetailLayoutBuilder

//module
#include "RemoteEditing.h"
class UApparanceResourceListEntry;
class UApparanceResourceListEntry_Asset;
#include "UI/EnumCustomization.h"
#include "ResourceList/ApparanceResourceListEntry_Asset.h"

//apparance
#include "Apparance.h"

/// <summary>
/// entity property customisation
/// also used as base for other customisers that want to re-use some of the functionality (e.g. procedure selection)
/// </summary>
class FResourcesListEntryPropertyCustomisation : public IDetailCustomization
{
	//enum editing UIs
	TSharedPtr<FEnumCustomization> TemplateTypeEnumControl;
	TSharedPtr<FEnumCustomization> BoundsSourceEnumControl;
	TSharedPtr<FEnumCustomization> FixOverrideEnumControl;
	TSharedPtr<FEnumCustomization> FixOrientationEnumControl;
	TSharedPtr<FEnumCustomization> SizeModeEnumControl;
	
	IDetailLayoutBuilder* DetailBuilderPtr;

	static bool s_bInlineHelpEnabled;

public:
	//font to use (default textblock doesn't match property editor panel)
	FSlateFontInfo DefaultFont;
	
public:
	FResourcesListEntryPropertyCustomisation();

	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable( new FResourcesListEntryPropertyCustomisation );
	}	

	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder) override;

	//enable/disable inline help (not, doesn't cause rebuild, have to do that externally)
	static void ToggleInlineHelpText() { s_bInlineHelpEnabled = !s_bInlineHelpEnabled; }

protected:
	

public:

	//changes
	void OnPropertyChanged( const FPropertyChangedEvent& );
	void Rebuild();

	//ui
	TSharedRef<class SWidget> BuildParameterSelectMenu(UApparanceResourceListEntry_Asset* passetentry, TSharedPtr<IPropertyHandle> id_property);
	bool BuildTransformParameterSelect(
			IDetailCategoryBuilder& parameter_category, 
			FText filter_string,
			UApparanceResourceListEntry_Asset* passetentry,
			TSharedPtr<IPropertyHandle> id_property,
			TSharedPtr<IPropertyHandle> custom_value_property );
	FReply OnTransformPresetSelected(class UApparanceResourceListEntry_3DAsset* p3dasset, int preset_index);
	void BuildParameterRow(
			IDetailCategoryBuilder& parameter_category, 
			FText filter_string,
			UApparanceResourceListEntry_Asset* passetentry,
			int parameter_index);
	TSharedRef<class SWidget> BuildParameterTypeSelectMenu(UApparanceResourceListEntry_Asset* passetentry);
	void BuildTextureRow(
			IDetailCategoryBuilder& parameter_category, 
			FText filter_string,
			class UApparanceResourceListEntry_Material* pmaterial,
			int texture_index);
	void BuildSetupActionRow( 
			IDetailCategoryBuilder& component_category, 
			FText filter_string,
			class UApparanceResourceListEntry_Component* pcomponent,
			int action_index);
	TSharedRef<class SWidget> BuildAddSetupActionMenu(const class FApparanceComponentInfo* pcomponent_info, class UApparanceResourceListEntry_Component* pcomponent);
	void BuildMaterialBindingRow( 
		IDetailCategoryBuilder& material_category, 
		FText filter_string,
		class UApparanceResourceListEntry_Material* pmaterial,
		int binding_index);
	TSharedRef<class SWidget> BuildValueSourceEditor(class UApparanceResourceListEntry_Component* pcomponent, class UApparanceComponentSetupAction* paction, int source_index);
	TSharedRef<class SWidget> BuildConstantValueEditControl(class UApparanceValueSource* psource);
	TSharedRef<class SWidget> BuildAddMaterialBindingMenu(class UApparanceResourceListEntry_Material* pmaterial);
	FText OnGetResourceDescriptor( UApparanceResourceListEntry* pentry ) const;
	void BuildDocumentationText(IDetailCategoryBuilder& category, FText help_text);

	// the entry we are details panel for
	//
	template<class T> T* GetEntry()
	{
		TArray<TWeakObjectPtr<UObject>> EditedObjects;
		DetailBuilderPtr->GetObjectsBeingCustomized( EditedObjects );
		for(int32 i = 0; i < EditedObjects.Num(); i++)
		{
			T* pentry = Cast<T>( EditedObjects[i].Get() );
			if(pentry)
			{
				return pentry;
			}
		}
		return nullptr;
	}

	//parameter list editing
	FText OnGetParameterNameText( UApparanceResourceListEntry_Asset* passetentry, int parameter_index ) const;
	void OnParameterNameTextCommitted(const FText& new_name, ETextCommit::Type operation, UApparanceResourceListEntry_Asset* passetentry, int parameter_index);
	void OnAddParameter(UApparanceResourceListEntry_Asset* passetentry, EApparanceParameterType parameter_type);
	void OnRemoveParameter(UApparanceResourceListEntry_Asset* passetentry, int parameter_index);
	FReply OnMoveParameterDown(UApparanceResourceListEntry_Asset* passetentry, int parameter_index);
	FReply OnMoveParameterUp(UApparanceResourceListEntry_Asset* passetentry, int parameter_index);

	//material texture list editing
	FText OnGetTextureNameText( class UApparanceResourceListEntry_Material* pmaterial, int texture_index ) const;
	void OnTextureNameTextCommitted(const FText& new_name, ETextCommit::Type operation, class UApparanceResourceListEntry_Material* pmaterial, int texture_index);
	FReply OnAddTexture(class UApparanceResourceListEntry_Material* pmaterial);
	void OnRemoveTexture(class UApparanceResourceListEntry_Material* pmaterial, int texture_index);
	FReply OnMoveTextureDown(class UApparanceResourceListEntry_Material* pmaterial, int texture_index);
	FReply OnMoveTextureUp(class UApparanceResourceListEntry_Material* pmaterial, int texture_index);
	
	//component setup action list editing
	void OnAddSetupAction_Property(class UApparanceResourceListEntry_Component* pcomponent,const class FApparancePropertyInfo* prop_info);
	void OnAddSetupAction_Method(class UApparanceResourceListEntry_Component* pcomponent,const class FApparanceMethodInfo* method_info );
	void OnRemoveSetupAction(class UApparanceResourceListEntry_Component* pcomponent, int action_index);
	FReply OnMoveSetupActionDown(class UApparanceResourceListEntry_Component* pcomponent, int action_index);
	FReply OnMoveSetupActionUp(class UApparanceResourceListEntry_Component* pcomponent, int action_index);

	//material binding list editing
	void OnSyncMaterialBinding(class UApparanceResourceListEntry_Material* pmaterial);
	void OnAddMaterialBinding(class UApparanceResourceListEntry_Material* pmaterial,FGuid material_parameter_id);
	void OnRemoveMaterialBinding(class UApparanceResourceListEntry_Material* pmaterial, int binding_index);
	FReply OnMoveMaterialBindingDown(class UApparanceResourceListEntry_Material* pmaterial, int binding_index);
	FReply OnMoveMaterialBindingUp(class UApparanceResourceListEntry_Material* pmaterial, int binding_index);
	
};

