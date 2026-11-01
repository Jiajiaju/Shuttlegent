#include "Graph/FShuttlegentGraphAdapters.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/RigVMEdGraphNode.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "GraphEditor.h"
#include "IMaterialEditor.h"
#include "Layout/SlateRect.h"
#include "MaterialGraph/MaterialGraph.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialFunction.h"
#include "RigVMEditorAsset.h"
#include "RigVMModel/Nodes/RigVMCommentNode.h"
#include "RigVMModel/RigVMClient.h"
#include "RigVMModel/RigVMController.h"
#include "RigVMModel/RigVMGraph.h"
#include "RigVMModel/RigVMLink.h"
#include "RigVMModel/RigVMNode.h"
#include "RigVMModel/RigVMPin.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/UObjectHash.h"

#include "Graph/FShuttlegentLayoutGraph.h"
#include "Graph/IShuttlegentGraphAdapter.h"

namespace FShuttlegentGraphAdaptersPrivate
{
	// 材质节点图的名称；材质只有一张图
	inline const TCHAR* MaterialGraphName = TEXT("MaterialGraph");

	// 材质输出节点的对象名，供 nodes 参数匹配
	inline const TCHAR* MaterialRootName = TEXT("MaterialRoot");

	// RigVM 撤销记录的标题
	inline const TCHAR* RigVMUndoTitle = TEXT("Tidy Graph");

	// 无法取得实际绘制大小时的估算：每个标题字符的宽度、标题两侧留白、最小宽度、标题栏高度、每行引脚高度与底部留白（节点图单位）
	inline constexpr double CharWidth = 7.0;
	inline constexpr double TitlePadding = 48.0;
	inline constexpr double MinNodeWidth = 120.0;
	inline constexpr double HeaderHeight = 32.0;
	inline constexpr double PinRowHeight = 26.0;
	inline constexpr double FooterPadding = 12.0;

	// 材质节点估算高度时的底部留白（节点图单位）；标题栏与每行引脚的高度沿用引擎的 ME_CAPTION_HEIGHT、ME_STD_TAB_HEIGHT
	inline constexpr double MaterialFooterPadding = 16.0;

	// 位置是否变化的容差（节点图单位）
	inline constexpr double MoveTolerance = 0.5;

	/** 引脚在 Count 个同向引脚中第 Index 个时的相对高度，取该行中点。 */
	float PinFraction(int32 Index, int32 Count)
	{
		return Count > 0 && Index >= 0 ? (static_cast<float>(Index) + 0.5f) / static_cast<float>(Count) : 0.5f;
	}

	FVector2D EstimateSize(const FString& Title, int32 Rows)
	{
		const double Width = FMath::Max(MinNodeWidth, Title.Len() * CharWidth + TitlePadding);
		return FVector2D(Width, HeaderHeight + FMath::Max(Rows, 1) * PinRowHeight + FooterPadding);
	}

	bool HasMoved(const FVector2D& Planned, const FVector2D& Current)
	{
		return !Planned.Equals(Current, MoveTolerance);
	}

	/** 资源已在材质编辑器中打开时的编辑器；材质编辑器编辑的是资源的副本。 */
	IMaterialEditor* FindMaterialEditor(UObject* Asset)
	{
		if (!IsValid(Asset) || GEditor == nullptr) return nullptr;

		UAssetEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		IAssetEditorInstance* Instance = IsValid(Subsystem) ? Subsystem->FindEditorForAsset(Asset, false) : nullptr;
		return Instance != nullptr ? static_cast<IMaterialEditor*>(Instance) : nullptr;
	}

	/** 材质编辑器正在编辑的节点图；未打开时为 nullptr。 */
	UEdGraph* FindMaterialEditorGraph(UObject* Asset)
	{
		IMaterialEditor* Editor = FindMaterialEditor(Asset);
		UMaterial* WorkingCopy = Editor != nullptr ? Cast<UMaterial>(Editor->GetMaterialInterface()) : nullptr;
		return IsValid(WorkingCopy) ? WorkingCopy->MaterialGraph.Get() : nullptr;
	}

