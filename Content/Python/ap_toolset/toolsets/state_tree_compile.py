"""StateTree 컴파일 툴세트.

엔진 StateTreeToolset은 조회 전용이고 UStateTreeEditingSubsystem::CompileStateTree는
C++ 전용이므로, ActionPracticeEditor 모듈의 UActionPracticeEditorScriptLibrary를 통해 호출.
"""

import json

import unreal

import toolset_registry


def _library():
    library = getattr(unreal, 'ActionPracticeEditorScriptLibrary', None)

    if library is None:
        raise RuntimeError(
            'ActionPracticeEditorScriptLibrary not found. Build the ActionPracticeEditor module and restart the editor.')

    return library


@unreal.uclass()
class StateTreeCompileTools(unreal.ToolsetDefinition):
    """Provides tools for compiling StateTree assets after their editor data was modified
    (e.g. via ObjectTools.set_properties on the editor data, schema, or state tasks)."""

    @toolset_registry.tool_call
    @staticmethod
    def compile_state_tree(state_tree: unreal.StateTree, save: bool = True) -> str:
        """Compiles a StateTree asset so its runtime data matches the editor data.

        Editing a StateTree's editor data (states, tasks, schema) does not update the
        compiled runtime data used in game; this must be called afterwards.

        Args:
            state_tree: The StateTree asset to compile.
            save: When True, saves the asset after a successful compile.

        Returns:
            A JSON string: {"succeeded": bool, "saved": bool, "messages": [str]}.
        """
        #Python 반환 순서: (반환값, out 파라미터)
        messages, succeeded = _library().compile_state_tree(state_tree)
        saved = False

        if succeeded and save:
            saved = unreal.EditorAssetLibrary.save_loaded_asset(state_tree, only_if_is_dirty=False)

        return json.dumps({'succeeded': bool(succeeded), 'saved': bool(saved), 'messages': list(messages)})

    @toolset_registry.tool_call
    @staticmethod
    def needs_recompile(state_tree: unreal.StateTree) -> bool:
        """Returns whether a StateTree's editor data changed since its last compile.

        Args:
            state_tree: The StateTree asset to query.

        Returns:
            True if the asset must be compiled before its runtime data is up to date.
        """
        return bool(_library().state_tree_needs_recompile(state_tree))
