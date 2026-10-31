# Telemetry Monitor (パフォーマンス監視ツール)

## 概要
Telemetry Monitor は、ゲーム本体（IrufemiEngine）から UDP で送信される JSON フォーマットの監視データを受け取り、リアルタイムに画面に表示するツールです。

最大の特徴として、**完全に独立したC# (WPF) アプリケーション**として動作するため、ゲームのメインループ（FPS）や処理負荷に一切の影響（オーバーヘッド）を与えずにパフォーマンスを確認できます。

## 使い方（起動手順）

チーム開発で共有しやすいよう、このツールは独立した単一の実行ファイル（`.exe`）としてコンパイル済みです。

1. 以下の場所にある `TelemetryMonitor.exe` を直接ダブルクリックして起動します。
   📂 `project/Binaries/TelemetryMonitor/TelemetryMonitor.exe`
2. 続けてゲーム本体（IrufemiEngine）を起動します。
3. 監視ツールの画面上に、エンジンから送られてきた `System/FPS` や `System/GPU_Time_ms` などのデータが自動的に表示され、更新され続けます。

> **※開発者向け（ツールのコードを改修した場合）**
> ツールのコード（`MainWindow.xaml.cs` 等）を変更した場合は、Visual Studio上でビルドするだけでなく、必ず以下のバッチファイルをダブルクリックして配布用の単一 `.exe` を更新してください。
> 
> 📂 `tools/TelemetryMonitor/BuildMonitor.bat`
> 
> ※このバッチファイルは単なる過去ビルドのコピーではありません。**最新のC#ソースコードを自動検知してReleaseモードでコンパイル（ビルド）し、最適化された最新の `.exe` を `Binaries` フォルダへ全自動で出力**する仕組みになっています。

## 新しい監視データを追加する方法

このツールは **JSONのキーを自動解釈して画面にUIを生成** するため、C#側のコードを一切書き換えることなく、ゲーム本体（C++）のコードに1行追加するだけで、どんなデータでも監視できるようになります。

### C++ (IrufemiEngine) 側のコード追加例

```cpp
#include "Engine/Profiler/TelemetrySender.h"

// 数値データを監視する（例：敵の数）
TelemetrySender::GetInstance().SetMetric("Game/ActiveEnemies", 42);

// 文字列データを監視する（例：現在のプレイヤーステート）
TelemetrySender::GetInstance().SetMetric("Player/State", "Attacking");

// FPSなどの小数点データを監視する
TelemetrySender::GetInstance().SetMetric("System/Memory_MB", 256.5f);

// 突発的なイベントを投げる（配列として表示されます）
TelemetrySender::GetInstance().LogEvent("Boss Spawned at X:10 Y:20!");
```

上記のコードを毎フレーム（Update 等で）呼ぶだけで、ツール側に即座に `Game/ActiveEnemies` の項目が増え、リアルタイムに数値が監視できます。

## 技術的な仕様
- **通信方式**: UDP (ポート番号: 8888, ローカルホスト `127.0.0.1`)
- **送信フォーマット**: `nlohmann::json`
- **スレッド設計**: エンジン側は独立した裏スレッドでデータをシリアライズして送信するため、メインスレッドへの影響は皆無です。
