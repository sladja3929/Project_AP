"""오디오 에셋 툴세트.

엔진 5.8 내장 툴세트에 없는 오디오 에셋 작업을 프로젝트에서 보충.
- WAV 파일 임포트 (SoundWave)
- SoundWave 여러 개로 랜덤 SoundCue 생성 (SoundCueFactoryNew가 Random 노드를 구성)
- 감쇠(SoundAttenuation)·동시 재생 제한(SoundConcurrency)·사운드 클래스 에셋 생성
"""

import json
import os

import unreal

import toolset_registry
from toolset_registry.helpers import create_asset


_SOUND_ASSET_TYPES = {
    'SoundAttenuation': (unreal.SoundAttenuation, 'SoundAttenuationFactory'),
    'SoundConcurrency': (unreal.SoundConcurrency, 'SoundConcurrencyFactory'),
    'SoundClass': (unreal.SoundClass, 'SoundClassFactory'),
}


def _try_property(obj, name):
    #에디터 전용·비노출 프로퍼티는 None
    try:
        return obj.get_editor_property(name)
    except Exception:
        return None


def _wave_info(wave: unreal.SoundWave) -> dict:
    return {
        'asset': wave.get_path_name(),
        'duration': round(wave.get_editor_property('duration'), 3),
        'sample_rate': _try_property(wave, 'imported_sample_rate'),
        'channels': _try_property(wave, 'num_channels'),
    }


def _describe_node(node) -> dict:
    if node is None:
        return {'class': None}

    info = {'class': node.get_class().get_name()}

    if isinstance(node, unreal.SoundNodeWavePlayer):
        wave = node.get_editor_property('sound_wave')
        info['wave'] = wave.get_path_name() if wave else None

    children = node.get_editor_property('child_nodes')

    if children:
        info['children'] = [_describe_node(child) for child in children]

    return info


