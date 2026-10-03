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
      githubUrl: 'https://github.com/Irufemi/GravityShooter/blob/master/project/Application_solo/RailMechanics/RailRelativeFollowerComponent.h',
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

    // プレイヤーのレール進捗にオフセットを加算
    float targetDistance = std::clamp(targetFollower_->GetCurrentDistance() + distanceOffset_,
                                      0.0f, cachedPath_->GetTotalLength());

    // スプライン座標と接線（進行方向ベクトル）を取得
    Vector3 basePos = cachedPath_->GetPointAtDistance(targetDistance);
    Vector3 tangent = cachedPath_->GetTangentAtDistance(targetDistance);

    // 進行方向から姿勢を算出し、レール直交平面のXYローカルオフセットを合成
    float yaw = std::atan2(tangent.x, tangent.z);
    float pitch = std::asin(std::clamp(-tangent.y, -1.0f, 1.0f));
    Matrix4x4 rotMat = Math::MakeRotateXYZMatrix({pitch, yaw, 0.0f});
    Vector3 offsetWorld = Math::TransformNormal(localOffset_, rotMat);

    gameObject_->SetWorldPosition(basePos + offsetWorld);
    gameObject_->SetWorldRotation({pitch, yaw, 0.0f});
}</code></pre>
      `
    },
    'targeting': {
      title: '3Dマルチロックオン＆非同期レイキャスト遮蔽判定（Dynamic BVH連携）',
      subtitle: 'Application_solo/Player/PlayerTargetingComponent.h',
      githubUrl: 'https://github.com/Irufemi/GravityShooter/blob/master/project/Application_solo/Player/PlayerTargetingComponent.h',
      content: `
        <p>画面内の敵をレティクルでホバー検知し、距離・画角スコアリングで複数ターゲットをキュー登録。さらに、自作エンジンの空間分割（<strong>Dynamic BVH</strong>）と連携したレイキャスト遮蔽判定（Line of Sight）を行い、壁裏の敵に対する誤ロックを完全遮断します。</p>
        <h4>技術的特徴</h4>
        <ul>
          <li><strong>非同期レイキャスト (Amortization):</strong> 初回は同期判定で即座に遮蔽を確定。2回目以降はフレームレート低下を防ぐため、<code>ThreadPool</code> を介した <code>RaycastAsync</code> で非同期に視線キャッシュを更新。</li>
          <li><strong>サテライトUI連携:</strong> 同一敵への重複ロックオン数を検知し、マーカー周囲にサテライト状にサブマーカーを展開。</li>
        </ul>
        <h4>主要コードスニペット</h4>
        <pre class="code-snippet"><code>// PlayerTargetingComponent.cpp (抜粋: 非同期遮蔽判定)
if (!cache.hasCheckedOnce) {
    // 初回: 即時レイキャストで壁裏敵の一瞬の透過ロックを完全防止
    RaycastHit hitInfo{};
    bool hit = engine->GetCollisionManager()->Raycast(ray, hitInfo, dist, mask, player);
    cache.canSee = (!hit || hitInfo.hitObject == obj);
    cache.hasCheckedOnce = true;
} else if (currentTime - cache.lastCheckTime > 0.1f && !cache.pendingTask) {
    // 2回目以降: ThreadPoolで非同期分散実行し、メインスレッド負荷をゼロ化
    cache.pendingTask = std::make_shared<std::future<std::pair<bool, RaycastHit>>>(
        engine->GetCollisionManager()->RaycastAsync(engine->GetThreadPool(), ray, dist, mask, player)
    );
}</code></pre>
      `
    },
    'debris-loop': {
      title: 'ガレキ物理循環ゲームループ（最適密度チューニング）',
      subtitle: 'Application_solo/Environment/DebrisManagerComponent.h',
      githubUrl: 'https://github.com/Irufemi/GravityShooter/blob/master/project/Application_solo/Environment/DebrisManagerComponent.h',
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
      githubUrl: 'https://github.com/Irufemi/GravityShooter/blob/master/project/Application_solo/Combat/Boss/BossComponent.h',
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
      githubUrl: 'https://github.com/Irufemi/GravityShooter/blob/master/project/IrufemiEngine/Framework/Component/VirtualEntity/VirtualEntityManagerComponent.h',
      content: `
        <p>ポインタベースのGameObjectを大量生成するとキャッシュミスでCPUが飽和します。本システムでは、連続した密配列（Dense Array）に座標データを保持し、<strong>描画・干渉時のみPromote（実体化）</strong>するハイブリッド設計を採用。</p>
        <h4>定量的成果</h4>
        <ul>
          <li><strong>10,000個ストレステスト:</strong> CPUディスパッチ時間 <strong>0.066ms</strong> を達成。</li>
          <li><strong>本編実運用:</strong> 高い余力（ヘッドルーム）を維持することで、激しいボス戦中も常時完全60FPSを担保。</li>
        </ul>
        <pre class="code-snippet"><code>// VirtualEntityManagerComponent.h (抜粋)
