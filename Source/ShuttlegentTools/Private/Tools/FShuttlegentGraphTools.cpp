#include "Tools/FShuttlegentGraphTools.h"

#include "FShuttlegentToolBase.h"
#include "FShuttlegentToolObjects.h"
#include "Graph/FShuttlegentGraphAdapters.h"
#include "Graph/FShuttlegentGraphLayout.h"
#include "Graph/FShuttlegentLayoutGraph.h"
#include "Graph/IShuttlegentGraphAdapter.h"
#include "Tool/FShuttlegentToolArguments.h"
#include "Tool/FShuttlegentToolSchema.h"
#include "WriteGate/FShuttlegentWriteRequest.h"

#define LOCTEXT_NAMESPACE "ShuttlegentGraphTools"

namespace FShuttlegentGraphToolsPrivate
{
	// 工具名称
	inline const TCHAR* TidyGraphName = TEXT("tidy_graph");

	// 参数名称
	inline const TCHAR* ArgAsset = TEXT("asset");
	inline const TCHAR* ArgGraph = TEXT("graph");
	inline const TCHAR* ArgNodes = TEXT("nodes");

	// 列出名称时的分隔符
	inline const TCHAR* NameListSeparator = TEXT(", ");

	// nodes 匹配不上时报错中最多列出的节点数
	inline constexpr int32 MaxListedNodes = 60;

	/** 一张节点图读出的当前状态及其排版结果。 */
	struct FGraphPlan
	{
		TSharedPtr<IShuttlegentGraphAdapter> Adapter = nullptr;
		FShuttlegentLayoutGraph Current {};
		FShuttlegentLayoutGraph Planned {};
		int32 Moved = 0;
	};

	bool MatchesNode(const FShuttlegentLayoutNode& Node, const FString& Name)
	{
		return Node.Name.Equals(Name, ESearchCase::IgnoreCase) || Node.Title.Equals(Name, ESearchCase::IgnoreCase);
	}

	FText DescribeBounds(const FShuttlegentLayoutGraph& Graph)
	{
		const FVector2D Size = FShuttlegentGraphLayout::GetBounds(Graph).GetSize();
		return FText::Format(LOCTEXT("Bounds", "{0} × {1}"), FText::AsNumber(FMath::RoundToInt32(Size.X)), FText::AsNumber(FMath::RoundToInt32(Size.Y)));
	}

	/** 读出 asset 中选中的节点图并在副本上排版；给出 nodes 时只排这些节点，其余节点原地不动。 */
	bool PlanGraphs(const TSharedRef<FJsonObject>& Arguments, UObject*& OutAsset, TArray<FGraphPlan>& OutPlans, FString& OutError)
	{
		const FString Path = FShuttlegentToolArguments::GetString(Arguments, ArgAsset);
		const FString GraphName = FShuttlegentToolArguments::GetString(Arguments, ArgGraph);
		const TArray<FString> Names = FShuttlegentToolArguments::GetStringArray(Arguments, ArgNodes);
		OutAsset = FShuttlegentToolObjects::ResolveAsset(Path);
		if (!IsValid(OutAsset))
		{
			OutError = FString::Printf(TEXT("No asset found at %s; nothing was moved."), *Path);
			return false;
		}

		const TArray<TSharedRef<IShuttlegentGraphAdapter>> Adapters = FShuttlegentGraphAdapters::Collect(OutAsset);
		if (Adapters.IsEmpty())
		{
			OutError = FString::Printf(TEXT("%s has no node graph to arrange; tidy_graph works on Blueprints, materials, material functions, Control Rigs and other node-graph assets."), *OutAsset->GetName());
			return false;
		}

		TArray<FString> GraphNames;
		TArray<FString> NodeLabels;
		TSet<FString> MatchedNames;
		for (const TSharedRef<IShuttlegentGraphAdapter>& Adapter : Adapters)
		{
			GraphNames.Add(Adapter->GetGraphName());
			if (!GraphName.IsEmpty() && !Adapter->GetGraphName().Equals(GraphName, ESearchCase::IgnoreCase)) continue;

			FGraphPlan Plan;
			Plan.Adapter = Adapter;
			Adapter->Read(Plan.Current);
			if (Plan.Current.Nodes.IsEmpty()) continue;

			int32 Selected = 0;
			for (FShuttlegentLayoutNode& Node : Plan.Current.Nodes)
			{
				const FString* Matched = Names.FindByPredicate([&Node](const FString& Name) { return MatchesNode(Node, Name); });
				Node.bArrange = Names.IsEmpty() || Matched != nullptr;
				if (Matched != nullptr) MatchedNames.Add(Matched->ToLower());
				if (NodeLabels.Num() < MaxListedNodes) NodeLabels.Add(FString::Printf(TEXT("%s (%s)"), *Node.Title, *Node.Name));
				if (Node.bArrange) ++Selected;
			}
			if (Selected == 0) continue;

			Plan.Planned = Plan.Current;
			FShuttlegentGraphLayout::Arrange(Plan.Planned);
			for (int32 Index = 0; Index < Plan.Current.Nodes.Num(); ++Index)
			{
				if (!Plan.Planned.Nodes[Index].Position.Equals(Plan.Current.Nodes[Index].Position)) ++Plan.Moved;
			}
			OutPlans.Add(MoveTemp(Plan));
		}

		if (!GraphName.IsEmpty() && !GraphNames.ContainsByPredicate([&GraphName](const FString& Name) { return Name.Equals(GraphName, ESearchCase::IgnoreCase); }))
		{
			OutError = FString::Printf(TEXT("%s has no graph %s; nothing was moved. Its graphs: %s."), *OutAsset->GetName(), *GraphName, *FString::Join(GraphNames, NameListSeparator));
			return false;
		}
		for (const FString& Name : Names)
		{
			if (MatchedNames.Contains(Name.ToLower())) continue;

			OutError = FString::Printf(TEXT("No node named %s in the selected graphs of %s; nothing was moved. Nodes are matched by title or object name; the nodes are, as title (object name): %s."), *Name, *OutAsset->GetName(), *FString::Join(NodeLabels, NameListSeparator));
			return false;
		}
		if (OutPlans.IsEmpty()) OutError = TEXT("Nothing to arrange: the selected graphs have no nodes.");
		return !OutPlans.IsEmpty();
	}

