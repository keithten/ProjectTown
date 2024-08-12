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
#include "Application/ThrottleManager.h"

//module
#include "RemoteEditing.h"
#include "ApparanceEntity.h"

//apparance
#include "Apparance.h"

//enums
enum class EApparanceFrameComponent
{
	Size,
	Offset,
	Orientation
};
enum class EApparanceFrameAxis
{
	X,Y,Z
};
//combination of above (used to get around 4 param limit of widget attributes)
struct EApparanceFrameElement
{
	EApparanceFrameComponent Component;
	EApparanceFrameAxis Axis;

	EApparanceFrameElement( EApparanceFrameComponent c, EApparanceFrameAxis a )
		: Component( c )
		, Axis( a ) {}
};
	
/// <summary>
/// entity property customisation
/// also used as base for other customisers that want to re-use some of the functionality (e.g. procedure selection)
/// </summary>
class FEntityPropertyCustomisation : public IDetailCustomization
{
	//cached procedure property
	TSharedPtr<IPropertyHandle> ProcedureIDProperty;

	//font to use (default textblock doesn't match property editor panel)
	FSlateFontInfo DefaultFont;

	//is colour picking currently being done interactively?
	bool bColourPickingInteractive;
	FLinearColor LastColourValue;

	//interaction
	FThrottleRequest InteractivePropertyResponsivnessThrottle;

public:
	FEntityPropertyCustomisation();

	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable( new FEntityPropertyCustomisation );		
	}	

	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder) override;

protected:
	//default implementation is for AApparanceEntity
	virtual bool ShowProcedureSelection() { return true; }
	virtual bool ShowProcedureParameters() { return true; }
	virtual bool ShowDiagnostics() { return !ShowPinEnableUI(); }
	virtual bool ShowPinEnableUI() { return false; }
	virtual FName GetProcedureIDPropertyName()
	{
		return GET_MEMBER_NAME_CHECKED(AApparanceEntity, ProcedureID);
	}
	virtual UClass* GetProcedureIDPropertyClass()
	{
		return AApparanceEntity::StaticClass();
	}
	virtual bool IsPinEnabled(int param_id) const { return true; }
	virtual void SetPinEnable(int param_id, bool enable, bool is_editing_transaction ) { }
	

