#include "Graph/FShuttlegentGraphLayout.h"

#include "Graph/FShuttlegentLayoutGraph.h"

namespace FShuttlegentGraphLayoutPrivate
{
	// 相邻两列之间的水平间距（节点图单位）
	inline constexpr double ColumnGap = 80.0;

	// 同列相邻节点之间的垂直间距（节点图单位）
	inline constexpr double RowGap = 40.0;

	// 互不相连的各部分之间、以及与孤立节点列之间的间距（节点图单位）
	inline constexpr double ComponentGap = 160.0;

	// 注释框四周的留白与顶部标题栏高度（节点图单位）
	inline constexpr double CommentPadding = 30.0;
	inline constexpr double CommentTitleHeight = 60.0;

	// 列内排序的往返扫描次数，以及纵向对齐的往返次数
	inline constexpr int32 OrderSweeps = 4;
	inline constexpr int32 AlignPasses = 3;

	// 避让已占区域时最多下移的次数，防止异常数据下死循环
	inline constexpr int32 MaxAvoidSteps = 1000;

	/** 一组互相连通、参与排版的节点；Local 为排版后相对本组左上角的位置。 */
	struct FComponent
	{
		TArray<int32> Nodes;
		TMap<int32, FVector2D> Local;
		FVector2D Size {};
		double OriginalTop = 0.0;
	};

	FBox2D NodeBox(const FVector2D& Position, const FVector2D& Size)
	{
		return FBox2D(Position, Position + Size);
	}

	int32 FindRoot(TArray<int32>& Parents, int32 Index)
	{
		while (Parents[Index] != Index)
		{
			Parents[Index] = Parents[Parents[Index]];
			Index = Parents[Index];
		}
		return Index;
	}

	/** 按参与排版的节点之间的连线分组；没有任何连线（包括与不动节点之间）的节点归入 OutIsolated。 */
	TArray<FComponent> FindComponents(const FShuttlegentLayoutGraph& Graph, TArray<int32>& OutIsolated)
	{
		const int32 Count = Graph.Nodes.Num();
		TArray<int32> Parents;
		TArray<bool> HasEdge;
		Parents.SetNum(Count);
		HasEdge.Init(false, Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Parents[Index] = Index;
		}

		for (const FShuttlegentLayoutEdge& Edge : Graph.Edges)
		{
			if (!Graph.Nodes.IsValidIndex(Edge.Source) || !Graph.Nodes.IsValidIndex(Edge.Target) || Edge.Source == Edge.Target) continue;

			HasEdge[Edge.Source] = true;
			HasEdge[Edge.Target] = true;
			if (Graph.Nodes[Edge.Source].bArrange && Graph.Nodes[Edge.Target].bArrange) Parents[FindRoot(Parents, Edge.Source)] = FindRoot(Parents, Edge.Target);
		}

		TMap<int32, int32> ComponentOfRoot;
		TArray<FComponent> Components;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (!Graph.Nodes[Index].bArrange) continue;
			if (!HasEdge[Index])
			{
				OutIsolated.Add(Index);
				continue;
			}

			const int32 Root = FindRoot(Parents, Index);
			const int32* Existing = ComponentOfRoot.Find(Root);
			const int32 ComponentIndex = Existing != nullptr ? *Existing : Components.AddDefaulted();
			ComponentOfRoot.Add(Root, ComponentIndex);
			Components[ComponentIndex].Nodes.Add(Index);
		}

