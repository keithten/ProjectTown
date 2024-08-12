//----
// Apparance Unreal Plugin
// Written by Sam R. Swain
// Copyright (c) 2022 Apparance Studios Ltd
// All rights reserved
// https://www.apparance.uk
//----

#pragma once

// unreal
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/Texture.h"

// apparance
#include "Apparance.h"

// module
#include "ApparanceResourceListEntry_3DAsset.h"
#include "Utility/ApparanceConversion.h"

// auto (last)
#include "ApparanceResourceListEntry_Component.generated.h"

// component vs blueprint
UENUM()
enum EComponentTemplateType
{
	//Use configurable instance of one of the fixed component types
	Component,

	//Use a blueprint implemented ActorComponent asset as the component template type
	Blueprint,
};

// base for constant values
UCLASS(abstract)
class APPARANCEUNREAL_API UApparanceValueConstant : public UObject
{
	GENERATED_BODY()
public:
	//access
	virtual FString DescribeValue() const { return TEXT(""); }
	//operation
};
UCLASS()
class APPARANCEUNREAL_API UApparanceValueConstant_Integer : public UApparanceValueConstant
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category = Apparance )
	int Value;
	virtual FString DescribeValue() const override { return FString::Format(TEXT("{0}"), { Value }); }
};
UCLASS()
class APPARANCEUNREAL_API UApparanceValueConstant_Float : public UApparanceValueConstant
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category=Apparance)
	float Value;
	virtual FString DescribeValue() const override { return FString::Format(TEXT("{0}"), { Value }); }
};
UCLASS()
class APPARANCEUNREAL_API UApparanceValueConstant_Bool : public UApparanceValueConstant
{
	GENERATED_BODY()
public:
	UPROPERTY( EditAnywhere, Category = Apparance )
	bool Value;	
	virtual FString DescribeValue() const override { return FString::Format(TEXT("{0}"), { Value?TEXT("True"):TEXT("False") }); }
};
UCLASS()
class APPARANCEUNREAL_API UApparanceValueConstant_String : public UApparanceValueConstant
{
	GENERATED_BODY()
public:
	UPROPERTY( EditAnywhere, Category = Apparance )
	FString Value;
	virtual FString DescribeValue() const override { return FString::Format(TEXT("\"{0}{1}\""), { Value.Left(12), (Value.Len()>12)?TEXT("..."):TEXT("") }); }
};
UCLASS()
class APPARANCEUNREAL_API UApparanceValueConstant_Vector : public UApparanceValueConstant
{
	GENERATED_BODY()
public:
	UPROPERTY( EditAnywhere, Category = Apparance )
	FVector Value;
	virtual FString DescribeValue() const override { return FString::Printf(TEXT("[%.3f, %.3f, %.3f]"), Value.X, Value.Y, Value.Z ); }
};
UCLASS()
class APPARANCEUNREAL_API UApparanceValueConstant_Colour : public UApparanceValueConstant
{
	GENERATED_BODY()
public:
	UPROPERTY( EditAnywhere, Category = Apparance )
	FLinearColor Value;
	virtual FString DescribeValue() const override { return FString::Printf(TEXT("[%.3f, %.3f, %.3f, %.3f]"), Value.R, Value.G, Value.B, Value.A ); }
};
UCLASS()
class APPARANCEUNREAL_API UApparanceValueConstant_Transform : public UApparanceValueConstant
{
	GENERATED_BODY()
public:
	UPROPERTY( EditAnywhere, Category = Apparance )
	FTransform Value;
	virtual FString DescribeValue() const override { return TEXT("Transform"); }
};
UCLASS()
class APPARANCEUNREAL_API UApparanceValueConstant_Texture : public UApparanceValueConstant
{
	GENERATED_BODY()
public:
	UPROPERTY( EditAnywhere, Category = Apparance )
	class UTexture* Value;
	virtual FString DescribeValue() const override { return Value?Value->GetDesc():TEXT("Texture"); }
};

UENUM()
enum class EApparanceValueSource
{
	Unknown,
	Parameter,
	Constant,
};

// configurable value source, parameter or constant
UCLASS()
class APPARANCEUNREAL_API UApparanceValueSource : public UObject
{
	GENERATED_BODY()
	
public:
	UPROPERTY()
	EApparanceValueSource Source;

