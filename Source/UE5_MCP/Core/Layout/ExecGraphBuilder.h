#pragma once

#include "BlueprintGraphLayoutTypes.h"
#include "EdGraph/EdGraph.h"

/** Builds exec-only graph: roots, per-node exec successors with subIndex, and pure-node set. */
class FExecGraphBuilder
{
public:
	/** Build from graph. Fills Roots, Successors, PureNodes. */
	static void Build(UEdGraph* Graph, FExecGraphResult& OutResult);
};
