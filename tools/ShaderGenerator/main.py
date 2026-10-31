from fastapi import FastAPI, UploadFile, Form
from fastapi.responses import PlainTextResponse
import uvicorn
import os
import io
import time
import random
from PIL import Image
from google import genai
from google.genai import types
from pydantic import BaseModel
from dotenv import load_dotenv

# .envファイルから環境変数を読み込む
load_dotenv()

app = FastAPI(title="AI Magic Brush Backend")

class GenerateRequest(BaseModel):
    prompt: str
    image_path: str

@app.post("/generate")
async def generate_shader(request: GenerateRequest):
    """
    C++のTL1Scene（エディタ）から画像パスとプロンプトを受け取り、
    Geminiに送信してHLSLを生成するためのエンドポイント
    """
    try:
        contents_list = []
        
        # 画像が指定されている場合のみ画像を開いてリストに追加
        if request.image_path and request.image_path.strip():
            if not os.path.exists(request.image_path):
                raise Exception(f"Image not found at path: {request.image_path}")
            pil_image = Image.open(request.image_path)
            contents_list.append(pil_image)
        
        # システムプロンプトの読み込み
        system_instruction_text = ""
        prompt_path = os.path.join(os.path.dirname(__file__), "system_prompt.txt")
        if os.path.exists(prompt_path):
            with open(prompt_path, "r", encoding="utf-8") as f:
                system_instruction_text = f.read()
                
        # API呼び出し (GEMINI_API_KEY環境変数が必要)
        api_key = os.getenv("GEMINI_API_KEY")
        if not api_key:
            raise Exception("GEMINI_API_KEY is not set in .env file or environment variables.")
        client = genai.Client(api_key=api_key)
        
        # プロンプトが空の場合はデフォルトテキストを設定
        user_prompt = request.prompt.strip()
        if not user_prompt:
            if not contents_list:
                raise Exception("Prompt and Image Path cannot both be empty.")
            user_prompt = "この画像のビジュアルや色合いの雰囲気を、HLSLシェーダー（PostProcess / Shadertoy的アプローチ）で再現してください。"

        # APIキャッシュ回避だけでなく、AIに「違うものを出せ」と強烈に指示する
        unique_prompt = f"ユーザーからのリクエスト: {user_prompt}\n\n【AIへの追加指示】\n同じリクエストであっても、絶対に前回とは異なるアプローチ（使用する数学関数、色の組み合わせ、空間の歪ませ方など）を採用してください。\nランダムシード [ {time.time()}_{random.randint(0, 10000)} ] を基準にして、今回はどのような斬新なビジュアルにするか全く新しい構成でHLSLコードを記述してください。\nマンネリ化した同じパターンの出力は避けてください。"
        contents_list.append(unique_prompt)
        
        # 自動リトライロジック (最大5回、5秒間隔)
        max_retries = 5
        retry_delay = 5
        response = None
        
        for attempt in range(max_retries):
            try:
                response = client.models.generate_content(
                    model='gemini-2.5-flash',
                    contents=contents_list,
                    config=types.GenerateContentConfig(
                        system_instruction=system_instruction_text,
                        temperature=0.8
                    )
                )
                break # 成功したらループを抜ける
            except Exception as e:
                error_str = str(e)
                if "503" in error_str or "429" in error_str or "UNAVAILABLE" in error_str:
                    if attempt < max_retries - 1:
                        time.sleep(retry_delay)
                        continue
                raise e # リトライ対象外、または最大試行回数に達した場合はエラーを投げる
        
        hlsl_code = response.text
        if "```hlsl" in hlsl_code:
            hlsl_code = hlsl_code.split("```hlsl")[1].split("```")[0].strip()
        elif "```" in hlsl_code:
            hlsl_code = hlsl_code.split("```")[1].split("```")[0].strip()
        
        return PlainTextResponse(content=hlsl_code)

    except Exception as e:
        return PlainTextResponse(status_code=500, content="ERROR: " + str(e))

class FixRequest(BaseModel):
    error_log: str
    code: str

@app.post("/fix_error")
async def fix_error(request: FixRequest):
    """
    C++エンジン側で発生したコンパイルエラーを修復するエンドポイント
    """
    try:
        system_instruction_text = ""
        prompt_path = os.path.join(os.path.dirname(__file__), "system_prompt.txt")
        if os.path.exists(prompt_path):
            with open(prompt_path, "r", encoding="utf-8") as f:
                system_instruction_text = f.read()
                
        # API呼び出し
        api_key = os.getenv("GEMINI_API_KEY")
        if not api_key:
            raise Exception("GEMINI_API_KEY is not set in .env file or environment variables.")
        client = genai.Client(api_key=api_key)
        
        fix_prompt = f"""
以下のHLSLシェーダーコードをコンパイルしたところ、エラーが発生しました。
エラーの内容を元に、コードを修正して完全なHLSLコードだけを出力してください。
【重要】ピクセルシェーダーのエントリーポイント関数名は、必ず `main` にしてください。(`ps_main`等は不可)

【コンパイルエラー】
{request.error_log}

【元のコード】
```hlsl
{request.code}
```
"""
        # 自動リトライロジック (最大3回)
        max_retries = 3
        retry_delay = 3
        response = None
        
        for attempt in range(max_retries):
            try:
                response = client.models.generate_content(
                    model='gemini-2.5-flash',
                    contents=[fix_prompt],
                    config=types.GenerateContentConfig(
                        system_instruction=system_instruction_text,
                        temperature=0.8
                    )
                )
                break
            except Exception as e:
                error_str = str(e)
                if "503" in error_str or "429" in error_str or "UNAVAILABLE" in error_str:
                    if attempt < max_retries - 1:
                        time.sleep(retry_delay)
                        continue
                raise e
        
        hlsl_code = response.text
        if "```hlsl" in hlsl_code:
            hlsl_code = hlsl_code.split("```hlsl")[1].split("```")[0].strip()
        elif "```" in hlsl_code:
            hlsl_code = hlsl_code.split("```")[1].split("```")[0].strip()
            
        return PlainTextResponse(content=hlsl_code)

    except Exception as e:
        return PlainTextResponse(status_code=500, content="ERROR: " + str(e))