	//type (fixed)
	UPROPERTY()
	EApparanceParameterType Type;

	//parameter ID
	UPROPERTY()
	int Parameter;

	//constant value (ptr)
	UPROPERTY()
	UApparanceValueConstant* Constant;

public:

	//access
	FString DescribeValue(class UApparanceResourceListEntry_Asset* passet = nullptr);

	//setup
	void SetType( EApparanceParameterType type ) //ensure type set up
	{
		if(Type == EApparanceParameterType::None || Type != type)
		{
			Type = type;

			//ensure always has a const
			if(!Constant)
			{
				//use default setup to init appropriate constant
				EApparanceValueSource preserve_source = Source;
				SetDefault( Type );
				Source = preserve_source;
			}
		}
	}
	void SetParameter(int parameter_id)
	{
		//type should already be set
		check(Type != EApparanceParameterType::None);

		//always want a constant value holder (for UI, switching)
		if (!Constant)
		{
			SetDefault(Type);
		}
		//now set as parameter
		Parameter = parameter_id;
		Source = EApparanceValueSource::Parameter;
	}
	void SetDefault(EApparanceParameterType type)
	{
		switch (type)
		{
			case EApparanceParameterType::Integer: SetConstant((int)0); break;
			case EApparanceParameterType::Float: SetConstant((float)0); break;
			case EApparanceParameterType::Bool: SetConstant(false); break;
			case EApparanceParameterType::String: SetConstant(FString()); break;
			case EApparanceParameterType::Colour: SetConstant(FLinearColor(0,0,0)); break;
			case EApparanceParameterType::Vector: SetConstant(FVector(0.0f,0.0f,0.0f )); break;
			case EApparanceParameterType::Frame: SetConstant(FTransform::Identity); break;
			case EApparanceParameterType::Texture: SetConstant((class UTexture*)nullptr); break;
			default:
				Constant = nullptr;
				Type = EApparanceParameterType::None;
				Source = EApparanceValueSource::Unknown;
				Parameter = 0;
				break;
		}
	}
	void SetConstant(int value)
	{
		UApparanceValueConstant_Integer* c = NewObject<UApparanceValueConstant_Integer>(this, UApparanceValueConstant_Integer::StaticClass(), NAME_None, RF_Transactional);
		c->Value = value;
		Constant = c;
		Type = EApparanceParameterType::Integer;
		Source = EApparanceValueSource::Constant;
	}
	void SetConstant(float value)
	{
		UApparanceValueConstant_Float* c= NewObject<UApparanceValueConstant_Float>(this, UApparanceValueConstant_Float::StaticClass(), NAME_None, RF_Transactional);
		c->Value = value;
		Constant = c;
		Type = EApparanceParameterType::Float;
		Source = EApparanceValueSource::Constant;
	}
	void SetConstant(bool value)
	{
		UApparanceValueConstant_Bool* c = NewObject<UApparanceValueConstant_Bool>(this, UApparanceValueConstant_Bool::StaticClass(), NAME_None, RF_Transactional);
		c->Value = value;
		Constant = c;
		Type = EApparanceParameterType::Bool;
		Source = EApparanceValueSource::Constant;
	}
	void SetConstant(FString value)
	{
		UApparanceValueConstant_String* c = NewObject<UApparanceValueConstant_String>(this, UApparanceValueConstant_String::StaticClass(), NAME_None, RF_Transactional);
		c->Value = value;
		Constant = c;
		Type = EApparanceParameterType::String;
		Source = EApparanceValueSource::Constant;
	}
	void SetConstant(FLinearColor value)
	{
		UApparanceValueConstant_Colour* c = NewObject<UApparanceValueConstant_Colour>(this, UApparanceValueConstant_Colour::StaticClass(), NAME_None, RF_Transactional);
		c->Value = value;
		Constant = c;
		Type = EApparanceParameterType::Colour;
		Source = EApparanceValueSource::Constant;
	}
	void SetConstant(FVector value)
	{
		UApparanceValueConstant_Vector* c = NewObject<UApparanceValueConstant_Vector>(this, UApparanceValueConstant_Vector::StaticClass(), NAME_None, RF_Transactional);
		c->Value = value;
		Constant = c;
		Type = EApparanceParameterType::Vector;
		Source = EApparanceValueSource::Constant;
	}
	void SetConstant(FTransform value)
	{
		UApparanceValueConstant_Transform* c = NewObject<UApparanceValueConstant_Transform>(this, UApparanceValueConstant_Transform::StaticClass(), NAME_None, RF_Transactional);
		c->Value = value;
		Constant = c;
		Type = EApparanceParameterType::Frame;
		Source = EApparanceValueSource::Constant;
	}
	void SetConstant(class UTexture* value)
	{
		UApparanceValueConstant_Texture* c = NewObject<UApparanceValueConstant_Texture>(this, UApparanceValueConstant_Texture::StaticClass(), NAME_None, RF_Transactional);
		c->Value = value;
		Constant = c;
		Type = EApparanceParameterType::Texture;
		Source = EApparanceValueSource::Constant;
	}	

