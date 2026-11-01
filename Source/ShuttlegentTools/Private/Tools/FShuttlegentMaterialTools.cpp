#include "Tools/FShuttlegentMaterialTools.h"

#include "AssetToolsModule.h"
#include "Components/MeshComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Texture.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "GameFramework/Actor.h"
#include "IAssetTools.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#include "FShuttlegentToolBase.h"
#include "FShuttlegentToolObjects.h"
#include "Tool/FShuttlegentToolArguments.h"
#include "Tool/FShuttlegentToolSchema.h"
#include "WriteGate/FShuttlegentWriteRequest.h"

#define LOCTEXT_NAMESPACE "ShuttlegentMaterialTools"

namespace FShuttlegentMaterialToolsPrivate
{
	// 工具名称
	inline const TCHAR* GetParametersName = TEXT("get_material_parameters");
	inline const TCHAR* SetParametersName = TEXT("set_material_parameters");
	inline const TCHAR* CreateInstancesName = TEXT("create_material_instances");
	inline const TCHAR* AssignMaterialName = TEXT("assign_material");

	// 参数名称
	inline const TCHAR* ArgTargets = TEXT("targets");
	inline const TCHAR* ArgParameters = TEXT("parameters");
	inline const TCHAR* ArgParent = TEXT("parent");
	inline const TCHAR* ArgNames = TEXT("names");
	inline const TCHAR* ArgFolder = TEXT("folder");
	inline const TCHAR* ArgMaterial = TEXT("material");
	inline const TCHAR* ArgSlots = TEXT("slots");

	// 可读写的参数类型，即 get_material_parameters 的输出顺序；其余类型（如运行时虚拟纹理）不在材质工具范围内
	inline const EMaterialParameterType ParameterTypes[] = { EMaterialParameterType::Scalar, EMaterialParameterType::Vector, EMaterialParameterType::Texture, EMaterialParameterType::StaticSwitch };

	// 向量参数的分量数：RGB 或 RGBA；只给 RGB 时 A 取 1
	inline constexpr int32 ColorChannels = 3;
	inline constexpr int32 ColorChannelsWithAlpha = 4;

	// 十六进制颜色（sRGB）的前缀，例如 #FF8800
	inline const TCHAR* HexColorPrefix = TEXT("#");

	// 参数值文本：空贴图与静态开关的取值
	inline const TCHAR* NoneValue = TEXT("None");
	inline const TCHAR* TrueValue = TEXT("true");
	inline const TCHAR* FalseValue = TEXT("false");

	// 包路径的层级分隔符
	inline const TCHAR* PackageSeparator = TEXT("/");

	// 列出参数名称时的分隔符
	inline const TCHAR* NameListSeparator = TEXT(", ");

	/** 材质上的一个全局参数（不含材质图层参数），及其在该材质上的取值与元数据。 */
	struct FMaterialParameter
	{
		FMaterialParameterInfo Info {};
		EMaterialParameterType Type = EMaterialParameterType::None;
		FMaterialParameterMetadata Meta {};
	};

	/** 一个待修改的参数及其新值。 */
	struct FParameterChange
	{
		FMaterialParameter Parameter {};
		FMaterialParameterValue Value {};
	};

	/** assign_material 要改的一个材质槽。 */
	struct FSlotTarget
	{
		TWeakObjectPtr<UMeshComponent> Component = nullptr;
		int32 Index = INDEX_NONE;
	};

	FString DescribeType(EMaterialParameterType Type)
	{
		switch (Type)
		{
		case EMaterialParameterType::Scalar:
		{
			return TEXT("scalar");
		}
		case EMaterialParameterType::Vector:
		{
			return TEXT("vector");
		}
		case EMaterialParameterType::Texture:
		{
			return TEXT("texture");
		}
		case EMaterialParameterType::StaticSwitch:
		{
			return TEXT("static switch");
		}
		default:
		{
			return TEXT("other");
		}
		}
	}

	/** 参数值的文本：标量 0.5，向量 (R=1,G=0,B=0,A=1)，贴图的对象路径或 None，静态开关 true / false。 */
	FString FormatValue(const FMaterialParameterValue& Value)
	{
		switch (Value.Type)
		{
		case EMaterialParameterType::Scalar:
		{
			return FString::SanitizeFloat(Value.AsScalar());
		}
		case EMaterialParameterType::Vector:
		{
			const FLinearColor Color = Value.AsLinearColor();
			return FString::Printf(TEXT("(R=%g,G=%g,B=%g,A=%g)"), Color.R, Color.G, Color.B, Color.A);
		}
		case EMaterialParameterType::Texture:
		{
			const UObject* Texture = Value.AsTextureObject();
			return IsValid(Texture) ? Texture->GetPathName() : FString(NoneValue);
		}
		case EMaterialParameterType::StaticSwitch:
		{
			return Value.AsStaticSwitch() ? TrueValue : FalseValue;
		}
		default:
		{
			return FString();
		}
		}
	}