		for (FComponent& Component : Components)
		{
			Component.OriginalTop = TNumericLimits<double>::Max();
			for (const int32 Node : Component.Nodes)
			{
				Component.OriginalTop = FMath::Min(Component.OriginalTop, Graph.Nodes[Node].Position.Y);
			}
		}
		return Components;
	}

	/** 在组内分列、排序、定坐标，结果写入 Component.Local 与 Component.Size。 */
	void LayoutComponent(const FShuttlegentLayoutGraph& Graph, FComponent& Component)
	{
		const TArray<int32>& Members = Component.Nodes;
		const int32 Count = Members.Num();
		TMap<int32, int32> LocalIndex;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			LocalIndex.Add(Members[Index], Index);
		}

		// 组内连线，按本地下标记录前驱与后继
		TArray<TArray<const FShuttlegentLayoutEdge*>> Incoming;
		TArray<TArray<const FShuttlegentLayoutEdge*>> Outgoing;
		Incoming.SetNum(Count);
		Outgoing.SetNum(Count);
		for (const FShuttlegentLayoutEdge& Edge : Graph.Edges)
		{
			const int32* Source = LocalIndex.Find(Edge.Source);
			const int32* Target = LocalIndex.Find(Edge.Target);
			if (Source == nullptr || Target == nullptr || *Source == *Target) continue;

			Outgoing[*Source].Add(&Edge);
			Incoming[*Target].Add(&Edge);
		}

		// 分列：按拓扑序取最长路径；有环时从最靠左的未处理节点断开
		TArray<int32> Layer;
		TArray<int32> InDegree;
		TArray<int32> TopoPosition;
		TArray<int32> Topo;
		Layer.Init(0, Count);
		InDegree.SetNum(Count);
		TopoPosition.Init(INDEX_NONE, Count);
		TArray<int32> Ready;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			InDegree[Index] = Incoming[Index].Num();
			if (InDegree[Index] == 0) Ready.Add(Index);
		}

		const auto ByOriginalPosition = [&Graph, &Members](int32 Left, int32 Right)
		{
			const FVector2D& A = Graph.Nodes[Members[Left]].Position;
			const FVector2D& B = Graph.Nodes[Members[Right]].Position;
			return A.X != B.X ? A.X < B.X : A.Y < B.Y;
		};
		while (Topo.Num() < Count)
		{
			if (Ready.IsEmpty())
			{
				int32 Forced = INDEX_NONE;
				for (int32 Index = 0; Index < Count; ++Index)
				{
					if (TopoPosition[Index] == INDEX_NONE && (Forced == INDEX_NONE || ByOriginalPosition(Index, Forced))) Forced = Index;
				}
				Ready.Add(Forced);
			}

			Ready.Sort(ByOriginalPosition);
			const int32 Current = Ready[0];
			Ready.RemoveAt(0);
			if (TopoPosition[Current] != INDEX_NONE) continue;

			TopoPosition[Current] = Topo.Num();
			Topo.Add(Current);
			for (const FShuttlegentLayoutEdge* Edge : Outgoing[Current])
			{
				const int32 Next = LocalIndex[Edge->Target];
				if (TopoPosition[Next] != INDEX_NONE) continue;

				Layer[Next] = FMath::Max(Layer[Next], Layer[Current] + 1);
				if (--InDegree[Next] == 0) Ready.Add(Next);
			}
		}

		// 压紧：上游节点挪到紧挨其最近下游的左一列，避免常量一类节点被甩到最左
		for (int32 Position = Count - 1; Position >= 0; --Position)
		{
			const int32 Current = Topo[Position];
			int32 NearestNext = TNumericLimits<int32>::Max();
			for (const FShuttlegentLayoutEdge* Edge : Outgoing[Current])
			{
				const int32 Next = LocalIndex[Edge->Target];
				if (TopoPosition[Next] > Position) NearestNext = FMath::Min(NearestNext, Layer[Next]);
			}
			if (NearestNext != TNumericLimits<int32>::Max() && NearestNext - 1 > Layer[Current]) Layer[Current] = NearestNext - 1;
		}

		int32 MinLayer = TNumericLimits<int32>::Max();
		int32 MaxLayer = 0;
		for (const int32 Value : Layer)
		{
			MinLayer = FMath::Min(MinLayer, Value);
		}
		for (int32& Value : Layer)
		{
			Value -= MinLayer;
			MaxLayer = FMath::Max(MaxLayer, Value);
		}

		// 列内顺序：先按原纵坐标，再按相邻列的引脚位置往返扫描以减少交叉
		TArray<TArray<int32>> Columns;
		TArray<double> Rank;
		Columns.SetNum(MaxLayer + 1);
		Rank.Init(0.0, Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Columns[Layer[Index]].Add(Index);
		}
		for (TArray<int32>& Column : Columns)
		{
			Column.StableSort([&Graph, &Members](int32 Left, int32 Right) { return Graph.Nodes[Members[Left]].Position.Y < Graph.Nodes[Members[Right]].Position.Y; });
			for (int32 Row = 0; Row < Column.Num(); ++Row)
			{
				Rank[Column[Row]] = Row;
			}
		}

		for (int32 Sweep = 0; Sweep < OrderSweeps; ++Sweep)
		{
			const bool bForward = Sweep % 2 == 0;
			for (int32 Step = 1; Step <= MaxLayer; ++Step)
			{
				TArray<int32>& Column = Columns[bForward ? Step : MaxLayer - Step];
				TMap<int32, double> Keys;
				for (const int32 Node : Column)
				{
					double Sum = 0.0;
					int32 Links = 0;
					for (const FShuttlegentLayoutEdge* Edge : bForward ? Incoming[Node] : Outgoing[Node])
					{
						const int32 Other = LocalIndex[bForward ? Edge->Source : Edge->Target];
						if (bForward ? Layer[Other] >= Layer[Node] : Layer[Other] <= Layer[Node]) continue;

						Sum += Rank[Other] + (bForward ? Edge->SourcePin : Edge->TargetPin);
						++Links;
					}
					Keys.Add(Node, Links > 0 ? Sum / Links : Rank[Node]);
				}

				Column.StableSort([&Keys](int32 Left, int32 Right) { return Keys[Left] < Keys[Right]; });
				for (int32 Row = 0; Row < Column.Num(); ++Row)
				{
					Rank[Column[Row]] = Row;
				}
			}
		}

		// 横坐标：每列宽度取列内最宽节点
		TArray<double> ColumnX;
		ColumnX.Init(0.0, MaxLayer + 1);
		double Cursor = 0.0;
		for (int32 Column = 0; Column <= MaxLayer; ++Column)
		{
			double Width = 0.0;
			for (const int32 Node : Columns[Column])
			{
				Width = FMath::Max(Width, Graph.Nodes[Members[Node]].Size.X);
			}
			ColumnX[Column] = Cursor;
			Cursor += Width + ColumnGap;
		}

		// 纵坐标：先逐列紧排，再按相连引脚的高度往返对齐，同列内只往下推以免重叠
		TArray<double> Y;
		Y.Init(0.0, Count);
		for (const TArray<int32>& Column : Columns)
		{
			double Bottom = 0.0;
			for (const int32 Node : Column)
			{
				Y[Node] = Bottom;
				Bottom += Graph.Nodes[Members[Node]].Size.Y + RowGap;
			}
		}

		for (int32 Pass = 0; Pass < AlignPasses; ++Pass)
		{
			const bool bForward = Pass % 2 == 0;
			for (int32 Step = 0; Step <= MaxLayer; ++Step)
			{
				const TArray<int32>& Column = Columns[bForward ? Step : MaxLayer - Step];
				double Bottom = -TNumericLimits<double>::Max();
				for (const int32 Node : Column)
				{
					const double Height = Graph.Nodes[Members[Node]].Size.Y;
					double Sum = 0.0;
					int32 Links = 0;
					for (const FShuttlegentLayoutEdge* Edge : bForward ? Incoming[Node] : Outgoing[Node])
					{
						const int32 Other = LocalIndex[bForward ? Edge->Source : Edge->Target];
						if (bForward ? Layer[Other] >= Layer[Node] : Layer[Other] <= Layer[Node]) continue;

						const double OtherHeight = Graph.Nodes[Members[Other]].Size.Y;
						const double OtherPin = bForward ? Edge->SourcePin : Edge->TargetPin;
						const double OwnPin = bForward ? Edge->TargetPin : Edge->SourcePin;
						Sum += Y[Other] + OtherPin * OtherHeight - OwnPin * Height;
						++Links;
					}

					const double Desired = Links > 0 ? Sum / Links : Y[Node];
					Y[Node] = FMath::Max(Desired, Bottom);
					Bottom = Y[Node] + Height + RowGap;
				}
			}
		}

		double Top = TNumericLimits<double>::Max();
		for (const double Value : Y)
		{
			Top = FMath::Min(Top, Value);
		}

		Component.Size = FVector2D::ZeroVector;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector2D Local(ColumnX[Layer[Index]], Y[Index] - Top);
			const FVector2D& Size = Graph.Nodes[Members[Index]].Size;
			Component.Local.Add(Members[Index], Local);
			Component.Size.X = FMath::Max(Component.Size.X, Local.X + Size.X);
			Component.Size.Y = FMath::Max(Component.Size.Y, Local.Y + Size.Y);
		}
	}

	/** 把区域往下推，直到不与任何已占区域相交。 */
	FVector2D Avoid(FVector2D Offset, const FVector2D& Size, const TArray<FBox2D>& Occupied)
	{
		for (int32 Step = 0; Step < MaxAvoidSteps; ++Step)
		{
			const FBox2D Box = NodeBox(Offset, Size).ExpandBy(FVector2D(0.0, RowGap * 0.5));
			const FBox2D* Hit = Occupied.FindByPredicate([&Box](const FBox2D& Other) { return Box.Intersect(Other); });
			if (Hit == nullptr) return Offset;

			Offset.Y = Hit->Max.Y + RowGap;
		}
		return Offset;
	}

	/** 组与不动节点之间的第一条连线所决定的放置位置；没有这种连线时返回 false。 */
	bool AnchorOffset(const FShuttlegentLayoutGraph& Graph, const FComponent& Component, FVector2D& OutOffset)
	{
		for (const FShuttlegentLayoutEdge& Edge : Graph.Edges)
		{
			if (!Graph.Nodes.IsValidIndex(Edge.Source) || !Graph.Nodes.IsValidIndex(Edge.Target)) continue;

			const bool bFeedsFixed = Component.Local.Contains(Edge.Source) && !Graph.Nodes[Edge.Target].bArrange;
			const bool bFedByFixed = Component.Local.Contains(Edge.Target) && !Graph.Nodes[Edge.Source].bArrange;
			if (!bFeedsFixed && !bFedByFixed) continue;

			const FShuttlegentLayoutNode& Fixed = Graph.Nodes[bFeedsFixed ? Edge.Target : Edge.Source];
			const int32 Own = bFeedsFixed ? Edge.Source : Edge.Target;
			const FVector2D& Local = Component.Local[Own];
			const double OwnHeight = Graph.Nodes[Own].Size.Y;
			if (bFeedsFixed)
			{
				OutOffset.X = Fixed.Position.X - ColumnGap - Component.Size.X;
				OutOffset.Y = Fixed.Position.Y + Edge.TargetPin * Fixed.Size.Y - (Local.Y + Edge.SourcePin * OwnHeight);
			}
			else
			{
				OutOffset.X = Fixed.Position.X + Fixed.Size.X + ColumnGap - Local.X;
				OutOffset.Y = Fixed.Position.Y + Edge.SourcePin * Fixed.Size.Y - (Local.Y + Edge.TargetPin * OwnHeight);
			}
			return true;
		}
		return false;
	}

	/** Arrange 的实现，见 FShuttlegentGraphLayout::Arrange。 */
	void ArrangeGraph(FShuttlegentLayoutGraph& Graph)
	{
		const int32 Count = Graph.Nodes.Num();
		if (!Graph.Nodes.ContainsByPredicate([](const FShuttlegentLayoutNode& Node) { return Node.bArrange; })) return;

		// 注释框框住的节点按排版前的位置确定
		TArray<TArray<int32>> CommentMembers;
		for (const FShuttlegentLayoutComment& Comment : Graph.Comments)
		{
			const FBox2D CommentBox = NodeBox(Comment.Position, Comment.Size);
			TArray<int32>& Members = CommentMembers.AddDefaulted_GetRef();
			for (int32 Index = 0; Index < Count; ++Index)
			{
				if (CommentBox.IsInside(NodeBox(Graph.Nodes[Index].Position, Graph.Nodes[Index].Size))) Members.Add(Index);
			}
		}

		TArray<int32> Isolated;
		TArray<FComponent> Components = FindComponents(Graph, Isolated);
		for (FComponent& Component : Components)
		{
			LayoutComponent(Graph, Component);
		}
		Components.StableSort([](const FComponent& Left, const FComponent& Right) { return Left.OriginalTop < Right.OriginalTop; });
		Isolated.StableSort([&Graph](int32 Left, int32 Right) { return Graph.Nodes[Left].Position.Y < Graph.Nodes[Right].Position.Y; });

		FBox2D Fixed(ForceInit);
		FBox2D Arranged(ForceInit);
		TArray<FBox2D> Occupied;
		for (const FShuttlegentLayoutNode& Node : Graph.Nodes)
		{
			const FBox2D Box = NodeBox(Node.Position, Node.Size);
			if (Node.bArrange)
			{
				Arranged += Box;
				continue;
			}
			Fixed += Box;
			Occupied.Add(Box);
		}

		TArray<FVector2D> NewPositions;
		NewPositions.SetNum(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			NewPositions[Index] = Graph.Nodes[Index].Position;
		}

		double IsolatedWidth = 0.0;
		for (const int32 Node : Isolated)
		{
			IsolatedWidth = FMath::Max(IsolatedWidth, Graph.Nodes[Node].Size.X);
		}

		if (!Fixed.bIsValid)
		{
			// 整张图重排：孤立节点排成最左一列，各部分按原先的上下顺序叠放，整体左上角留在原处
			double IsolatedY = Arranged.Min.Y;
			for (const int32 Node : Isolated)
			{
				NewPositions[Node] = FVector2D(Arranged.Min.X, IsolatedY);
				IsolatedY += Graph.Nodes[Node].Size.Y + RowGap;
			}

			const double Left = Arranged.Min.X + (Isolated.IsEmpty() ? 0.0 : IsolatedWidth + ComponentGap);
			double Cursor = Arranged.Min.Y;
			for (const FComponent& Component : Components)
			{
				for (const TPair<int32, FVector2D>& Pair : Component.Local)
				{
					NewPositions[Pair.Key] = FVector2D(Left, Cursor) + Pair.Value;
				}
				Cursor += Component.Size.Y + ComponentGap;
			}
		}
		else
		{
			// 部分重排：与不动节点相连的部分靠到连线的另一端，其余放到不动节点下方；都避开已占区域
			double Cursor = Fixed.Max.Y + ComponentGap;
			for (const FComponent& Component : Components)
			{
				FVector2D Offset(Fixed.Min.X, Cursor);
				const bool bAnchored = AnchorOffset(Graph, Component, Offset);
				Offset = Avoid(Offset, Component.Size, Occupied);
				Occupied.Add(NodeBox(Offset, Component.Size));
				if (!bAnchored) Cursor = Offset.Y + Component.Size.Y + ComponentGap;

				for (const TPair<int32, FVector2D>& Pair : Component.Local)
				{
					NewPositions[Pair.Key] = Offset + Pair.Value;
				}
			}

			for (const int32 Node : Isolated)
			{
				const FVector2D Offset = Avoid(FVector2D(Fixed.Min.X, Cursor), Graph.Nodes[Node].Size, Occupied);
				Occupied.Add(NodeBox(Offset, Graph.Nodes[Node].Size));
				NewPositions[Node] = Offset;
				Cursor = Offset.Y + Graph.Nodes[Node].Size.Y + RowGap;
			}
		}

		for (int32 Index = 0; Index < Count; ++Index)
		{
			Graph.Nodes[Index].Position = FVector2D(FMath::RoundToDouble(NewPositions[Index].X), FMath::RoundToDouble(NewPositions[Index].Y));
		}

		// 注释框跟随：框住的节点有任一被移动时，调整为正好框住这些节点
		for (int32 CommentIndex = 0; CommentIndex < Graph.Comments.Num(); ++CommentIndex)
		{
			const TArray<int32>& Members = CommentMembers[CommentIndex];
			if (!Members.ContainsByPredicate([&Graph](int32 Node) { return Graph.Nodes[Node].bArrange; })) continue;

			FBox2D Box(ForceInit);
			for (const int32 Node : Members)
			{
				Box += NodeBox(Graph.Nodes[Node].Position, Graph.Nodes[Node].Size);
			}

			FShuttlegentLayoutComment& Comment = Graph.Comments[CommentIndex];
			Comment.Position = Box.Min - FVector2D(CommentPadding, CommentTitleHeight);
			Comment.Size = Box.GetSize() + FVector2D(CommentPadding * 2.0, CommentPadding + CommentTitleHeight);
		}
	}
}

void FShuttlegentGraphLayout::Arrange(FShuttlegentLayoutGraph& Graph)
{
	FShuttlegentGraphLayoutPrivate::ArrangeGraph(Graph);
}

FBox2D FShuttlegentGraphLayout::GetBounds(const FShuttlegentLayoutGraph& Graph)
{
	FBox2D Bounds(ForceInit);
	for (const FShuttlegentLayoutNode& Node : Graph.Nodes)
	{
		Bounds += FShuttlegentGraphLayoutPrivate::NodeBox(Node.Position, Node.Size);
	}
	return Bounds;
}