@unreal.uclass()
class AudioTools(unreal.ToolsetDefinition):
    """Provides audio asset tools missing from the built-in toolsets: WAV import, random SoundCue
    creation and SoundAttenuation / SoundConcurrency / SoundClass asset creation."""

    @toolset_registry.tool_call
    @staticmethod
    def import_sound_waves(source_files: list[str], folder_path: str, replace_existing: bool = False) -> str:
        """Imports WAV files from disk as SoundWave assets. The assets are not saved.

        Each asset is named after its file name without extension. All files are checked
        before anything is imported, so a missing file or an existing asset (when
        replace_existing is False) fails without importing any file.

        Args:
            source_files: Absolute paths of .wav files on disk.
            folder_path: Content folder to import into (e.g. '/Game/Sounds/Wolf').
            replace_existing: Re-import over existing assets of the same name, keeping
                their references. False fails if any asset already exists.

        Returns:
            A JSON string: {"imported": [{"asset", "duration", "sample_rate", "channels"}]}.
            sample_rate / channels are null when the engine does not expose them.
        """
        if not source_files:
            raise ValueError('source_files is empty')

        if not folder_path.startswith('/'):
            raise ValueError(f'folder_path must be a content path such as /Game/Sounds: {folder_path}')

        names = []

        for path in source_files:
            if not path.lower().endswith('.wav'):
                raise ValueError(f'Only .wav files are supported: {path}')

            if not os.path.isfile(path):
                raise FileNotFoundError(f'File not found: {path}')

            name = os.path.splitext(os.path.basename(path))[0]
            asset_path = f'{folder_path}/{name}'

            if not replace_existing and unreal.EditorAssetLibrary.does_asset_exist(asset_path):
                raise RuntimeError(f'Asset already exists: {asset_path} (pass replace_existing=True to re-import)')

            names.append(name)

        if len(set(names)) != len(names):
            raise ValueError('source_files contain duplicate file names')

        tasks = []

        for path in source_files:
            task = unreal.AssetImportTask()
            task.set_editor_property('filename', path)
            task.set_editor_property('destination_path', folder_path)
            task.set_editor_property('automated', True)
            task.set_editor_property('replace_existing', replace_existing)
            task.set_editor_property('save', False)
            tasks.append(task)

        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

        imported = []
        failed = []

        for name in names:
            wave = unreal.EditorAssetLibrary.load_asset(f'{folder_path}/{name}')

            if isinstance(wave, unreal.SoundWave):
                imported.append(_wave_info(wave))
            else:
                failed.append(name)

        if failed:
            raise RuntimeError(f'Import failed for: {failed} (imported: {[i["asset"] for i in imported]})')

        return json.dumps({'imported': imported})

    @toolset_registry.tool_call
    @staticmethod
    def create_sound_cue(folder_path: str, asset_name: str, sound_waves: list[unreal.SoundWave]) -> str:
        """Creates a SoundCue that plays the given SoundWaves. The asset is not saved.

        One wave makes WavePlayer -> Output. Several waves make a Random node over one
        WavePlayer per wave (equal weights, randomize without replacement), built by the
        engine SoundCueFactoryNew the same way as the content browser "Create Single Cue".
        Set attenuation, concurrency, sound class and volume afterwards with
        ObjectTools.set_properties on the cue.

        Args:
            folder_path: Content folder for the cue (e.g. '/Game/Sounds/Wolf').
            asset_name: Name of the cue (e.g. 'SC_Wolf_Snarl').
            sound_waves: The SoundWaves to play. Must not be empty or contain duplicates.

        Returns:
            A JSON string: {"asset", "duration", "graph": node tree from the output node}.
        """
        if not sound_waves:
            raise ValueError('sound_waves is empty')

        paths = [wave.get_path_name() for wave in sound_waves]

        if len(set(paths)) != len(paths):
            raise ValueError(f'sound_waves contain duplicates: {paths}')

        factory = unreal.SoundCueFactoryNew()
        factory.set_editor_property('initial_sound_waves', list(sound_waves))
        cue = create_asset(folder_path, asset_name, unreal.SoundCue, factory)
        assert isinstance(cue, unreal.SoundCue)

        graph = _describe_node(cue.get_editor_property('first_node'))
        waves_in_graph = []

        def collect(node_info):
            if node_info.get('wave'):
                waves_in_graph.append(node_info['wave'])

            for child in node_info.get('children', []):
                collect(child)

        collect(graph)

        if sorted(waves_in_graph) != sorted(paths):
            #잘못 구성된 Cue가 저장되지 않도록 방금 만든 에셋 하나만 삭제 (폴더 단위 삭제 금지)
            cue_path = cue.get_path_name()
            deleted = unreal.EditorAssetLibrary.delete_asset(f'{folder_path}/{asset_name}')
            raise RuntimeError(f'{cue_path} graph did not match the waves and was {"deleted" if deleted else "NOT deleted (delete it manually)"}: {graph}')

        return json.dumps({'asset': cue.get_path_name(), 'duration': round(cue.get_editor_property('duration'), 3), 'graph': graph})

    @toolset_registry.tool_call
    @staticmethod
    def describe_sound_cue(sound_cue: unreal.SoundCue) -> str:
        """Returns a SoundCue's node tree and main settings.

        Args:
            sound_cue: The SoundCue to inspect.

        Returns:
            A JSON string: {"asset", "duration", "volume_multiplier", "pitch_multiplier",
            "attenuation", "sound_class", "concurrency": [str], "graph"}.
        """
        attenuation = sound_cue.get_editor_property('attenuation_settings')
        sound_class = sound_cue.get_editor_property('sound_class_object')
        concurrency = sound_cue.get_editor_property('concurrency_set')

        return json.dumps({
            'asset': sound_cue.get_path_name(),
            'duration': round(sound_cue.get_editor_property('duration'), 3),
            'volume_multiplier': sound_cue.get_editor_property('volume_multiplier'),
            'pitch_multiplier': sound_cue.get_editor_property('pitch_multiplier'),
            'attenuation': attenuation.get_path_name() if attenuation else None,
            'sound_class': sound_class.get_path_name() if sound_class else None,
            'concurrency': [c.get_path_name() for c in concurrency] if concurrency else [],
            'graph': _describe_node(sound_cue.get_editor_property('first_node')),
        })

    @toolset_registry.tool_call
    @staticmethod
    def create_sound_asset(folder_path: str, asset_name: str, asset_type: str) -> unreal.Object:
        """Creates a SoundAttenuation, SoundConcurrency or SoundClass asset with default settings.
        The asset is not saved. Edit its settings with ObjectTools.set_properties.

        Args:
            folder_path: Content folder for the asset (e.g. '/Game/Sounds/Wolf').
            asset_name: Name of the asset (e.g. 'SA_Wolf_Vocal').
            asset_type: 'SoundAttenuation', 'SoundConcurrency' or 'SoundClass'.

        Returns:
            The created asset (unreal.SoundAttenuation, unreal.SoundConcurrency or unreal.SoundClass).
            Attenuation settings are in its 'Attenuation' struct, concurrency settings in 'Concurrency'.
        """
        if asset_type not in _SOUND_ASSET_TYPES:
            raise ValueError(f'asset_type must be one of {list(_SOUND_ASSET_TYPES)}: {asset_type}')

        asset_class, factory_name = _SOUND_ASSET_TYPES[asset_type]
        factory_class = getattr(unreal, factory_name, None)

        if factory_class is None:
            raise RuntimeError(f'unreal.{factory_name} is not available in this editor')

        return create_asset(folder_path, asset_name, asset_class, factory_class())