	/** 材质或材质实例的全部全局参数，按分组、名称排序。 */
	TArray<FMaterialParameter> CollectParameters(const UMaterialInterface* Material)
	{
		TArray<FMaterialParameter> Parameters;
		for (const EMaterialParameterType Type : ParameterTypes)
		{
			TMap<FMaterialParameterInfo, FMaterialParameterMetadata> Found;
			Material->GetAllParametersOfType(Type, Found);
			for (const TPair<FMaterialParameterInfo, FMaterialParameterMetadata>& Pair : Found)
			{
				if (Pair.Key.Association != EMaterialParameterAssociation::GlobalParameter) continue;

				FMaterialParameter& Parameter = Parameters.AddDefaulted_GetRef();
				Parameter.Info = Pair.Key;
				Parameter.Type = Type;
				Parameter.Meta = Pair.Value;
			}
		}

		Parameters.Sort([](const FMaterialParameter& Left, const FMaterialParameter& Right)
		{
			if (Left.Meta.Group != Right.Meta.Group) return Left.Meta.Group.LexicalLess(Right.Meta.Group);
			return Left.Info.Name.LexicalLess(Right.Info.Name);
		});
		return Parameters;
	}

	const FMaterialParameter* FindParameter(const TArray<FMaterialParameter>& Parameters, const FString& Name)
	{
		return Parameters.FindByPredicate([&Name](const FMaterialParameter& Parameter) { return Parameter.Info.Name.ToString().Equals(Name, ESearchCase::IgnoreCase); });
	}

	FString ListParameterNames(const TArray<FMaterialParameter>& Parameters)
	{
		TArray<FString> Names;
		for (const FMaterialParameter& Parameter : Parameters)
		{
			Names.Add(Parameter.Info.Name.ToString());
		}
		return Names.IsEmpty() ? FString(NoneValue) : FString::Join(Names, NameListSeparator);
	}

	/** 按参数类型解析模型给出的 JSON 值：标量为数字，向量为 [r, g, b] / [r, g, b, a] 或 #RRGGBB，贴图为资源路径（空或 None 表示清空），静态开关为布尔值。 */
	bool ParseValue(const FMaterialParameter& Parameter, const TSharedPtr<FJsonValue>& Json, FMaterialParameterValue& OutValue, FString& OutError)
	{
		const FString Name = Parameter.Info.Name.ToString();
		if (!Json.IsValid())
		{
			OutError = FString::Printf(TEXT("No value given for %s."), *Name);
			return false;
		}

		switch (Parameter.Type)
		{
		case EMaterialParameterType::Scalar:
		{
			double Number = 0.0;
			if (Json->TryGetNumber(Number))
			{
				OutValue = FMaterialParameterValue(static_cast<float>(Number));
				return true;
			}
			OutError = FString::Printf(TEXT("%s is a scalar parameter; give a number."), *Name);
			return false;
		}
		case EMaterialParameterType::Vector:
		{
			const TArray<TSharedPtr<FJsonValue>>* Channels = nullptr;
			FString Text;
			if (Json->TryGetArray(Channels) && (Channels->Num() == ColorChannels || Channels->Num() == ColorChannelsWithAlpha))
			{
				double Values[ColorChannelsWithAlpha] = { 0.0, 0.0, 0.0, 1.0 };
				for (int32 Index = 0; Index < Channels->Num(); ++Index)
				{
					if ((*Channels)[Index].IsValid() && (*Channels)[Index]->TryGetNumber(Values[Index])) continue;

					OutError = FString::Printf(TEXT("%s: every channel must be a number."), *Name);
					return false;
				}
				OutValue = FMaterialParameterValue(FLinearColor(static_cast<float>(Values[0]), static_cast<float>(Values[1]), static_cast<float>(Values[2]), static_cast<float>(Values[3])));
				return true;
			}
			if (Json->TryGetString(Text) && Text.StartsWith(HexColorPrefix))
			{
				OutValue = FMaterialParameterValue(FLinearColor(FColor::FromHex(Text)));
				return true;
			}
			OutError = FString::Printf(TEXT("%s is a vector parameter; give [r, g, b], [r, g, b, a] (linear, 0-1) or #RRGGBB (sRGB)."), *Name);
			return false;
		}
		case EMaterialParameterType::Texture:
		{
			FString Path;
			if (!Json->TryGetString(Path))
			{
				OutError = FString::Printf(TEXT("%s is a texture parameter; give a texture asset path."), *Name);
				return false;
			}
			if (Path.IsEmpty() || Path.Equals(NoneValue, ESearchCase::IgnoreCase))
			{
				OutValue = FMaterialParameterValue(static_cast<UTexture*>(nullptr));
				return true;
			}

			UTexture* Texture = Cast<UTexture>(FShuttlegentToolObjects::ResolveAsset(Path));
			if (!IsValid(Texture))
			{
				OutError = FString::Printf(TEXT("%s: no texture found at %s."), *Name, *Path);
				return false;
			}
			OutValue = FMaterialParameterValue(Texture);
			return true;
		}
		case EMaterialParameterType::StaticSwitch:
		{
			bool bValue = false;
			if (Json->TryGetBool(bValue))
			{
				OutValue = FMaterialParameterValue(bValue);
				return true;
			}
			OutError = FString::Printf(TEXT("%s is a static switch parameter; give true or false."), *Name);
			return false;
		}
		default:
		{
			OutError = FString::Printf(TEXT("%s has a parameter type the material tools do not change."), *Name);
			return false;
		}
		}
	}

