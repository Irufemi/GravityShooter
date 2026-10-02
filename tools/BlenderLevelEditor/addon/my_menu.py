import bpy
from .stretch_vertex import MYADDON_OT_stretch_vertex
from .create_ico_sphere import MYADDON_OT_create_ico_sphere
from .export_scene import MYADDON_OT_export_scene
from .import_scene import MYADDON_OT_import_scene
from .clear_scene import MYADDON_OT_clear_scene
from .spawn import MYADDON_OT_spawn_create_player_symbol, MYADDON_OT_spawn_create_enemy_symbol

#トップバーの拡張メニュー
class TOPBAR_MT_my_menu(bpy.types.Menu):
    bl_idname = "TOPBAR_MT_my_menu"
    bl_label = "MyMenu"
    bl_description = "拡張メニュー"

    # サブメニューの描画
    def draw(self, context):
        self.layout.operator(MYADDON_OT_stretch_vertex.bl_idname, text=MYADDON_OT_stretch_vertex.bl_label)
        self.layout.operator(MYADDON_OT_create_ico_sphere.bl_idname, text=MYADDON_OT_create_ico_sphere.bl_label)
        self.layout.operator(MYADDON_OT_export_scene.bl_idname, text=MYADDON_OT_export_scene.bl_label)
        self.layout.operator(MYADDON_OT_import_scene.bl_idname, text=MYADDON_OT_import_scene.bl_label)
        self.layout.operator(MYADDON_OT_clear_scene.bl_idname, text=MYADDON_OT_clear_scene.bl_label)
        self.layout.operator(MYADDON_OT_spawn_create_player_symbol.bl_idname, text=MYADDON_OT_spawn_create_player_symbol.bl_label)
        self.layout.operator(MYADDON_OT_spawn_create_enemy_symbol.bl_idname, text=MYADDON_OT_spawn_create_enemy_symbol.bl_label)

    # 既存のメニューにサブメニューを追加
    def submenu(self, context):
        self.layout.menu(TOPBAR_MT_my_menu.bl_idname)