	class FTidyGraph : public FShuttlegentToolBase
	{
	public:
		FTidyGraph()
		{
			Definition.Name = TidyGraphName;
			Definition.Description = TEXT("Tidy the node layout of a node-graph asset: Blueprints (event graphs, functions, macros, Animation and Widget Blueprints), materials and material functions, Control Rigs, and other assets that keep a node graph. Nodes flow left to right in columns, rows follow pin order to reduce crossing wires, separate node groups are stacked, unconnected nodes go to one side, and comment boxes are resized to keep wrapping their nodes. Pass nodes to arrange only those (for example the nodes you just added) and leave the rest where the user put them; nodes takes the titles the editor shows (Branch, Print String) or object names, and a title shared by several nodes arranges all of them; arrange whole graphs only when the user asks for it. Sizes come from the asset's open editor, otherwise they are estimated. A material open in the Material Editor has its working graph arranged; the user applies or saves it there. Runs as one undoable step.");
			Definition.bReadOnly = false;
			Definition.InputSchema = FShuttlegentToolSchema()
				.String(ArgAsset, TEXT("Asset path, e.g. /Game/BP/BP_Door or /Game/Materials/M_Wall."), true)
				.String(ArgGraph, TEXT("Only this graph, e.g. EventGraph or a function name. Defaults to every graph of the asset; an error lists the graph names."))
				.StringArray(ArgNodes, TEXT("Only these nodes, by object name or by the title the editor shows; the other nodes stay where they are and anchor the arranged ones. Defaults to every node."))
				.Build();
		}

		virtual bool Preview(const TSharedRef<FJsonObject>& Arguments, FShuttlegentWriteRequest& OutRequest, FString& OutError) override
		{
			UObject* Asset = nullptr;
			TArray<FGraphPlan> Plans;
			if (!PlanGraphs(Arguments, Asset, Plans, OutError)) return false;

			OutRequest.Action = LOCTEXT("TidyAction", "Tidy the graph layout").ToString();
			for (const FGraphPlan& Plan : Plans)
			{
				const FText Estimated = Plan.Adapter->HasRenderedSizes() ? FText::GetEmpty() : LOCTEXT("Estimated", " (node sizes are estimated)");

				FShuttlegentWriteObject& Item = OutRequest.Objects.AddDefaulted_GetRef();
				Item.Name = Asset->GetName();
				Item.Path = Asset->GetPathName();
				Item.Field = Plan.Adapter->GetGraphName();
				Item.CurrentValue = FText::Format(LOCTEXT("CurrentLayout", "{0} node(s), bounds {1}"), FText::AsNumber(Plan.Current.Nodes.Num()), DescribeBounds(Plan.Current)).ToString();
				Item.PlannedValue = FText::Format(LOCTEXT("PlannedLayout", "Move {0} node(s), bounds {1}{2}"), FText::AsNumber(Plan.Moved), DescribeBounds(Plan.Planned), Estimated).ToString();
			}
			return true;
		}

		virtual void Execute(const TSharedRef<FJsonObject>& Arguments, TArray<FShuttlegentWriteObject>& InOutObjects) override
		{
			// 重新读取并排版：确认期间节点图若有变化，以执行时的状态为准
			UObject* Asset = nullptr;
			TArray<FGraphPlan> Plans;
			FString Error;
			PlanGraphs(Arguments, Asset, Plans, Error);
			for (FShuttlegentWriteObject& Item : InOutObjects)
			{
				const FGraphPlan* Plan = Plans.FindByPredicate([&Item](const FGraphPlan& Candidate) { return Candidate.Adapter->GetGraphName() == Item.Field; });
				if (Plan == nullptr)
				{
					Item.Status = EShuttlegentWriteObjectStatus::Failed;
					Item.Message = Error.IsEmpty() ? LOCTEXT("GraphGone", "The graph no longer exists or has no nodes left to arrange").ToString() : Error;
					continue;
				}

				Plan->Adapter->Write(Plan->Planned);
				Item.Status = EShuttlegentWriteObjectStatus::Completed;
			}
		}
	};
}

void FShuttlegentGraphTools::Append(TArray<TSharedRef<IShuttlegentTool>>& OutTools)
{
	OutTools.Add(MakeShared<FShuttlegentGraphToolsPrivate::FTidyGraph>());
}

#undef LOCTEXT_NAMESPACE
