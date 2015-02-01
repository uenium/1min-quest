#ifndef __HELLOWORLD_SCENE_H__
#define __HELLOWORLD_SCENE_H__

#include "Config.h"
#include "cocos2d.h"
#include "cocos-ext.h"
#include "sqlite3.h"
#include "InappPurchaseManager.h"
#include "NativeManager.h"
USING_NS_CC;
USING_NS_CC_EXT;
#include "picojson.h"
#include "../network/HttpClient.h"
#include "../network/HttpRequest.h"
#include "../network/HttpResponse.h"

using namespace cocos2d::network;

class HelloWorld : public cocos2d::Layer, public InappPurchaseDelegate, cocos2d::extension::EditBoxDelegate
{
private:
    
    int w;
    int h;
    int x;
    int y;
    int yg; // グローバルメニューの上端座標
    int yt; // 画面上メニューの下端座標
    int hc; // コンテンツ領域の高さ
    
    std::string userName;
    unsigned char userName2[64];
    LabelTTF* userNameLabel;

    int stamina;
    int staminaMax;
    long lastStaminaDecreaseTime;
    LabelTTF* staminaLabel;
    LabelTTF* staminaMaxLabel;
    LabelTTF* staminaLastTimeLabel;
    
    int battleStamina;
    int battleStaminaMax;
    long lastBattleStaminaDecreaseTime;
    LabelTTF* battleStaminaLabel;
    LabelTTF* battleStaminaMaxLabel;
    LabelTTF* battleStaminaLastTimeLabel;
    
    int coin;
    LabelTTF* coinLabel;
    
    int mahouseki;
    int mahouseki_purchase;
    bool storeProductChecking;
    int storeProductCheckedNum;
    
    long lastTwitterPostTime;
    long lastMahousekiGotTime;
    LabelTTF* mahousekiLabel;
    Sprite* mahousekiPresentWarningSprite;
    bool mahousekiPresentWarningFlag;
    
    int visibleDangeonNumber;
    int visitedDangeonNumber;
    
    // チュートリアル
    Sprite* menu100tutorialSprite;
    Sprite* menu500tutorialSprite;
    int presentMenuNum;
    
    // 強化
    int kyoukaNextLevel;
    LabelTTF* kyoukaNextLevelLabel;
    int kyoukaNextClassLevel;
    LabelTTF* kyoukaNextClassLevelLabel;
    int kyoukaNextHp;
    LabelTTF* kyoukaNextHpLabel;
    int kyoukaNextAtk;
    LabelTTF* kyoukaNextAtkLabel;
    int kyoukaNextMgc;
    LabelTTF* kyoukaNextMgcLabel;
    int kyoukaNextPhysicalDef;
    int kyoukaNextMagicalDef;
    int kyoukaNextCoin;
    LabelTTF* kyoukaNextCoinLabel;
    int kyoukaTotalCoin;
    LabelTTF* kyoukaTotalCoinLabel;
    int shinkaTotalMahouseki;
    LabelTTF* shinkaTotalMahousekiLabel;
    
    // スキル売却
    int baikyakuKouho[10];
    int baikyakuCoin;
    
    
    int jinkei;
    int battleRank;
    LabelTTF* rankLabel;
    int battleCount;
    int battleWinCount;
    int battleCountAtCurrentRank;
    int battleWinCountAtCurrentRank;
    
    bool partyOrderChanging;
    bool spesialDangeonRemainingTimeCountingDown;
    int previousSpesialDangeonRemainingTime;
    
    Sprite* worldmap;
    bool globalMenuChanging;
    bool tapProhibit;
    
    // アリーナで別の相手を捜すフラグ
    bool searchingOtherOpponent;
    
    bool playingSE;
    bool playingSE1;
    
    bool arenaPartyMemberEditing;
    bool userNameChanging;
    bool kisyuhenkouCodeEntering;
    char kisyuhenkouCodeEntered[64];
    
