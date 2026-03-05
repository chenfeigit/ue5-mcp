#include "GraphUtils.h"

#include "BPUtils.h"
#include "ClassUtils.h"
#include "StructUtils.h"
#include "EnumUtils.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CastByteToEnum.h"
#include "K2Node_ClassDynamicCast.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_SwitchEnum.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "EdGraph/EdGraph.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "PinUtils.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/Field.h"

const int32 GraphUtils::DefaultNewNodeSpacing = 400;

namespace
{
	// Cache for get_supported_nodes (lazy-built, invalidate via InvalidateSupportedNodesCache)
	static TArray<FString> GSupportedNodesCache;
	static bool GSupportedNodesCacheValid = false;

	/** Converts UClass path to Module.ClassName format compatible with ClassUtils::FindClassByName (e.g. Engine.BlueprintMapLibrary). */
	static FString GetLibraryClassDisplayName(UClass* Class)
	{
		if (!Class) return FString();
		FString Path = Class->GetPathName();
		// Typical C++ class path: /Script/ModuleName.ClassName
		if (Path.StartsWith(TEXT("/Script/")))
		{
			return Path.Mid(8);
		}
		return Class->GetName();
	}
}

void GraphUtils::SetNewNodePosition(UEdGraph* Graph, UEdGraphNode* NewNode)
{
	if (!Graph || !NewNode) return;
	int32 MaxRight = 0;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (Node == NewNode) continue;
		MaxRight = FMath::Max(MaxRight, Node->NodePosX + FMath::Max(Node->NodeWidth, 1));
	}
	NewNode->NodePosX = MaxRight + DefaultNewNodeSpacing;
	NewNode->NodePosY = 0;
}

void GraphUtils::AddFunctionCallToGraph(UBlueprint* Blueprint, UEdGraph* Graph, const FString& FunctionName)
{
    if (!Blueprint || !Graph)
        throw std::runtime_error("Blueprint or Graph is null");

    auto BPClass = Blueprint->GeneratedClass;
    if (!BPClass)
        throw std::runtime_error("Blueprint's GeneratedClass is null, please compile the Blueprint first");

    UFunction* TargetFunction = BPClass->FindFunctionByName(*FunctionName);
    if (!TargetFunction && !FunctionName.StartsWith(TEXT("K2_")))
        TargetFunction = BPClass->FindFunctionByName(*(FString(TEXT("K2_")) + FunctionName));
    if (!TargetFunction)
        throw std::runtime_error(TCHAR_TO_UTF8(*FString::Printf(TEXT("Function %s not found in Blueprint class"), *FunctionName)));

    FGraphNodeCreator<UK2Node_CallFunction> NodeCreator(*Graph);
    UK2Node_CallFunction* CallFuncNode = NodeCreator.CreateNode();
    CallFuncNode->SetFromFunction(TargetFunction);
    SetNewNodePosition(Graph, CallFuncNode);
    NodeCreator.Finalize();

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
}

void GraphUtils::AddFunctionCallToGraph(UBlueprint* Blueprint, UEdGraph* Graph, const FString& ClassToCall,
	const FString& FunctionName)
{
	if (!Blueprint || !Graph)
		throw std::runtime_error("Blueprint or Graph is null");

	auto TargetClass = ClassUtils::FindClassByName(ClassToCall);
	if (!TargetClass)
		throw std::runtime_error(TCHAR_TO_UTF8(*FString::Printf(TEXT("Class %s not found"), *ClassToCall)));

	UFunction* TargetFunction = TargetClass->FindFunctionByName(*FunctionName);
	if (!TargetFunction && !FunctionName.StartsWith(TEXT("K2_")))
		TargetFunction = TargetClass->FindFunctionByName(*(FString(TEXT("K2_")) + FunctionName));
	if (!TargetFunction)
		throw std::runtime_error(TCHAR_TO_UTF8(*FString::Printf(TEXT("Function %s not found in class %s"), *FunctionName, *ClassToCall)));

	FGraphNodeCreator<UK2Node_CallFunction> NodeCreator(*Graph);
	UK2Node_CallFunction* CallFuncNode = NodeCreator.CreateNode();
	CallFuncNode->SetFromFunction(TargetFunction);
	SetNewNodePosition(Graph, CallFuncNode);
	NodeCreator.Finalize();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
}

