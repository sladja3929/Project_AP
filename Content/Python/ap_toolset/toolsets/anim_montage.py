"""AnimMontage 생성 및 AnimNotify / AnimNotifyState 편집 툴세트.

엔진 5.8 내장 툴세트에는 몽타주 생성·노티파이 편집 툴이 없어 프로젝트에서 보충.
노티파이 프로퍼티(예: AnimNotifyState_HitDetection의 설정값)는 add_* 결과로 받은
오브젝트 레퍼런스를 ObjectTools.set_properties에 넘겨 편집.
"""

import json

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


@unreal.uclass()
class AnimMontageTools(unreal.ToolsetDefinition):
    """Provides tools for creating AnimMontage assets and editing AnimNotify /
    AnimNotifyState events on montages and animation sequences."""

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
    def get_root_motion_info(animation: unreal.AnimSequence, sample_count: int = 30) -> str:
        """Measures how the root bone moves in an animation sequence.

        Motion warping only modifies root motion, so a montage needs root motion enabled
        on its sequences for warp windows to have any effect. Use this to check whether
        enabling root motion would move the capsule (non-zero translation) or only lets
        the warp rotate it (in-place animation).

        Args:
            animation: The animation sequence to inspect.
            sample_count: Number of evenly spaced samples between start and end.

        Returns:
            A JSON string: {"enable_root_motion", "root_lock", "root_bone", "length",
            "end_translation" [x, y, z], "end_yaw", "max_horizontal_distance"}. Translation and
            yaw are relative to the first frame in the root bone's local space.
        """
        length = _anim_lib().get_sequence_length(animation)
        options = unreal.AnimPoseEvaluationOptions()
        extensions = unreal.AnimPoseExtensions
        root_bone = None
        start = None
        end = None
        max_distance = 0.0

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

        end_delta = end.translation - start.translation
        return json.dumps({
            'enable_root_motion': animation.get_editor_property('enable_root_motion'),
            'root_lock': str(animation.get_editor_property('root_motion_root_lock')),
            'root_bone': str(root_bone),
            'length': length,
            'end_translation': [round(end_delta.x, 2), round(end_delta.y, 2), round(end_delta.z, 2)],
            'end_yaw': round(end.rotation.rotator().yaw - start.rotation.rotator().yaw, 2),
            'max_horizontal_distance': round(max_distance, 2),
        })
