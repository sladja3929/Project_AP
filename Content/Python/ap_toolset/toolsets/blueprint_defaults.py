"""블루프린트 기본값 편집 툴세트.

ObjectTools.set_properties로 BP CDO(또는 컴포넌트 템플릿)를 직접 바꾸면 이미 로드된 레벨 배치
인스턴스에 값이 전파되지 않음. 이 상태로 레벨을 저장하면 이전 값이 인스턴스 오버라이드로 굳어
BP를 고쳐도 맵의 액터는 계속 이전 값을 사용함.
에디터 디테일 패널과 같은 규칙(인스턴스 값이 이전 기본값과 같으면 새 값으로 갱신, 다르면 의도된
오버라이드로 보고 유지)으로 기본값 수정과 인스턴스 전파를 한 번에 처리.

ObjectTools.reset_properties는 컴포넌트를 BP 값이 아닌 네이티브 클래스 기본값으로 되돌리므로
BP 인스턴스 동기화에는 sync_blueprint_instances를 사용.
"""

import json

import unreal

import toolset_registry


_GEN_VARIABLE_SUFFIX = '_GEN_VARIABLE'


def _editor_world() -> unreal.World:
    if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
        raise RuntimeError('Stop the Play-In-Editor session before editing Blueprint defaults')

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

    if world is None:
        raise RuntimeError('No editor world is loaded')

    return world


def _instance_component_name(template: unreal.Object) -> str:
    #SCS·상속 컴포넌트 템플릿은 "<변수명>_GEN_VARIABLE", 네이티브 컴포넌트는 서브오브젝트 이름 그대로
    name = template.get_name()

    if name.endswith(_GEN_VARIABLE_SUFFIX):
        name = name[:-len(_GEN_VARIABLE_SUFFIX)]

    return name


def _find_template(blueprint: unreal.Blueprint, component_name: str) -> unreal.Object:
    generated_class = blueprint.generated_class()

    if generated_class is None:
        raise RuntimeError(f'{blueprint.get_name()} has no generated class. Compile the Blueprint first')

    cdo = unreal.get_default_object(generated_class)

    if not component_name:
        return cdo

    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    library = unreal.SubobjectDataBlueprintFunctionLibrary
    candidates = []

    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        data = library.get_data(handle)
        template = library.get_object_for_blueprint(data, blueprint)

        if template is None or not isinstance(template, unreal.ActorComponent):
            continue

        variable_name = str(library.get_variable_name(data))
        names = {variable_name, template.get_name(), _instance_component_name(template)}
        candidates.append(variable_name)

        if component_name in names:
            return template

    #컴포넌트가 아닌 CDO 기본 서브오브젝트 (예: AIController의 SightConfig 같은 AISenseConfig)
    subobject = unreal.find_object(None, f'{cdo.get_path_name()}:{component_name}')

    if subobject is not None:
        return subobject

    raise RuntimeError(f'Component or default subobject "{component_name}" not found on {blueprint.get_name()}. Components: {candidates}')


def _is_default_subobject(template: unreal.Object) -> bool:
    #CDO 자체도, 컴포넌트 템플릿도 아닌 CDO 하위 서브오브젝트 (outer가 자기 클래스의 CDO)
    #레벨에 배치되지 않고 런타임에 스폰되는 클래스(AIController 등)는 전파 대상 인스턴스가 없어 CDO 저장만 유효
    if isinstance(template, unreal.ActorComponent):
        return False

    outer = template.get_outer()

    return outer is not None and outer == unreal.get_default_object(outer.get_class())


def _is_child_of(cls: unreal.Class, base: unreal.Class) -> bool:
    #generated_class()는 파이썬 타입이 아닌 UClass라 isinstance 대신 UKismetMathLibrary::ClassIsChildOf 사용
    return unreal.MathLibrary.class_is_child_of(cls, base)