	// access
	FString GetStringValue(Apparance::IParameterCollection* placement_parameters, const UApparanceResourceListEntry_Asset* passet) const;
	FLinearColor GetColourValue(Apparance::IParameterCollection* placement_parameters, const UApparanceResourceListEntry_Asset* passet) const;
	int GetIntValue(Apparance::IParameterCollection* placement_parameters, const UApparanceResourceListEntry_Asset* passet) const;
	float GetFloatValue(Apparance::IParameterCollection* placement_parameters, const UApparanceResourceListEntry_Asset* passet) const;
	bool GetBoolValue(Apparance::IParameterCollection* placement_parameters, const UApparanceResourceListEntry_Asset* passet) const;
	FVector GetVectorValue(Apparance::IParameterCollection* placement_parameters, const UApparanceResourceListEntry_Asset* passet) const;
	FTransform GetTransformValue(Apparance::IParameterCollection* placement_parameters, const UApparanceResourceListEntry_Asset* passet) const;
	class UTexture* GetTextureValue(const TArray<Apparance::TextureID>& textures, const UApparanceResourceListEntry_Asset* passet) const;
	
	//use
	bool ApplyProperty(FProperty* property, uint8* pdata, Apparance::IParameterCollection* placement_parameters, const UApparanceResourceListEntry_Component* pcomponententry) const;
	
};

//type reflection
class FApparancePropertyInfo
{
public:
	FName Name;
	FText Description;
	EApparanceParameterType Type;
	const FProperty* Property;
};
class FApparanceMethodInfo
{
public:
	FName Name;
	FText Description;
	UFunction* Function;
	TArray<FApparancePropertyInfo*> Arguments;
};
class APPARANCEUNREAL_API FApparanceComponentInfo
{
public:
	TArray<FApparancePropertyInfo*> Properties;
	TArray<FApparanceMethodInfo*> Methods;

	//setup
	FApparanceComponentInfo(const UClass* component_type);

	//query
	const FApparancePropertyInfo* FindProperty(FName property_name) const
	{
		for (int i = 0; i < Properties.Num(); i++)
		{
			if (Properties[i]->Name == property_name)
			{
				return Properties[i];
			}
		}
		return nullptr;
	}
	const FApparanceMethodInfo* FindMethod(FName method_name) const
	{
		for (int i = 0; i < Methods.Num(); i++)
		{
			if (Methods[i]->Name == method_name)
			{
				return Methods[i];
			}
		}
		return nullptr;
	}
	
private:
	void AddProperty( FProperty* pprop, EApparanceParameterType type, const FName name )
	{
		FApparancePropertyInfo* pi = new FApparancePropertyInfo();
		pi->Name = name;
		pi->Type = type;
		pi->Property = pprop;
		pi->Description = StaticEnum<EApparanceParameterType>()->GetDisplayNameTextByValue((int64)type);
		Properties.Add(pi);
	}
};
class APPARANCEUNREAL_API FApparanceComponentInventory
{
	static TMap<const UClass*, FApparanceComponentInfo*> Components;
public:
	static const FApparanceComponentInfo* GetComponentInfo(const UClass* component_type)
	{
		//find existing
		FApparanceComponentInfo** ppci = Components.Find(component_type);
		if (ppci)
		{
			return *ppci;
		}

		//build on demand
		FApparanceComponentInfo* pci = new FApparanceComponentInfo( component_type );
		Components.Add(component_type, pci);
		return pci;
	}
};