	TArray<UEdGraphPin*> VisiblePins(const UEdGraphNode* Node, EEdGraphPinDirection Direction)
	{
		TArray<UEdGraphPin*> Pins;
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin != nullptr && !Pin->bHidden && Pin->Direction == Direction) Pins.Add(Pin);
		}
		return Pins;
	}

	/** 节点即数据的 UEdGraph：蓝图图表、材质编辑器中的工作图，以及其他以 UEdGraph 保存节点的资源。 */
	class FEdGraphAdapter : public IShuttlegentGraphAdapter
	{
	private:
		TWeakObjectPtr<UEdGraph> Graph = nullptr;
		FString Name;
		TFunction<void()> OnWritten;
		TArray<TWeakObjectPtr<UEdGraphNode>> Nodes;
		TArray<TWeakObjectPtr<UEdGraphNode_Comment>> Comments;
		bool bRenderedSizes = false;

	public:
		FEdGraphAdapter(UEdGraph* InGraph, const FString& InName, TFunction<void()> InOnWritten)
			: Graph(InGraph)
			, Name(InName)
			, OnWritten(MoveTemp(InOnWritten))
		{
		}

		//~ Begin IShuttlegentGraphAdapter Interface.
		virtual FString GetGraphName() const override { return Name; }
		virtual bool HasRenderedSizes() const override { return bRenderedSizes; }

		virtual void Read(FShuttlegentLayoutGraph& OutGraph) override
		{
			Nodes.Reset();
			Comments.Reset();
			OutGraph = FShuttlegentLayoutGraph();
			UEdGraph* EdGraph = Graph.Get();
			if (!IsValid(EdGraph)) return;

			const TSharedPtr<SGraphEditor> Editor = SGraphEditor::FindGraphEditorForGraph(EdGraph);
			const bool bK2 = IsValid(Cast<const UEdGraphSchema_K2>(EdGraph->GetSchema()));
			bRenderedSizes = Editor.IsValid();

			TMap<const UEdGraphNode*, int32> Index;
			for (UEdGraphNode* Node : EdGraph->Nodes)
			{
				if (!IsValid(Node)) continue;

				UEdGraphNode_Comment* Comment = Cast<UEdGraphNode_Comment>(Node);
				if (IsValid(Comment))
				{
					FShuttlegentLayoutComment& Layout = OutGraph.Comments.AddDefaulted_GetRef();
					Layout.Position = FVector2D(Comment->NodePosX, Comment->NodePosY);
					Layout.Size = FVector2D(Comment->NodeWidth, Comment->NodeHeight);
					Comments.Add(Comment);
					continue;
				}

				FShuttlegentLayoutNode& Layout = OutGraph.Nodes.AddDefaulted_GetRef();
				Layout.Name = Node->GetName();
				Layout.Title = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
				Layout.Position = FVector2D(Node->NodePosX, Node->NodePosY);
				Layout.Size = Measure(Editor, Node, bK2, Layout.Title);
				Index.Add(Node, Nodes.Num());
				Nodes.Add(Node);
			}

			for (int32 NodeIndex = 0; NodeIndex < Nodes.Num(); ++NodeIndex)
			{
				const TArray<UEdGraphPin*> Outputs = VisiblePins(Nodes[NodeIndex].Get(), EGPD_Output);
				for (int32 OutputIndex = 0; OutputIndex < Outputs.Num(); ++OutputIndex)
				{
					for (const UEdGraphPin* Linked : Outputs[OutputIndex]->LinkedTo)
					{
						const int32* Target = Linked != nullptr ? Index.Find(Linked->GetOwningNode()) : nullptr;
						if (Target == nullptr) continue;

						const TArray<UEdGraphPin*> Inputs = VisiblePins(Linked->GetOwningNode(), EGPD_Input);
						FShuttlegentLayoutEdge& Edge = OutGraph.Edges.AddDefaulted_GetRef();
						Edge.Source = NodeIndex;
						Edge.Target = *Target;
						Edge.SourcePin = PinFraction(OutputIndex, Outputs.Num());
						Edge.TargetPin = PinFraction(Inputs.IndexOfByKey(Linked), Inputs.Num());
					}
				}
			}
		}

		virtual void Write(const FShuttlegentLayoutGraph& Layout) override
		{
			bool bChanged = false;
			for (int32 Index = 0; Index < Nodes.Num() && Index < Layout.Nodes.Num(); ++Index)
			{
				UEdGraphNode* Node = Nodes[Index].Get();
				const FShuttlegentLayoutNode& Planned = Layout.Nodes[Index];
				if (!IsValid(Node) || !Planned.bArrange || !HasMoved(Planned.Position, FVector2D(Node->NodePosX, Node->NodePosY))) continue;

				Node->Modify();
				Node->NodePosX = FMath::RoundToInt32(Planned.Position.X);
				Node->NodePosY = FMath::RoundToInt32(Planned.Position.Y);
				bChanged = true;
			}

			for (int32 Index = 0; Index < Comments.Num() && Index < Layout.Comments.Num(); ++Index)
			{
				UEdGraphNode_Comment* Comment = Comments[Index].Get();
				const FShuttlegentLayoutComment& Planned = Layout.Comments[Index];
				const bool bMoved = IsValid(Comment) && (HasMoved(Planned.Position, FVector2D(Comment->NodePosX, Comment->NodePosY)) || HasMoved(Planned.Size, FVector2D(Comment->NodeWidth, Comment->NodeHeight)));
				if (!bMoved) continue;

				Comment->Modify();
				Comment->NodePosX = FMath::RoundToInt32(Planned.Position.X);
				Comment->NodePosY = FMath::RoundToInt32(Planned.Position.Y);
				Comment->NodeWidth = FMath::RoundToInt32(Planned.Size.X);
				Comment->NodeHeight = FMath::RoundToInt32(Planned.Size.Y);
				bChanged = true;
			}

			UEdGraph* EdGraph = Graph.Get();
			if (!bChanged || !IsValid(EdGraph)) return;

			EdGraph->NotifyGraphChanged();
			if (OnWritten) OnWritten();
		}
		//~ End IShuttlegentGraphAdapter Interface.

	private:
		/** 已打开编辑器中实际绘制的大小；取不到时依次用节点保存的大小、蓝图的估算、按标题与引脚数的估算。 */
		FVector2D Measure(const TSharedPtr<SGraphEditor>& Editor, UEdGraphNode* Node, bool bK2, const FString& Title)
		{
			FSlateRect Rect;
			if (Editor.IsValid() && Editor->GetBoundsForNode(Node, Rect, 0.0f) && Rect.GetSize().X > 0.0 && Rect.GetSize().Y > 0.0) return FVector2D(Rect.GetSize());

			bRenderedSizes = false;
			if (Node->NodeWidth > 0 && Node->NodeHeight > 0) return FVector2D(Node->NodeWidth, Node->NodeHeight);
			if (bK2) return FVector2D(UEdGraphSchema_K2::EstimateNodeWidth(Node), UEdGraphSchema_K2::EstimateNodeHeight(Node));

			const int32 Rows = FMath::Max(VisiblePins(Node, EGPD_Input).Num(), VisiblePins(Node, EGPD_Output).Num());
			return EstimateSize(Title, Rows);
		}
	};

	/** 未在材质编辑器中打开的材质或材质函数：直接读写材质节点保存的位置，尺寸为估算。 */
	class FMaterialAssetAdapter : public IShuttlegentGraphAdapter
	{
	private:
		TWeakObjectPtr<UObject> Asset = nullptr;
		TArray<TWeakObjectPtr<UMaterialExpression>> Expressions;
		TArray<TWeakObjectPtr<UMaterialExpressionComment>> Comments;
		int32 RootIndex = INDEX_NONE;

	public:
		explicit FMaterialAssetAdapter(UObject* InAsset)
			: Asset(InAsset)
		{
		}

		//~ Begin IShuttlegentGraphAdapter Interface.
		virtual FString GetGraphName() const override { return MaterialGraphName; }
		virtual bool HasRenderedSizes() const override { return false; }

		virtual void Read(FShuttlegentLayoutGraph& OutGraph) override
		{
			Expressions.Reset();
			Comments.Reset();
			RootIndex = INDEX_NONE;
			OutGraph = FShuttlegentLayoutGraph();

			UMaterial* Material = Cast<UMaterial>(Asset.Get());
			UMaterialFunction* Function = Cast<UMaterialFunction>(Asset.Get());
			if (!IsValid(Material) && !IsValid(Function)) return;

			const TConstArrayView<TObjectPtr<UMaterialExpression>> All = IsValid(Material) ? Material->GetExpressions() : Function->GetExpressions();
			const TConstArrayView<TObjectPtr<UMaterialExpressionComment>> AllComments = IsValid(Material) ? Material->GetEditorComments() : Function->GetEditorComments();

			// 只排顶层节点；复合节点子图中的节点有自己的坐标系
			TMap<const UMaterialExpression*, int32> Index;
			for (UMaterialExpression* Expression : All)
			{
				if (!IsValid(Expression) || Expression->IsA<UMaterialExpressionComment>() || IsValid(Expression->SubgraphExpression)) continue;

				TArray<FString> Captions;
				Expression->GetCaption(Captions);

				FShuttlegentLayoutNode& Layout = OutGraph.Nodes.AddDefaulted_GetRef();
				Layout.Name = Expression->GetName();
				Layout.Title = Captions.IsEmpty() ? Layout.Name : Captions[0];
				Layout.Position = FVector2D(Expression->MaterialExpressionEditorX, Expression->MaterialExpressionEditorY);
				Layout.Size = Measure(Expression, Layout.Title);
				Index.Add(Expression, Expressions.Num());
				Expressions.Add(Expression);
			}

			for (int32 NodeIndex = 0; NodeIndex < Expressions.Num(); ++NodeIndex)
			{
				UMaterialExpression* Expression = Expressions[NodeIndex].Get();
				const int32 InputCount = CountInputs(Expression);
				for (FExpressionInputIterator It{ Expression }; It; ++It)
				{
					const int32* Source = It->Expression != nullptr ? Index.Find(It->Expression) : nullptr;
					if (Source == nullptr) continue;

					FShuttlegentLayoutEdge& Edge = OutGraph.Edges.AddDefaulted_GetRef();
					Edge.Source = *Source;
					Edge.Target = NodeIndex;
					Edge.SourcePin = PinFraction(It->OutputIndex, It->Expression->GetOutputs().Num());
					Edge.TargetPin = PinFraction(It.Index, InputCount);
				}
			}

			if (IsValid(Material)) ReadRoot(Material, Index, OutGraph);

			for (UMaterialExpressionComment* Comment : AllComments)
			{
				if (!IsValid(Comment)) continue;

				FShuttlegentLayoutComment& Layout = OutGraph.Comments.AddDefaulted_GetRef();
				Layout.Position = FVector2D(Comment->MaterialExpressionEditorX, Comment->MaterialExpressionEditorY);
				Layout.Size = FVector2D(Comment->SizeX, Comment->SizeY);
				Comments.Add(Comment);
			}
		}

		virtual void Write(const FShuttlegentLayoutGraph& Layout) override
		{
			for (int32 Index = 0; Index < Expressions.Num() && Index < Layout.Nodes.Num(); ++Index)
			{
				UMaterialExpression* Expression = Expressions[Index].Get();
				const FShuttlegentLayoutNode& Planned = Layout.Nodes[Index];
				if (!IsValid(Expression) || !Planned.bArrange || !HasMoved(Planned.Position, FVector2D(Expression->MaterialExpressionEditorX, Expression->MaterialExpressionEditorY))) continue;

				Expression->Modify();
				Expression->MaterialExpressionEditorX = FMath::RoundToInt32(Planned.Position.X);
				Expression->MaterialExpressionEditorY = FMath::RoundToInt32(Planned.Position.Y);
			}

			UMaterial* Material = Cast<UMaterial>(Asset.Get());
			if (IsValid(Material) && Layout.Nodes.IsValidIndex(RootIndex) && Layout.Nodes[RootIndex].bArrange && HasMoved(Layout.Nodes[RootIndex].Position, FVector2D(Material->EditorX, Material->EditorY)))
			{
				Material->Modify();
				Material->EditorX = FMath::RoundToInt32(Layout.Nodes[RootIndex].Position.X);
				Material->EditorY = FMath::RoundToInt32(Layout.Nodes[RootIndex].Position.Y);
			}

			for (int32 Index = 0; Index < Comments.Num() && Index < Layout.Comments.Num(); ++Index)
			{
				UMaterialExpressionComment* Comment = Comments[Index].Get();
				const FShuttlegentLayoutComment& Planned = Layout.Comments[Index];
				const bool bMoved = IsValid(Comment) && (HasMoved(Planned.Position, FVector2D(Comment->MaterialExpressionEditorX, Comment->MaterialExpressionEditorY)) || HasMoved(Planned.Size, FVector2D(Comment->SizeX, Comment->SizeY)));
				if (!bMoved) continue;

				Comment->Modify();
				Comment->MaterialExpressionEditorX = FMath::RoundToInt32(Planned.Position.X);
				Comment->MaterialExpressionEditorY = FMath::RoundToInt32(Planned.Position.Y);
				Comment->SizeX = FMath::RoundToInt32(Planned.Size.X);
				Comment->SizeY = FMath::RoundToInt32(Planned.Size.Y);
			}
		}
		//~ End IShuttlegentGraphAdapter Interface.

	private:
		static int32 CountInputs(UMaterialExpression* Expression)
		{
			int32 Count = 0;
			for (FExpressionInputIterator It{ Expression }; It; ++It)
			{
				++Count;
			}
			return Count;
		}

		/** 引擎的 GetHeight 只按输出引脚数计算高度，输入多的节点会偏矮，因此按输入与输出中较多的一侧补足。 */
		static FVector2D Measure(UMaterialExpression* Expression, const FString& Title)
		{
			const int32 Rows = FMath::Max(CountInputs(Expression), Expression->GetOutputs().Num());
			const double Width = FMath::Max(static_cast<double>(Expression->GetWidth()), EstimateSize(Title, Rows).X);
			const double Height = FMath::Max(static_cast<double>(Expression->GetHeight()), ME_CAPTION_HEIGHT + Rows * ME_STD_TAB_HEIGHT + MaterialFooterPadding);
			return FVector2D(Width, Height);
		}

		/** 材质输出节点：每个在编辑器中显示的材质属性占一行。 */
		void ReadRoot(UMaterial* Material, const TMap<const UMaterialExpression*, int32>& Index, FShuttlegentLayoutGraph& OutGraph)
		{
			TArray<EMaterialProperty> Properties;
			for (int32 Property = 0; Property < MP_MAX; ++Property)
			{
				if (Material->IsPropertyActiveInEditor(static_cast<EMaterialProperty>(Property))) Properties.Add(static_cast<EMaterialProperty>(Property));
			}

			RootIndex = OutGraph.Nodes.Num();
			FShuttlegentLayoutNode& Root = OutGraph.Nodes.AddDefaulted_GetRef();
			Root.Name = MaterialRootName;
			Root.Title = Material->GetName();
			Root.Position = FVector2D(Material->EditorX, Material->EditorY);
			Root.Size = FVector2D(EstimateSize(Root.Title, 1).X, ME_CAPTION_HEIGHT + Properties.Num() * ME_STD_TAB_HEIGHT + MaterialFooterPadding);

			for (int32 Row = 0; Row < Properties.Num(); ++Row)
			{
				const FExpressionInput* Input = Material->GetExpressionInputForProperty(Properties[Row]);
				const int32* Source = Input != nullptr && Input->Expression != nullptr ? Index.Find(Input->Expression) : nullptr;
				if (Source == nullptr) continue;

				FShuttlegentLayoutEdge& Edge = OutGraph.Edges.AddDefaulted_GetRef();
				Edge.Source = *Source;
				Edge.Target = RootIndex;
				Edge.SourcePin = PinFraction(Input->OutputIndex, Input->Expression->GetOutputs().Num());
				Edge.TargetPin = PinFraction(Row, Properties.Num());
			}
		}
	};

	/** RigVM 资源（Control Rig 等）的一张模型图：位置经 RigVM 控制器写入，撤销随编辑器事务。 */
	class FRigVMGraphAdapter : public IShuttlegentGraphAdapter
	{
	private:
		TWeakObjectPtr<UObject> Asset = nullptr;
		TWeakObjectPtr<URigVMGraph> Model = nullptr;
		TArray<TWeakObjectPtr<URigVMNode>> Nodes;
		TArray<TWeakObjectPtr<URigVMNode>> Comments;
		bool bRenderedSizes = false;

	public:
		FRigVMGraphAdapter(UObject* InAsset, URigVMGraph* InModel)
			: Asset(InAsset)
			, Model(InModel)
		{
		}

		//~ Begin IShuttlegentGraphAdapter Interface.
		virtual FString GetGraphName() const override { return Model.IsValid() ? Model->GetGraphName() : FString(); }
		virtual bool HasRenderedSizes() const override { return bRenderedSizes; }

		virtual void Read(FShuttlegentLayoutGraph& OutGraph) override
		{
			Nodes.Reset();
			Comments.Reset();
			OutGraph = FShuttlegentLayoutGraph();
			URigVMGraph* Graph = Model.Get();
			if (!IsValid(Graph)) return;

			// 编辑器打开时，按模型节点名找到界面上的节点以取实际绘制大小
			IRigVMEditorAssetInterface* EditorAsset = Cast<IRigVMEditorAssetInterface>(Asset.Get());
			UEdGraph* EdGraph = EditorAsset != nullptr ? EditorAsset->GetEdGraph(Graph) : nullptr;
			const TSharedPtr<SGraphEditor> Editor = IsValid(EdGraph) ? SGraphEditor::FindGraphEditorForGraph(EdGraph) : nullptr;
			TMap<FName, UEdGraphNode*> ViewNodes;
			if (IsValid(EdGraph))
			{
				for (UEdGraphNode* Node : EdGraph->Nodes)
				{
					const URigVMEdGraphNode* ViewNode = Cast<URigVMEdGraphNode>(Node);
					if (IsValid(ViewNode)) ViewNodes.Add(ViewNode->GetModelNodeName(), Node);
				}
			}
			bRenderedSizes = Editor.IsValid();

			TMap<const URigVMNode*, int32> Index;
			for (URigVMNode* Node : Graph->GetNodes())
			{
				if (!IsValid(Node) || !Node->IsVisibleInUI()) continue;
				if (Node->IsA<URigVMCommentNode>())
				{
					FShuttlegentLayoutComment& Layout = OutGraph.Comments.AddDefaulted_GetRef();
					Layout.Position = Node->GetPosition();
					Layout.Size = Node->GetSize();
					Comments.Add(Node);
					continue;
				}

				UEdGraphNode** ViewNode = ViewNodes.Find(Node->GetFName());
				FShuttlegentLayoutNode& Layout = OutGraph.Nodes.AddDefaulted_GetRef();
				Layout.Name = Node->GetName();
				Layout.Title = Node->GetNodeTitle();
				Layout.Position = Node->GetPosition();
				Layout.Size = Measure(Editor, ViewNode != nullptr ? *ViewNode : nullptr, Node, Layout.Title);
				Index.Add(Node, Nodes.Num());
				Nodes.Add(Node);
			}

			for (const URigVMLink* Link : Graph->GetLinks())
			{
				const URigVMPin* SourcePin = IsValid(Link) ? Link->GetSourcePin() : nullptr;
				const URigVMPin* TargetPin = IsValid(Link) ? Link->GetTargetPin() : nullptr;
				const int32* Source = IsValid(SourcePin) ? Index.Find(SourcePin->GetNode()) : nullptr;
				const int32* Target = IsValid(TargetPin) ? Index.Find(TargetPin->GetNode()) : nullptr;
				if (Source == nullptr || Target == nullptr) continue;

				FShuttlegentLayoutEdge& Edge = OutGraph.Edges.AddDefaulted_GetRef();
				Edge.Source = *Source;
				Edge.Target = *Target;
				Edge.SourcePin = PinFraction(SourcePin->GetRootPin()->GetPinIndex(), SourcePin->GetNode()->GetPins().Num());
				Edge.TargetPin = PinFraction(TargetPin->GetRootPin()->GetPinIndex(), TargetPin->GetNode()->GetPins().Num());
			}
		}

		virtual void Write(const FShuttlegentLayoutGraph& Layout) override
		{
			IRigVMClientHost* Host = Cast<IRigVMClientHost>(Asset.Get());
			URigVMController* Controller = Host != nullptr && Model.IsValid() ? Host->GetOrCreateController(Model.Get()) : nullptr;
			if (!IsValid(Controller)) return;

			// 一次排版只记一条 RigVM 撤销记录，它随写入门的编辑器事务一并撤销
			Controller->OpenUndoBracket(RigVMUndoTitle);
			for (int32 Index = 0; Index < Nodes.Num() && Index < Layout.Nodes.Num(); ++Index)
			{
				URigVMNode* Node = Nodes[Index].Get();
				const FShuttlegentLayoutNode& Planned = Layout.Nodes[Index];
				if (IsValid(Node) && Planned.bArrange && HasMoved(Planned.Position, Node->GetPosition())) Controller->SetNodePosition(Node, Planned.Position, true, false, false);
			}

			for (int32 Index = 0; Index < Comments.Num() && Index < Layout.Comments.Num(); ++Index)
			{
				URigVMNode* Comment = Comments[Index].Get();
				const FShuttlegentLayoutComment& Planned = Layout.Comments[Index];
				if (!IsValid(Comment)) continue;

				if (HasMoved(Planned.Position, Comment->GetPosition())) Controller->SetNodePosition(Comment, Planned.Position, true, false, false);
				if (HasMoved(Planned.Size, Comment->GetSize())) Controller->SetNodeSize(Comment, Planned.Size, true, false, false);
			}
			Controller->CloseUndoBracket();
		}
		//~ End IShuttlegentGraphAdapter Interface.

	private:
		static int32 CountVisiblePins(const TArray<URigVMPin*>& Pins)
		{
			int32 Count = 0;
			for (const URigVMPin* Pin : Pins)
			{
				if (!IsValid(Pin)) continue;

				++Count;
				if (Pin->IsExpanded()) Count += CountVisiblePins(Pin->GetSubPins());
			}
			return Count;
		}

		/** 已打开编辑器中实际绘制的大小；取不到时用模型保存的大小，再取不到按标题与展开的引脚数估算。 */
		FVector2D Measure(const TSharedPtr<SGraphEditor>& Editor, const UEdGraphNode* ViewNode, const URigVMNode* Node, const FString& Title)
		{
			FSlateRect Rect;
			if (Editor.IsValid() && ViewNode != nullptr && Editor->GetBoundsForNode(ViewNode, Rect, 0.0f) && Rect.GetSize().X > 0.0 && Rect.GetSize().Y > 0.0) return FVector2D(Rect.GetSize());

			bRenderedSizes = false;
			const FVector2D Saved = Node->GetSize();
			if (Saved.X > 0.0 && Saved.Y > 0.0) return Saved;

			return EstimateSize(Title, CountVisiblePins(Node->GetPins()));
		}
	};
}