	/** 模型给出的参数值，参数名不区分大小写。 */
	TSharedPtr<FJsonValue> FindArgumentValue(const TSharedPtr<FJsonObject>& Values, const FString& Name)
	{
		if (!Values.IsValid()) return nullptr;

		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Values->Values)
		{
			if (Pair.Key.Equals(Name, ESearchCase::IgnoreCase)) return Pair.Value;
		}
		return nullptr;
	}

	TSharedPtr<FJsonObject> GetParametersArgument(const TSharedRef<FJsonObject>& Arguments)
	{
		const TSharedPtr<FJsonObject>* Values = nullptr;
		Arguments->TryGetObjectField(ArgParameters, Values);
		return Values != nullptr ? *Values : nullptr;
	}

	/** 把 parameters 参数对照材质的参数逐个解析；有任一参数不存在或值不合法时整体拒绝。 */
	bool PlanChanges(const UMaterialInterface* Material, const TSharedPtr<FJsonObject>& Values, TArray<FParameterChange>& OutChanges, FString& OutError)
	{
		if (!Values.IsValid()) return true;

		const TArray<FMaterialParameter> Parameters = CollectParameters(Material);
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Values->Values)
		{
			const FMaterialParameter* Parameter = FindParameter(Parameters, Pair.Key);
			if (Parameter == nullptr)
			{
				OutError = FString::Printf(TEXT("%s has no parameter %s; nothing was changed. Its parameters: %s."), *Material->GetName(), *Pair.Key, *ListParameterNames(Parameters));
				return false;
			}

			FParameterChange& Change = OutChanges.AddDefaulted_GetRef();
			Change.Parameter = *Parameter;
			if (!ParseValue(*Parameter, Pair.Value, Change.Value, OutError)) return false;
		}
		return true;
	}

	/** 写入一个参数，不刷新材质实例；全部写完后由调用方调用一次 UpdateMaterialInstance。 */
	void ApplyChange(UMaterialInstanceConstant* Instance, const FParameterChange& Change)
	{
		switch (Change.Parameter.Type)
		{
		case EMaterialParameterType::Scalar:
		{
			Instance->SetScalarParameterValueEditorOnly(Change.Parameter.Info, Change.Value.AsScalar());
			break;
		}
		case EMaterialParameterType::Vector:
		{
			Instance->SetVectorParameterValueEditorOnly(Change.Parameter.Info, Change.Value.AsLinearColor());
			break;
		}
		case EMaterialParameterType::Texture:
		{
			Instance->SetTextureParameterValueEditorOnly(Change.Parameter.Info, Cast<UTexture>(Change.Value.AsTextureObject()));
			break;
		}
		case EMaterialParameterType::StaticSwitch:
		{
			UMaterialEditingLibrary::SetMaterialInstanceStaticSwitchParameterValue(Instance, Change.Parameter.Info.Name, Change.Value.AsStaticSwitch(), EMaterialParameterAssociation::GlobalParameter, false);
			break;
		}
		default:
		{
			break;
		}
		}
	}

	bool HasStaticSwitch(const TArray<FParameterChange>& Changes)
	{
		return Changes.ContainsByPredicate([](const FParameterChange& Change) { return Change.Parameter.Type == EMaterialParameterType::StaticSwitch; });
	}

	FString DescribeMaterial(const UMaterialInterface* Material)
	{
		TArray<FString> Lines;
		Lines.Add(FShuttlegentToolObjects::Describe(Material));

		const UMaterialInstance* Instance = Cast<UMaterialInstance>(Material);
		if (IsValid(Instance))
		{
			TArray<FString> Chain;
			const UMaterialInterface* Parent = Instance->Parent;
			while (IsValid(Parent))
			{
				Chain.Add(Parent->GetPathName());
				const UMaterialInstance* ParentInstance = Cast<UMaterialInstance>(Parent);
				Parent = IsValid(ParentInstance) ? ParentInstance->Parent.Get() : nullptr;
			}
			Lines.Add(FString::Printf(TEXT("Parent chain: %s"), Chain.IsEmpty() ? NoneValue : *FString::Join(Chain, TEXT(" -> "))));
		}

		const TArray<FMaterialParameter> Parameters = CollectParameters(Material);
		Lines.Add(FString::Printf(TEXT("Parameters (%d), as [group] name (type) = value%s:"), Parameters.Num(), IsValid(Instance) ? TEXT(", overridden here or inherited") : TEXT("; these are the defaults instances inherit")));
		for (const FMaterialParameter& Parameter : Parameters)
		{
			const FString Group = Parameter.Meta.Group.IsNone() ? FString() : FString::Printf(TEXT("[%s] "), *Parameter.Meta.Group.ToString());
			const FString Source = !IsValid(Instance) ? FString() : FString(Parameter.Meta.bOverride ? TEXT(" (overridden)") : TEXT(" (inherited)"));
			Lines.Add(FString::Printf(TEXT("- %s%s (%s) = %s%s"), *Group, *Parameter.Info.Name.ToString(), *DescribeType(Parameter.Type), *FormatValue(Parameter.Meta.Value), *Source));
		}
		return FString::Join(Lines, TEXT("\n"));
	}

	bool IsSlotOverridden(const UMeshComponent* Component, int32 Index)
	{
		return Component->OverrideMaterials.IsValidIndex(Index) && IsValid(Component->OverrideMaterials[Index]);
	}

	FString DescribeSlotName(const UMeshComponent* Component, int32 Index)
	{
		const TArray<FName> SlotNames = Component->GetMaterialSlotNames();
		return SlotNames.IsValidIndex(Index) ? SlotNames[Index].ToString() : FString();
	}

	FString DescribeActorMaterials(const AActor* Actor)
	{
		TArray<FString> Lines;
		Lines.Add(FShuttlegentToolObjects::Describe(Actor));

		TArray<UMeshComponent*> Components;
		Actor->GetComponents<UMeshComponent>(Components);
		if (Components.IsEmpty()) Lines.Add(TEXT("No mesh components."));

		for (const UMeshComponent* Component : Components)
		{
			Lines.Add(FString::Printf(TEXT("- %s (%s) %s"), *Component->GetName(), *Component->GetClass()->GetName(), *Component->GetPathName()));
			for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
			{
				const UMaterialInterface* Material = Component->GetMaterial(Index);
				const FString MaterialPath = IsValid(Material) ? Material->GetPathName() : FString(NoneValue);
				Lines.Add(FString::Printf(TEXT("  slot %d %s = %s (%s)"), Index, *DescribeSlotName(Component, Index), *MaterialPath, IsSlotOverridden(Component, Index) ? TEXT("component override") : TEXT("mesh default")));
			}
		}
		Lines.Add(TEXT("Read a material's parameters by passing its path."));
		return FString::Join(Lines, TEXT("\n"));
	}

	class FGetMaterialParameters : public FShuttlegentToolBase
	{
	public:
		FGetMaterialParameters()
		{
			Definition.Name = GetParametersName;
			Definition.Description = TEXT("Every parameter of materials or material instances in one call: name, group, type (scalar, vector, texture, static switch), current value and, for instances, whether it is overridden or inherited; plus the parent chain. Pass actors instead to list the materials on each slot of their mesh components. Prefer this over reading parameters one by one.");
			Definition.InputSchema = FShuttlegentToolSchema()
				.StringArray(ArgTargets, TEXT("Material or material instance asset paths, actor labels or paths in the open level, or mesh component paths."), true)
				.Build();
		}

		virtual FShuttlegentToolResult Run(const TSharedRef<FJsonObject>& Arguments) override
		{
			const TArray<FString> Targets = FShuttlegentToolArguments::GetStringArray(Arguments, ArgTargets);
			if (Targets.IsEmpty()) return FShuttlegentToolResult::Error(TEXT("Give targets."));

			TArray<FString> Sections;
			for (const FString& Target : Targets)
			{
				const UObject* Object = FShuttlegentToolObjects::ResolveAsset(Target);
				const UMaterialInterface* Material = Cast<UMaterialInterface>(Object);
				const AActor* Actor = Cast<AActor>(Object);
				const UMeshComponent* Component = Cast<UMeshComponent>(Object);
				if (IsValid(Material)) Sections.Add(DescribeMaterial(Material));
				else if (IsValid(Component)) Sections.Add(DescribeActorMaterials(Component->GetOwner()));
				else if (IsValid(Actor)) Sections.Add(DescribeActorMaterials(Actor));
				else Sections.Add(FString::Printf(TEXT("%s: not a material, material instance, actor or mesh component."), *Target));
			}
			return FShuttlegentToolResult::Ok(FString::Join(Sections, TEXT("\n\n")));
		}
	};

	class FSetMaterialParameters : public FShuttlegentToolBase
	{
	public:
		FSetMaterialParameters()
		{
			Definition.Name = SetParametersName;
			Definition.Description = TEXT("Change several parameters on one or more material instances in one step, e.g. the same color on eight instances; the user confirms one card listing every instance and parameter with its current and planned value. Values: scalar as a number; vector as [r, g, b] or [r, g, b, a] (linear 0-1) or \"#RRGGBB\" (sRGB); texture as an asset path, or \"None\"; static switch as true / false (recompiles the material). Base materials cannot be changed this way: create instances with create_material_instances. Runs as one undoable step, rolled back as a whole if any instance fails. Prefer this over changing parameters one at a time.");
			Definition.bReadOnly = false;
			Definition.InputSchema = FShuttlegentToolSchema()
				.StringArray(ArgTargets, TEXT("Material instance asset paths."), true)
				.Object(ArgParameters, TEXT("Parameter name to new value, e.g. {\"BaseColor\": [1, 0, 0], \"Roughness\": 0.4}. Every instance must have every parameter."), true)
				.Build();
		}

		virtual bool Preview(const TSharedRef<FJsonObject>& Arguments, FShuttlegentWriteRequest& OutRequest, FString& OutError) override
		{
			const TArray<FString> Targets = FShuttlegentToolArguments::GetStringArray(Arguments, ArgTargets);
			const TSharedPtr<FJsonObject> Values = GetParametersArgument(Arguments);
			if (Targets.IsEmpty() || !Values.IsValid() || Values->Values.IsEmpty())
			{
				OutError = TEXT("Give targets and parameters.");
				return false;
			}

			bool bStaticSwitch = false;
			for (const FString& Target : Targets)
			{
				UObject* Object = FShuttlegentToolObjects::ResolveAsset(Target);
				UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(Object);
				if (IsValid(Cast<UMaterial>(Object)))
				{
					OutError = FString::Printf(TEXT("%s is a base material: its parameters are defaults in the material graph. Create instances with create_material_instances and change those; nothing was changed."), *Target);
					return false;
				}
				if (!IsValid(Instance))
				{
					OutError = FString::Printf(TEXT("No material instance asset found for %s; nothing was changed."), *Target);
					return false;
				}

				TArray<FParameterChange> Changes;
				if (!PlanChanges(Instance, Values, Changes, OutError)) return false;

				bStaticSwitch = bStaticSwitch || HasStaticSwitch(Changes);
				for (const FParameterChange& Change : Changes)
				{
					FShuttlegentWriteObject& Item = OutRequest.Objects.AddDefaulted_GetRef();
					Item.Name = Instance->GetName();
					Item.Path = Instance->GetPathName();
					Item.Field = Change.Parameter.Info.Name.ToString();
					Item.CurrentValue = FormatValue(Change.Parameter.Meta.Value);
					if (!Change.Parameter.Meta.bOverride) Item.CurrentValue += LOCTEXT("Inherited", "（继承自父材质）").ToString();
					Item.PlannedValue = FormatValue(Change.Value);
				}
			}

			OutRequest.Action = bStaticSwitch ? LOCTEXT("SetParametersStaticAction", "修改材质实例参数（含静态开关，将重新编译材质）").ToString() : LOCTEXT("SetParametersAction", "修改材质实例参数").ToString();
			return true;
		}

		virtual void Execute(const TSharedRef<FJsonObject>& Arguments, TArray<FShuttlegentWriteObject>& InOutObjects) override
		{
			const TSharedPtr<FJsonObject> Values = GetParametersArgument(Arguments);
			TArray<UMaterialInstanceConstant*> Changed;
			for (FShuttlegentWriteObject& Item : InOutObjects)
			{
				UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(FShuttlegentToolObjects::ResolveAsset(Item.Path));
				const TArray<FMaterialParameter> Parameters = IsValid(Instance) ? CollectParameters(Instance) : TArray<FMaterialParameter>();
				const FMaterialParameter* Parameter = FindParameter(Parameters, Item.Field);
				FParameterChange Change;
				FString Error;
				if (Parameter == nullptr)
				{
					Item.Status = EShuttlegentWriteObjectStatus::Failed;
					Item.Message = LOCTEXT("ParameterGone", "材质实例或参数已不存在").ToString();
					continue;
				}

				Change.Parameter = *Parameter;
				if (!ParseValue(*Parameter, FindArgumentValue(Values, Item.Field), Change.Value, Error))
				{
					Item.Status = EShuttlegentWriteObjectStatus::Failed;
					Item.Message = Error;
					continue;
				}

				if (!Changed.Contains(Instance))
				{
					Instance->Modify();
					Changed.Add(Instance);
				}
				ApplyChange(Instance, Change);
				Item.Status = EShuttlegentWriteObjectStatus::Completed;
			}

			// 每个实例只刷新一次：重建静态排列、通知材质实例编辑器并重绘视口
			for (UMaterialInstanceConstant* Instance : Changed)
			{
				UMaterialEditingLibrary::UpdateMaterialInstance(Instance);
			}
		}
	};

	class FCreateMaterialInstances : public FShuttlegentToolBase
	{
	public:
		FCreateMaterialInstances()
		{
			Definition.Name = CreateInstancesName;
			Definition.Description = TEXT("Create several material instance assets of one parent material in one step, optionally with parameter values (same value format as set_material_parameters). Fails if an asset of that name already exists. New assets cannot be undone from the editor; they stay unsaved until save_assets. Prefer this over creating instances one at a time.");
			Definition.bReadOnly = false;
			Definition.bTransactional = false;
			Definition.InputSchema = FShuttlegentToolSchema()
				.String(ArgParent, TEXT("Parent material or material instance asset path."), true)
				.StringArray(ArgNames, TEXT("Asset names of the new instances, e.g. MI_Wall_Red."), true)
				.String(ArgFolder, TEXT("Package folder for the new assets, e.g. /Game/Materials/Walls. Defaults to the parent's folder."))
				.Object(ArgParameters, TEXT("Parameter name to value, set on every new instance."))
				.Build();
		}

		virtual bool Preview(const TSharedRef<FJsonObject>& Arguments, FShuttlegentWriteRequest& OutRequest, FString& OutError) override
		{
			const FString ParentPath = FShuttlegentToolArguments::GetString(Arguments, ArgParent);
			const TArray<FString> Names = FShuttlegentToolArguments::GetStringArray(Arguments, ArgNames);
			UMaterialInterface* Parent = Cast<UMaterialInterface>(FShuttlegentToolObjects::ResolveAsset(ParentPath));
			if (!IsValid(Parent))
			{
				OutError = FString::Printf(TEXT("No material or material instance found for %s; nothing was created."), *ParentPath);
				return false;
			}
			if (Names.IsEmpty())
			{
				OutError = TEXT("Give names for the new instances.");
				return false;
			}

			TArray<FParameterChange> Changes;
			if (!PlanChanges(Parent, GetParametersArgument(Arguments), Changes, OutError)) return false;

			TArray<FString> Planned;
			Planned.Add(FText::Format(LOCTEXT("PlannedParent", "父材质 {0}"), FText::FromString(Parent->GetName())).ToString());
			for (const FParameterChange& Change : Changes)
			{
				Planned.Add(FString::Printf(TEXT("%s = %s"), *Change.Parameter.Info.Name.ToString(), *FormatValue(Change.Value)));
			}

			const FString Folder = GetFolder(Arguments, Parent);
			for (const FString& Name : Names)
			{
				const FString PackageName = Folder + PackageSeparator + Name;
				FText Reason;
				if (!FPackageName::IsValidLongPackageName(PackageName, false, &Reason))
				{
					OutError = FString::Printf(TEXT("%s is not a valid asset path (%s); nothing was created."), *PackageName, *Reason.ToString());
					return false;
				}
				if (FPackageName::DoesPackageExist(PackageName) || IsValid(FindPackage(nullptr, *PackageName)) || OutRequest.Objects.ContainsByPredicate([&PackageName](const FShuttlegentWriteObject& Item) { return Item.Path == PackageName; }))
				{
					OutError = FString::Printf(TEXT("An asset named %s already exists or is listed twice; nothing was created."), *PackageName);
					return false;
				}

				FShuttlegentWriteObject& Item = OutRequest.Objects.AddDefaulted_GetRef();
				Item.Name = Name;
				Item.Path = PackageName;
				Item.Field = LOCTEXT("CreateField", "材质实例").ToString();
				Item.CurrentValue = LOCTEXT("NotCreated", "（新建）").ToString();
				Item.PlannedValue = FString::Join(Planned, TEXT("; "));
			}

			OutRequest.Action = HasStaticSwitch(Changes) ? LOCTEXT("CreateStaticAction", "新建材质实例（含静态开关，将编译材质）").ToString() : LOCTEXT("CreateAction", "新建材质实例").ToString();
			return true;
		}

		virtual void Execute(const TSharedRef<FJsonObject>& Arguments, TArray<FShuttlegentWriteObject>& InOutObjects) override
		{
			UMaterialInterface* Parent = Cast<UMaterialInterface>(FShuttlegentToolObjects::ResolveAsset(FShuttlegentToolArguments::GetString(Arguments, ArgParent)));
			TArray<FParameterChange> Changes;
			FString Error;
			const bool bPlanned = IsValid(Parent) && PlanChanges(Parent, GetParametersArgument(Arguments), Changes, Error);
			IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();
			for (FShuttlegentWriteObject& Item : InOutObjects)
			{
				if (!bPlanned)
				{
					Item.Status = EShuttlegentWriteObjectStatus::Failed;
					Item.Message = Error.IsEmpty() ? LOCTEXT("ParentGone", "父材质已不存在").ToString() : Error;
					continue;
				}

				UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
				Factory->InitialParent = Parent;
				UObject* Asset = AssetTools.CreateAsset(FPackageName::GetShortName(Item.Path), FPackageName::GetLongPackagePath(Item.Path), UMaterialInstanceConstant::StaticClass(), Factory);
				UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(Asset);
				if (!IsValid(Instance))
				{
					Item.Status = EShuttlegentWriteObjectStatus::Failed;
					Item.Message = LOCTEXT("CreateFailed", "无法在该路径新建资源").ToString();
					continue;
				}

				for (const FParameterChange& Change : Changes)
				{
					ApplyChange(Instance, Change);
				}
				UMaterialEditingLibrary::UpdateMaterialInstance(Instance);
				Item.Path = Instance->GetPathName();
				Item.Status = EShuttlegentWriteObjectStatus::Completed;
			}
		}

	private:
		static FString GetFolder(const TSharedRef<FJsonObject>& Arguments, const UMaterialInterface* Parent)
		{
			FString Folder = FShuttlegentToolArguments::GetString(Arguments, ArgFolder);
			if (Folder.IsEmpty()) Folder = FPackageName::GetLongPackagePath(Parent->GetOutermost()->GetName());
			Folder.RemoveFromEnd(PackageSeparator);
			return Folder;
		}
	};

	/** assign_material 选中的材质槽：targets 中各 Actor 的网格体组件（或直接给出的组件）上，slots 所指的槽；slots 为空时取全部槽。 */
	bool CollectSlots(const TSharedRef<FJsonObject>& Arguments, TArray<FSlotTarget>& OutSlots, FString& OutError)
	{
		const TArray<FString> Targets = FShuttlegentToolArguments::GetStringArray(Arguments, ArgTargets);
		const TArray<FString> Slots = FShuttlegentToolArguments::GetStringArray(Arguments, ArgSlots);
		TSet<FString> MatchedSlots;
		for (const FString& Target : Targets)
		{
			UObject* Object = FShuttlegentToolObjects::ResolveObject(Target);
			AActor* Actor = Cast<AActor>(Object);
			UMeshComponent* Single = Cast<UMeshComponent>(Object);
			TArray<UMeshComponent*> Components;
			if (IsValid(Actor)) Actor->GetComponents<UMeshComponent>(Components);
			else if (IsValid(Single)) Components.Add(Single);

			if (Components.IsEmpty())
			{
				OutError = FString::Printf(TEXT("%s is not an actor with mesh components or a mesh component of the open level; nothing was changed."), *Target);
				return false;
			}

			for (UMeshComponent* Component : Components)
			{
				for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
				{
					const FString SlotName = DescribeSlotName(Component, Index);
					const FString* Matched = Slots.FindByPredicate([Index, &SlotName](const FString& Slot) { return Slot == FString::FromInt(Index) || (!SlotName.IsEmpty() && Slot.Equals(SlotName, ESearchCase::IgnoreCase)); });
					if (!Slots.IsEmpty() && Matched == nullptr) continue;

					if (Matched != nullptr) MatchedSlots.Add(*Matched);
					FSlotTarget& Slot = OutSlots.AddDefaulted_GetRef();
					Slot.Component = Component;
					Slot.Index = Index;
				}
			}
		}

		for (const FString& Slot : Slots)
		{
			if (MatchedSlots.Contains(Slot)) continue;

			OutError = FString::Printf(TEXT("No mesh component of the targets has slot %s; nothing was changed. Slots are given by index (0) or slot name; get_material_parameters lists them."), *Slot);
			return false;
		}
		if (OutSlots.IsEmpty()) OutError = TEXT("The targets have no material slots; nothing was changed.");
		return !OutSlots.IsEmpty();
	}

	class FAssignMaterial : public FShuttlegentToolBase
	{
	public:
		FAssignMaterial()
		{
			Definition.Name = AssignMaterialName;
			Definition.Description = TEXT("Put a material or material instance on placed actors in the open level, as a per-component override like the Details panel's Materials list. Only those actors change: the mesh asset and every other placement of it keep their materials. Without slots, every slot of every mesh component of the targets is set. Runs as one undoable step, rolled back as a whole if any slot fails.");
			Definition.bReadOnly = false;
			Definition.InputSchema = FShuttlegentToolSchema()
				.StringArray(ArgTargets, TEXT("Actor labels or paths in the open level, or mesh component paths."), true)
				.String(ArgMaterial, TEXT("Material or material instance asset path."), true)
				.StringArray(ArgSlots, TEXT("Slots to set, by index (\"0\") or slot name, as get_material_parameters lists them. Defaults to every slot."))
				.Build();
		}

		virtual bool Preview(const TSharedRef<FJsonObject>& Arguments, FShuttlegentWriteRequest& OutRequest, FString& OutError) override
		{
			const FString MaterialPath = FShuttlegentToolArguments::GetString(Arguments, ArgMaterial);
			const UMaterialInterface* Material = Cast<UMaterialInterface>(FShuttlegentToolObjects::ResolveAsset(MaterialPath));
			if (!IsValid(Material))
			{
				OutError = FString::Printf(TEXT("No material or material instance found for %s; nothing was changed."), *MaterialPath);
				return false;
			}

			TArray<FSlotTarget> Slots;
			if (!CollectSlots(Arguments, Slots, OutError)) return false;

			OutRequest.Action = FText::Format(LOCTEXT("AssignAction", "替换材质为 {0}"), FText::FromString(Material->GetName())).ToString();
			for (const FSlotTarget& Slot : Slots)
			{
				const UMeshComponent* Component = Slot.Component.Get();
				const UMaterialInterface* Current = Component->GetMaterial(Slot.Index);
				const AActor* Owner = Component->GetOwner();

				FShuttlegentWriteObject& Item = OutRequest.Objects.AddDefaulted_GetRef();
				Item.Name = FString::Printf(TEXT("%s.%s"), *FShuttlegentToolObjects::GetDisplayName(Owner), *Component->GetName());
				Item.Path = Component->GetPathName();
				Item.Field = FText::Format(LOCTEXT("SlotField", "材质槽 {0} {1}"), FText::AsNumber(Slot.Index), FText::FromString(DescribeSlotName(Component, Slot.Index))).ToString();
				Item.CurrentValue = IsValid(Current) ? Current->GetPathName() : FString(NoneValue);
				if (!IsSlotOverridden(Component, Slot.Index)) Item.CurrentValue += LOCTEXT("MeshDefault", "（网格体默认）").ToString();
				Item.PlannedValue = Material->GetPathName();
				if (OutRequest.Level.IsEmpty()) OutRequest.Level = FShuttlegentToolObjects::GetLevelName(Component);
			}
			return true;
		}

		virtual void Execute(const TSharedRef<FJsonObject>& Arguments, TArray<FShuttlegentWriteObject>& InOutObjects) override
		{
			UMaterialInterface* Material = Cast<UMaterialInterface>(FShuttlegentToolObjects::ResolveAsset(FShuttlegentToolArguments::GetString(Arguments, ArgMaterial)));
			TArray<FSlotTarget> Slots;
			FString Error;
			CollectSlots(Arguments, Slots, Error);
			FProperty* OverrideProperty = FindFProperty<FProperty>(UMeshComponent::StaticClass(), GET_MEMBER_NAME_CHECKED(UMeshComponent, OverrideMaterials));

			// 按预览时的顺序逐槽写入；关卡在确认期间变化、槽与预览不再对应时该槽失败
			for (int32 Index = 0; Index < InOutObjects.Num(); ++Index)
			{
				FShuttlegentWriteObject& Item = InOutObjects[Index];
				UMeshComponent* Component = Slots.IsValidIndex(Index) ? Slots[Index].Component.Get() : nullptr;
				if (!IsValid(Material) || !IsValid(Component) || Component->GetPathName() != Item.Path)
				{
					Item.Status = EShuttlegentWriteObjectStatus::Failed;
					Item.Message = LOCTEXT("SlotGone", "材质、组件或材质槽已变化").ToString();
					continue;
				}

				Component->Modify();
				Component->PreEditChange(OverrideProperty);
				Component->SetMaterial(Slots[Index].Index, Material);
				FPropertyChangedEvent ChangedEvent(OverrideProperty, EPropertyChangeType::ValueSet);
				Component->PostEditChangeProperty(ChangedEvent);
				Item.Status = EShuttlegentWriteObjectStatus::Completed;
			}
		}
	};
}

void FShuttlegentMaterialTools::AppendRead(TArray<TSharedRef<IShuttlegentTool>>& OutTools)
{
	OutTools.Add(MakeShared<FShuttlegentMaterialToolsPrivate::FGetMaterialParameters>());
}

void FShuttlegentMaterialTools::AppendWrite(TArray<TSharedRef<IShuttlegentTool>>& OutTools)
{
	OutTools.Add(MakeShared<FShuttlegentMaterialToolsPrivate::FSetMaterialParameters>());
	OutTools.Add(MakeShared<FShuttlegentMaterialToolsPrivate::FCreateMaterialInstances>());
	OutTools.Add(MakeShared<FShuttlegentMaterialToolsPrivate::FAssignMaterial>());
}

#undef LOCTEXT_NAMESPACE
