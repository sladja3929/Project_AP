"""AnimMontage 생성 및 AnimNotify / AnimNotifyState 편집 툴세트.

엔진 5.8 내장 툴세트에는 몽타주 생성·노티파이 편집 툴이 없어 프로젝트에서 보충.
노티파이 프로퍼티(예: AnimNotifyState_HitDetection의 설정값)는 add_* 결과로 받은
오브젝트 레퍼런스를 ObjectTools.set_properties에 넘겨 편집.
"""

import json
import math

import unreal

import toolset_registry
from toolset_registry.helpers import create_asset


def _anim_lib():
    return unreal.AnimationLibrary


def _ensure_track(animation: unreal.AnimSequenceBase, track_name: str) -> None:
    track_names = [str(name) for name in _anim_lib().get_animation_notify_track_names(animation)]

    if track_name not in track_names:
        _anim_lib().add_animation_notify_track(animation, track_name)


def _mark_dirty(animation: unreal.AnimSequenceBase) -> None:
    animation.modify()


def _editor_lib():
    library = getattr(unreal, 'ActionPracticeEditorScriptLibrary', None)

    if library is None:
        raise RuntimeError(
            'ActionPracticeEditorScriptLibrary not found. Build the ActionPracticeEditor module and restart the editor.')

    return library


def _set_slot_name(montage: unreal.AnimMontage, slot_name: str, slot_track_index: int) -> bool:
    #SlotAnimTracks는 BlueprintReadOnly라 C++ 라이브러리 경유
    return bool(_editor_lib().set_montage_slot_name(montage, slot_track_index, slot_name))


def _get_sections(montage: unreal.AnimMontage) -> list:
    #CompositeSections는 스크립트에 노출되지 않아 C++ 라이브러리 경유
    names, start_times, next_names = _editor_lib().get_montage_sections(montage)
    return [{'name': str(name), 'start_time': start, 'next': '' if str(next_name) == 'None' else str(next_name)}
            for name, start, next_name in zip(names, start_times, next_names)]


def _quat_delta(reference: unreal.Quat, rotation: unreal.Quat) -> unreal.Quat:
    #inverse(reference) * rotation (Hamilton 곱, 단위 쿼터니언이므로 역원 = 켤레)
    ax, ay, az, aw = -reference.x, -reference.y, -reference.z, reference.w
    bx, by, bz, bw = rotation.x, rotation.y, rotation.z, rotation.w
    return unreal.Quat(
        aw * bx + ax * bw + ay * bz - az * by,
        aw * by - ax * bz + ay * bw + az * bx,
        aw * bz + ax * by - ay * bx + az * bw,
        aw * bw - ax * bx - ay * by - az * bz)


def _quat_slerp(a: unreal.Quat, b: unreal.Quat, alpha: float) -> unreal.Quat:
    dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w
    sign = 1.0

    #최단 경로 보간
    if dot < 0.0:
        dot = -dot
        sign = -1.0

    if dot > 0.9995:
        weight_a = 1.0 - alpha
        weight_b = alpha * sign
    else:
        theta = math.acos(dot)
        weight_a = math.sin((1.0 - alpha) * theta) / math.sin(theta)
        weight_b = math.sin(alpha * theta) / math.sin(theta) * sign

    x = weight_a * a.x + weight_b * b.x
    y = weight_a * a.y + weight_b * b.y
    z = weight_a * a.z + weight_b * b.z
    w = weight_a * a.w + weight_b * b.w
    length = math.sqrt(x * x + y * y + z * z + w * w)
    return unreal.Quat(x / length, y / length, z / length, w / length)


def _window_weight(time: float, start: float, end: float, blend_in: float, blend_out: float) -> float:
    if time < start or time > end:
        return 0.0

    weight = 1.0

    if blend_in > 0.0 and time < start + blend_in:
        ratio = (time - start) / blend_in
        weight = min(weight, ratio * ratio * (3.0 - 2.0 * ratio))

    if blend_out > 0.0 and time > end - blend_out:
        ratio = (end - time) / blend_out
        weight = min(weight, ratio * ratio * (3.0 - 2.0 * ratio))

    return weight


