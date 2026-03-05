#include "Graph.h"

#include "UE5_MCP/API/Utils.h"
#include "UE5_MCP/API/DTO/Graph/GraphOperationReq.h"
#include "UE5_MCP/Core/Layout/BlueprintGraphLayout.h"
#include "UE5_MCP/API/DTO/Graph/GenericAddNodeToGraphReq.h"
#include "UE5_MCP/API/DTO/Graph/PinOperationReq.h"
#include "UE5_MCP/API/DTO/Graph/SetPinDefaultValueReq.h"
#include "UE5_MCP/Core/BPUtils.h"
#include "UE5_MCP/Core/GraphUtils.h"

/** Resolve graph by name: event graph (UbergraphPages) first, then function graph (FunctionGraphs). */
static UEdGraph* GetGraphByName(UBlueprint* BP, const FString& GraphName)
{
	if (!BP) return nullptr;
	UEdGraph* G = BPUtils::GetEventGraph(BP, GraphName);
	if (!G) G = BPUtils::GetFunctionGraph(BP, GraphName);
	return G;
}

/** Resolve short class/type name to engine path for FindClassByName. Pass-through if already contains "/Script/" or "::". */
static FString ResolveClassName(const FString& Name)
{
	if (Name.IsEmpty() || Name.Contains(TEXT("/Script/")) || Name.Contains(TEXT("::"))) return Name;
	static const TMap<FString, FString> ClassAliases = {
		{TEXT("Actor"), TEXT("/Script/Engine.Actor")},
		{TEXT("KismetMathLibrary"), TEXT("/Script/Engine.KismetMathLibrary")},
		{TEXT("UKismetMathLibrary"), TEXT("/Script/Engine.KismetMathLibrary")},
		{TEXT("BlueprintMapLibrary"), TEXT("/Script/Engine.BlueprintMapLibrary")},
		{TEXT("GameModeBase"), TEXT("/Script/Engine.GameModeBase")},
		{TEXT("Pawn"), TEXT("/Script/Engine.Pawn")},
		{TEXT("Character"), TEXT("/Script/Engine.Character")},
		{TEXT("StaticMeshComponent"), TEXT("/Script/Engine.StaticMeshComponent")},
	};
	const FString* Resolved = ClassAliases.Find(Name);
	return Resolved ? *Resolved : Name;
}

/** Resolve friendly node_type_name alias to K2 class name or macro name. Pass-through if already K2 name or contains "::".
 *  If not in alias map and name does not start with "K2Node_", try "K2Node_" + name for engine K2 node class names. */
static FString ResolveNodeTypeName(const FString& NodeTypeName)
{
	if (NodeTypeName.Contains(TEXT("::"))) return NodeTypeName;
	static const TMap<FString, FString> AliasMap = {
		{TEXT("CallFunction"), TEXT("K2Node_CallFunction")},
		{TEXT("FunctionCall"), TEXT("K2Node_CallFunction")},
		{TEXT("VariableGet"), TEXT("K2Node_VariableGet")},
		{TEXT("VariableSet"), TEXT("K2Node_VariableSet")},
		{TEXT("Branch"), TEXT("K2Node_IfThenElse")},
		{TEXT("If"), TEXT("K2Node_IfThenElse")},
		{TEXT("Switch"), TEXT("K2Node_SwitchEnum")},
		{TEXT("Select"), TEXT("K2Node_SwitchEnum")},
		{TEXT("Event"), TEXT("K2Node_Event")},
		{TEXT("CustomEvent"), TEXT("K2Node_CustomEvent")},
		{TEXT("MakeStruct"), TEXT("K2Node_MakeStruct")},
		{TEXT("BreakStruct"), TEXT("K2Node_BreakStruct")},
		{TEXT("DynamicCast"), TEXT("K2Node_DynamicCast")},
		{TEXT("ClassCast"), TEXT("K2Node_ClassDynamicCast")},
		{TEXT("EnumCast"), TEXT("K2Node_CastByteToEnum")},
		{TEXT("Comment"), TEXT("K2Node_Comment")},
	};
	const FString* Resolved = AliasMap.Find(NodeTypeName);
	if (Resolved) return *Resolved;
	// Allow omitting K2Node_ prefix: e.g. IfThenElse -> K2Node_IfThenElse, PlayAnimation -> K2Node_PlayAnimation
	if (!NodeTypeName.StartsWith(TEXT("K2Node_")))
		return FString(TEXT("K2Node_")) + NodeTypeName;
	return NodeTypeName;
}

