"""프로젝트 툴세트 등록 목록."""

from toolset_registry.registration import Registration

from ap_toolset.toolsets import anim_montage
from ap_toolset.toolsets import audio
from ap_toolset.toolsets import blueprint_defaults
from ap_toolset.toolsets import level_edit
from ap_toolset.toolsets import retarget
from ap_toolset.toolsets import state_tree_compile
from ap_toolset.toolsets import state_tree_edit
from ap_toolset.toolsets import toolset_admin
from ap_toolset.toolsets import ui_assets

registration = Registration([
    anim_montage.AnimMontageTools,
    audio.AudioTools,
    blueprint_defaults.BlueprintDefaultsTools,
    level_edit.LevelEditTools,
    retarget.RetargetTools,
    state_tree_compile.StateTreeCompileTools,
    state_tree_edit.StateTreeEditTools,
    toolset_admin.ProjectToolsetAdminTools,
    ui_assets.UIAssetTools,
])