def _bake_reversed(
        source_animation: unreal.AnimSequence, reversed_animation: unreal.AnimSequence,
        rebase_root: bool, strip_events: bool) -> dict:
    if source_animation.get_path_name() == reversed_animation.get_path_name():
        raise RuntimeError('Source and destination are the same asset')

    model = source_animation.get_editor_property('data_model_interface')
    num_keys = model.get_number_of_keys()
    length = _anim_lib().get_sequence_length(source_animation)
    bone_names = [str(name) for name in model.get_bone_track_names()]
    skeleton_root = str(source_animation.get_editor_property('skeleton').get_reference_pose().get_bone_names()[0])
    root_index = bone_names.index(skeleton_root) if skeleton_root in bone_names else -1

    if rebase_root and root_index < 0:
        raise RuntimeError(f'rebase_root requested but root bone {skeleton_root} has no animation track')

    #쓰기 전에 대상 트랙 구성 검증 (덮어쓰기 시 부분 갱신 방지)
    target_model = reversed_animation.get_editor_property('data_model_interface')
    target_bone_names = [str(name) for name in target_model.get_bone_track_names()]

    if target_model.get_number_of_keys() != num_keys or sorted(target_bone_names) != sorted(bone_names):
        raise RuntimeError(
            f'{reversed_animation.get_name()} does not match the source (keys '
            f'{target_model.get_number_of_keys()} vs {num_keys}, bone tracks differ: '
            f'{sorted(target_bone_names) != sorted(bone_names)})')

    #프레임별 로컬 본 트랜스폼 샘플링 (루트 모션 추출 없이 원본 트랙 그대로)
    frames = [_anim_lib().get_bone_poses_for_frame(source_animation, bone_names, frame, False)
              for frame in range(num_keys)]
    root_offset = unreal.Vector(0.0, 0.0, 0.0)

    if rebase_root:
        root_offset = frames[0][root_index].translation - frames[-1][root_index].translation

    controller = reversed_animation.get_editor_property('controller')

    if not controller:
        raise RuntimeError(f'{reversed_animation.get_name()} has no animation data controller')

    controller.open_bracket(unreal.Text('Reverse Animation'), False)

    try:
        for bone_index, bone_name in enumerate(bone_names):
            transforms = [frames[num_keys - 1 - key][bone_index] for key in range(num_keys)]
            positions = [transform.translation for transform in transforms]

            if bone_index == root_index:
                positions = [position + root_offset for position in positions]

            controller.set_bone_track_keys(
                bone_name, positions,
                [transform.rotation for transform in transforms],
                [transform.scale3d for transform in transforms], False)
    finally:
        controller.close_bracket(False)

    curves_reversed = []

    for curve_name in _anim_lib().get_animation_curve_names(
            source_animation, unreal.RawCurveTrackTypes.RCT_FLOAT):
        times, values = _anim_lib().get_float_keys(source_animation, curve_name)

        if _anim_lib().does_curve_exist(reversed_animation, curve_name, unreal.RawCurveTrackTypes.RCT_FLOAT):
            _anim_lib().remove_curve(reversed_animation, curve_name, False)

        _anim_lib().add_curve(reversed_animation, curve_name, unreal.RawCurveTrackTypes.RCT_FLOAT, False)
        _anim_lib().add_float_curve_keys(
            reversed_animation, curve_name,
            [length - time for time in reversed(times)], list(reversed(values)))
        curves_reversed.append(str(curve_name))

    removed_sync_markers = 0
    removed_notifies = []

    if strip_events:
        removed_sync_markers, removed_notifies = _strip_events(reversed_animation, length)

    _mark_dirty(reversed_animation)

    return {
        'asset': reversed_animation.get_path_name(),
        'length': length,
        'num_keys': num_keys,
        'bone_tracks': len(bone_names),
        'root_bone': skeleton_root if root_index >= 0 else None,
        'curves_reversed': curves_reversed,
        'removed_sync_markers': removed_sync_markers,
        'removed_notifies': removed_notifies,
    }


def _strip_events(animation: unreal.AnimSequence, length: float) -> tuple:
    removed_sync_markers = len(_anim_lib().get_animation_sync_markers(animation))

    if removed_sync_markers:
        _anim_lib().remove_all_animation_sync_markers(animation)

    removed_notifies = []

    for track_name in _anim_lib().get_animation_notify_track_names(animation):
        for event in _anim_lib().get_animation_notify_events_for_track(animation, track_name):
            duration = _anim_lib().get_anim_notify_event_duration(event)
            removed_notifies.append({
                'name': str(event.get_editor_property('notify_name')),
                'track': str(track_name),
                'mirrored_time': round(length - _anim_lib().get_anim_notify_event_trigger_time(event) - duration, 4),
                'duration': duration,
            })

        _anim_lib().remove_animation_notify_events_by_track(animation, track_name)

    return removed_sync_markers, removed_notifies


