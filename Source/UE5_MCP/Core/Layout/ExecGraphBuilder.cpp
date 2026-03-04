#include "ExecGraphBuilder.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"

void FExecGraphBuilder::Build(UEdGraph* Graph, FExecGraphResult& OutResult)
{
	OutResult.Roots.Empty();
	OutResult.Successors.Empty();
	OutResult.PureNodes.Empty();

	if (!Graph) return;

	const FName ExecCategory = UEdGraphSchema_K2::PC_Exec;

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node) continue;

		bool bHasExecInputConnection = false;
		bool bHasExecOutput = false;
		TArray<FExecSuccessor> OutEdges;
		int32 SubIndexBase = 0;

		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (!Pin || Pin->PinType.PinCategory != ExecCategory) continue;

			if (Pin->Direction == EGPD_Input)
			{
				if (Pin->LinkedTo.Num() > 0) bHasExecInputConnection = true;
			}
			else if (Pin->Direction == EGPD_Output)
			{
				bHasExecOutput = true;
				// subIndex = order of this output exec pin (Branch: True=0, False=1; Sequence: Then0=0, Then1=1)
				const int32 PinSubIndex = SubIndexBase++;
				TArray<UEdGraphPin*> Linked = Pin->LinkedTo;
				Linked.Sort([](const UEdGraphPin& A, const UEdGraphPin& B)
				{
					UEdGraphNode* NodeA = A.GetOwningNode();
					UEdGraphNode* NodeB = B.GetOwningNode();
					if (!NodeA) return NodeB != nullptr;
					if (!NodeB) return false;
					return NodeA->NodeGuid < NodeB->NodeGuid;
				});
				for (UEdGraphPin* ToPin : Linked)
				{
					if (ToPin && ToPin->GetOwningNode())
					{
						FExecSuccessor S;
						S.Target = ToPin->GetOwningNode();
						S.SubIndex = PinSubIndex;
						OutEdges.Add(S);
					}
				}
			}
		}

		if (OutEdges.Num() > 0)
		{
			OutResult.Successors.Add(Node, MoveTemp(OutEdges));
		}

		if (!bHasExecInputConnection && !bHasExecOutput)
		{
			OutResult.PureNodes.Add(Node);
		}
		else if (!bHasExecInputConnection)
		{
			OutResult.Roots.Add(Node);
		}
	}
}
