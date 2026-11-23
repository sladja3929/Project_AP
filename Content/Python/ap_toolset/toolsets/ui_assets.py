"""UI 에셋 툴세트.

엔진 5.8 내장 툴세트에 없는 UI 에셋 작업을 프로젝트에서 보충.
- 폰트 파일(.ttf/.otf) 임포트 (FontFace)
- FontFace 여러 개를 타입페이스로 묶은 런타임 폰트(Font) 생성
  FTypefaceEntry·FFontData는 스크립트에 노출되지 않아 ActionPracticeEditor 모듈의
  UActionPracticeEditorScriptLibrary.CreateFontFromFaces를 통해 처리
"""

import json
import os

import unreal

import toolset_registry


_FONT_EXTENSIONS = ('.ttf', '.otf', '.ttc', '.otc')


def _get_library():
    library = getattr(unreal, 'ActionPracticeEditorScriptLibrary', None)

    if library is None or not hasattr(library, 'create_font_from_faces'):
        raise RuntimeError(
            'ActionPracticeEditorScriptLibrary.create_font_from_faces not found. '
            'Build the ActionPracticeEditor module and restart the editor.')

    return library


def _describe_font(font: unreal.Font) -> dict:
    #타입페이스 수는 C++ 래퍼가 적용 후 재검증하므로 여기서는 노출되는 값만 보고
    info = {'asset': font.get_path_name()}

    try:
        info['font_cache_type'] = str(font.get_editor_property('font_cache_type'))
    except Exception:
        info['font_cache_type'] = None

    return info


@unreal.uclass()
class UIAssetTools(unreal.ToolsetDefinition):
    """Provides UI asset tools missing from the built-in toolsets: font file import (FontFace)
    and runtime Font asset creation from several FontFace assets (one typeface each)."""

    @toolset_registry.tool_call
    @staticmethod
    def import_font_faces(source_files: list[str], folder_path: str, asset_names: list[str], replace_existing: bool = False) -> str:
        """Imports .ttf/.otf/.ttc/.otc files from disk as FontFace assets. The assets are not saved.

        No Font asset is created by this call (the engine only offers that in an interactive
        dialog). Use create_font afterwards to group the faces into a Font usable by UMG.
        All inputs are checked before anything is imported.

        Args:
            source_files: Absolute paths of font files on disk.
            folder_path: Content folder to import into (e.g. '/Game/UI/Fonts').
            asset_names: Asset name for each file, same order and length as source_files.
            replace_existing: Re-import over existing assets of the same name.
                False fails if any asset already exists.

        Returns:
            A JSON string: {"imported": ["/Game/.../FF_Name.FF_Name", ...]}.
        """
        if not source_files:
            raise ValueError('source_files is empty')

        if len(asset_names) != len(source_files):
            raise ValueError('asset_names must have the same length as source_files')

        if len(set(asset_names)) != len(asset_names):
            raise ValueError('asset_names contain duplicates')

        if not folder_path.startswith('/'):
            raise ValueError(f'folder_path must be a content path such as /Game/UI/Fonts: {folder_path}')

        for path, name in zip(source_files, asset_names):
            if not path.lower().endswith(_FONT_EXTENSIONS):
                raise ValueError(f'Only {_FONT_EXTENSIONS} files are supported: {path}')

            if not os.path.isfile(path):
                raise FileNotFoundError(f'File not found: {path}')

            if not name or '/' in name or '.' in name:
                raise ValueError(f'Invalid asset name: {name!r}')

            asset_path = f'{folder_path}/{name}'

            if not replace_existing and unreal.EditorAssetLibrary.does_asset_exist(asset_path):
                raise RuntimeError(f'Asset already exists: {asset_path} (pass replace_existing=True to re-import)')

        tasks = []

        for path, name in zip(source_files, asset_names):
            task = unreal.AssetImportTask()
            task.set_editor_property('filename', path)
            task.set_editor_property('destination_path', folder_path)
            task.set_editor_property('destination_name', name)
            task.set_editor_property('automated', True)
            task.set_editor_property('replace_existing', replace_existing)
            task.set_editor_property('save', False)
            tasks.append(task)

        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

        imported = []
        failed = []

        for name in asset_names:
            face = unreal.EditorAssetLibrary.load_asset(f'{folder_path}/{name}')

            if isinstance(face, unreal.FontFace):
                imported.append(face.get_path_name())
            else:
                failed.append(name)

        if failed:
            raise RuntimeError(f'Import failed for: {failed} (imported: {imported})')

        return json.dumps({'imported': imported})

    @toolset_registry.tool_call
    @staticmethod
    def create_font(font_asset_path: str, typeface_names: list[str], font_face_paths: list[str]) -> str:
        """Creates (or updates) a runtime-cached Font asset whose default typeface lists the given
        FontFace assets. The asset is not saved.

        UMG text uses it through FontObject + TypefaceFontName (one of typeface_names).
        Running it again with the same input gives the same result; an existing Font at the
        path has its typeface list replaced.

        Args:
            font_asset_path: Content path of the Font asset (e.g. '/Game/UI/Fonts/F_TitleSerif').
            typeface_names: Typeface names (e.g. ['Regular', 'SemiBold']), unique.
            font_face_paths: FontFace asset path for each typeface, same order and length.

        Returns:
            A JSON string: {"asset", "font_cache_type"}.
        """
        if not typeface_names or len(typeface_names) != len(font_face_paths):
            raise ValueError('typeface_names and font_face_paths must be non-empty and the same length')

        faces = []

        for path in font_face_paths:
            face = unreal.EditorAssetLibrary.load_asset(path)

            if not isinstance(face, unreal.FontFace):
                raise ValueError(f'Not a FontFace asset: {path}')

            faces.append(face)

        error = _get_library().create_font_from_faces(font_asset_path, [unreal.Name(n) for n in typeface_names], faces)

        if error:
            raise RuntimeError(error)

        object_path = font_asset_path if '.' in font_asset_path else f'{font_asset_path}.{font_asset_path.rsplit("/", 1)[-1]}'
        font = unreal.load_object(None, object_path)

        if not isinstance(font, unreal.Font):
            raise RuntimeError(f'Font was not found after creation: {object_path}')

        return json.dumps(_describe_font(font))