class VisualEvaluateRequest(BaseModel):
    reference_image_path: str
    current_output_image_path: str
    code: str

@app.post("/evaluate_visual")
async def evaluate_visual(request: VisualEvaluateRequest):
    """
    C++エンジン側でキャプチャしたスクリーンショットと目標画像を比較し、
    HLSLコードを自己修復（フィードバック）するエンドポイント
    """
    try:
        system_instruction_text = ""
        prompt_path = os.path.join(os.path.dirname(__file__), "system_prompt.txt")
        if os.path.exists(prompt_path):
            with open(prompt_path, "r", encoding="utf-8") as f:
                system_instruction_text = f.read()

        api_key = os.getenv("GEMINI_API_KEY")
        if not api_key:
            raise Exception("GEMINI_API_KEY is not set in .env file or environment variables.")
        client = genai.Client(api_key=api_key)

        contents_list = []

        # 1. ユーザーの参考画像を読み込み（API制限やトークン節約のため 512x512 等に縮小）
        if request.reference_image_path and request.reference_image_path.strip():
            if not os.path.exists(request.reference_image_path):
                raise Exception(f"Reference Image not found at path: {request.reference_image_path}")
            ref_image = Image.open(request.reference_image_path)
            ref_image.thumbnail((512, 512))
            contents_list.append("ユーザーが提示した参考画像（目標とする理想のビジュアル）:")
            contents_list.append(ref_image)

        # 2. 現在の出力スクショ画像を読み込み（同様に縮小）
        if request.current_output_image_path and request.current_output_image_path.strip():
            if not os.path.exists(request.current_output_image_path):
                raise Exception(f"Current Output Image not found at path: {request.current_output_image_path}")
            out_image = Image.open(request.current_output_image_path)
            out_image.thumbnail((512, 512))
            contents_list.append("現在のHLSLが出力した実際の描画結果（スクリーンショット）:")
            contents_list.append(out_image)

        # 3. プロンプトと現在のHLSLコードを追加
        fix_prompt = f"""
あなたは世界トップクラスのテクニカルアーティスト・シェーダープログラマーです。
現在、ユーザーが提示した参考画像（目標）に対して、出力された描画結果（スクリーンショット）の見た目が異なっています。
2つの画像を視覚的に比較・分析し、「色合い」「形状のディテール」「UVスケール」「エフェクトの強さ」など、どこが間違っているのかを考察してください。
その後、現在のHLSLコードを修正して、目標画像により近づけた完全なHLSLコードを出力してください。

【AIへの追加の強力な指示】
ユーザーから「前回と同じようなコードを出力している」と指摘されています。
微調整で済ますのではなく、数式や計算ロジック、定数（マジックナンバー）、ノイズ関数のスケールなどを大胆に変更し、前回とは明らかに異なる出力になるように修正してください。
ランダムシード: [ {time.time()}_{random.randint(0, 10000)} ]

【重要】
- 出力は完全なHLSLコードのみにしてください（```hlsl と ``` で囲むこと）。
- ピクセルシェーダーのエントリーポイント関数名は、必ず `main` にしてください。

【現在のHLSLコード】
```hlsl
{request.code}
```
"""
        contents_list.append(fix_prompt)

        # 自動リトライロジック (最大5回、5秒間隔)
        max_retries = 5
        retry_delay = 5
        response = None

        for attempt in range(max_retries):
            try:
                response = client.models.generate_content(
                    model='gemini-2.5-flash',
                    contents=contents_list,
                    config=types.GenerateContentConfig(
                        system_instruction=system_instruction_text,
                        temperature=0.85
                    )
                )
                break
            except Exception as e:
                error_str = str(e)
                if "503" in error_str or "429" in error_str or "UNAVAILABLE" in error_str:
                    if attempt < max_retries - 1:
                        time.sleep(retry_delay)
                        continue
                raise e

        hlsl_code = response.text
        if "```hlsl" in hlsl_code:
            hlsl_code = hlsl_code.split("```hlsl")[1].split("```")[0].strip()
        elif "```" in hlsl_code:
            hlsl_code = hlsl_code.split("```")[1].split("```")[0].strip()

        return PlainTextResponse(content=hlsl_code)

    except Exception as e:
        return PlainTextResponse(status_code=500, content="ERROR: " + str(e))


if __name__ == "__main__":
    # ローカルホストの8000番ポートでサーバーを起動
    uvicorn.run(app, host="127.0.0.1", port=8000)
