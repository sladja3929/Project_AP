"""레벨 편집 보조 툴세트.

엔진 5.8 내장 SceneTools에 없는 레벨 제작 기능을 프로젝트에서 보충.
- 내비게이션 재빌드 (Static 내비메시)
- 빈 레벨 생성
- 높이맵·웨이트맵 raw 파일로 랜드스케이프 생성 (C++ UActionPracticeLandscapeEditLibrary 경유)
- 폴리지 타입·랜드스케이프 그래스 타입 에셋 생성, 폴리지 인스턴스 일괄 배치
"""

import json
import os

import unreal

import toolset_registry


def _landscape_library():
    library = getattr(unreal, 'ActionPracticeLandscapeEditLibrary', None)

    if library is None:
        raise RuntimeError(
            'ActionPracticeLandscapeEditLibrary not found. Build the ActionPracticeEditor module and restart the editor.')

    return library


def _create_asset(folder_path: str, asset_name: str, asset_class, factory) -> unreal.Object:
    asset_path = f'{folder_path}/{asset_name}'

    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        raise RuntimeError(f'Asset already exists: {asset_path}')

    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(asset_name, folder_path, asset_class, factory)

    if asset is None:
        raise RuntimeError(f'Failed to create {asset_class.__name__} at {asset_path}')

    return asset


def _editor_world() -> unreal.World:
    #PIE 중에는 콘솔 명령이 PIE 월드 기준으로 동작할 수 있어 거부
    if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
        raise RuntimeError('Stop the Play-In-Editor session before editing the level')

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

    if world is None:
        raise RuntimeError('No editor world is loaded')

    return world


def _bounds_to_dict(actor: unreal.Actor) -> dict:
    origin, extent = actor.get_actor_bounds(False)

    return {
        'min': {'x': round(origin.x - extent.x), 'y': round(origin.y - extent.y), 'z': round(origin.z - extent.z)},
        'max': {'x': round(origin.x + extent.x), 'y': round(origin.y + extent.y), 'z': round(origin.z + extent.z)},
    }


