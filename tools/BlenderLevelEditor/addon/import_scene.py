import bpy
import bpy_extras
import math
import json
import bmesh
from mathutils import Euler, Vector

# オペレータ: シーン入力
class MYADDON_OT_import_scene(bpy.types.Operator, bpy_extras.io_utils.ImportHelper):
    bl_idname = "myaddon.myaddon_ot_import_scene"
    bl_label = "シーン入力"
    bl_description = "シーン情報をImportします"
    # 入力するファイルの拡張子
    filename_ext = ".json"

    def create_object(self, json_obj):
        name = json_obj.get("name", "Object")
        obj_type = json_obj.get("type", "EMPTY")
        
        obj = None
        # 名前やタイプから推測してオブジェクトを生成
        if obj_type == "MESH":
            mesh = bpy.data.meshes.new(name)
            bm = bmesh.new()
            if "Cube" in name or "キューブ" in name:
                bmesh.ops.create_cube(bm, size=2.0)
            elif "ICO球" in name or "Icosphere" in name:
                bmesh.ops.create_icosphere(bm, subdivisions=2, radius=1.0)
            else:
                # その他のメッシュはダミーとして立方体を生成
                bmesh.ops.create_cube(bm, size=2.0)
                
            bm.to_mesh(mesh)
            bm.free()
            obj = bpy.data.objects.new(name, mesh)
            bpy.context.collection.objects.link(obj)
            
        elif obj_type == "CAMERA":
            cam_data = bpy.data.cameras.new(name)
            obj = bpy.data.objects.new(name, cam_data)
            bpy.context.collection.objects.link(obj)
            
        elif obj_type == "LIGHT":
            light_data = bpy.data.lights.new(name, type='POINT')
            obj = bpy.data.objects.new(name, light_data)
            bpy.context.collection.objects.link(obj)
            
        else:
            obj = bpy.data.objects.new(name, None)
            obj.empty_display_type = 'ARROWS'
            bpy.context.collection.objects.link(obj)
            
        return obj

    def parse_scene_recursive_json(self, object_list, parent_obj=None):
        for json_obj in object_list:
            # オブジェクト生成
            obj = self.create_object(json_obj)
            
            # 親子関係の設定
            if parent_obj:
                obj.parent = parent_obj
                
            # トランスフォームの復元
            if "transform" in json_obj:
                t = json_obj["transform"]
                if "translation" in t:
                    obj.location = Vector(t["translation"])
                if "rotation" in t:
                    rot_deg = t["rotation"]
                    # 度数法からラジアンへ変換
                    obj.rotation_euler = Euler((math.radians(rot_deg[0]), math.radians(rot_deg[1]), math.radians(rot_deg[2])), 'XYZ')
                if "scaling" in t:
                    obj.scale = Vector(t["scaling"])
            
            # カスタムプロパティの復元
            if "file_name" in json_obj:
                obj["file_name"] = json_obj["file_name"]
            
            if "collider" in json_obj:
                c = json_obj["collider"]
                obj["collider"] = c.get("type", "BOX")
                obj["collider_center"] = c.get("center", [0.0, 0.0, 0.0])
                obj["collider_size"] = c.get("size", [2.0, 2.0, 2.0])
                
            if "disabled" in json_obj:
                obj["disabled"] = json_obj["disabled"]
                
            # 子オブジェクトの再帰的パース
            if "children" in json_obj:
                self.parse_scene_recursive_json(json_obj["children"], obj)

    def execute(self, context):
        print("シーン情報入力開始... %r" % self.filepath)
        
        # 独立したシーン全消去オペレータを呼び出す
        bpy.ops.myaddon.myaddon_ot_clear_scene()
        
        with open(self.filepath, "rt", encoding="utf-8") as file:
            json_text = file.read()
            json_object_root = json.loads(json_text)
            
            if "objects" in json_object_root:
                self.parse_scene_recursive_json(json_object_root["objects"])
                
        print("シーン情報をImportしました")
        self.report({'INFO'}, "シーン情報をImportしました")
        return {'FINISHED'}