// action base
UCLASS(abstract)
class APPARANCEUNREAL_API UApparanceComponentSetupAction : public UObject
{
	GENERATED_BODY()
public:

	//setup
	virtual void InitMemberMetadata(const FApparanceComponentInfo* pcomp_info) {}

	//access
	virtual FString GetDescription() const { return TEXT(""); }
	virtual FString GetTargetName() const { return TEXT(""); }
	virtual bool SourcesAsChildren() const { return false; }
	virtual FString GetSourceSummary(class UApparanceResourceListEntry_Component* pcomponent=nullptr) const { return TEXT(""); }
	virtual int GetSourceCount() const { return 0; }
	virtual UApparanceValueSource* GetSource(int index) const { return nullptr; }
	virtual FString DescribeSource(int index) { return TEXT(""); }
	
	//apply the action
	virtual bool Apply(UObject* target, Apparance::IParameterCollection* placement_parameters, const UApparanceResourceListEntry_Component* pcomponententry) const { return false ; }
};

// set a property of a component
UCLASS()
class APPARANCEUNREAL_API UApparanceComponentSetupAction_SetProperty : public UApparanceComponentSetupAction
{
	GENERATED_BODY()
	
	//property info (transient)
	const FApparancePropertyInfo* Property;
	
public:
	//property
	UPROPERTY()
	FName Name;

	//value
	UPROPERTY()
	UApparanceValueSource* Value;

public:
	//setup
	virtual void InitMemberMetadata(const FApparanceComponentInfo* pcomp_info) override
	{
		Property = pcomp_info->FindProperty(Name);
		if (!Value)
		{
			SetProperty(Property);
		}
		Value->SetType( Property->Type );
	}
	
	//access
	virtual FString GetDescription() const override { return FString::Format(TEXT("Set {0} ="), { *Name.ToString() }); }
	virtual FString GetTargetName() const override { return Name.ToString(); }
	virtual int GetSourceCount() const override { return 1; }
	virtual UApparanceValueSource* GetSource(int index) const override { check(index == 0); return Value; }
	
	//apply the action
	virtual bool Apply(UObject* target, Apparance::IParameterCollection* placement_parameters, const UApparanceResourceListEntry_Component* pcomponententry) const override;
	
	void SetParameter(int parameter_id)
	{
		auto type = Value?Value->Type:EApparanceParameterType::None;	//keep type if available

		Value = NewObject<UApparanceValueSource>(this, UApparanceValueSource::StaticClass(), NAME_None, RF_Transactional);
		Value->Type = type;
		Value->SetParameter(parameter_id);
	}
	void SetProperty( const class FApparancePropertyInfo* property_info )
	{
		Name = property_info->Name;
		Property = property_info;
		if(!Value)
		{
			//default
			Value = NewObject<UApparanceValueSource>(this, UApparanceValueSource::StaticClass(), NAME_None, RF_Transactional);
			Value->Source = EApparanceValueSource::Constant;				
			Value->SetType( Property->Type );
		}
	}
	
};

// call a method of a component
UCLASS()
class APPARANCEUNREAL_API UApparanceComponentSetupAction_CallMethod : public UApparanceComponentSetupAction
{
	GENERATED_BODY()
	
	//transient
	
	//method info
	const FApparanceMethodInfo* Method;
	
public:
	//function
	UPROPERTY()
	FName Name;

	//function args
	UPROPERTY()
	TArray<UApparanceValueSource*> Arguments;
	
public:
	//setup
	virtual void InitMemberMetadata(const FApparanceComponentInfo* pcomp_info) override
	{
		auto m = pcomp_info->FindMethod(Name);
		if (m)
		{
			SetMethod(m);
		}
	}
	//get access
	virtual FString GetDescription() const override { return FString::Format(TEXT("Call {0}"), { *Name.ToString() }); }
	virtual FString GetTargetName() const override { return Name.ToString(); }
	virtual bool SourcesAsChildren() const override { return true; }
	virtual FString GetSourceSummary(class UApparanceResourceListEntry_Component* pcomponent = nullptr) const;
	virtual int GetSourceCount() const override { return Arguments.Num(); }
	virtual UApparanceValueSource* GetSource(int index) const override { return Arguments[index]; }
	virtual FString DescribeSource(int index)
	{ 
		if (Method && index<Method->Arguments.Num())
		{
			return Method->Arguments[index]->Name.ToString();
		}
		return FString::Format(TEXT("Argument {0}"), { index }); 
	}
	