private:
	TSharedRef<class SWidget> BuildProceduresMenu();
	FText GetLogoOverlayText() const;
	FText GetProcedureName() const;
	FText GetProcedureInformation() const;
	FReply Editor_ExportEntityStructure();
	FReply Editor_OpenProcedure();
	ECheckBoxState GetInteractiveEditingState( AApparanceEntity* pactor ) const;
	void OnInteractiveEditingChanged( ECheckBoxState new_state, AApparanceEntity* pactor );


	TSharedRef<SWidget> GenerateParameterEditControl( 
		class IProceduralObject* pentity,
		int parameter_index,
		const Apparance::IParameterCollection* spec_params,
		TArray<const Apparance::IParameterCollection*>* prop_hierarchy );

	bool IsWorldspace(Apparance::ValueID input_id, const TArray<const Apparance::IParameterCollection*>* prop_hierarchy) const;
	float CalculateSpinSpeed(bool is_worldspace) const;

	//info
	bool CheckParameterEditable( IProceduralObject* pentity, Apparance::ValueID input_id ) const;
	FText GetParameterLabel( IProceduralObject* pentity, Apparance::ValueID input_id ) const;
	FText GetParameterTooltip( IProceduralObject* pentity, Apparance::ValueID input_id, const Apparance::IParameterCollection* spec_params ) const;
		
	//type specific controls
	TSharedPtr<SWidget> GenerateIntegerEditControl( IProceduralObject* pentity, Apparance::ValueID input_id, const TArray<const Apparance::IParameterCollection*>* prop_hierarchy, const Apparance::IParameterCollection* spec_params );
	TSharedPtr<SWidget> GenerateFloatEditControl( IProceduralObject* pentity, Apparance::ValueID input_id, const TArray<const Apparance::IParameterCollection*>* prop_hierarchy );
	TSharedPtr<SWidget> GenerateBoolEditControl( IProceduralObject* pentity, Apparance::ValueID input_id, const TArray<const Apparance::IParameterCollection*>* prop_hierarchy );
	TSharedPtr<SWidget> GenerateColourEditControl( IProceduralObject* pentity, Apparance::ValueID input_id, const TArray<const Apparance::IParameterCollection*>* prop_hierarchy );
	TSharedPtr<SWidget> GenerateStringEditControl( IProceduralObject* pentity, Apparance::ValueID input_id, const TArray<const Apparance::IParameterCollection*>* prop_hierarchy );
	TSharedPtr<SWidget> GenerateVectorEditControl( IProceduralObject* pentity, Apparance::ValueID input_id, const TArray<const Apparance::IParameterCollection*>* prop_hierarchy );
	TSharedPtr<SWidget> GenerateFrameEditControl( IProceduralObject* pentity, Apparance::ValueID input_id, const TArray<const Apparance::IParameterCollection*>* prop_hierarchy );
	
	//parameter accessors
	int32 GetIntegerParameter( IProceduralObject* pentity, Apparance::ValueID input_id ) const;
	void SetIntegerParameter( int32 new_value, IProceduralObject* pentity, Apparance::ValueID input_id, bool editing_transaction );
	float GetFloatParameter( IProceduralObject* pentity, Apparance::ValueID input_id ) const;
	void SetFloatParameter( float new_value, IProceduralObject* pentity, Apparance::ValueID input_id, bool editing_transaction );
	bool GetBoolParameter( IProceduralObject* pentity, Apparance::ValueID input_id ) const;
	void SetBoolParameter( bool new_value, IProceduralObject* pentity, Apparance::ValueID input_id, bool editing_transaction );
	FLinearColor GetColourParameter( IProceduralObject* pentity, Apparance::ValueID input_id ) const;
	void SetColourParameter( FLinearColor new_value, IProceduralObject* pentity, Apparance::ValueID input_id, bool editing_transaction );
	FText GetStringParameter( IProceduralObject* pentity, Apparance::ValueID input_id ) const;
	void SetStringParameter( FText new_value, IProceduralObject* pentity, Apparance::ValueID input_id, bool editing_transaction );
	float GetVectorParameterComponent( IProceduralObject* pentity, Apparance::ValueID input_id, EApparanceFrameAxis axis ) const;
	void SetVectorParameterComponent( float new_value, IProceduralObject* pentity, Apparance::ValueID input_id, EApparanceFrameAxis axis, bool editing_transaction );
	float GetFrameParameterComponent( IProceduralObject* pentity, Apparance::ValueID input_id, EApparanceFrameComponent component, EApparanceFrameAxis axis, bool is_worldspace ) const;
	void SetFrameParameterComponent( float new_value, IProceduralObject* pentity, Apparance::ValueID input_id, EApparanceFrameComponent component, EApparanceFrameAxis axis, bool editing_transaction, bool is_worldspace );
	
	//interactivity fixes
	bool IsInteracting() const { return InteractivePropertyResponsivnessThrottle.IsValid(); }
	void BeginInteraction();
	void EndInteraction();

	//UI delagates (accessor wrappers)
	//NOTE: Both Change/Commit are needed for interactive/slider/etc editable values
	TOptional<int32> CurrentIntegerParameter( IProceduralObject* pentity, Apparance::ValueID input_id ) const { return GetIntegerParameter( pentity, input_id ); }
	void ChangeIntegerParameter( int32 new_value, IProceduralObject* pentity, Apparance::ValueID input_id ) { BeginInteraction(); SetIntegerParameter( new_value, pentity, input_id, false ); }
	void CommitIntegerParameter( int32 new_value, ETextCommit::Type commit_info, IProceduralObject* pentity, Apparance::ValueID input_id ) { EndInteraction();  SetIntegerParameter( new_value, pentity, input_id, true ); }
	TOptional<float> CurrentFloatParameter( IProceduralObject* pentity, Apparance::ValueID input_id ) const { return GetFloatParameter( pentity, input_id ); }
	void ChangeFloatParameter( float new_value, IProceduralObject* pentity, Apparance::ValueID input_id ) { BeginInteraction();  SetFloatParameter( new_value, pentity, input_id, false ); }
	void CommitFloatParameter( float new_value, ETextCommit::Type commit_info, IProceduralObject* pentity, Apparance::ValueID input_id ) { EndInteraction(); SetFloatParameter( new_value, pentity, input_id, true ); }
	ECheckBoxState CurrentBoolParameter( IProceduralObject* pentity, Apparance::ValueID input_id ) const { return GetBoolParameter( pentity, input_id )?ECheckBoxState::Checked:ECheckBoxState::Unchecked; }
	void ChangeBoolParameter( ECheckBoxState new_value, IProceduralObject* pentity, Apparance::ValueID input_id ) { SetBoolParameter( new_value==ECheckBoxState::Checked, pentity, input_id, true ); }
	FLinearColor CurrentColourParameter( IProceduralObject* pentity, Apparance::ValueID input_id ) const { return GetColourParameter( pentity, input_id ); }
	void OnColorPickerInteractiveBegin();
	void OnColorPickerInteractiveEnd( IProceduralObject* pentity, Apparance::ValueID input_id );
	FReply OnClickColorBlock(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, IProceduralObject* pentity, Apparance::ValueID input_id);
	void OnSetColorFromColorPicker(FLinearColor NewColor, IProceduralObject* pentity, Apparance::ValueID input_id);
	FText CurrentStringParameter( IProceduralObject* pentity, Apparance::ValueID input_id ) const { return GetStringParameter( pentity, input_id ); }
	void ChangeStringParameter( const FText& new_value, IProceduralObject* pentity, Apparance::ValueID input_id ) { SetStringParameter( new_value, pentity, input_id, false ); }
	void CommitStringParameter( const FText& new_value, ETextCommit::Type commit_info, IProceduralObject* pentity, Apparance::ValueID input_id ) { SetStringParameter( new_value, pentity, input_id, true ); }
	TOptional<float> CurrentVectorParameterComponent( IProceduralObject* pentity, Apparance::ValueID input_id, EApparanceFrameAxis axis ) const { return GetVectorParameterComponent( pentity, input_id, axis ); }
	void ChangeVectorParameterComponent( float new_value, IProceduralObject* pentity, Apparance::ValueID input_id, EApparanceFrameAxis axis ) { BeginInteraction(); SetVectorParameterComponent( new_value, pentity, input_id, axis, false ); }
	void CommitVectorParameterComponent( float new_value, ETextCommit::Type commit_info, IProceduralObject* pentity, Apparance::ValueID input_id, EApparanceFrameAxis axis ) { EndInteraction(); SetVectorParameterComponent( new_value, pentity, input_id, axis, true ); }
	TOptional<float> CurrentFrameParameterComponent( IProceduralObject* pentity, Apparance::ValueID input_id, EApparanceFrameElement element, bool is_worldspace ) const { return GetFrameParameterComponent( pentity, input_id, element.Component, element.Axis, is_worldspace ); }
	void ChangeFrameParameterComponent( float new_value, IProceduralObject* pentity, Apparance::ValueID input_id, EApparanceFrameElement element, bool is_worldspace ) { BeginInteraction(); SetFrameParameterComponent( new_value, pentity, input_id, element.Component, element.Axis, false, is_worldspace ); }
	void CommitFrameParameterComponent( float new_value, ETextCommit::Type commit_info, IProceduralObject* pentity, Apparance::ValueID input_id, EApparanceFrameElement element, bool is_worldspace ) { EndInteraction(); SetFrameParameterComponent( new_value, pentity, input_id, element.Component, element.Axis, true, is_worldspace ); }
	FText CurrentListParameterDesc(IProceduralObject* pentity, Apparance::ValueID input_id) const;

	ECheckBoxState CurrentPinEnabled( Apparance::ValueID input_id ) const { return IsPinEnabled( input_id )?ECheckBoxState::Checked:ECheckBoxState::Unchecked; }
	void ChangePinEnable( ECheckBoxState new_value, Apparance::ValueID input_id ) { SetPinEnable( input_id, new_value==ECheckBoxState::Checked, true ); }
	
	//context menu
	FReply HandleMouseButtonUp(
		const FGeometry& InGeometry, 
		const FPointerEvent& InPointerEvent, 
		IProceduralObject* pentity,
		int parameter_index,
		const Apparance::IParameterCollection* spec_params );

	void AddFlagMetadataMenuItems(FMenuBuilder& MenuBuilder, IProceduralObject* pentity,	int parameter_index,const Apparance::IParameterCollection* spec_params,	TArray<const Apparance::IParameterCollection*>* prop_hierarchy);
	void AddNumericMetadataMenuItems(FMenuBuilder& MenuBuilder, IProceduralObject* pentity,	int parameter_index,const Apparance::IParameterCollection* spec_params,	TArray<const Apparance::IParameterCollection*>* prop_hierarchy);
	void AddSpatialMetadataMenuItems(FMenuBuilder& MenuBuilder, IProceduralObject* pentity,	int parameter_index,const Apparance::IParameterCollection* spec_params,	TArray<const Apparance::IParameterCollection*>* prop_hierarchy);
	TSharedRef<SWidget> GenerateTextWidget(FString text) const;
};