def _find_instance_target(actor: unreal.Actor, template: unreal.Object, is_default_subobject: bool = None):
    #템플릿에 대응하는 인스턴스 쪽 오브젝트. 없으면 None. is_default_subobject는 반복 호출 시 미리 계산한 값 전달용
    if is_default_subobject is None:
        is_default_subobject = _is_default_subobject(template)

    if is_default_subobject:
        #인스턴스의 같은 이름 서브오브젝트 (CDO 서브오브젝트를 아키타입으로 생성됨)
        return unreal.find_object(None, f'{actor.get_path_name()}:{template.get_name()}')

    if not isinstance(template, unreal.ActorComponent):
        return actor

    component_name = _instance_component_name(template)

    for component in actor.get_components_by_class(unreal.ActorComponent):
        if component.get_name() == component_name:
            return component

    return None


def _placed_instances(blueprint: unreal.Blueprint, template: unreal.Object) -> tuple:
    #반환: (액터, 대상 오브젝트) 목록, 같은 이름 컴포넌트를 못 찾은 액터 경로 목록
    _editor_world()
    generated_class = blueprint.generated_class()
    result = []
    skipped = []

    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        if not _is_child_of(actor.get_class(), generated_class):
            continue

        match = _find_instance_target(actor, template)

        if match is None:
            skipped.append(actor.get_path_name())
        else:
            result.append((actor, match))

    return result, skipped


def _read(obj: unreal.Object, properties: list) -> dict:
    return json.loads(unreal.ToolsetLibrary.get_object_properties(obj, properties))


def _same(a, b) -> bool:
    #JSON 왕복 시 float 표현 오차가 생길 수 있어 수치는 허용오차 비교, 구조체·배열은 재귀 비교
    if isinstance(a, bool) or isinstance(b, bool):
        return a == b

    if isinstance(a, (int, float)) and isinstance(b, (int, float)):
        return abs(a - b) <= 1e-4 * max(1.0, abs(a), abs(b))

    if isinstance(a, dict) and isinstance(b, dict):
        return a.keys() == b.keys() and all(_same(a[key], b[key]) for key in a)

    if isinstance(a, list) and isinstance(b, list):
        return len(a) == len(b) and all(_same(x, y) for x, y in zip(a, b))

    return a == b


def _property_name_candidates(key: str) -> list:
    #툴 JSON 키(camelCase 예: relativeLocation, bXxx, snake_case)에서 get/set_editor_property에 넘길 이름 후보
    if not key:
        return []

    candidates = [key]

    if not (len(key) > 1 and key[0] == 'b' and key[1].isupper()):
        candidates.insert(0, key[0].upper() + key[1:])

    return candidates


def _template_owned_object(value, template: unreal.Object):
    #값(또는 배열 원소)이 템플릿과 같은 BP 패키지 소유 오브젝트(Instanced 서브오브젝트 등)면 그 오브젝트 반환
    #그대로 대입하면 레벨 인스턴스가 BP 패키지 오브젝트를 참조하게 되어 저장 시 외부 참조 문제 발생
    items = value if isinstance(value, (list, tuple, unreal.Array)) else [value]
    package = template.get_outermost()

    for item in items:
        if isinstance(item, unreal.Object) and item.get_outermost() == package:
            return item

    return None


def _apply_to_instance(actor: unreal.Actor, template: unreal.Object, values: dict) -> tuple:
    #템플릿 값을 프로퍼티 단위로 통째 복사한 뒤 다시 읽어 검증
    #반환: (적용되지 않은 프로퍼티 이름 목록, 프로퍼티별 실패 원인)
    #SCS 컴포넌트는 값 변경마다 컨스트럭션 스크립트가 재실행되어 새 컴포넌트로 교체되므로
    #JSON 리프 단위 쓰기 대신 set_editor_property로 한 번에 쓰고, 매 쓰기 전후 대상을 이름으로 다시 찾음
    is_default_subobject = _is_default_subobject(template)
    not_applied = []
    errors = {}

    for key in values:
        target = _find_instance_target(actor, template, is_default_subobject)

        if target is None:
            not_applied.append(key)
            errors[key] = 'instance object not found'
            continue

        candidates = _property_name_candidates(key)
        last_error = 'empty property name'
        applied = False

        for name in candidates:
            try:
                value = template.get_editor_property(name)
            except Exception as error:  # pylint: disable=broad-except
                last_error = str(error)
                continue

            owned = _template_owned_object(value, template)

            if owned is not None:
                last_error = f'value references Blueprint-owned object {owned.get_path_name()}; set it on the instance manually'
                break

            try:
                target.modify()
                target.set_editor_property(name, value)
                applied = True
            except Exception as error:  # pylint: disable=broad-except
                last_error = str(error)

            break

        if not applied:
            not_applied.append(key)
            errors[key] = last_error

    target = _find_instance_target(actor, template, is_default_subobject)
    remaining = [key for key in values if key not in not_applied]
    after = _read(target, remaining) if target is not None and remaining else {}

    for key in remaining:
        if not _same(after.get(key), values[key]):
            not_applied.append(key)
            errors[key] = f'read back {after.get(key)!r} after write'

    return not_applied, errors


