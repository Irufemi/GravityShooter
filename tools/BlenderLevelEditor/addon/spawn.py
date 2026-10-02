import bpy
import os

class SpawnNames():
    # インデックス
    PROTOTYPE = 0  # プロトタイプのオブジェクト名
    INSTANCE = 1   # 量産時のオブジェクト名
    FILENAME = 2   # リソースファイル名

    names = {}
    names["Enemy"] = ("PrototypeEnemySpawn", "EnemySpawn", "enemy/enemy.obj")
    names["Player"] = ("PrototypePlayerSpawn", "PlayerSpawn", "player/player.obj")

class MYADDON_OT_spawn_import_symbol(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_spawn_import_symbol"
    bl_label = "出現ポイントシンボルのImport"
    bl_description = "出現ポイントのシンボルをImportします"
    
    def load_obj(self, type):
        print("出現ポイントのシンボルをImportします")
        # 重複ロード防止
        spawn_object = bpy.data.objects.get(SpawnNames.names[type][SpawnNames.PROTOTYPE])
        if spawn_object is not None:
            return {'CANCELLED'}
        
        # モデルのパスを構築
        filename = SpawnNames.names[type][SpawnNames.FILENAME]
        filepath = os.path.join(os.path.dirname(__file__), filename)
        if not os.path.exists(filepath):
            self.report({'ERROR'}, f"ファイルが見つかりません: {filepath}")
            return {'CANCELLED'}
        
        # モデルをインポート
        bpy.ops.wm.obj_import('EXEC_DEFAULT', filepath=filepath, forward_axis='Z', up_axis='Y')
        obj = bpy.context.active_object
        bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
        
        # 名前とカスタムプロパティを設定
        obj.name = SpawnNames.names[type][SpawnNames.PROTOTYPE]
        obj["type"] = SpawnNames.names[type][SpawnNames.INSTANCE]
        
        # 現在のコレクション（シーン）から除外してプロトタイプ化
        bpy.context.collection.objects.unlink(obj)
        
        return {'FINISHED'}

    def execute(self, context):
        # Enemyオブジェクト読み込み
        self.load_obj("Enemy")
        # Playerオブジェクト読み込み
        self.load_obj("Player")
        
        return {'FINISHED'}

class MYADDON_OT_spawn_create_symbol(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_spawn_create_symbol"
    bl_label = "出現ポイントシンボルの作成"
    bl_description = "出現ポイントのシンボルを作成します"
    
    type: bpy.props.StringProperty(name="Type", default="Player")
    
    def execute(self, context):
        print("出現ポイントのシンボルを作成します")
        
        # プロトタイプがまだない場合は自動でインポートする
        spawn_object = bpy.data.objects.get(SpawnNames.names[self.type][SpawnNames.PROTOTYPE])
        if spawn_object is None:
            bpy.ops.myaddon.myaddon_ot_spawn_import_symbol('EXEC_DEFAULT')
            spawn_object = bpy.data.objects.get(SpawnNames.names[self.type][SpawnNames.PROTOTYPE])
            
        if not spawn_object:
            self.report({'ERROR'}, "シンボルのプロトタイプが見つかりません")
            return {'CANCELLED'}
            
        bpy.ops.object.select_all(action='DESELECT')
        
        new_obj = spawn_object.copy()
        bpy.context.collection.objects.link(new_obj)
        
        new_obj.name = SpawnNames.names[self.type][SpawnNames.INSTANCE]
        
        bpy.context.view_layer.objects.active = new_obj
        new_obj.select_set(True)
        
        return {'FINISHED'}

class MYADDON_OT_spawn_create_player_symbol(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_spawn_create_player_symbol"
    bl_label = "プレイヤー出現ポイントシンボルの作成"
    bl_description = "プレイヤー出現ポイントのシンボルを作成します"
    
    def execute(self, context):
        bpy.ops.myaddon.myaddon_ot_spawn_create_symbol('EXEC_DEFAULT', type="Player")
        return {'FINISHED'}

class MYADDON_OT_spawn_create_enemy_symbol(bpy.types.Operator):
    bl_idname = "myaddon.myaddon_ot_spawn_create_enemy_symbol"
    bl_label = "敵出現ポイントシンボルの作成"
    bl_description = "敵出現ポイントのシンボルを作成します"
    
    def execute(self, context):
        bpy.ops.myaddon.myaddon_ot_spawn_create_symbol('EXEC_DEFAULT', type="Enemy")
        return {'FINISHED'}
