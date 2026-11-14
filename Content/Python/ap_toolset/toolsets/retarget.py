"""IK Rig / IK Retargeter 구성 및 배치 리타게팅 툴세트.

엔진 5.8 내장 툴세트에는 리타게팅 툴이 없어 프로젝트에서 보충.
사족 보행처럼 자동 체인 생성이 맞지 않는 스켈레톤은 get_bone_hierarchy로 본 구조를 확인한 뒤
set_retarget_chains로 체인을 직접 정의하고, 소스·타깃 체인 이름을 맞춰 Exact 매핑을 사용.
"""

import json
import math

import unreal

import toolset_registry
from toolset_registry.helpers import create_asset


def _side(source_or_target: str) -> unreal.RetargetSourceOrTarget:
    if source_or_target.lower() == 'source':
        return unreal.RetargetSourceOrTarget.SOURCE

    if source_or_target.lower() == 'target':
        return unreal.RetargetSourceOrTarget.TARGET

    raise ValueError(f"source_or_target must be 'source' or 'target', got '{source_or_target}'")


def _chains_to_list(rig_controller: unreal.IKRigController) -> list:
    return [{
        'name': str(chain.get_editor_property('chain_name')),
        'start_bone': str(rig_controller.get_retarget_chain_start_bone(chain.get_editor_property('chain_name'))),
        'end_bone': str(rig_controller.get_retarget_chain_end_bone(chain.get_editor_property('chain_name'))),
        'goal': str(rig_controller.get_retarget_chain_goal(chain.get_editor_property('chain_name'))),
    } for chain in rig_controller.get_retarget_chains()]


def _rig_info(rig: unreal.IKRigDefinition) -> dict:
    controller = unreal.IKRigController.get_controller(rig)
    mesh = controller.get_skeletal_mesh()
    return {
        'asset': rig.get_path_name(),
        'skeletal_mesh': mesh.get_path_name() if mesh else None,
        'retarget_root': str(controller.get_retarget_root()),
        'root_motion_bone': str(controller.get_root_motion_bone()),
        'chains': _chains_to_list(controller),
    }


def _chain_mapping(retargeter_controller: unreal.IKRetargeterController, target_rig: unreal.IKRigDefinition) -> dict:
    target_chains = [str(chain.get_editor_property('chain_name'))
                     for chain in unreal.IKRigController.get_controller(target_rig).get_retarget_chains()]
    mapping = {}

    for target_chain in target_chains:
        source_chain = str(retargeter_controller.get_source_chain(target_chain))
        mapping[target_chain] = '' if source_chain == 'None' else source_chain

    return mapping


def _quat_angle(quat: unreal.Quat) -> float:
    return math.degrees(2.0 * math.acos(min(1.0, abs(quat.w))))


def _auto_map(controller: unreal.IKRetargeterController, map_type: str) -> None:
    types = {'exact': unreal.AutoMapChainType.EXACT, 'fuzzy': unreal.AutoMapChainType.FUZZY}

    if map_type.lower() not in types:
        raise ValueError(f"map_type must be 'Exact' or 'Fuzzy', got '{map_type}'")

    controller.auto_map_chains(types[map_type.lower()], True)


