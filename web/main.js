/**
 * Gravity Shooter - Portfolio Main Script
 * Handles modal popups, smooth scrolling, and dynamic interactions
 */

document.addEventListener('DOMContentLoaded', () => {
  // Modal Data Dictionary
  const modalData = {
    'rail-mechanics': {
      title: 'スプライン曲線＆レール座標系 相対追従ドッグファイト',
      subtitle: 'Application_solo/RailMechanics/RailRelativeFollowerComponent.h',
      content: `
        <p>単に見えないレール上を前進するだけでなく、<strong>「ボスがプレイヤーの前方を一定距離保ちつつ、レール座標系基準のローカルオフセットで左右上下に逃走・旋回する」</strong>という本格的な3Dレールシューターのチェイス戦闘を実現しています。</p>
        <h4>設計のポイント</h4>
        <ul>
          <li><strong>座標系の完全分離:</strong> スプライン進行（Z深度）と、プレイヤー・ボスの自立回避（XY平面）を直交する座標系として分離することで、地形貫通バグを物理的に防止。</li>
          <li><strong>動的ドッグファイト:</strong> プレイヤーが右へロール回避すると、ボスもそれに呼応して姿勢制御（バンキング）を行いながら旋回。</li>
        </ul>
        <h4>主要コードスニペット</h4>
        <pre class="code-snippet"><code>// RailRelativeFollowerComponent.cpp (抜粋)
void RailRelativeFollowerComponent::Update() {
    if (!targetFollower_ || !cachedPath_) return;

    // プレイヤーのレール進行進捗に距離オフセットを加算
    float targetDistance = targetFollower_->GetCurrentDistance() + distanceOffset_;
    
    // スプライン上のワールドTransformをサンプリング
    Transform railTransform = cachedPath_->SampleTransformAtDistance(targetDistance);
    
    // レール基準のローカル回転・オフセットを合成して姿勢を決定
    Vector3 finalPos = railTransform.position + railTransform.rotation * localOffset_;
    GetOwner()->GetTransform()->SetPosition(finalPos);
}</code></pre>
      `
    },
    'targeting': {
      title: '3Dマルチロックオン＆レイキャスト遮蔽判定',
      subtitle: 'Application_solo/Player/PlayerTargetingComponent.h',
      content: `
        <p>画面内の敵をレティクルでホバー検知し、距離・画角に基づきスコアリングして複数ターゲットをキューに登録。さらに、<strong>レイキャストによる障害物の遮蔽判定（Line of Sight）</strong>を行い、壁裏の敵に対する誤ロックを完全に遮断します。</p>
        <h4>技術的特徴</h4>
        <ul>
          <li><strong>サテライトUI連携:</strong> 同一の敵に対する重複ロックオン数を検知し、マーカー周囲にサテライト状にサブマーカーを展開。</li>
          <li><strong>ゼロアロケーション設計:</strong> 毎フレームの動的メモリ確保（new/malloc）を徹底排除し、事前割り当てキューで高速処理。</li>
        </ul>
        <h4>主要コードスニペット</h4>
        <pre class="code-snippet"><code>// PlayerTargetingComponent.cpp (抜粋)
void PlayerTargetingComponent::MarkTarget(size_t maxLockOn) {
    if (queuedTargets_.size() >= maxLockOn) return;
    
    // ホバー中の対象が遮蔽されていないかRaycastで最終確認
    if (hoverTarget_ && !IsOccluded(hoverTarget_->GetTransform()->GetWorldPosition())) {
        queuedTargets_.push_back(hoverTarget_);
        lockonUI_->AddSatelliteMarker(hoverTarget_);
    }
}</code></pre>
      `
    },
    'debris-loop': {
      title: 'ガレキ物理循環ゲームループ（最適密度チューニング）',
      subtitle: 'Application_solo/Environment/DebrisManagerComponent.h',
      content: `
        <p>最大10,000個の生成に耐えうるアーキテクチャを持ちながら、<strong>「実際のゲームプレイでは画面の視認性と弾幕認識を最優先」</strong>し、あえて数十〜100個程度のガレキが高密度に循環するよう調整しています。</p>
        <h4>循環サイクルの4フェーズ</h4>
        <ol>
          <li><strong>引き寄せ (Pull):</strong> 周囲のガレキを重力で自機周辺へ吸着（Fake PhysicsによりCPU負荷ゼロ）。</li>
          <li><strong>旋回防御 (Orbit):</strong> 自機周囲を回転し、敵の弾幕を相殺するシールドとして機能。</li>
          <li><strong>射出 (Fire):</strong> ロックオン対象へ時間差で一斉掃射。着弾時に大爆発。</li>
          <li><strong>ドロップ＆回収 (Drop):</strong> 敵撃破時にスケールに応じた破片がバースト散乱し、次の攻撃リソースへ瞬時に還元。</li>
        </ol>
      `
    },
    'multi-boss': {
      title: 'ステートマシン式マルチフェーズボスバトル',
      subtitle: 'Application_solo/Combat/Boss/BossComponent.h',
      content: `
        <p>Stateパターンを用いて実装された本格ボス戦。外装装甲の破壊によって内部の弱点コアが露出し、より激化する弾幕フェーズへとドラマチックに移行します。</p>
        <h4>フェーズ遷移構造</h4>
        <ul>
          <li><strong>Phase 1 [BossStateIdle]:</strong> 重厚な外装シールドを旋回させ、ドローンと通常弾幕で迎撃。</li>
          <li><strong>Phase 2 [BossStateCoreExposed]:</strong> プレイヤーが一斉掃射で装甲を剥がすと弱点コアが露出。被弾シェーダー（Damage Visualizer）が激しく点滅し、強力なビーム・広範囲AOE攻撃を発動。</li>
          <li><strong>Phase 3 [BossStateDestroyed]:</strong> 撃破シーケンスへ移行し、スローモーション演出とボクセル爆散物理で壮絶に崩壊。</li>
        </ul>
      `
    },
    'virtual-entity': {
      title: 'Virtual Entity & Sparse Set キャッシュ最適化',
      subtitle: 'IrufemiEngine/Framework/Component/VirtualEntity/VirtualEntityManagerComponent.h',
      content: `
        <p>ポインタベースのGameObjectを大量生成するとキャッシュミスでCPUが飽和します。本システムでは、連続した密配列（Dense Array）に座標データを保持し、<strong>描画・干渉時のみPromote（実体化）</strong>するハイブリッド設計を採用。</p>
        <h4>定量的成果</h4>
        <ul>
          <li><strong>10,000個ストレステスト:</strong> CPUディスパッチ時間 <strong>0.066ms</strong> を達成。</li>
          <li><strong>本編実運用:</strong> 高い余力（ヘッドルーム）を維持することで、激しいボス戦中も常時完全60FPSを担保。</li>
        </ul>
        <pre class="code-snippet"><code>// VirtualEntityManagerComponent.h (データ構造イメージ)
struct VirtualEntityData {
    Vector3 position;
    Vector3 velocity;
    uint32_t stateFlags;
};
std::vector<VirtualEntityData> denseEntities_; // 密配列でCPUキャッシュヒット率極大</code></pre>
      `
    },
    'render-graph': {
      title: 'DirectX 12 RenderGraph＆一時リソース再利用',
      subtitle: 'IrufemiEngine/Renderer/Pipeline/RenderGraph/RenderGraph.h',
      content: `
        <p>描画パス（Opaque, Transparent, Shadow, Fog, PostProcess, TopMost, UI）の依存関係を有向非巡回グラフ（DAG）として構築。パス間で寿命の重ならない一時レンダーターゲット（Transient Resource）を自動でエイリアシング（別名再利用）し、<strong>VRAM使用量を劇的に圧縮</strong>します。</p>
      `
    },
    'gpu-voxel': {
      title: 'GPUボクセル破壊パーティクル ＆ バイソニックソート',
      subtitle: 'IrufemiEngine/Renderer/System/VoxelParticle/VoxelParticleSystem.h',
      content: `
        <p>3Dメッシュモデルを実行時にボクセル化（VoxelizedModel）し、Compute Shaderで数千〜数万のキューブ破片を完全GPU物理シミュレーション（OBB衝突・重力・分散）。半透明破片は <code>BitonicSort.CS.hlsl</code> によりGPU上で並列ソートされ、描画の破綻を防ぎます。</p>
      `
    },
    'post-process': {
      title: 'AAA級ポストプロセス（Dual Kawase Bloom & ゴッドレイ）',
      subtitle: 'IrufemiEngine/Renderer/PostProcess/PostProcessManager.h',
      content: `
        <p>モバイルおよび最新コンソールゲームで採用される <strong>Dual Kawase Blur</strong> を実装。従来のGaussian Blurに比べて極めて少ないパス数で滑らかかつ広範囲の発光（青白いエネルギーグロー）を実現。さらにラジアルブラーと輝度抽出を組み合わせたボリュメトリック・ゴッドレイ（光条）を合成しています。</p>
      `
    },
    'telemetry-monitor': {
      title: 'C# WPF リアルタイム・テレメトリ監視ツール (UDP:8888)',
      subtitle: 'Tools/TelemetryMonitor & IrufemiEngine/Core/Profiler/TelemetrySender.h',
      content: `
        <p>C# / WPF (.NET 10) で開発された外部プロファイリングGUIツール。ゲームエンジン側のGPUパーティクル数、CPUディスパッチ時間、FPSをリアルタイムにグラフ監視します。</p>
        <h4>なぜTCPではなく「UDP」なのか？</h4>
        <ul>
          <li><strong>ゲームループ無負荷設計:</strong> プロファイリング自体がゲームの動作を重くしては本末転倒。TCPの再送待ちによるブロッキングを避け、<strong>低オーバーヘッドなUDP通信</strong>を採用。</li>
          <li><strong>独立ワーカースレッド送信:</strong> C++エンジン側はメインループを一切止めず、専用スレッド（<code>std::thread</code> + <code>std::condition_variable</code>）からバックグラウンドで非同期送信。</li>
        </ul>
        <pre class="code-snippet"><code>// TelemetrySender.h (抜粋)
void Initialize(const std::string& targetIp = "127.0.0.1", uint16_t targetPort = 8888);
void SetMetric(const std::string& key, const nlohmann::json& value);
// 毎フレーム末尾にバックグラウンドスレッドへデータをキック</code></pre>
      `
    },
    'irufemi-editor': {
      title: 'インハウス・ゲームエンジンエディタ (IrufemiEditor)',
      subtitle: 'IrufemiEditor/Core/EditorManager.h',
      content: `
        <p>ImGuiとDirectX 12を統合した本格的な自作エディタフレームワーク。UnityやUnreal Engineのような快適な制作環境を提供します。</p>
        <h4>主要な搭載機能</h4>
        <ul>
          <li><strong>Commandパターン Undo/Redo:</strong> 全てのオブジェクト移動やパラメータ変更をスタック管理し、Ctrl+Z / Ctrl+Y で完全に巻き戻し可能。</li>
          <li><strong>マルチビュー構成:</strong> 3Dシーンビュー、ヒエラルキーパネル、インスペクター、プロジェクトブラウザ、ログコンソールを統合。</li>
          <li><strong>ゲーム専用インスペクター拡張:</strong> <code>BossComponentEditor</code> や <code>WaveManagerComponentEditor</code> により、プランナー・デザイナーが直感的にゲームバランスを調整可能。</li>
        </ul>
      `
    }
  };

  // Modal Event Listeners
  const modal = document.getElementById('infoModal');
  const modalTitle = document.getElementById('modalTitle');
  const modalSubtitle = document.getElementById('modalSubtitle');
  const modalBody = document.getElementById('modalBody');
  const modalClose = document.getElementById('modalClose');

  function openModal(key) {
    const data = modalData[key];
    if (!data) return;

    modalTitle.textContent = data.title;
    modalSubtitle.textContent = data.subtitle;
    modalBody.innerHTML = data.content;

    modal.classList.add('active');
    document.body.style.overflow = 'hidden';
  }

  function closeModal() {
    modal.classList.remove('active');
    document.body.style.overflow = '';
  }

  document.querySelectorAll('[data-modal]').forEach(card => {
    card.addEventListener('click', () => {
      const key = card.getAttribute('data-modal');
      openModal(key);
    });
  });

  if (modalClose) {
    modalClose.addEventListener('click', closeModal);
  }

  window.addEventListener('click', (e) => {
    if (e.target === modal) {
      closeModal();
    }
  });

  window.addEventListener('keydown', (e) => {
    if (e.key === 'Escape' && modal.classList.contains('active')) {
      closeModal();
    }
  });

  // Active Link on Scroll
  const navLinks = document.querySelectorAll('.nav-links a');
  const sections = document.querySelectorAll('section');

  window.addEventListener('scroll', () => {
    let current = '';
    sections.forEach(section => {
      const sectionTop = section.offsetTop - 100;
      if (window.pageYOffset >= sectionTop) {
        current = section.getAttribute('id');
      }
    });

    navLinks.forEach(link => {
      link.classList.remove('active');
      if (link.getAttribute('href') === `#${current}`) {
        link.classList.add('active');
      }
    });
  });
});