def _loaded_world_packages() -> set:
    #스트리밍 서브레벨까지 포함한 로드된 월드 패키지 (액터의 outer 체인 최상위가 월드 패키지)
    packages = {_editor_world().get_outermost().get_name()}

    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        packages.add(actor.get_outermost().get_name())

    return packages


def _actor_package(actor: unreal.Actor):
    #OFPA(외부 액터) 맵은 액터별 패키지에 저장되므로 월드 패키지가 아닌 액터 패키지를 저장
    get_package = getattr(actor, 'get_package', None)

    if get_package is not None:
        return get_package()

    return actor.get_outermost()


def _save(blueprint: unreal.Blueprint, actors: list) -> dict:
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    saved_blueprint = unreal.EditorAssetLibrary.save_loaded_asset(blueprint, False)

    packages = []

    for actor in actors:
        package = _actor_package(actor)

        if package is not None and package not in packages:
            packages.append(package)

    saved_levels = unreal.EditorLoadingAndSavingUtils.save_packages(packages, True) if packages else True

    return {
        'blueprint_saved': bool(saved_blueprint),
        'instance_packages_saved': bool(saved_levels),
        'instance_packages': [package.get_name() for package in packages],
    }


def _unloaded_placements(blueprint: unreal.Blueprint) -> list:
    #로드되지 않은 맵의 배치 인스턴스는 전파 대상이 아니므로 참조 패키지로만 보고
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=False,
                                                    include_hard_package_references=True,
                                                    include_searchable_names=False,
                                                    include_soft_management_references=False,
                                                    include_hard_management_references=False)
    referencers = registry.get_referencers(blueprint.get_outermost().get_name(), options) or []
    loaded_worlds = _loaded_world_packages()

    #외부 액터 패키지 경로: /Game/__ExternalActors__/<맵 경로>/<해시>/<해시>/<액터>
    loaded_external_prefixes = tuple(world.replace('/Game/', '/Game/__ExternalActors__/', 1) + '/'
                                     for world in loaded_worlds)
    result = []

    for referencer in referencers:
        name = str(referencer)

        if '/__ExternalActors__/' in name:
            if not name.startswith(loaded_external_prefixes):
                result.append(name)

            continue

        is_world = any(str(asset.asset_class_path.asset_name) == 'World'
                       for asset in registry.get_assets_by_package_name(name))

        if is_world and name not in loaded_worlds:
            result.append(name)

    return result


