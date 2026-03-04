#pragma once

#include "BlueprintGraphLayoutTypes.h"
#include "EdGraph/EdGraph.h"

class UBlueprint;

/** Entry for full graph layout: runs Phase0–Phase3 and writes node positions. */
class FBlueprintGraphLayout
{
public:
	/** Run layout on Graph and mark blueprint modified. */
	static void LayoutGraph(UBlueprint* BP, UEdGraph* Graph);
};