TArray<TSharedRef<IShuttlegentGraphAdapter>> FShuttlegentGraphAdapters::Collect(UObject* Asset)
{
	TArray<TSharedRef<IShuttlegentGraphAdapter>> Adapters;
	if (!IsValid(Asset)) return Adapters;

	// Control Rig 蓝图同时也是 UBlueprint，其 UEdGraph 只是 RigVM 模型的视图，必须先于蓝图判断
	IRigVMClientHost* Host = Cast<IRigVMClientHost>(Asset);
	if (Host != nullptr)
	{
		for (URigVMGraph* Model : Host->GetAllModels())
		{
			if (IsValid(Model)) Adapters.Add(MakeShared<FShuttlegentGraphAdaptersPrivate::FRigVMGraphAdapter>(Asset, Model));
		}
		return Adapters;
	}

	// 材质编辑器打开时排它的工作图，由编辑器在应用或保存时写回资源；否则直接改资源
	if (IsValid(Cast<UMaterial>(Asset)) || IsValid(Cast<UMaterialFunction>(Asset)))
	{
		UEdGraph* EditorGraph = FShuttlegentGraphAdaptersPrivate::FindMaterialEditorGraph(Asset);
		const TWeakObjectPtr<UObject> WeakAsset = Asset;
		TFunction<void()> OnWritten = [WeakAsset]()
		{
			IMaterialEditor* Editor = FShuttlegentGraphAdaptersPrivate::FindMaterialEditor(WeakAsset.Get());
			if (Editor != nullptr) Editor->UpdateMaterialAfterGraphChange();
		};
		if (IsValid(EditorGraph)) Adapters.Add(MakeShared<FShuttlegentGraphAdaptersPrivate::FEdGraphAdapter>(EditorGraph, FShuttlegentGraphAdaptersPrivate::MaterialGraphName, MoveTemp(OnWritten)));
		else Adapters.Add(MakeShared<FShuttlegentGraphAdaptersPrivate::FMaterialAssetAdapter>(Asset));
		return Adapters;
	}

	TArray<UEdGraph*> Graphs;
	UBlueprint* Blueprint = Cast<UBlueprint>(Asset);
	if (IsValid(Blueprint))
	{
		Blueprint->GetAllGraphs(Graphs);
	}
	else
	{
		TArray<UObject*> Objects;
		GetObjectsWithOuter(Asset->GetOutermost(), Objects, EGetObjectsFlags::IncludeNestedObjects);
		for (UObject* Object : Objects)
		{
			UEdGraph* Graph = Cast<UEdGraph>(Object);
			if (IsValid(Graph)) Graphs.Add(Graph);
		}
	}

	for (UEdGraph* Graph : Graphs)
	{
		if (IsValid(Graph)) Adapters.Add(MakeShared<FShuttlegentGraphAdaptersPrivate::FEdGraphAdapter>(Graph, Graph->GetName(), nullptr));
	}
	return Adapters;
}