    // データ移行
    bool kisyuhenkouCanceling;
    
    
    // 関数
    float setGlobalMenu();
    float setTopMenu();
    void updateTopMenu();
    void checkTopMenu(float dt);
    void moveMap(int menuNum);
    void moveMapFinished();
    
    void globalMenu(int menuNum);
    void setPageTitleAndIn(int menuNum);
    void setPageTitleAndInFinished();
    void setPageTitleAndIn2(int menuNum, int dangeonMasterId);
    int getBackMenuNum(int menuNum);
    int getBackMenuNum2(int menuNum);
    void pageTitleOutAndRemove(int menuNum);
    void pageTitleOutAndRemove2(int menuNum, int dangeonMasterId);
    void pageTitleRemoveAndSetNextPageTitle(int menuNum);
    void pageTitleRemoveAndSetNextPageTitle2(int menuNum, int dangeonMasterId);
    void setMenu(int menuNum);
    void removeMenu();
    void menuTapped(int menuNum);
    
    
    void setMenu100();// 親ダンジョン一覧
    void setMenu110(int parentDangeon);
    void setMenu120_specialDangeonList();
    void setMenu120_specialDangeonListRemainingTimeUpdate(LabelTTF* tmplabel);
    void setMenu110battleStart(int dangeonDetailId);
    
    void setMenu201(bool arena); // パーティー編成
    void setMenu210(int unitId, int sordId, bool arena); // パーティー編成で１キャラ目選択した
    void setMenu210selected(int unitId, int unitId2, bool arena); // パーティー編成で２キャラ目選択した
    void setMenu201OrderChange(int order, int upOrDown, bool arena); // パーティー編成内の並べ替えのみ
    void setMenu202(int sortId); // ユニット一覧
    void setMenu220_unitDetail(int user_unit_id, int hyoujiMenu, bool scrollViewCheck); // ユニット詳細. hyoujiMenu : 0,skill 1,kyouka
    void setMenu220_unitDetail_kyoukaUIin(int user_unit_id, Scale9Sprite* image); // 強化
    void setMenu220_unitDetail_kyoukaUpDownSelected(int user_unit_id, int org_level, int org_class_level, bool upOrDown); // 強化でレベルの上下が選択された
    void setMenu220_unitDetail_kyoukaInfoReWrite(int user_unit_id); // 強化関連情報の表示更新だけ
    void setMenu220_unitDetail_kyoukaInfoReWrite2(int user_unit_id); // 強化関連情報の表示更新だけ
    void setMenu220_unitDetail_kyoukaExec(int user_unit_id, bool shinkaFlag, int target_zokusei, int target_class_type); // 強化実行
    void setMenu220_unitDetail_shinkaUIin(int user_unit_id, int base_zokusei, int base_class_type); // 進化ui
    void setMenu220_unitDetail_shinkaExec(int user_unit_id, int target_zokusei, int target_class_type);
    void setMenu220_unitDetail_skillUIin(int user_unit_id, Scale9Sprite* image, int class_type); // スキル変更
    void setMenu220_unitDetail_infoUIin(int user_unit_id, Scale9Sprite* image); // 情報表示
    
    void setMenu301(); // 魔法石購入
    void setMenu301_selected(int kosuu);
    void setMenu301_execPurchase(int kosuu);
    void setMenu301_mahousekiPreset();
    void setMenu301_mahousekiPresentCalcLastTime(LabelTTF* tmpLabel);
    void setMenu301_mahousekiPresetSelected();
    void setMenu301_mahousekiPresetClancel();
    void setMenu301_mahousekiPresetTwitter();
    void setMenu301_twitterRenkei();
    
    void setMenu320_purchasingWait();
    void setMenu320_purchased(bool purchaseResult, std::string itemId);
    void setMenu302(bool staminaOrButtleStamina, bool warningMessage); // スタミナ回復, バトルスタミナ
    void setMenu302_staminaKaifukuExec(bool staminaOrButtleStamina);
    void setMenu302_cancel();
    