@unreal.uclass()
class AnimMontageTools(unreal.ToolsetDefinition):
    """Provides tools for creating AnimMontage assets, editing AnimNotify /
    AnimNotifyState events on montages and animation sequences, and baking
    derived animation sequences (e.g. reversed playback)."""

    @toolset_registry.tool_call
    @staticmethod
    def create_montage(
            folder_path: str, asset_name: str, source_animation: unreal.AnimSequence,
            slot_name: str = '') -> unreal.AnimMontage:
        """Creates a new AnimMontage from an AnimSequence.

        The montage uses the sequence's skeleton and contains one slot track with
        the sequence as its only segment. The asset is not saved.

        Args:
            folder_path: The content-browser folder to create the montage in.
            asset_name: The name of the new montage (e.g. 'AM_Wolf_Attack').
            source_animation: The AnimSequence to build the montage from.
            slot_name: Optional slot name for the montage's slot track, without the
                group prefix (e.g. 'DefaultSlot', 'UpperBody'). Must be registered on
                the skeleton. Empty keeps the factory default.

        Returns:
            The created AnimMontage.
        """
        factory = unreal.AnimMontageFactory()
        factory.set_editor_property('target_skeleton', source_animation.get_editor_property('skeleton'))
        factory.set_editor_property('source_animation', source_animation)
        montage = create_asset(folder_path, asset_name, unreal.AnimMontage, factory)
        assert isinstance(montage, unreal.AnimMontage)

        if slot_name and not _set_slot_name(montage, slot_name, 0):
            raise RuntimeError(f'{asset_name} was created but slot {slot_name} is not registered on its skeleton')

        return montage

    @toolset_registry.tool_call
    @staticmethod
    def set_slot_name(montage: unreal.AnimMontage, slot_name: str, slot_track_index: int = 0) -> bool:
        """Sets the slot name of one of a montage's slot tracks.

        Args:
            montage: The montage to modify.
            slot_name: The slot name without the group prefix (e.g. 'DefaultSlot').
            slot_track_index: The slot track to modify.

        Returns:
            True if the slot name was set. False if the index is invalid or the slot
            is not registered on the montage's skeleton.
        """
        return _set_slot_name(montage, slot_name, slot_track_index)

    @toolset_registry.tool_call
    @staticmethod
    def get_montage_info(montage: unreal.AnimMontage) -> str:
        """Returns basic information about a montage.

        Args:
            montage: The montage to query.

        Returns:
            A JSON string with length, slots (segments with start_pos / anim start-end time),
            blend in/out times, and sections (name, start_time, next).
        """
        slots = []

        for slot_track in montage.get_editor_property('slot_anim_tracks'):
            anim_track = slot_track.get_editor_property('anim_track')
            segments = []

            for segment in anim_track.get_editor_property('anim_segments'):
                reference = segment.get_editor_property('anim_reference')
                segments.append({
                    'animation': reference.get_path_name() if reference else None,
                    'start_pos': segment.get_editor_property('start_pos'),
                    'anim_start_time': segment.get_editor_property('anim_start_time'),
                    'anim_end_time': segment.get_editor_property('anim_end_time'),
                })

            slots.append({'slot_name': str(slot_track.get_editor_property('slot_name')), 'segments': segments})

        blend_in = montage.get_editor_property('blend_in')
        blend_out = montage.get_editor_property('blend_out')
        return json.dumps({
            'length': _anim_lib().get_sequence_length(montage),
            'slots': slots,
            'blend_in_time': blend_in.get_editor_property('blend_time'),
            'blend_out_time': blend_out.get_editor_property('blend_time'),
            'blend_out_trigger_time': montage.get_editor_property('blend_out_trigger_time'),
            'enable_auto_blend_out': montage.get_editor_property('enable_auto_blend_out'),
            'sections': _get_sections(montage),
        })

    @toolset_registry.tool_call
    @staticmethod
    def set_montage_sections(montage: unreal.AnimMontage, sections: str) -> str:
        """Replaces all composite sections of a montage. The asset is not saved.

        Loops are built with next links, e.g. a section whose next is itself loops
        forever until code changes its next section.

        Args:
            montage: The montage to modify.
            sections: JSON array of {"name": str, "start_time": float, "next": str}.
                One section must start at 0. "next" is the section played after this
                one ends; empty string means the montage ends after this section.
                Example: [{"name": "Start", "start_time": 0, "next": "Loop"},
                {"name": "Loop", "start_time": 2.5, "next": "Loop"},
                {"name": "End", "start_time": 9.0, "next": ""}]

        Returns:
            A JSON string of the resulting sections sorted by start time.
        """
        entries = json.loads(sections)
        names = [entry['name'] for entry in entries]
        start_times = [float(entry['start_time']) for entry in entries]
        next_names = [entry.get('next') or 'None' for entry in entries]
        error = _editor_lib().set_montage_sections(montage, names, start_times, next_names)

        if error:
            raise RuntimeError(error)

        return json.dumps(_get_sections(montage))

    @toolset_registry.tool_call
    @staticmethod
    def list_notifies(animation: unreal.AnimSequenceBase) -> str:
        """Lists all notify and notify state events on a montage or sequence.

        Args:
            animation: The montage or animation sequence to query.

        Returns:
            A JSON string: {"length": float, "tracks": [str], "notifies": [{"name",
            "track", "trigger_time", "duration", "is_state", "class", "object"}]},
            sorted by trigger time. "object" is the notify instance path usable with ObjectTools.
        """
        track_names = [str(name) for name in _anim_lib().get_animation_notify_track_names(animation)]
        notifies = []

        #FAnimNotifyEvent::TrackIndex는 스크립트에 노출되지 않으므로 트랙 단위로 조회
        for track_name in track_names:
            for event in _anim_lib().get_animation_notify_events_for_track(animation, track_name):
                notify = event.get_editor_property('notify')
                notify_state = event.get_editor_property('notify_state_class')
                instance = notify_state if notify_state else notify
                notifies.append({
                    'name': str(event.get_editor_property('notify_name')),
                    'track': track_name,
                    'trigger_time': _anim_lib().get_anim_notify_event_trigger_time(event),
                    'duration': _anim_lib().get_anim_notify_event_duration(event),
                    'is_state': notify_state is not None,
                    'class': instance.get_class().get_path_name() if instance else None,
                    'object': instance.get_path_name() if instance else None,
                })

        notifies.sort(key=lambda entry: entry['trigger_time'])

        return json.dumps({
            'length': _anim_lib().get_sequence_length(animation),
            'tracks': track_names,
            'notifies': notifies,
        })

    @toolset_registry.tool_call
    @staticmethod
    def add_notify(
            animation: unreal.AnimSequenceBase, notify_class: unreal.Class,
            trigger_time: float, track_name: str = '1') -> unreal.AnimNotify:
        """Adds an AnimNotify event to a montage or sequence.

        The notify track is created if it does not exist. The asset is not saved.

        Args:
            animation: The montage or animation sequence to modify.
            notify_class: The AnimNotify subclass to instantiate.
            trigger_time: The time in seconds at which the notify fires.
            track_name: The notify track to place the notify on.

        Returns:
            The created notify instance (edit it with ObjectTools.set_properties).
        """
        _mark_dirty(animation)
        _ensure_track(animation, track_name)
        notify = _anim_lib().add_animation_notify_event(animation, track_name, trigger_time, notify_class)
        assert notify, f'Failed to add {notify_class.get_name()} to {animation.get_name()}'
        return notify

    @toolset_registry.tool_call
    @staticmethod
    def add_notify_state(
            animation: unreal.AnimSequenceBase, notify_state_class: unreal.Class,
            start_time: float, duration: float, track_name: str = '1') -> unreal.AnimNotifyState:
        """Adds an AnimNotifyState event to a montage or sequence.

        The notify track is created if it does not exist. The asset is not saved.

        Args:
            animation: The montage or animation sequence to modify.
            notify_state_class: The AnimNotifyState subclass to instantiate.
            start_time: The time in seconds at which the notify state begins.
            duration: The length of the notify state in seconds.
            track_name: The notify track to place the notify state on.

        Returns:
            The created notify state instance (edit it with ObjectTools.set_properties).
        """
        _mark_dirty(animation)
        _ensure_track(animation, track_name)
        notify_state = _anim_lib().add_animation_notify_state_event(
            animation, track_name, start_time, duration, notify_state_class)
        assert notify_state, f'Failed to add {notify_state_class.get_name()} to {animation.get_name()}'
        return notify_state

    @toolset_registry.tool_call
    @staticmethod
    def remove_notifies_by_name(animation: unreal.AnimSequenceBase, notify_name: str) -> int:
        """Removes all notify events with the given name (as shown by list_notifies).

        Args:
            animation: The montage or animation sequence to modify.
            notify_name: The notify name to remove.

        Returns:
            The number of removed notify events.
        """
        _mark_dirty(animation)
        return _anim_lib().remove_animation_notify_events_by_name(animation, notify_name)

    @toolset_registry.tool_call
    @staticmethod
    def remove_notifies_by_track(animation: unreal.AnimSequenceBase, track_name: str) -> int:
        """Removes all notify events on the given notify track. The track itself is kept.

        Args:
            animation: The montage or animation sequence to modify.
            track_name: The notify track to clear.

        Returns:
            The number of removed notify events.
        """
        _mark_dirty(animation)
        return _anim_lib().remove_animation_notify_events_by_track(animation, track_name)

    @toolset_registry.tool_call
    @staticmethod
    def get_root_motion_info(
            animation: unreal.AnimSequence, sample_count: int = 30, include_samples: bool = False) -> str:
        """Measures how the root bone moves in an animation sequence.

        Motion warping only modifies root motion, so a montage needs root motion enabled
        on its sequences for warp windows to have any effect. Use this to check whether
        enabling root motion would move the capsule (non-zero translation) or only lets
        the warp rotate it (in-place animation).

        Args:
            animation: The animation sequence to inspect.
            sample_count: Number of evenly spaced samples between start and end.
            include_samples: Also return per-sample root translation to see when the
                movement happens.

        Returns:
            A JSON string: {"enable_root_motion", "root_lock", "root_bone", "length",
            "end_translation" [x, y, z], "end_yaw", "max_horizontal_distance"}. Translation and
            yaw are relative to the first frame in the root bone's local space.
            With include_samples, "samples": [[time, x, y, z]] is added.
        """
        length = _anim_lib().get_sequence_length(animation)
        options = unreal.AnimPoseEvaluationOptions()
        extensions = unreal.AnimPoseExtensions
        root_bone = None
        start = None
        end = None
        max_distance = 0.0
        samples = []

        for index in range(max(2, sample_count)):
            time = length * index / (max(2, sample_count) - 1)
            pose = extensions.get_anim_pose_at_time(animation, time, options)

            if root_bone is None:
                root_bone = extensions.get_bone_names(pose)[0]

            transform = extensions.get_bone_pose(pose, root_bone, unreal.AnimPoseSpaces.LOCAL)

            if start is None:
                start = transform

            end = transform
            delta = transform.translation - start.translation
            max_distance = max(max_distance, (delta.x ** 2 + delta.y ** 2) ** 0.5)
            samples.append([round(time, 3), round(delta.x, 2), round(delta.y, 2), round(delta.z, 2)])

        end_delta = end.translation - start.translation
        result = {
            'enable_root_motion': animation.get_editor_property('enable_root_motion'),
            'root_lock': str(animation.get_editor_property('root_motion_root_lock')),
            'root_bone': str(root_bone),
            'length': length,
            'end_translation': [round(end_delta.x, 2), round(end_delta.y, 2), round(end_delta.z, 2)],
            'end_yaw': round(end.rotation.rotator().yaw - start.rotation.rotator().yaw, 2),
            'max_horizontal_distance': round(max_distance, 2),
        }

        if include_samples:
            result['samples'] = samples

        return json.dumps(result)

    @toolset_registry.tool_call
    @staticmethod
    def get_bone_rotation_info(
            animation: unreal.AnimSequence, bone_name: str, sample_count: int = 25) -> str:
        """Measures how far a bone rotates away from its reference pose over an animation.

        Useful to compare a bone between a source and a retargeted animation (e.g. how
        wide a jaw opens). Values are relative to each skeleton's own reference pose, so
        compare across skeletons only when their reference poses mean the same posture.

        Args:
            animation: The animation sequence to inspect.
            bone_name: The bone to measure.
            sample_count: Number of evenly spaced samples between start and end.

        Returns:
            A JSON string: {"bone", "max_angle", "samples": [[time, angle, roll, pitch, yaw]]}.
            Angle is in degrees between the animated and reference local rotation;
            roll/pitch/yaw are that delta rotation in the bone's local space.
        """
        length = _anim_lib().get_sequence_length(animation)
        options = unreal.AnimPoseEvaluationOptions()
        extensions = unreal.AnimPoseExtensions
        reference = extensions.get_ref_bone_pose(
            animation.get_editor_property('skeleton').get_reference_pose(), bone_name, unreal.AnimPoseSpaces.LOCAL)
        count = max(2, sample_count)
        samples = []

        for index in range(count):
            time = length * index / (count - 1)
            pose = extensions.get_anim_pose_at_time(animation, time, options)
            rotation = extensions.get_bone_pose(pose, bone_name, unreal.AnimPoseSpaces.LOCAL).rotation
            delta = _quat_delta(reference.rotation, rotation)
            angle = math.degrees(2.0 * math.acos(min(1.0, abs(delta.w))))
            rotator = delta.rotator()
            samples.append([round(time, 3), round(angle, 2),
                            round(rotator.roll, 2), round(rotator.pitch, 2), round(rotator.yaw, 2)])

        return json.dumps({
            'bone': bone_name,
            'max_angle': max(sample[1] for sample in samples),
            'samples': samples,
        })

    @toolset_registry.tool_call
    @staticmethod
    def get_bone_location_info(
            animation: unreal.AnimSequence, bone_name: str, sample_count: int = 25) -> str:
        """Samples a bone's component-space location over an animation (e.g. pelvis height
        to find crouch / takeoff / landing timing).

        Args:
            animation: The animation sequence to inspect.
            bone_name: The bone to sample.
            sample_count: Number of evenly spaced samples between start and end.

        Returns:
            A JSON string: {"bone", "samples": [[time, x, y, z]]}.
        """
        length = _anim_lib().get_sequence_length(animation)
        options = unreal.AnimPoseEvaluationOptions()
        extensions = unreal.AnimPoseExtensions
        count = max(2, sample_count)
        samples = []

        for index in range(count):
            time = length * index / (count - 1)
            pose = extensions.get_anim_pose_at_time(animation, time, options)
            location = extensions.get_bone_pose(pose, bone_name, unreal.AnimPoseSpaces.WORLD).translation
            samples.append([round(time, 3), round(location.x, 2), round(location.y, 2), round(location.z, 2)])

        return json.dumps({'bone': bone_name, 'samples': samples})

    @toolset_registry.tool_call
    @staticmethod
    def blend_bones_to_pose(
            animation: unreal.AnimSequence, bones: list[str], translation_bones: list[str],
            reference_time: float, alpha: float, start_time: float, end_time: float,
            blend_in: float = 0.1, blend_out: float = 0.1) -> str:
        """Partially blends the local rotations of selected bones toward their pose at
        reference_time, only inside a time window. Edits keys in place; the asset is not saved.

        Use to tone down part of a motion (e.g. less crouch on landing by blending toward
        the standing pose after landing). Blending only child bones while their parent
        keeps its pose can push contacts through the floor; include the parent chain
        (e.g. pelvis in translation_bones) so the whole body moves consistently.
        Rotations of bones change; translations change only for translation_bones; scales
        are kept. Not transacted (no undo), so duplicate the sequence as a backup first.

        Args:
            animation: The animation sequence to modify (must not be additive).
            bones: Bone names to modify. Each must have an animation track.
            translation_bones: Bones (also listed in bones) whose local translation is
                blended too, e.g. the pelvis. Pass [] to blend rotations only.
            reference_time: Time in seconds whose pose is the blend target.
            alpha: Maximum blend weight toward the reference pose (0..1).
            start_time: Window start in seconds.
            end_time: Window end in seconds.
            blend_in: Seconds to ease the weight in from 0 at the window start.
            blend_out: Seconds to ease the weight out to 0 at the window end.

        Returns:
            A JSON string: {"asset", "modified_bones", "modified_keys", "max_weight"}.
        """
        if animation.get_editor_property('additive_anim_type') != unreal.AdditiveAnimationType.AAT_NONE:
            raise RuntimeError(f'{animation.get_name()} is additive; blend_bones_to_pose does not support it')

        if not 0.0 <= alpha <= 1.0:
            raise ValueError(f'alpha must be within 0..1, got {alpha}')

        if end_time <= start_time or blend_in < 0.0 or blend_out < 0.0:
            raise ValueError(f'Invalid window {start_time}-{end_time}s (blend_in {blend_in}, blend_out {blend_out})')

        length = _anim_lib().get_sequence_length(animation)

        if start_time >= length or end_time <= 0.0:
            raise ValueError(f'Window {start_time}-{end_time}s is outside the sequence (length {length:.3f}s)')

        model = animation.get_editor_property('data_model_interface')
        num_keys = model.get_number_of_keys()
        frame_rate = model.get_frame_rate()
        fps = frame_rate.numerator / frame_rate.denominator
        #FName은 대소문자 무시 비교이고 표기는 이름 테이블 첫 등록 기준이라 소문자로 매칭
        track_names = {str(name).lower(): str(name) for name in model.get_bone_track_names()}
        missing = [bone for bone in bones if bone.lower() not in track_names]

        if missing:
            raise ValueError(f'Bones without animation tracks: {missing}')

        bones = [track_names[bone.lower()] for bone in bones]
        blend_translation = {bone.lower() for bone in translation_bones}
        not_listed = blend_translation - {bone.lower() for bone in bones}

        if not_listed:
            raise ValueError(f'translation_bones must also be in bones: {sorted(not_listed)}')

        reference_frame = min(num_keys - 1, max(0, int(round(reference_time * fps))))
        reference = _anim_lib().get_bone_poses_for_frame(animation, bones, reference_frame, False)
        frames = [_anim_lib().get_bone_poses_for_frame(animation, bones, frame, False) for frame in range(num_keys)]
        weights = [alpha * _window_weight(frame / fps, start_time, end_time, blend_in, blend_out)
                   for frame in range(num_keys)]
        #쓰기 중 실패로 일부 본만 바뀌는 범위를 줄이도록 모든 키를 먼저 계산
        tracks = []

        for bone_index, bone_name in enumerate(bones):
            transforms = [frame[bone_index] for frame in frames]
            rotations = [_quat_slerp(transform.rotation, reference[bone_index].rotation, weights[key])
                         if weights[key] > 0.0 else transform.rotation
                         for key, transform in enumerate(transforms)]
            positions = [transform.translation for transform in transforms]

            if bone_name.lower() in blend_translation:
                target = reference[bone_index].translation
                positions = [position + (target - position) * weights[key]
                             for key, position in enumerate(positions)]

            tracks.append((bone_name, positions, rotations, [transform.scale3d for transform in transforms]))

        controller = animation.get_editor_property('controller')

        if not controller:
            raise RuntimeError(f'{animation.get_name()} has no animation data controller')

        controller.open_bracket(unreal.Text('Blend Bones To Pose'), False)

        try:
            for bone_name, positions, rotations, scales in tracks:
                controller.set_bone_track_keys(bone_name, positions, rotations, scales, False)
        finally:
            controller.close_bracket(False)

        _mark_dirty(animation)
        modified_frames = [key for key, weight in enumerate(weights) if weight > 0.0]

        return json.dumps({
            'asset': animation.get_path_name(),
            'modified_bones': len(bones),
            'modified_keys': len(modified_frames),
            'modified_frame_range': [modified_frames[0], modified_frames[-1]] if modified_frames else None,
            'max_weight': round(max(weights), 3),
        })

    @toolset_registry.tool_call
    @staticmethod
    def copy_bone_keys(source_animation: unreal.AnimSequence, target_animation: unreal.AnimSequence) -> str:
        """Overwrites all bone track keys of target with source's keys (e.g. restore a
        sequence from its backup copy while keeping the target's settings, notifies and
        references). The asset is not saved.

        Args:
            source_animation: The sequence to copy keys from.
            target_animation: The sequence to overwrite. Must have the same bone tracks and key count.

        Returns:
            A JSON string: {"asset", "bone_tracks", "num_keys"}.
        """
        if source_animation.get_path_name() == target_animation.get_path_name():
            raise RuntimeError('Source and target are the same asset')

        for animation in (source_animation, target_animation):
            if animation.get_editor_property('additive_anim_type') != unreal.AdditiveAnimationType.AAT_NONE:
                raise RuntimeError(f'{animation.get_name()} is additive; copy_bone_keys does not support it')

        if source_animation.get_editor_property('skeleton') != target_animation.get_editor_property('skeleton'):
            raise RuntimeError('Source and target use different skeletons')

        source_model = source_animation.get_editor_property('data_model_interface')
        target_model = target_animation.get_editor_property('data_model_interface')
        source_rate = source_model.get_frame_rate()
        target_rate = target_model.get_frame_rate()

        if (source_rate.numerator * target_rate.denominator) != (target_rate.numerator * source_rate.denominator):
            raise RuntimeError('Source and target frame rates differ')

        num_keys = source_model.get_number_of_keys()
        bone_names = [str(name) for name in source_model.get_bone_track_names()]
        target_names = [str(name) for name in target_model.get_bone_track_names()]

        if target_model.get_number_of_keys() != num_keys or sorted(target_names) != sorted(bone_names):
            raise RuntimeError(f'{target_animation.get_name()} does not match {source_animation.get_name()}')

        frames = [_anim_lib().get_bone_poses_for_frame(source_animation, bone_names, frame, False)
                  for frame in range(num_keys)]
        controller = target_animation.get_editor_property('controller')

        if not controller:
            raise RuntimeError(f'{target_animation.get_name()} has no animation data controller')

        controller.open_bracket(unreal.Text('Copy Bone Keys'), False)

        try:
            for bone_index, bone_name in enumerate(bone_names):
                transforms = [frame[bone_index] for frame in frames]
                controller.set_bone_track_keys(
                    bone_name, [transform.translation for transform in transforms],
                    [transform.rotation for transform in transforms],
                    [transform.scale3d for transform in transforms], False)
        finally:
            controller.close_bracket(False)

        _mark_dirty(target_animation)
        return json.dumps({'asset': target_animation.get_path_name(), 'bone_tracks': len(bone_names),
                           'num_keys': num_keys})

    @toolset_registry.tool_call
    @staticmethod
    def trim_sequence(
            animation: unreal.AnimSequence, start_time: float, end_time: float = -1.0,
            rebase_root: bool = True) -> str:
        """Removes frames outside [start_time, end_time] from an animation sequence in place.
        The asset is not saved. Montages using the sequence are not updated.

        Bone keys of the kept range are re-written explicitly after the resize, and float
        curves are resized by the engine. Times snap to the nearest frame. Notifies and
        sync markers are not moved; trim sequences without them or fix them afterwards.
        Kept keys are the evaluated pose per frame, so additive sequences are not supported.
        Edits are not transacted (no undo) and a failure can leave a partially trimmed
        asset, so duplicate the sequence as a backup first.

        Args:
            animation: The animation sequence to trim.
            start_time: First kept time in seconds.
            end_time: Last kept time in seconds. Negative keeps up to the end.
            rebase_root: Shift the skeleton root bone translation so the new first frame
                starts where the old first frame started.

        Returns:
            A JSON string: {"asset", "removed_start_frames", "removed_end_frames",
            "num_keys", "length"}.
        """
        model = animation.get_editor_property('data_model_interface')
        num_keys = model.get_number_of_keys()
        frame_rate = model.get_frame_rate()
        fps = frame_rate.numerator / frame_rate.denominator
        last_frame = num_keys - 1
        start_frame = int(round(start_time * fps))
        end_frame = last_frame if end_time < 0 else int(round(end_time * fps))

        if not 0 <= start_frame < end_frame <= last_frame:
            raise ValueError(
                f'Invalid trim range {start_time}s-{end_time}s (frames {start_frame}-{end_frame}, '
                f'last frame {last_frame})')

        bone_names = [str(name) for name in model.get_bone_track_names()]
        skeleton_root = str(animation.get_editor_property('skeleton').get_reference_pose().get_bone_names()[0])
        root_index = bone_names.index(skeleton_root) if skeleton_root in bone_names else -1

        if rebase_root and root_index < 0:
            raise RuntimeError(f'rebase_root requested but root bone {skeleton_root} has no animation track')

        #리사이즈 전에 남길 구간 샘플링
        kept = [_anim_lib().get_bone_poses_for_frame(animation, bone_names, frame, False)
                for frame in range(start_frame, end_frame + 1)]
        root_offset = unreal.Vector(0.0, 0.0, 0.0)

        if rebase_root:
            first = _anim_lib().get_bone_poses_for_frame(animation, bone_names, 0, False)
            root_offset = first[root_index].translation - kept[0][root_index].translation

        controller = animation.get_editor_property('controller')

        if not controller:
            raise RuntimeError(f'{animation.get_name()} has no animation data controller')

        if animation.get_editor_property('additive_anim_type') != unreal.AdditiveAnimationType.AAT_NONE:
            raise RuntimeError(f'{animation.get_name()} is additive; trim_sequence does not support additive sequences')

        controller.open_bracket(unreal.Text('Trim Animation'), False)

        try:
            #뒤쪽부터 제거해야 앞쪽 프레임 번호가 유지됨
            if end_frame < last_frame:
                controller.resize_in_frames(
                    unreal.FrameNumber(end_frame), unreal.FrameNumber(end_frame),
                    unreal.FrameNumber(last_frame), False)

            if start_frame > 0:
                controller.resize_in_frames(
                    unreal.FrameNumber(end_frame - start_frame), unreal.FrameNumber(0),
                    unreal.FrameNumber(start_frame), False)

            for bone_index, bone_name in enumerate(bone_names):
                transforms = [frame[bone_index] for frame in kept]
                positions = [transform.translation for transform in transforms]

                if bone_index == root_index:
                    positions = [position + root_offset for position in positions]

                controller.set_bone_track_keys(
                    bone_name, positions,
                    [transform.rotation for transform in transforms],
                    [transform.scale3d for transform in transforms], False)
        finally:
            controller.close_bracket(False)

        _mark_dirty(animation)

        return json.dumps({
            'asset': animation.get_path_name(),
            'removed_start_frames': start_frame,
            'removed_end_frames': last_frame - end_frame,
            'num_keys': model.get_number_of_keys(),
            'length': _anim_lib().get_sequence_length(animation),
        })

    @toolset_registry.tool_call
    @staticmethod
    def create_reversed_sequence(
            source_animation: unreal.AnimSequence, folder_path: str, asset_name: str,
            rebase_root: bool = True, overwrite: bool = False) -> str:
        """Creates a copy of an animation sequence whose keys play backwards. The asset is not saved.

        Bakes reversed bone keys and float curves instead of relying on a negative
        play rate, so root motion, montages and network playback behave like a normal
        forward animation. Root motion is reversed too (a forward jump becomes a
        backward one). Notifies and sync markers are removed because their timing no
        longer applies; the result lists mirrored notify times so they can be re-added.
        Reversed float curves are recreated, so their flags and interpolation reset to
        defaults. Transform curves and attribute curves are not reversed.
        If anything fails, the partially created asset is deleted.

        Args:
            source_animation: The animation sequence to reverse.
            folder_path: The content-browser folder for the new sequence.
            asset_name: The name of the new sequence (e.g. 'A_Wolf_BackJump').
            rebase_root: Shift the skeleton root bone translation (in its local space,
                all axes) so the reversed first frame starts where the source first frame
                starts. Raises if the root bone has no animation track.
            overwrite: If the asset already exists, re-bake its bone keys and float curves
                in place instead of failing. Its settings (root motion, etc.), references
                (montages), notifies and sync markers are kept. Raises before writing if its
                bone tracks or key count differ from the source.

        Returns:
            A JSON string: {"asset", "length", "num_keys", "bone_tracks", "root_bone",
            "curves_reversed": [str], "removed_sync_markers": int, "removed_notifies":
            [{"name", "track", "mirrored_time", "duration"}]}.
        """
        destination = f'{folder_path}/{asset_name}'

        if unreal.EditorAssetLibrary.does_asset_exist(destination):
            if not overwrite:
                raise RuntimeError(f'{destination} already exists')

            existing = unreal.EditorAssetLibrary.load_asset(destination)

            if not isinstance(existing, unreal.AnimSequence):
                raise RuntimeError(f'{destination} is not an AnimSequence')

            #기존 에셋은 참조(몽타주)가 있을 수 있어 실패해도 삭제하지 않음, 사용자가 추가한 노티파이도 유지
            return json.dumps(_bake_reversed(source_animation, existing, rebase_root, False))

        reversed_animation = unreal.EditorAssetLibrary.duplicate_asset(source_animation.get_path_name(), destination)

        if not isinstance(reversed_animation, unreal.AnimSequence):
            raise RuntimeError(f'Failed to duplicate to {destination}')

        try:
            result = _bake_reversed(source_animation, reversed_animation, rebase_root, True)
        except Exception:
            #실패 시 반쯤 변형된 복제 에셋 정리
            unreal.EditorAssetLibrary.delete_asset(destination)
            raise

        return json.dumps(result)