	//apply the action
	virtual bool Apply(UObject* target, Apparance::IParameterCollection* placement_parameters, const UApparanceResourceListEntry_Component* pcomponententry) const override;

	//setup
	void SetMethod( const class FApparanceMethodInfo* method_info )
	{
		Name = method_info->Name;
		Method = method_info;		
		for (int i = 0; i < method_info->Arguments.Num(); i++)
		{
			if(i >= Arguments.Num()) //ignore existing
			{
				//new
				UApparanceValueSource* arg = AddArgument();
				arg->SetDefault( method_info->Arguments[i]->Type );
			}
		}
	}
	void AddDefaultArgument()
	{
		//use non-parameter to signify "use default"
		AddArgument()->SetParameter(0);
	}
	void AddParameterArgument(int parameter_id)
	{
		AddArgument()->SetParameter( parameter_id );
	}
	template<typename T> void AddConstantArgument( T value )
	{
		AddArgument()->SetConstant( value );
	}
	
private:
	UApparanceValueSource* AddArgument()
	{
		UApparanceValueSource* a = NewObject<UApparanceValueSource>(this, UApparanceValueSource::StaticClass(), NAME_None, RF_Transactional);
		a->Source = EApparanceValueSource::Unknown;
		Arguments.Add(a);
		return a;
	}
};


#if 1 ///////// DEPRECATED /////////
UENUM()
enum class EComponentPlacementMappingAction
{
	None,
	SetProperty,
};

USTRUCT()
struct FComponentPlacementMapping
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Apparance )
	EComponentPlacementMappingAction Action;

	UPROPERTY(EditAnywhere, Category = Apparance )
	FString Name;

	//defaults
	FComponentPlacementMapping()
		: Action( EComponentPlacementMappingAction::None ) {}
};


USTRUCT()
struct FComponentPlacementCallParameter
{
	GENERATED_BODY()

	//function parameter name
	UPROPERTY(VisibleAnywhere, Category = Apparance )
	FString Name;

	//feed from placement parameter (instead of constant)
	UPROPERTY(EditAnywhere, Category = Apparance )
	bool bUseParameter;

	//parameter instead	
	UPROPERTY(EditAnywhere, Category = Apparance, meta=(EditConditionHides, EditCondition="bUseParameter"))
	int ParameterIndex;
	
	//ugly but simple
	UPROPERTY()
	uint8 ConstantType; //0=none,1=int,2=float,3=bool,4=vector,5=colour,6=Transform
	//ugly but simple
	UPROPERTY(EditAnywhere, Category = Apparance, meta=(EditConditionHides, EditCondition="!bUseParameter && ConstantType==1", DisplayName="Integer"))
	int ConstantInt;
	UPROPERTY(EditAnywhere, Category = Apparance, meta=(EditConditionHides, EditCondition="!bUseParameter && ConstantType==2", DisplayName="Float"))
	float ConstantFloat;
	UPROPERTY(EditAnywhere, Category = Apparance, meta=(EditConditionHides, EditCondition="!bUseParameter && ConstantType==3", DisplayName="Bool"))
	bool ConstantBool;
	UPROPERTY(EditAnywhere, Category = Apparance, meta=(EditConditionHides, EditCondition="!bUseParameter && ConstantType==4", DisplayName="Vector"))
	FVector ConstantVector;
	UPROPERTY(EditAnywhere, Category = Apparance, meta=(EditConditionHides, EditCondition="!bUseParameter && ConstantType==5", DisplayName="Colour"))
	FLinearColor ConstantColour;	
	UPROPERTY(EditAnywhere, Category = Apparance, meta=(EditConditionHides, EditCondition="!bUseParameter && ConstantType==6", DisplayName="Transform"))
	FTransform ConstantTransform;

	//modifiers
	UPROPERTY(EditAnywhere, Category = Apparance, meta=(EditConditionHides, EditCondition="bUseParameter"))
	float Multiplier;
	UPROPERTY(EditAnywhere, Category = Apparance, meta=(EditConditionHides, EditCondition="bUseParameter"))
	bool bIsSpatial;
	
	//transient
	FProperty* Property;

