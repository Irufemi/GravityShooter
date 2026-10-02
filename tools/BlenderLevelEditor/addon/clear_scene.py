import bpy

# オペレータ: シーンの全消去
class MYADDON_OT_clear_scene(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_clear_scene"
    bl_label = "シーンの全消去"
    bl_description = "現在のシーンにあるすべてのオブジェクトを削除します"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        # 現在のシーンにあるすべてのオブジェクトを削除する
        for obj in context.scene.objects:
            bpy.data.objects.remove(obj, do_unlink=True)
            
        self.report({'INFO'}, "シーンの全オブジェクトを削除しました")
        return {'FINISHED'}