    void setMenu401(int sortId); // スキル一覧
    void setMenu490_skillListFromUnitDetail(int sortId, int user_unit_id, int previousSoubiSkillId); // ユニット詳細から呼ばれるスキル装備変更
    void setMenu490_skillListFromUnitDetail_selected(int user_unit_id, int _id, int previousSoubiSkillId);
    void setMenu402(int sortId, bool reset);
    void setMenu421_skillEvolutionDetail(int _id, int pattern); // スキル進化の詳細画面
    void setMenu421_shinkaExec(int _id);
    void setMenu403(int sortId, bool reset);
    void setMenu431_baikyakuKouhoSelect(int _id, int level, Node* sprite);
    void setMenu432_baikyakuExec();
    
    void setMenu500_BattleInfo();//void HelloWorld::setMenu(int menuNum) の内部から呼ばれる。対戦相手情報を描画する部分のみ。
    void setMenu501_battleStart();
    void setMenu502_searchOtherOpponent();
    
    void setMenu601(); // 最新情報・攻略情報
    void setMenu602(); // 名前変更
    virtual void editBoxEditingDidBegin(cocos2d::extension::EditBox* editBox);
    virtual void editBoxReturn(cocos2d::extension::EditBox* editBox);
    void setMenu602_nameChange(); // 名前変更
    void setMenu603(); // クレジット
    void setMenu604(); // ヘルプ
    void setMenu604_riyoukiyaku();
    void setMenu605(); // 機種変更
    void setMenu605_displayCodeForm();
    void setMenu605_checkEnteredCodeAndCheckServerDataAndConfirm(int pattern);
    void setMenu605_checkEnteredCodeAndCheckServerDataAndConfirmCallback(HttpClient* sender, HttpResponse* response);
    void setMenu605_checkEnteredCodeAndCheckServerDataAndConfirmCallback2(HttpClient* sender, HttpResponse* response);
    void setMenu605_checkEnteredCodeAndCheckServerDataAndConfirmCallback3(HttpClient* sender, HttpResponse* response);
    void setMenu605_uploadData();
    void setMenu605_uploadDataCallback(HttpClient* sender, HttpResponse* response);
    void setMenu605_uploadDataFailed();
    void setMenu605_kisyuhenkoutyuuCheck(int pattern);
    void setMenu605_checkDataCallback(HttpClient* sender, HttpResponse* response);
    void setMenu605_kisyuhenkouCancel();
    void setMenu605_kisyuhenkouCancel_deleteCallback(HttpClient* sender, HttpResponse* response);
    void setMenu605_gotoHelloWorld();
    void setMenu610(); // その他その他
    void setMenu611(); // お勧めアプリ
    
    void makeUserDb();
    void updateUserDataDb();
    bool expendMahouseki(int num);
    
    // 利用規約画面
    void confirmRiyoukiyaku();
    void confirmRiyoukiyakuOK();
    void confirmRiyoukiyakuOK2();
    
    // 強制アップデート
    void forceVersionUP();
    void forceVersionUPOK();
    
    // 購入処理
    virtual void onSuccess(std::string itemId);
    virtual void onCancel();
    void onPurchaseEnded(bool purchaseResult, std::string itemId);
    void checkStoreProducts();
    void onGotStoreProduct(std::string itemId, std::string productDescription, std::string productPrice);
    void checkStoreProductsFailed(float dt);
    
    // 他プレイヤーのパーティー取得
    void searchOtherParties();
    void searchOtherPartiesCallback(HttpClient* sender, HttpResponse* response);
    
    // SEを鳴らす
    void playSE(int se_id);
    void playSE_timerReset(float dt);
    
    void gotoBattleScene();

    void displayOpening();
    void blank();
    
public:
    static cocos2d::Scene* createScene();
    virtual bool init();

    CREATE_FUNC(HelloWorld);
};

#endif // __HELLOWORLD_SCENE_H__