	//defaults
	FComponentPlacementCallParameter()
		: bUseParameter( false )
		, ParameterIndex( 0 )
		, ConstantType( 1 )
		, ConstantInt( 0 )
		, ConstantFloat( 0.0f )
		, ConstantBool( false )
		, ConstantVector( FVector::ZeroVector )
		, ConstantColour( EForceInit::ForceInitToZero )
		, ConstantTransform( FTransform::Identity )
		, Multiplier(1.0f)
		, bIsSpatial(false)
		, Property(nullptr)
	{}
};

USTRUCT()
struct FComponentPlacementCall
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, Category = Apparance )
	FName Function;

	UPROPERTY(EditAnywhere, Category = Apparance )
	TArray<FComponentPlacementCallParameter> Parameters;
};
#endif


// entry for mapping resource name to component with binding information
//
UCLASS()
class APPARANCEUNREAL_API UApparanceResourceListEntry_Component
	: public UApparanceResourceListEntry_3DAsset
{
	GENERATED_BODY()

public:

	// which template source to use?
	UPROPERTY( EditAnywhere, Category = Apparance )
	TEnumAsByte<EComponentTemplateType> TemplateType;

	// component prototype/defaults
	//
	UPROPERTY( EditAnywhere, Category = Apparance )
	UActorComponent* ComponentTemplate;

	// component prototype/defaults
	//
	UPROPERTY( EditAnywhere, Category = Apparance )
	UBlueprint* BlueprintTemplate;

	// how to map incoming parameter list to actions that will affect the placed component
	//
	UPROPERTY(meta=(Deprecated))
	TArray<FComponentPlacementMapping> ParameterMap;
	
	// set up placed component by calling functions using incoming placement parameters
	// as function parameters, or constant values
	//
	UPROPERTY(meta=(Deprecated))
	TArray<FComponentPlacementCall> FunctionCalls;

	// Parameter driven actions to apply to each placed component
	UPROPERTY( EditAnywhere, Category = Apparance )
	TArray<UApparanceComponentSetupAction*> ComponentSetup;
	
public:
	
	/* Begin UObject */
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual void PostLoad() override;
	/* End UObject */

	//setup
	UApparanceComponentSetupAction_SetProperty* AddAction_SetProperty(const FApparancePropertyInfo* prop_info);
	UApparanceComponentSetupAction_CallMethod* AddAction_CallMethod(const FApparanceMethodInfo* method_info);


	//access
	const UClass* GetTemplateClass() const;
	virtual UClass* GetAssetClass() const override { return UActorComponent::StaticClass(); }
	virtual FText GetAssetTypeName() const override;
	virtual FName GetIconName( bool is_open=false ) override;

	static FText StaticAssetTypeName();
	static FName StaticIconName( bool is_open=false );

	void CheckUpgrade();
	void InitMemberMetadata();
	void ApplyParameters( struct Apparance::IParameterCollection* placement_parameters, class UActorComponent* pcomponent) const;

public: //static utils

	//param helpers: general
	static float GetIntParameter(const Apparance::IParameterCollection* placement_parameters, int parameter_index);
	static float GetFloatParameter(const Apparance::IParameterCollection* placement_parameters, int parameter_index);
	static bool GetBoolParameter(const Apparance::IParameterCollection* placement_parameters, int parameter_index);
	//param helpers: apparance	
	static Apparance::Vector3 GetVector3Parameter(const Apparance::IParameterCollection* placement_parameters, int parameter_index, bool allow_coersion=false);
	static bool GetFrameParameter(const Apparance::IParameterCollection* placement_parameters, int parameter_index, Apparance::Frame& frame_out);
	//param helpers: unreal (includes spacial remapping)
	static FLinearColor GetColourParameter(const Apparance::IParameterCollection* placement_parameters, int parameter_index, bool allow_coersion=false);
	static FString GetStringParameter(const Apparance::IParameterCollection* placement_parameters, int parameter_index );
	static FVector GetFVectorParameter(const Apparance::IParameterCollection* placement_parameters, int parameter_index, bool allow_coersion=false);
	static FTransform GetTransformParameter(const Apparance::IParameterCollection* placement_parameters, int parameter_index, bool allow_coersion=false);
	static class UTexture* GetTextureParameter(const TArray<Apparance::TextureID>& textures, int texture_index);
	
};