static void SendOk(const FHttpResultCallback& OnComplete)
{
	TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create("OK", TEXT("text/plain"));
	Resp->Code = EHttpServerResponseCodes::Ok;
	OnComplete(MoveTemp(Resp));
}

/** Handler for node types that require ExtraInfo (struct/class/enum name or comment text). */
using FAddExtraInfoNodeFunc = void(*)(UBlueprint*, UEdGraph*, const FString&);

static const TMap<FString, TPair<FString, FAddExtraInfoNodeFunc>>& GetExtraInfoNodeTable()
{
	static const TMap<FString, TPair<FString, FAddExtraInfoNodeFunc>> Table = {
		{TEXT("K2Node_MakeStruct"),     {TEXT("Error: ExtraInfo (struct type name) is required for MakeStruct"),     &GraphUtils::AddMakeStructNodeToGraph}},
		{TEXT("K2Node_BreakStruct"),    {TEXT("Error: ExtraInfo (struct type name) is required for BreakStruct"),    &GraphUtils::AddBreakStructNodeToGraph}},
		{TEXT("K2Node_DynamicCast"),    {TEXT("Error: ExtraInfo (class name) is required for DynamicCast"),         &GraphUtils::AddDynamicCastNodeToGraph}},
		{TEXT("K2Node_ClassDynamicCast"), {TEXT("Error: ExtraInfo (class name) is required for ClassCast"),         &GraphUtils::AddClassCastNodeToGraph}},
		{TEXT("K2Node_CastByteToEnum"), {TEXT("Error: ExtraInfo (enum name) is required for EnumCast"),              &GraphUtils::AddByteToEnumNodeCastToGraph}},
	};
	return Table;
}

/** Returns true if ResolvedType was handled (node added or validation error set in OutError). */
static bool TryAddExtraInfoNode(UBlueprint* BP, UEdGraph* Graph, const FString& ResolvedType,
	const FGenericAddNodeToGraphReq& Body, FString& OutError)
{
	if (const TPair<FString, FAddExtraInfoNodeFunc>* Handler = GetExtraInfoNodeTable().Find(ResolvedType))
	{
		if (Body.ExtraInfo.IsEmpty()) { OutError = Handler->Key; return true; }
		FString ExtraInfo = Body.ExtraInfo;
		if (ResolvedType == TEXT("K2Node_DynamicCast") || ResolvedType == TEXT("K2Node_ClassDynamicCast"))
		{
			ExtraInfo = ResolveClassName(ExtraInfo);
		}
		Handler->Value(BP, Graph, ExtraInfo);
		return true;
	}
	if (ResolvedType == TEXT("K2Node_Comment"))
	{
		GraphUtils::AddCommentNodeToGraph(BP, Graph, Body.ExtraInfo);
		return true;
	}
	return false;
}

static void SendError(const FHttpResultCallback& OnComplete, const FString& Message, EHttpServerResponseCodes Code = EHttpServerResponseCodes::ServerError)
{
	TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create(Message, TEXT("text/plain"));
	Resp->Code = Code;
	OnComplete(MoveTemp(Resp));
}

/** Runs Op(); on success calls SendOk, on std::runtime_error calls SendError. Returns true. */
template<typename FOp>
static bool RunGraphOp(const FHttpResultCallback& OnComplete, FOp&& Op)
{
	try
	{
		Op();
		SendOk(OnComplete);
		return true;
	}
	catch (std::runtime_error& e)
	{
		SendError(OnComplete, FString::Printf(TEXT("Error: %s"), UTF8_TO_TCHAR(e.what())));
		return true;
	}
}