@unreal.uclass()
class RetargetTools(unreal.ToolsetDefinition):
    """Provides tools for building IK Rigs and IK Retargeters and batch retargeting
    animation sequences between skeletons."""

    @toolset_registry.tool_call
    @staticmethod
    def get_bone_hierarchy(mesh: unreal.SkeletalMesh) -> str:
        """Returns every bone of a skeletal mesh with its parent and reference pose.

        Use this to design retarget chains (start bone must be an ancestor of end bone).

        Args:
            mesh: The skeletal mesh to inspect.

        Returns:
            A JSON array in skeleton order (root first): [{"name", "parent",
            "ref_location" [x, y, z] (component space)}].
        """
        skeleton = mesh.get_editor_property('skeleton')
        assert skeleton, f'{mesh.get_name()} has no skeleton assigned.'
        subsystem = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
        pose = skeleton.get_reference_pose()
        bones = []

        for name in pose.get_bone_names():
            parent = str(subsystem.get_bone_parent(mesh, name))
            location = unreal.AnimPoseExtensions.get_bone_pose(
                pose, name, unreal.AnimPoseSpaces.WORLD).translation
            bones.append({
                'name': str(name),
                'parent': '' if parent == 'None' else parent,
                'ref_location': [round(location.x, 2), round(location.y, 2), round(location.z, 2)],
            })

        return json.dumps(bones)

    @toolset_registry.tool_call
    @staticmethod
    def create_ik_rig(
            folder_path: str, asset_name: str, mesh: unreal.SkeletalMesh,
            auto_generate_chains: bool = False) -> str:
        """Creates an IK Rig for a skeletal mesh. The asset is not saved.

        Auto generation works for humanoids and some known templates. For other
        skeletons (e.g. quadrupeds) leave it off and call set_retarget_chains.

        Args:
            folder_path: The content-browser folder to create the rig in.
            asset_name: The name of the new rig (e.g. 'IK_Wolf').
            mesh: The skeletal mesh the rig operates on.
            auto_generate_chains: Apply the engine's auto-generated retarget definition.

        Returns:
            A JSON string of the rig info (asset, skeletal_mesh, retarget_root,
            root_motion_bone, chains).
        """
        rig = create_asset(folder_path, asset_name, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
        assert isinstance(rig, unreal.IKRigDefinition)
        controller = unreal.IKRigController.get_controller(rig)

        if not controller.set_skeletal_mesh(mesh):
            raise RuntimeError(f'Failed to assign {mesh.get_name()} to {asset_name}')

        if auto_generate_chains and not controller.apply_auto_generated_retarget_definition():
            raise RuntimeError(f'Auto retarget definition failed for {asset_name}. Define chains manually.')

        return json.dumps(_rig_info(rig))

    @toolset_registry.tool_call
    @staticmethod
    def get_ik_rig_info(rig: unreal.IKRigDefinition) -> str:
        """Returns the retarget definition of an IK Rig.

        Args:
            rig: The IK Rig to query.

        Returns:
            A JSON string: {"asset", "skeletal_mesh", "retarget_root",
            "root_motion_bone", "chains": [{"name", "start_bone", "end_bone", "goal"}]}.
        """
        return json.dumps(_rig_info(rig))

    @toolset_registry.tool_call
    @staticmethod
    def set_retarget_chains(
            rig: unreal.IKRigDefinition, retarget_root: str, chains: str,
            root_motion_bone: str = '', clear_existing: bool = True) -> str:
        """Defines the retarget root and retarget chains of an IK Rig. The asset is not saved.

        Retarget root is usually the pelvis/hips bone (the bone that carries body
        translation), not the skeleton root. Chain names should match between source
        and target rigs so the retargeter can map them with exact matching.

        Args:
            rig: The IK Rig to modify.
            retarget_root: The retarget root bone name.
            chains: JSON array of {"name", "start_bone", "end_bone"}. start_bone must be
                end_bone or one of its ancestors. Example: [{"name": "Spine",
                "start_bone": "spine_01", "end_bone": "spine_03"}].
            root_motion_bone: Optional root motion bone (usually the skeleton root).
                Empty keeps the current setting.
            clear_existing: Remove all existing chains first.

        Returns:
            A JSON string of the resulting rig info.
        """
        controller = unreal.IKRigController.get_controller(rig)

        if not controller.set_retarget_root(retarget_root):
            raise RuntimeError(f'Failed to set retarget root {retarget_root}')

        if root_motion_bone and not controller.set_root_motion_bone(root_motion_bone):
            raise RuntimeError(f'Failed to set root motion bone {root_motion_bone}')

        if clear_existing:
            for chain in list(controller.get_retarget_chains()):
                controller.remove_retarget_chain(chain.get_editor_property('chain_name'))

        for entry in json.loads(chains):
            added = controller.add_retarget_chain(entry['name'], entry['start_bone'], entry['end_bone'], 'None')

            if str(added) == 'None':
                raise RuntimeError(f"Failed to add chain {entry['name']} ({entry['start_bone']} -> {entry['end_bone']})")

        return json.dumps(_rig_info(rig))

    @toolset_registry.tool_call
    @staticmethod
    def create_retargeter(
            folder_path: str, asset_name: str, source_rig: unreal.IKRigDefinition,
            target_rig: unreal.IKRigDefinition, map_type: str = 'Exact') -> str:
        """Creates an IK Retargeter with the default op stack and maps chains. The asset is not saved.

        Default ops: Pelvis Motion, FK Chains, Run IK Rig, Root Motion, Curve Remap.
        Preview meshes come from the IK Rigs.

        Args:
            folder_path: The content-browser folder to create the retargeter in.
            asset_name: The name of the new retargeter (e.g. 'RTG_Labrador_To_Wolf').
            source_rig: The IK Rig of the skeleton animations are copied from.
            target_rig: The IK Rig of the skeleton animations are copied to.
            map_type: Chain auto mapping, 'Exact' (same name) or 'Fuzzy' (closest name).

        Returns:
            A JSON string: {"asset", "ops": [str], "chain_mapping": {target_chain: source_chain}}.
            An empty source chain means the target chain is unmapped.
        """
        retargeter = create_asset(folder_path, asset_name, unreal.IKRetargeter, unreal.IKRetargetFactory())
        assert isinstance(retargeter, unreal.IKRetargeter)
        controller = unreal.IKRetargeterController.get_controller(retargeter)
        controller.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, source_rig)
        controller.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, target_rig)

        if controller.get_num_retarget_ops() == 0:
            controller.add_default_ops()

        #SetIKRig는 소스만 Op에 자동 반영하므로 타깃은 명시적으로 할당
        controller.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.TARGET, target_rig)
        _auto_map(controller, map_type)

        return json.dumps({
            'asset': retargeter.get_path_name(),
            'ops': [str(controller.get_op_name(index)) for index in range(controller.get_num_retarget_ops())],
            'chain_mapping': _chain_mapping(controller, target_rig),
        })

    @toolset_registry.tool_call
    @staticmethod
    def set_chain_mapping(retargeter: unreal.IKRetargeter, mapping: str) -> str:
        """Sets source chains for target chains on every chain-mapping op. The asset is not saved.

        Args:
            retargeter: The IK Retargeter to modify.
            mapping: JSON object {target_chain: source_chain}. Use '' to unmap.

        Returns:
            A JSON string of the full resulting mapping {target_chain: source_chain}.
        """
        controller = unreal.IKRetargeterController.get_controller(retargeter)

        for target_chain, source_chain in json.loads(mapping).items():
            if not controller.set_source_chain(source_chain or 'None', target_chain):
                raise RuntimeError(f'Failed to map {source_chain} -> {target_chain}')

        target_rig = controller.get_ik_rig(unreal.RetargetSourceOrTarget.TARGET)
        return json.dumps(_chain_mapping(controller, target_rig))

    @toolset_registry.tool_call
    @staticmethod
    def auto_align_retarget_pose(retargeter: unreal.IKRetargeter, source_or_target: str = 'target') -> bool:
        """Auto-aligns all mapped bones of one side's retarget pose to the other side
        (chain to chain). Use when the two skeletons have different reference poses.
        The asset is not saved.

        Args:
            retargeter: The IK Retargeter to modify.
            source_or_target: Which side's retarget pose to modify, 'source' or 'target'.

        Returns:
            True when finished.
        """
        controller = unreal.IKRetargeterController.get_controller(retargeter)
        controller.auto_align_all_bones(_side(source_or_target), unreal.RetargetAutoAlignMethod.CHAIN_TO_CHAIN)
        return True

    @toolset_registry.tool_call
    @staticmethod
    def get_retarget_pose_offsets(retargeter: unreal.IKRetargeter, source_or_target: str = 'target') -> str:
        """Lists bones whose rotation is offset in the current retarget pose of one side.

        Args:
            retargeter: The IK Retargeter to query.
            source_or_target: Which side's retarget pose to query, 'source' or 'target'.

        Returns:
            A JSON string: {"pose", "root_offset" [x, y, z], "bones": [{"name",
            "angle" (degrees), "rotator" [roll, pitch, yaw]}]} sorted by angle, largest first.
        """
        side = _side(source_or_target)
        controller = unreal.IKRetargeterController.get_controller(retargeter)
        mesh = controller.get_preview_mesh(side)
        assert mesh, f'{retargeter.get_name()} has no {source_or_target} preview mesh'
        bones = []

        for name in mesh.get_editor_property('skeleton').get_reference_pose().get_bone_names():
            offset = controller.get_rotation_offset_for_retarget_pose_bone(name, side)
            angle = _quat_angle(offset)

            if angle > 0.01:
                rotator = offset.rotator()
                bones.append({
                    'name': str(name),
                    'angle': round(angle, 2),
                    'rotator': [round(rotator.roll, 2), round(rotator.pitch, 2), round(rotator.yaw, 2)],
                })

        bones.sort(key=lambda entry: entry['angle'], reverse=True)
        root_offset = controller.get_root_offset_in_retarget_pose(side)
        return json.dumps({
            'pose': str(controller.get_current_retarget_pose_name(side)),
            'root_offset': [round(root_offset.x, 2), round(root_offset.y, 2), round(root_offset.z, 2)],
            'bones': bones,
        })

    @toolset_registry.tool_call
    @staticmethod
    def reset_retarget_pose_bones(
            retargeter: unreal.IKRetargeter, bones: list[str], source_or_target: str = 'target') -> bool:
        """Removes retarget pose rotation offsets of the given bones (back to reference pose).
        The asset is not saved.

        Args:
            retargeter: The IK Retargeter to modify.
            bones: Bone names to reset.
            source_or_target: Which side's current retarget pose to modify, 'source' or 'target'.

        Returns:
            True when finished.
        """
        side = _side(source_or_target)
        controller = unreal.IKRetargeterController.get_controller(retargeter)
        controller.reset_retarget_pose(controller.get_current_retarget_pose_name(side), bones, side)
        return True

    @toolset_registry.tool_call
    @staticmethod
    def set_retarget_pose_bone_rotation(
            retargeter: unreal.IKRetargeter, bone: str, roll: float, pitch: float, yaw: float,
            source_or_target: str = 'target') -> str:
        """Sets the rotation offset (relative to reference pose, bone local space) of one
        bone in the current retarget pose. Replaces any existing offset. The asset is not saved.

        Use when the two skeletons' reference poses differ for a bone (e.g. one mesh is
        bound with an open mouth), so the retarget poses represent the same posture.

        Args:
            retargeter: The IK Retargeter to modify.
            bone: The bone name.
            roll: Roll in degrees.
            pitch: Pitch in degrees.
            yaw: Yaw in degrees.
            source_or_target: Which side's current retarget pose to modify, 'source' or 'target'.

        Returns:
            A JSON string of the resulting offset: {"bone", "angle", "rotator" [roll, pitch, yaw]}.
        """
        side = _side(source_or_target)
        controller = unreal.IKRetargeterController.get_controller(retargeter)
        offset = unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw).quaternion()
        controller.set_rotation_offset_for_retarget_pose_bone(bone, offset, side)
        result = controller.get_rotation_offset_for_retarget_pose_bone(bone, side)
        rotator = result.rotator()
        return json.dumps({
            'bone': bone,
            'angle': round(_quat_angle(result), 2),
            'rotator': [round(rotator.roll, 2), round(rotator.pitch, 2), round(rotator.yaw, 2)],
        })

    @toolset_registry.tool_call
    @staticmethod
    def batch_retarget(
            retargeter: unreal.IKRetargeter, animations: list[unreal.AnimSequence],
            source_mesh: unreal.SkeletalMesh, target_mesh: unreal.SkeletalMesh,
            target_folder: str, prefix: str = '', suffix: str = '',
            search: str = '', replace: str = '', overwrite: bool = False) -> list[str]:
        """Duplicates animation sequences and retargets them to the target mesh's skeleton.

        New assets are named prefix + (name with search replaced) + suffix and are
        created in target_folder. Referenced assets are not duplicated.

        Args:
            retargeter: The IK Retargeter to use.
            animations: The source animation sequences.
            source_mesh: A skeletal mesh of the source skeleton.
            target_mesh: The skeletal mesh to retarget onto.
            target_folder: The content-browser folder for the new animations.
            prefix: Prefix for new asset names.
            suffix: Suffix for new asset names.
            search: Substring of source names to replace.
            replace: Replacement for search.
            overwrite: Overwrite existing assets with the same name.

        Returns:
            Object paths of the created animation assets (usable as refPath).
        """
        inputs = unreal.IKRetargetBatchOperationInputs()
        asset_datas = []

        for anim in animations:
            asset_data = unreal.EditorAssetLibrary.find_asset_data(anim.get_path_name())

            if not asset_data.is_valid():
                raise RuntimeError(f'Asset data not found for {anim.get_path_name()}')

            asset_datas.append(asset_data)

        inputs.set_editor_property('assets_to_retarget', asset_datas)
        inputs.set_editor_property('source_mesh', source_mesh)
        inputs.set_editor_property('target_mesh', target_mesh)
        inputs.set_editor_property('ik_retarget_asset', retargeter)
        inputs.set_editor_property('target_path', target_folder)
        inputs.set_editor_property('use_source_path', False)
        inputs.set_editor_property('include_referenced_assets', False)
        inputs.set_editor_property('overwrite_existing_files', overwrite)
        inputs.set_editor_property('prefix', prefix)
        inputs.set_editor_property('suffix', suffix)
        inputs.set_editor_property('search', search)
        inputs.set_editor_property('replace', replace)
        results = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
        return [f"{asset_data.get_editor_property('package_name')}.{asset_data.get_editor_property('asset_name')}"
                for asset_data in results]