struct VirtualInstance {
    int id;
    Vector3 position;
    Vector3 rotation;
    Vector3 scale;
    bool isPromoted = false;
    bool isDestroyed = false;
    ObjectPool<GameObject>::Handle promotedHandle;
};

std::vector<VirtualInstance> dense_; // 密配列: 連続メモリ配置でCPUキャッシュミスを根絶
std::vector<int> sparse_;             // 疎配列: 仮想IDから密配列インデックスへのO(1)即時逆引き</code></pre>
      `
    },
    'render-graph': {
      title: 'DirectX 12 RenderGraph＆一時リソース再利用',
      subtitle: 'IrufemiEngine/Renderer/Pipeline/RenderGraph/RenderGraph.h',
      githubUrl: 'https://github.com/Irufemi/GravityShooter/blob/master/project/IrufemiEngine/Renderer/Pipeline/RenderGraph/RenderGraph.h',
      content: `
        <p>描画パス（Opaque, Transparent, Shadow, Fog, PostProcess, TopMost, UI）の依存関係を有向非巡回グラフ（DAG）として構築。パス間で寿命の重ならない一時レンダーターゲット（Transient Resource）を自動でエイリアシング（別名再利用）し、<strong>VRAM使用量を劇的に圧縮</strong>します。</p>
      `
    },
    'gpu-voxel': {
      title: 'GPUボクセル破砕物理 ＆ GPUパーティクル（Bitonic Sort）',
      subtitle: 'IrufemiEngine/Renderer/System/VoxelParticle/VoxelParticleSystem.h',
      githubUrl: 'https://github.com/Irufemi/GravityShooter/blob/master/project/IrufemiEngine/Renderer/System/VoxelParticle/VoxelParticleSystem.h',
      content: `
        <p>自作DirectX 12エンジンの看板機能である、Compute Shaderを活用した2つの高度な並列GPU物理パイプラインです。</p>
        <h4>💥 実行時ボクセル破砕物理 (VoxelParticleSystem)</h4>
        <ul>
          <li><strong>メッシュの実行時ボクセル化:</strong> 3Dモデルからリアルタイムに立体キューブ破片群を生成（<code>VoxelizedModel</code>）。</li>
          <li><strong>GPU OBB衝突・分散シミュレーション:</strong> Compute Shaderで数千〜数万のボクセル破片に対し、爆風・重力・地形OBB衝突判定を完全GPU駆動で並列演算。</li>
        </ul>
        <h4>✨ 超大規模半透明ソート (GPUParticleSystem)</h4>
        <ul>
          <li><strong>Bitonic Sort並列深度ソート:</strong> 最大数十万個の半透明パーティクルに対し、<code>BitonicSort.CS.hlsl</code> によりGPU上で並列ソーティングを実行。半透明オブジェクトの深度前後関係破綻（Zファイティング・描画順逆転）を完全解消。</li>
        </ul>
      `
    },
    'post-process': {
      title: 'AAA級ポストプロセスパイプライン（実機運用 ＆ 拡張エフェクト群）',
      subtitle: 'IrufemiEngine/Renderer/PostProcess/PostProcessManager.h',
      githubUrl: 'https://github.com/Irufemi/GravityShooter/blob/master/project/IrufemiEngine/Renderer/PostProcess/PostProcessManager.h',
      content: `
        <p>ゲームのグラフィックス品質を決定づけるポストプロセス基盤。本編のリアルタイム運用（完全60FPS）と、エンジンとしての多彩な拡張性を両立しています。</p>
        <h4>🎮 ゲーム本編（InGame）での常時稼働パイプライン</h4>
        <ul>
          <li><strong>高輝度抽出＋2パス分離Bloom:</strong> 閾値（Threshold）以上の輝度を抽出し、水平（H）/垂直（V）の2パスガウシアンブラーを経て加算合成。レーザーや敵弾・ネオン発光の美しい光の滲みを低負荷で実現。</li>
          <li><strong>ACES Color Grading:</strong> ACES（Academy Color Encoding System）トーンマッピングおよびHSV彩度・露出補正により、黒潰れ・白飛びを防ぎ映画的なコントラストを生成。</li>
          <li><strong>Vignette（周辺減光）:</strong> 画面外周を緩やかに減光し、プレイヤーの視線を画面中央のレティクル・敵機へ自然に集中。</li>
          <li><strong>GlobalPostProcessVolume:</strong> シーン単位で動的にポストプロセスパラメータを制御・シリアライズするVolume Framework設計。</li>
        </ul>
        <h4>⚡ エンジン基盤に搭載された高度な拡張エフェクト群</h4>
        <ul>
          <li><strong>Dual Kawase Blur:</strong> <code>DualKawaseDownsample.PS.hlsl</code> / <code>DualKawaseUpsample.PS.hlsl</code> を用いたピラミッド型ダウン/アップサンプリングにより、極小パスで画面全体への超広範囲ブラーを提供。</li>
          <li><strong>ゴッドレイ（LightShafts）:</strong> 深度バッファからオクルージョン（遮蔽）を判定し、放射状ラジアルブラーによって太陽や高エネルギー光源から漏れる光条を合成。</li>
          <li><strong>演出用シェーダー:</strong> 敵撃破時のディゾルブ（Dissolve）、被弾時のグリッチ（Glitch）、被写界深度（DoF）などを完備。</li>
        </ul>
      `
    },
    'telemetry-monitor': {
      title: 'C# WPF リアルタイム・テレメトリ監視ツール (UDP:8888)',
      subtitle: 'Tools/TelemetryMonitor/MainWindow.xaml.cs',
      githubUrl: 'https://github.com/Irufemi/GravityShooter/blob/master/project/Tools/TelemetryMonitor/MainWindow.xaml.cs',
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
      githubUrl: 'https://github.com/Irufemi/GravityShooter/blob/master/project/IrufemiEditor/Core/EditorManager.h',
      content: `
        <p>ImGuiとDirectX 12を統合した本格的な自作エディタフレームワーク。UnityやUnreal Engineのような快適な制作環境を提供します。</p>
        <h4>主要な搭載機能</h4>
        <ul>
          <li><strong>Commandパターン Undo/Redo:</strong> 全てのオブジェクト移動やパラメータ変更をスタック管理し、Ctrl+Z / Ctrl+Y で完全に巻き戻し可能。</li>
          <li><strong>マルチビュー構成:</strong> 3Dシーンビュー、ヒエラルキーパネル、インスペクター、プロジェクトブラウザ、ログコンソールを統合。</li>
          <li><strong>ゲーム専用インスペクター拡張:</strong> <code>BossComponentEditor</code> や <code>WaveManagerComponentEditor</code> により、他の開発メンバーが直感的にゲームバランスを調整可能。</li>
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
    
    // Header Subtitle + Direct GitHub Link Button
    modalSubtitle.innerHTML = `
      <div class="modal-sub-row">
        <span>${data.subtitle}</span>
        ${data.githubUrl ? `<a href="${data.githubUrl}" target="_blank" class="modal-github-btn">📂 GitHubでコード原本を開く ↗</a>` : ''}
      </div>
    `;

    // Modal Content + Bottom Link Button
    modalBody.innerHTML = `
      ${data.content}
      ${data.githubUrl ? `
        <div class="modal-footer-action">
          <a href="${data.githubUrl}" target="_blank" class="btn btn-secondary">
            📂 GitHubで該当ファイル全体を閲覧する (${data.subtitle.split('/').pop()}) ↗
          </a>
        </div>
      ` : ''}
    `;

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