bool AddNodeToGraphHandler(const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete)
{
	auto Fail = [&OnComplete](const FString& Msg) { SendError(OnComplete, Msg); };
	try
	{
		FGenericAddNodeToGraphReq body = Utils::BufferToJson<FGenericAddNodeToGraphReq>(Req.Body);
		if (body.BpPath.IsEmpty()) { Fail(TEXT("Error: BpPath is required")); return true; }
		if (body.NodeTypeName.IsEmpty()) { Fail(TEXT("Error: NodeTypeName is required")); return true; }

		UBlueprint* BP = BPUtils::LoadBlueprint(body.BpPath);
		if (!BP) { Fail(TEXT("Error: Blueprint not found")); return true; }

		FString GraphName = body.GraphName.IsEmpty() ? TEXT("EventGraph") : body.GraphName;
		UEdGraph* Graph = GetGraphByName(BP, GraphName);
		if (!Graph) { Fail(FString::Printf(TEXT("Error: Graph %s not found"), *GraphName)); return true; }

		const FString& NodeTypeName = body.NodeTypeName;

		// ClassName::FunctionName -> AddFunctionCallToGraph
		if (NodeTypeName.Contains(TEXT("::")))
		{
			TArray<FString> Parts;
			NodeTypeName.ParseIntoArray(Parts, TEXT("::"), true);
			if (Parts.Num() == 2)
			{
				FString ClassToCall = ResolveClassName(Parts[0].TrimStartAndEnd());
				FString FunctionName = Parts[1].TrimStartAndEnd();
				if (!ClassToCall.IsEmpty() && !FunctionName.IsEmpty())
				{
					GraphUtils::AddFunctionCallToGraph(BP, Graph, ClassToCall, FunctionName);
					SendOk(OnComplete);
					return true;
				}
			}
		}

		FString ResolvedType = ResolveNodeTypeName(NodeTypeName);

		// CallFunction: require functionName or return explicit error
		if (ResolvedType == TEXT("K2Node_CallFunction"))
		{
			FString ClassToCall = body.ClassToCall.IsEmpty() ? FString() : ResolveClassName(body.ClassToCall.TrimStartAndEnd());
			FString FunctionName = body.FunctionName.IsEmpty() ? FString() : body.FunctionName.TrimStartAndEnd();
			if (FunctionName.IsEmpty())
			{
				Fail(TEXT("Error: CallFunction requires functionName (and optionally classToCall). Use node_type_name 'ClassName::FunctionName' or provide functionName (+ classToCall)."));
				return true;
			}
			ClassToCall.IsEmpty() ? GraphUtils::AddFunctionCallToGraph(BP, Graph, FunctionName)
				: GraphUtils::AddFunctionCallToGraph(BP, Graph, ClassToCall, FunctionName);
			SendOk(OnComplete);
			return true;
		}

		// VariableGet / VariableSet
		if (ResolvedType == TEXT("K2Node_VariableGet") || ResolvedType == TEXT("K2Node_VariableSet"))
		{
			FString VarName = body.VarName.IsEmpty() ? body.MemberName : body.VarName;
			if (VarName.IsEmpty()) { Fail(TEXT("Error: VarName is required for variable node")); return true; }
			(body.bIsSetter || ResolvedType == TEXT("K2Node_VariableSet"))
				? GraphUtils::AddSetVariableNodeToGraph(BP, Graph, VarName)
				: GraphUtils::AddGetVariableNodeToGraph(BP, Graph, VarName);
			SendOk(OnComplete);
			return true;
		}

		// Event / CustomEvent
		if (ResolvedType == TEXT("K2Node_Event") || ResolvedType == TEXT("K2Node_CustomEvent"))
		{
			FString EventName = body.EventName.IsEmpty() ? body.MemberName : body.EventName;
			if (EventName.IsEmpty()) { Fail(TEXT("Error: EventName is required for event node")); return true; }
			(body.bIsCustomEvent || ResolvedType == TEXT("K2Node_CustomEvent"))
				? GraphUtils::AddCustomEventToGraph(BP, Graph, EventName, body.EventSignature)
				: GraphUtils::AddEventToGraph(BP, Graph, EventName);
			SendOk(OnComplete);
			return true;
		}

		// MakeStruct / BreakStruct / DynamicCast / ClassCast / EnumCast / Comment (table-driven)
		FString ExtraError;
		if (TryAddExtraInfoNode(BP, Graph, ResolvedType, body, ExtraError))
		{
			if (!ExtraError.IsEmpty()) { Fail(ExtraError); return true; }
			SendOk(OnComplete);
			return true;
		}

		// Fallback: K2 class or macro
		GraphUtils::AddNodeByNameToGraph(BP, Graph, ResolvedType);
		SendOk(OnComplete);
		return true;
	}
	catch (std::runtime_error& e)
	{
		SendError(OnComplete, FString::Printf(TEXT("Error: %s"), UTF8_TO_TCHAR(e.what())));
		return true;
	}
}

