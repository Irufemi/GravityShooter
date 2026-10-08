#pragma once

class TitleMenuControllerComponent;

/**
 * @enum TitleMenuStateType
 * @brief タイトルメニューの動作状態を表すステート列挙体
 */
enum class TitleMenuStateType {
    Idle,         //!< 通常待機（ホバー・ナビゲーション入力受付中）
    OpeningModal, //!< モーダル決定演出中（Click Punch押し込み＆跳ね返り、他ボタンフェード ➔ 完了でPushScene発火）
    Suspended,    //!< モーダル表示中（メニュー非表示＆入力停止）
    Launching     //!< 出撃決定演出中（白光フラッシュ＆ディスミス拡散 ➔ InGame遷移）
};

/**
 * @class ITitleMenuState
 * @brief タイトルメニュー制御のステート基底インターフェース
 * @details
 * State Pattern に基づき、通常待機、モーダル決定演出、出撃演出、背面休眠の
 * ライフサイクルおよび更新処理をカプセル化・ポリモーフィズム化します。
 */
class ITitleMenuState {
public:
    virtual ~ITitleMenuState() = default;

    /**
     * @brief ステート開始時処理
     * @param[in] menu 対象のタイトルメニューコントローラー
     */
    virtual void Enter(TitleMenuControllerComponent* menu) = 0;

    /**
     * @brief 毎フレーム更新処理
     * @param[in] menu 対象のタイトルメニューコントローラー
     * @param[in] dt デルタタイム（秒）
     */
    virtual void Update(TitleMenuControllerComponent* menu, float dt) = 0;

    /**
     * @brief ステート終了時処理
     * @param[in] menu 対象のタイトルメニューコントローラー
     */
    virtual void Exit(TitleMenuControllerComponent* menu) = 0;

    /**
     * @brief 現在のステートタイプを取得する
     * @return 状態タイプ
     */
    virtual TitleMenuStateType GetStateType() const = 0;
};