void GraphUtils::AddMathFunctionCallToGraph(UBlueprint* Blueprint, UEdGraph* Graph, const FString& FunctionName)
{
	if (!Blueprint || !Graph)
		throw std::runtime_error("Blueprint or Graph is null");

	UClass* MathClass = UKismetMathLibrary::StaticClass();
	UFunction* TargetFunction = MathClass->FindFunctionByName(*FunctionName);
	if (!TargetFunction && !FunctionName.StartsWith(TEXT("K2_")))
		TargetFunction = MathClass->FindFunctionByName(*(FString(TEXT("K2_")) + FunctionName));
	if (!TargetFunction)
		throw std::runtime_error(TCHAR_TO_UTF8(*FString::Printf(TEXT("Function %s not found in KismetMathLibrary"), *FunctionName)));
	
	FGraphNodeCreator<UK2Node_CallFunction> NodeCreator(*Graph);
	UK2Node_CallFunction* CallFuncNode = NodeCreator.CreateNode();
	CallFuncNode->SetFromFunction(TargetFunction);
	SetNewNodePosition(Graph, CallFuncNode);
	NodeCreator.Finalize();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
}



void GraphUtils::AddEventToGraph(UBlueprint* Blueprint,
                                 UEdGraph* Graph, const FString& EventName)
{
    if (!Blueprint || !Graph)
        throw std::runtime_error("Blueprint or Graph is null");
    
    UClass* BPClass = Blueprint->GeneratedClass;
    if (!BPClass)
        throw std::runtime_error("Blueprint's GeneratedClass is null, please compile the Blueprint first");
    
    auto EventFunc = BPClass->FindFunctionByName(*EventName);
    if (!EventFunc)
        throw std::runtime_error(TCHAR_TO_UTF8(*FString::Printf(TEXT("Event %s not found in Blueprint class"), *EventName)));
    
    FGraphNodeCreator<UK2Node_Event> NodeCreator(*Graph);
    UK2Node_Event* EventNode = NodeCreator.CreateNode();
    EventNode->EventReference.SetFromField<UFunction>(EventFunc, true);
    EventNode->bOverrideFunction = true;
    SetNewNodePosition(Graph, EventNode);
    NodeCreator.Finalize();

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
}

void GraphUtils::AddCustomEventToGraph(
    UBlueprint* Blueprint,
    UEdGraph* Graph,
    const FString& EventName,
    const FString& EventSignature)
{
    if (!Blueprint || !Graph) return;

    // Create the node
    FGraphNodeCreator<UK2Node_CustomEvent> NodeCreator(*Graph);
    UK2Node_CustomEvent* CustomEventNode = NodeCreator.CreateNode();
    CustomEventNode->CustomFunctionName = *EventName;

    // Always has execution pin
    CustomEventNode->AllocateDefaultPins();

    TArray<FString> Params;
    FEdGraphPinType PinType;
    EventSignature.ParseIntoArray(Params, TEXT(","), true);
    for (FString& Param : Params)
    {
        FString TypeStr, NameStr;
        if (!PinUtils::SplitTypeVar(Param, TypeStr, NameStr))
        {
            UE_LOG(LogTemp, Warning, TEXT("Invalid parameter: %s"), *Param);
            continue;
        }
        
        NameStr = NameStr.TrimStartAndEnd();
        if (NameStr.IsEmpty())
        {
            UE_LOG(LogTemp, Warning, TEXT("Invalid parameter: %s"), *Param);
            continue;
        }
            
        if (PinUtils::ResolvePinTypeByName(TypeStr, PinType))
        {
            CustomEventNode->CreateUserDefinedPin(*NameStr, PinType, EGPD_Output);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Unknown type: %s"), *TypeStr);
        }
    }

    SetNewNodePosition(Graph, CustomEventNode);
    NodeCreator.Finalize();

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
}

