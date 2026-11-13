"""StateTree 구조 편집 툴세트.

엔진 StateTreeTools는 조회 전용이므로 ActionPracticeEditor 모듈의
UActionPracticeStateTreeEditLibrary를 통해 스테이트/노드/트랜지션을 편집한다.
스테이트는 "Root/Combat/Attack" 형식의 이름 경로로 지정한다.
노드 프로퍼티 바인딩 설정·제거·조회도 지원하며, 편집 후 StateTreeCompileTools.compile_state_tree로 컴파일해야 한다.
"""

import unreal

import toolset_registry


def _library():
    library = getattr(unreal, 'ActionPracticeStateTreeEditLibrary', None)

    if library is None:
        raise RuntimeError(
            'ActionPracticeStateTreeEditLibrary not found. Build the ActionPracticeEditor module and restart the editor.')

    return library


def _find_state(state_tree, state_path):
    state = _library().find_state_by_path(state_tree, state_path)

    if state is None:
        raise ValueError(f'State "{state_path}" not found in {state_tree.get_path_name()}')

    return state


def _find_optional_state(state_tree, state_path):
    return _find_state(state_tree, state_path) if state_path else None


def _parse_guid(text):
    guid, succeeded = unreal.GuidLibrary.parse_string_to_guid(text)

    if not succeeded:
        raise ValueError(f'Invalid GUID "{text}"')

    return guid


def _guid_to_string(guid):
    #8-4-4-4-12 하이픈 형식 (describe 출력과 동일)
    digits = unreal.GuidLibrary.conv_guid_to_string(guid)
    return f'{digits[0:8]}-{digits[8:12]}-{digits[12:16]}-{digits[16:20]}-{digits[20:32]}'


def _raise_if_error(error):
    if error:
        raise RuntimeError(error)


def _state_path(state):
    return _library().get_state_path(state)


