# ShaderToy (GLSL) から HLSL (DirectX) への移植ガイド

ShaderToyなどで公開されている素晴らしい2Dシェーダー（GLSL）を、本エンジン（HLSL / 3D環境）に移植する際に発生しやすい「共通のトラブルと対策」をまとめたガイドラインです。

## 1. 構文と組み込み関数の違い
GLSLとHLSLでは、型名や組み込み関数の名前に明確な違いがあります。
移植時はまず以下の単純な置換（リネーム）を行う必要があります。

| GLSL (ShaderToy) | HLSL (DirectX) | 備考 / 注意点 |
| :--- | :--- | :--- |
| `vec2`, `vec3`, `vec4` | `float2`, `float3`, `float4` | |
| `mat2`, `mat3`, `mat4` | `float2x2`, `float3x3`, `float4x4` | |
| `fract()` | `frac()` | |
| `mix()` | `lerp()` | |
| `mod(x, y)` | `fmod(x, y)` **(※要注意)** | ※後述の「modとfmodの挙動の違い」を参照 |
| `texture(sampler, uv)` | `tex.Sample(sampler, uv)` | テクスチャのサンプリング方法 |

### ⚠️ `mod` と `fmod` の計算仕様の違い（非常に重要）
GLSLの `mod()` と HLSLの `fmod()` は、**マイナスの値を与えた時の挙動が異なります**。
ShaderToyのコードで `mod` を使って空間をリピート（繰り返し）させている場合、そのまま `fmod` に置換するとマイナス座標側で絵が反転・崩壊します。

**【対策】**
HLSL側でGLSLと全く同じ挙動をする関数を自作して置き換えるのが最も安全です。
```hlsl
// GLSL互換の安全なmod関数
float mod_glsl(float x, float y) {
    return x - y * floor(x / y);
}
float2 mod_glsl(float2 x, float2 y) {
    return x - y * floor(x / y);
}
```

## 2. 行列のメモリレイアウトと乗算順序
GLSLは **列優先 (Column-Major)**、HLSLは標準で **行優先 (Row-Major)** です。
ShaderToy内で「回転行列（2Dの回転など）」を定義して掛け算している場合、そのまま移植すると「回転方向が逆になる」「形が歪む」といった現象が起きます。

**【対策】**
GLSLで `uv = m * uv;` と書かれている場合、HLSLでは以下のいずれかの対応が必要です。
1. **掛け算の順序を逆にする**: `uv = mul(uv, m);` に変更する。
2. **行列の定義を転置する**: HLSL向けに行列表現の縦横を入れ替えて定義する。
   ```hlsl
   // GLSLでの定義
   // mat2 m = mat2(cos(a), -sin(a), sin(a), cos(a));
   
   // HLSL向けに転置した定義
   float2x2 m = float2x2(cos(a), sin(a), -sin(a), cos(a));
   uv = mul(m, uv);
   ```

## 3. GPU精度とノイズ関数（Hash）のバグ
ShaderToyでは、以下のような `sin` と非常に巨大な数値を掛け合わせる乱数（ハッシュ）関数がよく使われます。
```glsl
// 古典的なGLSLのハッシュ
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}
```
**問題点**: HLSL（特にDirectXのネイティブGPU環境）では、浮動小数点の計算精度や `sin` のオーバーフロー処理の違いにより、この関数が**機能不全を起こし「ノイズが消えて真っ白（または真っ黒）になる」**ことが多々あります。

**【対策】**
三角関数（`sin` / `cos`）を使わない、より堅牢なモダンハッシュ関数（Dave Hoskins 氏の `hash12` など）に差し替えることを強く推奨します。
```hlsl
// 安定性の高いHLSL向けハッシュ関数
float hash12_safe(float2 p) {
    float3 p3  = frac(float3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.x + p3.y) * p3.z);
}
```

## 4. 2D座標 (uv) から 3Dオブジェクトへのマッピング
ShaderToyは「画面という平面 (2Dキャンバス)」に描画するため、座標として `fragCoord.xy / iResolution.xy` などのスクリーン座標が使われます。これを3Dエンジンに持ち込む際、何にマッピングするかで手法を変える必要があります。

- **板ポリゴン (Plane / Billboard) に貼る場合**:
  単純に頂点シェーダーから渡ってきた `texcoord` (UV座標) を中心が `(0,0)` になるように `(texcoord - 0.5) * 2.0` などと変換して渡せばOKです。
- **3Dの球体 (Sphere) に貼る場合**:
  UV座標を使うと極（北極・南極）でテクスチャが歪んでしまいます。これを回避し、常にカメラに向かって綺麗な模様を出すためには、**ビュー空間での法線（`viewNormal.xy`）**を擬似的な2D座標系として渡す手法が非常に有効です。

## 5. 背景合成とブレンドモードの罠
ShaderToyのコードは「背景が黒（0,0,0）」であることを前提に、色を加算（`color += glow`）して最終出力としているケースがほとんどです。
これを3Dエンジンで通常の半透明合成（`SrcAlpha`, `InvSrcAlpha`）で描画すると、エフェクトの周囲に**黒い四角い枠（Dark Halo）**が出現してしまいます。

**【対策】**
エンジン側のブレンドモードを **Premultiplied Alpha（乗算済みアルファ）** に設定し、シェーダー側で以下の計算ルールを守って出力します。
1. **不透明な物質（コアなど）**: その色のまま出力。
2. **加算発光する光（オーラなど）**: 不透明部分には足さず、外側にのみ加算する。
3. **最終出力**: `(不透明色 * 不透明度) + (発光色)` を `RGB` に入れ、アルファには `不透明度` だけを入れる。

```hlsl
// Premultiplied Alpha 向けの出力例
float3 finalColor = (baseColor * baseAlpha) + glowColor;
return float4(finalColor, baseAlpha);
```

---
このガイドラインを参考に移植を行うことで、ShaderToy上の美しいエフェクトを IrufemiEngine の3D空間内で破綻なく再現することが可能になります。