void GraphUtils::AddGetVariableNodeToGraph(
    UBlueprint* Blueprint, UEdGraph* Graph, const FString& VarName)
{
    if (!Blueprint || !Graph)
        throw std::runtime_error("Blueprint or Graph is null");

    auto BPClass = Blueprint->GeneratedClass;
    if (!BPClass)
        throw std::runtime_error("Blueprint's GeneratedClass is null, please compile the Blueprint first");

    auto Var = BPClass->FindPropertyByName(*VarName);
    if ( !Var)
    {
        throw std::runtime_error(TCHAR_TO_UTF8(*FString::Printf(TEXT("Variable %s not found in Blueprint class"), *VarName)));
    }

    FGraphNodeCreator<UK2Node_VariableGet> NodeCreator(*Graph);
    UK2Node_VariableGet* GetNode = NodeCreator.CreateNode();
    GetNode->VariableReference.SetSelfMember(*VarName);
    SetNewNodePosition(Graph, GetNode);
    NodeCreator.Finalize();
    
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
}

void GraphUtils::AddSetVariableNodeToGraph(
    UBlueprint* Blueprint, UEdGraph* Graph, const FString& VarName)
{
    if (!Blueprint || !Graph)
        throw std::runtime_error("Blueprint or Graph is null");

    
    auto BPClass = Blueprint->GeneratedClass;
    if (!BPClass)
        throw std::runtime_error("Blueprint's GeneratedClass is null, please compile the Blueprint first");

    auto Var = BPClass->FindPropertyByName(*VarName);
    if ( !Var)
    {
        throw std::runtime_error(TCHAR_TO_UTF8(*FString::Printf(TEXT("Variable %s not found in Blueprint class"), *VarName)));
    }
    
	FGraphNodeCreator<UK2Node_VariableSet> NodeCreator(*Graph);
	UK2Node_VariableSet* SetNode = NodeCreator.CreateNode();
	SetNode->VariableReference.SetSelfMember(*VarName);
	SetNewNodePosition(Graph, SetNode);
	NodeCreator.Finalize();

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
}

void GraphUtils::AddBreakStructNodeToGraph(UBlueprint* Blueprint, UEdGraph* Graph, const FString& StructTypeName)
{
	if (!Blueprint || !Graph)
		throw std::runtime_error("Blueprint or Graph is null");

	UScriptStruct* Struct = StructUtils::FindStructByName(StructTypeName);
	if (!Struct)
		throw std::runtime_error(TCHAR_TO_UTF8(*FString::Printf(TEXT("Struct %s not found (try Module.StructName or /Script/Module.StructName, e.g. /Script/Engine.FSkeletalMaterial)"), *StructTypeName)));

	FGraphNodeCreator<UK2Node_BreakStruct> NodeCreator(*Graph);
	UK2Node_BreakStruct* BreakStructNode = NodeCreator.CreateNode();
	BreakStructNode->StructType = Struct;
	SetNewNodePosition(Graph, BreakStructNode);
	NodeCreator.Finalize();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
}

void GraphUtils::AddMakeStructNodeToGraph(UBlueprint* Blueprint, UEdGraph* Graph, const FString& StructTypeName)
{
	if (!Blueprint || !Graph)
		throw std::runtime_error("Blueprint or Graph is null");

	UScriptStruct* Struct = StructUtils::FindStructByName(StructTypeName);
	if (!Struct)
		throw std::runtime_error(TCHAR_TO_UTF8(*FString::Printf(TEXT("Struct %s not found (try Module.StructName or /Script/Module.StructName, e.g. /Script/Engine.FSkeletalMaterial)"), *StructTypeName)));

	FGraphNodeCreator<UK2Node_MakeStruct> NodeCreator(*Graph);
	UK2Node_MakeStruct* MakeStructNode = NodeCreator.CreateNode();
	MakeStructNode->StructType = Struct;
	SetNewNodePosition(Graph, MakeStructNode);
	NodeCreator.Finalize();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
}

void GraphUtils::AddCommentNodeToGraph(UBlueprint* Blueprint, UEdGraph* Graph, const FString& CommentText)
{
	if (!Blueprint || !Graph)
		throw std::runtime_error("Blueprint or Graph is null");

	FGraphNodeCreator<UEdGraphNode_Comment> NodeCreator(*Graph);
	UEdGraphNode_Comment* CommentNode = NodeCreator.CreateNode();
	CommentNode->NodeComment = CommentText;
	SetNewNodePosition(Graph, CommentNode);
	NodeCreator.Finalize();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
}