@unreal.uclass()
class StateTreeEditTools(unreal.ToolsetDefinition):
    """Provides tools for editing StateTree structure: states, tasks, conditions and transitions.

    States are addressed by name path such as "Root/Combat/Attack". Nodes and transitions are
    addressed by the GUID shown by describe_state_tree. Property bindings are handled by
    get_binding_sources / set_property_binding / remove_property_binding / describe_bindings.
    After editing, call StateTreeCompileTools.compile_state_tree to update runtime data."""

    @toolset_registry.tool_call
    @staticmethod
    def describe_state_tree(state_tree: unreal.StateTree) -> str:
        """Returns the full StateTree outline: schema, evaluators, global tasks, and each state
        with its path, type, selection behavior, enter conditions, tasks, transitions and IDs.

        Args:
            state_tree: The StateTree asset.

        Returns:
            Indented text outline.
        """
        return _library().describe_state_tree(state_tree)

    @toolset_registry.tool_call
    @staticmethod
    def add_state(state_tree: unreal.StateTree, parent_path: str, state_name: str, state_type: str = 'State', index: int = -1) -> str:
        """Adds a new state.

        Args:
            state_tree: The StateTree asset.
            parent_path: Path of the parent state. Pass an empty string to add a top-level subtree.
            state_name: Name of the new state.
            state_type: State, Group, Linked, LinkedAsset or Subtree.
            index: Insert position among siblings. -1 appends.

        Returns:
            Path of the new state.
        """
        parent = _find_optional_state(state_tree, parent_path)
        state, error = _library().add_state(state_tree, parent, state_name, state_type, index)
        _raise_if_error(error)
        return _state_path(state)

    @toolset_registry.tool_call
    @staticmethod
    def remove_state(state_tree: unreal.StateTree, state_path: str) -> bool:
        """Removes a state and all of its children, then removes bindings left invalid.

        Args:
            state_tree: The StateTree asset.
            state_path: Path of the state to remove.

        Returns:
            True on success.
        """
        _raise_if_error(_library().remove_state(state_tree, _find_state(state_tree, state_path)))
        return True

    @toolset_registry.tool_call
    @staticmethod
    def move_state(state_tree: unreal.StateTree, state_path: str, target_path: str, placement: str) -> str:
        """Moves a state relative to another state in the same tree.

        Args:
            state_tree: The StateTree asset.
            state_path: Path of the state to move.
            target_path: Path of the reference state.
            placement: Before, After or Into (becomes last child of the target).

        Returns:
            New path of the moved state.
        """
        state = _find_state(state_tree, state_path)
        _raise_if_error(_library().move_state(state_tree, state, _find_state(state_tree, target_path), placement))
        return _state_path(state)

    @toolset_registry.tool_call
    @staticmethod
    def copy_state_from(source_tree: unreal.StateTree, source_path: str, target_tree: unreal.StateTree, target_parent_path: str, new_name: str) -> str:
        """Copies a state with all children, tasks, conditions and transitions from a source tree
        (can be the same tree) and appends it under a target parent.

        Node IDs are regenerated. Transitions and bindings inside the copied subtree are remapped;
        transitions pointing outside it keep the old target and must be re-targeted with
        set_transition_target. Bindings to the source tree's context or parameters may need
        rebinding by the user.

        Args:
            source_tree: StateTree to copy from.
            source_path: Path of the state to copy.
            target_tree: StateTree to paste into.
            target_parent_path: Parent state path in the target. Empty string pastes as a top-level subtree.
            new_name: Name for the pasted state. Empty string keeps the source name. Give a new name
                when pasting next to a sibling with the same name, otherwise paths become ambiguous.

        Returns:
            Path of the pasted state.
        """
        source_state = _find_state(source_tree, source_path)
        target_parent = _find_optional_state(target_tree, target_parent_path)
        state, error = _library().copy_state_from(source_tree, source_state, target_tree, target_parent)
        _raise_if_error(error)

        #같은 이름 형제가 있으면 경로 검색이 원본을 찾으므로 반환된 객체에 직접 이름 지정
        if new_name:
            _raise_if_error(_library().set_state_property(target_tree, state, 'Name', new_name))

        return _state_path(state)

    @toolset_registry.tool_call
    @staticmethod
    def set_state_property(state_tree: unreal.StateTree, state_path: str, property_path: str, value: str) -> bool:
        """Sets a field on the state itself using Unreal text format.

        Examples: ("Name", "Attack"), ("Type", "Group"), ("SelectionBehavior", "TrySelectChildrenAtRandom"),
        ("Tag", "(TagName=\\"State.Combat\\")"), ("bEnabled", "False"), ("Weight", "2.0").

        Args:
            state_tree: The StateTree asset.
            state_path: Path of the state.
            property_path: Property name, '.' separated for nested structs.
            value: Value in Unreal text export format.

        Returns:
            True on success.
        """
        _raise_if_error(_library().set_state_property(state_tree, _find_state(state_tree, state_path), property_path, value))
        return True

    @toolset_registry.tool_call
    @staticmethod
    def add_node(state_tree: unreal.StateTree, state_path: str, node_kind: str, node_type: str, transition_id: str = '') -> str:
        """Adds a task or condition node with default values.

        Args:
            state_tree: The StateTree asset.
            state_path: Path of the owning state.
            node_kind: Task, EnterCondition or TransitionCondition.
            node_type: C++ node struct path (e.g. /Script/StateTreeModule.StateTreeDelayTask) or a
                Blueprint task/condition class path (e.g. /Game/AI/BP_MyTask.BP_MyTask_C).
            transition_id: Transition GUID, required only for TransitionCondition. Pass an empty string otherwise.

        Returns:
            GUID of the new node.
        """
        state = _find_state(state_tree, state_path)
        transition_guid = _parse_guid(transition_id) if transition_id else unreal.Guid()
        node_id, error = _library().add_node(state_tree, state, node_kind, node_type, transition_guid)
        _raise_if_error(error)
        return _guid_to_string(node_id)

    @toolset_registry.tool_call
    @staticmethod
    def remove_node(state_tree: unreal.StateTree, state_path: str, node_id: str) -> bool:
        """Removes a task, enter condition, transition condition or transition by GUID.

        Args:
            state_tree: The StateTree asset.
            state_path: Path of the owning state.
            node_id: GUID of the node or transition.

        Returns:
            True on success.
        """
        state = _find_state(state_tree, state_path)
        _raise_if_error(_library().remove_node(state_tree, state, _parse_guid(node_id)))
        return True

    @toolset_registry.tool_call
    @staticmethod
    def copy_node_from(source_tree: unreal.StateTree, source_path: str, node_id: str, target_tree: unreal.StateTree, target_path: str) -> str:
        """Copies a task, condition or transition (with its conditions and values) from a source
        state to a target state. Tasks go to Tasks, enter conditions go to EnterConditions.
        Transition conditions cannot be copied individually; copy the whole transition instead.
        Property bindings are NOT copied. A GotoState transition copied from another tree must be
        re-targeted with set_transition_target.

        Args:
            source_tree: StateTree to copy from.
            source_path: Path of the source state.
            node_id: GUID of the node or transition to copy.
            target_tree: StateTree to paste into.
            target_path: Path of the target state.

        Returns:
            Comma separated GUIDs that were newly created on the target state.
        """
        source_state = _find_state(source_tree, source_path)
        target_state = _find_state(target_tree, target_path)
        new_ids, error = _library().copy_node_from(source_tree, source_state, _parse_guid(node_id), target_tree, target_state)
        _raise_if_error(error)
        result = ','.join(_guid_to_string(guid) for guid in new_ids)

        #다른 트리에서 복사한 GotoState 트랜지션은 원본 트리 스테이트를 가리키므로 호출자에게 알림
        if source_tree.get_path_name() != target_tree.get_path_name():
            result += ' (copied across trees: if this is a GotoState transition, re-target it with set_transition_target)'

        return result

    @toolset_registry.tool_call
    @staticmethod
    def set_node_property(state_tree: unreal.StateTree, state_path: str, node_id: str, property_path: str, value: str) -> bool:
        """Sets a property on a node using Unreal text format. The node's instance data (task
        parameters) is used when it has the property, otherwise the node struct. If node_id is a transition GUID,
        sets fields on the transition (Priority, bDelayTransition, DelayDuration, Trigger,
        RequiredEvent.Tag, bTransitionEnabled ...).

        Condition expression fields of the editor node are also supported:
        ("ExpressionOperand", "Or"|"And"), ("ExpressionIndent", "1").

        Examples: ("Duration", "1.5"), ("AbilityClass", "/Game/GAS/BP_X.BP_X_C"),
        ("Priority", "High"), ("Tag", "(TagName=\\"Event.Hit\\")").

        Args:
            state_tree: The StateTree asset.
            state_path: Path of the owning state.
            node_id: GUID of the node or transition.
            property_path: Property name, '.' separated for nested structs.
            value: Value in Unreal text export format.

        Returns:
            True on success.
        """
        state = _find_state(state_tree, state_path)
        _raise_if_error(_library().set_node_property(state_tree, state, _parse_guid(node_id), property_path, value))
        return True

    @toolset_registry.tool_call
    @staticmethod
    def set_transition_target(state_tree: unreal.StateTree, state_path: str, transition_id: str, transition_type: str, target_path: str = '') -> bool:
        """Changes the destination of an existing transition.

        Args:
            state_tree: The StateTree asset.
            state_path: Path of the state owning the transition.
            transition_id: GUID of the transition.
            transition_type: GotoState, Succeeded, Failed, NextState, NextSelectableState, Parent ...
            target_path: Target state path, required for GotoState. Empty string otherwise.

        Returns:
            True on success.
        """
        state = _find_state(state_tree, state_path)
        target = _find_optional_state(state_tree, target_path)
        _raise_if_error(_library().set_transition_target(state_tree, state, _parse_guid(transition_id), transition_type, target))
        return True

    @toolset_registry.tool_call
    @staticmethod
    def add_transition(state_tree: unreal.StateTree, state_path: str, trigger: str, transition_type: str, target_path: str = '', event_tag: str = '', priority: str = '') -> str:
        """Adds a transition to a state.

        Args:
            state_tree: The StateTree asset.
            state_path: Path of the owning state.
            trigger: OnStateCompleted, OnStateSucceeded, OnStateFailed, OnTick, OnEvent or OnDelegate.
            transition_type: GotoState, Succeeded, Failed, NextState, NextSelectableState, Parent ...
            target_path: Target state path, required for GotoState. Empty string otherwise.
            event_tag: Gameplay tag name, required for OnEvent. Empty string otherwise.
            priority: Low, Normal, Medium, High or Critical. Empty string means Normal.

        Returns:
            GUID of the new transition.
        """
        state = _find_state(state_tree, state_path)
        target = _find_optional_state(state_tree, target_path)
        transition_id, error = _library().add_transition(state_tree, state, trigger, event_tag, transition_type, target, priority)
        _raise_if_error(error)
        return _guid_to_string(transition_id)

    @toolset_registry.tool_call
    @staticmethod
    def get_binding_sources(state_tree: unreal.StateTree, state_path: str, node_id: str) -> str:
        """Lists what a node property can be bound to: the target node's own properties and every
        source struct accessible from it (context, global evaluators/tasks, Parameters, parent
        state parameters, earlier tasks) with name, section, id and top-level properties.

        Args:
            state_tree: The StateTree asset.
            state_path: Path of the state owning the node. Empty string for global evaluators/tasks.
            node_id: GUID of the target node (task or condition).

        Returns:
            One line for the target, then one line per source: "Source <name> [section] id=<guid>: Prop(Type), ..."
        """
        state = _find_optional_state(state_tree, state_path)
        return _library().get_binding_sources(state_tree, state, _parse_guid(node_id))

    @toolset_registry.tool_call
    @staticmethod
    def set_property_binding(state_tree: unreal.StateTree, state_path: str, node_id: str, target_property: str, source_struct: str, source_property: str) -> bool:
        """Binds a node property to a property of an accessible source struct. Replaces an existing
        binding on the same target property. Validates accessibility and type compatibility.

        Examples: ("Left", "Update Target Info Evaluator", "DistanceToTarget"),
        ("Right", "Parameters", "AlertRange").

        Args:
            state_tree: The StateTree asset.
            state_path: Path of the state owning the node. Empty string for global evaluators/tasks.
            node_id: GUID of the target node.
            target_property: Internal property name on the target node, '.' separated for nested.
            source_struct: Source name, "Name [Section]" or id as printed by get_binding_sources.
                When several sources share the name, only those where source_property resolves are kept.
            source_property: Internal property name on the source, '.' separated for nested.
                Empty string binds the whole source struct / context object (e.g. an Actor or component).

        Returns:
            True on success.
        """
        state = _find_optional_state(state_tree, state_path)
        _raise_if_error(_library().set_property_binding(state_tree, state, _parse_guid(node_id), target_property, source_struct, source_property))
        return True

    @toolset_registry.tool_call
    @staticmethod
    def remove_property_binding(state_tree: unreal.StateTree, state_path: str, node_id: str, target_property: str) -> bool:
        """Removes the binding on a node property (and bindings on its sub-properties).

        Args:
            state_tree: The StateTree asset.
            state_path: Path of the state owning the node. Empty string for global evaluators/tasks.
            node_id: GUID of the target node.
            target_property: Internal property name on the target node.

        Returns:
            True on success. Raises if the property had no binding.
        """
        state = _find_optional_state(state_tree, state_path)
        _raise_if_error(_library().remove_property_binding(state_tree, state, _parse_guid(node_id), target_property))
        return True

    @toolset_registry.tool_call
    @staticmethod
    def describe_bindings(state_tree: unreal.StateTree, state_path: str = '') -> str:
        """Lists property bindings as "TargetNode(id).Property <- Source.Property".

        Args:
            state_tree: The StateTree asset.
            state_path: Only bindings targeting nodes of this state. Empty string lists every binding.

        Returns:
            One binding per line.
        """
        state = _find_optional_state(state_tree, state_path)
        return _library().describe_bindings(state_tree, state)

    @toolset_registry.tool_call
    @staticmethod
    def set_root_parameter(state_tree: unreal.StateTree, parameter_name: str, value: str, parameter_type: str = 'Float') -> bool:
        """Adds a StateTree root parameter (the global "Parameters" source) or sets its default value.
        If the parameter already exists, parameter_type is ignored and only the value is set.
        Bind it with set_property_binding(source_struct="Parameters", source_property=parameter_name).

        Args:
            state_tree: The StateTree asset.
            parameter_name: Parameter name, e.g. "WalkSpeed".
            value: Default value in Unreal text format, e.g. "300", "True"/"False" for Bool.
                Empty string keeps the current value (a new parameter keeps the type default).
            parameter_type: Type used when creating: Bool, Byte, Int32, Int64, Float, Double, Name, String, Text.

        Returns:
            True on success.
        """
        _raise_if_error(_library().set_root_parameter(state_tree, parameter_name, parameter_type, str(value)))
        return True

    @toolset_registry.tool_call
    @staticmethod
    def remove_root_parameter(state_tree: unreal.StateTree, parameter_name: str) -> bool:
        """Removes a StateTree root parameter. Fails and lists the users if any binding still uses it
        (remove those with remove_property_binding first).

        Args:
            state_tree: The StateTree asset.
            parameter_name: Parameter name, e.g. "WalkSpeed".

        Returns:
            True on success.
        """
        _raise_if_error(_library().remove_root_parameter(state_tree, parameter_name))
        return True
