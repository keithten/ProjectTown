//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Styling/SlateColor.h"

class IDetailCategoryBuilder;
class IPropertyHandle;
enum class ECheckBoxState : uint8;

// helper class to create an enum selection customization for the specified Property in the specified CategoryBuilder.
//
class FEnumCustomization : public TSharedFromThis<FEnumCustomization>
{
	UEnum* EnumType;
	TSharedPtr<IPropertyHandle> PropertyHandle;
public:

	FEnumCustomization(){}

	void CreateEnumCustomization( IDetailCategoryBuilder& category_builder, TSharedPtr<IPropertyHandle> enum_property, UEnum* enum_type, FText name );

private:

	ECheckBoxState IsEnumActive( uint8 enum_value ) const;
	FSlateColor GetEnumTextColor( uint8 enum_value ) const;
	FText GetEnumToolTip() const;
	void OnEnumChanged( ECheckBoxState checked_state, uint8 enum_value );
	
};