/*
void GraphUtils::AddSwitchEnumNodeToGraph(UBlueprint* Blueprint, UEdGraph* Graph, const FString& PinTypeName)
{
	if (!Blueprint || !Graph)
		throw std::runtime_error("Blueprint or Graph is null");

	UEnum* Enum = EnumUtils::FindEnumByName(PinTypeName);
	if (!Enum)
		throw std::runtime_error(TCHAR_TO_UTF8(*FString::Printf(TEXT("Enum %s not found (try Module.EnumName or /Script/Module.EnumName, e.g. /Script/Engine.ECollisionChannel)"), *PinTypeName)));
	
	FGraphNodeCreator<UK2Node_SwitchEnum> NodeCreator(*Graph);
	UK2Node_SwitchEnum* SwitchEnumNode = NodeCreator.CreateNode();
	// some reason SetEnum doesn't work
	// unresolved external symbol "public: void __cdecl UK2Node_SwitchEnum::SetEnum(class UEnum *)"
	SwitchEnumNode->SetEnum(Enum);
	NodeCreator.Finalize();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
}*/

UClass* GraphUtils::FindK2NodeClassByName(const FString& NodeClassName)
{
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (Class->IsChildOf(UEdGraphNode::StaticClass()) && Class->GetName() == NodeClassName)
		{
			return Class;
		}
	}
	return nullptr;
}

