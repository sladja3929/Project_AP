"""ActionPractice 프로젝트 에디터 Python 진입점.

에디터 시작 시 PythonScriptPlugin이 자동 실행. 프로젝트 전용 MCP 툴세트를
ToolsetRegistry에 등록하여 unreal-mcp에서 사용할 수 있게 함.
"""

import unreal

try:
    from ap_toolset import toolsets

    if not toolsets.registration.register():
        unreal.log_warning('ap_toolset: ToolsetRegistry not available, project toolsets were not registered')
except Exception as error:  # pylint: disable=broad-exception-caught
    #툴세트 로드 실패가 에디터 시작을 막지 않도록 로그만 남김
    unreal.log_error(f'ap_toolset: failed to register project toolsets: {error}')