@unreal.uclass()
class BlueprintDefaultsTools(unreal.ToolsetDefinition):
    """Edits Blueprint default values (CDO or component templates) and keeps actors placed in
    the loaded level in sync, like the editor Details panel does.

    Use this instead of ObjectTools.set_properties / reset_properties whenever a Blueprint class
    default or a Blueprint component default is changed.
    """

    @toolset_registry.tool_call
    @staticmethod
    def set_blueprint_defaults(blueprint: unreal.Blueprint, component_name: str, values: str,
                               save: bool = True) -> str:
        """Sets Blueprint default values and propagates them to placed instances in the loaded level.

        Each placed instance whose current value equals the previous default receives the new
        value. Instances with a different value are treated as intentional per-instance
        overrides, left unchanged and reported in "kept_overrides" (use sync_blueprint_instances
        with force=True to overwrite them). Values are compared per top-level property.
        Instance values are copied from the updated template one property at a time with
        set_editor_property; each write may re-run the actor construction script and replace
        Blueprint-created components, so the component is looked up by name again before each write.
        Each update is read back and verified; mismatches are reported in "failed_instances" with
        a per-property error message.
        If this call fails midway the defaults may already hold the new value, so a retry no longer
        sees the old value; recover with check_blueprint_instances + sync_blueprint_instances(force=True).

        Args:
            blueprint: The Blueprint asset.
            component_name: Component variable name or template object name (e.g. "CharacterMovement"
                or "CharMoveComp" for inherited native components), or the name of a non-component
                default subobject of the CDO (e.g. "SightConfig" on an AIController). Empty string
                edits the class defaults (CDO) itself.
            values: JSON object of top-level properties to set, same format as ObjectTools.set_properties.
            save: When True, compiles and saves the Blueprint and saves the packages of updated actors.

        Returns:
            JSON with the template path, previous and new values, updated instances, kept overrides,
            save results and map packages that reference the Blueprint but are not loaded (not synced).
        """
        new_values = json.loads(values)
        properties = list(new_values.keys())
        template = _find_template(blueprint, component_name)
        previous = _read(template, properties)

        template.modify()

        if not unreal.ToolsetLibrary.set_object_properties(template, values):
            raise RuntimeError(f'Failed to set {properties} on {template.get_path_name()}')

        current = _read(template, properties)
        updated = []
        kept = []
        failed = []
        updated_actors = []
        instances, skipped = _placed_instances(blueprint, template)

        for actor, target in instances:
            instance_values = _read(target, properties)
            to_apply = {key: current[key] for key in properties if _same(instance_values.get(key), previous.get(key))}
            diverged = {key: instance_values.get(key) for key in properties if key not in to_apply}

            if to_apply:
                not_applied, errors = _apply_to_instance(actor, template, to_apply)
                applied = [key for key in to_apply if key not in not_applied]

                if applied:
                    updated.append({'actor': actor.get_path_name(), 'properties': applied})
                    updated_actors.append(actor)

                if not_applied:
                    failed.append({'actor': actor.get_path_name(), 'properties': not_applied, 'errors': errors})

            if diverged:
                kept.append({'actor': actor.get_path_name(), 'values': diverged})

        result = {
            'template': template.get_path_name(),
            'previous': previous,
            'current': current,
            'updated_instances': updated,
            'kept_overrides': kept,
            'failed_instances': failed,
            'skipped_actors_without_component': skipped,
            'unloaded_map_references': _unloaded_placements(blueprint),
        }

        if save:
            result['save'] = _save(blueprint, updated_actors)

        return json.dumps(result)

    @toolset_registry.tool_call
    @staticmethod
    def check_blueprint_instances(blueprint: unreal.Blueprint, component_name: str, properties: list[str]) -> str:
        """Compares placed instances in the loaded level against the Blueprint defaults.

        Use before and after editing a Blueprint, or when a placed actor behaves differently from
        its Blueprint, to find instances that hold stale or overridden values.

        Args:
            blueprint: The Blueprint asset.
            component_name: Component variable name or template object name. Empty string checks the CDO.
            properties: Top-level property names to compare.

        Returns:
            JSON with the default values, instances whose values differ (with their values), the
            number of matching instances and map packages that reference the Blueprint but are not loaded.
        """
        template = _find_template(blueprint, component_name)
        defaults = _read(template, properties)
        differing = []
        matching = 0
        instances, skipped = _placed_instances(blueprint, template)

        for actor, target in instances:
            instance_values = _read(target, properties)
            diff = {key: instance_values.get(key) for key in properties
                    if not _same(instance_values.get(key), defaults.get(key))}

            if diff:
                differing.append({'actor': actor.get_path_name(), 'values': diff})
            else:
                matching += 1

        return json.dumps({
            'template': template.get_path_name(),
            'defaults': defaults,
            'differing_instances': differing,
            'matching_instances': matching,
            'skipped_actors_without_component': skipped,
            'unloaded_map_references': _unloaded_placements(blueprint),
        })

    @toolset_registry.tool_call
    @staticmethod
    def sync_blueprint_instances(blueprint: unreal.Blueprint, component_name: str, properties: list[str],
                                 force: bool = False, save: bool = True) -> str:
        """Copies the current Blueprint default values to placed instances in the loaded level.

        Overwrites every instance value of the given properties that differs from the default,
        discarding per-instance overrides (e.g. stale values left by an earlier CDO edit made
        without propagation). Inspect them first with check_blueprint_instances; force must be True.
        Values are copied one property at a time with set_editor_property (see set_blueprint_defaults);
        failures are reported in "failed_instances" with a per-property error message.

        Args:
            blueprint: The Blueprint asset.
            component_name: Component variable name or template object name. Empty string uses the CDO.
            properties: Top-level property names to copy.
            force: Must be True to overwrite instance values that differ from the defaults.
            save: When True, saves the packages of updated actors.

        Returns:
            JSON with updated instances and save results.
        """
        if not force:
            raise RuntimeError('Pass force=True to overwrite instance values. Inspect them first with check_blueprint_instances')

        template = _find_template(blueprint, component_name)
        defaults = _read(template, properties)
        updated = []
        failed = []
        updated_actors = []
        instances, skipped = _placed_instances(blueprint, template)

        for actor, target in instances:
            instance_values = _read(target, properties)
            to_apply = {key: defaults[key] for key in properties if not _same(instance_values.get(key), defaults.get(key))}

            if not to_apply:
                continue

            not_applied, errors = _apply_to_instance(actor, template, to_apply)
            applied = [key for key in to_apply if key not in not_applied]

            if applied:
                updated.append({'actor': actor.get_path_name(), 'properties': applied})
                updated_actors.append(actor)

            if not_applied:
                failed.append({'actor': actor.get_path_name(), 'properties': not_applied, 'errors': errors})

        result = {
            'template': template.get_path_name(),
            'defaults': defaults,
            'updated_instances': updated,
            'failed_instances': failed,
            'skipped_actors_without_component': skipped,
        }

        if save:
            result['save'] = _save(blueprint, updated_actors)

        return json.dumps(result)

    @toolset_registry.tool_call
    @staticmethod
    def describe_instance_components(actor: unreal.Actor) -> str:
        """Lists the scene components of a placed actor with their runtime attachment and transforms.

        Use when a placed Blueprint instance renders a component in the wrong place, e.g. after
        reparenting components in the Blueprint, to see which parent each component is actually
        attached to (ActorTools.get_parent_component reads the Blueprint hierarchy, not the instance).

        Pass an actor of the editor world; actors of a running Play-In-Editor world are a separate copy.

        Args:
            actor: The placed actor.

        Returns:
            JSON list of components with name, class, attach parent and socket, relative location,
            rotation and scale, and world location.
        """
        components = []

        for component in actor.get_components_by_class(unreal.SceneComponent):
            parent = component.get_attach_parent()
            relative_location = component.get_editor_property('relative_location')
            relative_rotation = component.get_editor_property('relative_rotation')
            relative_scale = component.get_editor_property('relative_scale3d')
            world_location = component.get_world_location()
            components.append({
                'name': component.get_name(),
                'class': component.get_class().get_name(),
                'attach_parent': parent.get_name() if parent is not None else None,
                'attach_socket': str(component.get_attach_socket_name()),
                'relative_location': [round(relative_location.x, 2), round(relative_location.y, 2), round(relative_location.z, 2)],
                'relative_rotation': [round(relative_rotation.pitch, 2), round(relative_rotation.yaw, 2), round(relative_rotation.roll, 2)],
                'relative_scale': [round(relative_scale.x, 3), round(relative_scale.y, 3), round(relative_scale.z, 3)],
                'world_location': [round(world_location.x, 2), round(world_location.y, 2), round(world_location.z, 2)],
            })

        return json.dumps({'actor': actor.get_path_name(), 'components': components})
