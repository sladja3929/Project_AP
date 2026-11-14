"""프로젝트 툴세트 등록 목록."""

from toolset_registry.registration import Registration

from ap_toolset.toolsets import anim_montage
from ap_toolset.toolsets import retarget
from ap_toolset.toolsets import state_tree_compile
from ap_toolset.toolsets import state_tree_edit
from ap_toolset.toolsets import toolset_admin

registration = Registration([
    anim_montage.AnimMontageTools,
    retarget.RetargetTools,
    state_tree_compile.StateTreeCompileTools,
    state_tree_edit.StateTreeEditTools,
    toolset_admin.ProjectToolsetAdminTools,
])
