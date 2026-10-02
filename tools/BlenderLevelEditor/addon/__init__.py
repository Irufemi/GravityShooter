import bpy

# アドオン情報
bl_info = {
    "name": "LevelEditor",
    "author": "Koichi Suehiro",
    "version": (1, 0),
    "blender": (3, 3, 1),
    "location": "",
    "description": "LevelEditor",
    "warning": "",
    "wiki_url": "",
    "tracker_url": "",
    "category": "Object"
}

from .stretch_vertex import MYADDON_OT_stretch_vertex
from .create_ico_sphere import MYADDON_OT_create_ico_sphere
from .add_filename import MYADDON_OT_add_filename
from .file_name import OBJECT_PT_file_name
from .add_collider import MYADDON_OT_add_collider
from .collider import OBJECT_PT_collider
from .export_scene import MYADDON_OT_export_scene
from .my_menu import TOPBAR_MT_my_menu
from .draw_collider import DrawCollider
from .disabled import MYADDON_OT_add_disabled, OBJECT_PT_disabled
from .import_scene import MYADDON_OT_import_scene
from .clear_scene import MYADDON_OT_clear_scene
from .spawn import MYADDON_OT_spawn_import_symbol, MYADDON_OT_spawn_create_symbol, MYADDON_OT_spawn_create_player_symbol, MYADDON_OT_spawn_create_enemy_symbol

# Blenderに登録するクラスリスト

classes = (
    MYADDON_OT_stretch_vertex,
    MYADDON_OT_create_ico_sphere,
    MYADDON_OT_export_scene,
    TOPBAR_MT_my_menu,
    MYADDON_OT_add_filename,
    OBJECT_PT_file_name,
    MYADDON_OT_add_collider,
    OBJECT_PT_collider,
    MYADDON_OT_add_disabled,
    OBJECT_PT_disabled,
    MYADDON_OT_import_scene,
    MYADDON_OT_clear_scene,
    MYADDON_OT_spawn_import_symbol,
    MYADDON_OT_spawn_create_symbol,
    MYADDON_OT_spawn_create_player_symbol,
    MYADDON_OT_spawn_create_enemy_symbol,
)


#Add-On有効化時コールバック
def register():
    # Blenderにクラスを登録
    for cls in classes:
        bpy.utils.register_class(cls)

    #メニューに項目を追加
    bpy.types.TOPBAR_MT_editor_menus.append(TOPBAR_MT_my_menu.submenu)
    #3Dビューに描画関数を追加
    DrawCollider.handle = bpy.types.SpaceView3D.draw_handler_add(DrawCollider.draw_collider, (), "WINDOW", "POST_VIEW")
    print("レベルエディタが有効化されました。")
    
#Add-On無効化時コールバック
def unregister():
    #メニューから項目を削除
    bpy.types.TOPBAR_MT_editor_menus.remove(TOPBAR_MT_my_menu.submenu)
    #3Dビューから描画関数を削除
    bpy.types.SpaceView3D.draw_handler_remove(DrawCollider.handle, "WINDOW")

    # Blenderからクラスを削除
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
    print("レベルエディタが無効化されました。")
