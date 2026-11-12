"""프로젝트 툴세트 관리 툴세트.

ap_toolset Python 파일 수정 후 에디터 재시작 없이 다시 로드하기 위한 용도.
C++(ActionPracticeEditorScriptLibrary) 변경은 반영되지 않으므로 빌드 + 에디터 재시작 필요.
"""

import unreal

import toolset_registry


@unreal.uclass()
class ProjectToolsetAdminTools(unreal.ToolsetDefinition):
    """Provides tools for maintaining the ActionPractice project toolsets (ap_toolset)."""

    @toolset_registry.tool_call
    @staticmethod
    def reload_project_toolsets() -> bool:
        """Reloads all ap_toolset Python modules and re-registers their toolsets.

        Use after editing files under Content/Python/ap_toolset. Changes to the
        ActionPracticeEditor C++ module still require a build and editor restart.

        Returns:
            True when the reload finished.
        """
        import ap_toolset  # pylint: disable=import-outside-toplevel

        toolset_registry.reload_module(ap_toolset)
        return True
