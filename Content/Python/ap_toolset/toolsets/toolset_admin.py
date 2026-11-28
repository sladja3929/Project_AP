"""프로젝트 툴세트 관리 툴세트.

ap_toolset Python 파일 수정 후 에디터 재시작 없이 다시 로드하기 위한 용도.
C++(ActionPracticeEditorScriptLibrary) 변경은 반영되지 않으므로 빌드 + 에디터 재시작 필요.
describe_tools_compact는 describe_toolset 전체 스키마 대신 시그니처 요약을 돌려주는 토큰 절약용 조회 툴.
"""

import inspect
import json
import re

import unreal

import toolset_registry


_SENTENCE_END = re.compile(r'(?<!e\.g)(?<!i\.e)\.(\s|$)')


def _first_sentence(text: str) -> str:
    #첫 문단을 한 줄로 이은 뒤 첫 문장만 (e.g. / i.e. 는 문장 끝으로 보지 않음)
    paragraph = ' '.join((text or '').strip().split('\n\n')[0].split())
    match = _SENTENCE_END.search(paragraph)

    if match:
        return paragraph[:match.start() + 1]

    return paragraph


def _type_label(prop: dict) -> str:
    #JSON 스키마 프로퍼티를 짧은 타입 표기로 변환
    if 'enum' in prop:
        values = [str(v) for v in prop['enum']]

        #긴 enum은 앞부분과 개수만
        if len(values) > 8:
            return '|'.join(values[:3]) + f'|...({len(values)} values)'

        return '|'.join(values)

    if prop.get('type') == 'object' and 'refPath' in (prop.get('properties') or {}):
        #에셋·클래스 참조는 {"refPath": "..."}로 넘김
        return 'ref<' + (prop.get('title') or 'Object').split('.')[-1] + '>'

    if 'anyOf' in prop or 'oneOf' in prop:
        return '|'.join(_type_label(p) for p in prop.get('anyOf', prop.get('oneOf', [])))

    kind = prop.get('type', '')

    if kind == 'array':
        return 'array<' + _type_label(prop.get('items', {})) + '>'

    if prop.get('title'):
        #/Script/Engine.AnimSequence → AnimSequence
        return prop['title'].split('.')[-1]

    if '$ref' in prop:
        return prop['$ref'].split('/')[-1]

    return kind or 'any'


def _signature(tool_schema: dict) -> str:
    #tool(arg*: type, arg: type=default) — 첫 문장
    input_schema = tool_schema.get('inputSchema', {}) or {}
    required = set(input_schema.get('required', []) or [])
    params = []

    for name, prop in (input_schema.get('properties', {}) or {}).items():
        label = name + ('*' if name in required else '') + ': ' + _type_label(prop)

        if 'default' in prop:
            label += '=' + json.dumps(prop['default'], ensure_ascii=False)

        params.append(label)

    short_name = tool_schema.get('name', '').split('.')[-1]
    return f"{short_name}({', '.join(params)}) — {_first_sentence(tool_schema.get('description', ''))}"


def _tool_schemas(toolset_schema: dict) -> list:
    return toolset_schema['tools']


def _all_toolset_schemas() -> list:
    #FToolsetRegistry::GetToolsetJsonSchemas 형식: [{"name": str, "description": str, "tools": [{tool schema}]}]
    raw = json.loads(unreal.ToolsetRegistry.get_all_toolset_json_schemas())
    valid = isinstance(raw, list) and all(
        isinstance(s, dict) and isinstance(s.get('name'), str) and isinstance(s.get('tools'), list) for s in raw)

    if not valid:
        raise RuntimeError('Unexpected toolset schema format from ToolsetRegistry.get_all_toolset_json_schemas '
                           '(expected a list of {"name", "tools": [...]}). The engine format may have changed.')

    return raw


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
            True when the reload finished. False if the toolset registry is unavailable.
        """
        import ap_toolset  # pylint: disable=import-outside-toplevel

        toolset_registry.reload_module(ap_toolset)

        #reload_module은 기존 등록 툴세트만 재등록하므로 등록 목록에 새로 추가된 툴세트까지 등록
        from ap_toolset import toolsets  # pylint: disable=import-outside-toplevel

        return toolsets.registration.register()

    @toolset_registry.tool_call
    @staticmethod
    def describe_tools_compact(toolset_name: str, tool_names: str) -> str:
        """Returns a compact summary of registered toolsets or tools, as a token-saving alternative to describe_toolset.

        Three modes:
        - toolset_name empty: one line per registered toolset (name and tool count).
        - toolset_name set, tool_names empty: one signature line per tool,
          "tool(arg*: type, arg: type=default) — first sentence of the description".
          '*' marks a required argument (pass it even if it has a default).
          ref<T> is an asset/class reference passed as {"refPath": "/Game/..."}.
        - toolset_name and tool_names set: the full schema (description and inputSchema)
          of only the named tools, as minified JSON.

        Works for every registered toolset, including engine toolsets.

        Args:
            toolset_name: Full toolset name (e.g. ap_toolset.toolsets.anim_montage.AnimMontageTools), or '' to list toolsets.
            tool_names: Comma-separated tool names without the toolset prefix (e.g. "list_notifies,add_notify"), or '' for signatures of all tools.

        Returns:
            Plain text (list modes) or minified JSON (detail mode).

        Raises:
            ValueError: The toolset or a tool name is not registered (the message lists candidates).
        """
        schemas = _all_toolset_schemas()

        if not toolset_name:
            lines = [f"{s.get('name', '')} ({len(_tool_schemas(s))})" for s in schemas]
            return '\n'.join(sorted(lines))

        toolset = next((s for s in schemas if s.get('name') == toolset_name), None)

        if toolset is None:
            #이름 일부만 맞는 후보 안내
            candidates = [s.get('name', '') for s in schemas if toolset_name.split('.')[-1].lower() in s.get('name', '').lower()]
            raise ValueError(f"Toolset '{toolset_name}' is not registered. Candidates: {candidates}")

        tools = _tool_schemas(toolset)

        if not tool_names.strip():
            header = f"{toolset_name} — {_first_sentence(toolset.get('description', ''))}"
            return '\n'.join([header] + [_signature(t) for t in tools])

        wanted = [n.strip() for n in tool_names.split(',') if n.strip()]
        by_short = {t.get('name', '').split('.')[-1]: t for t in tools}
        missing = [n for n in wanted if n not in by_short]

        if missing:
            raise ValueError(f"Tools {missing} not found in {toolset_name}. Available: {sorted(by_short)}")

        detail = [{'name': n,
                   'description': inspect.cleandoc(by_short[n].get('description', '')),
                   'inputSchema': by_short[n].get('inputSchema', {})} for n in wanted]
        return json.dumps(detail, ensure_ascii=False, separators=(',', ':'))
