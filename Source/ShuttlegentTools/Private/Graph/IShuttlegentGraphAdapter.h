#pragma once

#include "CoreMinimal.h"

struct FShuttlegentLayoutGraph;

/** 一张具体节点图（蓝图图表、材质、RigVM 图等）与通用排版之间的读写适配。仅限游戏线程。 */
class IShuttlegentGraphAdapter
{
public:
	virtual ~IShuttlegentGraphAdapter() = default;

	/** 节点图名称，即 tidy_graph 的 graph 参数与确认卡上的字段。 */
	virtual FString GetGraphName() const = 0;

	/** 最近一次 Read 的节点尺寸是否全部取自已打开编辑器中实际绘制的大小；否则含估算。 */
	virtual bool HasRenderedSizes() const = 0;

	/** 读取节点、连线与注释框的当前状态；下标与 Write 收到的图一一对应。 */
	virtual void Read(FShuttlegentLayoutGraph& OutGraph) = 0;

	/** 写回位置有变化的节点与注释框；须对改动的对象调用 Modify()，不得自行开启事务。 */
	virtual void Write(const FShuttlegentLayoutGraph& Graph) = 0;
};