static const FString StandardMacrosBlueprintPath(TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros"));

UEdGraph* GraphUtils::FindMacroGraphByName(const FString& MacroGraphName)
{
	UBlueprint* MacroBP = BPUtils::LoadBlueprint(StandardMacrosBlueprintPath);
	if (!MacroBP) return nullptr;
	const FName TargetName(*MacroGraphName);
	for (UEdGraph* MacroGraph : MacroBP->MacroGraphs)
	{
		if (MacroGraph && MacroGraph->GetFName() == TargetName)
			return MacroGraph;
	}
	return nullptr;
}

void GraphUtils::AddNodeByNameToGraph(UBlueprint* Blueprint, UEdGraph* Graph, const FString& NodeTypeName)
{
	if (!Blueprint || !Graph)
		throw std::runtime_error("Blueprint or Graph is null");

	// Unified interface: "ClassName::FunctionName" adds a library function call node (same as add_function_call_to_graph)
	TArray<FString> ClassAndFunc;
	NodeTypeName.ParseIntoArray(ClassAndFunc, TEXT("::"), true);
	if (ClassAndFunc.Num() == 2)
	{
		FString ClassToCall = ClassAndFunc[0].TrimStartAndEnd();
		FString FunctionName = ClassAndFunc[1].TrimStartAndEnd();
		if (!ClassToCall.IsEmpty() && !FunctionName.IsEmpty())
		{
			AddFunctionCallToGraph(Blueprint, Graph, ClassToCall, FunctionName);
			return;
		}
	}

	UClass* NewNodeClass = FindK2NodeClassByName(NodeTypeName);
	if (NewNodeClass)
	{
		auto NewNode = NewObject<UK2Node>(Graph, NewNodeClass);
		if (!NewNode)
			throw std::runtime_error("Failed to create node");
		NewNode->CreateNewGuid();
		NewNode->PostPlacedNewNode();
		NewNode->AllocateDefaultPins();
		SetNewNodePosition(Graph, NewNode);
		Graph->AddNode(NewNode);
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		return;
	}

	// Not a K2 class name; try resolving as a macro graph name (e.g. ForEachLoop, DoOnce)
	UEdGraph* MacroGraph = FindMacroGraphByName(NodeTypeName);
	if (MacroGraph)
	{
		FGraphNodeCreator<UK2Node_MacroInstance> NodeCreator(*Graph);
		UK2Node_MacroInstance* MacroNode = NodeCreator.CreateNode();
		MacroNode->SetMacroGraph(MacroGraph);
		SetNewNodePosition(Graph, MacroNode);
		NodeCreator.Finalize();
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		return;
	}

	throw std::runtime_error(TCHAR_TO_UTF8(*FString::Printf(TEXT("Node type %s not found"), *NodeTypeName)));
}

void GraphUtils::AddDynamicCastNodeToGraph(UBlueprint* Blueprint, UEdGraph* Graph, const FString& PinTypeName)
{
	if (!Blueprint || !Graph)
		throw std::runtime_error("Blueprint or Graph is null");

	auto TargetClass = ClassUtils::FindClassByName(PinTypeName);

	if (!TargetClass)
		throw std::runtime_error(TCHAR_TO_UTF8(*FString::Printf(TEXT("Class %s not found"), *PinTypeName)));

	FGraphNodeCreator<UK2Node_DynamicCast> NodeCreator(*Graph);
	UK2Node_DynamicCast* CastNode = NodeCreator.CreateNode();
	CastNode->TargetType = TargetClass;
	SetNewNodePosition(Graph, CastNode);
	NodeCreator.Finalize();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
}

void GraphUtils::AddClassCastNodeToGraph(UBlueprint* Blueprint, UEdGraph* Graph, const FString& PinTypeName)
{
	if (!Blueprint || !Graph)
		throw std::runtime_error("Blueprint or Graph is null");
	auto TargetClass = ClassUtils::FindClassByName(PinTypeName);
	if (!TargetClass)
		throw std::runtime_error(TCHAR_TO_UTF8(*FString::Printf(TEXT("Class %s not found"), *PinTypeName)));

	FGraphNodeCreator<UK2Node_ClassDynamicCast> NodeCreator(*Graph);
	UK2Node_ClassDynamicCast* CastNode = NodeCreator.CreateNode();
	CastNode->TargetType = TargetClass;
	SetNewNodePosition(Graph, CastNode);
	NodeCreator.Finalize();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
}

void GraphUtils::AddByteToEnumNodeCastToGraph(UBlueprint* Blueprint, UEdGraph* Graph, const FString& PinTypeName)
{
	if (!Blueprint || !Graph)
		throw std::runtime_error("Blueprint or Graph is null");

	UEnum* Enum = EnumUtils::FindEnumByName(PinTypeName);
	if (!Enum)
		throw std::runtime_error(TCHAR_TO_UTF8(*FString::Printf(TEXT("Enum %s not found (try Module.EnumName or /Script/Module.EnumName, e.g. /Script/Engine.ECollisionChannel)"), *PinTypeName)));

	FGraphNodeCreator<UK2Node_CastByteToEnum> NodeCreator(*Graph);
	UK2Node_CastByteToEnum* CastNode = NodeCreator.CreateNode();
	CastNode->Enum = Enum;
	SetNewNodePosition(Graph, CastNode);
	NodeCreator.Finalize();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
}


TArray<FString> GraphUtils::GetSupportedNode()
{
	if (GSupportedNodesCacheValid)
	{
		return GSupportedNodesCache;
	}

	TArray<FString> NodeTypes;

	// 1. K2 node class names
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (Class->IsChildOf(UK2Node::StaticClass()) && !Class->HasAnyClassFlags(CLASS_Abstract))
		{
			NodeTypes.Add(Class->GetName());
		}
	}

	// 2. Macro graph names from engine StandardMacros
	UBlueprint* MacroBP = BPUtils::LoadBlueprint(StandardMacrosBlueprintPath);
	if (MacroBP)
	{
		for (UEdGraph* MacroGraph : MacroBP->MacroGraphs)
		{
			if (MacroGraph) NodeTypes.Add(MacroGraph->GetFName().ToString());
		}
	}

	// 3. Library functions: UBlueprintFunctionLibrary subclasses, each callable function as "ClassName::FunctionName"
	UClass* BPLibClass = UBlueprintFunctionLibrary::StaticClass();
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (!Class->IsChildOf(BPLibClass) || Class->HasAnyClassFlags(CLASS_Abstract))
		{
			continue;
		}
		FString ClassDisplayName = GetLibraryClassDisplayName(Class);
		if (ClassDisplayName.IsEmpty()) continue;

		for (TFieldIterator<UFunction> FuncIt(Class, EFieldIteratorFlags::ExcludeSuper); FuncIt; ++FuncIt)
		{
			UFunction* Function = *FuncIt;
			if (UEdGraphSchema_K2::CanUserKismetCallFunction(Function))
			{
				NodeTypes.Add(ClassDisplayName + TEXT("::") + Function->GetName());
			}
		}
	}

	GSupportedNodesCache = MoveTemp(NodeTypes);
	GSupportedNodesCacheValid = true;
	return GSupportedNodesCache;
}