bool ConnectPinsHandler(const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete)
{
	return RunGraphOp(OnComplete, [&]()
	{
		FPinOperationReq body = Utils::BufferToJson<FPinOperationReq>(Req.Body);
		UBlueprint* BP = BPUtils::LoadBlueprint(body.BpPath);
		UEdGraph* Graph = GetGraphByName(BP, body.GraphName);
		if (!Graph) throw std::runtime_error(TCHAR_TO_UTF8(*FString::Printf(TEXT("Graph %s not found"), *body.GraphName)));
		GraphUtils::ConnectPins(BP, Graph, body.OutputNodeId, body.OutputPinName, body.InputNodeId, body.InputPinName);
	});
}

bool BreakPinConnectionHandler(const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete)
{
	return RunGraphOp(OnComplete, [&]()
	{
		FPinOperationReq body = Utils::BufferToJson<FPinOperationReq>(Req.Body);
		UBlueprint* BP = BPUtils::LoadBlueprint(body.BpPath);
		UEdGraph* Graph = GetGraphByName(BP, body.GraphName);
		if (!Graph) throw std::runtime_error(TCHAR_TO_UTF8(*FString::Printf(TEXT("Graph %s not found"), *body.GraphName)));
		GraphUtils::BreakPinConnection(BP, Graph, body.OutputNodeId, body.OutputPinName, body.InputNodeId, body.InputPinName);
	});
}

bool SetPinDefaultValueHandler(const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete)
{
	return RunGraphOp(OnComplete, [&]()
	{
		FSetPinDefaultValueReq body = Utils::BufferToJson<FSetPinDefaultValueReq>(Req.Body);
		UBlueprint* BP = BPUtils::LoadBlueprint(body.BpPath);
		UEdGraph* Graph = GetGraphByName(BP, body.GraphName);
		if (!Graph) throw std::runtime_error(TCHAR_TO_UTF8(*FString::Printf(TEXT("Graph %s not found"), *body.GraphName)));
		GraphUtils::SetPinDefaultValue(BP, Graph, body.NodeId, body.PinName, body.DefaultValue);
	});
}

bool GetSupportedNodesHandler(const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete)
{
	try
	{
		TArray<FString> Nodes = GraphUtils::GetSupportedNode();
		TUniquePtr<FHttpServerResponse> Resp = FHttpServerResponse::Create(Utils::ToJsonString(Nodes), TEXT("application/json"));
		Resp->Code = EHttpServerResponseCodes::Ok;
		OnComplete(MoveTemp(Resp));
		return true;
	}
	catch (const std::runtime_error& e)
	{
		SendError(OnComplete, FString::Printf(TEXT("Error: %s"), UTF8_TO_TCHAR(e.what())));
		return true;
	}
	catch (...)
	{
		SendError(OnComplete, TEXT("Error: unknown exception in get_supported_nodes"));
		return true;
	}
}

bool RefreshSupportedNodesCacheHandler(const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete)
{
	GraphUtils::InvalidateSupportedNodesCache();
	SendOk(OnComplete);
	return true;
}

bool LayoutGraphHandler(const FHttpServerRequest& Req, const FHttpResultCallback& OnComplete)
{
	return RunGraphOp(OnComplete, [&]()
	{
		FGraphOperationReq body = Utils::BufferToJson<FGraphOperationReq>(Req.Body);
		UBlueprint* BP = BPUtils::LoadBlueprint(body.BpPath);
		UEdGraph* Graph = GetGraphByName(BP, body.GraphName);
		if (!Graph) throw std::runtime_error(TCHAR_TO_UTF8(*FString::Printf(TEXT("Graph %s not found"), *body.GraphName)));
		FBlueprintGraphLayout::LayoutGraph(BP, Graph);
	});
}