@unreal.uclass()
class LevelEditTools(unreal.ToolsetDefinition):
    """Provides level editing helpers missing from the built-in SceneTools, such as rebuilding navigation."""

    @toolset_registry.tool_call
    @staticmethod
    def rebuild_navigation() -> str:
        """Rebuilds the navigation mesh of the level currently open in the editor.

        Runs the engine 'RebuildNavigation' console command (UNavigationSystemV1::Build).
        Call after moving or resizing NavMeshBoundsVolume actors when the RecastNavMesh
        uses Static runtime generation. Fails while Play-In-Editor is running or when the
        level has no NavMeshBoundsVolume. Save the level afterwards with
        AssetTools.save_assets to keep the result.

        Returns:
            JSON with the level path, the NavMeshBoundsVolume count and the bounds of each
            RecastNavMesh after the build. Compare the bounds with the volumes to confirm the
            build covered them; a build that is still running reports the previous bounds.
        """
        world = _editor_world()
        actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
        volumes = [actor for actor in actors if isinstance(actor, unreal.NavMeshBoundsVolume)]

        if not volumes:
            raise RuntimeError('The level has no NavMeshBoundsVolume to build navigation from')

        unreal.SystemLibrary.execute_console_command(world, 'RebuildNavigation')

        navmeshes = [actor for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
                     if isinstance(actor, unreal.RecastNavMesh)]

        return json.dumps({
            'level': world.get_path_name(),
            'nav_volumes': len(volumes),
            'navmeshes': {navmesh.get_actor_label(): _bounds_to_dict(navmesh) for navmesh in navmeshes},
        })

    @toolset_registry.tool_call
    @staticmethod
    def create_level(asset_path: str) -> str:
        """Creates a new empty (non World Partition) level asset and opens it in the editor.

        The current level must be saved first; unsaved changes make the call fail.

        Args:
            asset_path: The package path of the new level (e.g. '/Game/Maps/Lvl_NewMap').

        Returns:
            The path of the opened level.
        """
        if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
            raise RuntimeError('Stop the Play-In-Editor session before creating a level')

        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            raise RuntimeError(f'Level already exists: {asset_path}')

        if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).new_level(asset_path, False):
            raise RuntimeError(f'Failed to create level {asset_path} (save the current level first)')

        return _editor_world().get_path_name()

    @toolset_registry.tool_call
    @staticmethod
    def create_landscape(
            heightmap_file: str, layers: str, material: unreal.MaterialInterface, layer_info_folder: str,
            location_x: float, location_y: float, location_z: float, scale_xy: float = 100.0, scale_z: float = 100.0,
            component_count: int = 8, sections_per_component: int = 1, quads_per_section: int = 63) -> str:
        """Creates a landscape in the open level from raw heightmap and weightmap files.

        Size in vertices is component_count * sections_per_component * quads_per_section + 1
        on both axes (default 8 * 1 * 63 + 1 = 505). The heightmap is 16-bit little-endian raw
        (.r16), row-major with X fastest; 32768 is Z 0 and one unit is scale_z / 128 cm.
        Weightmaps are 8-bit raw files of the same size whose layer values sum to 255 per
        vertex. Layer info assets are reused from or created in layer_info_folder and saved.

        Args:
            heightmap_file: Absolute path to the .r16 heightmap file.
            layers: JSON array of {"name": str, "file": str} weightmap entries, in the order
                the landscape material's layer blend expects. May be an empty array.
            material: The landscape material to assign.
            layer_info_folder: Content folder for the layer info assets (e.g. '/Game/Maps/X/LayerInfo').
            location_x: World X of landscape vertex (0, 0).
            location_y: World Y of landscape vertex (0, 0).
            location_z: World Z of the landscape actor (height value 32768 maps to this Z).
            scale_xy: Distance between vertices in cm.
            scale_z: Vertical scale of the actor.
            component_count: Number of components on each axis.
            sections_per_component: 1 or 2.
            quads_per_section: One of 7, 15, 31, 63, 127, 255.

        Returns:
            JSON with the landscape label, its world bounds and the created layer names.
        """
        _editor_world()
        entries = json.loads(layers)

        for index, entry in enumerate(entries):
            if 'name' not in entry or 'file' not in entry:
                raise RuntimeError(f'Layer entry {index} must have "name" and "file" keys, got {entry}')

        for path in [heightmap_file] + [entry['file'] for entry in entries]:
            if not os.path.isfile(path):
                raise RuntimeError(f'File not found: {path}')

        names = [unreal.Name(entry['name']) for entry in entries]
        files = [entry['file'] for entry in entries]

        #Python 반환 순서: (반환값, out 파라미터)
        error, landscape = _landscape_library().create_landscape_from_raw(
            unreal.Vector(location_x, location_y, location_z), unreal.Vector(scale_xy, scale_xy, scale_z),
            component_count, component_count, sections_per_component, quads_per_section,
            heightmap_file, names, files, material, layer_info_folder)

        if error:
            raise RuntimeError(error)

        if entries:
            unreal.EditorAssetLibrary.save_directory(layer_info_folder, only_if_is_dirty=True, recursive=False)

        return json.dumps({
            'landscape': landscape.get_actor_label(),
            'bounds': _bounds_to_dict(landscape),
            'layers': [entry['name'] for entry in entries],
        })

    @toolset_registry.tool_call
    @staticmethod
    def create_foliage_type(
            folder_path: str, asset_name: str, mesh: unreal.StaticMesh, collision: bool,
            cull_end: int = 0, cast_shadow: bool = True) -> unreal.FoliageType:
        """Creates a FoliageType_InstancedStaticMesh asset for painting or scattering a mesh.

        Args:
            folder_path: The content folder for the new asset.
            asset_name: The asset name (e.g. 'FT_ScotsPine_01').
            mesh: The static mesh the foliage type instances.
            collision: True keeps the mesh collision (BlockAll), False disables it (NoCollision).
            cull_end: Instance cull distance in cm; 0 means never culled.
            cast_shadow: Whether instances cast shadows.

        Returns:
            The new foliage type asset.
        """
        foliage_type = _create_asset(folder_path, asset_name, unreal.FoliageType_InstancedStaticMesh,
                                     unreal.FoliageType_InstancedStaticMeshFactory())
        foliage_type.set_editor_property('mesh', mesh)
        foliage_type.set_editor_property('cull_distance', unreal.Int32Interval(min=0, max=cull_end))
        foliage_type.set_editor_property('cast_shadow', cast_shadow)

        body = foliage_type.get_editor_property('body_instance')
        body.set_editor_property('collision_profile_name', 'BlockAll' if collision else 'NoCollision')
        foliage_type.set_editor_property('body_instance', body)

        unreal.EditorAssetLibrary.save_loaded_asset(foliage_type, only_if_is_dirty=False)
        return foliage_type

    @toolset_registry.tool_call
    @staticmethod
    def create_landscape_grass_type(folder_path: str, asset_name: str, varieties: str) -> unreal.LandscapeGrassType:
        """Creates a LandscapeGrassType asset that a landscape material's LandscapeGrassOutput
        node spawns automatically on a weight-mapped layer.

        Args:
            folder_path: The content folder for the new asset.
            asset_name: The asset name (e.g. 'LGT_ForestFloor').
            varieties: JSON array of {"mesh": str asset path, "density": float per 10m x 10m,
                "start_cull": int cm, "end_cull": int cm, "scale_min": float, "scale_max": float,
                "align_to_surface": bool, "cast_shadow": bool}.

        Returns:
            The new grass type asset.
        """
        grass_type = _create_asset(folder_path, asset_name, unreal.LandscapeGrassType, unreal.LandscapeGrassTypeFactory())
        result = []

        for entry in json.loads(varieties):
            mesh = unreal.EditorAssetLibrary.load_asset(entry['mesh'])

            if not isinstance(mesh, unreal.StaticMesh):
                raise RuntimeError(f'Not a static mesh: {entry["mesh"]}')

            variety = unreal.GrassVariety()
            variety.set_editor_property('grass_mesh', mesh)
            variety.set_editor_property('grass_density', unreal.PerPlatformFloat(default=entry['density']))
            variety.set_editor_property('grass_density_quality', unreal.PerQualityLevelFloat(default=entry['density']))
            variety.set_editor_property('start_cull_distance', unreal.PerPlatformInt(default=entry['start_cull']))
            variety.set_editor_property('start_cull_distance_quality', unreal.PerQualityLevelInt(default=entry['start_cull']))
            variety.set_editor_property('end_cull_distance', unreal.PerPlatformInt(default=entry['end_cull']))
            variety.set_editor_property('end_cull_distance_quality', unreal.PerQualityLevelInt(default=entry['end_cull']))
            variety.set_editor_property('scale_x', unreal.FloatInterval(min=entry['scale_min'], max=entry['scale_max']))
            variety.set_editor_property('random_rotation', True)
            variety.set_editor_property('align_to_surface', entry['align_to_surface'])
            variety.set_editor_property('cast_dynamic_shadow', entry['cast_shadow'])
            result.append(variety)

        grass_type.set_editor_property('grass_varieties', result)
        unreal.EditorAssetLibrary.save_loaded_asset(grass_type, only_if_is_dirty=False)
        return grass_type

    @toolset_registry.tool_call
    @staticmethod
    def scatter_foliage(foliage_type: unreal.FoliageType, transforms_file: str, clear_existing: bool = True) -> str:
        """Adds foliage instances of a foliage type to the open level from a JSON file.

        Instances are added to the level's InstancedFoliageActor, so they stay editable with
        the editor Foliage tool. Save the level afterwards with AssetTools.save_assets.

        Args:
            foliage_type: The foliage type asset to place.
            transforms_file: Absolute path to a JSON array of [x, y, z, pitch, yaw, roll, scale]
                entries in world space (cm, degrees).
            clear_existing: When True, removes all existing instances of this foliage type first.

        Returns:
            JSON with the number of instances added.
        """
        world = _editor_world()

        if not os.path.isfile(transforms_file):
            raise RuntimeError(f'File not found: {transforms_file}')

        with open(transforms_file, encoding='utf-8') as file:
            entries = json.load(file)

        #기존 인스턴스를 지우기 전에 입력 전체를 변환해 두어, 입력 오류로 기존 배치만 사라지는 일을 방지
        transforms = []

        for index, entry in enumerate(entries):
            if len(entry) != 7:
                raise RuntimeError(f'Entry {index} must be [x, y, z, pitch, yaw, roll, scale], got {entry}')

            x, y, z, pitch, yaw, roll, scale = entry
            transforms.append(unreal.Transform(
                location=unreal.Vector(x, y, z),
                rotation=unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw),
                scale=unreal.Vector(scale, scale, scale)))

        #대량 배치를 Undo 한 번으로 되돌릴 수 있도록 트랜잭션으로 묶음
        with unreal.ScopedEditorTransaction(f'Scatter {foliage_type.get_name()}'):
            if clear_existing:
                unreal.InstancedFoliageActor.remove_all_instances(world, foliage_type)

            unreal.InstancedFoliageActor.add_instances(world, foliage_type, transforms)

        return json.dumps({'foliage_type': foliage_type.get_path_name(), 'added': len(transforms)})