void GraphUtils::InvalidateSupportedNodesCache()
{
	GSupportedNodesCacheValid = false;
	GSupportedNodesCache.Empty();
}

UEdGraphNode* GraphUtils::GetNodeById(
	UEdGraph* Graph,
	const FGuid& NodeId)
{
	if (!Graph)
		throw std::runtime_error("Graph is null");

	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (!Node)
			continue;
		if (Node->NodeGuid == NodeId)
		{
			return Node;
		}
	}

	return nullptr;
}

void GraphUtils::ConnectPins(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const FGuid& OutputNodeId, const FString& OutputPinName,
	const FGuid& InputNodeId, const FString& InputPinName)
{
	if (!Blueprint)
		throw std::runtime_error("Blueprint is null");
    
	if (!Graph)
		throw std::runtime_error("Graph is null");

	UEdGraphNode* OutputNode = GetNodeById(Graph, OutputNodeId);
	UEdGraphNode* InputNode = GetNodeById(Graph, InputNodeId);

	if (!OutputNode || !InputNode)
		throw std::runtime_error("OutputNode or InputNode not found");

	// Find pins
	UEdGraphPin* OutPin = OutputNode->FindPin(OutputPinName);
	UEdGraphPin* InPin = InputNode->FindPin(InputPinName);

	if (!OutPin || !InPin)
		throw std::runtime_error("OutPin or InPin not found");

	// When connecting Exec→Exec, if target has no exec input yet, place it to the right of source
	if (OutPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec &&
		InPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
	{
		bool bTargetHasExecInput = false;
		for (UEdGraphPin* P : InputNode->Pins)
		{
			if (P && P->Direction == EGPD_Input && P->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec && P->LinkedTo.Num() > 0)
			{
				bTargetHasExecInput = true;
				break;
			}
		}
		if (!bTargetHasExecInput)
		{
			InputNode->NodePosX = OutputNode->NodePosX + FMath::Max(OutputNode->NodeWidth, 1) + DefaultNewNodeSpacing;
			InputNode->NodePosY = OutputNode->NodePosY;
		}
	}

	OutPin->MakeLinkTo(InPin);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
}

void GraphUtils::BreakPinConnection(UBlueprint* Blueprint, UEdGraph* Graph, const FGuid& OutputNodeId,
	const FString& OutputPinName, const FGuid& InputNodeId, const FString& InputPinName)
{
	
	if (!Blueprint)
		throw std::runtime_error("Blueprint is null");
    
	if (!Graph)
		throw std::runtime_error("Graph is null");

	UEdGraphNode* OutputNode = GetNodeById(Graph, OutputNodeId);
	UEdGraphNode* InputNode = GetNodeById(Graph, InputNodeId);

	if (!OutputNode || !InputNode)
		throw std::runtime_error("OutputNode or InputNode not found");

	// Find pins
	UEdGraphPin* OutPin = OutputNode->FindPin(OutputPinName);
	UEdGraphPin* InPin = InputNode->FindPin(InputPinName);

	if (!OutPin || !InPin)
		throw std::runtime_error("OutPin or InPin not found");

	OutPin->BreakLinkTo(InPin);

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
}

void GraphUtils::SetPinDefaultValue(
	UBlueprint* Blueprint,
	UEdGraph* Graph,
	const FGuid& NodeId,
	const FString& PinName,
	const FString& DefaultValue)
{
	if (!Blueprint)
		throw std::runtime_error("Blueprint is null");
    
	if (!Graph)
		throw std::runtime_error("Graph is null");

	UEdGraphNode* Node = GetNodeById(Graph, NodeId);

	if (!Node)
		throw std::runtime_error("Node not found");

	UEdGraphPin* Pin = Node->FindPin(PinName);
	if (!Pin)
		throw std::runtime_error("Pin not found");

	Pin->DefaultValue = DefaultValue;

	Node->PinDefaultValueChanged(Pin);
}

