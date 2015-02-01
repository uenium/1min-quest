#include <stdio.h>
#include <time.h>
#include "HelloWorldScene.h"

#include "BattleScene.h"
#include "TipsScene.h"
#include "LocalizedString.h"
#include "SceneManager.h"
#include "SimpleAudioEngine.h"

#include "InappPurchaseManager.h"
#include "NativeManager.h"


Scene* HelloWorld::createScene()
{
    auto scene = Scene::create();
    auto layer = HelloWorld::create();
    scene->addChild(layer);
    return scene;
}

// on "init" you need to initialize your instance
bool HelloWorld::init()
{
    if ( !Layer::init() )
    {
        return false;
    }
    
    Size visibleSize = Director::getInstance()->getVisibleSize();
    Point origin = Director::getInstance()->getVisibleOrigin();
    
    w = visibleSize.width;
    h = visibleSize.height;
    x = origin.x;
    y = origin.y;
    
    srand(time(NULL));
    
    globalMenuChanging = false;
    tapProhibit = false;
    playingSE = false;
    playingSE1 = false;
    storeProductChecking = false;
    arenaPartyMemberEditing = false;
    
    kisyuhenkouCanceling = false;
    
    // editBoxのCallback内で区別するため
    userNameChanging = false;
    kisyuhenkouCodeEntering = false;
    
    spesialDangeonRemainingTimeCountingDown = false;
        
    // game DB を writable pathにコピー
    std::string tmpPath = FileUtils::getInstance()->getWritablePath();
    tmpPath += "gamedata.db";
    long tmpSize;
    const char* tmpDbData = (char*)FileUtils::getInstance()->getFileData("gamedata.db", "rb", &tmpSize);
    FILE* fp = fopen(tmpPath.c_str(), "wb");
    if(fp != NULL){
        fwrite(tmpDbData, tmpSize, 1, fp);
        CC_SAFE_DELETE_ARRAY(tmpDbData);
    }
    fclose(fp);
    

    
    std::string fullpath = FileUtils::getInstance()->getWritablePath();
    fullpath += "userdata.db";
    
    // 初期データベースがなければ利用規約画面へ(利用規約ok押下後にmakeUserDb()する)
    if(!(FileUtils::getInstance()->isFileExist(fullpath.c_str()))){
        displayOpening();
        return true;
    }
    
    // user_dataをデータベースから引っ張る
    sqlite3* db = NULL;
    
    char userNameTmp[64];
    char kisyuhenkou[64];
    sprintf(kisyuhenkou, "");
    
    if (sqlite3_open(fullpath.c_str(), &db) == SQLITE_OK) {
        sqlite3_stmt *stmt = NULL;
        std::string sql = "select * from user_data";
        if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
            sqlite3_step(stmt);
            sprintf(userNameTmp, "%s", sqlite3_column_text(stmt, 1));
            userName = userNameTmp;
            stamina                         = (int)sqlite3_column_int(stmt, 2);
            staminaMax                      = (int)sqlite3_column_int(stmt, 3);
            lastStaminaDecreaseTime         = (int)sqlite3_column_int(stmt, 4);
            if(lastStaminaDecreaseTime == 0){ lastStaminaDecreaseTime = time(NULL); }
            battleStamina                   = (int)sqlite3_column_int(stmt, 5);
            battleStaminaMax                = (int)sqlite3_column_int(stmt, 6);
            lastBattleStaminaDecreaseTime   = (int)sqlite3_column_int(stmt, 7);
            if(lastBattleStaminaDecreaseTime == 0){ lastBattleStaminaDecreaseTime = time(NULL); }
            coin                            = (int)sqlite3_column_int(stmt, 8);
            mahouseki                       = (int)sqlite3_column_int(stmt, 9);
            lastMahousekiGotTime            = (int)sqlite3_column_int(stmt, 10);
            visibleDangeonNumber            = (int)sqlite3_column_int(stmt, 11);
            visitedDangeonNumber            = (int)sqlite3_column_int(stmt, 12);
            jinkei                          = (int)sqlite3_column_int(stmt, 13);
            battleRank                      = (int)sqlite3_column_int(stmt, 14);
            battleCount                     = (int)sqlite3_column_int(stmt, 15);
            battleWinCount                  = (int)sqlite3_column_int(stmt, 16);
            battleCountAtCurrentRank        = (int)sqlite3_column_int(stmt, 17);
            battleWinCountAtCurrentRank     = (int)sqlite3_column_int(stmt, 18);
            mahouseki_purchase              = (int)sqlite3_column_int(stmt, 19);
            lastTwitterPostTime             = (int)sqlite3_column_int(stmt, 20);
            if(lastTwitterPostTime == 0){ lastTwitterPostTime = time(NULL); }
            sprintf(kisyuhenkou, "%s", sqlite3_column_text(stmt, 21));

        }
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    
    // 機種変更中の場合、機種変更中画面へ
    if(strlen(kisyuhenkou) == 8){
        setMenu605_kisyuhenkoutyuuCheck(0);
        return true;
    }
    
    // グローバルメニュー描画
    yg = setGlobalMenu();
    
    // トップメニュー描画
    setTopMenu();
    hc = h - yg - yt;
    
    // ワールドマップ描画
    worldmap = Sprite::create("worldmap.jpg");
    worldmap->setScale(h*0.9 / worldmap->getContentSize().height);
    worldmap->setAnchorPoint(Point(0, 0.5));
    worldmap->setPosition(Point(0, h * 0.5));
    this->addChild(worldmap, 1, 1);
    
    // BGM再生
    CocosDenshion::SimpleAudioEngine::getInstance()->setBackgroundMusicVolume(0.5);
    CocosDenshion::SimpleAudioEngine::getInstance()->setEffectsVolume(0.5);
    CocosDenshion::SimpleAudioEngine::getInstance()->stopBackgroundMusic();
    CocosDenshion::SimpleAudioEngine::getInstance()->playBackgroundMusic("bgm/bgm_main.mp3", true);
    
    // ダンジョンリスト表示
    setPageTitleAndIn(100);
    setMenu(100);
    moveMap(100);
    
    // 体力回復のためのタイマー
    checkTopMenu(0.0);
    this->schedule(schedule_selector(HelloWorld::checkTopMenu), 1.0);
    
    // 他プレイヤーのパーティー情報を取得
    searchOtherParties();
    
    return true;
}

//////////////////////////////////////////////////
//////////////////////////////////////////////////
//////////////////////////////////////////////////
//  グローバルメニューと画面上部を描画
//////////////////////////////////////////////////
//////////////////////////////////////////////////
//////////////////////////////////////////////////

float HelloWorld::setGlobalMenu()
{
    
    for(int i = 0; i < GLOBAL_MENU_NUM; i++){
        Scale9Sprite* image = Scale9Sprite::create("window2.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
        image->setContentSize(Size((float) 1.03 * w / GLOBAL_MENU_NUM , (float)w / GLOBAL_MENU_NUM));
        Size tmpSize = image->getContentSize();
        image->setAnchorPoint(Point::ZERO);
        image->setPosition(Point::ZERO);
        
        char tmpChar[64];
        sprintf(tmpChar, "pageTitle%d", (i+1)*100);
        float tmpLabelSize = 1.0;
        if(i == 0){ tmpLabelSize = 0.9; }
        auto label = LabelTTF::create(LocalizedString::getLocalizedString(tmpChar, tmpChar), FONT, LEVEL_FONT_SIZE * tmpLabelSize);
        label->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.5));
        image->addChild(label);
        
        
        if(i == 2){ // 魔法石がもらえる！！マーク
            mahousekiPresentWarningSprite = Sprite::create("whitecircle.png");
            Size tmpSize2 = mahousekiPresentWarningSprite->getContentSize();
            mahousekiPresentWarningSprite->setScale( 1.2 * w / 640);
            mahousekiPresentWarningSprite->setPosition(Point(tmpSize.width * 0.2, tmpSize.height * 0.95));
            image->addChild(mahousekiPresentWarningSprite);
            
            auto label = LabelTTF::create("!!", FONT, LEVEL_FONT_SIZE);
            label->setFontFillColor(Color3B(255, 255, 255));
            label->setPosition(Point(tmpSize2.width * 0.5, tmpSize2.height * 0.5));
            mahousekiPresentWarningSprite->addChild(label);
            
            mahousekiPresentWarningSprite->setVisible(false);
        }
        
        if(i == 0){
            // チュートリアル矢印
            menu100tutorialSprite = Sprite::create("tutorial.png");
            menu100tutorialSprite->setAnchorPoint(Point(0.5, 0));
            menu100tutorialSprite->setScale( 0.3 * w / 640);
            menu100tutorialSprite->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.9));
            image->addChild(menu100tutorialSprite, 2010, 2010);
            menu100tutorialSprite->setVisible(false);
            auto jump = JumpBy::create(0.5, Point::ZERO, w * 0.05, 1);
            menu100tutorialSprite->runAction(RepeatForever::create(Sequence::create(jump, NULL)));
        }
        if(i == 4){
            // チュートリアル矢印
            menu500tutorialSprite = Sprite::create("tutorial.png");
            menu500tutorialSprite->setAnchorPoint(Point(0.5, 0));
            menu500tutorialSprite->setScale( 0.3 * w / 640);
            menu500tutorialSprite->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.9));
            image->addChild(menu500tutorialSprite, 2011, 2011);
            menu500tutorialSprite->setVisible(false);
            auto jump = JumpBy::create(0.5, Point::ZERO, w * 0.05, 1);
            menu500tutorialSprite->runAction(RepeatForever::create(Sequence::create(jump, NULL)));
        }
        
        MenuItemSprite* menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::globalMenu, this, (i+1)*100));
        menuItem->setContentSize(tmpSize);
        menuItem->setAnchorPoint(Point::ZERO);
        menuItem->setPosition(Point::ZERO);
        
        Menu* menu = Menu::create(menuItem, NULL);
        menu->setContentSize(tmpSize);
        menu->setAnchorPoint(Point::ZERO);
        menu->setPosition(Point( (float)i * w / GLOBAL_MENU_NUM - i * 0.6 , 0));
        
        this->addChild(menu, 2001+i, 11001+i);
        
    }
    
    return (float)w / GLOBAL_MENU_NUM;
    
}

float HelloWorld::setTopMenu()
{
    Scale9Sprite* topMenuWindowImage = Scale9Sprite::create("window2.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    topMenuWindowImage->setContentSize(Size(w , h * 0.15));
    topMenuWindowImage->setAnchorPoint(Point::ZERO);
    topMenuWindowImage->setPosition(Point(x, h * 0.85));
    this->addChild(topMenuWindowImage, 2000);
    
    yt = h * 0.15;
    
    auto label = LabelTTF::create(LocalizedString::getLocalizedString("title", "title"), FONT, LEVEL_FONT_SIZE * 0.7);
    label->setFontFillColor(Color3B(255, 255, 255));
    label->setAnchorPoint(Point(1, 0.5));
    label->setPosition(Point(w * 0.97, h * 0.13));
    topMenuWindowImage->addChild(label);
    
    auto label2 = LabelTTF::create(LocalizedString::getLocalizedString("stamina", "stamina"), FONT, LEVEL_FONT_SIZE);
    label2->setFontFillColor(Color3B(255, 255, 255));
    label2->setAnchorPoint(Point(0, 0.5));
    label2->setPosition(Point(w * 0.03, h * 0.06));
    topMenuWindowImage->addChild(label2);
    
    auto label3 = LabelTTF::create(LocalizedString::getLocalizedString("battleStamina", "battleStamina"), FONT, LEVEL_FONT_SIZE);
    label3->setFontFillColor(Color3B(255, 255, 255));
    label3->setAnchorPoint(Point(0, 0.5));
    label3->setPosition(Point(w * 0.03, h * 0.03));
    topMenuWindowImage->addChild(label3);
    
    auto label4 = LabelTTF::create(LocalizedString::getLocalizedString("coin", "coin"), FONT, LEVEL_FONT_SIZE);
    label4->setFontFillColor(Color3B(255, 255, 255));
    label4->setPosition(Point(w * 0.7, h * 0.06));
    topMenuWindowImage->addChild(label4);
    
    auto label5 = LabelTTF::create(LocalizedString::getLocalizedString("mahouseki", "mahouseki"), FONT, LEVEL_FONT_SIZE);
    label5->setFontFillColor(Color3B(255, 255, 255));
    label5->setPosition(Point(w * 0.7, h * 0.03));
    topMenuWindowImage->addChild(label5);
    
    auto label6 = LabelTTF::create(LocalizedString::getLocalizedString("rank", "rank"), FONT, LEVEL_FONT_SIZE);
    label6->setFontFillColor(Color3B(255, 255, 255));
    label6->setPosition(Point(w * 0.7, h * 0.09));
    topMenuWindowImage->addChild(label6);
    
    char tmpChar[64];
    
    sprintf(tmpChar, "%s", userName.c_str());
    userNameLabel = LabelTTF::create(tmpChar, FONT2, LEVEL_FONT_SIZE);
    userNameLabel->setFontFillColor(Color3B(255, 255, 255));
    userNameLabel->setAnchorPoint(Point(0, 0.5));
    userNameLabel->setPosition(Point(w * 0.03, h * 0.1));
    topMenuWindowImage->addChild(userNameLabel);
    
    sprintf(tmpChar, "%d", staminaMax);
    staminaMaxLabel = LabelTTF::create(tmpChar, FONT2, LEVEL_FONT_SIZE);
    staminaMaxLabel->setFontFillColor(Color3B(255, 255, 255));
    staminaMaxLabel->setAnchorPoint(Point(1, 0.5));
    staminaMaxLabel->setPosition(Point(w * 0.6, h * 0.06));
    topMenuWindowImage->addChild(staminaMaxLabel);
    
    auto label7 = LabelTTF::create("/", FONT, LEVEL_FONT_SIZE);
    label7->setFontFillColor(Color3B(255, 255, 255));
    label7->setAnchorPoint(Point(1, 0.5));
    label7->setPosition(Point(w * 0.6 - staminaMaxLabel->getContentSize().width, h * 0.06));
    topMenuWindowImage->addChild(label7);
    
    sprintf(tmpChar, "%d", stamina);
    CCLOG("stamina %s", tmpChar);
    staminaLabel = LabelTTF::create(tmpChar, FONT2, LEVEL_FONT_SIZE);
    staminaLabel->setFontFillColor(Color3B(255, 255, 255));
    staminaLabel->setAnchorPoint(Point(1, 0.5));
    staminaLabel->setPosition(Point(w * 0.6 - staminaMaxLabel->getContentSize().width - label7->getContentSize().width, h * 0.06));
    topMenuWindowImage->addChild(staminaLabel);
    
    sprintf(tmpChar, "%d", battleStaminaMax);
    battleStaminaMaxLabel = LabelTTF::create(tmpChar, FONT2, LEVEL_FONT_SIZE);
    battleStaminaMaxLabel->setFontFillColor(Color3B(255, 255, 255));
    battleStaminaMaxLabel->setAnchorPoint(Point(1, 0.5));
    battleStaminaMaxLabel->setPosition(Point(w * 0.6, h * 0.03));
    topMenuWindowImage->addChild(battleStaminaMaxLabel);
    
    auto label8 = LabelTTF::create("/", FONT, LEVEL_FONT_SIZE);
    label8->setFontFillColor(Color3B(255, 255, 255));
    label8->setAnchorPoint(Point(1, 0.5));
    label8->setPosition(Point(w * 0.6 - staminaMaxLabel->getContentSize().width, h * 0.03));
    topMenuWindowImage->addChild(label8);
    
    sprintf(tmpChar, "%d", battleStamina);
    battleStaminaLabel = LabelTTF::create(tmpChar, FONT2, LEVEL_FONT_SIZE);
    battleStaminaLabel->setFontFillColor(Color3B(255, 255, 255));
    battleStaminaLabel->setAnchorPoint(Point(1, 0.5));
    battleStaminaLabel->setPosition(Point(w * 0.6 - staminaMaxLabel->getContentSize().width - label8->getContentSize().width, h * 0.03));
    topMenuWindowImage->addChild(battleStaminaLabel);
    
    sprintf(tmpChar, "%d", coin);
    coinLabel = LabelTTF::create(tmpChar, FONT2, LEVEL_FONT_SIZE);
    coinLabel->setFontFillColor(Color3B(255, 255, 255));
    coinLabel->setAnchorPoint(Point(1, 0.5));
    coinLabel->setPosition(Point(w * 0.97, h * 0.06));
    topMenuWindowImage->addChild(coinLabel);
    
    sprintf(tmpChar, "%d", mahouseki + mahouseki_purchase);
    mahousekiLabel = LabelTTF::create(tmpChar, FONT2, LEVEL_FONT_SIZE);
    mahousekiLabel->setFontFillColor(Color3B(255, 255, 255));
    mahousekiLabel->setAnchorPoint(Point(1, 0.5));
    mahousekiLabel->setPosition(Point(w * 0.97, h * 0.03));
    topMenuWindowImage->addChild(mahousekiLabel);
    
    sprintf(tmpChar, "%d", battleRank);
    rankLabel = LabelTTF::create(tmpChar, FONT2, LEVEL_FONT_SIZE);
    rankLabel->setFontFillColor(Color3B(255, 255, 255));
    rankLabel->setAnchorPoint(Point(1, 0.5));
    rankLabel->setPosition(Point(w * 0.97, h * 0.09));
    topMenuWindowImage->addChild(rankLabel);
    
    // 残り時間
    long tmpTimeDiff = STAMINAINCREACESECOND - (time(NULL) - lastStaminaDecreaseTime);
    sprintf(tmpChar, "(%s%ld:%02ld)", LocalizedString::getLocalizedString("last", "last"),
            (tmpTimeDiff / 60 ) % 60,
            tmpTimeDiff % 60);
    staminaLastTimeLabel = LabelTTF::create(tmpChar, FONT2, LEVEL_FONT_SIZE * 0.7);
    staminaLastTimeLabel->setFontFillColor(Color3B(255, 255, 255));
    staminaLastTimeLabel->setAnchorPoint(Point(1, 0.5));
    staminaLastTimeLabel->setPosition(Point(w * 0.38, h * 0.06));
    if(stamina == staminaMax){
        staminaLastTimeLabel->setString("");
    }
    topMenuWindowImage->addChild(staminaLastTimeLabel);
    
    tmpTimeDiff = BattleSTAMINAINCREACESECOND - (time(NULL) - lastBattleStaminaDecreaseTime);
    sprintf(tmpChar, "(%s%ld:%02ld)", LocalizedString::getLocalizedString("last", "last"),
            (tmpTimeDiff / 60 ) % 60,
            tmpTimeDiff % 60);
    battleStaminaLastTimeLabel = LabelTTF::create(tmpChar, FONT2, LEVEL_FONT_SIZE * 0.7);
    battleStaminaLastTimeLabel->setFontFillColor(Color3B(255, 255, 255));
    battleStaminaLastTimeLabel->setAnchorPoint(Point(1, 0.5));
    battleStaminaLastTimeLabel->setPosition(Point(w * 0.38, h * 0.03));
    if(battleStamina == battleStaminaMax){
        battleStaminaLastTimeLabel->setString("");
    }
    topMenuWindowImage->addChild(battleStaminaLastTimeLabel);
    
    return 0.0;
}

void HelloWorld::updateTopMenu()
{
    char tmpChar[64];
    
    sprintf(tmpChar, "%s", userName.c_str());
    userNameLabel->setString(tmpChar);
    
    sprintf(tmpChar, "%d", stamina);
    staminaLabel->setString(tmpChar);
    
    sprintf(tmpChar, "%d", staminaMax);
    staminaMaxLabel->setString(tmpChar);
    
    sprintf(tmpChar, "%d", battleStamina);
    battleStaminaLabel->setString(tmpChar);
    
    sprintf(tmpChar, "%d", battleStaminaMax);
    battleStaminaMaxLabel->setString(tmpChar);
    
    sprintf(tmpChar, "%d", coin);
    coinLabel->setString(tmpChar);
    
    sprintf(tmpChar, "%d", mahouseki + mahouseki_purchase);
    mahousekiLabel->setString(tmpChar);
    
    sprintf(tmpChar, "%d", battleRank);
    rankLabel->setString(tmpChar);
    
    int time = std::time(NULL);
    
    long tmpTimeDiff = STAMINAINCREACESECOND - (time - lastStaminaDecreaseTime);
    sprintf(tmpChar, "(%s%ld:%02ld)", LocalizedString::getLocalizedString("last", "last"),
            (tmpTimeDiff / 60 ) % 60,
            tmpTimeDiff % 60);
    staminaLastTimeLabel->setString(tmpChar);
    if(stamina == staminaMax){
        staminaLastTimeLabel->setString("");
    }
    
    tmpTimeDiff = BattleSTAMINAINCREACESECOND - (time - lastBattleStaminaDecreaseTime);
    sprintf(tmpChar, "(%s%ld:%02ld)", LocalizedString::getLocalizedString("last", "last"),
            (tmpTimeDiff / 60 ) % 60,
            tmpTimeDiff % 60);
    battleStaminaLastTimeLabel->setString(tmpChar);
    if(battleStamina == battleStaminaMax){
        battleStaminaLastTimeLabel->setString("");
    }
}

void HelloWorld::checkTopMenu(float dt)// １秒ごとにここが呼ばれる
{
    int check = 0;
    int check2 = 0;
    
    int time = std::time(NULL);
    
    // スタミナ回復
    if(stamina < staminaMax){
        if(time - lastStaminaDecreaseTime >= STAMINAINCREACESECOND){
            stamina += (time - lastStaminaDecreaseTime) / STAMINAINCREACESECOND;
            lastStaminaDecreaseTime = time - (time - lastStaminaDecreaseTime) % STAMINAINCREACESECOND;
            if(stamina > staminaMax){ stamina = staminaMax; }
            check++;
        }
    }
    
    // バトルスタミナ回復
    if(battleStamina < battleStaminaMax){
        if(time - lastBattleStaminaDecreaseTime >= BattleSTAMINAINCREACESECOND){
            battleStamina += (time - lastBattleStaminaDecreaseTime) / BattleSTAMINAINCREACESECOND;
            lastBattleStaminaDecreaseTime = time - (time - lastStaminaDecreaseTime) % BattleSTAMINAINCREACESECOND;
            if(battleStamina > battleStaminaMax){ battleStamina = battleStaminaMax; }
            check++;
        }
    }
    
    // スタミナ残り時間
    long tmpTimeDiff = STAMINAINCREACESECOND - (time - lastStaminaDecreaseTime);
    if(tmpTimeDiff > 0){
        check2++;
    }
    
    // バトルスタミナ残り時間
    tmpTimeDiff = BattleSTAMINAINCREACESECOND - (time - lastBattleStaminaDecreaseTime);
    if(tmpTimeDiff > 0){
        check2++;
    }
    
    // 魔法石がもらえる２４時間計算
    if(time - lastMahousekiGotTime >= MAHOUSEKITIME){
        mahousekiPresentWarningSprite->setVisible(true);
    }
    else{
        mahousekiPresentWarningSprite->setVisible(false);
    }
    
    if(check > 0){
        updateUserDataDb();
    }
    if(check > 0 || check2 > 0){
        updateTopMenu();
    }
    
    // 画面下のチュートリアル矢印　ダンジョン
    if(visibleDangeonNumber <= TUTORIAL1){
        if((presentMenuNum - presentMenuNum % 100)/ 100 == 1){
            menu100tutorialSprite->setVisible(false);
        }
        else{
            menu100tutorialSprite->setVisible(true);
        }
    }
    
    // 画面下のチュートリアル矢印　ダンジョン
    if(visibleDangeonNumber > TUTORIAL1 && battleCount == 0){
        if((presentMenuNum - presentMenuNum % 100)/ 100 == 5){
            menu500tutorialSprite->setVisible(false);
        }
        else{
            menu500tutorialSprite->setVisible(true);
        }
    }
    
    
}

void HelloWorld::moveMap(int menuNum)
{
    float test = - ( worldmap->getContentSize().width * (h*0.9 / worldmap->getContentSize().height) - w ) * (menuNum - 100) / ( (GLOBAL_MENU_NUM - 1) * 100 ) ;
    auto move = MoveTo::create(0.5, Point(test, h * 0.5 ));
    auto func = CallFunc::create(CC_CALLBACK_0(HelloWorld::moveMapFinished, this));
    worldmap->runAction(Sequence::create(move, func, NULL));
}

void HelloWorld::moveMapFinished()
{
    globalMenuChanging = false;
}

//////////////////////////////////////////////////
//////////////////////////////////////////////////
//////////////////////////////////////////////////
//  メニュー遷移
//////////////////////////////////////////////////
//////////////////////////////////////////////////
//////////////////////////////////////////////////

void HelloWorld::globalMenu(int menuNum)
{
    if(tapProhibit){
        return;
    }
    
    spesialDangeonRemainingTimeCountingDown = false;
    
    if(globalMenuChanging == false){
        globalMenuChanging = true;
        
        pageTitleOutAndRemove(menuNum);
        
        removeMenu();
        setMenu(menuNum);
        
        moveMap(menuNum);
    }
}

void HelloWorld::setPageTitleAndIn(int menuNum)
{
    
    Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image->setContentSize(Size(w * 0.7 , h * 0.08));
    image->setAnchorPoint(Point::ZERO);
    image->setPosition(Point(- w * 0.7, h * 0.77));
    
    char tmpChar[16];
    sprintf(tmpChar, "pageTitle%d", menuNum);
    auto label = LabelTTF::create(LocalizedString::getLocalizedString(tmpChar, "メニュー"), FONT, LEVEL_FONT_SIZE);
    Size tmpSize = image->getContentSize();
    label->setAnchorPoint(Point(0, 0.5));
    label->setPosition(Point(tmpSize.width * 0.12, tmpSize.height * 0.5));
    label->setFontFillColor(Color3B(255, 255, 255));
    image->addChild(label);
    
    this->addChild(image, 2000, 11000);
    
    if(menuNum % 100 != 0){ // グローバルメニュー以外は戻るがつく
        label->setPosition(Point(tmpSize.width * 0.22, tmpSize.height * 0.5));
        
        Scale9Sprite* backimage = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(30, 30, 120, 120));
        backimage->setContentSize(Size(w * 0.15 , h * 0.06));
        backimage->setPosition(Point::ZERO);
        Size tmpSize2 = backimage->getContentSize();
        
        auto backlabel = LabelTTF::create(LocalizedString::getLocalizedString("back", "back"), FONT, LEVEL_FONT_SIZE);
        backlabel->setPosition(Point(w * 0.075, h * 0.03));
        backlabel->setFontFillColor(Color3B(255, 255, 255));
        backimage->addChild(backlabel);
        
        MenuItemSprite* menuItem = MenuItemSprite::create(backimage, backimage, CC_CALLBACK_0(HelloWorld::menuTapped, this, getBackMenuNum(menuNum)));
        menuItem->setContentSize(tmpSize2);
        menuItem->setPosition(Point::ZERO);
        
        Menu* menu = Menu::create(menuItem, NULL);
        menu->setContentSize(tmpSize2);
        menu->setPosition(Point(w * 0.09, tmpSize.height * 0.5));
        
        image->addChild(menu, 2000, 11001);
        
    }
    
    auto move = MoveTo::create(0.15, Point(x, h * 0.77));
    auto func = CallFunc::create(CC_CALLBACK_0(HelloWorld::setPageTitleAndInFinished, this));
    image->runAction(Sequence::create(move, func, NULL));
    
}

void HelloWorld::setPageTitleAndInFinished()
{
    
}

void HelloWorld::setPageTitleAndIn2(int menuNum, int dangeonMasterId)
{
    
    Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image->setContentSize(Size(w * 0.7 , h * 0.08));
    image->setAnchorPoint(Point::ZERO);
    image->setPosition(Point(- w * 0.7, h * 0.77));
    
    CCLOG("dangeon_masterId %d", dangeonMasterId);
    char tmpChar[32];
    sprintf(tmpChar, "dangeon_master%d", dangeonMasterId);
    auto label = LabelTTF::create(LocalizedString::getLocalizedString(tmpChar, tmpChar), FONT, LEVEL_FONT_SIZE);
    Size tmpSize = image->getContentSize();
    label->setAnchorPoint(Point(0, 0.5));
    label->setPosition(Point(tmpSize.width * 0.12, tmpSize.height * 0.5));
    label->setFontFillColor(Color3B(255, 255, 255));
    image->addChild(label);
    
    this->addChild(image, 2000, 11000);
    
    if(menuNum % 100 != 0){ // グローバルメニュー以外は戻るがつく
        label->setPosition(Point(tmpSize.width * 0.22, tmpSize.height * 0.5));
        
        Scale9Sprite* backimage = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(30, 30, 120, 120));
        backimage->setContentSize(Size(w * 0.15 , h * 0.06));
        backimage->setPosition(Point::ZERO);
        Size tmpSize2 = backimage->getContentSize();
        
        auto backlabel = LabelTTF::create(LocalizedString::getLocalizedString("back", "back"), FONT, LEVEL_FONT_SIZE);
        backlabel->setPosition(Point(w * 0.075, h * 0.03));
        backlabel->setFontFillColor(Color3B(255, 255, 255));
        backimage->addChild(backlabel);
        
        MenuItemSprite* menuItem = MenuItemSprite::create(backimage, backimage, CC_CALLBACK_0(HelloWorld::menuTapped, this, getBackMenuNum(menuNum)));
        menuItem->setContentSize(tmpSize2);
        menuItem->setPosition(Point::ZERO);
        
        Menu* menu = Menu::create(menuItem, NULL);
        menu->setContentSize(tmpSize2);
        menu->setPosition(Point(w * 0.09, tmpSize.height * 0.5));
        
        image->addChild(menu, 2000, 11001);
    }
    
    auto move = MoveTo::create(0.15, Point(x, h * 0.77));
    image->runAction(move);
}

int HelloWorld::getBackMenuNum(int menuNum)
{
    CCLOG("getBackMenuNum : %d", menuNum);
    switch (menuNum) {
        case 110: // パーティー編成で１キャラ目を選んだ画面
            return 100; // パーティー編成
            break;
        case 210: // パーティー編成で１キャラ目を選んだ画面
            return getBackMenuNum2(210); //
            break;
        case 220: // ユニット詳細
            return 203; // ユニット一覧
            break;
        default:
            break;
    }
    
    if(menuNum % 100 != 0){
        return menuNum - menuNum % 100;
    }
    
    return menuNum;
}

int HelloWorld::getBackMenuNum2(int menuNum)
{
    if(!arenaPartyMemberEditing){
        return 201;
    }
    else{
        return 202;
    }
}

void HelloWorld::pageTitleOutAndRemove(int menuNum)
{
    auto node = this->getChildByTag(11000);
    auto move = MoveTo::create(0.15, Point(-w * 0.7, h * 0.77));
    auto func = CallFunc::create(CC_CALLBACK_0(HelloWorld::pageTitleRemoveAndSetNextPageTitle, this, menuNum));
    node->runAction(Sequence::create(move, func, NULL));
}

void HelloWorld::pageTitleOutAndRemove2(int menuNum, int dangeonMasterId)
{
    auto node = this->getChildByTag(11000);
    auto move = MoveTo::create(0.15, Point(-w * 0.7, h * 0.77));
    auto func = CallFunc::create(CC_CALLBACK_0(HelloWorld::pageTitleRemoveAndSetNextPageTitle2, this, menuNum, dangeonMasterId));
    node->runAction(Sequence::create(move, func, NULL));
}


void HelloWorld::pageTitleRemoveAndSetNextPageTitle(int menuNum)
{
    this->removeChildByTag(11000);
    setPageTitleAndIn(menuNum);
}

void HelloWorld::pageTitleRemoveAndSetNextPageTitle2(int menuNum, int dangeonMasterId)
{
    this->removeChildByTag(11000);
    setPageTitleAndIn2(menuNum, dangeonMasterId);
}

void HelloWorld::setMenu(int menuNum)
{
    // 画面下のチュートリアル矢印のためにモードを取得
    presentMenuNum = menuNum;
    
    if(menuNum == 100){ // ダンジョン一覧だけは特別扱い
        setMenu100();
        return;
    }
    
    for(int i = 0; i < MenuItems[(int)(menuNum / 100)]; i++){
        Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
        image->setContentSize(Size(w * 0.9 , w * 0.18));
        Point point = image->getAnchorPoint();
        image->setPosition(Point::ZERO);
        
        char tmpChar[16];
        sprintf(tmpChar, "pageTitle%d", menuNum+i+1);
        auto label = LabelTTF::create(LocalizedString::getLocalizedString(tmpChar, tmpChar), FONT, LEVEL_FONT_SIZE);
        Size tmpSize = image->getContentSize();
        label->setAnchorPoint(Point(0, 0.5));
        label->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.5));
        label->setFontFillColor(Color3B(255, 255, 255));
        image->addChild(label);
        
        // ショップは２４時間ごとに魔法石もらえる
        // 魔法石がもらえる２４時間計算
        if(menuNum == 300 && i == 0){
            if(time(NULL) - lastMahousekiGotTime >= MAHOUSEKITIME){
                auto sprite = Sprite::create("whitecircle.png");
                Size tmpSize2 = sprite->getContentSize();
                sprite->setScale( 1.2 * w / 640);
                sprite->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.8));
                image->addChild(sprite);
                
                auto label = LabelTTF::create("!!", FONT, LEVEL_FONT_SIZE);
                label->setFontFillColor(Color3B(255, 255, 255));
                label->setPosition(Point(tmpSize2.width * 0.5, tmpSize2.height * 0.5));
                sprite->addChild(label);
            }
        }
        
        MenuItemSprite* menuItem;
        // アリーナも特別扱い
        int b_rank; // 対戦相手のランク
        if(menuNum == 500 && i == 0){
            
            // 対戦相手のデータがDBに入っていたらバトル開始ボタンを出す
            sqlite3* db = NULL;
            std::string fullpath = FileUtils::getInstance()->getWritablePath();
            fullpath += "userdata.db";
            
            if (sqlite3_open(fullpath.c_str(), &db) == SQLITE_OK) {
                sqlite3_stmt *stmt = NULL;
                std::string sql = "select * from other_users_party";
                if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
                    sqlite3_step(stmt);
                    b_rank          = (int)sqlite3_column_int(stmt, 2);
                }
                sqlite3_reset(stmt);
                sqlite3_finalize(stmt);
            }
            sqlite3_close(db);

            menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::setMenu501_battleStart, this));
        }
        else{
            menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::menuTapped, this, menuNum+i+1));
        }
        menuItem->setContentSize(tmpSize);
        menuItem->setPosition(Point::ZERO);
        
        Menu* menu = Menu::create(menuItem, NULL);
        menu->setContentSize(tmpSize);
        
        MoveBy* move;

        if(menuNum == 500){
            if(visibleDangeonNumber < ARENAABLEDANGEONNUM){ // バトルできるまでダンジョンが進んでない場合、何も出さない
            }
            else if (b_rank == 0 && visibleDangeonNumber >= ARENAABLEDANGEONNUM) { // 対戦相手が見つからない場合、さがすボタンだけ出す
                if(i == 1) {
                    menu->setPosition(Point(w * 1.5, h * 0.7 - w * 0.18 * (i+2)));
                    this->addChild(menu, 100+i, 100+i);
                    move = MoveBy::create(0.25 + 0.05 * (i+1), Point(-w, 0));
                    menu->runAction(move);
                }
            }
            else{
                menu->setPosition(Point(w * 1.5, h * 0.7 - w * 0.18 * (i+3)));
                this->addChild(menu, 100+i, 100+i);
                move = MoveBy::create(0.25 + 0.05 * (i+1), Point(-w, 0));
                menu->runAction(move);
            }
        }
        else{
            menu->setPosition(Point(w * 1.5, h * 0.7 - w * 0.18 * i));
            this->addChild(menu, 100+i, 100+i);
            move = MoveBy::create(0.25 + 0.05 * i, Point(-w, 0));
            menu->runAction(move);
        }
        
        // アリーナは対戦相手を表示する。
        if(menuNum == 500 && i == 0){
            setMenu500_BattleInfo();
        }
        
    }
    
    // アリーナはチュートリアル矢印を出す
    if(menuNum == 500 && visibleDangeonNumber > TUTORIAL1 && battleCount == 0){
        auto sprite = Sprite::create("tutorial.png");
        sprite->setAnchorPoint(Point(0.5, 0));
        sprite->setScale( 0.3 * w / 640);
        sprite->setPosition(Point( w * 0.15 + w, h * 0.7 - w * 0.18 * 3 + w * 0.18 / 4));
        this->addChild(sprite, 300, 300);
        
        auto move2 = MoveBy::create(0.25, Point(-w,0));
        sprite->runAction(move2);
        auto jump = JumpBy::create(0.5, Point::ZERO, w * 0.05, 1);
        sprite->runAction(RepeatForever::create(Sequence::create(jump, NULL)));
    }
    

}

void HelloWorld::removeMenu()
{
    playSE(0); // se_enter.mp3
    
    for(int i = 100; i < 1000; i++){
        this->removeChildByTag(i);
    }
    
}

void HelloWorld::menuTapped(int menuNum)
{
    if(tapProhibit){
        return;
    }
    
    
    // お勧めアプリは外部を呼ぶだけなので、何もしない
    if(menuNum == 604){ // お勧めアプリ
        setMenu611();
        return;
    }
    
    if(menuNum != 502){
        pageTitleOutAndRemove(menuNum);
    }
    CCLOG("menuTapped %d", menuNum);
    removeMenu();
    
    switch (menuNum) {
        case 100:
            globalMenu(100);
            break;
        case 200:
            globalMenu(200);
            break;
        case 201:
            setMenu201(false); // パーティー編成（ダンジョン）
            break;
        case 202:
            setMenu201(true); // パーティー編成（アリーナ）
            break;
        case 203:
            setMenu202(0); // ユニット一覧
            break;
        case 300:
            globalMenu(300);
            break;
        case 301:
            // ストアのチェックを行う
            checkStoreProducts();
            //setMenu301();
            break;
        case 302:
            setMenu302(true, false);
            break;
        case 303:
            setMenu302(false, false);
            break;
        case 400:
            globalMenu(400);
            break;
        case 401:
            setMenu401(0); // スキル一覧
            break;
        case 402:
            setMenu402(0, true); // スキル進化
            break;
        case 403:
            setMenu403(0, true); // スキル売却
            break;
        case 500:
            globalMenu(500);
            break;
        case 501:
            setMenu501_battleStart();
            break;
        case 502:
            setMenu502_searchOtherOpponent();
            break;
        case 600:
            globalMenu(600);
            break;
        case 601:
            setMenu601();
            break;
        case 602:
            setMenu602();
            break;
        case 603:
            //setMenu603();
            // 「その他」へ飛ばす
            setMenu610();
            break;
        // お勧めアプリはこの関数内一番上ではじいている
        //case 604:
            //setMenu611();
            //setMenu604();
            break;
        //case 605:
            //setMenu605();
            //break;
            
        default:
            break;
    }
}


//////////////////////////////////////////////////
//////////////////////////////////////////////////
//////////////////////////////////////////////////
//  メニュー１００系
//////////////////////////////////////////////////
//////////////////////////////////////////////////
//////////////////////////////////////////////////

void HelloWorld::setMenu100()
{
    pageTitleOutAndRemove(100);
    removeMenu();
    
    int disp_count = 0;
    
    ScrollView *scrollView = ScrollView::create();
    scrollView->setViewSize(Size(w, hc));
    scrollView->setPosition(Point(0, yg));
    
    LayerColor* scrollLayer = LayerColor::create(Color4B(255, 255, 255, SCROLLVIEWOPACITY));
    scrollLayer->changeWidth(w);
    
    float scrollLayerHeight;
    
    sqlite3* db = NULL;
    std::string fullpath = FileUtils::getInstance()->getWritablePath();
    fullpath += "gamedata.db";
    
    if (sqlite3_open(fullpath.c_str(), &db) == SQLITE_OK) {
        sqlite3_stmt *stmt = NULL;
        
        std::string sql = "select max(parent_dangeon) from dangeon_detail where _id <= ";
        char tmpSql[128];
        sprintf(tmpSql, "%d", visibleDangeonNumber);
        sql += tmpSql;
        
        if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
            sqlite3_step(stmt);
            int visibleParentDangeonNumberMax = (int)sqlite3_column_int(stmt, 0);
            
            sql = "select * from dangeon_master where _id <= ";
            sprintf(tmpSql, "%d order by _id desc", visibleParentDangeonNumberMax);
            sql += tmpSql;
            sqlite3_stmt *stmt2 = NULL;
            
            if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt2, NULL) == SQLITE_OK){
                int count = 0;
                while (sqlite3_step(stmt2) == SQLITE_ROW) {
                    count++;
                }
                
                //float scrollLayerHeight = h * 0.15 + h * 0.1 * count;
                scrollLayerHeight = h * 0.15 + w * 0.18 * count;
                scrollLayer->changeHeight(scrollLayerHeight);
                scrollLayer->setAnchorPoint(Point(0, 1));
                scrollLayer->setPosition(Point::ZERO);
                
                int writeCount = 0;
                
                while (sqlite3_step(stmt2) == SQLITE_ROW) {
                    int dangeon_master_id                       = (int)sqlite3_column_int(stmt2, 0);
                    const unsigned char* dangeon_master_name    = (const unsigned char*)sqlite3_column_text(stmt2, 1);
                    
                    Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
                    image->setContentSize(Size(w * 0.9 , h * 0.1 ));
                    image->setContentSize(Size(w * 0.9 , w * 0.18 ));
                    Size tmpSize = image->getContentSize();
                    image->setPosition(Point::ZERO);
                    
                    char tmpChar[64];
                    sprintf(tmpChar, "%s", dangeon_master_name);
                    auto label = LabelTTF::create(LocalizedString::getLocalizedString(tmpChar, "dangeon_master"), FONT, LEVEL_FONT_SIZE);
                    label->setFontFillColor(Color3B(255, 255, 255));
                    label->setAnchorPoint(Point(0, 0.5));
                    label->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.5));
                    image->addChild(label);
                    
                    if(writeCount == 0){
                        writeCount++;
                    }
                    else{
                        auto label2 = LabelTTF::create(LocalizedString::getLocalizedString("clear", "clear"), FONT, LEVEL_FONT_SIZE);
                        label2->setFontFillColor(Color3B(255, 255, 255));
                        label2->enableStroke(Color3B(0, 0, 0), 3);
                        label2->setAnchorPoint(Point(0, 0.5));
                        label2->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.85));
                        image->addChild(label2);
                    }
                    
                    MenuItemSprite* menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::setMenu110, this, dangeon_master_id));
                    menuItem->setContentSize(tmpSize);
                    menuItem->setPosition(Point::ZERO);
                    
                    Menu* menu = Menu::create2(menuItem, NULL);
                    menu->setContentSize(tmpSize);
                    menu->setPosition(Point( w * 0.5 + w, scrollLayerHeight - h * 0.15 - h * 0.1 * disp_count ));
                    menu->setPosition(Point( w * 0.5 + w, scrollLayerHeight - h * 0.15 - w * 0.18 * disp_count ));
                    
                    scrollLayer->addChild(menu, 200+disp_count, 200+disp_count);
                    
                    auto move = MoveBy::create(0.25 + 0.05 * disp_count, Point(-w, 0));
                    menu->runAction(move);
                    
                    disp_count++;
                }
            }
            sqlite3_reset(stmt2);
            sqlite3_finalize(stmt2);
        }
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    
    scrollView->setContentOffset(Point(0, hc - scrollLayer->getContentSize().height));
    scrollView->setContentSize(scrollLayer->getContentSize());
    scrollView->addChild(scrollLayer, 100, 101);
    
    this->addChild(scrollView, 100, 100);
    
    
    // 特殊ダンジョン
    /*
    if(visibleDangeonNumber >= TOKUSYUDANGEONNUM){
        Scale9Sprite* image = Scale9Sprite::create("window_red.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
        image->setContentSize(Size(w * 0.2 , h * 0.08 ));
        Size tmpSize = image->getContentSize();
        image->setPosition(Point::ZERO);
        
        auto label = LabelTTF::create(LocalizedString::getLocalizedString("specialdangeon", "SP"), FONT, LEVEL_FONT_SIZE);
        label->setFontFillColor(Color3B(255, 255, 255));
        label->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.5));
        image->addChild(label);
        
        MenuItemSprite* menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::setMenu120_specialDangeonList, this));
        menuItem->setContentSize(tmpSize);
        menuItem->setPosition(Point::ZERO);
        
        Menu* menu = Menu::create(menuItem, NULL);
        menu->setContentSize(tmpSize);
        menu->setPosition(Point( w * 1.9, h * 0.81 ));
        
        this->addChild(menu, 105, 105);
        
        auto move = MoveBy::create(0.25, Point(-w, 0));
        menu->runAction(move);
    }
     */
    
    // チュートリアル
    if(visibleDangeonNumber <= TUTORIAL1){
        auto sprite = Sprite::create("tutorial.png");
        sprite->setAnchorPoint(Point(0.5, 0));
        sprite->setScale( 0.3 * w / 640);
        sprite->setPosition(Point( w * 0.15 + w, scrollLayerHeight - h * 0.15 + w * 0.18 / 4));
        scrollLayer->addChild(sprite, 300, 300);
        
        auto move2 = MoveBy::create(0.25, Point(-w,0));
        sprite->runAction(move2);
        auto jump = JumpBy::create(0.5, Point::ZERO, w * 0.05, 1);
        sprite->runAction(RepeatForever::create(Sequence::create(jump, NULL)));
    }

}

void HelloWorld::setMenu110(int dangeonMasterId)
{
    ScrollView* check = (ScrollView*)this->getChildByTag(100);
    if(!(check->touchEndPointCheck())){ return; }
    
    Point touchEndPoint = check->getTouchEndPoint();
    Node* check2 = (Node*)this->getChildByTag(11000);
    if (check2->getBoundingBox().containsPoint(touchEndPoint)) {
        return;
    }
    
    pageTitleOutAndRemove2(110, dangeonMasterId);
    
    removeMenu();
    
    int disp_count = 0;
    
    ScrollView *scrollView = ScrollView::create();
    scrollView->setViewSize(Size(w, hc));
    scrollView->setPosition(Point(0, yg));
    
    LayerColor* scrollLayer = LayerColor::create(Color4B(255, 255, 255, SCROLLVIEWOPACITY));
    scrollLayer->changeWidth(w);
    
    float scrollLayerHeight;
    
    sqlite3* db = NULL;
    std::string fullpath = FileUtils::getInstance()->getWritablePath();
    fullpath += "gamedata.db";
    
    if (sqlite3_open(fullpath.c_str(), &db) == SQLITE_OK) {
        sqlite3_stmt *stmt = NULL;
        
        std::string sql = "select * from dangeon_detail where _id <= ";
        char tmpSql[128];
        sprintf(tmpSql, "%d and parent_dangeon = %d order by _id desc", visibleDangeonNumber, dangeonMasterId);
        sql += tmpSql;
        
        if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
            sqlite3_stmt *stmt2 = stmt;
            
            int count = 0;
            while (sqlite3_step(stmt2) == SQLITE_ROW) {
                count++;
            }
            
            //float scrollLayerHeight = h * 0.15 + h * 0.1 * count;
            scrollLayerHeight = h * 0.15 + w * 0.18 * count;
            scrollLayer->changeHeight(scrollLayerHeight);
            scrollLayer->setAnchorPoint(Point(0, 1));
            scrollLayer->setPosition(Point::ZERO);
            
            while (sqlite3_step(stmt2) == SQLITE_ROW) {
                int dangeon_detail_id                       = (int)sqlite3_column_int(stmt, 0);
                int decreaceStamina                         = (int)sqlite3_column_int(stmt, 2);
                int addStaminaFirstTimeClear                = (int)sqlite3_column_int(stmt, 3);
                int type1                                   = (int)sqlite3_column_int(stmt, 4);
                int type2                                   = (int)sqlite3_column_int(stmt, 5);
                int type3                                   = (int)sqlite3_column_int(stmt, 6);
                int type4                                   = (int)sqlite3_column_int(stmt, 7);
                int bgm                                     = (int)sqlite3_column_int(stmt, 8);
                int bossBgm                                 = (int)sqlite3_column_int(stmt, 9);
                int numberOfBattle                          = (int)sqlite3_column_int(stmt, 10);
                const unsigned char* dangeon_detail_name    = (const unsigned char*)sqlite3_column_text(stmt, 11);
                
                Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
                image->setContentSize(Size(w * 0.9 , h * 0.1 ));
                image->setContentSize(Size(w * 0.9 , w * 0.18 ));
                Size tmpSize = image->getContentSize();
                image->setPosition(Point::ZERO);
                
                char tmpChar[64];
                sprintf(tmpChar, "%s", dangeon_detail_name);
                auto label = LabelTTF::create(LocalizedString::getLocalizedString(tmpChar, "dangeon_detail"), FONT, LEVEL_FONT_SIZE);
                label->setFontFillColor(Color3B(255, 255, 255));
                label->setAnchorPoint(Point(0, 0.5));
                label->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.5));
                image->addChild(label);
                
                
                if(dangeon_detail_id < visibleDangeonNumber){
                    auto label2 = LabelTTF::create(LocalizedString::getLocalizedString("clear", "clear"), FONT, LEVEL_FONT_SIZE);
                    label2->setFontFillColor(Color3B(255, 255, 255));
                    label2->enableStroke(Color3B(0, 0, 0), 3);
                    label2->setAnchorPoint(Point(0, 0.5));
                    label2->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.85));
                    image->addChild(label2);
                }
                
                sprintf(tmpChar, "%s:%d", LocalizedString::getLocalizedString("stamina", "stamina"), decreaceStamina);
                auto label3 = LabelTTF::create(tmpChar, FONT, LEVEL_FONT_SIZE);
                label3->setFontFillColor(Color3B(255, 255, 255));
                label3->setAnchorPoint(Point(1, 0));
                label3->setPosition(Point(tmpSize.width * 0.95, tmpSize.height * 0.5));
                image->addChild(label3);
                
                sprintf(tmpChar, "%s:%d", LocalizedString::getLocalizedString("battle", "battle"), numberOfBattle);
                auto label4 = LabelTTF::create(tmpChar, FONT, LEVEL_FONT_SIZE);
                label4->setFontFillColor(Color3B(255, 255, 255));
                label4->setAnchorPoint(Point(1, 1));
                label4->setPosition(Point(tmpSize.width * 0.95, tmpSize.height * 0.5));
                image->addChild(label4);
                
                MenuItemSprite* menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::setMenu110battleStart, this, dangeon_detail_id));
                menuItem->setContentSize(tmpSize);
                menuItem->setPosition(Point::ZERO);
                
                Menu* menu = Menu::create2(menuItem, NULL);
                menu->setContentSize(tmpSize);
                menu->setPosition(Point( w * 0.5 + w, scrollLayerHeight - h * 0.15 - h * 0.1 * disp_count ));
                menu->setPosition(Point( w * 0.5 + w, scrollLayerHeight - h * 0.15 - w * 0.18 * disp_count ));
                
                scrollLayer->addChild(menu, 200+disp_count, 200+disp_count);
                
                auto move = MoveBy::create(0.25 + 0.05 * disp_count, Point(-w, 0));
                menu->runAction(move);
                
                disp_count++;
            }
            //    sqlite3_reset(stmt2);
            //    sqlite3_finalize(stmt2);
        }
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    
    scrollView->setContentOffset(Point(0, hc - scrollLayer->getContentSize().height));
    scrollView->setContentSize(scrollLayer->getContentSize());
    scrollView->addChild(scrollLayer, 100, 101);
    
    this->addChild(scrollView, 100, 100);
    
    // チュートリアル矢印
    if(visibleDangeonNumber <= TUTORIAL1){
        auto sprite = Sprite::create("tutorial.png");
        sprite->setAnchorPoint(Point(0.5, 0));
        sprite->setScale( 0.3 * w / 640);
        sprite->setPosition(Point( w * 0.15 + w, scrollLayerHeight - h * 0.15 + w * 0.18 / 4));
        scrollLayer->addChild(sprite, 300, 300);
        
        auto move2 = MoveBy::create(0.25, Point(-w,0));
        sprite->runAction(move2);
        auto jump = JumpBy::create(0.5, Point::ZERO, w * 0.05, 1);
        sprite->runAction(RepeatForever::create(Sequence::create(jump, NULL)));
    }
    
}


void HelloWorld::setMenu120_specialDangeonList()
{
    ScrollView* check = (ScrollView*)this->getChildByTag(100);
//    if(!(check->touchEndPointCheck())){ return; } // 後から足したからよくわからないけど、特殊ダンジョン一覧はここコメントアウト。動作は問題なさそう。
    
    Point touchEndPoint = check->getTouchEndPoint();
    Node* check2 = (Node*)this->getChildByTag(11000);
    if (check2->getBoundingBox().containsPoint(touchEndPoint)) {
        return;
    }
    
    spesialDangeonRemainingTimeCountingDown = true;
    previousSpesialDangeonRemainingTime = SPECIALDANGEONTIMEPERIOD;
    
    pageTitleOutAndRemove(100);
    removeMenu();
    
    int disp_count = 0;
    
    ScrollView *scrollView = ScrollView::create();
    scrollView->setViewSize(Size(w, hc));
    scrollView->setPosition(Point(0, yg));
    
    LayerColor* scrollLayer = LayerColor::create(Color4B(255, 255, 255, SCROLLVIEWOPACITY));
    scrollLayer->changeWidth(w);
    
    sqlite3* db = NULL;
    std::string fullpath = FileUtils::getInstance()->getWritablePath();
    fullpath += "gamedata.db";
    
    if (sqlite3_open(fullpath.c_str(), &db) == SQLITE_OK) {
        sqlite3_stmt *stmt = NULL;
        
        std::string sql = "select * from special_dangeon_detail";
        char tmpSql[128];
        sprintf(tmpSql, " order by _id desc");
        sql += tmpSql;
        
        if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
            sqlite3_stmt *stmt2 = stmt;

            // 残り時間の計算
            int time = std::time(NULL);
            int spesial_dangeon_type = ( time / SPECIALDANGEONTIMEPERIOD ) % SPECIALDANGEONBASICTYPES + 1; // 0 は使用しない（-1を掛けると0なので）
            int spesial_dangeon_remaining_time = SPECIALDANGEONTIMEPERIOD - time % SPECIALDANGEONTIMEPERIOD;
            int spesial_dangeon_remaining_time_minus1 = spesial_dangeon_remaining_time - 1;
            
            // ScrollLayerの高さ計算。あわせて、クリアフラグ用に今回表示されるダンジョンリストもとる。
            int count = 0;
            int tmp_dangeon_list[64];
            for(int i = 0; i < 64; i++){ tmp_dangeon_list[i] = -1; }
            while (sqlite3_step(stmt2) == SQLITE_ROW) {
                int dangeon_detail_id                       = (int)sqlite3_column_int(stmt2, 0);
                int type4                                   = (int)sqlite3_column_int(stmt2, 7);
                if((visibleDangeonNumber >= TOKUSYUDANGEONNUM2 && type4 == spesial_dangeon_type) ||
                   (visibleDangeonNumber >= TOKUSYUDANGEONNUM && type4 == -1 * spesial_dangeon_type)
                   ){
                    tmp_dangeon_list[count] = dangeon_detail_id;
                    //CCLOG("tmp_dangeon_list %d", dangeon_detail_id);
                    count++;
                }
            }
            
            // Clearフラグのためにspecial_dangeon_clear_listテーブルを検索
            int special_dangeon_clear_list[64]; // clear がつくdangeonidがはいる。すぐ上で表示数計算用としてとっているtmp_dangeon_listが64なので、Clear用のこれも64と同じ値で良い
            for(int i = 0; i < 64; i++){ special_dangeon_clear_list[i] = -1; }
            sqlite3* db2 = NULL;
            std::string fullpath2 = FileUtils::getInstance()->getWritablePath();
            fullpath2 += "userdata.db";
            if (sqlite3_open(fullpath2.c_str(), &db2) == SQLITE_OK) {
                sqlite3_stmt *stmt3;
                std::string sql2 = "select * from special_dangeon_clear_list";
                if(sqlite3_prepare_v2(db2, sql2.c_str(), -1, &stmt3, NULL) == SQLITE_OK){
                    int j = 0;
                    while (sqlite3_step(stmt3) == SQLITE_ROW) {
                        int tmpClearDangeonNum = (int)sqlite3_column_int(stmt3, 1);
                        for(int i = 0; i < 64; i++){
                            //if(tmp_dangeon_list[i] != -1) CCLOG("Clear Flag %d %d", tmpClearDangeonNum, tmp_dangeon_list[i]+ 10000);
                            // クリアリストdbと、今回表示するダンジョンが一致。
                            if(tmpClearDangeonNum == tmp_dangeon_list[i]+ 10000 ){
                                special_dangeon_clear_list[j] = tmpClearDangeonNum;
                                j++;
                                
                                // 倒したら〜〜が仲間になる、の場合は表示しないのでスクロールレイヤーのサイズを減らす
                                CCLOG("%d", tmpClearDangeonNum);
                                if(tmpClearDangeonNum >= UNITADDDANGEONSTART && tmpClearDangeonNum <= UNITADDDANGEONEND){
                                //    count--;
                                }
                            }
                        }
                    }
                }
                sqlite3_reset(stmt3);
                sqlite3_finalize(stmt3);
            }
            sqlite3_close(db2);
            
            //float scrollLayerHeight = h * 0.15 + h * 0.1 * count;
            float scrollLayerHeight = h * 0.15 + w * 0.18 * count;
            scrollLayer->changeHeight(scrollLayerHeight);
            scrollLayer->setAnchorPoint(Point(0, 1));
            scrollLayer->setPosition(Point::ZERO);

            while (sqlite3_step(stmt2) == SQLITE_ROW) {
                int dangeon_detail_id                       = (int)sqlite3_column_int(stmt, 0);
                int decreaceStamina                         = (int)sqlite3_column_int(stmt, 2);
                int addStaminaFirstTimeClear                = (int)sqlite3_column_int(stmt, 3);
                int type1                                   = (int)sqlite3_column_int(stmt, 4);
                int type2                                   = (int)sqlite3_column_int(stmt, 5);
                int type3                                   = (int)sqlite3_column_int(stmt, 6);
                int type4                                   = (int)sqlite3_column_int(stmt, 7);
                int bgm                                     = (int)sqlite3_column_int(stmt, 8);
                int bossBgm                                 = (int)sqlite3_column_int(stmt, 9);
                int numberOfBattle                          = (int)sqlite3_column_int(stmt, 10);
                const unsigned char* dangeon_detail_name    = (const unsigned char*)sqlite3_column_text(stmt, 11);
                
                // クリアフラグ
                bool cleared = false;
                for(int i = 0; i < 64; i++){
                    if(special_dangeon_clear_list[i] == dangeon_detail_id + 10000){
                        cleared = true;
                    }
                }
                
                if((visibleDangeonNumber >= TOKUSYUDANGEONNUM2 && type4 == spesial_dangeon_type) ||
                   (visibleDangeonNumber >= TOKUSYUDANGEONNUM && type4 == -1 * spesial_dangeon_type)
                   ){
                    Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
                    image->setContentSize(Size(w * 0.9 , h * 0.1 ));
                    image->setContentSize(Size(w * 0.9 , w * 0.18 ));
                    Size tmpSize = image->getContentSize();
                    image->setPosition(Point::ZERO);
                    
                    char tmpChar[64];
                    sprintf(tmpChar, "%s%d", dangeon_detail_name, 10000 + dangeon_detail_id);
                    auto label = LabelTTF::create(LocalizedString::getLocalizedString(tmpChar, tmpChar), FONT, LEVEL_FONT_SIZE);
                    label->setFontFillColor(Color3B(255, 255, 255));
                    label->setAnchorPoint(Point(0, 0.5));
                    label->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.5));
                    image->addChild(label);

                    // 残り時間を表示
                    sprintf(tmpChar, "%s%d:%02d:%02d", LocalizedString::getLocalizedString("last", "last"), spesial_dangeon_remaining_time_minus1 / (60*60), (spesial_dangeon_remaining_time_minus1 / 60) % 60, spesial_dangeon_remaining_time_minus1 % 60);
                    auto label2 = LabelTTF::create(tmpChar, FONT, LEVEL_FONT_SIZE);
                    label2->setFontFillColor(Color3B(255, 255, 0));
                    label2->setAnchorPoint(Point(1, -1));
                    label2->enableStroke(Color3B(0, 0, 0), 3);
                    label2->setPosition(Point(tmpSize.width * 0.95, tmpSize.height * 0.5));
                    image->addChild(label2);
                    // 残り時間更新
                    auto delay = DelayTime::create(1.0);
                    auto func = CallFunc::create(CC_CALLBACK_0(HelloWorld::setMenu120_specialDangeonListRemainingTimeUpdate, this, label));
                    label->runAction(Sequence::create(delay, func, NULL));
                    
                    if(cleared){
                        auto label3 = LabelTTF::create(LocalizedString::getLocalizedString("clear", "clear"), FONT, LEVEL_FONT_SIZE);
                        label3->setFontFillColor(Color3B(255, 255, 255));
                        label3->enableStroke(Color3B(0, 0, 0), 3);
                        label3->setAnchorPoint(Point(0, 0.5));
                        label3->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.85));
                        image->addChild(label3);
                    }
                    
                    sprintf(tmpChar, "%s:%d", LocalizedString::getLocalizedString("stamina", "stamina"), decreaceStamina);
                    auto label4 = LabelTTF::create(tmpChar, FONT, LEVEL_FONT_SIZE);
                    label4->setFontFillColor(Color3B(255, 255, 255));
                    label4->setAnchorPoint(Point(1, 0));
                    label4->setPosition(Point(tmpSize.width * 0.95, tmpSize.height * 0.5));
                    image->addChild(label4);
                    
                    sprintf(tmpChar, "%s:%d", LocalizedString::getLocalizedString("battle", "battle"), numberOfBattle);
                    auto label5 = LabelTTF::create(tmpChar, FONT, LEVEL_FONT_SIZE);
                    label5->setFontFillColor(Color3B(255, 255, 255));
                    label5->setAnchorPoint(Point(1, 1));
                    label5->setPosition(Point(tmpSize.width * 0.95, tmpSize.height * 0.5));
                    image->addChild(label5);
                    
                    MenuItemSprite* menuItem;
                    if((visibleDangeonNumber >= TOKUSYUDANGEONNUM2 && type4 == spesial_dangeon_type) ||
                       (!cleared))
                       {
                           menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::setMenu110battleStart, this, 10000 + dangeon_detail_id));
                       }
                    else{
                        menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::blank, this));
                        // クリアした者は灰色。noselectable
                        Scale9Sprite* image2 = Scale9Sprite::create("noselectable.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
                        image2->setContentSize(Size(w * 0.9 , w * 0.18 ));
                        image2->setOpacity(128);
                        image2->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.5));
                        image->addChild(image2);
                    }
                    //menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::setMenu110battleStart, this, 10000 + dangeon_detail_id));
                    menuItem->setContentSize(tmpSize);
                    menuItem->setPosition(Point::ZERO);
                    
                    Menu* menu = Menu::create2(menuItem, NULL);
                    menu->setContentSize(tmpSize);
                    menu->setPosition(Point( w * 0.5 + w, scrollLayerHeight - h * 0.15 - h * 0.1 * disp_count ));
                    menu->setPosition(Point( w * 0.5 + w, scrollLayerHeight - h * 0.15 - w * 0.18 * disp_count ));
                    
                    scrollLayer->addChild(menu, 200+disp_count, 200+disp_count);
                    
                    auto move = MoveBy::create(0.25 + 0.05 * disp_count, Point(-w, 0));
                    menu->runAction(move);
                    
                    disp_count++;
                }
            }
        }
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    
    scrollView->setContentOffset(Point(0, hc - scrollLayer->getContentSize().height));
    scrollView->setContentSize(scrollLayer->getContentSize());
    scrollView->addChild(scrollLayer, 100, 101);
    
    this->addChild(scrollView, 100, 100);
    
    // ソート用ボタン
    if(visibleDangeonNumber >= TOKUSYUDANGEONNUM){
        Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
        image->setContentSize(Size(w * 0.2 , h * 0.08 ));
        Size tmpSize = image->getContentSize();
        image->setPosition(Point::ZERO);
        
        auto label = LabelTTF::create(LocalizedString::getLocalizedString("normaldangeon", "Nor"), FONT, LEVEL_FONT_SIZE);
        label->setFontFillColor(Color3B(255, 255, 255));
        label->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.5));
        image->addChild(label);
        
        MenuItemSprite* menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::globalMenu, this, 100));
        menuItem->setContentSize(tmpSize);
        menuItem->setPosition(Point::ZERO);
        
        Menu* menu = Menu::create(menuItem, NULL);
        menu->setContentSize(tmpSize);
        menu->setPosition(Point( w * 1.9, h * 0.81 ));
        
        this->addChild(menu, 105, 105);
        
        auto move = MoveBy::create(0.25, Point(-w, 0));
        menu->runAction(move);
    }
}

void HelloWorld::setMenu120_specialDangeonListRemainingTimeUpdate(LabelTTF* tmplabel)
{
    int time = std::time(NULL);
    int spesial_dangeon_remaining_time = SPECIALDANGEONTIMEPERIOD - time % SPECIALDANGEONTIMEPERIOD;
    int spesial_dangeon_remaining_time_minus1 = spesial_dangeon_remaining_time - 1;

    char tmpChar[64];
    sprintf(tmpChar, "%s%d:%02d:%02d", LocalizedString::getLocalizedString("last", "last"), spesial_dangeon_remaining_time_minus1 / (60*60), (spesial_dangeon_remaining_time_minus1 / 60) % 60, spesial_dangeon_remaining_time_minus1 % 60);
    tmplabel->setString(tmpChar);
    
    if(spesialDangeonRemainingTimeCountingDown){
        if(previousSpesialDangeonRemainingTime < spesial_dangeon_remaining_time){
            previousSpesialDangeonRemainingTime = spesial_dangeon_remaining_time;
            CCLOG("call setMenu120_specialDangeonList()");
            setMenu120_specialDangeonList();
        }
        else{
            previousSpesialDangeonRemainingTime = spesial_dangeon_remaining_time;
            auto delay = DelayTime::create(1.0);
            auto func = CallFunc::create(CC_CALLBACK_0(HelloWorld::setMenu120_specialDangeonListRemainingTimeUpdate, this, tmplabel));
            tmplabel->runAction(Sequence::create(delay, func, NULL));
        }
    }
}

void HelloWorld::setMenu110battleStart(int dangeonDetailId)
{
    ScrollView* check = (ScrollView*)this->getChildByTag(100);
    if(!(check->touchEndPointCheck())){ return; }
    
    Point touchEndPoint = check->getTouchEndPoint();
    Node* check2 = (Node*)this->getChildByTag(11000);
    if (check2->getBoundingBox().containsPoint(touchEndPoint)) {
        Node* check3 = (Node*)check2->getChildByTag(11001);
        Rect check2rect = check2->getBoundingBox();
        Rect check3rect = check3->getBoundingBox();
        if( touchEndPoint.x > check2rect.origin.x + check3rect.origin.x - check3rect.size.width / 2
           && touchEndPoint.x < check2rect.origin.x + check3rect.origin.x + check3rect.size.width / 2
           && touchEndPoint.y > check2rect.origin.y + check3rect.origin.y - check3rect.size.height / 2
           && touchEndPoint.y < check2rect.origin.y + check3rect.origin.y + check3rect.size.height / 2
           ){
            // 戻る
            globalMenu(100);
        }
        return;
    }
    
    
    int dangeon_detail_id;
    int decreaceStamina;
    int addStaminaFirstTimeClear;
    int type1;
    int type2;
    int type3;
    int type4;
    int bgm;
    int bossBgm;
    int numberOfBattle;
    
    
    CCLOG("battle start %d", dangeonDetailId);
    sqlite3* db = NULL;
    std::string fullpath = FileUtils::getInstance()->getWritablePath();
    fullpath += "gamedata.db";
    
    if (sqlite3_open(fullpath.c_str(), &db) == SQLITE_OK) {
        sqlite3_stmt *stmt = NULL;
        
        std::string sql;
        char tmpSql[256];
        if(dangeonDetailId < 10000){
            sprintf(tmpSql, "select * from dangeon_detail where _id = %d", dangeonDetailId);
        }
        else{
            sprintf(tmpSql, "select * from special_dangeon_detail where _id = %d", dangeonDetailId - 10000);
        }
        sql = tmpSql;
        
        if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
            
            sqlite3_step(stmt);
            dangeon_detail_id                       = (int)sqlite3_column_int(stmt, 0);
            decreaceStamina                         = (int)sqlite3_column_int(stmt, 2);
            addStaminaFirstTimeClear                = (int)sqlite3_column_int(stmt, 3);
            type1                                   = (int)sqlite3_column_int(stmt, 4);
            type2                                   = (int)sqlite3_column_int(stmt, 5);
            type3                                   = (int)sqlite3_column_int(stmt, 6);
            type4                                   = (int)sqlite3_column_int(stmt, 7);
            bgm                                     = (int)sqlite3_column_int(stmt, 8);
            bossBgm                                 = (int)sqlite3_column_int(stmt, 9);
            numberOfBattle                          = (int)sqlite3_column_int(stmt, 10);
            const unsigned char* dangeon_detail_name    = (const unsigned char*)sqlite3_column_text(stmt, 11);
            
            if(stamina < decreaceStamina){
                playSE(1); // se_ng.mp3

                sqlite3_reset(stmt);
                sqlite3_finalize(stmt);
                sqlite3_close(db);
                
                pageTitleOutAndRemove(302);
                removeMenu();
                setMenu302(true, true);
                return;
            }
            
            // スタート扱いでスタミナ減らす
            if(stamina == staminaMax){ // 既に減っている場合は、時計は更新しない。
                lastStaminaDecreaseTime = time(NULL);
            }
            CCLOG("battle start. stamina : %d - %d", stamina, decreaceStamina);
            stamina -= decreaceStamina;
            
            if(dangeonDetailId < 10000 && visitedDangeonNumber < dangeonDetailId){ visitedDangeonNumber = dangeonDetailId; }
            
            // ユーザデータベース更新
            updateUserDataDb();
        }
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    
    playSE(0); // se_enter.mp3

    // バトルのシーンに飛ばす
    SceneManager* sceneManager = SceneManager::getInstance();
    sceneManager->dangeonId = dangeonDetailId;
    
    gotoBattleScene();
}

//////////////////////////////////////////////////
//////////////////////////////////////////////////
//////////////////////////////////////////////////
//  メニュー２００系
//////////////////////////////////////////////////
//////////////////////////////////////////////////
//////////////////////////////////////////////////

void HelloWorld::setMenu201(bool arena) // パーティー編成
{
    int disp_count = 0;
    
    partyOrderChanging = false;
    int partyMemberUnitId[5];
    
    int skill_list[64][2];
    for(int i = 0; i < 64; i++){
        for(int j = 0; j < 2; j++){
            skill_list[i][j] = 0;
        }
    }
    
    Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image->setContentSize(Size(w , hc - h * 0.07));
    image->setAnchorPoint(Point::ZERO);
    image->setPosition(Point(w, yg));
    this->addChild(image, 100, 100);
    
    auto move = MoveBy::create(0.25 , Point(-w, 0));
    image->runAction(move);
    
    
    sqlite3* db = NULL;
    std::string fullpath = FileUtils::getInstance()->getWritablePath();
    fullpath += "userdata.db";
    
    if (sqlite3_open(fullpath.c_str(), &db) == SQLITE_OK) {
        sqlite3_stmt *stmt = NULL;
        
        // スキルを抽出
        std::string tmpsql = "select * from user_skill_list";
        if(sqlite3_prepare_v2(db, tmpsql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
            int count = 0;
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int _id             = (int)sqlite3_column_int(stmt, 0);
                int skill_id        = (int)sqlite3_column_int(stmt, 1);
                int level           = (int)sqlite3_column_int(stmt, 2);
                int soubi_unit_id   = (int)sqlite3_column_int(stmt, 3);
                skill_list[count][0] = skill_id;
                skill_list[count][1] = soubi_unit_id;
                count++;
            }
        }
        stmt = NULL;
        
        std::string sql;
        if(!arena){
            sql = "select * from user_unit inner join user_party on user_unit._id = user_party.unit_id order by user_party.unit_order asc";
        }
        else{
            sql = "select * from user_unit inner join user_party2 on user_unit._id = user_party2.unit_id order by user_party2.unit_order asc";
        }
        if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int user_unit_id    = (int)sqlite3_column_int(stmt, 0);
                int class_type      = (int)sqlite3_column_int(stmt, 1);
                int level           = (int)sqlite3_column_int(stmt, 2);
                int class_level     = (int)sqlite3_column_int(stmt, 3);
                int hp              = (int)sqlite3_column_int(stmt, 4);
                int physical_atk    = (int)sqlite3_column_int(stmt, 5);
                int magical_atk     = (int)sqlite3_column_int(stmt, 6);
                int physical_def    = (int)sqlite3_column_int(stmt, 7);
                int magical_def     = (int)sqlite3_column_int(stmt, 8);
                int skill1          = (int)sqlite3_column_int(stmt, 9);
                int skill2          = (int)sqlite3_column_int(stmt,10);
                int class_skill     = (int)sqlite3_column_int(stmt,11);
                int zokusei         = (int)sqlite3_column_int(stmt,12);
                int unit_order      = (int)sqlite3_column_int(stmt,14);
                partyMemberUnitId[disp_count] = user_unit_id;
                
                char tmpFileName[64];
                sprintf(tmpFileName, "window_chara/window_chara%d.png", zokusei);
                Scale9Sprite* image = Scale9Sprite::create(tmpFileName, Rect(0, 0, 180, 180), Rect(50, 50, 90, 90));
                image->setContentSize(Size(w * 0.2 , w * 0.2 ));
                Size tmpSize = image->getContentSize();
                image->setPosition(Point::ZERO);
                
                Animation* charaAnimation = Animation::create();
                SpriteFrame* charaSprite[3];
                for (int x = 0; x < 3; x++) {
                    char tmpFile[64];
                    sprintf(tmpFile, "charactor/chara%d-1.png", class_type);
                    charaSprite[x] = SpriteFrame::create(tmpFile, Rect(x * 32, 0, 32, 32));
                    charaAnimation->addSpriteFrame(charaSprite[x]);
                }
                charaAnimation->setDelayPerUnit(0.3);
                RepeatForever* charaAction = RepeatForever::create(Animate::create(charaAnimation));
                Sprite* chara = Sprite::create();
                chara->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.5));
                chara->setScale(w * 0.15 / 32);
                image->addChild(chara);
                chara->runAction(charaAction);
                
                MenuItemSprite* menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::setMenu210, this, user_unit_id, 0, arena));
                menuItem->setContentSize(tmpSize);
                menuItem->setPosition(Point::ZERO);
                
                Menu* menu = Menu::create(menuItem, NULL);
                menu->setContentSize(tmpSize);
                menu->setPosition(Point( w * 0.12+ w, h * 0.68 - h * 0.12 * disp_count ));
                
                this->addChild(menu, 400+disp_count, 400+disp_count);
                
                auto move = MoveBy::create(0.25 + 0.05 * disp_count, Point( -w, 0 ));
                menu->runAction(move);
                
                
                char tmpChar[64];
                
                auto label = LabelTTF::create(LocalizedString::getLocalizedString("hp", "HP"), FONT, LEVEL_FONT_SIZE);
                label->setAnchorPoint(Point(0, 0.5));
                label->setPosition(w * 1.22, h * 0.71 - h * 0.12 * disp_count );
                label->setFontFillColor(Color3B(255, 255, 255));
                this->addChild(label, 101, 101 + 20 * disp_count);
                move = MoveBy::create(0.25 + 0.05 * disp_count, Point(-w,0));
                label->runAction(move);
                
                sprintf(tmpChar, "%d", hp);
                auto label2 = LabelTTF::create(tmpChar, FONT2, LEVEL_FONT_SIZE);
                label2->setAnchorPoint(Point(1, 0.5));
                label2->setPosition(w * 1.45, h * 0.71 - h * 0.12 * disp_count );
                label2->setFontFillColor(Color3B(255, 255, 255));
                this->addChild(label2 ,101, 102 + 20 * disp_count);
                auto move2 = MoveBy::create(0.25 + 0.05 * disp_count, Point(-w,0));
                label2->runAction(move2);
                
                auto label3 = LabelTTF::create(LocalizedString::getLocalizedString("physicalAttack", "PhysicalAtk"), FONT, LEVEL_FONT_SIZE);
                label3->setAnchorPoint(Point(0, 0.5));
                label3->setPosition(w * 1.22, h * 0.68 - h * 0.12 * disp_count );
                label3->setFontFillColor(Color3B(255, 255, 255));
                this->addChild(label3, 101, 103 + 20 * disp_count);
                auto move3 = MoveBy::create(0.25 + 0.05 * disp_count, Point(-w,0));
                label3->runAction(move3);
                
                sprintf(tmpChar, "%d", physical_atk);
                auto label4 = LabelTTF::create(tmpChar, FONT2, LEVEL_FONT_SIZE);
                label4->setAnchorPoint(Point(1, 0.5));
                label4->setPosition(w * 1.45, h * 0.68 - h * 0.12 * disp_count );
                label4->setFontFillColor(Color3B(255, 255, 255));
                this->addChild(label4 ,101, 104 + 20 * disp_count);
                auto move4 = MoveBy::create(0.25 + 0.05 * disp_count, Point(-w,0));
                label4->runAction(move4);
                
                auto label5 = LabelTTF::create(LocalizedString::getLocalizedString("magicalAttack", "MagicalAtk"), FONT, LEVEL_FONT_SIZE);
                label5->setAnchorPoint(Point(0, 0.5));
                label5->setPosition(w * 1.22, h * 0.65 - h * 0.12 * disp_count );
                label5->setFontFillColor(Color3B(255, 255, 255));
                this->addChild(label5, 101, 105 + 20 * disp_count);
                auto move5 = MoveBy::create(0.25 + 0.05 * disp_count, Point(-w,0));
                label5->runAction(move5);
                
                sprintf(tmpChar, "%d", magical_atk);
                auto label6 = LabelTTF::create(tmpChar, FONT2, LEVEL_FONT_SIZE);
                label6->setAnchorPoint(Point(1, 0.5));
                label6->setPosition(w * 1.45, h * 0.65 - h * 0.12 * disp_count );
                label6->setFontFillColor(Color3B(255, 255, 255));
                this->addChild(label6 ,101, 106 + 20 * disp_count);
                auto move6 = MoveBy::create(0.25 + 0.05 * disp_count, Point(-w,0));
                label6->runAction(move6);
                
                auto label7 = LabelTTF::create(LocalizedString::getLocalizedString("skill", "Skill"), FONT, LEVEL_FONT_SIZE);
                label7->setAnchorPoint(Point(0, 0.5));
                label7->setPosition(w * 1.5, h * 0.71- h * 0.12 * disp_count );
                label7->setFontFillColor(Color3B(255, 255, 255));
                this->addChild(label7, 101, 107 + 20 * disp_count);
                auto move7 = MoveBy::create(0.25 + 0.05 * disp_count, Point(-w,0));
                label7->runAction(move7);
                
                
                // 装備スキル
                int tmp_skill_id = 0;
                for(int j = 0; j < 64; j++){
                    if(skill_list[j][1] == user_unit_id){
                        tmp_skill_id = skill_list[j][0];
                        break;
                    }
                }
                if(tmp_skill_id != 0){
                    auto skillSprite = Sprite::create();
                    skillSprite->setSpriteFrame(SpriteFrame::create("general_img/sticon_set.png", Rect(264, 144, 24, 24)));
                    skillSprite->setScale(1.2 * w/640);
                    skillSprite->setAnchorPoint(Point(0.5, 0.5));
                    skillSprite->setPosition(Point(tmpSize.width * 0.8, tmpSize.height * 0.8));
                    image->addChild(skillSprite);
                    
                    sprintf(tmpChar, "skillName%d", tmp_skill_id);
                    auto label8 = LabelTTF::create(LocalizedString::getLocalizedString(tmpChar, tmpChar), FONT, LEVEL_FONT_SIZE);
                    label8->setAnchorPoint(Point(0, 0.5));
                    label8->setPosition(w * 1.55, h * 0.68- h * 0.12 * disp_count );
                    label8->setFontFillColor(Color3B(255, 255, 255));
                    this->addChild(label8 ,101, 108 + 20 * disp_count);
                    auto move8 = MoveBy::create(0.25 + 0.05 * disp_count, Point(-w,0));
                    label8->runAction(move8);
                    
                }
                else{
                    sprintf(tmpChar, "%d", skill1);
                    auto label8 = LabelTTF::create(LocalizedString::getLocalizedString("nosoubi", "nosoubi"), FONT, LEVEL_FONT_SIZE);
                    label8->setAnchorPoint(Point(0, 0.5));
                    label8->setPosition(w * 1.55, h * 0.68- h * 0.12 * disp_count );
                    label8->setFontFillColor(Color3B(255, 255, 255));
                    this->addChild(label8 ,101, 108 + 20 * disp_count);
                    auto move8 = MoveBy::create(0.25 + 0.05 * disp_count, Point(-w,0));
                    label8->runAction(move8);
                }
                
                if(disp_count != 0){
                    
                    auto image9 = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
                    image9->setContentSize(Size(w * 0.1 , w * 0.1 ));
                    tmpSize = image9->getContentSize();
                    image9->setPosition(Point::ZERO);
                    
                    auto label9 = LabelTTF::create(LocalizedString::getLocalizedString("up", "up"), FONT, LEVEL_FONT_SIZE);
                    label9->setFontFillColor(Color3B(255, 255, 255));
                    label9->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.5));
                    image9->addChild(label9);
                    
                    auto menuItem9 = MenuItemSprite::create(image9, image9, CC_CALLBACK_0(HelloWorld::setMenu201OrderChange, this, disp_count, -1, arena));
                    menuItem9->setContentSize(tmpSize);
                    menuItem9->setPosition(Point::ZERO);
                    
                    auto menu9 = Menu::create(menuItem9, NULL);
                    menu9->setContentSize(tmpSize);
                    menu9->setPosition(Point( w * 1.92, h * 0.70 - h * 0.12 * disp_count ));
                    
                    this->addChild(menu9, 200+disp_count * 2, 200+disp_count * 2);
                    
                    auto move9 = MoveBy::create(0.25 + 0.05 * disp_count, Point( -w, 0 ));
                    menu9->runAction(move9);
                    
                }
                
                if(disp_count < 4){
                    
                    auto image10 = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
                    image10->setContentSize(Size(w * 0.1 , w * 0.1 ));
                    tmpSize = image10->getContentSize();
                    image10->setPosition(Point::ZERO);
                    
                    auto label10 = LabelTTF::create(LocalizedString::getLocalizedString("down", "down"), FONT, LEVEL_FONT_SIZE);
                    label10->setFontFillColor(Color3B(255, 255, 255));
                    label10->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.5));
                    image10->addChild(label10);
                    
                    auto menuItem10 = MenuItemSprite::create(image10, image10, CC_CALLBACK_0(HelloWorld::setMenu201OrderChange, this, disp_count, 1, arena));
                    menuItem10->setContentSize(tmpSize);
                    menuItem10->setPosition(Point::ZERO);
                    
                    auto menu10 = Menu::create(menuItem10, NULL);
                    menu10->setContentSize(tmpSize);
                    menu10->setPosition(Point( w * 1.92, h * 0.65 - h * 0.12 * disp_count ));
                    
                    this->addChild(menu10, 201+disp_count * 2, 201+disp_count * 2);
                    
                    auto move10 = MoveBy::create(0.25 + 0.05 * disp_count, Point( -w, 0 ));
                    menu10->runAction(move10);
                    
                }
                disp_count++;
            }
        }
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);
        
        
    }
    sqlite3_close(db);
}

void HelloWorld::setMenu210(int unitId, int sortId, bool arena) // バーティー編成の変更メンバー選択
{
    arenaPartyMemberEditing = arena;
    
    pageTitleOutAndRemove(210); // edit
    removeMenu();
    
    int disp_count = 0;
    int disp_i = 0;
    int disp_j = 0;
    int partyMemberUnitId[5]; // edit
    int skill_list[64][2];
    for(int i = 0; i < 64; i++){
        for(int j = 0; j < 2; j++){
            skill_list[i][j] = 0;
        }
    }
    
    ScrollView *scrollView = ScrollView::create();
    scrollView->setViewSize(Size(w, hc));
    scrollView->setPosition(Point(0, yg));
    
    LayerColor* scrollLayer = LayerColor::create(Color4B(255, 255, 255, SCROLLVIEWOPACITY));
    scrollLayer->changeWidth(w);
    
    sqlite3* db = NULL;
    std::string fullpath = FileUtils::getInstance()->getWritablePath();
    fullpath += "userdata.db";
    
    if (sqlite3_open(fullpath.c_str(), &db) == SQLITE_OK) {
        sqlite3_stmt *stmt = NULL;
        
        // パーティーに入っているユニットを抽出
        // edit start
        std::string tmpsql;
        if(!arena){
            tmpsql = "select * from user_party ";
        }
        else{
            tmpsql = "select * from user_party2 ";
        }
        if(sqlite3_prepare_v2(db, tmpsql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
            int count = 0;
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int tmp_unit_id = (int)sqlite3_column_int(stmt, 2);
                partyMemberUnitId[count] = tmp_unit_id;
                count++;
            }
        }
        stmt = NULL;
        // edit end
        
        // スキルを抽出
        // edit start
        tmpsql = "select * from user_skill_list";
        if(sqlite3_prepare_v2(db, tmpsql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
            int count = 0;
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int _id             = (int)sqlite3_column_int(stmt, 0);
                int skill_id        = (int)sqlite3_column_int(stmt, 1);
                int level           = (int)sqlite3_column_int(stmt, 2);
                int soubi_unit_id   = (int)sqlite3_column_int(stmt, 3);
                skill_list[count][0] = skill_id;
                skill_list[count][1] = soubi_unit_id;
                count++;
            }
        }
        stmt = NULL;
        // edit end
        
        std::string sql = "select * from user_unit";
        if(sortId == 0){ sql += " order by _id asc"; }
        if(sortId == 1){ sql += " order by hp desc"; }
        if(sortId == 2){ sql += " order by physical desc"; }
        if(sortId == 3){ sql += " order by magical desc"; }
        if(sortId == 4){ sql += " order by class_type asc"; }
        if(sortId == 5){ sql += " order by zokusei asc"; }
        
        if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
            
            sqlite3_stmt * stmt_count = stmt;
            int count = 0;
            while (sqlite3_step(stmt_count) == SQLITE_ROW) {
                count++;
            }
            float scrollLayerHeight =  (w * 0.2) * ( count - count % 5 + 5 ) / 5 + h * 0.1 ;
            scrollLayer->changeHeight(scrollLayerHeight);
            scrollLayer->setAnchorPoint(Point(0, 1));
            scrollLayer->setPosition(Point::ZERO);
            
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int user_unit_id = (int)sqlite3_column_int(stmt, 0);
                int class_type      = (int)sqlite3_column_int(stmt, 1);
                int level           = (int)sqlite3_column_int(stmt, 2);
                int class_level     = (int)sqlite3_column_int(stmt, 3);
                int hp              = (int)sqlite3_column_int(stmt, 4);
                int physical_atk    = (int)sqlite3_column_int(stmt, 5);
                int magical_atk     = (int)sqlite3_column_int(stmt, 6);
                int physical_def    = (int)sqlite3_column_int(stmt, 7);
                int magical_def     = (int)sqlite3_column_int(stmt, 8);
                int skill1          = (int)sqlite3_column_int(stmt, 9);
                int skill2          = (int)sqlite3_column_int(stmt,10);
                int class_skill     = (int)sqlite3_column_int(stmt,11);
                int zokusei         = (int)sqlite3_column_int(stmt,12);
                
                char tmpFileName[64];
                sprintf(tmpFileName, "window_chara/window_chara%d.png", zokusei);
                Scale9Sprite* image = Scale9Sprite::create(tmpFileName, Rect(0, 0, 180, 180), Rect(50, 50, 90, 90));
                image->setContentSize(Size(w * 0.2 , w * 0.2 ));
                Size tmpSize = image->getContentSize();
                image->setPosition(Point::ZERO);
                
                Animation* charaAnimation = Animation::create();
                SpriteFrame* charaSprite[4];
                for (int x = 0; x < 3; x++) {
                    char tmpFile[64];
                    sprintf(tmpFile, "charactor/chara%d-1.png", class_type);
                    charaSprite[x] = SpriteFrame::create(tmpFile, Rect(x * 32, 0, 32, 32));
                    charaAnimation->addSpriteFrame(charaSprite[x]);
                }
                charaAnimation->setDelayPerUnit(0.3);
                RepeatForever* charaAction = RepeatForever::create(Animate::create(charaAnimation));
                Sprite* chara = Sprite::create();
                chara->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.5));
                chara->setScale(w * 0.15 / 32);
                image->addChild(chara);
                chara->runAction(charaAction);
                
                // スキル
                int tmp_skill_id = 0;
                for(int j = 0; j < 64; j++){
                    if(skill_list[j][1] == user_unit_id){
                        tmp_skill_id = skill_list[j][0];
                        break;
                    }
                }
                if(tmp_skill_id != 0){
                    auto skillSprite = Sprite::create();
                    skillSprite->setSpriteFrame(SpriteFrame::create("general_img/sticon_set.png", Rect(264, 144, 24, 24)));
                    skillSprite->setScale(1.2 * w/640);
                    skillSprite->setAnchorPoint(Point(0.5, 0.5));
                    skillSprite->setPosition(Point(tmpSize.width * 0.8, tmpSize.height * 0.8));
                    image->addChild(skillSprite);
                }
                
                
                // レベル表示
                char tmpChar[16];
                sprintf(tmpChar, "Lv %d", level);
                auto label = LabelTTF::create(tmpChar, FONT2, LEVEL_FONT_SIZE);
                label->setFontFillColor(Color3B(255, 255, 255));
                label->enableStroke(Color3B(0, 0, 0), 3);
                label->setAnchorPoint(Point(0.5, 0.2));
                label->setPosition(Point(tmpSize.width * 0.5, 0));
                image->addChild(label);
                
                // パーティーメンバー表示
                if(user_unit_id == partyMemberUnitId[0] || user_unit_id == partyMemberUnitId[1] || user_unit_id == partyMemberUnitId[2] || user_unit_id == partyMemberUnitId[3] || user_unit_id == partyMemberUnitId[4] ){ // edit
                    
                    auto delay0 = DelayTime::create(0.0);
                    auto fadein = FadeIn::create(0.25);
                    auto delay1 = DelayTime::create(1.5);
                    auto fadeout = FadeOut::create(0.25);
                    auto delay2 = DelayTime::create(2.0);
                    label->runAction(Sequence::create(Hide::create(), delay0, Show::create(), NULL));
                    label->runAction(RepeatForever::create(Sequence::create(delay0, fadein, delay1, fadeout, delay2, NULL)));
                    
                    LabelTTF* label2;
                    if(!arena){
                        label2 = LabelTTF::create("Party", FONT2, LEVEL_FONT_SIZE);
                        label2->setFontFillColor(Color3B(255, 255, 0));
                        label2->enableStroke(Color3B(0, 0, 0), 3);
                        label2->setAnchorPoint(Point(0.5, 0.2));
                        label2->setPosition(Point(tmpSize.width * 0.5, 0));
                        image->addChild(label2);
                    }
                    else{
                        label2 = LabelTTF::create("Arena", FONT2, LEVEL_FONT_SIZE);
                        label2->setFontFillColor(Color3B(0, 255, 255));
                        label2->enableStroke(Color3B(0, 0, 0), 3);
                        label2->setAnchorPoint(Point(0.5, 0.2));
                        label2->setPosition(Point(tmpSize.width * 0.5, 0));
                        image->addChild(label2);
                    }
                    
                    delay0 = DelayTime::create(2.0);
                    fadein = FadeIn::create(0.25);
                    delay1 = DelayTime::create(1.5);
                    fadeout = FadeOut::create(0.25);
                    delay2 = DelayTime::create(0.0);
                    label2->runAction(Sequence::create(Hide::create(), delay0, Show::create(), NULL));
                    label2->runAction(RepeatForever::create(Sequence::create(delay0, fadein, delay1, fadeout, delay2, NULL)));
                    
                } // edit
                
                if(user_unit_id != partyMemberUnitId[0] && user_unit_id != partyMemberUnitId[1] && user_unit_id != partyMemberUnitId[2] && user_unit_id != partyMemberUnitId[3] && user_unit_id != partyMemberUnitId[4] ){ // edit
                    
                    MenuItemSprite* menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::setMenu210selected, this, unitId, user_unit_id, arena));
                    menuItem->setContentSize(tmpSize);
                    menuItem->setPosition(Point::ZERO);
                    
                    Menu* menu = Menu::create2(menuItem, NULL);
                    menu->setContentSize(tmpSize);
                    menu->setPosition(Point( w * (0.1 + 0.2 * disp_j) + w, scrollLayerHeight - h * 0.1 - w * (0.1 + 0.2 * disp_i) ));
                    
                    scrollLayer->addChild(menu, 200+disp_count, 200+disp_count);
                    
                    auto move = MoveBy::create(0.25 + 0.05 * disp_i, Point(-w, 0));
                    menu->runAction(move);
                } // edit
                else{ // edit start
                    image->setPosition(Point( w * (0.1 + 0.2 * disp_j) + w, scrollLayerHeight - h * 0.1 - w * (0.1 + 0.2 * disp_i) ));
                    scrollLayer->addChild(image, 200+disp_count, 200+disp_count);
                    auto move = MoveBy::create(0.25 + 0.05 * disp_i, Point(-w, 0));
                    image->runAction(move);
                    
                    Scale9Sprite* image2 = Scale9Sprite::create("noselectable.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
                    image2->setContentSize(Size(w * 0.2 , w * 0.2 ));
                    image2->setOpacity(128);
                    image2->setPosition(Point( w * (0.1 + 0.2 * disp_j) + w, scrollLayerHeight - h * 0.1 - w * (0.1 + 0.2 * disp_i) ));
                    scrollLayer->addChild(image2, 400+disp_count, 400+disp_count);
                    auto move2 = MoveBy::create(0.25 + 0.05 * disp_i, Point(-w, 0));
                    image2->runAction(move2);
                    
                } // edit end
                
                disp_count++;
                disp_j++;
                if(disp_j == 5){
                    disp_i++;
                    disp_j = 0;
                }
            }
        }
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    
    scrollView->setContentOffset(Point(0, hc - scrollLayer->getContentSize().height));
    scrollView->setContentSize(scrollLayer->getContentSize());
    scrollView->addChild(scrollLayer, 100, 101);
    
    this->addChild(scrollView, 100, 100);
    
    
    // ソート用ボタン
    Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image->setContentSize(Size(w * 0.2 , h * 0.08 ));
    Size tmpSize = image->getContentSize();
    image->setPosition(Point::ZERO);
    
    char tmpChar[64];
    sprintf(tmpChar, "sort%d", sortId);
    auto label = LabelTTF::create(LocalizedString::getLocalizedString(tmpChar, tmpChar), FONT, LEVEL_FONT_SIZE);
    label->setFontFillColor(Color3B(255, 255, 255));
    label->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.5));
    image->addChild(label);
    
    int nextSortId = sortId + 1;
    if (nextSortId == UNITSORTNUM) { nextSortId = 0; }
    
    MenuItemSprite* menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::setMenu210, this, unitId, nextSortId, arena)); // edit
    menuItem->setContentSize(tmpSize);
    menuItem->setPosition(Point::ZERO);
    
    Menu* menu = Menu::create(menuItem, NULL);
    menu->setContentSize(tmpSize);
    menu->setPosition(Point( w * 1.9, h * 0.81 ));
    
    this->addChild(menu, 105, 105);
    
    auto move = MoveBy::create(0.25, Point(-w, 0));
    menu->runAction(move);
    
}

void HelloWorld::setMenu210selected(int unitId, int unitId2, bool arena)
{
    ScrollView* check = (ScrollView*)this->getChildByTag(100);
    if(!(check->touchEndPointCheck())){ return; }
    
    Point touchEndPoint = check->getTouchEndPoint();
    Node* check2 = (Node*)this->getChildByTag(11000);
    if (check2->getBoundingBox().containsPoint(touchEndPoint)) {
        Node* check3 = (Node*)check2->getChildByTag(11001);
        Rect check2rect = check2->getBoundingBox();
        Rect check3rect = check3->getBoundingBox();
        if( touchEndPoint.x > check2rect.origin.x + check3rect.origin.x - check3rect.size.width / 2
           && touchEndPoint.x < check2rect.origin.x + check3rect.origin.x + check3rect.size.width / 2
           && touchEndPoint.y > check2rect.origin.y + check3rect.origin.y - check3rect.size.height / 2
           && touchEndPoint.y < check2rect.origin.y + check3rect.origin.y + check3rect.size.height / 2
           ){
            // 戻る
            if(!arena){
                globalMenu(201);
            }
            else{
                globalMenu(202);
            }
        }
        return;
    }
    
    sqlite3* db = NULL;
    std::string fullpath = FileUtils::getInstance()->getWritablePath();
    fullpath += "userdata.db";
    
    if (sqlite3_open(fullpath.c_str(), &db) == SQLITE_OK) {
        char tmpSql[2048];
        if(!arena){
            sprintf(tmpSql, "update user_party set unit_id = %d where unit_id = %d", unitId2, unitId);
        }
        else{
            sprintf(tmpSql, "update user_party2 set unit_id = %d where unit_id = %d", unitId2, unitId);
        }
        std::string sql = tmpSql;
        sqlite3_exec(db, sql.c_str(), NULL, NULL, NULL);
    }
    sqlite3_close(db);
    
    // パーティーをオンラインDB登録。
    searchOtherParties();

    if(!arena){
        menuTapped(201);
    }
    else{
        menuTapped(202);
    }
}



void HelloWorld::setMenu201OrderChange(int order, int upOrDown, bool arena)
{
    if(partyOrderChanging) return;
    
    partyOrderChanging = true;
    
    // -1 : up, 1 : down
    sqlite3* db = NULL;
    std::string fullpath = FileUtils::getInstance()->getWritablePath();
    fullpath += "userdata.db";
    
    if (sqlite3_open(fullpath.c_str(), &db) == SQLITE_OK) {
        
        char tmpSql[2048];
        std::string sql;
        if(!arena){
            sprintf(tmpSql, "update user_party set unit_order = -1 where unit_order = %d", order);
        }
        else{
            sprintf(tmpSql, "update user_party2 set unit_order = -1 where unit_order = %d", order);
        }
        sql = tmpSql;
        sqlite3_exec(db, sql.c_str(), NULL, NULL, NULL);
        
        if(!arena){
            sprintf(tmpSql, "update user_party set unit_order = %d where unit_order = %d", order, order + upOrDown);
        }
        else{
            sprintf(tmpSql, "update user_party2 set unit_order = %d where unit_order = %d", order, order + upOrDown);
        }
        sql = tmpSql;
        sqlite3_exec(db, sql.c_str(), NULL, NULL, NULL);
        
        if(!arena){
            sprintf(tmpSql, "update user_party set unit_order = %d where unit_order = -1", order + upOrDown);
        }
        else{
            sprintf(tmpSql, "update user_party2 set unit_order = %d where unit_order = -1", order + upOrDown);
        }
        sql = tmpSql;
        sqlite3_exec(db, sql.c_str(), NULL, NULL, NULL);
        
    }
    sqlite3_close(db);
    
    partyOrderChanging = false;
    
    // パーティーをオンラインDB登録。
    searchOtherParties();
    
    if(!arena){
        menuTapped(201);
    }
    else{
        menuTapped(202);
    }
    
}


void HelloWorld::setMenu202(int sortId) // ユニット一覧
{
    pageTitleOutAndRemove(202);
    removeMenu();
    
    int disp_count = 0;
    int disp_i = 0;
    int disp_j = 0;
    int partyMemberUnitId[5]; // edit
    int arenaMemberUnitId[5]; // edit
    int skill_list[64][2];
    for(int i = 0; i < 64; i++){
        for(int j = 0; j < 2; j++){
            skill_list[i][j] = 0;
        }
    }
    
    ScrollView *scrollView = ScrollView::create();
    scrollView->setViewSize(Size(w, hc));
    scrollView->setPosition(Point(0, yg));
    if(scrollView->isSwallowsTouches()){
        CCLOG("swallowTouch true");
    }
    //scrollView->setSwallowsTouches(false);
    
    LayerColor* scrollLayer = LayerColor::create(Color4B(255, 255, 255, SCROLLVIEWOPACITY));
    scrollLayer->changeWidth(w);
    
    sqlite3* db = NULL;
    std::string fullpath = FileUtils::getInstance()->getWritablePath();
    fullpath += "userdata.db";
    
    if (sqlite3_open(fullpath.c_str(), &db) == SQLITE_OK) {
        sqlite3_stmt *stmt = NULL;
        
        // パーティーに入っているユニットを抽出
        // edit start
        std::string tmpsql = "select * from user_party ";
        if(sqlite3_prepare_v2(db, tmpsql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
            int count = 0;
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int tmp_unit_id = (int)sqlite3_column_int(stmt, 2);
                partyMemberUnitId[count] = tmp_unit_id;
                count++;
            }
        }
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);
        stmt = NULL;
        // edit end
        
        // パーティーに入っているユニットを抽出
        // edit start
        tmpsql = "select * from user_party2 ";
        if(sqlite3_prepare_v2(db, tmpsql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
            int count = 0;
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int tmp_unit_id = (int)sqlite3_column_int(stmt, 2);
                arenaMemberUnitId[count] = tmp_unit_id;
                count++;
            }
        }
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);
        stmt = NULL;
        // edit end

        // スキルを抽出
        // edit start
        tmpsql = "select * from user_skill_list";
        if(sqlite3_prepare_v2(db, tmpsql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
            int count = 0;
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int _id             = (int)sqlite3_column_int(stmt, 0);
                int skill_id        = (int)sqlite3_column_int(stmt, 1);
                int level           = (int)sqlite3_column_int(stmt, 2);
                int soubi_unit_id   = (int)sqlite3_column_int(stmt, 3);
                skill_list[count][0] = skill_id;
                skill_list[count][1] = soubi_unit_id;
                count++;
            }
        }
        stmt = NULL;
        // edit end
        
        
        std::string sql = "select * from user_unit";
        if(sortId == 0){ sql += " order by _id asc"; }
        if(sortId == 1){ sql += " order by hp desc"; }
        if(sortId == 2){ sql += " order by physical desc"; }
        if(sortId == 3){ sql += " order by magical desc"; }
        if(sortId == 4){ sql += " order by class_type asc"; }
        if(sortId == 5){ sql += " order by zokusei asc"; }
        if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
            
            sqlite3_stmt * stmt_count = stmt;
            int count = 0;
            while (sqlite3_step(stmt_count) == SQLITE_ROW) {
                count++;
            }
            float scrollLayerHeight =  (w * 0.2) * ( count - count % 5 + 5 ) / 5 + h * 0.1 ;
            scrollLayer->changeHeight(scrollLayerHeight);
            scrollLayer->setAnchorPoint(Point(0, 1));
            scrollLayer->setPosition(Point::ZERO);
            
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int user_unit_id = (int)sqlite3_column_int(stmt, 0);
                int class_type      = (int)sqlite3_column_int(stmt, 1);
                int level           = (int)sqlite3_column_int(stmt, 2);
                int class_level     = (int)sqlite3_column_int(stmt, 3);
                int hp              = (int)sqlite3_column_int(stmt, 4);
                int physical_atk    = (int)sqlite3_column_int(stmt, 5);
                int magical_atk     = (int)sqlite3_column_int(stmt, 6);
                int physical_def    = (int)sqlite3_column_int(stmt, 7);
                int magical_def     = (int)sqlite3_column_int(stmt, 8);
                int skill1          = (int)sqlite3_column_int(stmt, 9);
                int skill2          = (int)sqlite3_column_int(stmt,10);
                int class_skill     = (int)sqlite3_column_int(stmt,11);
                int zokusei         = (int)sqlite3_column_int(stmt,12);
                
                char tmpFileName[64];
                sprintf(tmpFileName, "window_chara/window_chara%d.png", zokusei);
                Scale9Sprite* image = Scale9Sprite::create(tmpFileName, Rect(0, 0, 180, 180), Rect(50, 50, 90, 90));
                image->setContentSize(Size(w * 0.2 , w * 0.2 ));
                Size tmpSize = image->getContentSize();
                image->setPosition(Point::ZERO);
                
                Animation* charaAnimation = Animation::create();
                SpriteFrame* charaSprite[4];
                int i = 0;
                for (x = 0; x < 3; x++) {
                    char tmpFile[64];
                    sprintf(tmpFile, "charactor/chara%d-1.png", class_type);
                    charaSprite[i] = SpriteFrame::create(tmpFile, Rect(x * 32, 0, 32, 32));
                    charaAnimation->addSpriteFrame(charaSprite[i]);
                    i++;
                }
                charaAnimation->setDelayPerUnit(0.3);
                RepeatForever* charaAction = RepeatForever::create(Animate::create(charaAnimation));
                Sprite* chara = Sprite::create();
                chara->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.5));
                chara->setScale(w * 0.15 / 32);
                image->addChild(chara);
                chara->runAction(charaAction);
                
                // スキル
                int tmp_skill_id = 0;
                for(int j = 0; j < 64; j++){
                    if(skill_list[j][1] == user_unit_id){
                        tmp_skill_id = skill_list[j][0];
                        break;
                    }
                }
                if(tmp_skill_id != 0){
                    auto skillSprite = Sprite::create();
                    skillSprite->setSpriteFrame(SpriteFrame::create("general_img/sticon_set.png", Rect(264, 144, 24, 24)));
                    skillSprite->setScale(1.2 * w/640);
                    skillSprite->setAnchorPoint(Point(0.5, 0.5));
                    skillSprite->setPosition(Point(tmpSize.width * 0.8, tmpSize.height * 0.8));
                    image->addChild(skillSprite);
                }
                
                MenuItemSprite* menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::setMenu220_unitDetail, this, user_unit_id, 0, true));
                menuItem->setContentSize(tmpSize);
                menuItem->setPosition(Point::ZERO);
                
                Menu* menu = Menu::create2(menuItem, NULL);
                menu->setContentSize(tmpSize);
                menu->setPosition(Point( w * (0.1 + 0.2 * disp_j) + w, scrollLayerHeight - h * 0.1 - w * (0.1 + 0.2 * disp_i) ));
                
                scrollLayer->addChild(menu, 200+disp_count, 200+disp_count);
                
                auto move = MoveBy::create(0.25 + 0.05 * disp_i, Point(-w, 0));
                menu->runAction(move);
                
                // レベル表示
                char tmpChar[16];
                sprintf(tmpChar, "Lv %d", level);
                auto label = LabelTTF::create(tmpChar, FONT2, LEVEL_FONT_SIZE);
                label->setFontFillColor(Color3B(255, 255, 255));
                label->enableStroke(Color3B(0, 0, 0), 3);
                label->setAnchorPoint(Point(0.5, 0.2));
                label->setPosition(Point(tmpSize.width * 0.5, 0));
                image->addChild(label);
                
                // パーティーメンバー表示
                if((user_unit_id == partyMemberUnitId[0] || user_unit_id == partyMemberUnitId[1] || user_unit_id == partyMemberUnitId[2] || user_unit_id == partyMemberUnitId[3] || user_unit_id == partyMemberUnitId[4]) &&
                   (user_unit_id == arenaMemberUnitId[0] || user_unit_id == arenaMemberUnitId[1] || user_unit_id == arenaMemberUnitId[2] || user_unit_id == arenaMemberUnitId[3] || user_unit_id == arenaMemberUnitId[4])){ // edit
                    auto delay0 = DelayTime::create(0.0);
                    auto fadein = FadeIn::create(0.25);
                    auto delay1 = DelayTime::create(1.5);
                    auto fadeout = FadeOut::create(0.25);
                    auto delay2 = DelayTime::create(2.0);
                    label->runAction(Sequence::create(Hide::create(), delay0, Show::create(), NULL));
                    label->runAction(RepeatForever::create(Sequence::create(delay0, fadein, delay1, fadeout, delay2, NULL)));
                    
                    auto label2 = LabelTTF::create("Pty&Arn", FONT2, LEVEL_FONT_SIZE);
                    label2->setFontFillColor(Color3B(255, 0, 255));
                    label2->enableStroke(Color3B(0, 0, 0), 3);
                    label2->setAnchorPoint(Point(0.5, 0.2));
                    label2->setPosition(Point(tmpSize.width * 0.5, 0));
                    image->addChild(label2);
                    
                    delay0 = DelayTime::create(2.0);
                    fadein = FadeIn::create(0.25);
                    delay1 = DelayTime::create(1.5);
                    fadeout = FadeOut::create(0.25);
                    delay2 = DelayTime::create(0.0);
                    label2->runAction(Sequence::create(Hide::create(), delay0, Show::create(), NULL));
                    label2->runAction(RepeatForever::create(Sequence::create(delay0, fadein, delay1, fadeout, delay2, NULL)));
                }
                else if(user_unit_id == partyMemberUnitId[0] || user_unit_id == partyMemberUnitId[1] || user_unit_id == partyMemberUnitId[2] || user_unit_id == partyMemberUnitId[3] || user_unit_id == partyMemberUnitId[4] ){ // edit
                    auto delay0 = DelayTime::create(0.0);
                    auto fadein = FadeIn::create(0.25);
                    auto delay1 = DelayTime::create(1.5);
                    auto fadeout = FadeOut::create(0.25);
                    auto delay2 = DelayTime::create(2.0);
                    label->runAction(Sequence::create(Hide::create(), delay0, Show::create(), NULL));
                    label->runAction(RepeatForever::create(Sequence::create(delay0, fadein, delay1, fadeout, delay2, NULL)));
                    
                    auto label2 = LabelTTF::create("Party", FONT2, LEVEL_FONT_SIZE);
                    label2->setFontFillColor(Color3B(255, 255, 0));
                    label2->enableStroke(Color3B(0, 0, 0), 3);
                    label2->setAnchorPoint(Point(0.5, 0.2));
                    label2->setPosition(Point(tmpSize.width * 0.5, 0));
                    image->addChild(label2);
                    
                    delay0 = DelayTime::create(2.0);
                    fadein = FadeIn::create(0.25);
                    delay1 = DelayTime::create(1.5);
                    fadeout = FadeOut::create(0.25);
                    delay2 = DelayTime::create(0.0);
                    label2->runAction(Sequence::create(Hide::create(), delay0, Show::create(), NULL));
                    label2->runAction(RepeatForever::create(Sequence::create(delay0, fadein, delay1, fadeout, delay2, NULL)));
                }
                else if(user_unit_id == arenaMemberUnitId[0] || user_unit_id == arenaMemberUnitId[1] || user_unit_id == arenaMemberUnitId[2] || user_unit_id == arenaMemberUnitId[3] || user_unit_id == arenaMemberUnitId[4] ){ // edit
                    auto delay0 = DelayTime::create(0.0);
                    auto fadein = FadeIn::create(0.25);
                    auto delay1 = DelayTime::create(1.5);
                    auto fadeout = FadeOut::create(0.25);
                    auto delay2 = DelayTime::create(2.0);
                    label->runAction(Sequence::create(Hide::create(), delay0, Show::create(), NULL));
                    label->runAction(RepeatForever::create(Sequence::create(delay0, fadein, delay1, fadeout, delay2, NULL)));
                    
                    auto label2 = LabelTTF::create("Arena", FONT2, LEVEL_FONT_SIZE);
                    label2->setFontFillColor(Color3B(0, 255, 255));
                    label2->enableStroke(Color3B(0, 0, 0), 3);
                    label2->setAnchorPoint(Point(0.5, 0.2));
                    label2->setPosition(Point(tmpSize.width * 0.5, 0));
                    image->addChild(label2);
                    
                    delay0 = DelayTime::create(2.0);
                    fadein = FadeIn::create(0.25);
                    delay1 = DelayTime::create(1.5);
                    fadeout = FadeOut::create(0.25);
                    delay2 = DelayTime::create(0.0);
                    label2->runAction(Sequence::create(Hide::create(), delay0, Show::create(), NULL));
                    label2->runAction(RepeatForever::create(Sequence::create(delay0, fadein, delay1, fadeout, delay2, NULL)));
                }
                
                disp_count++;
                disp_j++;
                if(disp_j == 5){
                    disp_i++;
                    disp_j = 0;
                }
            }
        }
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    
    scrollView->setContentOffset(Point(0, hc - scrollLayer->getContentSize().height));
    scrollView->setContentSize(scrollLayer->getContentSize());
    scrollView->addChild(scrollLayer, 100, 101);
    
    this->addChild(scrollView, 100, 100);
    

    // ソート用ボタン
    Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image->setContentSize(Size(w * 0.2 , h * 0.08 ));
    Size tmpSize = image->getContentSize();
    image->setPosition(Point::ZERO);
    
    char tmpChar[64];
    sprintf(tmpChar, "sort%d", sortId);
    auto label = LabelTTF::create(LocalizedString::getLocalizedString(tmpChar, tmpChar), FONT, LEVEL_FONT_SIZE);
    label->setFontFillColor(Color3B(255, 255, 255));
    label->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.5));
    image->addChild(label);
    
    int nextSortId = sortId + 1;
    if (nextSortId == UNITSORTNUM) { nextSortId = 0; }
    
    MenuItemSprite* menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::setMenu202, this, nextSortId));
    menuItem->setContentSize(tmpSize);
    menuItem->setPosition(Point::ZERO);
    
    Menu* menu = Menu::create(menuItem, NULL);
    menu->setContentSize(tmpSize);
    menu->setPosition(Point( w * 1.9, h * 0.81 ));
    
    this->addChild(menu, 105, 105);
    
    auto move = MoveBy::create(0.25, Point(-w, 0));
    menu->runAction(move);
    
}

void HelloWorld::setMenu220_unitDetail(int user_unit_id, int hyoujiMenu, bool scrollViewCheck)// ユニット詳細
{
    ScrollView* check = (ScrollView*)this->getChildByTag(100);
    if(scrollViewCheck == true){// scrollViewがない画面から飛んでくる場合も考慮して判定
        if(!(check->touchEndPointCheck())){ return; }
        
        Point touchEndPoint = check->getTouchEndPoint();
        Node* check2 = (Node*)this->getChildByTag(11000);
        if (check2->getBoundingBox().containsPoint(touchEndPoint)) {
            Node* check3 = (Node*)check2->getChildByTag(11001);
            Rect check2rect = check2->getBoundingBox();
            Rect check3rect = check3->getBoundingBox();
            if( touchEndPoint.x > check2rect.origin.x + check3rect.origin.x - check3rect.size.width / 2
               && touchEndPoint.x < check2rect.origin.x + check3rect.origin.x + check3rect.size.width / 2
               && touchEndPoint.y > check2rect.origin.y + check3rect.origin.y - check3rect.size.height / 2
               && touchEndPoint.y < check2rect.origin.y + check3rect.origin.y + check3rect.size.height / 2
               ){
                // 戻る
                globalMenu(200);
            }
            return;
        }
    }
    
    pageTitleOutAndRemove(220);
    removeMenu();
    
    sqlite3* db = NULL;
    std::string fullpath = FileUtils::getInstance()->getWritablePath();
    fullpath += "userdata.db";
    
    if (sqlite3_open(fullpath.c_str(), &db) == SQLITE_OK) {
        sqlite3_stmt *stmt = NULL;
        
        char tmpChar[64];
        sprintf(tmpChar, "%d", user_unit_id);
        std::string sql = "select * from user_unit where _id = ";
        sql += tmpChar;
        if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
            sqlite3_step(stmt);
            int class_type      = (int)sqlite3_column_int(stmt, 1);
            int level           = (int)sqlite3_column_int(stmt, 2);
            int class_level     = (int)sqlite3_column_int(stmt, 3);
            int hp              = (int)sqlite3_column_int(stmt, 4);
            int physical_atk    = (int)sqlite3_column_int(stmt, 5);
            int magical_atk     = (int)sqlite3_column_int(stmt, 6);
            int physical_def    = (int)sqlite3_column_int(stmt, 7);
            int magical_def     = (int)sqlite3_column_int(stmt, 8);
            int skill1          = (int)sqlite3_column_int(stmt, 9);
            int skill2          = (int)sqlite3_column_int(stmt,10);
            int class_skill     = (int)sqlite3_column_int(stmt,11);
            int zokusei         = (int)sqlite3_column_int(stmt,12);
            
            
            
            CCLOG("zokusei %d", zokusei);
            char tmpFileName[64];
            sprintf(tmpFileName, "window_chara/window_chara%d.png", zokusei);
            Scale9Sprite* image = Scale9Sprite::create(tmpFileName, Rect(0, 0, 180, 180), Rect(50, 50, 90, 90));
            //    Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
            image->setContentSize(Size(w * 0.9 , h * 0.6));
            Size tmpSize = image->getContentSize();
            //image->setPosition(Point(w * 1.5, h * 0.45));
            image->setPosition(Point::ZERO);
            //    this->addChild(image, 100, 100);
            
            MenuItemSprite* menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::setMenu202, this, 0));
            menuItem->setContentSize(tmpSize);
            menuItem->setPosition(Point::ZERO);
            
            Menu* menu = Menu::create(menuItem, NULL);
            menu->setContentSize(tmpSize);
            menu->setPosition(Point( w * 1.5, h * 0.45 ));
            this->addChild(menu, 100, 100);
            
            auto move = MoveTo::create(0.25, Point(w * 0.5, h * 0.45));
            menu->runAction(move);
            
            auto label = LabelTTF::create(LocalizedString::getLocalizedString("unitType", "Type"), FONT, LEVEL_FONT_SIZE);
            label->setAnchorPoint(Point(0, 0.5));
            label->setPosition(w * 0.05, h * 0.55);
            label->setFontFillColor(Color3B(255, 255, 255));
            image->addChild(label, 101, 101);
            
            sprintf(tmpChar, "type%d", class_type);
            auto label2 = LabelTTF::create(LocalizedString::getLocalizedString(tmpChar, tmpChar), FONT, LEVEL_FONT_SIZE);
            label2->setAnchorPoint(Point(1, 0.5));
            label2->setPosition(w * 0.4, h * 0.55);
            label2->setFontFillColor(Color3B(255, 255, 255));
            image->addChild(label2 ,101, 101);
            
            auto label3 = LabelTTF::create(LocalizedString::getLocalizedString("level", "Lv"), FONT, LEVEL_FONT_SIZE);
            label3->setAnchorPoint(Point(0, 0.5));
            label3->setPosition(w * 0.05, h * 0.52);
            label3->setFontFillColor(Color3B(255, 255, 255));
            image->addChild(label3, 101, 101);
            
            sprintf(tmpChar, "%d", level);
            auto label4 = LabelTTF::create(tmpChar, FONT2, LEVEL_FONT_SIZE);
            label4->setAnchorPoint(Point(1, 0.5));
            label4->setPosition(w * 0.4, h * 0.52);
            label4->setFontFillColor(Color3B(255, 255, 255));
            image->addChild(label4 ,101, 101);
            
            auto label5 = LabelTTF::create(LocalizedString::getLocalizedString("classLevel", "Class Lv"), FONT, LEVEL_FONT_SIZE);
            label5->setAnchorPoint(Point(0, 0.5));
            label5->setPosition(w * 0.05, h * 0.49);
            label5->setFontFillColor(Color3B(255, 255, 255));
            image->addChild(label5, 101, 101);
            
            sprintf(tmpChar, "%d", class_level);
            auto label6 = LabelTTF::create(tmpChar, FONT2, LEVEL_FONT_SIZE);
            label6->setAnchorPoint(Point(1, 0.5));
            label6->setPosition(w * 0.4, h * 0.49);
            label6->setFontFillColor(Color3B(255, 255, 255));
            image->addChild(label6 ,101, 101);
            
            auto label7 = LabelTTF::create(LocalizedString::getLocalizedString("hp", "HP"), FONT, LEVEL_FONT_SIZE);
            label7->setAnchorPoint(Point(0, 0.5));
            label7->setPosition(w * 0.05, h * 0.46);
            label7->setFontFillColor(Color3B(255, 255, 255));
            image->addChild(label7, 101, 101);
            
            sprintf(tmpChar, "%d", hp);
            auto label8 = LabelTTF::create(tmpChar, FONT2, LEVEL_FONT_SIZE);
            label8->setAnchorPoint(Point(1, 0.5));
            label8->setPosition(w * 0.4, h * 0.46);
            label8->setFontFillColor(Color3B(255, 255, 255));
            image->addChild(label8 ,101, 101);
            
            auto label9 = LabelTTF::create(LocalizedString::getLocalizedString("physicalAttack", "PhysicalAtk"), FONT, LEVEL_FONT_SIZE);
            label9->setAnchorPoint(Point(0, 0.5));
            label9->setPosition(w * 0.05, h * 0.43);
            label9->setFontFillColor(Color3B(255, 255, 255));
            image->addChild(label9, 101, 101);
            
            sprintf(tmpChar, "%d", physical_atk);
            auto label10 = LabelTTF::create(tmpChar, FONT2, LEVEL_FONT_SIZE);
            label10->setAnchorPoint(Point(1, 0.5));
            label10->setPosition(w * 0.4, h * 0.43);
            label10->setFontFillColor(Color3B(255, 255, 255));
            image->addChild(label10 ,101, 101);
            
            auto label11 = LabelTTF::create(LocalizedString::getLocalizedString("magicalAttack", "MagicalAtk"), FONT, LEVEL_FONT_SIZE);
            label11->setAnchorPoint(Point(0, 0.5));
            label11->setPosition(w * 0.05, h * 0.4);
            label11->setFontFillColor(Color3B(255, 255, 255));
            image->addChild(label11, 101, 101);
            
            sprintf(tmpChar, "%d", magical_atk);
            auto label12 = LabelTTF::create(tmpChar, FONT2, LEVEL_FONT_SIZE);
            label12->setAnchorPoint(Point(1, 0.5));
            label12->setPosition(w * 0.4, h * 0.4);
            label12->setFontFillColor(Color3B(255, 255, 255));
            image->addChild(label12 ,101, 101);
            
            // キャラクターイラスト
            Animation* charaAnimation = Animation::create();
            SpriteFrame* charaSprite[4];
            for (int x = 0; x < 3; x++) {
                char tmpFile[64];
                sprintf(tmpFile, "charactor/chara%d-1.png", class_type);
                charaSprite[x] = SpriteFrame::create(tmpFile, Rect(x * 32, 0, 32, 32));
                charaAnimation->addSpriteFrame(charaSprite[x]);
            }
            charaAnimation->setDelayPerUnit(0.3);
            RepeatForever* charaAction = RepeatForever::create(Animate::create(charaAnimation));
            Sprite* chara = Sprite::create();
            chara->setPosition(Point(tmpSize.width * 0.7, tmpSize.height * 0.8));
            chara->setScale(w * 0.25 / 32);
            image->addChild(chara);
            chara->runAction(charaAction);
            
            
            ///////////////////////////////////
            ///////////////////////////////////
            ///////////////////////////////////
            //  ここから操作ボタン配置
            ///////////////////////////////////
            ///////////////////////////////////
            ///////////////////////////////////
            
            // 強化ボタン
            Scale9Sprite* image2 = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
            image2->setContentSize(Size(w * 0.3 , h * 0.08 ));
            Size tmpSize2 = image2->getContentSize();
            image2->setPosition(Point::ZERO);
            
            auto label20 = LabelTTF::create(LocalizedString::getLocalizedString("kyouka", "kyouka"), FONT, LEVEL_FONT_SIZE);
            label20->setFontFillColor(Color3B(255, 255, 255));
            label20->setPosition(Point(tmpSize2.width * 0.5, tmpSize2.height * 0.5));
            image2->addChild(label20, 201, 201);
            
            MenuItemSprite* menuItem2 = MenuItemSprite::create(image2, image2, CC_CALLBACK_0(HelloWorld::setMenu220_unitDetail_kyoukaUIin, this, user_unit_id, image));
            menuItem2->setContentSize(tmpSize2);
            menuItem2->setPosition(Point::ZERO);
            
            Menu* menu2 = Menu::create(menuItem2, NULL);
            menu2->setContentSize(tmpSize2);
            menu2->setPosition(Point(tmpSize.width * 0.50, tmpSize.height * 0.5));
            image->addChild(menu2, 200, 200);
            
            // スキルボタン
            Scale9Sprite* image3 = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
            image3->setContentSize(Size(w * 0.3 , h * 0.08 ));
            Size tmpSize3 = image3->getContentSize();
            image3->setPosition(Point::ZERO);
            
            auto label21 = LabelTTF::create(LocalizedString::getLocalizedString("skill", "skill"), FONT, LEVEL_FONT_SIZE);
            label21->setFontFillColor(Color3B(255, 255, 255));
            label21->setPosition(Point(tmpSize3.width * 0.5, tmpSize3.height * 0.5));
            image3->addChild(label21, 211, 211);
            
            MenuItemSprite* menuItem3 = MenuItemSprite::create(image3, image3, CC_CALLBACK_0(HelloWorld::setMenu220_unitDetail_skillUIin, this, user_unit_id, image, class_type));
            menuItem3->setContentSize(tmpSize3);
            menuItem3->setPosition(Point::ZERO);
            
            Menu* menu3 = Menu::create(menuItem3, NULL);
            menu3->setContentSize(tmpSize3);
            menu3->setPosition(Point(tmpSize.width * 0.2, tmpSize.height * 0.5));
            image->addChild(menu3, 210, 210);
            
            // infoボタン
            Scale9Sprite* image4 = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
            image4->setContentSize(Size(w * 0.3 , h * 0.08 ));
            Size tmpSize4 = image3->getContentSize();
            image4->setPosition(Point::ZERO);
            
            auto label22 = LabelTTF::create(LocalizedString::getLocalizedString("info", "Info"), FONT, LEVEL_FONT_SIZE);
            label22->setFontFillColor(Color3B(255, 255, 255));
            label22->setPosition(Point(tmpSize4.width * 0.5, tmpSize4.height * 0.5));
            image4->addChild(label22, 221, 221);
            
            MenuItemSprite* menuItem4 = MenuItemSprite::create(image4, image4, CC_CALLBACK_0(HelloWorld::setMenu220_unitDetail_infoUIin, this, user_unit_id, image));
            menuItem4->setContentSize(tmpSize4);
            menuItem4->setPosition(Point::ZERO);
            
            Menu* menu4 = Menu::create(menuItem4, NULL);
            menu4->setContentSize(tmpSize4);
            menu4->setPosition(Point(tmpSize.width * 0.8, tmpSize.height * 0.5));
            image->addChild(menu4, 220, 220);
            
            // 最初にスキルメニューを出しておく
            if(hyoujiMenu == 0){
                setMenu220_unitDetail_skillUIin(user_unit_id, image, class_type);
            }
            else{
                setMenu220_unitDetail_kyoukaUIin(user_unit_id, image);
            }
        }
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    
}

void HelloWorld::setMenu220_unitDetail_kyoukaUIin(int user_unit_id, Scale9Sprite* image)
{
    CCLOG("setMenu220_unitDetail_kyoukaUIin");
    for(int i = 500; i < 1000; i++){
        image->removeChildByTag(i);
    }
    playSE(0); // se_enter.mp3
    
    Size tmpSize = image->getContentSize();
    
    // ベースの四角。押しても何もならないリンクがついてる
    Scale9Sprite* image2 = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image2->setContentSize(Size(w * 0.88 , tmpSize.height * 0.4 ));
    Size tmpSize2 = image2->getContentSize();
    image2->setPosition(Point::ZERO);
    
    MenuItemSprite* menuItem2 = MenuItemSprite::create(image2, image2, CC_CALLBACK_0(HelloWorld::blank, this));
    menuItem2->setContentSize(tmpSize2);
    menuItem2->setPosition(Point::ZERO);
    
    Menu* menu2 = Menu::create(menuItem2, NULL);
    menu2->setContentSize(tmpSize2);
    menu2->setPosition(Point(tmpSize.width * 0.5 + w, tmpSize.height * 0.21));
    image->addChild(menu2, 500, 500);
    
    auto move = MoveBy::create(0.25, Point(-w, 0));
    menu2->runAction(move);
    
    // コンテンツ中身の準備
    
    sqlite3* db = NULL;
    std::string fullpath = FileUtils::getInstance()->getWritablePath();
    fullpath += "userdata.db";
    
    if (sqlite3_open(fullpath.c_str(), &db) == SQLITE_OK) {
        sqlite3_stmt *stmt = NULL;
        
        char tmpChar[64];
        sprintf(tmpChar, "%d", user_unit_id);
        std::string sql = "select * from user_unit where _id = ";
        sql += tmpChar;
        if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
            sqlite3_step(stmt);
            int class_type      = (int)sqlite3_column_int(stmt, 1);
            int level           = (int)sqlite3_column_int(stmt, 2);
            int class_level     = (int)sqlite3_column_int(stmt, 3);
            int hp              = (int)sqlite3_column_int(stmt, 4);
            int physical_atk    = (int)sqlite3_column_int(stmt, 5);
            int magical_atk     = (int)sqlite3_column_int(stmt, 6);
            int physical_def    = (int)sqlite3_column_int(stmt, 7);
            int magical_def     = (int)sqlite3_column_int(stmt, 8);
            int skill1          = (int)sqlite3_column_int(stmt, 9);
            int skill2          = (int)sqlite3_column_int(stmt,10);
            int class_skill     = (int)sqlite3_column_int(stmt,11);
            int zokusei         = (int)sqlite3_column_int(stmt,12);
            
            // ここからコンテンツ中身
            CCLOG("base class level : %d", class_level);
            kyoukaNextLevel = level;
            kyoukaNextClassLevel = class_level;
            kyoukaNextHp = hp;
            kyoukaNextAtk = physical_atk;
            kyoukaNextMgc = magical_atk;
            kyoukaNextPhysicalDef = physical_def;
            kyoukaNextMagicalDef = magical_def;
            kyoukaNextCoin = 0;
            kyoukaTotalCoin = 0;
            shinkaTotalMahouseki = 0;
            for(int i = level + 1; i < kyoukaNextLevel; i++){
                kyoukaTotalCoin += i * 10;
            }
            
            char tmpString[64];
            sprintf(tmpString, "Lv.%d", level);
            auto label = LabelTTF::create(tmpString, FONT2, LEVEL_FONT_SIZE);
            auto label2 = LabelTTF::create("↓↓↓", FONT, LEVEL_FONT_SIZE);
            sprintf(tmpString, "Lv.%d", kyoukaNextLevel);
            kyoukaNextLevelLabel = LabelTTF::create(tmpString, FONT2, LEVEL_FONT_SIZE);
            
            label->setPosition(Point(tmpSize2.width * 0.7, tmpSize2.height * 0.8));
            label2->setPosition(Point(tmpSize2.width * 0.7, tmpSize2.height * 0.8 - label->getContentSize().height*1.5));
            kyoukaNextLevelLabel->setPosition(Point(tmpSize2.width * 0.7, tmpSize2.height * 0.8 - label->getContentSize().height*1.5 - label2->getContentSize().height*1.5));
            
            image2->addChild(label);
            image2->addChild(label2);
            image2->addChild(kyoukaNextLevelLabel);
            
            // 強化ボタン
            if(level != UNITMAXLEVEL || class_level != UNITMAXCLASSLEVEL){ // ユニットのマックスクラスレベルを超えさせない
                Scale9Sprite* image3 = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
                image3->setContentSize(Size(w * 0.3 , h * 0.08 ));
                Size tmpSize4 = image3->getContentSize();
                image3->setPosition(Point::ZERO);
                
                if(level != UNITMAXLEVEL){ // 強化
                    label = LabelTTF::create(LocalizedString::getLocalizedString("dolvup", "exec lvup"), FONT, LEVEL_FONT_SIZE);
                }
                else{
                    if(zokusei <= 2){ // 進化
                        label = LabelTTF::create(LocalizedString::getLocalizedString("doevolution", "exec evolution"), FONT, LEVEL_FONT_SIZE);
                    }
                    else{ // 転生
                        label = LabelTTF::create(LocalizedString::getLocalizedString("dotensei", "dotensei"), FONT, LEVEL_FONT_SIZE);
                    }
                }
                label->setFontFillColor(Color3B(255, 255, 255));
                label->setPosition(Point(tmpSize4.width * 0.5, tmpSize4.height * 0.5));
                image3->addChild(label, 201, 201);
                
                MenuItemSprite* menuItem3;
                if(level != UNITMAXLEVEL){ // 強化
                    menuItem3 = MenuItemSprite::create(image3, image3, CC_CALLBACK_0(HelloWorld::setMenu220_unitDetail_kyoukaExec, this, user_unit_id, false, 0, 0));
                }
                else{ // 進化・転生
                    CCLOG("%d, %d, %d", user_unit_id, zokusei, class_type);
                        menuItem3 = MenuItemSprite::create(image3, image3, CC_CALLBACK_0(HelloWorld::setMenu220_unitDetail_shinkaUIin, this, user_unit_id, zokusei, class_type));
                }
                menuItem3->setContentSize(tmpSize4);
                menuItem3->setPosition(Point::ZERO);
                
                Menu* menu3 = Menu::create(menuItem3, NULL);
                menu3->setContentSize(tmpSize4);
                menu3->setPosition(Point(tmpSize2.width * 0.7, tmpSize2.height * 0.2));
                image2->addChild(menu3, 200, 200);
            }
            
            
            // 矢印ボタン
            if(level != UNITMAXLEVEL){
                auto buttonimage = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
                buttonimage->setContentSize(Size(w * 0.1 , w * 0.1 ));
                Size tmpSize3 = buttonimage->getContentSize();
                buttonimage->setPosition(Point::ZERO);
                
                auto label2 = LabelTTF::create(LocalizedString::getLocalizedString("left", "<-"), FONT, LEVEL_FONT_SIZE);
                label2->setFontFillColor(Color3B(255, 255, 255));
                label2->setPosition(Point(tmpSize3.width * 0.5, tmpSize3.height * 0.5));
                buttonimage->addChild(label2);
                
                auto buttonmenuItem = MenuItemSprite::create(buttonimage, buttonimage, CC_CALLBACK_0(HelloWorld::setMenu220_unitDetail_kyoukaUpDownSelected, this, user_unit_id, level, class_level, false));
                buttonmenuItem->setContentSize(tmpSize3);
                buttonmenuItem->setPosition(Point::ZERO);
                
                auto buttonmenu = Menu::create(buttonmenuItem, NULL);
                buttonmenu->setContentSize(tmpSize3);
                buttonmenu->setPosition(Point(tmpSize2.width * 0.7 - kyoukaNextLevelLabel->getContentSize().width - w * 0.02, tmpSize2.height * 0.8 - label->getContentSize().height*1.5 - label2->getContentSize().height*1.5));
                image2->addChild(buttonmenu);
                
                
                auto buttonimage2 = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
                buttonimage2->setContentSize(Size(w * 0.1 , w * 0.1 ));
                buttonimage2->setPosition(Point::ZERO);
                
                auto label3 = LabelTTF::create(LocalizedString::getLocalizedString("right", "->"), FONT, LEVEL_FONT_SIZE);
                label3->setFontFillColor(Color3B(255, 255, 255));
                label3->setPosition(Point(tmpSize3.width * 0.5, tmpSize3.height * 0.5));
                buttonimage2->addChild(label3);
                
                auto buttonmenuItem2 = MenuItemSprite::create(buttonimage2, buttonimage2, CC_CALLBACK_0(HelloWorld::setMenu220_unitDetail_kyoukaUpDownSelected, this, user_unit_id, level, class_level, true));
                buttonmenuItem2->setContentSize(tmpSize3);
                buttonmenuItem2->setPosition(Point::ZERO);
                
                auto buttonmenu2 = Menu::create(buttonmenuItem2, NULL);
                buttonmenu2->setContentSize(tmpSize3);
                buttonmenu2->setPosition(Point(tmpSize2.width * 0.7 + kyoukaNextLevelLabel->getContentSize().width + w * 0.02, tmpSize2.height * 0.8 - label->getContentSize().height*1.5 - label2->getContentSize().height*1.5));
                image2->addChild(buttonmenu2);
            }
            
            // ステータスアップ
            
            auto label10 = LabelTTF::create(LocalizedString::getLocalizedString("hp", "hp"), FONT, LEVEL_FONT_SIZE);
            label10->setAnchorPoint(Point(0, 0.5));
            label10->setFontFillColor(Color3B(255, 255, 255));
            label10->setPosition(Point(tmpSize2.width * 0.04, tmpSize2.height * 0.8));
            image2->addChild(label10);
            
            auto label11 = LabelTTF::create(LocalizedString::getLocalizedString("physicalAttack", "physicalAttack"), FONT, LEVEL_FONT_SIZE);
            label11->setAnchorPoint(Point(0, 0.5));
            label11->setFontFillColor(Color3B(255, 255, 255));
            label11->setPosition(Point(tmpSize2.width * 0.04, tmpSize2.height * 0.8 - label10->getContentSize().height));
            image2->addChild(label11);
            
            auto label12 = LabelTTF::create(LocalizedString::getLocalizedString("magicalAttack", "magicalAttack"), FONT, LEVEL_FONT_SIZE);
            label12->setAnchorPoint(Point(0, 0.5));
            label12->setFontFillColor(Color3B(255, 255, 255));
            label12->setPosition(Point(tmpSize2.width * 0.04, tmpSize2.height * 0.8 - label10->getContentSize().height * 2));
            image2->addChild(label12);
            
            sprintf(tmpString, "%d ->", hp);
            auto label20 = LabelTTF::create(tmpString, FONT, LEVEL_FONT_SIZE);
            label20->setAnchorPoint(Point(0, 0.5));
            label20->setFontFillColor(Color3B(255, 255, 255));
            label20->setPosition(Point(tmpSize2.width * 0.1, tmpSize2.height * 0.8));
            image2->addChild(label20);
            
            sprintf(tmpString, "%d ->", physical_atk);
            auto label21 = LabelTTF::create(tmpString, FONT, LEVEL_FONT_SIZE);
            label21->setAnchorPoint(Point(0, 0.5));
            label21->setFontFillColor(Color3B(255, 255, 255));
            label21->setPosition(Point(tmpSize2.width * 0.1, tmpSize2.height * 0.8 - label10->getContentSize().height));
            image2->addChild(label21);
            
            sprintf(tmpString, "%d ->", magical_atk);
            auto label22 = LabelTTF::create(tmpString, FONT, LEVEL_FONT_SIZE);
            label22->setAnchorPoint(Point(0, 0.5));
            label22->setFontFillColor(Color3B(255, 255, 255));
            label22->setPosition(Point(tmpSize2.width * 0.1, tmpSize2.height * 0.8 - label10->getContentSize().height * 2));
            image2->addChild(label22);
            
            float tmpMaxWidth = MAX(MAX(label20->getContentSize().width, label21->getContentSize().width), label22->getContentSize().width);
            
            sprintf(tmpString, "%d", kyoukaNextHp);
            kyoukaNextHpLabel = LabelTTF::create(tmpString, FONT, LEVEL_FONT_SIZE);
            kyoukaNextHpLabel->setAnchorPoint(Point(0, 0.5));
            kyoukaNextHpLabel->setFontFillColor(Color3B(255, 255, 255));
            kyoukaNextHpLabel->setPosition(Point(tmpSize2.width * 0.1 + tmpMaxWidth, tmpSize2.height * 0.8));
            image2->addChild(kyoukaNextHpLabel);
            
            sprintf(tmpString, "%d", kyoukaNextAtk);
            kyoukaNextAtkLabel = LabelTTF::create(tmpString, FONT, LEVEL_FONT_SIZE);
            kyoukaNextAtkLabel->setAnchorPoint(Point(0, 0.5));
            kyoukaNextAtkLabel->setFontFillColor(Color3B(255, 255, 255));
            kyoukaNextAtkLabel->setPosition(Point(tmpSize2.width * 0.1 + tmpMaxWidth, tmpSize2.height * 0.8 - label10->getContentSize().height));
            image2->addChild(kyoukaNextAtkLabel);
            
            sprintf(tmpString, "%d", kyoukaNextMgc);
            kyoukaNextMgcLabel = LabelTTF::create(tmpString, FONT, LEVEL_FONT_SIZE);
            kyoukaNextMgcLabel->setAnchorPoint(Point(0, 0.5));
            kyoukaNextMgcLabel->setFontFillColor(Color3B(255, 255, 255));
            kyoukaNextMgcLabel->setPosition(Point(tmpSize2.width * 0.1 + tmpMaxWidth, tmpSize2.height * 0.8 - label10->getContentSize().height * 2));
            image2->addChild(kyoukaNextMgcLabel);
            
            // 必要コイン
            
            if(level != UNITMAXLEVEL){ // 強化
                auto label40 = LabelTTF::create(LocalizedString::getLocalizedString("fornextlevel", "need"), FONT, LEVEL_FONT_SIZE);
                label40->setAnchorPoint(Point(0, 0.5));
                label40->setFontFillColor(Color3B(255, 255, 255));
                label40->setPosition(Point(tmpSize2.width * 0.04, tmpSize2.height * 0.4));
                image2->addChild(label40);
                
                auto label41 = LabelTTF::create(LocalizedString::getLocalizedString("needcoins", "needcoins"), FONT, LEVEL_FONT_SIZE);
                label41->setAnchorPoint(Point(0, 0.5));
                label41->setFontFillColor(Color3B(255, 255, 255));
                label41->setPosition(Point(tmpSize2.width * 0.04, tmpSize2.height * 0.4 - label40->getContentSize().height));
                image2->addChild(label41);
                
                float tmpMaxWidth2 = MAX(label40->getContentSize().width, label41->getContentSize().width);
                
                sprintf(tmpString, "%d", kyoukaNextCoin);
                kyoukaNextCoinLabel = LabelTTF::create(tmpString, FONT, LEVEL_FONT_SIZE);
                kyoukaNextCoinLabel->setAnchorPoint(Point(0, 0.5));
                kyoukaNextCoinLabel->setFontFillColor(Color3B(255, 255, 255));
                kyoukaNextCoinLabel->setPosition(Point(tmpSize2.width * 0.04 + tmpMaxWidth2, tmpSize2.height * 0.4));
                image2->addChild(kyoukaNextCoinLabel);
                
                sprintf(tmpString, "%d", kyoukaTotalCoin);
                kyoukaTotalCoinLabel = LabelTTF::create(tmpString, FONT, LEVEL_FONT_SIZE);
                kyoukaTotalCoinLabel->setAnchorPoint(Point(0, 0.5));
                kyoukaTotalCoinLabel->setFontFillColor(Color3B(255, 255, 255));
                kyoukaTotalCoinLabel->setPosition(Point(tmpSize2.width * 0.04 + tmpMaxWidth2, tmpSize2.height * 0.4 - label40->getContentSize().height));
                image2->addChild(kyoukaTotalCoinLabel);
                
                // 1レベルあげた状態を初期表示(コインが足りないとあがらない)
                setMenu220_unitDetail_kyoukaUpDownSelected(user_unit_id, level, class_level, true);
            
            }
            else{
             //   if(zokusei <= 2){ // 進化
                
                auto label41 = LabelTTF::create(LocalizedString::getLocalizedString("needmahouseki", "needmahouseki"), FONT, LEVEL_FONT_SIZE);
                label41->setAnchorPoint(Point(0, 0.5));
                label41->setFontFillColor(Color3B(255, 255, 255));
                label41->setPosition(Point(tmpSize2.width * 0.04, tmpSize2.height * 0.4 - label41->getContentSize().height));
                image2->addChild(label41);
                
                float tmpMaxWidth2 = label41->getContentSize().width;
                
                sprintf(tmpString, "%d", shinkaTotalMahouseki);
                kyoukaTotalCoinLabel = LabelTTF::create(tmpString, FONT, LEVEL_FONT_SIZE);
                kyoukaTotalCoinLabel->setAnchorPoint(Point(0, 0.5));
                kyoukaTotalCoinLabel->setFontFillColor(Color3B(255, 255, 255));
                kyoukaTotalCoinLabel->setPosition(Point(tmpSize2.width * 0.04 + tmpMaxWidth2, tmpSize2.height * 0.4 - label41->getContentSize().height));
                image2->addChild(kyoukaTotalCoinLabel);
                
                setMenu220_unitDetail_kyoukaUpDownSelected(user_unit_id, level, class_level, true);
             //   }
             //   else{ // 転生
             //   }
            }
        }
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    
}

void HelloWorld::setMenu220_unitDetail_kyoukaUpDownSelected(int user_unit_id, int org_level, int org_class_level, bool upOrDown)
{
    sqlite3* db = NULL;
    std::string fullpath = FileUtils::getInstance()->getWritablePath();
    fullpath += "userdata.db";
    
    if (sqlite3_open(fullpath.c_str(), &db) == SQLITE_OK) {
        sqlite3_stmt *stmt = NULL;
        
        char tmpChar[64];
        sprintf(tmpChar, "%d", user_unit_id);
        std::string sql = "select * from user_unit where _id = ";
        sql += tmpChar;
        if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
            sqlite3_step(stmt);
            int class_type      = (int)sqlite3_column_int(stmt, 1);
            int level           = (int)sqlite3_column_int(stmt, 2);
            int class_level     = (int)sqlite3_column_int(stmt, 3);
            int hp              = (int)sqlite3_column_int(stmt, 4);
            int physical_atk    = (int)sqlite3_column_int(stmt, 5);
            int magical_atk     = (int)sqlite3_column_int(stmt, 6);
            int physical_def    = (int)sqlite3_column_int(stmt, 7);
            int magical_def     = (int)sqlite3_column_int(stmt, 8);
            int skill1          = (int)sqlite3_column_int(stmt, 9);
            int skill2          = (int)sqlite3_column_int(stmt,10);
            int class_skill     = (int)sqlite3_column_int(stmt,11);
            int zokusei         = (int)sqlite3_column_int(stmt,12);
            
            // 成長テーブル取得
            double physical_initial;
            double physical_efficient;
            double physical_minimum;
            double magical_initial;
            double magical_efficient;
            double magical_minimum;
            double rankup_efficient;
            sqlite3* db2 = NULL;
            std::string fullpath2 = FileUtils::getInstance()->getWritablePath();
            fullpath2 += "gamedata.db";
            
            if (sqlite3_open(fullpath2.c_str(), &db2) == SQLITE_OK) {
                sqlite3_stmt *stmt2 = NULL;
                
                char tmpChar2[64];
                sprintf(tmpChar2, "%d", user_unit_id);
                std::string sql2 = "select * from unit_growth where _id = ";
                sql2 += tmpChar;
                if(sqlite3_prepare_v2(db2, sql2.c_str(), -1, &stmt2, NULL) == SQLITE_OK){
                    sqlite3_step(stmt2);
                    physical_initial    = (double)sqlite3_column_double(stmt2, 1);
                    physical_efficient  = (double)sqlite3_column_double(stmt2, 2);
                    physical_minimum    = (double)sqlite3_column_double(stmt2, 3);
                    magical_initial     = (double)sqlite3_column_double(stmt2, 4);
                    magical_efficient   = (double)sqlite3_column_double(stmt2, 5);
                    magical_minimum     = (double)sqlite3_column_double(stmt2, 6);
                    rankup_efficient    = (double)sqlite3_column_double(stmt2, 7);
                }
                sqlite3_reset(stmt2);
                sqlite3_finalize(stmt2);
            }
            sqlite3_close(db2);
            
            /* // 現状はこれ
            kyoukaNextLevel = level;
            kyoukaNextHp = hp;
            kyoukaNextAtk = physical_atk;
            kyoukaNextMgc = magical_atk;
            kyoukaNextPhysicalDef = physical_def;
            kyoukaNextMagicalDef = magical_def;
            kyoukaNextCoin = 0;
            kyoukaTotalCoin = 0;
             */

            // ここからコンテンツ中身
            int tmpkyoukaNextLevel = org_level;
            int tmpkyoukaNextClassLevel = org_class_level;
            int tmpkyoukaNextHp = 0;
            int tmpkyoukaNextAtk = 0;
            int tmpkyoukaNextMgc = 0;
            int tmpkyoukaNextPhysicalDef = 1;
            int tmpkyoukaNextMagicalDef = 1;
            if(upOrDown == true){
                tmpkyoukaNextLevel = kyoukaNextLevel + 1;
                CCLOG("Lv up junbi %d", tmpkyoukaNextLevel);
                tmpkyoukaNextPhysicalDef = kyoukaNextPhysicalDef + 10;
            }
            else{
                tmpkyoukaNextLevel = kyoukaNextLevel - 1;
                CCLOG("Lv down junbi %d", kyoukaNextLevel);
                tmpkyoukaNextPhysicalDef = kyoukaNextPhysicalDef - 10;
            }
            
            int tmpkyoukaNextCoin = 0;
            int tmpkyoukaTotalCoin = 0;

            tmpkyoukaNextAtk = physical_atk;
            tmpkyoukaNextMgc = magical_atk;
            for(int i = level; i < tmpkyoukaNextLevel; i++ ){
                tmpkyoukaNextAtk = tmpkyoukaNextAtk * MAX(physical_initial * pow(physical_efficient, (double)(i-1)), physical_minimum);
                tmpkyoukaNextMgc = tmpkyoukaNextMgc * MAX(magical_initial * pow(magical_efficient, (double)(i-1)), magical_minimum);
                tmpkyoukaNextHp = (tmpkyoukaNextAtk + tmpkyoukaNextMgc) * 2;
                tmpkyoukaNextCoin = tmpkyoukaNextHp;
                tmpkyoukaTotalCoin += tmpkyoukaNextHp;
            }
            
            // レベルアップお金たりない
            if(coin < tmpkyoukaTotalCoin && kyoukaNextLevel != org_level){
                sqlite3_reset(stmt);
                sqlite3_finalize(stmt);
                sqlite3_close(db);
                playSE(1); // se_ng.mp3
                
                return;
            }
            // レベルダウン、ベースと同じ
            if(org_level == tmpkyoukaNextLevel && kyoukaNextLevel != org_level){
                sqlite3_reset(stmt);
                sqlite3_finalize(stmt);
                sqlite3_close(db);
                playSE(1); // se_ng.mp3
                return;
            }
            // レベル上限を超えさせない
            if(tmpkyoukaNextLevel == UNITMAXLEVEL + 1 && org_level != UNITMAXLEVEL){
                sqlite3_reset(stmt);
                sqlite3_finalize(stmt);
                sqlite3_close(db);
                playSE(1); // se_ng.mp3
                return;
            }
            // クラスレベル上限を超えさせない(ここはネクスト表示を表示的に超えさせないだけ。実際は上でボタンを表示させない)
            if(org_level == UNITMAXLEVEL && org_class_level == UNITMAXCLASSLEVEL){
                sqlite3_reset(stmt);
                sqlite3_finalize(stmt);
                sqlite3_close(db);
                playSE(1); // se_ng.mp3
                return;
            }
            
            // クラスレベルアップ
            CCLOG("org_level %d, org_class_level %d", org_level, org_class_level);
            if(org_level == UNITMAXLEVEL && org_class_level != UNITMAXCLASSLEVEL){
                tmpkyoukaNextLevel = 1;
                tmpkyoukaNextClassLevel = org_class_level + 1;
                tmpkyoukaNextHp = kyoukaNextHp * rankup_efficient;
                tmpkyoukaNextAtk = kyoukaNextAtk * rankup_efficient;
                tmpkyoukaNextMgc = kyoukaNextMgc * rankup_efficient;
                tmpkyoukaNextPhysicalDef = kyoukaNextPhysicalDef * rankup_efficient;
                tmpkyoukaNextMagicalDef = kyoukaNextMagicalDef * rankup_efficient;
                tmpkyoukaNextCoin = tmpkyoukaNextCoin;
                tmpkyoukaTotalCoin = SHINKANEEDMAHOUSEKI;
            }
            
            if(kyoukaNextLevel != org_level){
                playSE(0); // se_enter.mp3
            }
            
            kyoukaNextLevel = tmpkyoukaNextLevel;
            kyoukaNextClassLevel = tmpkyoukaNextClassLevel;
            kyoukaNextHp = tmpkyoukaNextHp;
            kyoukaNextAtk = tmpkyoukaNextAtk;
            kyoukaNextMgc = tmpkyoukaNextMgc;
            kyoukaNextPhysicalDef = tmpkyoukaNextPhysicalDef;
            kyoukaNextMagicalDef = tmpkyoukaNextMagicalDef;
            kyoukaNextCoin = tmpkyoukaNextCoin;
            kyoukaTotalCoin = tmpkyoukaTotalCoin;
            
            // 進化・転生
            if(org_level == UNITMAXLEVEL && org_class_level != UNITMAXCLASSLEVEL){
                setMenu220_unitDetail_kyoukaInfoReWrite2(user_unit_id);
            }
            // 強化
            else{
                setMenu220_unitDetail_kyoukaInfoReWrite(user_unit_id);
            }
        }
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);
        
    }
    sqlite3_close(db);
}

void HelloWorld::setMenu220_unitDetail_kyoukaInfoReWrite(int user_unit_id)
{
    char tmpChar[64];
    
    sprintf(tmpChar, "Lv.%d", kyoukaNextLevel);
    kyoukaNextLevelLabel->setString(tmpChar);
    sprintf(tmpChar, "%d", kyoukaNextHp);
    kyoukaNextHpLabel->setString(tmpChar);
    sprintf(tmpChar, "%d", kyoukaNextAtk);
    kyoukaNextAtkLabel->setString(tmpChar);
    sprintf(tmpChar, "%d", kyoukaNextMgc);
    kyoukaNextMgcLabel->setString(tmpChar);
    sprintf(tmpChar, "%d", kyoukaNextCoin);
    kyoukaNextCoinLabel->setString(tmpChar);
    sprintf(tmpChar, "%d", kyoukaTotalCoin);
    kyoukaTotalCoinLabel->setString(tmpChar);
}

void HelloWorld::setMenu220_unitDetail_kyoukaInfoReWrite2(int user_unit_id)
{
    char tmpChar[64];
    
    sprintf(tmpChar, "Lv.%d", kyoukaNextLevel);
    kyoukaNextLevelLabel->setString(tmpChar);
    sprintf(tmpChar, "%d", kyoukaNextHp);
    kyoukaNextHpLabel->setString(tmpChar);
    sprintf(tmpChar, "%d", kyoukaNextAtk);
    kyoukaNextAtkLabel->setString(tmpChar);
    sprintf(tmpChar, "%d", kyoukaNextMgc);
    kyoukaNextMgcLabel->setString(tmpChar);

    sprintf(tmpChar, "%d", kyoukaTotalCoin); // 進化・転生のときは真宝石の数
    kyoukaTotalCoinLabel->setString(tmpChar);
}


void HelloWorld::setMenu220_unitDetail_kyoukaExec(int user_unit_id, bool shinkaFlag, int target_zokusei, int target_class_type) // 強化実行
{
    if(coin < kyoukaTotalCoin && shinkaFlag == false){
        playSE(1); // se_ng.mp3
        return;
    }

    CCLOG("setMenu220_unitDetail_kyoukaExec()");
    playSE(2); // se_levelup.mp3
    
    sqlite3* db = NULL;
    std::string fullpath = FileUtils::getInstance()->getWritablePath();
    fullpath += "userdata.db";
    
    if (sqlite3_open(fullpath.c_str(), &db) == SQLITE_OK) {
        char tmpChar[256];
        
        if(shinkaFlag == false){ // 強化
            sprintf(tmpChar, "UPDATE user_unit SET level = %d, class_level = %d, hp = %d, physical = %d, magical = %d, physical_def = %d , magical_def = %d where _id = %d", kyoukaNextLevel, kyoukaNextClassLevel, kyoukaNextHp, kyoukaNextAtk, kyoukaNextMgc, kyoukaNextPhysicalDef, kyoukaNextMagicalDef, user_unit_id);
        }
        else{ // 進化
            sprintf(tmpChar, "UPDATE user_unit SET level = %d, class_level = %d, hp = %d, physical = %d, magical = %d, physical_def = %d , magical_def = %d , zokusei = %d , class_type = %d where _id = %d", kyoukaNextLevel, kyoukaNextClassLevel, kyoukaNextHp, kyoukaNextAtk, kyoukaNextMgc, kyoukaNextPhysicalDef, kyoukaNextMagicalDef, target_zokusei, target_class_type, user_unit_id);
        }
        CCLOG("%s", tmpChar);
        sqlite3_exec(db, tmpChar, NULL, NULL, NULL);
    }
    sqlite3_close(db);
    
    coin -= kyoukaTotalCoin;
    updateUserDataDb();
    updateTopMenu();
    
    // パーティーをオンラインDB登録。
    searchOtherParties();
    
    setMenu220_unitDetail(user_unit_id, 1, false);
}


void HelloWorld::setMenu220_unitDetail_shinkaUIin(int user_unit_id, int base_zokusei, int base_class_type) // 進化UIイン
{
    CCLOG("setMenu220_unitDetail_shinkaUIin() , %d , %d , %d", user_unit_id, base_zokusei, base_class_type);
    
    playSE(0);
    
    for(int i = 100; i < 1000; i++){
        this->removeChildByTag(i);
    }
    
    Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image->setContentSize(Size(w * 0.9 , h * 0.6));
    Size tmpSize = image->getContentSize();
    image->setPosition(Point::ZERO);
    
    MenuItemSprite* menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::blank, this));
    menuItem->setContentSize(tmpSize);
    menuItem->setPosition(Point::ZERO);
    
    Menu* menu = Menu::create(menuItem, NULL);
    menu->setContentSize(tmpSize);
    menu->setPosition(Point( w * 1.5, h * 0.45 ));
    this->addChild(menu, 100, 100);
    
    auto move = MoveBy::create(0.25, Point(-w, 0));
    menu->runAction(move);

    // 進化先を選んでください
    LabelTTF* label;
    if(base_zokusei <= 2){ // 進化先を選んでください。
        label = LabelTTF::create(LocalizedString::getLocalizedString("chooseshinkasaki1", "chooseshinkasaki"), FONT, LEVEL_FONT_SIZE);
        label->setAnchorPoint(Point(0, 0.5));
        label->setFontFillColor(Color3B(255, 255, 255));
        label->setPosition(Point(tmpSize.width * 0.1, tmpSize.height * 0.9));
        image->addChild(label);
        
        auto label2 = LabelTTF::create(LocalizedString::getLocalizedString("chooseshinkasaki2", "chooseshinkasaki"), FONT, LEVEL_FONT_SIZE);
        label2->setAnchorPoint(Point(0, 0.5));
        label2->setFontFillColor(Color3B(255, 255, 255));
        label2->setPosition(Point(tmpSize.width * 0.1, tmpSize.height * 0.9 - label->getContentSize().height));
        image->addChild(label2);
    }
    else{ // 転生します
        label = LabelTTF::create(LocalizedString::getLocalizedString("tenseisimasu", "tenseisimasu"), FONT, LEVEL_FONT_SIZE);
        label->setAnchorPoint(Point(0, 0.5));
        label->setFontFillColor(Color3B(255, 255, 255));
        label->setPosition(Point(tmpSize.width * 0.1, tmpSize.height * 0.9));
        image->addChild(label);
        
        auto label2 = LabelTTF::create(LocalizedString::getLocalizedString("tenseisimasu2", "tenseisimasu"), FONT, LEVEL_FONT_SIZE);
        label2->setAnchorPoint(Point(0, 0.5));
        label2->setFontFillColor(Color3B(255, 255, 255));
        label2->setPosition(Point(tmpSize.width * 0.1, tmpSize.height * 0.9 - label->getContentSize().height));
        image->addChild(label2);
    }
    
   
    // 進化先キャラ描画準備
    int tmpTargetZokusei1 = 0;
    int tmpTargetZokusei2 = 0;
    switch (base_zokusei) {
        case 0:
            tmpTargetZokusei1 = 4;
            tmpTargetZokusei2 = 5;
            break;
        case 1:
            tmpTargetZokusei1 = 6;
            tmpTargetZokusei2 = 8;
            break;
        case 2:
            tmpTargetZokusei1 = 9;
            tmpTargetZokusei2 = 10;
            break;
        case 4:
            tmpTargetZokusei1 = 0;
            break;
        case 5:
            tmpTargetZokusei1 = 0;
            break;
        case 6:
            tmpTargetZokusei1 = 1;
            break;
        case 8:
            tmpTargetZokusei1 = 1;
            break;
        case 9:
            tmpTargetZokusei1 = 2;
            break;
        case 10:
            tmpTargetZokusei1 = 2;
            break;
            
        default:
            break;
    }
    
    // ここでターゲットのクラスタイプを決める
    int tmpTargetClassType = base_class_type;
    if(base_zokusei <= 2){ // 進化の時は進化先属性を選択
        tmpTargetClassType = base_class_type + 100;
    }
    else{
        tmpTargetClassType = base_class_type - 100;
    }
    
    // 進化先キャラ描画１
    char tmpFileName[64];
    sprintf(tmpFileName, "window_chara/window_chara%d.png", tmpTargetZokusei1);
    Scale9Sprite* image4 = Scale9Sprite::create(tmpFileName, Rect(0, 0, 180, 180), Rect(50, 50, 90, 90));
    image4->setContentSize(Size(w * 0.35 , w * 0.35 ));
    Size tmpSize4 = image4->getContentSize();
    if(base_zokusei <= 2){ // 進化の時は進化先属性を選択
        image4->setPosition(Point(tmpSize.width * 0.3, tmpSize.height * 0.6));
    }
    else{
        image4->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.6));
    }
    image->addChild(image4);
    
    Animation* charaAnimation = Animation::create();
    SpriteFrame* charaSprite[4];
    int i = 0;
    for (x = 0; x < 3; x++) {
        char tmpFile[64];
        sprintf(tmpFile, "charactor/chara%d-1.png", tmpTargetClassType);
        charaSprite[i] = SpriteFrame::create(tmpFile, Rect(x * 32, 0, 32, 32));
        charaAnimation->addSpriteFrame(charaSprite[i]);
        i++;
    }
    charaAnimation->setDelayPerUnit(0.3);
    RepeatForever* charaAction = RepeatForever::create(Animate::create(charaAnimation));
    Sprite* chara = Sprite::create();
    chara->setPosition(Point(tmpSize4.width * 0.5, tmpSize4.height * 0.5));
    chara->setScale(w * 0.15 / 16);
    image4->addChild(chara);
    chara->runAction(charaAction);
    
    
    // 進化先キャラ描画２
    if(base_zokusei <= 2){ // 進化の時は進化先属性を選択
        sprintf(tmpFileName, "window_chara/window_chara%d.png", tmpTargetZokusei2);
        Scale9Sprite* image5 = Scale9Sprite::create(tmpFileName, Rect(0, 0, 180, 180), Rect(50, 50, 90, 90));
        image5->setContentSize(Size(w * 0.35 , w * 0.35 ));
        Size tmpSize5 = image5->getContentSize();
        image5->setPosition(Point(tmpSize.width * 0.7, tmpSize.height * 0.6));
        image->addChild(image5);
        
        Animation* charaAnimation2 = Animation::create();
        SpriteFrame* charaSprite2[4];
        i = 0;
        for (x = 0; x < 3; x++) {
            char tmpFile[64];
            sprintf(tmpFile, "charactor/chara%d-1.png", tmpTargetClassType);
            charaSprite2[i] = SpriteFrame::create(tmpFile, Rect(x * 32, 0, 32, 32));
            charaAnimation2->addSpriteFrame(charaSprite2[i]);
            i++;
        }
        charaAnimation2->setDelayPerUnit(0.3);
        RepeatForever* charaAction2 = RepeatForever::create(Animate::create(charaAnimation2));
        Sprite* chara2 = Sprite::create();
        chara2->setPosition(Point(tmpSize4.width * 0.5, tmpSize4.height * 0.5));
        chara2->setScale(w * 0.15 / 16);
        image5->addChild(chara2);
        chara2->runAction(charaAction2);
    }
    
    
    
    // 選択ボタン１
    Scale9Sprite* image2 = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image2->setContentSize(Size(w * 0.3 , h * 0.08 ));
    Size tmpSize2 = image2->getContentSize();
    image2->setPosition(Point::ZERO);
    
    if(base_zokusei <= 2){ // 進化の時は進化先属性を選択
        label = LabelTTF::create(LocalizedString::getLocalizedString("korenisuru", "korenisuru"), FONT, LEVEL_FONT_SIZE);
    }
    else{
        label = LabelTTF::create(LocalizedString::getLocalizedString("OK", "OK"), FONT, LEVEL_FONT_SIZE);
    }
    label->setFontFillColor(Color3B(255, 255, 255));
    label->setPosition(Point(tmpSize2.width * 0.5, tmpSize2.height * 0.5));
    image2->addChild(label, 201, 201);
    
    MenuItemSprite* menuItem2 = MenuItemSprite::create(image2, image2, CC_CALLBACK_0(HelloWorld::setMenu220_unitDetail_shinkaExec, this, user_unit_id, tmpTargetZokusei1, tmpTargetClassType));
    menuItem2->setContentSize(tmpSize2);
    menuItem2->setPosition(Point::ZERO);
    
    Menu* menu2 = Menu::create(menuItem2, NULL);
    menu2->setContentSize(tmpSize2);
    if(base_zokusei <= 2){ // 進化の時は進化先属性を選択
        menu2->setPosition(Point(tmpSize.width * 0.3, tmpSize.height * 0.15));
    }
    else{
        menu2->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.15));
    }
    image->addChild(menu2, 200, 200);
    
    // 選択ボタン２
    if(base_zokusei <= 2){ // 進化の時は進化先属性を選択
        Scale9Sprite* image3 = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
        image3->setContentSize(Size(w * 0.3 , h * 0.08 ));
        Size tmpSize3 = image3->getContentSize();
        image3->setPosition(Point::ZERO);
        
        label = LabelTTF::create(LocalizedString::getLocalizedString("korenisuru", "korenisuru"), FONT, LEVEL_FONT_SIZE);
        label->setFontFillColor(Color3B(255, 255, 255));
        label->setPosition(Point(tmpSize3.width * 0.5, tmpSize3.height * 0.5));
        image3->addChild(label, 211, 211);
        
        MenuItemSprite* menuItem3 = MenuItemSprite::create(image3, image3, CC_CALLBACK_0(HelloWorld::setMenu220_unitDetail_shinkaExec, this, user_unit_id, tmpTargetZokusei2, tmpTargetClassType));
        menuItem3->setContentSize(tmpSize3);
        menuItem3->setPosition(Point::ZERO);
        
        Menu* menu3 = Menu::create(menuItem3, NULL);
        menu3->setContentSize(tmpSize3);
        menu3->setPosition(Point(tmpSize.width * 0.7, tmpSize.height * 0.15));
        image->addChild(menu3, 210, 210);
    }

}

void HelloWorld::setMenu220_unitDetail_shinkaExec(int user_unit_id, int target_zokusei, int target_class_type)
{
    CCLOG("setMenu220_unitDetail_shinkaExec(), %d, %d, %d", user_unit_id, target_zokusei, target_class_type);
    setMenu220_unitDetail_kyoukaExec(user_unit_id, true, target_zokusei, target_class_type);
}


void HelloWorld::setMenu220_unitDetail_skillUIin(int user_unit_id, Scale9Sprite* image, int class_type)
{
    CCLOG("setMenu220_unitDetail_skillUIin");
    for(int i = 500; i < 1000; i++){
        image->removeChildByTag(i);
    }
    
    playSE(0); // se_enter.mp3
    
    Size tmpSize = image->getContentSize();
    
    // ベースの四角。押しても何もならないリンクがついてる
    Scale9Sprite* image2 = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image2->setContentSize(Size(w * 0.88 , tmpSize.height * 0.4 ));
    Size tmpSize2 = image2->getContentSize();
    image2->setPosition(Point::ZERO);
    
    MenuItemSprite* menuItem2 = MenuItemSprite::create(image2, image2, CC_CALLBACK_0(HelloWorld::blank, this));
    menuItem2->setContentSize(tmpSize2);
    menuItem2->setPosition(Point::ZERO);
    
    Menu* menu2 = Menu::create(menuItem2, NULL);
    menu2->setContentSize(tmpSize2);
    menu2->setPosition(Point(tmpSize.width * 0.5 + w, tmpSize.height * 0.21));
    image->addChild(menu2, 500, 500);
    
    auto move = MoveBy::create(0.25, Point(-w, 0));
    menu2->runAction(move);
    
    // ここからコンテンツ中身
    //LabelTTF* label;
    auto label = LabelTTF::create(LocalizedString::getLocalizedString("koyuuskill", "koyuuskill"), FONT, LEVEL_FONT_SIZE);
    label->setAnchorPoint(Point(0, 0.5));
    label->setFontFillColor(Color3B(255, 255, 255));
    label->setPosition(Point(tmpSize2.width * 0.05 , tmpSize2.height * 0.9 ));
    image2->addChild(label);

    char tmpChar[32];
    sprintf(tmpChar, "classSkill%d", class_type);
    auto label2 = LabelTTF::create(LocalizedString::getLocalizedString(tmpChar, "??????????"), FONT, LEVEL_FONT_SIZE);
    label2->setAnchorPoint(Point(0, 0.5));
    label2->setFontFillColor(Color3B(255, 255, 255));
    label2->setPosition(Point(tmpSize2.width * 0.1 , tmpSize2.height * 0.75 ));
    image2->addChild(label2);
    
    auto label3 = LabelTTF::create(LocalizedString::getLocalizedString("soubiskill", "soubiskill"), FONT, LEVEL_FONT_SIZE);
    label3->setAnchorPoint(Point(0, 0.5));
    label3->setFontFillColor(Color3B(255, 255, 255));
    label3->setPosition(Point(tmpSize2.width * 0.05 , tmpSize2.height * 0.5 ));
    image2->addChild(label3);
    
    // 装備しているかチェック
    
    sqlite3* db = NULL;
    std::string fullpath = FileUtils::getInstance()->getWritablePath();
    fullpath += "userdata.db";
    
    if (sqlite3_open(fullpath.c_str(), &db) == SQLITE_OK) {
        sqlite3_stmt *stmt = NULL;
        
        char tmpString[128];
        sprintf(tmpString, "select * from user_skill_list where soubi_unit_id = %d", user_unit_id);
        if(sqlite3_prepare_v2(db, tmpString, -1, &stmt, NULL) == SQLITE_OK){
            int count = 0;
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                char tmpChar[64];
                
                int _id = (int)sqlite3_column_int(stmt, 0);
                
                sprintf(tmpChar, "skillName%d", (int)sqlite3_column_int(stmt, 1));
                auto label4 = LabelTTF::create(LocalizedString::getLocalizedString(tmpChar, tmpChar), FONT, LEVEL_FONT_SIZE);
                label4->setAnchorPoint(Point(0, 0.5));
                label4->setFontFillColor(Color3B(255, 255, 255));
                label4->setPosition(Point(tmpSize2.width * 0.1 , tmpSize2.height * (0.35 - 0.15*count) ));
                image2->addChild(label4);
                
                sprintf(tmpChar, "Lv %d", (int)sqlite3_column_int(stmt, 2));
                auto label5 = LabelTTF::create(LocalizedString::getLocalizedString(tmpChar, tmpChar), FONT, LEVEL_FONT_SIZE);
                label5->setAnchorPoint(Point(1, 0.5));
                label5->setFontFillColor(Color3B(255, 255, 255));
                label5->setPosition(Point(tmpSize2.width * 0.6 , tmpSize2.height * (0.35 - 0.15*count) ));
                image2->addChild(label5);
                
                // 変更するボタン
                Scale9Sprite* image3 = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
                image3->setContentSize(Size(w * 0.3 , h * 0.08 ));
                Size tmpSize4 = image3->getContentSize();
                image3->setPosition(Point::ZERO);
                
                auto label6 = LabelTTF::create(LocalizedString::getLocalizedString("henkoubutton", "change"), FONT, LEVEL_FONT_SIZE);
                label6->setFontFillColor(Color3B(255, 255, 255));
                label6->setPosition(Point(tmpSize4.width * 0.5, tmpSize4.height * 0.5));
                image3->addChild(label6, 201, 201);
                
                MenuItemSprite* menuItem3 = MenuItemSprite::create(image3, image3, CC_CALLBACK_0(HelloWorld::setMenu490_skillListFromUnitDetail, this, 0, user_unit_id, _id));
                menuItem3->setContentSize(tmpSize4);
                menuItem3->setPosition(Point::ZERO);
                
                Menu* menu3 = Menu::create(menuItem3, NULL);
                menu3->setContentSize(tmpSize4);
                menu3->setPosition(Point(tmpSize2.width * 0.8, tmpSize2.height * (0.35 - 0.15*count)));
                image2->addChild(menu3, 200, 200);
                
                count++;
            }
            if(count == 0){
                auto label7 = LabelTTF::create(LocalizedString::getLocalizedString("nosoubi", "nosoubi"), FONT, LEVEL_FONT_SIZE);
                label7->setAnchorPoint(Point(0, 0.5));
                label7->setFontFillColor(Color3B(255, 255, 255));
                label7->setPosition(Point(tmpSize2.width * 0.1 , tmpSize2.height * 0.35 ));
                image2->addChild(label7);
                
                // 装備するボタン
                Scale9Sprite* image3 = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
                image3->setContentSize(Size(w * 0.3 , h * 0.08 ));
                Size tmpSize4 = image3->getContentSize();
                image3->setPosition(Point::ZERO);
                
                auto label8 = LabelTTF::create(LocalizedString::getLocalizedString("soubibutton", "soubi"), FONT, LEVEL_FONT_SIZE);
                label8->setFontFillColor(Color3B(255, 255, 255));
                label8->setPosition(Point(tmpSize4.width * 0.5, tmpSize4.height * 0.5));
                image3->addChild(label8, 201, 201);
                
                MenuItemSprite* menuItem3 = MenuItemSprite::create(image3, image3, CC_CALLBACK_0(HelloWorld::setMenu490_skillListFromUnitDetail, this, 0, user_unit_id, 0));
                menuItem3->setContentSize(tmpSize4);
                menuItem3->setPosition(Point::ZERO);
                
                Menu* menu3 = Menu::create(menuItem3, NULL);
                menu3->setContentSize(tmpSize4);
                menu3->setPosition(Point(tmpSize2.width * 0.8, tmpSize2.height * 0.35));
                image2->addChild(menu3, 200, 200);
                
            }
        }
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    
}




void HelloWorld::setMenu220_unitDetail_infoUIin(int user_unit_id, Scale9Sprite* image)
{
    CCLOG("setMenu220_unitDetail_infoUIin");
    for(int i = 500; i < 1000; i++){
        image->removeChildByTag(i);
    }
    
    playSE(0); // se_enter.mp3
    
    Size tmpSize = image->getContentSize();
    
    // ベースの四角。押しても何もならないリンクがついてる
    Scale9Sprite* image2 = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image2->setContentSize(Size(w * 0.88 , tmpSize.height * 0.4 ));
    Size tmpSize2 = image2->getContentSize();
    image2->setPosition(Point::ZERO);
    
    MenuItemSprite* menuItem2 = MenuItemSprite::create(image2, image2, CC_CALLBACK_0(HelloWorld::blank, this));
    menuItem2->setContentSize(tmpSize2);
    menuItem2->setPosition(Point::ZERO);
    
    Menu* menu2 = Menu::create(menuItem2, NULL);
    menu2->setContentSize(tmpSize2);
    menu2->setPosition(Point(tmpSize.width * 0.5 + w, tmpSize.height * 0.21));
    image->addChild(menu2, 500, 500);
    
    auto move = MoveBy::create(0.25, Point(-w, 0));
    menu2->runAction(move);
    
    // ここからコンテンツ中身
    auto label = LabelTTF::create(LocalizedString::getLocalizedString("???????????", "???????????????????????????????????????????????????????????????????????"), FONT, LEVEL_FONT_SIZE, Size(tmpSize2.width*0.8, tmpSize2.height * 0.7), TextHAlignment::LEFT, TextVAlignment::TOP);
    label->setAnchorPoint(Point(0, 0.5));
    label->setFontFillColor(Color3B(255, 255, 255));
    label->setPosition(Point(tmpSize2.width * 0.1 , tmpSize2.height * 0.5 ));
    image2->addChild(label);
    
}


//////////////////////////////////////////////////
//////////////////////////////////////////////////
//////////////////////////////////////////////////
//  メニュー３００系　ショップ
//////////////////////////////////////////////////
//////////////////////////////////////////////////
//////////////////////////////////////////////////

void HelloWorld::setMenu301() // 魔法石購入
{
    ScrollView *scrollView = ScrollView::create();
    scrollView->setViewSize(Size(w, hc));
    scrollView->setPosition(Point(0, yg));
    
    LayerColor* scrollLayer = LayerColor::create(Color4B(255, 255, 255, SCROLLVIEWOPACITY));
    scrollLayer->changeWidth(w);
    
    float scrollLayerHeight =  (w * 0.18) * (MAHOUSEKITYPES + 2) + h * 0.1 ;
    scrollLayer->changeHeight(scrollLayerHeight);
    scrollLayer->setAnchorPoint(Point(0, 1));
    scrollLayer->setPosition(Point::ZERO);
    
    // 無料でもらえる
    Scale9Sprite* image2 = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image2->setContentSize(Size(w * 0.9 , w * 0.18));
    Point point = image2->getAnchorPoint();
    image2->setPosition(Point::ZERO);
    
    char tmpChar[64];
    sprintf(tmpChar, "%s", LocalizedString::getLocalizedString("mahousekiPresent", "mahousekiPresent"));
    auto label = LabelTTF::create(tmpChar, FONT, LEVEL_FONT_SIZE);
    Size tmpSize = image2->getContentSize();
    label->setAnchorPoint(Point(0, 0.5));
    label->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.5));
    label->setFontFillColor(Color3B(255, 255, 255));
    image2->addChild(label);
    
    // 魔法石がもらえる２４時間計算
    if(time(NULL) - lastMahousekiGotTime >= MAHOUSEKITIME){
        auto sprite = Sprite::create("whitecircle.png");
        Size tmpSize2 = sprite->getContentSize();
        sprite->setScale( 1.2 * w / 640);
        sprite->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.8));
        image2->addChild(sprite);
        
        auto label = LabelTTF::create("!!", FONT, LEVEL_FONT_SIZE);
        label->setFontFillColor(Color3B(255, 255, 255));
        label->setPosition(Point(tmpSize2.width * 0.5, tmpSize2.height * 0.5));
        sprite->addChild(label);
    }
    
    MenuItemSprite* menuItem = MenuItemSprite::create(image2, image2, CC_CALLBACK_0(HelloWorld::setMenu301_mahousekiPreset, this));
    menuItem->setContentSize(tmpSize);
    menuItem->setPosition(Point::ZERO);
    
    Menu* menu = Menu::create2(menuItem, NULL);
    menu->setContentSize(tmpSize);
    menu->setPosition(Point(w * 1.5, h * 0.7));
    menu->setPosition(Point( w * 0.5 + w, scrollLayerHeight - h * 0.15 - w * 0.18 * 0 ));
    
    scrollLayer->addChild(menu, 101, 101);
    
    auto move = MoveBy::create(0.25, Point(-w, 0));
    menu->runAction(move);
    
    int disp_count = 0;
    // 魔法石所持が１億を超えていたら購入できない
    if(mahouseki < MAHOUSEKIBUYSTOP){
        // 商品リスト
        sqlite3* db = NULL;
        std::string fullpath = FileUtils::getInstance()->getWritablePath();
        fullpath += "userdata.db";
        
        if (sqlite3_open(fullpath.c_str(), &db) == SQLITE_OK) {
            sqlite3_stmt *stmt = NULL;
            
            char tmpSql[256];
            sprintf(tmpSql, "select * from store_products order by product_id asc");
            sprintf(tmpSql, "select * from store_products order by product_id asc");
            sprintf(tmpSql, "select * from store_products order by product_id asc");
            CCLOG("%s", tmpSql);
            //CCLOG("%d, %d", sqlite3_prepare_v2(db, tmpSql, -1, &stmt, NULL) , SQLITE_OK);
            
            if(sqlite3_prepare_v2(db, tmpSql, -1, &stmt, NULL) == SQLITE_OK){
                CCLOG("select * from store_products OK");
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    CCLOG("select * from store_products while");
                    int _id  = (int)sqlite3_column_int(stmt, 0);
                    int time = (int)sqlite3_column_int(stmt, 1);
                    const unsigned char* productId          = (const unsigned char*)sqlite3_column_text(stmt, 2);
                    const unsigned char* productDescription = (const unsigned char*)sqlite3_column_text(stmt, 3);
                    const unsigned char* productPrice       = (const unsigned char*)sqlite3_column_text(stmt, 4);
                    
                    CCLOG("%s", productDescription);
                    
                    int tmpNum = -1;
                    for(int i = 0; i < MAHOUSEKITYPESDEBUG; i++){
                        if(strcmp((const char*)productId, inappPurchaseItems[i].c_str()) == 0){
                            tmpNum = i;
                        }
                    }
                    if(tmpNum != -1){
                        // ここで商品１つを記載する
                        CCLOG("ここで商品をひとつ記載。 %s", productDescription);
                        Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
                        image->setContentSize(Size(w * 0.9 , w * 0.18));
                        Point point = image->getAnchorPoint();
                        image->setPosition(Point::ZERO);
                        
                        auto label = LabelTTF::create((const char*)productDescription, FONT, LEVEL_FONT_SIZE);
                        Size tmpSize = image->getContentSize();
                        label->setAnchorPoint(Point(0, 0.5));
                        label->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.5));
                        label->setFontFillColor(Color3B(255, 255, 255));
                        image->addChild(label);
                        
                        label = LabelTTF::create((const char*)productPrice, FONT, LEVEL_FONT_SIZE);
                        label->setAnchorPoint(Point(1, 0.5));
                        label->setPosition(Point(tmpSize.width * 0.95, tmpSize.height * 0.5));
                        label->setFontFillColor(Color3B(255, 255, 255));
                        image->addChild(label);
                        
                        MenuItemSprite* menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::setMenu301_selected, this, disp_count));
                        menuItem->setContentSize(tmpSize);
                        menuItem->setPosition(Point::ZERO);
                        
                        Menu* menu = Menu::create2(menuItem, NULL);
                        menu->setContentSize(tmpSize);
                        menu->setPosition(Point( w * 0.5 + w, scrollLayerHeight - h * 0.15 - w * 0.18 * ( disp_count + 1 ) ));
                        scrollLayer->addChild(menu, 103+disp_count, 103+disp_count);
                        
                        auto move = MoveBy::create(0.25 + 0.05 * (disp_count + 2), Point(-w, 0));
                        menu->runAction(move);
                        
                        disp_count++;
                        
                    }
                    
                }
            }
            sqlite3_reset(stmt);
            sqlite3_finalize(stmt);
        }
        sqlite3_close(db);
    }
    /*
    // 商品リスト
    for(int i = disp_count; i < MAHOUSEKITYPES; i++){
        Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
        image->setContentSize(Size(w * 0.9 , w * 0.18));
        Point point = image->getAnchorPoint();
        image->setPosition(Point::ZERO);
        
        char tmpChar[64];
        sprintf(tmpChar, "%s%d", LocalizedString::getLocalizedString("mahouseki", "mahouseki"), MahuosekiList[i]);
        auto label = LabelTTF::create(tmpChar, FONT, LEVEL_FONT_SIZE);
        Size tmpSize = image->getContentSize();
        label->setAnchorPoint(Point(0, 0.5));
        label->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.5));
        label->setFontFillColor(Color3B(255, 255, 255));
        image->addChild(label);
        
        MenuItemSprite* menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::setMenu301_selected, this, i));
        menuItem->setContentSize(tmpSize);
        menuItem->setPosition(Point::ZERO);
        
        Menu* menu = Menu::create2(menuItem, NULL);
        menu->setContentSize(tmpSize);
        menu->setPosition(Point(w * 1.5, h * 0.7 - w * 0.18 * (i + 1)));
        menu->setPosition(Point( w * 0.5 + w, scrollLayerHeight - h * 0.15 - w * 0.18 * ( i + 1 ) ));
        scrollLayer->addChild(menu, 103+i, 103+i);
        
        auto move = MoveBy::create(0.25 + 0.05 * (i + 2), Point(-w, 0));
        menu->runAction(move);
    }
     */
    
    scrollView->setContentOffset(Point(0, hc - scrollLayer->getContentSize().height));
    scrollView->setContentSize(scrollLayer->getContentSize());
    scrollView->addChild(scrollLayer, 100, 101);
    
    this->addChild(scrollView, 100, 100);
    
}

void HelloWorld::setMenu301_selected(int kosuu) // ここに購入処理を入れないと
{
    ScrollView* check = (ScrollView*)this->getChildByTag(100);
    if(!(check->touchEndPointCheck())){ return; }
    
    CCLOG("mahouseki %d", kosuu);
    // debug
    if(kosuu < 2){
        tapProhibit = true;
        
        setMenu320_purchasingWait();
        
        auto delay = DelayTime::create(0.5);
        auto func = CallFunc::create(CC_CALLBACK_0(HelloWorld::setMenu301_execPurchase, this, kosuu));
        this->runAction(Sequence::create(delay, func, NULL));
        
    }
    else{
        playSE(3); // se_continue.mp3

        mahouseki += MahuosekiList[kosuu];
        updateUserDataDb();
        updateTopMenu();
        
        globalMenu(300);
    }
    
}

void HelloWorld::setMenu301_execPurchase(int kosuu)
{
    CCLOG("%s", inappPurchaseItems[kosuu].c_str());
    InappPurchaseManager::setDelegate((InappPurchaseDelegate *)this);
    InappPurchaseManager::purchaseForItemId(inappPurchaseItems[kosuu].c_str());
    
}

void HelloWorld::setMenu301_mahousekiPreset()
{
    pageTitleOutAndRemove(310);
    removeMenu();
    
    Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image->setContentSize(Size(w * 0.9 , w * 0.5));
    Size tmpSize = image->getContentSize();
    image->setPosition(Point(w * 1.5, h * 0.7 - w * 0.2));
    
    auto label = LabelTTF::create(LocalizedString::getLocalizedString("", ""), FONT, LEVEL_FONT_SIZE);
    label->setAnchorPoint(Point(0, 0.5));
    label->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.5));
    label->setFontFillColor(Color3B(255, 255, 255));
    image->addChild(label);
    
    this->addChild(image, 100, 100);
    
    auto move = MoveBy::create(0.25 + 0.05, Point(-w, 0));
    image->runAction(move);
    
    
    // 本文
    
    auto label2 = LabelTTF::create(LocalizedString::getLocalizedString("mahousekiPresentMessage1", "mahousekiPresentMessage1"), FONT, LEVEL_FONT_SIZE);
    label2->setAnchorPoint(Point(0, 0.5));
    label2->setPosition(tmpSize.width * 0.05, tmpSize.height * 0.85);
    label2->setFontFillColor(Color3B(255, 255, 255));
    image->addChild(label2, 210, 210);
    
    auto label3 = LabelTTF::create(LocalizedString::getLocalizedString("mahousekiPresentMessage2", "mahousekiPresentMessage2"), FONT, LEVEL_FONT_SIZE);
    label3->setAnchorPoint(Point(0, 0.5));
    label3->setPosition(tmpSize.width * 0.05, tmpSize.height * 0.75);
    label3->setFontFillColor(Color3B(255, 255, 255));
    image->addChild(label3, 211, 211);
    
    auto mahousekiTimeLastLabel = LabelTTF::create("Calcutating...", FONT, LEVEL_FONT_SIZE);
    mahousekiTimeLastLabel->setAnchorPoint(Point(0, 0.5));
    mahousekiTimeLastLabel->setPosition(tmpSize.width * 0.05, tmpSize.height * 0.55);
    mahousekiTimeLastLabel->setFontFillColor(Color3B(255, 255, 255));
    image->addChild(mahousekiTimeLastLabel, 212, 212);
    
    setMenu301_mahousekiPresentCalcLastTime(mahousekiTimeLastLabel);
    
    // YES/NOボタン
    Scale9Sprite* image2 = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image2->setContentSize(Size(w * 0.3 , h * 0.08 ));
    Size tmpSize2 = image2->getContentSize();
    image2->setPosition(Point::ZERO);
    
    auto label4 = LabelTTF::create(LocalizedString::getLocalizedString("morau", "YES"), FONT, LEVEL_FONT_SIZE);
    label4->setFontFillColor(Color3B(255, 255, 255));
    label4->setPosition(Point(tmpSize2.width * 0.5, tmpSize2.height * 0.5));
    image2->addChild(label4);
    
    MenuItemSprite* menuItem2 = MenuItemSprite::create(image2, image2, CC_CALLBACK_0(HelloWorld::setMenu301_mahousekiPresetSelected, this));
    menuItem2->setContentSize(tmpSize2);
    menuItem2->setPosition(Point::ZERO);
    
    Menu* menu2 = Menu::create(menuItem2, NULL);
    menu2->setContentSize(tmpSize2);
    menu2->setPosition(Point(tmpSize.width * 0.7, tmpSize.height * 0.3));
    image->addChild(menu2, 214, 214);
    
    
    Scale9Sprite* image3 = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image3->setContentSize(Size(w * 0.3 , h * 0.08 ));
    Size tmpSize3 = image3->getContentSize();
    image3->setPosition(Point::ZERO);
    
    label = LabelTTF::create(LocalizedString::getLocalizedString("back", "NO"), FONT, LEVEL_FONT_SIZE);
    label->setFontFillColor(Color3B(255, 255, 255));
    label->setPosition(Point(tmpSize3.width * 0.5, tmpSize3.height * 0.5));
    image3->addChild(label);
    
    MenuItemSprite* menuItem3 = MenuItemSprite::create(image3, image3, CC_CALLBACK_0(HelloWorld::setMenu301_mahousekiPresetClancel, this));
    menuItem3->setContentSize(tmpSize3);
    menuItem3->setPosition(Point::ZERO);
    
    Menu* menu3 = Menu::create(menuItem3, NULL);
    menu3->setContentSize(tmpSize3);
    menu3->setPosition(Point(tmpSize.width * 0.3, tmpSize.height * 0.3));
    image->addChild(menu3, 215, 215);
    
    
}

void HelloWorld::setMenu301_mahousekiPresentCalcLastTime(LabelTTF* tmpLabel)
{
    char tmpChar[64];
    bool nextLoop = false;
    
    if(time(NULL) - lastMahousekiGotTime < MAHOUSEKITIME){
        long tmpTimeDiff = MAHOUSEKITIME - (time(NULL) - lastMahousekiGotTime);
        sprintf(tmpChar, "%s %02ld:%02ld:%02ld", LocalizedString::getLocalizedString("last", "last"),
                (tmpTimeDiff / 3600) % 24,
                (tmpTimeDiff / 60 ) % 60,
                tmpTimeDiff % 60);
        
        nextLoop = true;
    }
    else{
        sprintf(tmpChar, "%s", LocalizedString::getLocalizedString("uketoreru", "uketoreru"));
    }
    if(tmpLabel->getReferenceCount()){
        //CCLOG("%s", tmpChar);
        tmpLabel->setString(tmpChar);
        
        if(nextLoop){
            auto delay = DelayTime::create(1.0);
            auto func = CallFunc::create(CC_CALLBACK_0(HelloWorld::setMenu301_mahousekiPresentCalcLastTime, this, tmpLabel));
            tmpLabel->runAction(Sequence::create(delay, func, NULL));
        }
    }
}

void HelloWorld::setMenu301_mahousekiPresetSelected()
{
    
    if(time(NULL) - lastMahousekiGotTime < MAHOUSEKITIME){
        playSE(1); // se_ng.mp3
        return;
    }
    
    CocosDenshion::SimpleAudioEngine::getInstance()->playEffect("se/se_continue.mp3");
    playSE(3); // se_continue.mp3

    mahouseki += 1;
    lastMahousekiGotTime = time(NULL);
    updateUserDataDb();
    updateTopMenu();
    
    globalMenu(300);
}

void HelloWorld::setMenu301_mahousekiPresetClancel()
{
    globalMenu(300);
}


void HelloWorld::setMenu301_mahousekiPresetTwitter()
{
    pageTitleOutAndRemove(311);
    removeMenu();
    
    Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image->setContentSize(Size(w * 0.9 , w * 0.8));
    image->setAnchorPoint(Point(0.5, 1));
    Size tmpSize = image->getContentSize();
    image->setPosition(Point(w * 1.5, h * 0.7));
    
    this->addChild(image, 100, 100);
    
    auto move = MoveBy::create(0.25, Point(-w, 0));
    image->runAction(move);
    
    
    // 本文
    auto label = LabelTTF::create(LocalizedString::getLocalizedString("mahousekiTwitterMessage1", "mahousekiPresentMessage1"), FONT, LEVEL_FONT_SIZE);
    label->setAnchorPoint(Point(0, 0.5));
    Size tmpHeight = label->getContentSize();
    label->setPosition(tmpSize.width * 0.05, tmpSize.height * 0.90);
    label->setFontFillColor(Color3B(255, 255, 255));
    image->addChild(label, 210, 210);
    
    auto label2 = LabelTTF::create(LocalizedString::getLocalizedString("mahousekiTwitterMessage2", "mahousekiPresentMessage2"), FONT, LEVEL_FONT_SIZE);
    label2->setAnchorPoint(Point(0, 0.5));
    label2->setPosition(tmpSize.width * 0.05, tmpSize.height * 0.90 - tmpHeight.height * 1);
    label2->setFontFillColor(Color3B(255, 255, 255));
    image->addChild(label2, 211, 211);
    
    auto mahousekiTimeLastLabel = LabelTTF::create("Calcutating...", FONT, LEVEL_FONT_SIZE);
    mahousekiTimeLastLabel->setAnchorPoint(Point(0, 0.5));
    mahousekiTimeLastLabel->setPosition(tmpSize.width * 0.05, tmpSize.height * 0.90 - tmpHeight.height * 3);
    mahousekiTimeLastLabel->setFontFillColor(Color3B(255, 255, 255));
    image->addChild(mahousekiTimeLastLabel, 212, 212);
    
    char tmpChar[64];
    for(int i = 0; i < 7; i++){
        sprintf(tmpChar, "twitterToukou%d", i);
        label = LabelTTF::create(LocalizedString::getLocalizedString(tmpChar, tmpChar), FONT, LEVEL_FONT_SIZE);
        label->setAnchorPoint(Point(0, 0.5));
        label->setPosition(tmpSize.width * 0.05, tmpSize.height * 0.90 - tmpHeight.height * (5+i));
        label->setFontFillColor(Color3B(255, 255, 255));
        image->addChild(label, 212+i, 21+i);
        
    }
    
    // YES/NOボタン
    Scale9Sprite* image2 = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image2->setContentSize(Size(w * 0.7 , h * 0.08 ));
    Size tmpSize2 = image2->getContentSize();
    image2->setPosition(Point::ZERO);
    
    label = LabelTTF::create(LocalizedString::getLocalizedString("twitterRenkei", "twitterRenkei"), FONT, LEVEL_FONT_SIZE);
    label->setFontFillColor(Color3B(255, 255, 255));
    label->setPosition(Point(tmpSize2.width * 0.5, tmpSize2.height * 0.5));
    image2->addChild(label);
    
    MenuItemSprite* menuItem2 = MenuItemSprite::create(image2, image2, CC_CALLBACK_0(HelloWorld::setMenu301_twitterRenkei, this));
    menuItem2->setContentSize(tmpSize2);
    menuItem2->setPosition(Point::ZERO);
    
    Menu* menu2 = Menu::create(menuItem2, NULL);
    menu2->setContentSize(tmpSize2);
    menu2->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.15));
    image->addChild(menu2, 214, 214);
    
}

void HelloWorld::setMenu301_twitterRenkei()
{
    NativeManager::twitterRenkei();
}

void HelloWorld::setMenu320_purchasingWait()
{
    pageTitleOutAndRemove(320);
    removeMenu();
    
    playSE(0); // se_enter.mp3
    
    Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image->setContentSize(Size(w * 0.9 , w * 0.5));
    Size tmpSize = image->getContentSize();
    image->setPosition(Point(w * 1.5, h * 0.7 - w * 0.2));
    
    auto label = LabelTTF::create(LocalizedString::getLocalizedString("", ""), FONT, LEVEL_FONT_SIZE);
    label->setAnchorPoint(Point(0, 0.5));
    label->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.5));
    label->setFontFillColor(Color3B(255, 255, 255));
    image->addChild(label);
    
    this->addChild(image, 100, 100);
    
    auto move = MoveBy::create(0.25 + 0.05, Point(-w, 0));
    image->runAction(move);
    
    
    // 本文
    
    label = LabelTTF::create(LocalizedString::getLocalizedString("mahousekiPurchaseWaitMessage1", "mahousekiPurchaseWaitMessage1"), FONT, LEVEL_FONT_SIZE);
    label->setAnchorPoint(Point(0, 0.5));
    label->setPosition(tmpSize.width * 0.05, tmpSize.height * 0.85);
    label->setFontFillColor(Color3B(255, 255, 255));
    image->addChild(label, 210, 210);
    
    label = LabelTTF::create(LocalizedString::getLocalizedString("mahousekiPurchaseWaitMessage2", "mahousekiPurchaseWaitMessage2"), FONT, LEVEL_FONT_SIZE);
    label->setAnchorPoint(Point(0, 0.5));
    label->setPosition(tmpSize.width * 0.05, tmpSize.height * 0.75);
    label->setFontFillColor(Color3B(255, 255, 255));
    image->addChild(label, 211, 211);
    
}

void HelloWorld::setMenu320_purchased(bool purchaseResult, std::string itemId)
{
    pageTitleOutAndRemove(320);
    removeMenu();
    
    Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image->setContentSize(Size(w * 0.9 , w * 0.5));
    Size tmpSize = image->getContentSize();
    image->setPosition(Point(w * 1.5, h * 0.7 - w * 0.2));
    
    auto label = LabelTTF::create(LocalizedString::getLocalizedString("", ""), FONT, LEVEL_FONT_SIZE);
    label->setAnchorPoint(Point(0, 0.5));
    label->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.5));
    label->setFontFillColor(Color3B(255, 255, 255));
    image->addChild(label);
    
    this->addChild(image, 100, 100);
    
    auto move = MoveBy::create(0.25 + 0.05, Point(-w, 0));
    image->runAction(move);
    
    
    // 本文
    if(purchaseResult){
        label = LabelTTF::create(LocalizedString::getLocalizedString("mahousekiPurchasedOKMessage1", "mahousekiPurchasedOKMessage1"), FONT, LEVEL_FONT_SIZE);
    }
    else{
        label = LabelTTF::create(LocalizedString::getLocalizedString("mahousekiPurchasedNGMessage1", "mahousekiPurchasedNGMessage1"), FONT, LEVEL_FONT_SIZE);
        
    }
    label->setAnchorPoint(Point(0, 0.5));
    label->setPosition(tmpSize.width * 0.05, tmpSize.height * 0.85);
    label->setFontFillColor(Color3B(255, 255, 255));
    image->addChild(label, 210, 210);
    
    if(purchaseResult){
        int purchasedItem = -1;
        for(int i = 0; i < MAHOUSEKITYPESDEBUG; i++){
            
            CCLOG("strcmp %s, %s, %d", itemId.c_str(), inappPurchaseItems[i].c_str(), strcmp(itemId.c_str(), inappPurchaseItems[i].c_str()));
            
            if(strcmp(itemId.c_str(), inappPurchaseItems[i].c_str()) == 0){
                purchasedItem = i;
                break;
            }
        }
        
        if(purchasedItem != -1){
            label = LabelTTF::create(LocalizedString::getLocalizedString("mahouseki", "mahouseki"), FONT, LEVEL_FONT_SIZE);
            label->setAnchorPoint(Point(0, 0.5));
            label->setPosition(tmpSize.width * 0.05, tmpSize.height * 0.65);
            label->setFontFillColor(Color3B(255, 255, 255));
            image->addChild(label, 210, 210);
            
            char tmpChar[64];
            sprintf(tmpChar, "%d %s %d", mahouseki - inappPurchaseMahouseki[purchasedItem], LocalizedString::getLocalizedString("right", "right"), mahouseki);
            
            label = LabelTTF::create(tmpChar, FONT, LEVEL_FONT_SIZE);
            label->setAnchorPoint(Point(0, 0.5));
            label->setPosition(tmpSize.width * 0.15, tmpSize.height * 0.5);
            label->setFontFillColor(Color3B(255, 255, 255));
            image->addChild(label, 210, 210);
            
        }
    }
    
}


void HelloWorld::setMenu302(bool staminaOrButtleStamina, bool warningMessage) // スタミナ回復
{
    Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image->setContentSize(Size(w * 0.9 , w * 0.5));
    Size tmpSize = image->getContentSize();
    image->setPosition(Point(w * 1.5, h * 0.7 - w * 0.2));
    
    auto label = LabelTTF::create(LocalizedString::getLocalizedString("", ""), FONT, LEVEL_FONT_SIZE);
    label->setAnchorPoint(Point(0, 0.5));
    label->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.5));
    label->setFontFillColor(Color3B(255, 255, 255));
    image->addChild(label);
    
    this->addChild(image, 100, 100);
    
    auto move = MoveBy::create(0.25 + 0.05, Point(-w, 0));
    image->runAction(move);
    
    
    // 本文
    if(warningMessage){
        if (staminaOrButtleStamina == true) { // スタミナ足りない
            label = LabelTTF::create(LocalizedString::getLocalizedString("staminaWarning", "staminaWarning"), FONT, LEVEL_FONT_SIZE * 1.2);
        }
        else{
            label = LabelTTF::create(LocalizedString::getLocalizedString("battleStaminaWarning", "battleStaminaWarning"), FONT, LEVEL_FONT_SIZE * 1.2);
        }
        label->setAnchorPoint(Point(0, 0.5));
        label->setPosition(tmpSize.width * 0.05, tmpSize.height * 0.85);
        label->setFontFillColor(Color3B(255, 255, 255));
        image->addChild(label, 210, 210);
    }
    
    
    label = LabelTTF::create(LocalizedString::getLocalizedString("staminaMessage1", "staminaMessage1"), FONT, LEVEL_FONT_SIZE);
    label->setAnchorPoint(Point(0, 0.5));
    label->setPosition(tmpSize.width * 0.05, tmpSize.height * 0.75);
    label->setFontFillColor(Color3B(255, 255, 255));
    image->addChild(label, 210, 210);
    
    if(staminaOrButtleStamina == true){
        label = LabelTTF::create(LocalizedString::getLocalizedString("staminaMessage2", "staminaMessage2"), FONT, LEVEL_FONT_SIZE);
    }
    else{
        label = LabelTTF::create(LocalizedString::getLocalizedString("staminaMessage2-2", "staminaMessage2"), FONT, LEVEL_FONT_SIZE);
    }
    label->setAnchorPoint(Point(0, 0.5));
    label->setPosition(tmpSize.width * 0.05, tmpSize.height * 0.65);
    label->setFontFillColor(Color3B(255, 255, 255));
    image->addChild(label, 211, 211);
    
    label = LabelTTF::create(LocalizedString::getLocalizedString("staminaMessage3", "staminaMessage3"), FONT, LEVEL_FONT_SIZE);
    label->setAnchorPoint(Point(0, 0.5));
    label->setPosition(tmpSize.width * 0.05, tmpSize.height * 0.55);
    label->setFontFillColor(Color3B(255, 255, 255));
    image->addChild(label, 212, 212);
    
    
    // ボタン
    // YES/NOボタン
    
    Scale9Sprite* image2 = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image2->setContentSize(Size(w * 0.3 , h * 0.08 ));
    Size tmpSize2 = image2->getContentSize();
    image2->setPosition(Point::ZERO);
    
    label = LabelTTF::create(LocalizedString::getLocalizedString("staminaYes", "YES"), FONT, LEVEL_FONT_SIZE);
    label->setFontFillColor(Color3B(255, 255, 255));
    label->setPosition(Point(tmpSize2.width * 0.5, tmpSize2.height * 0.5));
    image2->addChild(label);
    
    MenuItemSprite* menuItem2 = MenuItemSprite::create(image2, image2, CC_CALLBACK_0(HelloWorld::setMenu302_staminaKaifukuExec, this, staminaOrButtleStamina));
    menuItem2->setContentSize(tmpSize2);
    menuItem2->setPosition(Point::ZERO);
    
    Menu* menu2 = Menu::create(menuItem2, NULL);
    menu2->setContentSize(tmpSize2);
    menu2->setPosition(Point(tmpSize.width * 0.7, tmpSize.height * 0.25));
    image->addChild(menu2, 214, 214);
    
    
    Scale9Sprite* image3 = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image3->setContentSize(Size(w * 0.3 , h * 0.08 ));
    Size tmpSize3 = image3->getContentSize();
    image3->setPosition(Point::ZERO);
    
    label = LabelTTF::create(LocalizedString::getLocalizedString("staminaNo", "NO"), FONT, LEVEL_FONT_SIZE);
    label->setFontFillColor(Color3B(255, 255, 255));
    label->setPosition(Point(tmpSize3.width * 0.5, tmpSize3.height * 0.5));
    image3->addChild(label);
    
    MenuItemSprite* menuItem3 = MenuItemSprite::create(image3, image3, CC_CALLBACK_0(HelloWorld::setMenu302_cancel, this));
    menuItem3->setContentSize(tmpSize3);
    menuItem3->setPosition(Point::ZERO);
    
    Menu* menu3 = Menu::create(menuItem3, NULL);
    menu3->setContentSize(tmpSize3);
    menu3->setPosition(Point(tmpSize.width * 0.3, tmpSize.height * 0.25));
    image->addChild(menu3, 215, 215);
    
}


void HelloWorld::setMenu302_staminaKaifukuExec(bool staminaOrButtleStamina)
{
    if(mahouseki + mahouseki_purchase == 0){
        playSE(1); // se_ng.mp3
        menuTapped(301);
        return;
    }
    
    if(staminaOrButtleStamina == true){
        if(stamina == staminaMax){
            playSE(1); // se_ng.mp3
            return;
        }
        stamina = staminaMax;
    }
    else{
        if(battleStamina == battleStaminaMax){
            playSE(1); // se_ng.mp3
            return;
        }
        battleStamina = battleStaminaMax;
    }
    
    expendMahouseki(1);
    updateTopMenu();
    playSE(3); // se_continue.mp3
    
    globalMenu(300);
}
void HelloWorld::setMenu302_cancel()
{
    globalMenu(300);
}


//////////////////////////////////////////////////
//////////////////////////////////////////////////
//////////////////////////////////////////////////
//  メニュー４００系　スキル
//////////////////////////////////////////////////
//////////////////////////////////////////////////
//////////////////////////////////////////////////

void HelloWorld::setMenu401(int sortId) // スキル一覧
{
    pageTitleOutAndRemove(401);
    removeMenu();
    
    int disp_count = 0;
    
    ScrollView *scrollView = ScrollView::create();
    scrollView->setViewSize(Size(w, hc));
    scrollView->setPosition(Point(0, yg));
    
    LayerColor* scrollLayer = LayerColor::create(Color4B(255, 255, 255, SCROLLVIEWOPACITY));
    scrollLayer->changeWidth(w);
    
    sqlite3* db = NULL;
    std::string fullpath = FileUtils::getInstance()->getWritablePath();
    fullpath += "userdata.db";
    
    if (sqlite3_open(fullpath.c_str(), &db) == SQLITE_OK) {
        sqlite3_stmt *stmt = NULL;
        
        std::string sql = "select * from user_skill_list";
        if(sortId == 0){ sql += " order by skill_id asc"; }
        if(sortId == 1){ sql += " order by skill_id desc"; }
        if(sortId == 2){ sql += " order by level asc"; }
        if(sortId == 3){ sql += " order by level desc"; }
        if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
            
            sqlite3_stmt * stmt_count = stmt;
            int count = 0;
            while (sqlite3_step(stmt_count) == SQLITE_ROW) {
                count++;
            }
            CCLOG("count %d", count);
            float scrollLayerHeight =  (w * 0.18) * count + h * 0.1 ;
            scrollLayer->changeHeight(scrollLayerHeight);
            scrollLayer->setAnchorPoint(Point(0, 1));
            scrollLayer->setPosition(Point::ZERO);
            
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int _id             = (int)sqlite3_column_int(stmt, 0);
                int skill_id        = (int)sqlite3_column_int(stmt, 1);
                int level           = (int)sqlite3_column_int(stmt, 2);
                int soubi_unit_id   = (int)sqlite3_column_int(stmt, 3);
                
                Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(30, 30, 120, 120));
                image->setContentSize(Size(w * 0.9 , w * 0.18 ));
                Size tmpSize = image->getContentSize();
                image->setPosition(Point::ZERO);
                
                char tmpChar[64];
                sprintf(tmpChar, "skillName%d", skill_id);
                auto label = LabelTTF::create(LocalizedString::getLocalizedString(tmpChar, tmpChar), FONT, LEVEL_FONT_SIZE);
                label->setAnchorPoint(Point(0, 0.5));
                label->setFontFillColor(Color3B(255, 255, 255));
                label->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.70));
                image->addChild(label);
                
                sprintf(tmpChar, "skill_detail%d", skill_id);
                auto label2 = LabelTTF::create(LocalizedString::getLocalizedString(tmpChar, tmpChar), FONT, LEVEL_FONT_SIZE*0.8, Size(tmpSize.width*0.9, NULL), TextHAlignment::LEFT, TextVAlignment::TOP);
                label2->setAnchorPoint(Point(0, 0.5));
                label2->setFontFillColor(Color3B(255, 255, 255));
                label2->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.35));
                image->addChild(label2);
                
                sprintf(tmpChar, "Lv.%d", level);
                auto label3 = LabelTTF::create(tmpChar, FONT, LEVEL_FONT_SIZE*0.8);
                label3->setAnchorPoint(Point(1, 0));
                label3->setFontFillColor(Color3B(255, 255, 255));
                label3->setPosition(Point(tmpSize.width * 0.95, tmpSize.height * 0.7));
                image->addChild(label3);
                
                if(soubi_unit_id){
                    sprintf(tmpChar, "%d%s", soubi_unit_id, LocalizedString::getLocalizedString("issoubi", "issoubi"));
                    auto label4 = LabelTTF::create(tmpChar, FONT, LEVEL_FONT_SIZE*0.8);
                    label4->setAnchorPoint(Point(1, 1));
                    label4->setFontFillColor(Color3B(255, 255, 255));
                    label4->setPosition(Point(tmpSize.width * 0.95, tmpSize.height * 0.7));
                    image->addChild(label4);
                }
                else{
                    auto label5 = LabelTTF::create(LocalizedString::getLocalizedString("noonesoubi", "noonesoubi"), FONT, LEVEL_FONT_SIZE*0.8);
                    label5->setAnchorPoint(Point(1, 1));
                    label5->setFontFillColor(Color3B(255, 255, 255));
                    label5->setPosition(Point(tmpSize.width * 0.95, tmpSize.height * 0.7));
                    image->addChild(label5);
                }
                
                MenuItemSprite* menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::blank, this));
                menuItem->setContentSize(tmpSize);
                menuItem->setPosition(Point::ZERO);
                
                Menu* menu = Menu::create2(menuItem, NULL);
                menu->setContentSize(tmpSize);
                menu->setPosition(Point( w * 0.5 + w, scrollLayerHeight - h * 0.15 - w * 0.18 * disp_count ));
                
                scrollLayer->addChild(menu, 200+disp_count, 200+disp_count);
                
                auto move = MoveBy::create(0.25 + 0.05 * disp_count, Point(-w, 0));
                menu->runAction(move);
                                
                disp_count++;
                
            }
        }
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    
    scrollView->setContentOffset(Point(0, hc - scrollLayer->getContentSize().height));
    scrollView->setContentSize(scrollLayer->getContentSize());
    scrollView->addChild(scrollLayer, 100, 101);
    
    this->addChild(scrollView, 100, 100);
    
    
    // ソート用ボタン
    Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image->setContentSize(Size(w * 0.2 , h * 0.08 ));
    Size tmpSize = image->getContentSize();
    image->setPosition(Point::ZERO);
    
    char tmpChar[64];
    sprintf(tmpChar, "sortskill%d", sortId);
    auto label = LabelTTF::create(LocalizedString::getLocalizedString(tmpChar, tmpChar), FONT, LEVEL_FONT_SIZE);
    label->setFontFillColor(Color3B(255, 255, 255));
    label->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.5));
    image->addChild(label);
    
    int nextSortId = sortId + 1;
    if (nextSortId == 4) { nextSortId = 0; }
    
    MenuItemSprite* menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::setMenu401, this, nextSortId));
    menuItem->setContentSize(tmpSize);
    menuItem->setPosition(Point::ZERO);
    
    Menu* menu = Menu::create(menuItem, NULL);
    menu->setContentSize(tmpSize);
    menu->setPosition(Point( w * 1.9, h * 0.81 ));
    
    this->addChild(menu, 105, 105);
    
    auto move = MoveBy::create(0.25, Point(-w, 0));
    menu->runAction(move);
    
    
}



void HelloWorld::setMenu490_skillListFromUnitDetail(int sortId, int user_unit_id, int previousSoubiSkillId) // ユニット詳細から呼ばれるスキル装備変更
{
    pageTitleOutAndRemove(230);
    removeMenu();
    
    int disp_count = 0;
    
    ScrollView *scrollView = ScrollView::create();
    scrollView->setViewSize(Size(w, hc));
    scrollView->setPosition(Point(0, yg));
    
    LayerColor* scrollLayer = LayerColor::create(Color4B(255, 255, 255, SCROLLVIEWOPACITY));
    scrollLayer->changeWidth(w);
    
    sqlite3* db = NULL;
    std::string fullpath = FileUtils::getInstance()->getWritablePath();
    fullpath += "userdata.db";
    
    if (sqlite3_open(fullpath.c_str(), &db) == SQLITE_OK) {
        sqlite3_stmt *stmt = NULL;
        
        std::string sql = "select * from user_skill_list";
        if(sortId == 0){ sql += " order by skill_id asc"; }
        if(sortId == 1){ sql += " order by skill_id desc"; }
        if(sortId == 2){ sql += " order by level asc"; }
        if(sortId == 3){ sql += " order by level desc"; }
        if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
            
            sqlite3_stmt * stmt_count = stmt;
            int count = 0;
            while (sqlite3_step(stmt_count) == SQLITE_ROW) {
                count++;
            }
            float scrollLayerHeight =  (w * 0.18) * count + h * 0.1 ;
            if(previousSoubiSkillId != 0){
                scrollLayerHeight += w * 0.18;
            }
            scrollLayer->changeHeight(scrollLayerHeight);
            scrollLayer->setAnchorPoint(Point(0, 1));
            scrollLayer->setPosition(Point::ZERO);
            
            if(previousSoubiSkillId != 0){
                // 装備スキルを外す
                Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(30, 30, 120, 120));
                image->setContentSize(Size(w * 0.9 , w * 0.18 ));
                Size tmpSize = image->getContentSize();
                image->setPosition(Point::ZERO);
                
                auto label = LabelTTF::create(LocalizedString::getLocalizedString("hazusu", "hazusu"), FONT, LEVEL_FONT_SIZE);
                label->setAnchorPoint(Point(0, 0.5));
                label->setFontFillColor(Color3B(255, 255, 255));
                label->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.5));
                image->addChild(label);
                
                MenuItemSprite* menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::setMenu490_skillListFromUnitDetail_selected, this, user_unit_id, 0, previousSoubiSkillId));
                menuItem->setContentSize(tmpSize);
                menuItem->setPosition(Point::ZERO);
                
                Menu* menu = Menu::create2(menuItem, NULL);
                menu->setContentSize(tmpSize);
                menu->setPosition(Point( w * 0.5 + w, scrollLayerHeight - h * 0.15 - w * 0.18 * disp_count ));
                
                scrollLayer->addChild(menu, 200+disp_count, 200+disp_count);
                
                auto move = MoveBy::create(0.25 + 0.05 * disp_count, Point(-w, 0));
                menu->runAction(move);
                
                disp_count++;
            }
            
            
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int _id             = (int)sqlite3_column_int(stmt, 0);
                int skill_id        = (int)sqlite3_column_int(stmt, 1);
                int level           = (int)sqlite3_column_int(stmt, 2);
                int soubi_unit_id   = (int)sqlite3_column_int(stmt, 3);
                
                Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(30, 30, 120, 120));
                image->setContentSize(Size(w * 0.9 , w * 0.18 ));
                Size tmpSize = image->getContentSize();
                image->setPosition(Point::ZERO);
                
                char tmpChar[64];
                sprintf(tmpChar, "skillName%d", skill_id);
                auto label = LabelTTF::create(LocalizedString::getLocalizedString(tmpChar, tmpChar), FONT, LEVEL_FONT_SIZE);
                label->setAnchorPoint(Point(0, 0.5));
                label->setFontFillColor(Color3B(255, 255, 255));
                label->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.7));
                image->addChild(label);
                
                sprintf(tmpChar, "skill_detail%d", skill_id);
                auto label2 = LabelTTF::create(LocalizedString::getLocalizedString(tmpChar, tmpChar), FONT, LEVEL_FONT_SIZE*0.8, Size(tmpSize.width*0.9, NULL), TextHAlignment::LEFT, TextVAlignment::TOP);
                label2->setAnchorPoint(Point(0, 0.5));
                label2->setFontFillColor(Color3B(255, 255, 255));
                label2->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.35));
                image->addChild(label2);
                
                sprintf(tmpChar, "Lv.%d", level);
                auto label3 = LabelTTF::create(tmpChar, FONT, LEVEL_FONT_SIZE*0.8);
                label3->setAnchorPoint(Point(1, 0));
                label3->setFontFillColor(Color3B(255, 255, 255));
                label3->setPosition(Point(tmpSize.width * 0.95, tmpSize.height * 0.7));
                image->addChild(label3);
                
                if(soubi_unit_id){
                    // 装備されちゃっているものは黒枠
                    sprintf(tmpChar, "%d%s", soubi_unit_id, LocalizedString::getLocalizedString("issoubi", "issoubi"));
                    auto label4 = LabelTTF::create(tmpChar, FONT, LEVEL_FONT_SIZE*0.8);
                    label4->setAnchorPoint(Point(1, 1));
                    label4->setFontFillColor(Color3B(255, 255, 255));
                    label4->setPosition(Point(tmpSize.width * 0.95, tmpSize.height * 0.7));
                    image->addChild(label4);
                    
                    Scale9Sprite* image2 = Scale9Sprite::create("noselectable.png", Rect(0, 0, 180, 180), Rect(30, 30, 120, 120));
                    image2->setContentSize(tmpSize);
                    image2->setOpacity(0x80);
                    image2->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.5));
                    image->addChild(image2);
                    
                    image->setPosition(Point( w * 0.5 + w, scrollLayerHeight - h * 0.15 - w * 0.18 * disp_count ));
                    scrollLayer->addChild(image, 200+disp_count, 200+disp_count);
                    
                    auto move = MoveBy::create(0.25 + 0.05 * disp_count, Point(-w, 0));
                    image->runAction(move);
                    
                }
                else{
                    auto label5 = LabelTTF::create(LocalizedString::getLocalizedString("noonesoubi", "noonesoubi"), FONT, LEVEL_FONT_SIZE*0.8);
                    label5->setAnchorPoint(Point(1, 1));
                    label5->setFontFillColor(Color3B(255, 255, 255));
                    label5->setPosition(Point(tmpSize.width * 0.95, tmpSize.height * 0.7));
                    image->addChild(label5);
                    
                    MenuItemSprite* menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::setMenu490_skillListFromUnitDetail_selected, this, user_unit_id, _id, previousSoubiSkillId));
                    menuItem->setContentSize(tmpSize);
                    menuItem->setPosition(Point::ZERO);
                    
                    Menu* menu = Menu::create2(menuItem, NULL);
                    menu->setContentSize(tmpSize);
                    menu->setPosition(Point( w * 0.5 + w, scrollLayerHeight - h * 0.15 - w * 0.18 * disp_count ));
                    
                    scrollLayer->addChild(menu, 200+disp_count, 200+disp_count);
                    
                    auto move = MoveBy::create(0.25 + 0.05 * disp_count, Point(-w, 0));
                    menu->runAction(move);
                }
                
                disp_count++;
                
            }
        }
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    
    scrollView->setContentOffset(Point(0, hc - scrollLayer->getContentSize().height));
    scrollView->setContentSize(scrollLayer->getContentSize());
    scrollView->addChild(scrollLayer, 100, 101);
    
    this->addChild(scrollView, 100, 100);
    
    
    // ソート用ボタン
    Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image->setContentSize(Size(w * 0.2 , h * 0.08 ));
    Size tmpSize = image->getContentSize();
    image->setPosition(Point::ZERO);
    
    char tmpChar[64];
    sprintf(tmpChar, "sortskill%d", sortId);
    auto label = LabelTTF::create(LocalizedString::getLocalizedString(tmpChar, tmpChar), FONT, LEVEL_FONT_SIZE);
    label->setFontFillColor(Color3B(255, 255, 255));
    label->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.5));
    image->addChild(label);
    
    int nextSortId = sortId + 1;
    if (nextSortId == 4) { nextSortId = 0; }
    
    MenuItemSprite* menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::setMenu490_skillListFromUnitDetail, this, nextSortId, user_unit_id, previousSoubiSkillId));
    menuItem->setContentSize(tmpSize);
    menuItem->setPosition(Point::ZERO);
    
    Menu* menu = Menu::create(menuItem, NULL);
    menu->setContentSize(tmpSize);
    menu->setPosition(Point( w * 1.9, h * 0.81 ));
    
    this->addChild(menu, 105, 105);
    
    auto move = MoveBy::create(0.25, Point(-w, 0));
    menu->runAction(move);
    
    
}

void HelloWorld::setMenu490_skillListFromUnitDetail_selected(int user_unit_id, int _id, int previousSoubiSkillId) // スキル装備変更実行
{
    ScrollView* check = (ScrollView*)this->getChildByTag(100);
    if(!(check->touchEndPointCheck())){ return; }
    
    sqlite3* db = NULL;
    std::string fullpath = FileUtils::getInstance()->getWritablePath();
    fullpath += "userdata.db";
    
    CCLOG("setMenu490_skillListFromUnitDetail_selected user_unit_id %d, _id(target) %d previous %d", user_unit_id, _id, previousSoubiSkillId);
    
    if (sqlite3_open(fullpath.c_str(), &db) == SQLITE_OK) {
        char tmpChar[128];
        
        // 装備を外す
        if(previousSoubiSkillId != 0){
            sprintf(tmpChar, "UPDATE user_skill_list SET soubi_unit_id = 0 where _id = %d", previousSoubiSkillId);
            sqlite3_exec(db, tmpChar, NULL, NULL, NULL);
        }
        // 装備する
        if(_id != 0){
            sprintf(tmpChar, "UPDATE user_skill_list SET soubi_unit_id = %d where _id = %d", user_unit_id, _id);
            sqlite3_exec(db, tmpChar, NULL, NULL, NULL);
        }
    }
    sqlite3_close(db);
    
    // パーティーをオンラインDB登録。
    searchOtherParties();
    
    setMenu220_unitDetail(user_unit_id, 0, false);
}

void HelloWorld::setMenu402(int sortId, bool reset) // スキル進化
{
    pageTitleOutAndRemove(402);
    removeMenu();
    
    int disp_count = 0;
    if(reset){
        for(int i = 0; i < 10; i++){
            baikyakuKouho[i] = 0;
        }
        baikyakuCoin = 0;
    }
    
    ScrollView *scrollView = ScrollView::create();
    scrollView->setViewSize(Size(w, hc));
    scrollView->setPosition(Point(0, yg));
    
    LayerColor* scrollLayer = LayerColor::create(Color4B(255, 255, 255, SCROLLVIEWOPACITY));
    scrollLayer->changeWidth(w);
    
    sqlite3* db = NULL;
    std::string fullpath = FileUtils::getInstance()->getWritablePath();
    fullpath += "userdata.db";
    
    if (sqlite3_open(fullpath.c_str(), &db) == SQLITE_OK) {
        sqlite3_stmt *stmt = NULL;
        
        std::string sql = "select * from user_skill_list";
        if(sortId == 0){ sql += " order by skill_id asc"; }
        if(sortId == 1){ sql += " order by skill_id desc"; }
        if(sortId == 2){ sql += " order by level asc"; }
        if(sortId == 3){ sql += " order by level desc"; }
        if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
            
            sqlite3_stmt * stmt_count = stmt;
            int count = 0;
            while (sqlite3_step(stmt_count) == SQLITE_ROW) {
                count++;
            }
            float scrollLayerHeight =  (w * 0.18) * count + h * 0.1 ;
            scrollLayer->changeHeight(scrollLayerHeight);
            scrollLayer->setAnchorPoint(Point(0, 1));
            scrollLayer->setPosition(Point::ZERO);
            
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int _id             = (int)sqlite3_column_int(stmt, 0);
                int skill_id        = (int)sqlite3_column_int(stmt, 1);
                int level           = (int)sqlite3_column_int(stmt, 2);
                int soubi_unit_id   = (int)sqlite3_column_int(stmt, 3);
                
                Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(30, 30, 120, 120));
                image->setContentSize(Size(w * 0.9 , w * 0.18 ));
                Size tmpSize = image->getContentSize();
                image->setPosition(Point::ZERO);
                
                char tmpChar[64];
                sprintf(tmpChar, "skillName%d", skill_id);
                auto label = LabelTTF::create(LocalizedString::getLocalizedString(tmpChar, tmpChar), FONT, LEVEL_FONT_SIZE);
                label->setAnchorPoint(Point(0, 0.5));
                label->setFontFillColor(Color3B(255, 255, 255));
                label->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.7));
                image->addChild(label);
                
                sprintf(tmpChar, "skill_detail%d", skill_id);
                auto label2 = LabelTTF::create(LocalizedString::getLocalizedString(tmpChar, tmpChar), FONT, LEVEL_FONT_SIZE*0.8, Size(tmpSize.width*0.9, NULL), TextHAlignment::LEFT, TextVAlignment::TOP);
                label2->setAnchorPoint(Point(0, 0.5));
                label2->setFontFillColor(Color3B(255, 255, 255));
                label2->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.35));
                image->addChild(label2);
                
                sprintf(tmpChar, "Lv.%d", level);
                auto label3 = LabelTTF::create(tmpChar, FONT, LEVEL_FONT_SIZE*0.8);
                label3->setAnchorPoint(Point(1, 0));
                label3->setFontFillColor(Color3B(255, 255, 255));
                label3->setPosition(Point(tmpSize.width * 0.95, tmpSize.height * 0.7));
                image->addChild(label3);

                auto label4 = LabelTTF::create(LocalizedString::getLocalizedString("noonesoubi", "noonesoubi"), FONT, LEVEL_FONT_SIZE*0.8);
                label4->setAnchorPoint(Point(1, 1));
                label4->setFontFillColor(Color3B(255, 255, 255));
                label4->setPosition(Point(tmpSize.width * 0.95, tmpSize.height * 0.7));
                image->addChild(label4);
                
                MenuItemSprite* menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::setMenu421_skillEvolutionDetail, this, skill_id, 0));
                menuItem->setContentSize(tmpSize);
                menuItem->setPosition(Point::ZERO);
                
                Menu* menu = Menu::create2(menuItem, NULL);
                menu->setContentSize(tmpSize);
                menu->setPosition(Point( w * 0.5 + w, scrollLayerHeight - h * 0.15 - w * 0.18 * disp_count ));
                
                scrollLayer->addChild(menu, 200+disp_count, 200+disp_count);
                
                auto move = MoveBy::create(0.25 + 0.05 * disp_count, Point(-w, 0));
                menu->runAction(move);
                // }
                
                disp_count++;
                
            }
        }
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    
    scrollView->setContentOffset(Point(0, hc - scrollLayer->getContentSize().height));
    scrollView->setContentSize(scrollLayer->getContentSize());
    scrollView->addChild(scrollLayer, 100, 101);
    
    this->addChild(scrollView, 100, 100);
    
    
    // ソート用ボタン
    Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image->setContentSize(Size(w * 0.2 , h * 0.08 ));
    Size tmpSize = image->getContentSize();
    image->setPosition(Point::ZERO);
    
    char tmpChar[64];
    sprintf(tmpChar, "sortskill%d", sortId);
    auto label = LabelTTF::create(LocalizedString::getLocalizedString(tmpChar, tmpChar), FONT, LEVEL_FONT_SIZE);
    label->setFontFillColor(Color3B(255, 255, 255));
    label->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.5));
    image->addChild(label);
    
    int nextSortId = sortId + 1;
    if (nextSortId == 4) { nextSortId = 0; }
    
    MenuItemSprite* menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::setMenu402, this, nextSortId, false));
    menuItem->setContentSize(tmpSize);
    menuItem->setPosition(Point::ZERO);
    
    Menu* menu = Menu::create(menuItem, NULL);
    menu->setContentSize(tmpSize);
    menu->setPosition(Point( w * 1.9, h * 0.81 ));
    
    this->addChild(menu, 105, 105);
    
    auto move = MoveBy::create(0.25, Point(-w, 0));
    menu->runAction(move);
    
}


void HelloWorld::setMenu421_skillEvolutionDetail(int _id, int pattern) // スキル進化の詳細画面
{
    pageTitleOutAndRemove(421);
    removeMenu();
    
    Scale9Sprite* image;
    Size tmpSize;
    LabelTTF* label;
    MenuItem* menuItem;
    Menu* menu;
    MoveBy* move;
    
    ScrollView *scrollView = ScrollView::create();
    scrollView->setViewSize(Size(w, hc));
    scrollView->setPosition(Point(0, yg));
    
    LayerColor* scrollLayer = LayerColor::create(Color4B(255, 255, 255, SCROLLVIEWOPACITY));
    scrollLayer->changeWidth(w);
    
    float scrollLayerHeight =  (w * 0.18) * 6 + h * 0.1 ;
    scrollLayer->changeHeight(scrollLayerHeight);
    scrollLayer->setAnchorPoint(Point(0, 1));
    scrollLayer->setPosition(Point::ZERO);
    
    int count = 0; // 指定されたスキルを使ってできるスキルがいくつあるか。左右ボタン用。
    int disp_count = 0; // 出来上がりスキルに対し、いくつのスキルが素材になるか
    int tmp_skill_to = -1;
    int tmptmp_skill_to = -1; // 進化合成先のスキル
    int not_have_count = 0;
    
    int average_level = 0;
    int level_count = 0;
    int from_level = 0;

    sqlite3* db = NULL;
    std::string fullpath = FileUtils::getInstance()->getWritablePath();
    fullpath += "gamedata.db";
    
    if (sqlite3_open(fullpath.c_str(), &db) == SQLITE_OK) {
        sqlite3_stmt *stmt = NULL;
        
        std::string sql = "select count(*) from skill_evolution where ";
        char tmpSql[128];
        sprintf(tmpSql, "skill1 = %d or skill2 = %d or skill3 = %d or skill4 = %d", _id, _id, _id, _id);
        sql += tmpSql;
        
        // 左右矢印のためのカウント
        if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
            if(sqlite3_step(stmt) == SQLITE_ROW) {
                count = (int)sqlite3_column_int(stmt, 0);
            }
            
            // 左矢印
            if(pattern != 0 && count != 0){
                image = Scale9Sprite::create("window_red.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
                image->setContentSize(Size(w * 0.1 , w * 0.1 ));
                tmpSize = image->getContentSize();
                //                image->setPosition(Point( w * (0.1 + 0.2 * disp_j) + w, h * 0.75 - w * (0.1 + 0.2 * disp_i) ));
                image->setPosition(Point::ZERO);
                
                label = LabelTTF::create(LocalizedString::getLocalizedString("left", "left"), FONT, LEVEL_FONT_SIZE);
                label->setFontFillColor(Color3B(255, 255, 255));
                label->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.5));
                image->addChild(label);
                
                menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::setMenu421_skillEvolutionDetail, this, _id, pattern-1));
                menuItem->setContentSize(tmpSize);
                menuItem->setPosition(Point::ZERO);
                
                menu = Menu::create(menuItem, NULL);
                menu->setContentSize(tmpSize);
                menu->setPosition(Point( w * 1.08, h * 0.55 ));
                
                this->addChild(menu, 190, 190);
                
                move = MoveBy::create(0.25, Point( -w, 0 ));
                menu->runAction(move);
            }
            
            // 右矢印
            
            if(pattern != count -1 && count != 0){
                image = Scale9Sprite::create("window_red.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
                image->setContentSize(Size(w * 0.1 , w * 0.1 ));
                tmpSize = image->getContentSize();
                image->setPosition(Point::ZERO);
                
                label = LabelTTF::create(LocalizedString::getLocalizedString("right", "right"), FONT, LEVEL_FONT_SIZE);
                label->setFontFillColor(Color3B(255, 255, 255));
                label->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.5));
                image->addChild(label);
                
                menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::setMenu421_skillEvolutionDetail, this, _id, pattern+1));
                menuItem->setContentSize(tmpSize);
                menuItem->setPosition(Point::ZERO);
                
                menu = Menu::create(menuItem, NULL);
                menu->setContentSize(tmpSize);
                menu->setPosition(Point( w * 1.92, h * 0.55 ));
                
                this->addChild(menu, 191, 191);
                
                move = MoveBy::create(0.25, Point( -w, 0 ));
                menu->runAction(move);
            }
            
        }
        
        // リストを出す
        sqlite3* db2 = NULL;
        std::string fullpath2 = FileUtils::getInstance()->getWritablePath();
        fullpath2 += "userdata.db";
        
        sql = "select * from skill_evolution where ";
        sprintf(tmpSql, "skill1 = %d or skill2 = %d or skill3 = %d or skill4 = %d", _id, _id, _id, _id);
        sql += tmpSql;
        CCLOG("%s", sql.c_str());
        
        
        sqlite3_stmt *stmt2 = NULL;
        if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt2, NULL) == SQLITE_OK){
            int skill_count = 0;
            while (sqlite3_step(stmt2) == SQLITE_ROW) {
                //int _id             = (int)sqlite3_column_int(stmt2, 0);
                int skill_to        = (int)sqlite3_column_int(stmt2, 1);
                //int skill1          = (int)sqlite3_column_int(stmt2, 2);
                //int skill2          = (int)sqlite3_column_int(stmt2, 3);
                //int skill3          = (int)sqlite3_column_int(stmt2, 4);
                //int skill4          = (int)sqlite3_column_int(stmt2, 5);
                
                tmp_skill_to = skill_to;
                
                if(skill_count == pattern){
                    tmptmp_skill_to = tmp_skill_to;
                    for(int i = 2; i < 6; i++){
                        int tmpSkill_id = (int)sqlite3_column_int(stmt2, i);
                        int this_time_disp = 0; // 素材スキルを持ってない場合0
                        CCLOG("1 : %d, %d, to: %d,   skill_count %d", tmpSkill_id, i, skill_to, skill_count);
                        if(tmpSkill_id != 0){  // 目的の進化後スキルに対する素材アイテムが0ではない(素材が２つの場合、for文の３つ目以降が該当)
                            
                            if (sqlite3_open(fullpath2.c_str(), &db2) == SQLITE_OK) {
                                CCLOG("2");
                                
                                sqlite3_stmt *stmt3 = NULL;
                                sql = "select * from user_skill_list where ";
                                sprintf(tmpSql, "skill_id = %d", tmpSkill_id);
                                sql += tmpSql;
                                
                                if(sqlite3_prepare_v2(db2, sql.c_str(), -1, &stmt3, NULL) == SQLITE_OK){
                                    
                                    CCLOG("3");
                                    
                                    while (sqlite3_step(stmt3) == SQLITE_ROW) { // 必要な素材を持っているか
                                        int _id             = (int)sqlite3_column_int(stmt3, 0);
                                        int skill_id        = (int)sqlite3_column_int(stmt3, 1);
                                        int level           = (int)sqlite3_column_int(stmt3, 2);
                                        int soubi_unit_id   = (int)sqlite3_column_int(stmt3, 3);
                                        this_time_disp++;
                                        
                                        average_level = average_level + level;
                                        level_count++;

                                        CCLOG("4");
                                        
                                        Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(30, 30, 120, 120));
                                        image->setContentSize(Size(w * 0.9 , w * 0.18 ));
                                        Size tmpSize = image->getContentSize();
                                        image->setPosition(Point::ZERO);
                                        
                                        char tmpChar[64];
                                        sprintf(tmpChar, "skillName%d", skill_id);
                                        auto label = LabelTTF::create(LocalizedString::getLocalizedString(tmpChar, tmpChar), FONT, LEVEL_FONT_SIZE);
                                        label->setAnchorPoint(Point(0, 0.5));
                                        label->setFontFillColor(Color3B(255, 255, 255));
                                        label->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.7));
                                        image->addChild(label);
                                        
                                        sprintf(tmpChar, "skill_detail%d", skill_id);
                                        auto label2 = LabelTTF::create(LocalizedString::getLocalizedString(tmpChar, tmpChar), FONT, LEVEL_FONT_SIZE*0.8, Size(tmpSize.width*0.9, NULL), TextHAlignment::LEFT, TextVAlignment::TOP);
                                        label2->setAnchorPoint(Point(0, 0.5));
                                        label2->setFontFillColor(Color3B(255, 255, 255));
                                        label2->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.35));
                                        image->addChild(label2);

                                        sprintf(tmpChar, "Lv.%d", level);
                                        auto label3 = LabelTTF::create(tmpChar, FONT, LEVEL_FONT_SIZE*0.8);
                                        label3->setAnchorPoint(Point(1, 0));
                                        label3->setFontFillColor(Color3B(255, 255, 255));
                                        label3->setPosition(Point(tmpSize.width * 0.95, tmpSize.height * 0.7));
                                        image->addChild(label3);
                                        
                                        if(soubi_unit_id){
                                            sprintf(tmpChar, "%d%s", soubi_unit_id, LocalizedString::getLocalizedString("issoubi", "issoubi"));
                                            auto label4 = LabelTTF::create(tmpChar, FONT, LEVEL_FONT_SIZE*0.8);
                                            label4->setAnchorPoint(Point(1, 1));
                                            label4->setFontFillColor(Color3B(255, 255, 255));
                                            label4->setPosition(Point(tmpSize.width * 0.95, tmpSize.height * 0.7));
                                            image->addChild(label4);
                                        }
                                        else{
                                            auto label5 = LabelTTF::create(LocalizedString::getLocalizedString("noonesoubi", "noonesoubi"), FONT, LEVEL_FONT_SIZE*0.8);
                                            label5->setAnchorPoint(Point(1, 1));
                                            label5->setFontFillColor(Color3B(255, 255, 255));
                                            label5->setPosition(Point(tmpSize.width * 0.95, tmpSize.height * 0.7));
                                            image->addChild(label5);
                                        }
                                        
                                        MenuItemSprite* menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::blank, this));
                                        menuItem->setContentSize(tmpSize);
                                        menuItem->setPosition(Point::ZERO);
                                        
                                        Menu* menu = Menu::create2(menuItem, NULL);
                                        menu->setContentSize(tmpSize);
                                        menu->setPosition(Point( w * 0.5 + w, scrollLayerHeight - h * 0.15 - w * 0.18 * disp_count ));
                                        
                                        scrollLayer->addChild(menu, 200+disp_count, 200+disp_count);
                                        
                                        auto move = MoveBy::create(0.25 + 0.05 * disp_count, Point(-w, 0));
                                        menu->runAction(move);
                                        
                                        disp_count++;
                                    }// while
                                }//if()
                                sqlite3_reset(stmt3);
                                sqlite3_finalize(stmt3);
                            }
                            sqlite3_close(db2);
                        }//if(tmpSkill_id != 0){
                        if(this_time_disp == 0 && tmpSkill_id != 0){ // 必要な素材スキルを持ってない
                            not_have_count++;
                            
                            Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(30, 30, 120, 120));
                            image->setContentSize(Size(w * 0.9 , w * 0.18 ));
                            Size tmpSize = image->getContentSize();
                            image->setPosition(Point::ZERO);
                            
                            char tmpChar[64];
                            sprintf(tmpChar, "skillName%d", tmpSkill_id);
                            auto label = LabelTTF::create(LocalizedString::getLocalizedString(tmpChar, tmpChar), FONT, LEVEL_FONT_SIZE);
                            label->setAnchorPoint(Point(0, 0.5));
                            label->setFontFillColor(Color3B(255, 255, 255));
                            label->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.5));
                            image->addChild(label);
                            
                            // 持ってないスキルは詳細を出さない
                            /*
                            sprintf(tmpChar, "skill_detail%d", tmpSkill_id);
                            label = LabelTTF::create(LocalizedString::getLocalizedString(tmpChar, tmpChar), FONT, LEVEL_FONT_SIZE*0.8, Size(tmpSize.width*0.55, NULL), TextHAlignment::LEFT, TextVAlignment::TOP);
                            label->setAnchorPoint(Point(0, 1));
                            label->setFontFillColor(Color3B(255, 255, 255));
                            label->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.5));
                            image->addChild(label);
                             */

                            auto label2 = LabelTTF::create(LocalizedString::getLocalizedString("nothave", "nothave"), FONT, LEVEL_FONT_SIZE*0.8);
                            label2->setAnchorPoint(Point(1, 1));
                            label2->setFontFillColor(Color3B(255, 255, 255));
                            label2->setPosition(Point(tmpSize.width * 0.95, tmpSize.height * 0.5));
                            image->addChild(label2);
                            
                            Scale9Sprite* image2 = Scale9Sprite::create("noselectable.png", Rect(0, 0, 180, 180), Rect(30, 30, 120, 120));
                            image2->setContentSize(tmpSize);
                            image2->setOpacity(0x80);
                            image2->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.5));
                            image->addChild(image2);
                            
                            MenuItemSprite* menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::blank, this));
                            menuItem->setContentSize(tmpSize);
                            menuItem->setPosition(Point::ZERO);
                            
                            Menu* menu = Menu::create2(menuItem, NULL);
                            menu->setContentSize(tmpSize);
                            menu->setPosition(Point( w * 0.5 + w, scrollLayerHeight - h * 0.15 - w * 0.18 * disp_count ));
                            
                            scrollLayer->addChild(menu, 200+disp_count, 200+disp_count);
                            
                            auto move = MoveBy::create(0.25 + 0.05 * disp_count, Point(-w, 0));
                            menu->runAction(move);
                            
                            disp_count++;
                            
                        }
                    }// for()
                }// if(skill_count == pattern){
                skill_count++;
            }// while
            sqlite3_reset(stmt2);
            sqlite3_finalize(stmt2);
            
            // 進化前・進化後がLvいくつになるか表示するための情報収集
            if (sqlite3_open(fullpath2.c_str(), &db2) == SQLITE_OK) {
                sqlite3_stmt *stmt3 = NULL;
                sql = "select * from user_skill_list where ";
                sprintf(tmpSql, "skill_id = %d", tmptmp_skill_to);
                sql += tmpSql;
                
                if(sqlite3_prepare_v2(db2, sql.c_str(), -1, &stmt3, NULL) == SQLITE_OK){
                    
                    sqlite3_step(stmt3);
                    from_level           = (int)sqlite3_column_int(stmt3, 2);
              
                }
                sqlite3_reset(stmt3);
                sqlite3_finalize(stmt3);
            }
            sqlite3_close(db2);

            if(disp_count != 0){
                // 下矢印
                Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(30, 30, 120, 120));
                image->setContentSize(Size(w * 0.60 , w * 0.18 ));
                Size tmpSize = image->getContentSize();
                image->setPosition(Point::ZERO);
                image->setPosition(Point( w * 0.5 + w, scrollLayerHeight - h * 0.15 - w * 0.18 * 4 ));
                
                auto label10 = LabelTTF::create("↓↓↓", FONT, LEVEL_FONT_SIZE);
                label10->setAnchorPoint(Point(0.5, 0.25));
                label10->setFontFillColor(Color3B(255, 255, 255));
                label10->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.70));
                image->addChild(label10);
                
                auto label11 = LabelTTF::create(LocalizedString::getLocalizedString("vernishsozai", "vernish sozai"), FONT, LEVEL_FONT_SIZE * 0.6);
                label11->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.56));
                image->addChild(label11);
                
                auto label12 = LabelTTF::create(LocalizedString::getLocalizedString("jidougousei1", "jidou gousei"), FONT, LEVEL_FONT_SIZE * 0.6);
                label12->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.40));
                image->addChild(label12);
                
                auto label13 = LabelTTF::create(LocalizedString::getLocalizedString("jidougousei2", "jidou gousei"), FONT, LEVEL_FONT_SIZE * 0.6);
                label13->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.24));
                image->addChild(label13);
                
                scrollLayer->addChild(image, 200+disp_count, 200+disp_count);
                auto move = MoveBy::create(0.25 + 0.05 * 4, Point(-w, 0));
                image->runAction(move);
                
                
                // 進化後
                image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(30, 30, 120, 120));
                image->setContentSize(Size(w * 0.68 , w * 0.18 ));
                tmpSize = image->getContentSize();
                image->setPosition(Point::ZERO);
                image->setPosition(Point( w * 0.35 + w, scrollLayerHeight - h * 0.15 - w * 0.18 * 5 ));
                
                char tmpChar[64];
                sprintf(tmpChar, "skillName%d", tmptmp_skill_to);
                auto label14 = LabelTTF::create(LocalizedString::getLocalizedString(tmpChar, tmpChar), FONT, LEVEL_FONT_SIZE);
                label14->setAnchorPoint(Point(0, 0.5));
                label14->setFontFillColor(Color3B(255, 255, 255));
                if(not_have_count == 0 || from_level != 0){ // 全部素材を持っている
                    label14->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.7));
                }
                else{
                    label14->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.5));
                }
                image->addChild(label14);

                if(not_have_count == 0 || from_level != 0){ // 全部素材を持っている（持ってない素材がある場合は進化後の詳細を出さない）
                    sprintf(tmpChar, "skill_detail%d", tmptmp_skill_to);
                    auto label15 = LabelTTF::create(LocalizedString::getLocalizedString(tmpChar, tmpChar), FONT, LEVEL_FONT_SIZE*0.8, Size(tmpSize.width*0.9, NULL), TextHAlignment::LEFT, TextVAlignment::TOP);
                    label15->setAnchorPoint(Point(0, 0.5));
                    label15->setFontFillColor(Color3B(255, 255, 255));
                    label15->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.35));
                    image->addChild(label15);
                }
                
                if(from_level == 0){
                    sprintf(tmpChar, "Lv%d", average_level / level_count);
                }
                else{
                    sprintf(tmpChar, "Lv%d -> Lv%d", from_level, from_level + average_level / level_count);
                }
                auto label16 = LabelTTF::create(tmpChar, FONT, LEVEL_FONT_SIZE);
                label16->setAnchorPoint(Point(1, 0.5));
                label16->setFontFillColor(Color3B(255, 255, 255));
                if(not_have_count == 0 || from_level != 0){ // 全部素材を持っている（持ってない素材がある場合は進化後の詳細を出さない）
                    label16->setPosition(Point(tmpSize.width * 0.95, tmpSize.height * 0.7));
                }
                else{
                    label16->setPosition(Point(tmpSize.width * 0.95, tmpSize.height * 0.5));
                }
                image->addChild(label16);
                
                scrollLayer->addChild(image, 200+disp_count, 200+disp_count);
                move = MoveBy::create(0.25 + 0.05 * 5, Point(-w, 0));
                image->runAction(move);
                
                
                // 進化ボタン
                if(not_have_count == 0){ // 全部素材を持っている
                    Scale9Sprite* image3 = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
                    image3->setContentSize(Size(w * 0.3 , w * 0.18 ));
                    Size tmpSize4 = image3->getContentSize();
                    image3->setPosition(Point::ZERO);
                    
                    auto label20 = LabelTTF::create(LocalizedString::getLocalizedString("dogousei", "exec gousei"), FONT, LEVEL_FONT_SIZE);
                    label20->setFontFillColor(Color3B(255, 255, 255));
                    label20->setPosition(Point(tmpSize4.width * 0.5, tmpSize4.height * 0.5));
                    image3->addChild(label20, 201, 201);
                    
                    MenuItemSprite* menuItem3 = MenuItemSprite::create(image3, image3, CC_CALLBACK_0(HelloWorld::setMenu421_shinkaExec, this, tmptmp_skill_to));
                    menuItem3->setContentSize(tmpSize4);
                    menuItem3->setPosition(Point::ZERO);
                    
                    Menu* menu3 = Menu::create(menuItem3, NULL);
                    menu3->setContentSize(tmpSize4);
                    menu3->setPosition(Point(w * 0.83 + w, scrollLayerHeight - h * 0.15 - w * 0.18 * 5 ));
                    
                    scrollLayer->addChild(menu3, 200+disp_count, 200+disp_count);
                    
                    move = MoveBy::create(0.25 + 0.05 * 5, Point(-w, 0));
                    menu3->runAction(move);
                }
                else{ // 持っていない素材がある
                    Scale9Sprite* image3 = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
                    image3->setContentSize(Size(w * 0.3 , w * 0.18 ));
                    Size tmpSize4 = image3->getContentSize();
                    image3->setPosition(Point::ZERO);
                    
                    auto label21 = LabelTTF::create(LocalizedString::getLocalizedString("dogousei", "exec gousei"), FONT, LEVEL_FONT_SIZE);
                    label21->setFontFillColor(Color3B(255, 255, 255));
                    label21->setPosition(Point(tmpSize4.width * 0.5, tmpSize4.height * 0.5));
                    image3->addChild(label21, 201, 201);
                    
                    Scale9Sprite* image2 = Scale9Sprite::create("noselectable.png", Rect(0, 0, 180, 180), Rect(30, 30, 120, 120));
                    image2->setContentSize(tmpSize4);
                    image2->setOpacity(0x80);
                    image2->setPosition(Point(tmpSize4.width * 0.5, tmpSize4.height * 0.5));
                    image3->addChild(image2, 202, 202);
                    
                    image3->setPosition(Point(w * 0.83 + w, scrollLayerHeight - h * 0.15 - w * 0.18 * 5 ));
                    scrollLayer->addChild(image3, 200+disp_count, 200+disp_count);
                    
                    move = MoveBy::create(0.25 + 0.05 * 5, Point(-w, 0));
                    image3->runAction(move);
                    
                }
            }
        }// if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt2, NULL) == SQLITE_OK){
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    
    scrollView->setContentOffset(Point(0, hc - scrollLayer->getContentSize().height));
    scrollView->setContentSize(scrollLayer->getContentSize());
    scrollView->addChild(scrollLayer, 100, 101);
    
    this->addChild(scrollView, 100, 100);
}

void HelloWorld::setMenu421_shinkaExec(int skill_id){ // 進化合成実行
    CCLOG("Shinka Gousei : %d", skill_id);
    
    playSE(3); // se_continue.mp3
  
    sqlite3* db = NULL;
    std::string fullpath = FileUtils::getInstance()->getWritablePath();
    fullpath += "gamedata.db";
    
    int average_level = 0;
    int level_count = 0;
    
    if (sqlite3_open(fullpath.c_str(), &db) == SQLITE_OK) {
        sqlite3_stmt *stmt = NULL;
        
        // 素材が何かを調べる
        std::string sql = "select * from skill_evolution where ";
        char tmpSql[128];
        sprintf(tmpSql, "skill_to = %d", skill_id);
        sql += tmpSql;
        
        if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
            
            sqlite3* db2 = NULL;
            std::string fullpath2 = FileUtils::getInstance()->getWritablePath();
            fullpath2 += "userdata.db";
            
            if (sqlite3_open(fullpath2.c_str(), &db2) == SQLITE_OK) {
                
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    int _id             = (int)sqlite3_column_int(stmt, 0);
                    int skill_to        = (int)sqlite3_column_int(stmt, 1);
                    int skill1          = (int)sqlite3_column_int(stmt, 2);
                    int skill2          = (int)sqlite3_column_int(stmt, 3);
                    int skill3          = (int)sqlite3_column_int(stmt, 4);
                    int skill4          = (int)sqlite3_column_int(stmt, 5);
                    
                    // 素材スキルのレベルを調べる。素材１
                    sqlite3_stmt *stmt3 = NULL;
                    std::string sql = "select * from user_skill_list where ";
                    char tmpSql[128];
                    sprintf(tmpSql, "skill_id = %d", skill1);
                    sql += tmpSql;
                    
                    if(sqlite3_prepare_v2(db2, sql.c_str(), -1, &stmt3, NULL) == SQLITE_OK){
                        while (sqlite3_step(stmt3) == SQLITE_ROW) {
                            int tmp_id          = (int)sqlite3_column_int(stmt3, 0);
                            int tmp_skill_id    = (int)sqlite3_column_int(stmt3, 1);
                            int tmp_level       = (int)sqlite3_column_int(stmt3, 2);
                            
                            average_level += tmp_level;
                            level_count++;
                            CCLOG("1 level %d, sum_level %d, level_count %d", tmp_level, average_level, level_count);
                        }
                    }
                    sqlite3_reset(stmt3);
                    sqlite3_finalize(stmt3);
                    
                    // 素材スキルのレベルを調べる。素材２
                    stmt3 = NULL;
                    sql = "select * from user_skill_list where ";
                    sprintf(tmpSql, "skill_id = %d", skill2);
                    sql += tmpSql;
                    
                    if(sqlite3_prepare_v2(db2, sql.c_str(), -1, &stmt3, NULL) == SQLITE_OK){
                        while (sqlite3_step(stmt3) == SQLITE_ROW) {
                            int tmp_id          = (int)sqlite3_column_int(stmt3, 0);
                            int tmp_skill_id    = (int)sqlite3_column_int(stmt3, 1);
                            int tmp_level       = (int)sqlite3_column_int(stmt3, 2);
                            
                            average_level += tmp_level;
                            level_count++;
                            CCLOG("2 level %d, sum_level %d, level_count %d", tmp_level, average_level, level_count);
                        }
                    }
                    sqlite3_reset(stmt3);
                    sqlite3_finalize(stmt3);
                    
                    // 素材スキルのレベルを調べる。素材３
                    stmt3 = NULL;
                    sql = "select * from user_skill_list where ";
                    sprintf(tmpSql, "skill_id = %d", skill3);
                    sql += tmpSql;
                    
                    if(sqlite3_prepare_v2(db2, sql.c_str(), -1, &stmt3, NULL) == SQLITE_OK){
                        while (sqlite3_step(stmt3) == SQLITE_ROW) {
                            int tmp_id          = (int)sqlite3_column_int(stmt3, 0);
                            int tmp_skill_id    = (int)sqlite3_column_int(stmt3, 1);
                            int tmp_level       = (int)sqlite3_column_int(stmt3, 2);
                            
                            average_level += tmp_level;
                            level_count++;
                            CCLOG("3 level %d, sum_level %d, level_count %d", tmp_level, average_level, level_count);
                        }
                    }
                    sqlite3_reset(stmt3);
                    sqlite3_finalize(stmt3);
                    
                    // 素材スキルのレベルを調べる。素材４
                    stmt3 = NULL;
                    sql = "select * from user_skill_list where ";
                    sprintf(tmpSql, "skill_id = %d", skill4);
                    sql += tmpSql;
                    
                    if(sqlite3_prepare_v2(db2, sql.c_str(), -1, &stmt3, NULL) == SQLITE_OK){
                        while (sqlite3_step(stmt3) == SQLITE_ROW) {
                            int tmp_id          = (int)sqlite3_column_int(stmt3, 0);
                            int tmp_skill_id    = (int)sqlite3_column_int(stmt3, 1);
                            int tmp_level       = (int)sqlite3_column_int(stmt3, 2);
                            
                            average_level += tmp_level;
                            level_count++;
                            CCLOG("4 level %d, sum_level %d, level_count %d", tmp_level, average_level, level_count);
                            
                        }
                    }
                    sqlite3_reset(stmt3);
                    sqlite3_finalize(stmt3);
                    
                    
                    
                    // 素材スキルの削除実行
                    char tmpString[128];
                    sprintf(tmpString, "DELETE from user_skill_list where skill_id = %d", skill1);
                    sqlite3_exec(db2, tmpString, NULL, NULL, NULL);
                    
                    if(skill2 != 0){
                        sprintf(tmpString, "DELETE from user_skill_list where skill_id = %d", skill2);
                        sqlite3_exec(db2, tmpString, NULL, NULL, NULL);
                    }
                    
                    if(skill3 != 0){
                        sprintf(tmpString, "DELETE from user_skill_list where skill_id = %d", skill3);
                        sqlite3_exec(db2, tmpString, NULL, NULL, NULL);
                    }
                    
                    if(skill4 != 0){
                        sprintf(tmpString, "DELETE from user_skill_list where skill_id = %d", skill4);
                        sqlite3_exec(db2, tmpString, NULL, NULL, NULL);
                    }
                }
                
                // 進化後スキルのINSERT/UPDATE
                sqlite3_stmt *stmt2 = NULL;
                
                // 進化後のスキルを既にもっているか
                std::string sql = "select * from user_skill_list where ";
                char tmpSql[128];
                sprintf(tmpSql, "skill_id = %d", skill_id);
                sql += tmpSql;
                
                CCLOG("%s", sql.c_str());
                
                int count = 0;
                int level = 0;
                if(sqlite3_prepare_v2(db2, sql.c_str(), -1, &stmt2, NULL) == SQLITE_OK){
                    sqlite3_step(stmt2);
                    int _id             = (int)sqlite3_column_int(stmt2, 0);
                    int tmp_skill_id    = (int)sqlite3_column_int(stmt2, 1);
                    level               = (int)sqlite3_column_int(stmt2, 2);
                    int soubi_unit_id   = (int)sqlite3_column_int(stmt2, 3);
                    
                    if(tmp_skill_id != 0){
                        count++;
                    }
                    CCLOG("count %d, _id %d, tmp_skill_id %d, level %d, soubi_unit %d", count, _id, tmp_skill_id, level, soubi_unit_id);
                }// if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt2, NULL) == SQLITE_OK){
                sqlite3_reset(stmt2);
                sqlite3_finalize(stmt2);
                
                CCLOG("setMenu421_shinkaExec count = %d", count);
                CCLOG("average = %d / %d = %d", average_level, level_count, average_level / level_count);
                
                if(count != 0){ // 既にスキルはあるので、LvUPのUPDATE
                    sql = "update user_skill_list set ";
                    sprintf(tmpSql, "level = %d where skill_id = %d",level + average_level / level_count, skill_id);
                    sql += tmpSql;
                }
                else{ // スキルは無いので、INSERT
                    sql = "insert into user_skill_list ";
                    sprintf(tmpSql, "(_id,skill_id,level,soubi_unit_id) values(NULL, %d, %d, 0)", skill_id, average_level / level_count);
                    sql += tmpSql;
                }
                CCLOG("%s", sql.c_str());
                sqlite3_exec(db2, sql.c_str(), NULL, NULL, NULL);
                
            }
            sqlite3_close(db2);
        }
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    
    // パーティーをオンラインDB登録。
    searchOtherParties();

    globalMenu(400);
    
}



void HelloWorld::setMenu403(int sortId, bool reset) // スキル売却
{
    
    pageTitleOutAndRemove(403);
    removeMenu();
    
    int disp_count = 0;
    if(reset){
        for(int i = 0; i < 10; i++){
            baikyakuKouho[i] = 0;
        }
        baikyakuCoin = 0;
    }
    
    ScrollView *scrollView = ScrollView::create();
    scrollView->setViewSize(Size(w, hc - w * 0.2));
    scrollView->setPosition(Point(0, yg + w * 0.2));
    
    LayerColor* scrollLayer = LayerColor::create(Color4B(255, 255, 255, SCROLLVIEWOPACITY));
    scrollLayer->changeWidth(w);
    
    
    // 売却情報の枠
    Scale9Sprite* image2 = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(30, 30, 120, 120));
    image2->setContentSize(Size(w * 0.7 , w * 0.18 ));
    Size tmpSize2 = image2->getContentSize();
    image2->setPosition(Point(w * 0.35 + w, yg + w * 0.1));
    this->addChild(image2, 110, 110);
    
    int baikyakuCount = 0;
    for(int i = 0; i < 10; i++){
        if(baikyakuKouho[i] != 0){
            baikyakuCount++;
        }
    }
    
    char tmpChar2[128];
    sprintf(tmpChar2, "%d%s", baikyakuCount, LocalizedString::getLocalizedString("selected", "selecred"));
    auto label2 = LabelTTF::create(tmpChar2, FONT, LEVEL_FONT_SIZE);
    label2->setAnchorPoint(Point(0, 0.5));
    label2->setFontFillColor(Color3B(255, 255, 255));
    label2->setPosition(Point(tmpSize2.width * 0.1, tmpSize2.height * 0.5));
    image2->addChild(label2, 120, 120);
    
    sprintf(tmpChar2, "%d%s", baikyakuCoin, LocalizedString::getLocalizedString("coin", "coin"));
    label2 = LabelTTF::create(tmpChar2, FONT, LEVEL_FONT_SIZE);
    label2->setAnchorPoint(Point(1, 0.5));
    label2->setFontFillColor(Color3B(255, 255, 255));
    label2->setPosition(Point(tmpSize2.width * 0.9, tmpSize2.height * 0.5));
    image2->addChild(label2, 121, 121);
    
    // 売却ボタン
    Scale9Sprite* image4 = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(30, 30, 120, 120));
    image4->setContentSize(Size(w * 0.3 , w * 0.18 ));
    Size tmpSize4 = image4->getContentSize();
    image4->setPosition(Point::ZERO);
    
    auto label4 = LabelTTF::create(LocalizedString::getLocalizedString("baikyakubutton", "baikyaku"), FONT, LEVEL_FONT_SIZE);
    label4->setFontFillColor(Color3B(255, 255, 255));
    label4->setPosition(Point(tmpSize4.width * 0.5, tmpSize4.height * 0.5));
    image4->addChild(label4, 151, 151);
    
    MenuItemSprite* menuItem4 = MenuItemSprite::create(image4, image4, CC_CALLBACK_0(HelloWorld::setMenu432_baikyakuExec, this));
    menuItem4->setContentSize(tmpSize4);
    menuItem4->setPosition(Point::ZERO);
    
    Menu* menu4 = Menu::create(menuItem4, NULL);
    menu4->setContentSize(tmpSize4);
    menu4->setPosition(Point(w * 0.85 + w, yg + w * 0.1));
    this->addChild(menu4, 152, 152);
    
    auto move2 = MoveBy::create(0.25, Point(-w, 0));
    image2->runAction(move2);
    menu4->runAction(move2->clone());
    
    sqlite3* db = NULL;
    std::string fullpath = FileUtils::getInstance()->getWritablePath();
    fullpath += "userdata.db";
    
    if (sqlite3_open(fullpath.c_str(), &db) == SQLITE_OK) {
        sqlite3_stmt *stmt = NULL;
        
        std::string sql = "select * from user_skill_list";
        if(sortId == 0){ sql += " order by skill_id asc"; }
        if(sortId == 1){ sql += " order by skill_id desc"; }
        if(sortId == 2){ sql += " order by level asc"; }
        if(sortId == 3){ sql += " order by level desc"; }
        if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
            
            sqlite3_stmt * stmt_count = stmt;
            int count = 0;
            while (sqlite3_step(stmt_count) == SQLITE_ROW) {
                count++;
            }
            float scrollLayerHeight =  (w * 0.18) * count + h * 0.1 ;
            scrollLayer->changeHeight(scrollLayerHeight);
            scrollLayer->setAnchorPoint(Point(0, 1));
            scrollLayer->setPosition(Point::ZERO);
            
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int _id             = (int)sqlite3_column_int(stmt, 0);
                int skill_id        = (int)sqlite3_column_int(stmt, 1);
                int level           = (int)sqlite3_column_int(stmt, 2);
                int soubi_unit_id   = (int)sqlite3_column_int(stmt, 3);
                
                Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(30, 30, 120, 120));
                image->setContentSize(Size(w * 0.9 , w * 0.18 ));
                Size tmpSize = image->getContentSize();
                image->setPosition(Point::ZERO);
                
                char tmpChar[64];
                sprintf(tmpChar, "skillName%d", skill_id);
                auto label = LabelTTF::create(LocalizedString::getLocalizedString(tmpChar, tmpChar), FONT, LEVEL_FONT_SIZE);
                label->setAnchorPoint(Point(0, 0.5));
                label->setFontFillColor(Color3B(255, 255, 255));
                label->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.7));
                image->addChild(label);
                
                sprintf(tmpChar, "skill_detail%d", skill_id);
                auto label2 = LabelTTF::create(LocalizedString::getLocalizedString(tmpChar, tmpChar), FONT, LEVEL_FONT_SIZE*0.8, Size(tmpSize.width*0.9, NULL), TextHAlignment::LEFT, TextVAlignment::TOP);
                label2->setAnchorPoint(Point(0, 0.5));
                label2->setFontFillColor(Color3B(255, 255, 255));
                label2->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.35));
                image->addChild(label2);
                
                sprintf(tmpChar, "Lv.%d", level);
                auto label3 = LabelTTF::create(tmpChar, FONT, LEVEL_FONT_SIZE*0.8);
                label3->setAnchorPoint(Point(1, 0));
                label3->setFontFillColor(Color3B(255, 255, 255));
                label3->setPosition(Point(tmpSize.width * 0.95, tmpSize.height * 0.7));
                image->addChild(label3);
                
                // 選択されているものに枠
                for(int i = 0; i < 10; i++){
                    if(baikyakuKouho[i] == _id){
                        Scale9Sprite* image3 = Scale9Sprite::create("selected.png", Rect(0, 0, 180, 180), Rect(30, 30, 120, 120));
                        image3->setContentSize(Size(w * 0.9 , w * 0.18 ));
                        image3->setAnchorPoint(Point::ZERO);
                        image->addChild(image3, _id, _id);
                    }
                }
                
                if(soubi_unit_id){
                    // 装備されちゃっているものは黒枠
                    sprintf(tmpChar, "%d%s", soubi_unit_id, LocalizedString::getLocalizedString("issoubi", "issoubi"));
                    auto label4 = LabelTTF::create(tmpChar, FONT, LEVEL_FONT_SIZE*0.8);
                    label4->setAnchorPoint(Point(1, 1));
                    label4->setFontFillColor(Color3B(255, 255, 255));
                    label4->setPosition(Point(tmpSize.width * 0.95, tmpSize.height * 0.7));
                    image->addChild(label4);
                    
                    Scale9Sprite* image2 = Scale9Sprite::create("noselectable.png", Rect(0, 0, 180, 180), Rect(30, 30, 120, 120));
                    image2->setContentSize(tmpSize);
                    image2->setOpacity(0x80);
                    image2->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.5));
                    image->addChild(image2);
                    
                    image->setPosition(Point( w * 0.5 + w, scrollLayerHeight - h * 0.15 - w * 0.18 * disp_count ));
                    scrollLayer->addChild(image, 200+disp_count, 200+disp_count);
                    
                    auto move = MoveBy::create(0.25 + 0.05 * disp_count, Point(-w, 0));
                    image->runAction(move);
                    
                }
                else{
                    auto label5 = LabelTTF::create(LocalizedString::getLocalizedString("noonesoubi", "noonesoubi"), FONT, LEVEL_FONT_SIZE*0.8);
                    label5->setAnchorPoint(Point(1, 1));
                    label5->setFontFillColor(Color3B(255, 255, 255));
                    label5->setPosition(Point(tmpSize.width * 0.95, tmpSize.height * 0.7));
                    image->addChild(label5);
                    
                    MenuItemSprite* menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::setMenu431_baikyakuKouhoSelect, this, _id, level, image));
                    menuItem->setContentSize(tmpSize);
                    menuItem->setPosition(Point::ZERO);
                    
                    Menu* menu = Menu::create2(menuItem, NULL);
                    menu->setContentSize(tmpSize);
                    menu->setPosition(Point( w * 0.5 + w, scrollLayerHeight - h * 0.15 - w * 0.18 * disp_count ));
                    
                    scrollLayer->addChild(menu, 200+disp_count, 200+disp_count);
                    
                    auto move = MoveBy::create(0.25 + 0.05 * disp_count, Point(-w, 0));
                    menu->runAction(move);
                }
                
                disp_count++;
                
            }
        }
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    
    scrollView->setContentOffset(Point(0, hc - scrollLayer->getContentSize().height - w * 0.2));
    scrollView->setContentSize(scrollLayer->getContentSize());
    scrollView->addChild(scrollLayer, 100, 101);
    
    this->addChild(scrollView, 100, 100);
    
    
    // ソート用ボタン
    Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image->setContentSize(Size(w * 0.2 , h * 0.08 ));
    Size tmpSize = image->getContentSize();
    image->setPosition(Point::ZERO);
    
    char tmpChar[64];
    sprintf(tmpChar, "sortskill%d", sortId);
    auto label = LabelTTF::create(LocalizedString::getLocalizedString(tmpChar, tmpChar), FONT, LEVEL_FONT_SIZE);
    label->setFontFillColor(Color3B(255, 255, 255));
    label->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.5));
    image->addChild(label);
    
    int nextSortId = sortId + 1;
    if (nextSortId == 4) { nextSortId = 0; }
    
    MenuItemSprite* menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::setMenu403, this, nextSortId, false));
    menuItem->setContentSize(tmpSize);
    menuItem->setPosition(Point::ZERO);
    
    Menu* menu = Menu::create(menuItem, NULL);
    menu->setContentSize(tmpSize);
    menu->setPosition(Point( w * 1.9, h * 0.81 ));
    
    this->addChild(menu, 105, 105);
    
    auto move = MoveBy::create(0.25, Point(-w, 0));
    menu->runAction(move);
    
    
}

void HelloWorld::setMenu431_baikyakuKouhoSelect(int _id, int level, Node* sprite)
{
    ScrollView* check = (ScrollView*)this->getChildByTag(100);
    if(!(check->touchEndPointCheck())){ return; }
    
    // 10個全部埋まってたら何もせずリターン
    int baikyakuCount = 0;
    for(int i = 0; i < 10; i++){
        if(baikyakuKouho[i] != 0){
            baikyakuCount++;
        }
    }
    if(baikyakuCount == 10){
        return;
    }
//    CocosDenshion::SimpleAudioEngine::getInstance()->playEffect("se/se_enter.mp3");
    playSE(0); // se_enter.mp3

    bool tmpCheck = false;
    // 解除の場合
    for(int i = 0; i < 10; i++) {
        // 前に寄せる
        if(tmpCheck == true){
            baikyakuKouho[i-1] = baikyakuKouho[i];
        }
        else{
            if(baikyakuKouho[i] == _id){
                // ここで解除処理
                baikyakuKouho[i] = 0;
                tmpCheck = true;
                
                baikyakuCount--;
                baikyakuCoin -= level * 10;
                
                char tmpChar2[128];
                sprintf(tmpChar2, "%d%s", baikyakuCount, LocalizedString::getLocalizedString("selected", "selecred"));
                ((LabelTTF*)this->getChildByTag(110)->getChildByTag(120))->setString(tmpChar2);
                
                sprintf(tmpChar2, "%d%s", baikyakuCoin, LocalizedString::getLocalizedString("coin", "coin"));
                ((LabelTTF*)this->getChildByTag(110)->getChildByTag(121))->setString(tmpChar2);
                
                sprite->removeChildByTag(_id);
            }
        }
    }
    // 追加の場合
    if(tmpCheck == false){
        for(int i = 0; i < 10; i++) {
            if(tmpCheck == false){
                if(baikyakuKouho[i] == 0){
                    // ここで追加
                    baikyakuKouho[i] = _id;
                    tmpCheck = true;
                    
                    Scale9Sprite* image = Scale9Sprite::create("selected.png", Rect(0, 0, 180, 180), Rect(30, 30, 120, 120));
                    image->setContentSize(Size(w * 0.9 , w * 0.18 ));
                    image->setAnchorPoint(Point::ZERO);
                    
                    baikyakuCount++;
                    baikyakuCoin += level * 10;
                    
                    char tmpChar2[128];
                    sprintf(tmpChar2, "%d%s", baikyakuCount, LocalizedString::getLocalizedString("selected", "selecred"));
                    ((LabelTTF*)this->getChildByTag(110)->getChildByTag(120))->setString(tmpChar2);
                    
                    sprintf(tmpChar2, "%d%s", baikyakuCoin, LocalizedString::getLocalizedString("coin", "coin"));
                    ((LabelTTF*)this->getChildByTag(110)->getChildByTag(121))->setString(tmpChar2);
                    
                    sprite->addChild(image, _id, _id);
                }
            }
        }
    }
}

void HelloWorld::setMenu432_baikyakuExec()
{
    // 選択されていなかったら何もせずリターン
    int baikyakuCount = 0;
    for(int i = 0; i < 10; i++){
        if(baikyakuKouho[i] != 0){
            baikyakuCount++;
        }
    }
    if(baikyakuCount == 0){
        return;
    }
    
//    CocosDenshion::SimpleAudioEngine::getInstance()->playEffect("se/se_coin.mp3");
    playSE(4); // se_coin.mp3
    
    // 削除実行
    
    sqlite3* db = NULL;
    std::string fullpath = FileUtils::getInstance()->getWritablePath();
    fullpath += "userdata.db";
    
    if (sqlite3_open(fullpath.c_str(), &db) == SQLITE_OK) {
        for(int i = 0; i < 10; i++){
            if(baikyakuKouho[i] != 0){
                char tmpString[128];
                sprintf(tmpString, "DELETE from user_skill_list where _id = %d", baikyakuKouho[i]);
                sqlite3_exec(db, tmpString, NULL, NULL, NULL);
            }
        }
    }
    sqlite3_close(db);
    
    
    coin += baikyakuCoin;
    updateUserDataDb();
    updateTopMenu();
    baikyakuCoin = 0;
    
    setMenu403(0, true);
    
}


//////////////////////////////////////////////////
//////////////////////////////////////////////////
//////////////////////////////////////////////////
//  メニュー５００系
//////////////////////////////////////////////////
//////////////////////////////////////////////////
//////////////////////////////////////////////////

void HelloWorld::setMenu500_BattleInfo()
{
    //void HelloWorld::setMenu(int menuNum) の内部から呼ばれる。対戦相手情報を描画する部分のみ。
    
    int i = 0;
    // ここから対戦相手情報と自分の情報を描画
    std::string battleUserName; // 対戦相手
    int b_rank = 0; // 対戦相手のランク
    int b_type;// 対戦相手のキャラ１のタイプ
    int b_zokusei;// 対戦相手のキャラ１の属性
    int type; // 自分のキャラ１のタイプ
    int zokusei; // 自分のキャラ１の属性
    
    sqlite3* db = NULL;
    std::string fullpath = FileUtils::getInstance()->getWritablePath();
    fullpath += "userdata.db";
    
    char userNameTmp[64];
    
    if (sqlite3_open(fullpath.c_str(), &db) == SQLITE_OK) {
        // 対戦相手のキャラ１描画用
        sqlite3_stmt *stmt = NULL;
        std::string sql = "select * from other_users_party where _id = 2";
        if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
            if(sqlite3_step(stmt) == SQLITE_ROW){
                sprintf(userNameTmp, "%s", sqlite3_column_text(stmt, 1));
                battleUserName = userNameTmp;
                b_rank          = (int)sqlite3_column_int(stmt, 2);
                b_type          = (int)sqlite3_column_int(stmt, 3);
                b_zokusei       = (int)sqlite3_column_int(stmt,15);
            }
        }
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);
        
        // 自分のキャラ１描画用
        sql = "select * from user_unit inner join user_party2 on user_unit._id = user_party2.unit_id order by user_party2.unit_order desc";
        if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int user_unit_id    = (int)sqlite3_column_int(stmt, 0);
                type                = (int)sqlite3_column_int(stmt, 1);
                zokusei             = (int)sqlite3_column_int(stmt,12);
            }
        }
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    
    CCLOG("visibleDangeonNumber %d, ARENAABLEDANGEONNUM %d", visibleDangeonNumber, ARENAABLEDANGEONNUM);
    
    Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image->setContentSize(Size(w * 0.9 , w * 0.18 * 3));
    Point point = image->getAnchorPoint();
    image->setPosition(Point::ZERO);
    Size tmpSize = image->getContentSize();
    
    MenuItemSprite* menuItem;
    if(visibleDangeonNumber >= ARENAABLEDANGEONNUM && b_rank != 0){  // 対戦をできる状態のとき
        menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::setMenu501_battleStart, this));
    }
    else{ // 対戦相手がいない、もしくは、アリーナ対戦ができるまでダンジョンが進行していない
        menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::blank, this));
    }
    menuItem->setContentSize(tmpSize);
    menuItem->setPosition(Point::ZERO);
    
    Menu* menu = Menu::create(menuItem, NULL);
    menu->setContentSize(tmpSize);
    menu->setPosition(Point(w * 1.5, h * 0.7 - w * 0.18 * (i+1)));
    
    if(visibleDangeonNumber >= ARENAABLEDANGEONNUM && b_rank != 0){
        char tmpChar[32];
        // 対戦相手の名前
        auto label = LabelTTF::create(battleUserName.c_str(), FONT, LEVEL_FONT_SIZE);
        label->setFontFillColor(Color3B(255, 255, 255));
        label->setAnchorPoint(Point(0.5, 0.5));
        label->setPosition(Point(tmpSize.width * 0.27, tmpSize.height * 0.85));
        image->addChild(label);
        
        // 対戦相手のランク
        sprintf(tmpChar, "%s：%d", LocalizedString::getLocalizedString("rank", "rank"), b_rank);
        auto label2 = LabelTTF::create(tmpChar, FONT, LEVEL_FONT_SIZE);
        label2->setFontFillColor(Color3B(255, 255, 255));
        label2->setAnchorPoint(Point(0.5, 0.5));
        label2->setPosition(Point(tmpSize.width * 0.27, tmpSize.height * 0.75));
        image->addChild(label2);
        
        // 対戦相手の戦闘数、勝利数（情報なし。？表示）
        sprintf(tmpChar, "?? %s ?? %s", LocalizedString::getLocalizedString("battles501", "battle"), LocalizedString::getLocalizedString("wins501", "win"));
        auto label3 = LabelTTF::create(tmpChar, FONT, LEVEL_FONT_SIZE * 0.8);
        label3->setFontFillColor(Color3B(255, 255, 255));
        label3->setAnchorPoint(Point(0.5, 0.5));
        label3->setPosition(Point(tmpSize.width * 0.27, tmpSize.height * 0.68));
        image->addChild(label3);
        
        // 中央のVS文字
        sprintf(tmpChar, "VS");
        auto label4 = LabelTTF::create(tmpChar, FONT, LEVEL_FONT_SIZE);
        label4->setFontFillColor(Color3B(255, 255, 255));
        label4->setAnchorPoint(Point(0.5, 0.5));
        label4->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.5));
        image->addChild(label4);
        
        // プレイヤー名
        auto label5 = LabelTTF::create(userName.c_str(), FONT, LEVEL_FONT_SIZE);
        label5->setFontFillColor(Color3B(255, 255, 255));
        label5->setAnchorPoint(Point(0.5, 0.5));
        label5->setPosition(Point(tmpSize.width * 0.73, tmpSize.height * 0.85));
        image->addChild(label5);
        
        // プレイヤーのランク
        sprintf(tmpChar, "%s：%d", LocalizedString::getLocalizedString("rank", "rank"), battleRank);
        auto label6 = LabelTTF::create(tmpChar, FONT, LEVEL_FONT_SIZE);
        label6->setFontFillColor(Color3B(255, 255, 255));
        label6->setAnchorPoint(Point(0.5, 0.5));
        label6->setPosition(Point(tmpSize.width * 0.73, tmpSize.height * 0.75));
        image->addChild(label6);
        
        // プレイヤーの戦闘数、勝利数
        sprintf(tmpChar, "%d %s %d %s", battleCount, LocalizedString::getLocalizedString("battles501", "battle"), battleWinCount, LocalizedString::getLocalizedString("wins501", "win"));
        auto label7 = LabelTTF::create(tmpChar, FONT, LEVEL_FONT_SIZE * 0.8);
        label7->setFontFillColor(Color3B(255, 255, 255));
        label7->setAnchorPoint(Point(0.5, 0.5));
        label7->setPosition(Point(tmpSize.width * 0.73, tmpSize.height * 0.68));
        image->addChild(label7);
        
        // 後＊勝でランクアップ
        int tmpTillRankUp = MAX(MIN(battleRank, RANKUPWINSMAX), RANKUPWINSMIN) - battleWinCountAtCurrentRank;
        sprintf(tmpChar, "%s%d%s", LocalizedString::getLocalizedString("tillRankUp1", "battle"), tmpTillRankUp, LocalizedString::getLocalizedString("tillRankUp2", "win"));
        auto label8 = LabelTTF::create(tmpChar, FONT, LEVEL_FONT_SIZE * 0.8);
        label8->setFontFillColor(Color3B(255, 255, 255));
        label8->setAnchorPoint(Point(0.5, 0.5));
        label8->setPosition(Point(tmpSize.width * 0.73, tmpSize.height * 0.62));
        image->addChild(label8);

        // 対戦相手のキャラクターイラスト準備
        char tmpFileName[64];
        sprintf(tmpFileName, "window_chara/window_chara%d.png", b_zokusei);
        Scale9Sprite* image2 = Scale9Sprite::create(tmpFileName, Rect(0, 0, 180, 180), Rect(50, 50, 90, 90));
        image2->setContentSize(Size(w * 0.9 , h * 0.6));
        Size tmpSize2 = image2->getContentSize();
        image2->setPosition(Point::ZERO);
        
        // 対戦相手のキャラクターイラスト
        Animation* charaAnimation = Animation::create();
        SpriteFrame* charaSprite[4];
        for (int x = 0; x < 3; x++) {
            char tmpFile[64];
            sprintf(tmpFile, "charactor/chara%d-1.png", b_type);
            charaSprite[x] = SpriteFrame::create(tmpFile, Rect(x * 32, 0, 32, 32));
            charaAnimation->addSpriteFrame(charaSprite[x]);
        }
        charaAnimation->setDelayPerUnit(0.3);
        RepeatForever* charaAction = RepeatForever::create(Animate::create(charaAnimation));
        Sprite* chara = Sprite::create();
        chara->setPosition(Point(tmpSize.width * 0.25, tmpSize.height * 0.32));
        chara->setScale(w * 0.25 / 32);
        chara->setFlippedX(true); // 左右反転
        image->addChild(chara);
        chara->runAction(charaAction);
        
        // 自分のキャラクターイラスト準備
        sprintf(tmpFileName, "window_chara/window_chara%d.png", zokusei);
        Scale9Sprite* image3 = Scale9Sprite::create(tmpFileName, Rect(0, 0, 180, 180), Rect(50, 50, 90, 90));
        image3->setContentSize(Size(w * 0.9 , h * 0.6));
        Size tmpSize3 = image3->getContentSize();
        image3->setPosition(Point::ZERO);
        
        // 自分のキャラクターイラスト
        Animation* charaAnimation2 = Animation::create();
        SpriteFrame* charaSprite2[4];
        for (int x = 0; x < 3; x++) {
            char tmpFile[64];
            sprintf(tmpFile, "charactor/chara%d-1.png", type);
            charaSprite2[x] = SpriteFrame::create(tmpFile, Rect(x * 32, 0, 32, 32));
            charaAnimation2->addSpriteFrame(charaSprite2[x]);
        }
        charaAnimation2->setDelayPerUnit(0.3);
        RepeatForever* charaAction2 = RepeatForever::create(Animate::create(charaAnimation2));
        Sprite* chara2 = Sprite::create();
        chara2->setPosition(Point(tmpSize.width * 0.75, tmpSize.height * 0.32));
        chara2->setScale(w * 0.25 / 32);
        image->addChild(chara2);
        chara2->runAction(charaAction2);
        
    }
    else if(visibleDangeonNumber < ARENAABLEDANGEONNUM){ // アリーナ対戦できるまでにダンジョンが進んでない
        auto label = LabelTTF::create(LocalizedString::getLocalizedString("arenangmessage1", "arenangmessage1"), FONT, LEVEL_FONT_SIZE, Size(tmpSize.width*0.6, tmpSize.height), TextHAlignment::CENTER, TextVAlignment::CENTER);
        label->setFontFillColor(Color3B(255, 255, 255));
        label->setAnchorPoint(Point(0.5, 0.5));
        label->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.5));
        image->addChild(label);
    }
    else{ // 対戦相手がみつからない
        auto label = LabelTTF::create(LocalizedString::getLocalizedString("arenangmessage2", "arenangmessage2"), FONT, LEVEL_FONT_SIZE);
        label->setFontFillColor(Color3B(255, 255, 255));
        label->setAnchorPoint(Point(0.5, 0.5));
        label->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.5));
        image->addChild(label);
    }
    
    this->addChild(menu, 100+i+10, 100+i+10);
    CCLOG("500info tag = %d", 110+i);
    
    auto move = MoveBy::create(0.25 + 0.05 * i, Point(-w, 0));
    menu->runAction(move);

}

void HelloWorld::setMenu501_battleStart()
{
    CCLOG("setMenu501_battleStart");
    if(battleStamina == 0){
        //CocosDenshion::SimpleAudioEngine::getInstance()->playEffect("se/se_ng.mp3");
        playSE(1); // se_ng.mp3
        pageTitleOutAndRemove(303);
        removeMenu();
        setMenu302(false, true);
        return;
    }
    
//    CocosDenshion::SimpleAudioEngine::getInstance()->playEffect("se/se_enter.mp3");
    playSE(0); // se_enter.mp3

    // スタート扱いでスタミナ減らす
    if(battleStamina == battleStaminaMax){
        lastBattleStaminaDecreaseTime = time(NULL); // 既に減っている場合は時計は変更しない
    }
    battleStamina -= 1;
    
    // バトル数を更新
    battleCount++;
    battleCountAtCurrentRank++;
    
    // ユーザデータベース更新
    updateUserDataDb();
    
    // バトルのシーンに飛ばす
    SceneManager* sceneManager = SceneManager::getInstance();
    sceneManager->dangeonId = -1;
    
    gotoBattleScene();
    //auto scene = BattleScene::createScene();
    //Director::getInstance()->replaceScene(scene);
}

void HelloWorld::setMenu502_searchOtherOpponent()
{
    CCLOG("setMenu502_searchOtherOpponent();");
    tapProhibit = true;
    searchingOtherOpponent = true;
    
    
    Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image->setContentSize(Size(w * 0.9 , w * 0.18 * 3));
    Point point = image->getAnchorPoint();
    image->setPosition(Point(w * 1.5, h * 0.7 - w * 0.18 * 1));
    Size tmpSize = image->getContentSize();
    
    auto label = LabelTTF::create(LocalizedString::getLocalizedString("searchOtherOpponent", "searching..."), FONT, LEVEL_FONT_SIZE);
    label->setFontFillColor(Color3B(255, 255, 255));
    label->setAnchorPoint(Point(0.5, 0.5));
    label->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.5));
    image->addChild(label);
    
    this->addChild(image, 100, 100);
    
    auto move = MoveBy::create(0.25, Point(-w, 0));
    image->runAction(move);

    
    searchOtherParties();

}


//////////////////////////////////////////////////
//////////////////////////////////////////////////
//////////////////////////////////////////////////
//  メニュー６００系
//////////////////////////////////////////////////
//////////////////////////////////////////////////
//////////////////////////////////////////////////

void HelloWorld::setMenu601()
{
    //    NativeManager* nativeManager = NativeManager::getInstance();
    //    nativeManager->displayUrlByBrowser(INFOURL);
    NativeManager::displayUrlByBrowser(INFOURL);
    globalMenu(600);

}

void HelloWorld::setMenu602()
{
    userNameChanging = true;
    kisyuhenkouCanceling = false;
    
    Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image->setContentSize(Size(w * 0.9 , w * 0.18));
    image->setPosition(Point(w * 1.5, h * 0.7));
    Size tmpSize = image->getContentSize();
    this->addChild(image, 100, 100);

    EditBox* editBox = EditBox::create(Size(w * 0.8 , w * 0.14), Scale9Sprite::create("window_base.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100)));
    editBox->setFontName(FONT);
    editBox->setFontSize(LEVEL_FONT_SIZE);
    editBox->setPlaceHolder(userName.c_str());
    editBox->setInputMode(EditBox::InputMode::SINGLE_LINE);
    editBox->setMaxLength(10);
    editBox->setPosition(Point(w * 1.5, h * 0.7));
    editBox->setDelegate(this);
    this->addChild(editBox, 101, 101);
    
    // 変更ボタン(画像があるだけで実際はボタンじゃない)
    Scale9Sprite* image2 = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image2->setContentSize(Size(w * 0.2 , w * 0.14));
    Point point = image2->getAnchorPoint();
    image2->setPosition(Point(w * 1.8, h * 0.7));
    Size tmpSize2 = image2->getContentSize();
    
    auto label2 = LabelTTF::create(LocalizedString::getLocalizedString("change", "change"), FONT, LEVEL_FONT_SIZE);
    label2->setAnchorPoint(Point(0.5, 0.5));
    label2->setPosition(Point(tmpSize2.width * 0.5, tmpSize2.height * 0.5));
    label2->setFontFillColor(Color3B(255, 255, 255));
    image2->addChild(label2);
    
    this->addChild(image2, 102, 102);
    
    auto move = MoveBy::create(0.25, Point(-w, 0));
    image->runAction(move);
    
    auto move2 = MoveBy::create(0.25, Point(-w, 0));
    editBox->runAction(move2);
    
    auto move3 = MoveBy::create(0.25, Point(-w, 0));
    image2->runAction(move3);
}

void HelloWorld::editBoxEditingDidBegin(cocos2d::extension::EditBox* editBox)
{
    if(userNameChanging){
        CCLOG("editBox EditingDidBegin");
        editBox->setPlaceHolder(userName.c_str());
    }
}

void HelloWorld::editBoxReturn(cocos2d::extension::EditBox* editBox){
    if(userNameChanging){
        CCLOG("editBox finished");
        std::string userNameTmp2 = editBox->getText();
        
        if(userNameTmp2.size() != 0){
            userName = editBox->getText();
            
            updateTopMenu();
            updateUserDataDb();
            searchOtherParties();
        }
        else{
            editBox->setPlaceHolder(userName.c_str());
        }
    }
    
    if(kisyuhenkouCanceling){
        std::string tmpKisyuhenkouCodeEntered = editBox->getText();
        
        sprintf(kisyuhenkouCodeEntered, "%s", tmpKisyuhenkouCodeEntered.c_str());
        CCLOG("code entered : %s", kisyuhenkouCodeEntered);
    }
}

void HelloWorld::setMenu602_nameChange()
{
//    CocosDenshion::SimpleAudioEngine::getInstance()->playEffect("se/se_enter.mp3");
    playSE(0); // se_enter.mp3

}

void HelloWorld::setMenu603()
{
    playSE(0);

    pageTitleRemoveAndSetNextPageTitle(612);
    for(int i = 0; i< 10; i++){
        this->removeChildByTag(101+i);
    }

    Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image->setContentSize(Size(w * 0.9 , w * 0.18 * 5.5));
    image->setPosition(Point(w * 1.5, h * 0.7 - w * 0.18 * 2.25));
    this->addChild(image, 100, 100);
    
    Size tmpSize = image->getContentSize();
    
    ScrollView *scrollView = ScrollView::create();
    scrollView->setViewSize(Size(w * 0.9, w * 0.18 * (5.5 - 0.5)));
    scrollView->setPosition(Point(0, tmpSize.height / 5.5 / 4));
    
    LayerColor* scrollLayer = LayerColor::create(Color4B(255, 255, 255, SCROLLVIEWOPACITY));
    scrollLayer->changeWidth(w * 0.9);
    auto tmplabel = LabelTTF::create("tmp", FONT, LEVEL_FONT_SIZE);
    float scrollLayerHeight =  tmplabel->getContentSize().height *  CREDITLINES;
    scrollLayer->changeHeight(scrollLayerHeight);
    scrollLayer->setAnchorPoint(Point(0, 1));
    scrollLayer->setPosition(Point::ZERO);
    
    for(int i = 0; i < CREDITLINES; i++){
        char tmpChar[16];
        sprintf(tmpChar, "credit%d", i);
        auto label = LabelTTF::create(LocalizedString::getLocalizedString(tmpChar, "クレジット"), FONT, LEVEL_FONT_SIZE);
        label->setPosition(Point(tmpSize.width * 0.5, scrollLayer->getContentSize().height - label->getContentSize().height * (i+1)));
        label->setFontFillColor(Color3B(255, 255, 255));
        scrollLayer->addChild(label);
    }
    
    scrollView->setContentOffset(Point(0, w * 0.18 * (5.5 - 0.5) - scrollLayer->getContentSize().height));
    scrollView->setContentSize(scrollLayer->getContentSize());
    scrollView->addChild(scrollLayer, 100, 101);
    
    image->addChild(scrollView, 100, 100);
    
    auto move = MoveBy::create(0.25, Point(-w, 0));
    image->runAction(move);
    
}


void HelloWorld::setMenu604()
{
    
    pageTitleRemoveAndSetNextPageTitle(613);
    for(int i = 0; i< 10; i++){
        this->removeChildByTag(101+i);
    }

    playSE(0);
    
    
    Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image->setContentSize(Size(w * 0.9 , w * 0.18 * 5.5));
    image->setPosition(Point(w * 1.5, h * 0.7 - w * 0.18 * 2.25));
    this->addChild(image, 100, 100);
    
    Size tmpSize = image->getContentSize();
    
    ScrollView *scrollView = ScrollView::create();
    scrollView->setViewSize(Size(w * 0.9, w * 0.18 * (5.5 - 0.5)));
    scrollView->setPosition(Point(0, tmpSize.height / 5.5 / 4));
    
    LayerColor* scrollLayer = LayerColor::create(Color4B(255, 255, 255, SCROLLVIEWOPACITY));
    scrollLayer->changeWidth(w * 0.9);
    auto tmplabel = LabelTTF::create("tmp", FONT, LEVEL_FONT_SIZE);
    float scrollLayerHeight = 0;
    // 高さ計算のみ
    for(int i = 0; i < HELPLINES; i++){
        char tmpChar[16];
        sprintf(tmpChar, "help%d", i);
        auto label = LabelTTF::create(LocalizedString::getLocalizedString(tmpChar, "Help"), FONT, LEVEL_FONT_SIZE, Size(tmpSize.width*0.9, NULL), TextHAlignment::LEFT, TextVAlignment::TOP);
        scrollLayerHeight += label->getContentSize().height;
    }
    scrollLayer->changeHeight(scrollLayerHeight);
    scrollLayer->setAnchorPoint(Point(0, 1));
    scrollLayer->setPosition(Point::ZERO);
    
    double labelHeight = 0;
    for(int i = 0; i < HELPLINES; i++){
        char tmpChar[16];
        sprintf(tmpChar, "help%d", i);
        auto label = LabelTTF::create(LocalizedString::getLocalizedString(tmpChar, "Help"), FONT, LEVEL_FONT_SIZE, Size(tmpSize.width*0.9, NULL), TextHAlignment::LEFT, TextVAlignment::TOP);
        label->setAnchorPoint(Point(0, 1));
        label->setPosition(Point(tmpSize.width * 0.05, scrollLayer->getContentSize().height - labelHeight));
        labelHeight += label->getContentSize().height;
        CCLOG("help%d, height %f, total height %f", i, label->getContentSize().height, labelHeight);
        label->setFontFillColor(Color3B(255, 255, 255));
        scrollLayer->addChild(label);
    }
    
    scrollView->setContentOffset(Point(0, w * 0.18 * (5.5 - 0.5) - scrollLayer->getContentSize().height));
    scrollView->setContentSize(scrollLayer->getContentSize());
    scrollView->addChild(scrollLayer, 100, 101);
    
    image->addChild(scrollView, 100, 100);
    
    auto move2 = MoveBy::create(0.25 + 0.05, Point(-w, 0));
    image->runAction(move2);

}

void HelloWorld::setMenu604_riyoukiyaku()
{
    NativeManager::displayUrlByBrowser(KIYAKUURL);
    globalMenu(600);
}

void HelloWorld::setMenu605()
{
    
    pageTitleRemoveAndSetNextPageTitle(614);
    for(int i = 0; i< 10; i++){
        this->removeChildByTag(101+i);
    }
    
    playSE(0);
    

    // データ移行
    Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image->setContentSize(Size(w * 0.9 , w * 0.18));
    Point point = image->getAnchorPoint();
    image->setPosition(Point::ZERO);
    Size tmpSize = image->getContentSize();
    
    MenuItemSprite* menuItem;
    menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::setMenu605_uploadData, this));
    menuItem->setContentSize(tmpSize);
    menuItem->setPosition(Point::ZERO);
    
    Menu* menu = Menu::create(menuItem, NULL);
    menu->setContentSize(tmpSize);
    menu->setPosition(Point(w * 1.5, h * 0.7));
    
    auto label = LabelTTF::create(LocalizedString::getLocalizedString("kisyuhenkou1", "kisyuhenkou1"), FONT, LEVEL_FONT_SIZE, Size(tmpSize.width*0.9, tmpSize.height), TextHAlignment::LEFT, TextVAlignment::CENTER);
    label->setFontFillColor(Color3B(255, 255, 255));
    label->setAnchorPoint(Point(0, 0.5));
    label->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.66));
    image->addChild(label);
    
    auto label2 = LabelTTF::create(LocalizedString::getLocalizedString("kisyuhenkou2", "kisyuhenkou2"), FONT, LEVEL_FONT_SIZE*0.8, Size(tmpSize.width*0.9, tmpSize.height), TextHAlignment::LEFT, TextVAlignment::CENTER);
    label2->setFontFillColor(Color3B(255, 255, 255));
    label2->setAnchorPoint(Point(0, 0.5));
    label2->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.33));
    image->addChild(label2);
    
    this->addChild(menu, 100, 100);
    
    auto move = MoveBy::create(0.25, Point(-w, 0));
    menu->runAction(move);
    
    
    // データ取り込み
    Scale9Sprite* image2 = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image2->setContentSize(Size(w * 0.9 , w * 0.18));
    Point point2 = image->getAnchorPoint();
    image2->setPosition(Point::ZERO);
    Size tmpSize2 = image->getContentSize();
    
    MenuItemSprite* menuItem2;
    menuItem2 = MenuItemSprite::create(image2, image2, CC_CALLBACK_0(HelloWorld::setMenu605_displayCodeForm, this));
    menuItem2->setContentSize(tmpSize2);
    menuItem2->setPosition(Point::ZERO);
    
    Menu* menu2 = Menu::create(menuItem2, NULL);
    menu2->setContentSize(tmpSize2);
    menu2->setPosition(Point(w * 1.5, h * 0.7 - w * 0.18));
    
    auto label3 = LabelTTF::create(LocalizedString::getLocalizedString("kisyuhenkou3", "kisyuhenkou3"), FONT, LEVEL_FONT_SIZE, Size(tmpSize2.width*0.9, tmpSize2.height), TextHAlignment::LEFT, TextVAlignment::CENTER);
    label3->setFontFillColor(Color3B(255, 255, 255));
    label3->setAnchorPoint(Point(0, 0.5));
    label3->setPosition(Point(tmpSize2.width * 0.05, tmpSize2.height * 0.66));
    image2->addChild(label3);
    
    auto label4 = LabelTTF::create(LocalizedString::getLocalizedString("kisyuhenkou4", "kisyuhenkou4"), FONT, LEVEL_FONT_SIZE*0.8, Size(tmpSize2.width*0.9, tmpSize2.height), TextHAlignment::LEFT, TextVAlignment::CENTER);
    label4->setFontFillColor(Color3B(255, 255, 255));
    label4->setAnchorPoint(Point(0, 0.5));
    label4->setPosition(Point(tmpSize2.width * 0.05, tmpSize2.height * 0.33));
    image2->addChild(label4);
    
    this->addChild(menu2, 101, 101);
    
    auto move2 = MoveBy::create(0.25 + 0.05, Point(-w, 0));
    menu2->runAction(move2);
    
    
}

void HelloWorld::setMenu605_displayCodeForm()
{
    this->removeChildByTag(100);
    this->removeChildByTag(101);
    
    userNameChanging = false;
    kisyuhenkouCanceling = true;
    
    sprintf(kisyuhenkouCodeEntered, "");
    
    Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image->setContentSize(Size(w * 0.9 , w * 0.18 * 3));
    Point point = image->getAnchorPoint();
    image->setPosition(Point::ZERO);
    Size tmpSize = image->getContentSize();
    
    MenuItemSprite* menuItem;
    menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::blank, this));
    menuItem->setContentSize(tmpSize);
    menuItem->setPosition(Point::ZERO);
    
    Menu* menu = Menu::create(menuItem, NULL);
    menu->setContentSize(tmpSize);
    menu->setPosition(Point(w * 1.5, h * 0.7 - w * 0.18));
    
    auto label = LabelTTF::create(LocalizedString::getLocalizedString("kisyuhenkou12", "kisyuhenkou12"), FONT, LEVEL_FONT_SIZE, Size(tmpSize.width*0.9, tmpSize.height), TextHAlignment::LEFT, TextVAlignment::CENTER);
    label->setFontFillColor(Color3B(255, 255, 255));
    label->setAnchorPoint(Point(0, 0.5));
    label->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.9));
    image->addChild(label);
    
    // 入力フォーム
    EditBox* editBox = EditBox::create(Size(w * 0.8 , w * 0.14), Scale9Sprite::create("window_base.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100)));
    editBox->setFontName(FONT);
    editBox->setFontSize(LEVEL_FONT_SIZE*1.2);
//    editBox->setPlaceHolder();
    editBox->setInputMode(EditBox::InputMode::SINGLE_LINE);
    editBox->setMaxLength(8);
    editBox->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.6));
    editBox->setDelegate(this);
    image->addChild(editBox, 101, 101);
    
    // 変更ボタン(画像があるだけで実際はボタンじゃない)
    Scale9Sprite* image2 = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image2->setContentSize(Size(w * 0.2 , w * 0.14));
    image2->setPosition(Point(tmpSize.width * 0.8, tmpSize.height * 0.6));
    Size tmpSize2 = image2->getContentSize();
    
    auto label2 = LabelTTF::create(LocalizedString::getLocalizedString("kisyuhenkou13", "kisyuhenkou13"), FONT, LEVEL_FONT_SIZE);
    label2->setAnchorPoint(Point(0.5, 0.5));
    label2->setPosition(Point(tmpSize2.width * 0.5, tmpSize2.height * 0.5));
    label2->setFontFillColor(Color3B(255, 255, 255));
    image2->addChild(label2);
    
    image->addChild(image2, 102, 102);

    // 実行ボタン
    Scale9Sprite* image3 = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image3->setContentSize(Size(w * 0.6 , w * 0.14));
    image3->setPosition(Point::ZERO);
    Size tmpSize3 = image3->getContentSize();
    
    MenuItemSprite* menuItem3;
    menuItem3 = MenuItemSprite::create(image3, image3, CC_CALLBACK_0(HelloWorld::setMenu605_checkEnteredCodeAndCheckServerDataAndConfirm, this, 0));
    menuItem3->setContentSize(tmpSize3);
    menuItem3->setPosition(Point::ZERO);
    
    Menu* menu3 = Menu::create(menuItem3, NULL);
    menu3->setContentSize(tmpSize3);
    menu3->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.2));

    auto label3 = LabelTTF::create(LocalizedString::getLocalizedString("kisyuhenkou14", "kisyuhenkou14"), FONT, LEVEL_FONT_SIZE);
    label3->setAnchorPoint(Point(0.5, 0.5));
    label3->setPosition(Point(tmpSize3.width * 0.5, tmpSize3.height * 0.5));
    label3->setFontFillColor(Color3B(255, 255, 255));
    image3->addChild(label3);
    
    image->addChild(menu3, 103, 103);
    
    
    
    this->addChild(menu, 100, 100);
    
    auto move = MoveBy::create(0.25, Point(-w, 0));
    menu->runAction(move);

}

void HelloWorld::setMenu605_checkEnteredCodeAndCheckServerDataAndConfirm(int pattern)
{
    CCLOG("setMenu605_checkEnteredCodeAndCheckServerDataAndConfirm, code entered : %s", kisyuhenkouCodeEntered);
    this->removeChildByTag(100);
    
    if(pattern == 0 || pattern == 2){ tapProhibit = true; }
    
    Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image->setContentSize(Size(w * 0.9 , w * 0.18));
    Point point = image->getAnchorPoint();
    image->setPosition(Point::ZERO);
    Size tmpSize = image->getContentSize();
    
    MenuItemSprite* menuItem;
    menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::blank, this));
    menuItem->setContentSize(tmpSize);
    menuItem->setPosition(Point::ZERO);
    
    Menu* menu = Menu::create(menuItem, NULL);
    menu->setContentSize(tmpSize);
    menu->setPosition(Point(w * 1.5, h * 0.7));
    
    LabelTTF* label;
    if(pattern == 0 || pattern == 2){
        label = LabelTTF::create(LocalizedString::getLocalizedString("kisyuhenkou15", "kisyuhenkou15"), FONT, LEVEL_FONT_SIZE, Size(tmpSize.width*0.9, tmpSize.height), TextHAlignment::LEFT, TextVAlignment::CENTER);
    }
    if(pattern == 1){
        label = LabelTTF::create(LocalizedString::getLocalizedString("kisyuhenkou16", "kisyuhenkou16"), FONT, LEVEL_FONT_SIZE, Size(tmpSize.width*0.9, tmpSize.height), TextHAlignment::LEFT, TextVAlignment::CENTER);
    }
    if(pattern == 3){
        label = LabelTTF::create(LocalizedString::getLocalizedString("kisyuhenkou19", "kisyuhenkou19"), FONT, LEVEL_FONT_SIZE, Size(tmpSize.width*0.9, tmpSize.height), TextHAlignment::LEFT, TextVAlignment::CENTER);
    }
    label->setFontFillColor(Color3B(255, 255, 255));
    label->setAnchorPoint(Point(0, 0.5));
    label->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.5));
    image->addChild(label);

    this->addChild(menu, 100, 100);
    
    auto move = MoveBy::create(0.25, Point(-w, 0));
    menu->runAction(move);
    
    
    //
    if(pattern == 0){
        // 入力された文字に . / などの文字が無いか確認する
        bool tmpError = false;
        if(strlen(kisyuhenkouCodeEntered) != 8){ tmpError = true; }
        if(strpbrk(kisyuhenkouCodeEntered, ".")){ tmpError = true; }
        if(strpbrk(kisyuhenkouCodeEntered, "/")){ tmpError = true; }
        if(strpbrk(kisyuhenkouCodeEntered, "?")){ tmpError = true; }
        if(strpbrk(kisyuhenkouCodeEntered, "&")){ tmpError = true; }
        if(strpbrk(kisyuhenkouCodeEntered, "'")){ tmpError = true; }
        if(tmpError){
            tapProhibit = false;
            setMenu605_checkEnteredCodeAndCheckServerDataAndConfirm(1);
            return;
        }
        
        // サーバにファイルがあるか確認に行く
        char tmpUrl[64];
        sprintf(tmpUrl, "http://1min-quest.com/kisyuhenkou/%s", kisyuhenkouCodeEntered);
        CCLOG("check %s", tmpUrl);
        
        auto request = new HttpRequest();
        request->setUrl(tmpUrl);
        request->setRequestType(HttpRequest::Type::GET);
        request->setResponseCallback(this, httpresponse_selector(HelloWorld::setMenu605_checkEnteredCodeAndCheckServerDataAndConfirmCallback));
        request->setTag("GET userData");
        HttpClient::getInstance()->send(request);
        request->release();

    }
    if(pattern == 2){
        // サーバにファイルがあるか確認に行く
        char tmpUrl[64];
        sprintf(tmpUrl, "http://1min-quest.com/kisyuhenkou/%s", kisyuhenkouCodeEntered);
        CCLOG("check %s", tmpUrl);
        
        auto request = new HttpRequest();
        request->setUrl(tmpUrl);
        request->setRequestType(HttpRequest::Type::GET);
        request->setResponseCallback(this, httpresponse_selector(HelloWorld::setMenu605_checkEnteredCodeAndCheckServerDataAndConfirmCallback2));
        request->setTag("GET userData");
        HttpClient::getInstance()->send(request);
        request->release();
        
    }
}

void HelloWorld::setMenu605_checkEnteredCodeAndCheckServerDataAndConfirmCallback(HttpClient* sender, HttpResponse* response)
{
    if(!(response->isSucceed())){ tapProhibit = false; setMenu605_checkEnteredCodeAndCheckServerDataAndConfirm(1); return; }// 圏外対策

    this->removeChildByTag(100);
    tapProhibit = false;
    
    Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image->setContentSize(Size(w * 0.9 , w * 0.18 * 2));
    Point point = image->getAnchorPoint();
    image->setPosition(Point::ZERO);
    Size tmpSize = image->getContentSize();
    
    MenuItemSprite* menuItem;
    menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::blank, this));
    menuItem->setContentSize(tmpSize);
    menuItem->setPosition(Point::ZERO);
    
    Menu* menu = Menu::create(menuItem, NULL);
    menu->setContentSize(tmpSize);
    menu->setPosition(Point(w * 1.5, h * 0.7 - w * 0.18 / 2));
    
    auto label = LabelTTF::create(LocalizedString::getLocalizedString("kisyuhenkou17", "kisyuhenkou17"), FONT, LEVEL_FONT_SIZE, Size(tmpSize.width*0.9, tmpSize.height), TextHAlignment::LEFT, TextVAlignment::CENTER);
    label->setFontFillColor(Color3B(255, 255, 255));
    label->setAnchorPoint(Point(0, 0.5));
    label->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.8));
    image->addChild(label);
    
    auto label2 = LabelTTF::create(LocalizedString::getLocalizedString("kisyuhenkou4", "kisyuhenkou4"), FONT, LEVEL_FONT_SIZE * 0.8, Size(tmpSize.width*0.9, tmpSize.height), TextHAlignment::LEFT, TextVAlignment::CENTER);
    label2->setFontFillColor(Color3B(255, 255, 255));
    label2->setAnchorPoint(Point(0, 0.5));
    label2->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.6));
    image->addChild(label2);
    

    
    // 実行ボタン
    Scale9Sprite* image3 = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image3->setContentSize(Size(w * 0.6 , w * 0.14));
    image3->setPosition(Point::ZERO);
    Size tmpSize3 = image3->getContentSize();
    
    MenuItemSprite* menuItem3;
    menuItem3 = MenuItemSprite::create(image3, image3, CC_CALLBACK_0(HelloWorld::setMenu605_checkEnteredCodeAndCheckServerDataAndConfirm, this, 2));
    menuItem3->setContentSize(tmpSize3);
    menuItem3->setPosition(Point::ZERO);
    
    Menu* menu3 = Menu::create(menuItem3, NULL);
    menu3->setContentSize(tmpSize3);
    menu3->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.3));
    
    auto label3 = LabelTTF::create(LocalizedString::getLocalizedString("kisyuhenkou18", "kisyuhenkou18"), FONT, LEVEL_FONT_SIZE);
    label3->setAnchorPoint(Point(0.5, 0.5));
    label3->setPosition(Point(tmpSize3.width * 0.5, tmpSize3.height * 0.5));
    label3->setFontFillColor(Color3B(255, 255, 255));
    image3->addChild(label3);
    
    image->addChild(menu3, 103, 103);
    
    
    
    this->addChild(menu, 100, 100);
    
    auto move = MoveBy::create(0.25, Point(-w, 0));
    menu->runAction(move);
    
}

void HelloWorld::setMenu605_checkEnteredCodeAndCheckServerDataAndConfirmCallback2(HttpClient* sender, HttpResponse* response)
{
    if(!(response->isSucceed())){ tapProhibit = false; setMenu605_checkEnteredCodeAndCheckServerDataAndConfirm(3); return; }// 圏外対策

    CCLOG("ダウンロードファイルを上書きしてHelloWorldに飛ばす");
    
    //データ取得
    std::vector<char>* buffer = response->getResponseData();
    
    // dump data
//    for (unsigned int i = 0; i < buffer->size(); i++) printf("%c", (*buffer)[i]); printf("\n");
    
    char* userdataDB = (char *) malloc(buffer->size() + 1);
    std::string s2(buffer->begin(), buffer->end());
    strcpy(userdataDB, s2.c_str());

    
    // データ書き込み
    FILE* fp;
    
    std::string fullpath;
    fullpath = FileUtils::getInstance()->getWritablePath();
    fullpath += "userdata.db";
    
    fp = fopen(fullpath.c_str(), "wb");
    CCLOG("%s", s2.c_str());
    fwrite(s2.c_str(), 1, s2.size(), fp);
    //    fwrite(buffer, 1, buffer->size(), fp);
    fclose(fp);
    
    // サーバーアクセスしてファイルを削除
    char postData[64];
    sprintf(postData, "code=%s", kisyuhenkouCodeEntered);
    auto request = new HttpRequest();
    request->setUrl("http://1min-quest.com/kisyuhenkou_cancel.php");
    request->setRequestType(HttpRequest::Type::POST);
    request->setResponseCallback(this, httpresponse_selector(HelloWorld::setMenu605_checkEnteredCodeAndCheckServerDataAndConfirmCallback3));
    request->setRequestData(postData, strlen(postData));
    request->setTag("GET userData");
    HttpClient::getInstance()->send(request);
    request->release();
    
    //debug
    //auto scene = HelloWorld::createScene();
    //Director::getInstance()->replaceScene(scene);
    
}

void HelloWorld::setMenu605_checkEnteredCodeAndCheckServerDataAndConfirmCallback3(HttpClient* sender, HttpResponse* response)
{
    tapProhibit = false;
    
    setMenu605_kisyuhenkoutyuuCheck(3);
}

void HelloWorld::setMenu605_uploadData()
{
    this->removeChildByTag(100);
    this->removeChildByTag(101);
    
    tapProhibit = true;
    
    // データ移行
    Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image->setContentSize(Size(w * 0.9 , w * 0.18));
    Point point = image->getAnchorPoint();
    image->setPosition(Point::ZERO);
    Size tmpSize = image->getContentSize();
    
    MenuItemSprite* menuItem;
    menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::blank, this));
    menuItem->setContentSize(tmpSize);
    menuItem->setPosition(Point::ZERO);
    
    Menu* menu = Menu::create(menuItem, NULL);
    menu->setContentSize(tmpSize);
    menu->setPosition(Point(w * 1.5, h * 0.7));
    
    auto label = LabelTTF::create(LocalizedString::getLocalizedString("kisyuhenkou5", "kisyuhenkou5"), FONT, LEVEL_FONT_SIZE, Size(tmpSize.width*0.9, tmpSize.height), TextHAlignment::LEFT, TextVAlignment::CENTER);
    label->setFontFillColor(Color3B(255, 255, 255));
    label->setAnchorPoint(Point(0, 0.5));
    label->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.66));
    image->addChild(label);
    
    auto label2 = LabelTTF::create(LocalizedString::getLocalizedString("kisyuhenkou6", "kisyuhenkou6"), FONT, LEVEL_FONT_SIZE*0.8, Size(tmpSize.width*0.9, tmpSize.height), TextHAlignment::LEFT, TextVAlignment::CENTER);
    label2->setFontFillColor(Color3B(255, 255, 255));
    label2->setAnchorPoint(Point(0, 0.5));
    label2->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.33));
    image->addChild(label2);
    
    this->addChild(menu, 100, 100);
    
    auto move = MoveBy::create(0.25, Point(-w, 0));
    menu->runAction(move);
    
    // データ送信
    FILE* fp;
    long filesize;
    char * buffer;
    
    std::string fullpath;
    fullpath = FileUtils::getInstance()->getWritablePath();
    fullpath += "userdata.db";
    
    fp = fopen(fullpath.c_str(), "r");
    fseek (fp , 0 , SEEK_END);
    filesize = ftell(fp);
    rewind(fp);
    CCLOG("upload %ld bytes", filesize);
    
    buffer = (char*) malloc (sizeof(char)*filesize);
    fread (buffer,1,filesize,fp);

    auto request = new HttpRequest();
    request->setUrl("http://1min-quest.com/kisyuhenkou.php");
    request->setRequestType(HttpRequest::Type::PUT);
    request->setResponseCallback(this, httpresponse_selector(HelloWorld::setMenu605_uploadDataCallback));
    request->setRequestData((const char*)buffer, ftell(fp));
    request->setTag("PUT userData");
    HttpClient::getInstance()->send(request);
    request->release();
    
    fclose(fp);
    free(buffer);

}

void HelloWorld::setMenu605_uploadDataCallback(HttpClient* sender, HttpResponse* response)
{
    CCLOG("setMenu605_uploadDataCallback()");
    
    picojson::value response_data;
    std::string err;
    
    if(!(response->isSucceed())){ tapProhibit = false; setMenu605_uploadDataFailed(); return; }// 圏外対策
    
    CCASSERT(response, "response empty");
    CCASSERT(response->isSucceed(), "response failed");
    
    //Json取得
    std::vector<char>* buffer = response->getResponseData();
    
    // dump data
    for (unsigned int i = 0; i < buffer->size(); i++) printf("%c", (*buffer)[i]); printf("\n");
    
    char * json = (char *) malloc(buffer->size() + 1);
    std::string s2(buffer->begin(), buffer->end());
    strcpy(json, s2.c_str());
    picojson::parse(response_data, json, json + strlen(json), &err);
    
    CCASSERT(err.empty(), err.c_str());
    picojson::object& all_data = response_data.get<picojson::object>();
    
    auto pass = Value(all_data["KISYUHENKOU"].get<std::string>());
    CCLOG("%s", pass.asString().c_str());

    
    // DB保存
    sqlite3* db;
    std::string fullpath;
    
    db = NULL;
    fullpath = FileUtils::getInstance()->getWritablePath();
    fullpath += "userdata.db";
    
    if (sqlite3_open(fullpath.c_str(), &db) == SQLITE_OK) {
        char tmpSql[512];
        sprintf(tmpSql, "update user_data set kisyuhenkou = '%s' where _id = 1", pass.asString().c_str());
        std::string sql = tmpSql;
        CCLOG("%s", sql.c_str());
        sqlite3_exec(db, sql.c_str(), NULL, NULL, NULL);
        
    }
    // close db
    sqlite3_close(db);
    
    auto scene = HelloWorld::createScene();
    Director::getInstance()->replaceScene(scene);
    
    tapProhibit = false;
}


void HelloWorld::setMenu605_uploadDataFailed()
{
    this->removeChildByTag(100);
    
    // データ移行
    Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image->setContentSize(Size(w * 0.9 , w * 0.18));
    Point point = image->getAnchorPoint();
    image->setPosition(Point::ZERO);
    Size tmpSize = image->getContentSize();
    
    MenuItemSprite* menuItem;
    menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::blank, this));
    menuItem->setContentSize(tmpSize);
    menuItem->setPosition(Point::ZERO);
    
    Menu* menu = Menu::create(menuItem, NULL);
    menu->setContentSize(tmpSize);
    menu->setPosition(Point(w * 1.5, h * 0.7));
    
    auto label = LabelTTF::create(LocalizedString::getLocalizedString("kisyuhenkou7", "kisyuhenkou7"), FONT, LEVEL_FONT_SIZE, Size(tmpSize.width*0.9, tmpSize.height), TextHAlignment::LEFT, TextVAlignment::CENTER);
    label->setFontFillColor(Color3B(255, 255, 255));
    label->setAnchorPoint(Point(0, 0.5));
    label->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.50));
    image->addChild(label);
    
    this->addChild(menu, 100, 100);
    
    auto move = MoveBy::create(0.25, Point(-w, 0));
    menu->runAction(move);
    
}


void HelloWorld::setMenu605_kisyuhenkoutyuuCheck(int pattern)
{
    this->removeChildByTag(100);
    this->removeChildByTag(101);
    
    playSE(0);
    
    // データ移行中
    Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image->setContentSize(Size(w , h));
    Point point = image->getAnchorPoint();
    image->setPosition(Point::ZERO);
    Size tmpSize = image->getContentSize();
    
    MenuItemSprite* menuItem;
    menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::blank, this));
    menuItem->setContentSize(tmpSize);
    menuItem->setPosition(Point::ZERO);
    
    Menu* menu = Menu::create(menuItem, NULL);
    menu->setContentSize(tmpSize);
    menu->setPosition(Point(w * 1.5, h * 0.5));
    
    LabelTTF* label;
    // 起動時のチェック
    if(pattern == 0){
        label = LabelTTF::create(LocalizedString::getLocalizedString("kisyuhenkou8", "kisyuhenkou8"), FONT, LEVEL_FONT_SIZE, Size(tmpSize.width*0.9, tmpSize.height), TextHAlignment::CENTER, TextVAlignment::CENTER);
    }
    // 機種変更まち
    else if(pattern == 1){
        label = LabelTTF::create(LocalizedString::getLocalizedString("kisyuhenkou9", "kisyuhenkou9"), FONT, LEVEL_FONT_SIZE, Size(tmpSize.width*0.9, tmpSize.height), TextHAlignment::CENTER, TextVAlignment::CENTER);
        
    }
    // 変更処理が完了した
    else if(pattern == 3){
        label = LabelTTF::create(LocalizedString::getLocalizedString("kisyuhenkou20", "kisyuhenkou20"), FONT, LEVEL_FONT_SIZE, Size(tmpSize.width*0.9, tmpSize.height), TextHAlignment::CENTER, TextVAlignment::CENTER);
        
    }
    // キャンセル処理が完了した
    else if(pattern == 4){
        label = LabelTTF::create(LocalizedString::getLocalizedString("kisyuhenkou21", "kisyuhenkou21"), FONT, LEVEL_FONT_SIZE, Size(tmpSize.width*0.9, tmpSize.height), TextHAlignment::CENTER, TextVAlignment::CENTER);
        
    }
    // 機種変更完了済み
    else{
        label = LabelTTF::create(LocalizedString::getLocalizedString("kisyuhenkou10", "kisyuhenkou10"), FONT, LEVEL_FONT_SIZE, Size(tmpSize.width*0.9, tmpSize.height), TextHAlignment::CENTER, TextVAlignment::CENTER);
        
    }
    label->setFontFillColor(Color3B(255, 255, 255));
    label->setAnchorPoint(Point(0.5, 0.5));
    label->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.60));
    image->addChild(label);
    
    
    sqlite3* db = NULL;
    std::string fullpath = FileUtils::getInstance()->getWritablePath();
    fullpath += "userdata.db";
    
    char kisyuhenkou[64];
    sprintf(kisyuhenkou, "");
    
    if (sqlite3_open(fullpath.c_str(), &db) == SQLITE_OK) {
        sqlite3_stmt *stmt = NULL;
        std::string sql = "select * from user_data";
        if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
            if(sqlite3_step(stmt) == SQLITE_ROW){
                sprintf(kisyuhenkou, "%s", sqlite3_column_text(stmt, 21));
            }
        }
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);

    // 機種変更コードを表示
    if(pattern == 1){
        auto label3 = LabelTTF::create(kisyuhenkou, FONT, LEVEL_FONT_SIZE * 2);
        label3->setFontFillColor(Color3B(255, 255, 255));
        label3->setAnchorPoint(Point(0.5, 0.5));
        label3->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.45));
        image->addChild(label3);
    }
    
    this->addChild(menu, 100000000, 100);
    
    auto move = MoveBy::create(0.25, Point(-w, 0));
    menu->runAction(move);
    
    // 機種変更とりやめボタン
    if(pattern == 1){
        
        Scale9Sprite* image2 = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
        image2->setContentSize(Size(w * 0.5 , w * 0.18));
        Point point = image->getAnchorPoint();
        image2->setPosition(Point::ZERO);
        Size tmpSize2 = image2->getContentSize();
        
        MenuItemSprite* menuItem2;
        menuItem2 = MenuItemSprite::create(image2, image2, CC_CALLBACK_0(HelloWorld::setMenu605_kisyuhenkouCancel, this));
        menuItem2->setContentSize(tmpSize2);
        menuItem2->setPosition(Point::ZERO);
        
        Menu* menu2 = Menu::create(menuItem2, NULL);
        menu2->setContentSize(tmpSize2);
        menu2->setPosition(Point(w * 1.5, h * 0.25));
        
        auto label2 = LabelTTF::create(LocalizedString::getLocalizedString("kisyuhenkou11", "kisyuhenkou11"), FONT, LEVEL_FONT_SIZE, Size(tmpSize.width*0.9, tmpSize.height), TextHAlignment::CENTER, TextVAlignment::CENTER);
        label2->setFontFillColor(Color3B(255, 255, 255));
        label2->setAnchorPoint(Point(0.5, 0.5));
        label2->setPosition(Point(tmpSize2.width * 0.5, tmpSize2.height * 0.5));
        image2->addChild(label2);
        this->addChild(menu2, 100000001, 101);
        
        auto move2 = MoveBy::create(0.25, Point(-w, 0));
        menu2->runAction(move2);
        
    }
    
    // BGM再生
    CocosDenshion::SimpleAudioEngine::getInstance()->setBackgroundMusicVolume(0.5);
    CocosDenshion::SimpleAudioEngine::getInstance()->setEffectsVolume(0.5);
    CocosDenshion::SimpleAudioEngine::getInstance()->stopBackgroundMusic();
    CocosDenshion::SimpleAudioEngine::getInstance()->playBackgroundMusic("bgm/bgm_main.mp3", true);

    // 移行中か(ファイルがあるか)チェックする
    if(pattern == 0){
        char tmpUrl[128];
        sprintf(tmpUrl, "http://1min-quest.com/kisyuhenkou/%s", kisyuhenkou);

        auto request = new HttpRequest();
        request->setUrl(tmpUrl);
        request->setRequestType(HttpRequest::Type::GET);
        request->setResponseCallback(this, httpresponse_selector(HelloWorld::setMenu605_checkDataCallback));
        request->setTag("GET userData ckeckKisyuhenkouData");
        HttpClient::getInstance()->send(request);
        request->release();
    }
    
    // 処理完了の際はHelloWorldに飛ばす
    if(pattern == 3 || pattern == 4){
        auto delay = DelayTime::create(2.0);
        auto func = CallFunc::create(CC_CALLBACK_0(HelloWorld::setMenu605_gotoHelloWorld, this));
        this->runAction(Sequence::create(delay, func, NULL));
    }
}

void HelloWorld::setMenu605_checkDataCallback(HttpClient* sender, HttpResponse* response)
{
    if(!(response->isSucceed())){ setMenu605_kisyuhenkoutyuuCheck(2);  return; }// 圏外対策
    
    CCASSERT(response, "response empty");
    CCASSERT(response->isSucceed(), "response failed");

    if(!kisyuhenkouCanceling){
        setMenu605_kisyuhenkoutyuuCheck(1);
    }
    else{ // 機種変更をキャンセル。
        CCLOG("機種変更をキャンセル。ローカルのbikouデータを削除。サーバ上のデータを削除する。");
        kisyuhenkouCanceling = false;
        
        
        // DBから機種変更コード取得、その後削除
        char kisyuhenkou[64];
        sprintf(kisyuhenkou, "");

        sqlite3* db;
        std::string fullpath;
        
        db = NULL;
        fullpath = FileUtils::getInstance()->getWritablePath();
        fullpath += "userdata.db";
        
        if (sqlite3_open(fullpath.c_str(), &db) == SQLITE_OK) {
            // データ取得
            sqlite3_stmt *stmt = NULL;
            std::string sql = "select * from user_data";
            if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
                sqlite3_step(stmt);
                sprintf(kisyuhenkou, "%s", sqlite3_column_text(stmt, 21));
            }
            sqlite3_reset(stmt);
            sqlite3_finalize(stmt);
            
            // 機種変更状態を解除
            char tmpSql[512];
            sprintf(tmpSql, "update user_data set kisyuhenkou = 'null' where _id = 1" );
            std::string sql2 = tmpSql;
            CCLOG("%s", sql2.c_str());
            sqlite3_exec(db, sql2.c_str(), NULL, NULL, NULL);
        }
        // close db
        sqlite3_close(db);
        
        // サーバ上のデータ削除
        char postData[64];
        sprintf(postData, "code=%s", kisyuhenkou);
        auto request = new HttpRequest();
        request->setUrl("http://1min-quest.com/kisyuhenkou_cancel.php");
        request->setRequestType(HttpRequest::Type::POST);
        request->setResponseCallback(this, httpresponse_selector(HelloWorld::setMenu605_kisyuhenkouCancel_deleteCallback));
        request->setRequestData(postData, strlen(postData));
        request->setTag("GET userData");
        HttpClient::getInstance()->send(request);
        request->release();
    }
}

void HelloWorld::setMenu605_kisyuhenkouCancel()
{
    CCLOG("kisyuhenkou Cancel");
    kisyuhenkouCanceling = true;
    setMenu605_kisyuhenkoutyuuCheck(0);
}

void HelloWorld::setMenu605_kisyuhenkouCancel_deleteCallback(HttpClient* sender, HttpResponse* response)
{
    // 直前でファイルがあるかチェックしているから、サーバや圏外は問題ないと思うけど、これどうしよう。
    if(!(response->isSucceed())){ CCLOG("setMenu605_kisyuhenkouCancel_deleteCallback() error"); }// 圏外対策

    setMenu605_kisyuhenkoutyuuCheck(4);
}

void HelloWorld::setMenu605_gotoHelloWorld()
{
    auto scene = HelloWorld::createScene();
    Director::getInstance()->replaceScene(scene);
}


void HelloWorld::setMenu610()
{
    
    // その他その他
    for(int i = 0; i < 4; i++){
        Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
        image->setContentSize(Size(w * 0.9 , w * 0.18));
        Point point = image->getAnchorPoint();
        image->setPosition(Point::ZERO);
        
        char tmpChar[64];
        if(i==0){
            sprintf(tmpChar, "%s", LocalizedString::getLocalizedString("pageTitle611", "pageTitle611"));
        }
        if(i==1){
            sprintf(tmpChar, "%s", LocalizedString::getLocalizedString("pageTitle612", "pageTitle612"));
        }
        if(i==2){
            sprintf(tmpChar, "%s", LocalizedString::getLocalizedString("pageTitle613", "pageTitle613"));
        }
        if(i==3){
            sprintf(tmpChar, "%s", LocalizedString::getLocalizedString("pageTitle614", "pageTitle614"));
        }
        
        auto label = LabelTTF::create(tmpChar, FONT, LEVEL_FONT_SIZE);
        Size tmpSize = image->getContentSize();
        label->setAnchorPoint(Point(0, 0.5));
        label->setPosition(Point(tmpSize.width * 0.05, tmpSize.height * 0.5));
        label->setFontFillColor(Color3B(255, 255, 255));
        image->addChild(label);
        
        MenuItemSprite* menuItem;
        if(i==0){
            menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::setMenu604_riyoukiyaku, this));
        }
        if(i==1){
            menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::setMenu603, this));
        }
        if(i==2){
            menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::setMenu604, this));
        }
        if(i==3){
            menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::setMenu605, this));
        }
        menuItem->setContentSize(tmpSize);
        menuItem->setPosition(Point::ZERO);
        
        Menu* menu = Menu::create2(menuItem, NULL);
        menu->setContentSize(tmpSize);
        menu->setPosition(Point(w * 1.5, h * 0.7 - w * 0.18 * i));
        
        this->addChild(menu, 101+i, 101+i);
        
        auto move = MoveBy::create(0.25 + 0.05 * i, Point(-w, 0));
        menu->runAction(move);
    }
}

void HelloWorld::setMenu611()
{
    playSE(0);
    
    // ここでお勧めアプリを表示する
    CCLOG("お勧めアプリを表示する");
    
    // add GAMEFEET
    NativeManager::showGameFeat();
    
}



//////////////////////////////////////////////////
//////////////////////////////////////////////////
//////////////////////////////////////////////////
//  ユーザーデータベースの初期データ作成
//////////////////////////////////////////////////
//////////////////////////////////////////////////
//////////////////////////////////////////////////

void HelloWorld::makeUserDb()
{
    sqlite3* db;
    std::string fullpath;
    
    db = NULL;
    fullpath = FileUtils::getInstance()->getWritablePath();
    fullpath += "userdata.db";
    
    if(!(FileUtils::getInstance()->isFileExist(fullpath.c_str()))){
        CCLOG("generate user db.");
        
        if (sqlite3_open(fullpath.c_str(), &db) == SQLITE_OK) {
            
            sqlite3_exec(db, "CREATE TABLE user_data (_id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, stamina INTEGER NOT NULL DEFAULT 0, stamina_max INTEGER NOT NULL DEFAULT 0, last_stamina_decrease_time INTEGER NOT NULL DEFAULT 0, battle_stamina INTEGER NOT NULL DEFAULT 0, battle_stamina_max INTEGER NOT NULL DEFAULT 0, last_battle_stamina_decrease_time INTEGER NOT NULL DEFAULT 0, coin INTEGER NOT NULL DEFAULT 0, mahouseki INTEGER NOT NULL DEFAULT 0, last_mahouseki_got_time INTEGER NOT NULL DEFAULT 0, visible_dungeon_number INTEGER NOT NULL DEFAULT 0, visited_dungeon_number INTEGER NOT NULL DEFAULT 0, jinkei INTEGER NOT NULL DEFAULT 0, battle_rank INTEGER NOT NULL DEFAULT 0, battle_count INTEGER NOT NULL DEFAULT 0, battle_win_count INTEGER NOT NULL DEFAULT 0, battle_count_at_current_rank INTEGER NOT NULL DEFAULT 0, battle_win_count_at_current_rank INTEGER NOT NULL DEFAULT 0, mahouseki_purchase INTEGER NOT NULL DEFAULT 0, last_twitter_post_time INTEGER NOT NULL DEFAULT 0, kisyuhenkou TEXT)", NULL, NULL, NULL);
            
            char tmpSql[2048];
            char tmpNameString[64];
            sprintf(tmpNameString, "defaultUserName%d", rand() % 300);
            CCLOG("%s", tmpNameString);
            
            sprintf(tmpSql, "INSERT INTO user_data (_id,name,stamina,stamina_max,last_stamina_decrease_time,battle_stamina,battle_stamina_max,last_battle_stamina_decrease_time,coin,mahouseki,last_mahouseki_got_time,visible_dungeon_number,visited_dungeon_number,jinkei,battle_rank,battle_count,battle_win_count,battle_count_at_current_rank,battle_win_count_at_current_rank,mahouseki_purchase,last_twitter_post_time,kisyuhenkou) VALUES (1,'%s',14,14,0,3,3,0,999999999,990,0,1,0,0,1,0,0,0,0,0,0,'null')", LocalizedString::getLocalizedString(tmpNameString, "andy"));
            CCLOG("%s", tmpSql);
            sqlite3_exec(db, tmpSql, NULL, NULL, NULL);
            
            // Unit
            sqlite3_exec(db, "CREATE TABLE user_unit (_id INTEGER PRIMARY KEY AUTOINCREMENT, class_type INTEGER NOT NULL DEFAULT 0, level INTEGER NOT NULL DEFAULT 0, class_level INTEGER NOT NULL DEFAULT 0, hp INTEGER NOT NULL DEFAULT 0, physical INTEGER NOT NULL DEFAULT 0, magical INTEGER NOT NULL DEFAULT 0, physical_def INTEGER NOT NULL DEFAULT 0, magical_def INTEGER NOT NULL DEFAULT 0, skill1 INTEGER NOT NULL DEFAULT 0, skill2 INTEGER NOT NULL DEFAULT 0, class_skill INTEGER NOT NULL DEFAULT 0, zokusei INTEGER NOT NULL DEFAULT 0)", NULL, NULL, NULL);
            
            
            sqlite3_exec(db, "INSERT INTO user_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,1,1,1,20,8,2,1,1,0,0,0,0)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,2,1,1,18,0,10,1,1,0,0,0,1)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,3,1,1,18,4,5,1,1,0,0,0,2)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,4,1,1,20,10,0,1,1,0,0,0,0)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,5,1,1,18,5,4,1,1,0,0,0,1)", NULL, NULL, NULL);
            /*
            sqlite3_exec(db, "INSERT INTO user_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,6,1,1,30,10,10,1,1,0,0,0,2)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,7,1,1,30,10,10,1,1,0,0,0,3)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,8,1,1,30,10,10,1,1,0,0,0,4)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,9,1,1,30,10,10,1,1,0,0,0,5)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,10,1,1,30,10,10,1,1,0,0,0,6)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,11,1,1,30,10,10,1,1,0,0,0,7)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,12,1,1,30,10,10,1,1,0,0,0,8)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,13,1,1,30,10,10,1,1,0,0,0,9)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,14,1,1,30,10,10,1,1,0,0,0,10)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,15,1,1,30,10,10,1,1,0,0,0,11)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,16,1,1,30,10,10,1,1,0,0,0,0)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,17,1,1,30,10,10,1,1,0,0,0,3)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO 1ser_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,18,1,1,30,10,10,1,1,0,0,0,2)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,19,1,1,30,10,10,1,1,0,0,0,3)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,20,1,1,30,10,10,1,1,0,0,0,4)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,21,1,1,30,10,10,1,1,0,0,0,5)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,22,1,1,30,10,10,1,1,0,0,0,6)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,23,1,1,30,10,10,1,1,0,0,0,7)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,24,1,1,30,10,10,1,1,0,0,0,8)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,25,1,1,30,10,10,1,1,0,0,0,9)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,26,1,1,30,10,10,1,1,0,0,0,10)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,27,1,1,30,10,10,1,1,0,0,0,11)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,28,1,1,30,10,10,1,1,0,0,0,0)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,29,1,1,30,10,10,1,1,0,0,0,1)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,30,1,1,30,10,10,1,1,0,0,0,2)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,31,1,1,30,10,10,1,1,0,0,0,3)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,32,1,1,30,10,10,1,1,0,0,0,4)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,33,1,1,30,10,10,1,1,0,0,0,5)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,34,1,1,30,10,10,1,1,0,0,0,6)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,35,1,1,30,10,10,1,1,0,0,0,7)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,36,1,1,30,10,10,1,1,0,0,0,8)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,37,1,1,30,10,10,1,1,0,0,0,9)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,38,1,1,30,10,10,1,1,0,0,0,10)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_unit (_id,class_type,level,class_level,hp,physical,magical,physical_def,magical_def,skill1,skill2,class_skill,zokusei) VALUES (NULL,39,1,1,30,10,10,1,1,0,0,0,11)", NULL, NULL, NULL);
             */
           
            // Party
            sqlite3_exec(db, "CREATE TABLE user_party (_id INTEGER PRIMARY KEY AUTOINCREMENT, unit_order INTEGER NOT NULL DEFAULT 0, unit_id INTEGER NOT NULL DEFAULT 0)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_party (_id,unit_order,unit_id) VALUES (1,0,1)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_party (_id,unit_order,unit_id) VALUES (2,1,2)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_party (_id,unit_order,unit_id) VALUES (3,2,3)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_party (_id,unit_order,unit_id) VALUES (4,3,4)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_party (_id,unit_order,unit_id) VALUES (5,4,5)", NULL, NULL, NULL);
            
            // Party_Arena
            sqlite3_exec(db, "CREATE TABLE user_party2 (_id INTEGER PRIMARY KEY AUTOINCREMENT, unit_order INTEGER NOT NULL DEFAULT 0, unit_id INTEGER NOT NULL DEFAULT 0)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_party2 (_id,unit_order,unit_id) VALUES (1,0,1)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_party2 (_id,unit_order,unit_id) VALUES (2,1,2)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_party2 (_id,unit_order,unit_id) VALUES (3,2,3)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_party2 (_id,unit_order,unit_id) VALUES (4,3,4)", NULL, NULL, NULL);
            sqlite3_exec(db, "INSERT INTO user_party2 (_id,unit_order,unit_id) VALUES (5,4,5)", NULL, NULL, NULL);
            
            // skill list
            sqlite3_exec(db, "CREATE TABLE user_skill_list (_id INTEGER PRIMARY KEY AUTOINCREMENT, skill_id INTEGER DEFAULT 0, level INTEGER DEFAULT 0, soubi_unit_id INTEGER DEFAULT 0)", NULL, NULL, NULL);
            
            // store products
            sqlite3_exec(db, "CREATE TABLE store_products (_id INTEGER PRIMARY KEY AUTOINCREMENT, lastchecktime INTEGER NOT NULL DEFAULT 0, product_id TEXT, description TEXT, price TEXT)", NULL, NULL, NULL);
            
            // OtherUsersParty
            sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS other_users_party (  _id INTEGER PRIMARY KEY AUTOINCREMENT,  name TEXT NOT NULL,  rank INTEGER NOT NULL DEFAULT 0,  c1_type INTEGER NOT NULL DEFAULT 0,  c1_level INTEGER NOT NULL DEFAULT 0,  c1_class_level INTEGER NOT NULL DEFAULT 0,  c1_hp INTEGER NOT NULL DEFAULT 0,  c1_physical_atk INTEGER NOT NULL DEFAULT 0,  c1_physical_def INTEGER NOT NULL DEFAULT 0,  c1_magical_atk INTEGER NOT NULL DEFAULT 0,  c1_magical_def INTEGER NOT NULL DEFAULT 0,  c1_skill1 INTEGER NOT NULL DEFAULT 0,  c1_skill1_lv INTEGER NOT NULL DEFAULT 0,  c1_skill2 INTEGER NOT NULL DEFAULT 0,  c1_skill2_lv INTEGER NOT NULL DEFAULT 0,  c1_zokusei INTEGER NOT NULL DEFAULT 0,  c2_type INTEGER NOT NULL DEFAULT 0,  c2_level INTEGER NOT NULL DEFAULT 0,  c2_class_level INTEGER NOT NULL DEFAULT 0,  c2_hp INTEGER NOT NULL DEFAULT 0,  c2_physical_atk INTEGER NOT NULL DEFAULT 0,  c2_physical_def INTEGER NOT NULL DEFAULT 0,  c2_magical_atk INTEGER NOT NULL DEFAULT 0,  c2_magical_def INTEGER NOT NULL DEFAULT 0,  c2_skill1 INTEGER NOT NULL DEFAULT 0,  c2_skill1_lv INTEGER NOT NULL DEFAULT 0,  c2_skill2 INTEGER NOT NULL DEFAULT 0,  c2_skill2_lv INTEGER NOT NULL DEFAULT 0,  c2_zokusei INTEGER NOT NULL DEFAULT 0,  c3_type INTEGER NOT NULL DEFAULT 0,  c3_level INTEGER NOT NULL DEFAULT 0,  c3_class_level INTEGER NOT NULL DEFAULT 0,  c3_hp INTEGER NOT NULL DEFAULT 0,  c3_physical_atk INTEGER NOT NULL DEFAULT 0,  c3_physical_def INTEGER NOT NULL DEFAULT 0,  c3_magical_atk INTEGER NOT NULL DEFAULT 0,  c3_magical_def INTEGER NOT NULL DEFAULT 0,  c3_skill1 INTEGER NOT NULL DEFAULT 0,  c3_skill1_lv INTEGER NOT NULL DEFAULT 0,  c3_skill2 INTEGER NOT NULL DEFAULT 0,  c3_skill2_lv INTEGER NOT NULL DEFAULT 0,  c3_zokusei INTEGER NOT NULL DEFAULT 0,  c4_type INTEGER NOT NULL DEFAULT 0,  c4_level INTEGER NOT NULL DEFAULT 0,  c4_class_level INTEGER NOT NULL DEFAULT 0,  c4_hp INTEGER NOT NULL DEFAULT 0,  c4_physical_atk INTEGER NOT NULL DEFAULT 0,  c4_physical_def INTEGER NOT NULL DEFAULT 0,  c4_magical_atk INTEGER NOT NULL DEFAULT 0,  c4_magical_def INTEGER NOT NULL DEFAULT 0,  c4_skill1 INTEGER NOT NULL DEFAULT 0,  c4_skill1_lv INTEGER NOT NULL DEFAULT 0,  c4_skill2 INTEGER NOT NULL DEFAULT 0,  c4_skill2_lv INTEGER NOT NULL DEFAULT 0,  c4_zokusei INTEGER NOT NULL DEFAULT 0,  c5_type INTEGER NOT NULL DEFAULT 0,  c5_level INTEGER NOT NULL DEFAULT 0,  c5_class_level INTEGER NOT NULL DEFAULT 0,  c5_hp INTEGER NOT NULL DEFAULT 0,  c5_physical_atk INTEGER NOT NULL DEFAULT 0,  c5_physical_def INTEGER NOT NULL DEFAULT 0,  c5_magical_atk INTEGER NOT NULL DEFAULT 0,  c5_magical_def INTEGER NOT NULL DEFAULT 0,  c5_skill1 INTEGER NOT NULL DEFAULT 0,  c5_skill1_lv INTEGER NOT NULL DEFAULT 0,  c5_skill2 INTEGER NOT NULL DEFAULT 0,  c5_skill2_lv INTEGER NOT NULL DEFAULT 0,  c5_zokusei INTEGER NOT NULL DEFAULT 0) ", NULL, NULL, NULL);
            
            // SpecialDangeonClearList
            sqlite3_exec(db, "CREATE TABLE special_dangeon_clear_list (_id INTEGER PRIMARY KEY AUTOINCREMENT, dangeon_id INTEGER)", NULL, NULL, NULL);
            
        }
        // close db
        sqlite3_close(db);
        
    } // if(!(FileUtils::getInstance()->isFileExist(fullpath.c_str())))
    
    
}

void HelloWorld::updateUserDataDb() // BattleSceneにも同じのがある。ここを更新したらバトルシーンの方も更新する
{
    if(coin > COINCOUNTSTOP){
        coin = COINCOUNTSTOP;
    }
    if(mahouseki > MAHOUSEKICOUNTSTOP){
        mahouseki = MAHOUSEKICOUNTSTOP;
    }
    sqlite3* db;
    std::string fullpath;
    
    db = NULL;
    fullpath = FileUtils::getInstance()->getWritablePath();
    fullpath += "userdata.db";
    
    if (sqlite3_open(fullpath.c_str(), &db) == SQLITE_OK) {
        char tmpSql[2048];
        sprintf(tmpSql, "update user_data set name = '%s', stamina = %d, stamina_max = %d, last_stamina_decrease_time = %ld, battle_stamina = %d, battle_stamina_max = %d, last_battle_stamina_decrease_time = %ld, coin = %d, mahouseki = %d, last_mahouseki_got_time = %ld, visible_dungeon_number  = %d, visited_dungeon_number = %d, jinkei = %d, battle_rank = %d, battle_count = %d, battle_win_count = %d, battle_count_at_current_rank = %d, battle_win_count_at_current_rank = %d, mahouseki_purchase = %d, last_twitter_post_time = %ld where _id = 1", userName.c_str(), stamina, staminaMax, lastStaminaDecreaseTime, battleStamina, battleStaminaMax, lastBattleStaminaDecreaseTime, coin, mahouseki, lastMahousekiGotTime, visibleDangeonNumber, visitedDangeonNumber, jinkei, battleRank, battleCount, battleWinCount, battleCountAtCurrentRank, battleWinCountAtCurrentRank, mahouseki_purchase, lastTwitterPostTime);
        std::string sql = tmpSql;
        //    CCLOG("%s", sql.c_str());
        sqlite3_exec(db, sql.c_str(), NULL, NULL, NULL);
        
        
    }
    sqlite3_close(db);
}

bool HelloWorld::expendMahouseki(int num) // BattleSceneにも同じのがある。ここを更新したらバトルシーンの方も更新する
{
    if(mahouseki_purchase + mahouseki < num){
        return false;
    }
    
    if(mahouseki_purchase > num){
        mahouseki_purchase -= num;
    }
    else{
        int tmp_mahouseki = mahouseki_purchase;
        mahouseki_purchase = 0;
        
        mahouseki -= (num - tmp_mahouseki);
    }
    
    updateUserDataDb();
    return true;
}

////////////////////////////////////////////
////////////////////////////////////////////
////////////////////////////////////////////
////////////////////////////////////////////
////////////////////////////////////////////
////////////////////////////////////////////
////////////////////////////////////////////

void HelloWorld::confirmRiyoukiyaku()
{
    this->removeAllChildren();
    this->stopAllActions();
    playSE(0);
    
    Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image->setContentSize(Size(w , h));
    Point point = image->getAnchorPoint();
    image->setPosition(Point::ZERO);
    Size tmpSize = image->getContentSize();
    
    MenuItemSprite* menuItem;
    menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::blank, this));
    menuItem->setContentSize(tmpSize);
    menuItem->setPosition(Point::ZERO);
    
    Menu* menu = Menu::create(menuItem, NULL);
    menu->setContentSize(tmpSize);
    menu->setPosition(Point(w * 1.5, h * 0.5));
    
    auto label = LabelTTF::create(LocalizedString::getLocalizedString("riyoukiyakuConfirm1", "riyoukiyakuConfirm1"), FONT, LEVEL_FONT_SIZE, Size(tmpSize.width*0.9, tmpSize.height), TextHAlignment::CENTER, TextVAlignment::CENTER);
    label->setFontFillColor(Color3B(255, 255, 255));
    label->setAnchorPoint(Point(0.5, 0.5));
    label->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.7));
    image->addChild(label);
    
    this->addChild(menu, 100, 100);
    
    auto move = MoveBy::create(0.25, Point(-w, 0));
    menu->runAction(move);
    
    // 規約を読むボタン
    Scale9Sprite* image2 = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image2->setContentSize(Size(w * 0.7 , w * 0.18));
    image2->setPosition(Point::ZERO);
    Size tmpSize2 = image2->getContentSize();
    
    MenuItemSprite* menuItem2;
    menuItem2 = MenuItemSprite::create(image2, image2, CC_CALLBACK_0(HelloWorld::setMenu604_riyoukiyaku, this));
    menuItem2->setContentSize(tmpSize2);
    menuItem2->setPosition(Point::ZERO);
    
    Menu* menu2 = Menu::create(menuItem2, NULL);
    menu2->setContentSize(tmpSize2);
    menu2->setPosition(Point(w * 0.5, h * 0.5));
    
    auto label2 = LabelTTF::create(LocalizedString::getLocalizedString("riyoukiyakuConfirm2", "riyoukiyakuConfirm2"), FONT, LEVEL_FONT_SIZE, Size(tmpSize.width*0.9, tmpSize.height), TextHAlignment::CENTER, TextVAlignment::CENTER);
    label2->setFontFillColor(Color3B(255, 255, 255));
    label2->setAnchorPoint(Point(0.5, 0.5));
    label2->setPosition(Point(tmpSize2.width * 0.5, tmpSize2.height * 0.5));
    image2->addChild(label2);
    image->addChild(menu2, 101, 101);
    
    // 同意して始めるボタン
    Scale9Sprite* image3 = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image3->setContentSize(Size(w * 0.7 , w * 0.18));
    image3->setPosition(Point::ZERO);
    Size tmpSize3 = image3->getContentSize();
    
    MenuItemSprite* menuItem3;
    menuItem3 = MenuItemSprite::create(image3, image3, CC_CALLBACK_0(HelloWorld::confirmRiyoukiyakuOK, this));
    menuItem3->setContentSize(tmpSize3);
    menuItem3->setPosition(Point::ZERO);
    
    Menu* menu3 = Menu::create(menuItem3, NULL);
    menu3->setContentSize(tmpSize3);
    menu3->setPosition(Point(w * 0.5, h * 0.25));
    
    auto label3 = LabelTTF::create(LocalizedString::getLocalizedString("riyoukiyakuConfirm3", "riyoukiyakuConfirm3"), FONT, LEVEL_FONT_SIZE, Size(tmpSize.width*0.9, tmpSize.height), TextHAlignment::CENTER, TextVAlignment::CENTER);
    label3->setFontFillColor(Color3B(255, 255, 255));
    label3->setAnchorPoint(Point(0.5, 0.5));
    label3->setPosition(Point(tmpSize3.width * 0.5, tmpSize3.height * 0.5));
    image3->addChild(label3);
    image->addChild(menu3, 101, 101);
    
}

void HelloWorld::confirmRiyoukiyakuOK()
{
    playSE(0);
    
    this->removeChildByTag(100);
    
    Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image->setContentSize(Size(w , h));
    Point point = image->getAnchorPoint();
    image->setPosition(Point::ZERO);
    Size tmpSize = image->getContentSize();
    
    MenuItemSprite* menuItem;
    menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::blank, this));
    menuItem->setContentSize(tmpSize);
    menuItem->setPosition(Point::ZERO);
    
    Menu* menu = Menu::create(menuItem, NULL);
    menu->setContentSize(tmpSize);
    menu->setPosition(Point(w * 1.5, h * 0.5));
    
    auto label = LabelTTF::create(LocalizedString::getLocalizedString("riyoukiyakuConfirm4", "riyoukiyakuConfirm4"), FONT, LEVEL_FONT_SIZE, Size(tmpSize.width*0.9, tmpSize.height), TextHAlignment::CENTER, TextVAlignment::CENTER);
    label->setFontFillColor(Color3B(255, 255, 255));
    label->setAnchorPoint(Point(0.5, 0.5));
    label->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.5));
    image->addChild(label);
    
    this->addChild(menu, 100, 100);
    
    auto move = MoveBy::create(0.25, Point(-w, 0));
    menu->runAction(move);
    
    
    auto delay = DelayTime::create(0.5);
    auto func = CallFunc::create(CC_CALLBACK_0(HelloWorld::confirmRiyoukiyakuOK2, this));
    this->runAction(Sequence::create(delay, func, NULL));

}

void HelloWorld::confirmRiyoukiyakuOK2(){
    // 初期データベースを生成
    makeUserDb();
    
    auto scene = HelloWorld::createScene();
    Director::getInstance()->replaceScene(scene);
    
}

////////////////////////////////////////////
////////////////////////////////////////////
////////////////////////////////////////////
////////////////////////////////////////////
////////////////////////////////////////////
////////////////////////////////////////////

void HelloWorld::forceVersionUP()
{
    this->unscheduleAllSelectors();
    this->removeAllChildren();
    
    Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image->setContentSize(Size(w , h));
    Point point = image->getAnchorPoint();
    image->setPosition(Point::ZERO);
    Size tmpSize = image->getContentSize();
    
    MenuItemSprite* menuItem;
    menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::blank, this));
    menuItem->setContentSize(tmpSize);
    menuItem->setPosition(Point::ZERO);
    
    Menu* menu = Menu::create(menuItem, NULL);
    menu->setContentSize(tmpSize);
    menu->setPosition(Point(w * 1.5, h * 0.5));
    
    auto label = LabelTTF::create(LocalizedString::getLocalizedString("forceVersionUP", "forceVersionUP"), FONT, LEVEL_FONT_SIZE, Size(tmpSize.width*0.9, tmpSize.height), TextHAlignment::CENTER, TextVAlignment::CENTER);
    label->setFontFillColor(Color3B(255, 255, 255));
    label->setAnchorPoint(Point(0.5, 0.5));
    label->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.7));
    image->addChild(label);
    
    this->addChild(menu, 100, 100);
    
    auto move = MoveBy::create(0.25, Point(-w, 0));
    menu->runAction(move);
    
    
    // 同意して始めるボタン
    Scale9Sprite* image3 = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image3->setContentSize(Size(w * 0.7 , w * 0.18));
    image3->setPosition(Point::ZERO);
    Size tmpSize3 = image3->getContentSize();
    
    MenuItemSprite* menuItem3;
    menuItem3 = MenuItemSprite::create(image3, image3, CC_CALLBACK_0(HelloWorld::forceVersionUPOK, this));
    menuItem3->setContentSize(tmpSize3);
    menuItem3->setPosition(Point::ZERO);
    
    Menu* menu3 = Menu::create(menuItem3, NULL);
    menu3->setContentSize(tmpSize3);
    menu3->setPosition(Point(w * 0.5, h * 0.25));
    
    auto label3 = LabelTTF::create(LocalizedString::getLocalizedString("forceVersionUPOK", "forceVersionUPOK"), FONT, LEVEL_FONT_SIZE, Size(tmpSize.width*0.9, tmpSize.height), TextHAlignment::CENTER, TextVAlignment::CENTER);
    label3->setFontFillColor(Color3B(255, 255, 255));
    label3->setAnchorPoint(Point(0.5, 0.5));
    label3->setPosition(Point(tmpSize3.width * 0.5, tmpSize3.height * 0.5));
    image3->addChild(label3);
    image->addChild(menu3, 101, 101);
    

}

void HelloWorld::forceVersionUPOK()
{
    playSE(0);
    NativeManager::displayUrlByBrowser(STOREURL);
}

////////////////////////////////////////////
////////////////////////////////////////////
////////////////////////////////////////////
////////////////////////////////////////////
////////////////////////////////////////////
////////////////////////////////////////////

void HelloWorld::onSuccess(std::string itemId)
{
    CCLOG("Payment Successed: %s", itemId.c_str());
    
    for(int i = 0; i < MAHOUSEKITYPESDEBUG; i++){
        if(strcmp(itemId.c_str(), inappPurchaseItems[i].c_str()) == 0){
            mahouseki_purchase += inappPurchaseMahouseki[i];
            updateTopMenu();
            updateUserDataDb();
            
            break;
        }
    }
    
    onPurchaseEnded(true, itemId);
}


void HelloWorld::onCancel()
{
    CCLOG("Payment Canceled");
    std::string tmp = "";

    onPurchaseEnded(false, tmp);
}

void HelloWorld::onPurchaseEnded(bool purchaseResult, std::string itemId)
{
    auto delay = DelayTime::create(1.0);
    auto func = CallFunc::create(CC_CALLBACK_0(HelloWorld::setMenu320_purchased, this, purchaseResult, itemId));
    
    this->runAction(Sequence::create(delay, func, NULL));
    tapProhibit = false;
    
}

// プロダクト情報を取得するだけ
void HelloWorld::checkStoreProducts()
{
    CCLOG("checkStoreProducts");
    tapProhibit = true;
    storeProductChecking = true;
    storeProductCheckedNum = 0;
    
    // ストアのプロダクトをチェック中表示
    Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image->setContentSize(Size(w * 0.9 , w * 0.18 * 3));
    Point point = image->getAnchorPoint();
    image->setPosition(Point::ZERO);
    Size tmpSize = image->getContentSize();
    image->setPosition(Point(w * 1.5, h * 0.7 - w * 0.18));
    
    auto label = LabelTTF::create(LocalizedString::getLocalizedString("checkingstoreproducts", "checking store products"), FONT, LEVEL_FONT_SIZE, Size(tmpSize.width*0.6, tmpSize.height), TextHAlignment::CENTER, TextVAlignment::CENTER);
    label->setFontFillColor(Color3B(255, 255, 255));
    label->setAnchorPoint(Point(0.5, 0.5));
    label->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.5));
    image->addChild(label);

    this->addChild(image, 100, 100);
    
    auto move = MoveBy::create(0.25, Point(-w, 0));
    image->runAction(move);
    
    // タイムアウト処理
    this->schedule(schedule_selector(HelloWorld::checkStoreProductsFailed), MAHOUSEKICHECKTIMEOUT);
    
    InappPurchaseManager::setDelegate((InappPurchaseDelegate *)this);
    InappPurchaseManager::getStoreProduct(inappPurchaseItems[0]);
    InappPurchaseManager::getStoreProduct(inappPurchaseItems[1]);
    InappPurchaseManager::getStoreProduct(inappPurchaseItems[2]);
    InappPurchaseManager::getStoreProduct(inappPurchaseItems[3]);
    InappPurchaseManager::getStoreProduct(inappPurchaseItems[4]);
}

// プロダクト情報を取得したコールバック。アイテムの数ぶん来る。
void HelloWorld::onGotStoreProduct(std::string itemId, std::string productDescription, std::string productPrice)
{
    CCLOG("onGotStoreProduct");
    char tmpChar[128];
    sprintf(tmpChar, "%s, %s, %s", itemId.c_str(), productDescription.c_str(), productPrice.c_str());
    CCLOG("%s", tmpChar);
    
    sqlite3* db = NULL;
    std::string fullpath = FileUtils::getInstance()->getWritablePath();
    fullpath += "userdata.db";
    
    if (sqlite3_open(fullpath.c_str(), &db) == SQLITE_OK) {
        sqlite3_stmt *stmt = NULL;
        
        char tmpSql[256];
        sprintf(tmpSql, "select count(*) from store_products where product_id = '%s'", itemId.c_str());
        CCLOG("%s",tmpSql);
        
        if(sqlite3_prepare_v2(db, tmpSql, -1, &stmt, NULL) == SQLITE_OK){
            if(sqlite3_step(stmt) == SQLITE_ROW) {
                int count = (int)sqlite3_column_int(stmt, 0);
                
                if(count == 0){
                    CCLOG("count == 0, INSERT");
                    sprintf(tmpSql, "insert into store_products (_id,lastchecktime,product_id,description,price) values(NULL,%ld,'%s','%s','%s')", time(NULL), itemId.c_str(), productDescription.c_str(), productPrice.c_str());
                    sqlite3_exec(db, tmpSql, NULL, NULL, NULL);
                    
                }
                else{
                    CCLOG("count == %d, UPDATE", count);
                    sprintf(tmpSql, "update store_products set lastchecktime = %ld, description = '%s', price = '%s' where product_id = %s", time(NULL), itemId.c_str(), productDescription.c_str(), productPrice.c_str());
                    sqlite3_exec(db, tmpSql, NULL, NULL, NULL);
                    
                }
                CCLOG("%s", tmpSql);
                
            }
        }
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);
    }
    sqlite3_close(db);
    
    storeProductCheckedNum++;
    
    if((storeProductCheckedNum == MAHOUSEKITYPESDEBUG) && storeProductChecking && tapProhibit){
        storeProductChecking = false;
        tapProhibit = false;
        
        // アンスケジュール
        this->unschedule(schedule_selector(HelloWorld::checkStoreProductsFailed));
        
        // チェック中表示を消す
        this->removeChildByTag(100);
        
        setMenu301();
    }
}

void HelloWorld::checkStoreProductsFailed(float dt)
{
    if(storeProductChecking && tapProhibit){
        tapProhibit = false;
        
        // チェック中表示を消す
        this->removeChildByTag(100);
        
        // ストアのプロダクトをチェック中表示
        Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
        image->setContentSize(Size(w * 0.9 , w * 0.18 * 3));
        Point point = image->getAnchorPoint();
        image->setPosition(Point::ZERO);
        Size tmpSize = image->getContentSize();
        image->setPosition(Point(w * 1.5, h * 0.7 - w * 0.18));
        
        auto label = LabelTTF::create(LocalizedString::getLocalizedString("checkstoreproductsfailed", "store products check failed"), FONT, LEVEL_FONT_SIZE, Size(tmpSize.width*0.6, tmpSize.height), TextHAlignment::CENTER, TextVAlignment::CENTER);
        label->setFontFillColor(Color3B(255, 255, 255));
        label->setAnchorPoint(Point(0.5, 0.5));
        label->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.5));
        image->addChild(label);
        
        this->addChild(image, 100, 100);
        
        auto move = MoveBy::create(0.25, Point(-w, 0));
        image->runAction(move);

    }
    
    storeProductChecking = false;
}


////////////////////////////////////////////
////////////////////////////////////////////
////////////////////////////////////////////
////////////////////////////////////////////
////////////////////////////////////////////

void HelloWorld::searchOtherParties(){
    CCLOG("searchOtherParties()");
    
    int partyMemberUnitId[5];
    int queryData[10][13];
    
    int skill_list[64][2];
    for(int i = 0; i < 64; i++){
        for(int j = 0; j < 2; j++){
            skill_list[i][j] = 0;
        }
    }
    
    sqlite3* db = NULL;
    std::string fullpath = FileUtils::getInstance()->getWritablePath();
    fullpath += "userdata.db";
    
    if (sqlite3_open(fullpath.c_str(), &db) == SQLITE_OK) {
        sqlite3_stmt *stmt = NULL;
        
        // スキルを抽出
        // edit start
        std::string tmpsql = "select * from user_skill_list";
        if(sqlite3_prepare_v2(db, tmpsql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
            int count = 0;
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int _id             = (int)sqlite3_column_int(stmt, 0);
                int skill_id        = (int)sqlite3_column_int(stmt, 1);
                int level           = (int)sqlite3_column_int(stmt, 2);
                int soubi_unit_id   = (int)sqlite3_column_int(stmt, 3);
                skill_list[count][0] = skill_id;
                skill_list[count][1] = soubi_unit_id;
                count++;
            }
        }
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);
        stmt = NULL;
        // edit end
        
        int count = 0;
        std::string sql = "select * from user_unit inner join user_party on user_unit._id = user_party.unit_id order by user_party.unit_order asc";
        if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int user_unit_id     = (int)sqlite3_column_int(stmt, 0);
                queryData[count][0]  = (int)sqlite3_column_int(stmt, 1); // type
                queryData[count][1]  = (int)sqlite3_column_int(stmt, 2); // level
                queryData[count][2]  = (int)sqlite3_column_int(stmt, 3); // class_level
                queryData[count][3]  = (int)sqlite3_column_int(stmt, 4); // hp
                queryData[count][4]  = (int)sqlite3_column_int(stmt, 5); // physical_atk
                queryData[count][6]  = (int)sqlite3_column_int(stmt, 6); // magical_atk
                queryData[count][5]  = (int)sqlite3_column_int(stmt, 7); // physical_def
                queryData[count][7]  = (int)sqlite3_column_int(stmt, 8); // magical_def
                //queryData[count][8]  = (int)sqlite3_column_int(stmt, 9); // skill1
                //queryData[count][9]  = 1; // skill1_lv
                queryData[count][10] = (int)sqlite3_column_int(stmt,10); // skill2
                queryData[count][11] = 1; // skill2_lv
                int class_skill      = (int)sqlite3_column_int(stmt,11);
                queryData[count][12] = (int)sqlite3_column_int(stmt,12); // zokusei
                int unit_order       = (int)sqlite3_column_int(stmt,14);
                
                // 装備スキル取得
                sqlite3_stmt *stmt3 = NULL;
                char tmpString[128];
                sprintf(tmpString, "select * from user_skill_list where soubi_unit_id = %d", user_unit_id);
                if(sqlite3_prepare_v2(db, tmpString, -1, &stmt3, NULL) == SQLITE_OK){
                    sqlite3_step(stmt3);
                    queryData[count][8] = (int)sqlite3_column_int(stmt3, 1);
                    queryData[count][9] = (int)sqlite3_column_int(stmt3, 2);
                }
                sqlite3_reset(stmt3);
                sqlite3_finalize(stmt3);

                count++;
            }
        }
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);
        
        
        sql = "select * from user_unit inner join user_party2 on user_unit._id = user_party2.unit_id order by user_party2.unit_order asc";
        if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int user_unit_id     = (int)sqlite3_column_int(stmt, 0);
                queryData[count][0]  = (int)sqlite3_column_int(stmt, 1); // type
                queryData[count][1]  = (int)sqlite3_column_int(stmt, 2); // level
                queryData[count][2]  = (int)sqlite3_column_int(stmt, 3); // class_level
                queryData[count][3]  = (int)sqlite3_column_int(stmt, 4); // hp
                queryData[count][4]  = (int)sqlite3_column_int(stmt, 5); // physical_atk
                queryData[count][6]  = (int)sqlite3_column_int(stmt, 6); // magical_atk
                queryData[count][5]  = (int)sqlite3_column_int(stmt, 7); // physical_def
                queryData[count][7]  = (int)sqlite3_column_int(stmt, 8); // magical_def
                //queryData[count][8]  = (int)sqlite3_column_int(stmt, 9); // skill1
                //queryData[count][9]  = 1; // skill1_lv
                queryData[count][10] = (int)sqlite3_column_int(stmt,10); // skill2
                queryData[count][11] = 1; // skill2_lv
                int class_skill      = (int)sqlite3_column_int(stmt,11);
                queryData[count][12] = (int)sqlite3_column_int(stmt,12); // zokusei
                int unit_order       = (int)sqlite3_column_int(stmt,14);
                
                // 装備スキル取得
                sqlite3_stmt *stmt3 = NULL;
                char tmpString[128];
                sprintf(tmpString, "select * from user_skill_list where soubi_unit_id = %d", user_unit_id);
                if(sqlite3_prepare_v2(db, tmpString, -1, &stmt3, NULL) == SQLITE_OK){
                    sqlite3_step(stmt3);
                    queryData[count][8] = (int)sqlite3_column_int(stmt3, 1);
                    queryData[count][9] = (int)sqlite3_column_int(stmt3, 2);
                }
                sqlite3_reset(stmt3);
                sqlite3_finalize(stmt3);
                
                count++;
            }
        }
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);

    }
    sqlite3_close(db);
    
    char postData[4096]; //
    
    sprintf(postData, "_id=&name=%s&rank=%d&c1_type=%d&c1_level=%d&c1_class_level=%d&c1_hp=%d&c1_physical_atk=%d&c1_physical_def=%d&c1_magical_atk=%d&c1_magical_def=%d&c1_skill1=%d&c1_skill1_lv=%d&c1_skill2=%d&c1_skill2_lv=%d&c1_zokusei=%d&c2_type=%d&c2_level=%d&c2_class_level=%d&c2_hp=%d&c2_physical_atk=%d&c2_physical_def=%d&c2_magical_atk=%d&c2_magical_def=%d&c2_skill1=%d&c2_skill1_lv=%d&c2_skill2=%d&c2_skill2_lv=%d&c2_zokusei=%d&c3_type=%d&c3_level=%d&c3_class_level=%d&c3_hp=%d&c3_physical_atk=%d&c3_physical_def=%d&c3_magical_atk=%d&c3_magical_def=%d&c3_skill1=%d&c3_skill1_lv=%d&c3_skill2=%d&c3_skill2_lv=%d&c3_zokusei=%d&c4_type=%d&c4_level=%d&c4_class_level=%d&c4_hp=%d&c4_physical_atk=%d&c4_physical_def=%d&c4_magical_atk=%d&c4_magical_def=%d&c4_skill1=%d&c4_skill1_lv=%d&c4_skill2=%d&c4_skill2_lv=%d&c4_zokusei=%d&c5_type=%d&c5_level=%d&c5_class_level=%d&c5_hp=%d&c5_physical_atk=%d&c5_physical_def=%d&c5_magical_atk=%d&c5_magical_def=%d&c5_skill1=%d&c5_skill1_lv=%d&c5_skill2=%d&c5_skill2_lv=%d&c5_zokusei=%d&c6_type=%d&c6_level=%d&c6_class_level=%d&c6_hp=%d&c6_physical_atk=%d&c6_physical_def=%d&c6_magical_atk=%d&c6_magical_def=%d&c6_skill1=%d&c6_skill1_lv=%d&c6_skill2=%d&c6_skill2_lv=%d&c6_zokusei=%d&c7_type=%d&c7_level=%d&c7_class_level=%d&c7_hp=%d&c7_physical_atk=%d&c7_physical_def=%d&c7_magical_atk=%d&c7_magical_def=%d&c7_skill1=%d&c7_skill1_lv=%d&c7_skill2=%d&c7_skill2_lv=%d&c7_zokusei=%d&c8_type=%d&c8_level=%d&c8_class_level=%d&c8_hp=%d&c8_physical_atk=%d&c8_physical_def=%d&c8_magical_atk=%d&c8_magical_def=%d&c8_skill1=%d&c8_skill1_lv=%d&c8_skill2=%d&c8_skill2_lv=%d&c8_zokusei=%d&c9_type=%d&c9_level=%d&c9_class_level=%d&c9_hp=%d&c9_physical_atk=%d&c9_physical_def=%d&c9_magical_atk=%d&c9_magical_def=%d&c9_skill1=%d&c9_skill1_lv=%d&c9_skill2=%d&c9_skill2_lv=%d&c9_zokusei=%d&c10_type=%d&c10_level=%d&c10_class_level=%d&c10_hp=%d&c10_physical_atk=%d&c10_physical_def=%d&c10_magical_atk=%d&c10_magical_def=%d&c10_skill1=%d&c10_skill1_lv=%d&c10_skill2=%d&c10_skill2_lv=%d&c10_zokusei=%d",
            userName.c_str(), // name
            battleRank, // rank
            queryData[0][0], // type
            queryData[0][1], // level
            queryData[0][2], // class_level
            queryData[0][3], // hp
            queryData[0][4], // physical_atk
            queryData[0][5], // physical_def
            queryData[0][6], // magical_atk
            queryData[0][7], // magical_def
            queryData[0][8], // skill1
            queryData[0][9], // skill1_lv
            queryData[0][10], // skill2
            queryData[0][11], // skill2_lv
            queryData[0][12], // zokusei
            queryData[1][0], // type
            queryData[1][1], // level
            queryData[1][2], // class_level
            queryData[1][3], // hp
            queryData[1][4], // physical_atk
            queryData[1][5], // physical_def
            queryData[1][6], // magical_atk
            queryData[1][7], // magical_def
            queryData[1][8], // skill1
            queryData[1][9], // skill1_lv
            queryData[1][10], // skill2
            queryData[1][11], // skill2_lv
            queryData[1][12], // zokusei
            queryData[2][0], // type
            queryData[2][1], // level
            queryData[2][2], // class_level
            queryData[2][3], // hp
            queryData[2][4], // physical_atk
            queryData[2][5], // physical_def
            queryData[2][6], // magical_atk
            queryData[2][7], // magical_def
            queryData[2][8], // skill1
            queryData[2][9], // skill1_lv
            queryData[2][10], // skill2
            queryData[2][11], // skill2_lv
            queryData[2][12], // zokusei
            queryData[3][0], // type
            queryData[3][1], // level
            queryData[3][2], // class_level
            queryData[3][3], // hp
            queryData[3][4], // physical_atk
            queryData[3][5], // physical_def
            queryData[3][6], // magical_atk
            queryData[3][7], // magical_def
            queryData[3][8], // skill1
            queryData[3][9], // skill1_lv
            queryData[3][10], // skill2
            queryData[3][11], // skill2_lv
            queryData[3][12], // zokusei
            queryData[4][0], // type
            queryData[4][1], // level
            queryData[4][2], // class_level
            queryData[4][3], // hp
            queryData[4][4], // physical_atk
            queryData[4][5], // physical_def
            queryData[4][6], // magical_atk
            queryData[4][7], // magical_def
            queryData[4][8], // skill1
            queryData[4][9], // skill1_lv
            queryData[4][10], // skill2
            queryData[4][11], // skill2_lv
            queryData[4][12], // zokusei
            queryData[5][0], // type
            queryData[5][1], // level
            queryData[5][2], // class_level
            queryData[5][3], // hp
            queryData[5][4], // physical_atk
            queryData[5][5], // physical_def
            queryData[5][6], // magical_atk
            queryData[5][7], // magical_def
            queryData[5][8], // skill1
            queryData[5][9], // skill1_lv
            queryData[5][10], // skill2
            queryData[5][11], // skill2_lv
            queryData[5][12], // zokusei
            queryData[6][0], // type
            queryData[6][1], // level
            queryData[6][2], // class_level
            queryData[6][3], // hp
            queryData[6][4], // physical_atk
            queryData[6][5], // physical_def
            queryData[6][6], // magical_atk
            queryData[6][7], // magical_def
            queryData[6][8], // skill1
            queryData[6][9], // skill1_lv
            queryData[6][10], // skill2
            queryData[6][11], // skill2_lv
            queryData[6][12], // zokusei
            queryData[7][0], // type
            queryData[7][1], // level
            queryData[7][2], // class_level
            queryData[7][3], // hp
            queryData[7][4], // physical_atk
            queryData[7][5], // physical_def
            queryData[7][6], // magical_atk
            queryData[7][7], // magical_def
            queryData[7][8], // skill1
            queryData[7][9], // skill1_lv
            queryData[7][10], // skill2
            queryData[7][11], // skill2_lv
            queryData[7][12], // zokusei
            queryData[8][0], // type
            queryData[8][1], // level
            queryData[8][2], // class_level
            queryData[8][3], // hp
            queryData[8][4], // physical_atk
            queryData[8][5], // physical_def
            queryData[8][6], // magical_atk
            queryData[8][7], // magical_def
            queryData[8][8], // skill1
            queryData[8][9], // skill1_lv
            queryData[8][10], // skill2
            queryData[8][11], // skill2_lv
            queryData[8][12], // zokusei
            queryData[9][0], // type
            queryData[9][1], // level
            queryData[9][2], // class_level
            queryData[9][3], // hp
            queryData[9][4], // physical_atk
            queryData[9][5], // physical_def
            queryData[9][6], // magical_atk
            queryData[9][7], // magical_def
            queryData[9][8], // skill1
            queryData[9][9], // skill1_lv
            queryData[9][10], // skill2
            queryData[9][11], // skill2_lv
            queryData[9][12] // zokusei
            );
    
    //CCLOG("%ld:%s", strlen(postData), postData);
    
    auto request = new HttpRequest();
    request->setUrl("http://1min-quest.com/form.php");
    request->setRequestType(HttpRequest::Type::POST);
    request->setResponseCallback(this, httpresponse_selector(HelloWorld::searchOtherPartiesCallback));
    request->setRequestData(postData, strlen(postData));
    request->setTag("POST partyData");
    HttpClient::getInstance()->send(request);
    request->release();
}

void HelloWorld::searchOtherPartiesCallback(HttpClient* sender, HttpResponse* response){
    
    CCLOG("searchOtherPartiesCallback()");
    
    picojson::value response_data;
    std::string err;
    
    if(!(response->isSucceed())){ tapProhibit = false; searchingOtherOpponent = false; return; }// 圏外対策
    
    CCASSERT(response, "response empty");
    CCASSERT(response->isSucceed(), "response failed");
    
    //Json取得
    std::vector<char>* buffer = response->getResponseData();
    
    // dump data
    //for (unsigned int i = 0; i < buffer->size(); i++) printf("%c", (*buffer)[i]); printf("\n");
    
    char * json = (char *) malloc(buffer->size() + 1);
    std::string s2(buffer->begin(), buffer->end());
    strcpy(json, s2.c_str());
    picojson::parse(response_data, json, json + strlen(json), &err);
    
    CCASSERT(err.empty(), err.c_str());
    picojson::object& all_data = response_data.get<picojson::object>();
    auto version = Value(all_data["version"].get<double>());
    picojson::object& p1 = all_data["p1"].get<picojson::object>();
    picojson::object& p2 = all_data["p2"].get<picojson::object>();
    
//    auto j_name = Value(p1["name"].get<std::string>());
//    CCLOG("json name : %s", j_name.asString().c_str());
//    auto j_id = Value(p1["_id"].get<double>());
//    CCLOG("json _id : %d", j_id.asInt());

    auto p1__id = Value(p1["_id"].get<double>());
    auto p1_name = Value(p1["name"].get<std::string>());
    auto p1_rank = Value(p1["rank"].get<double>());
    auto p1_c1_type = Value(p1["c1_type"].get<double>());
    auto p1_c1_level = Value(p1["c1_level"].get<double>());
    auto p1_c1_class_level = Value(p1["c1_class_level"].get<double>());
    auto p1_c1_hp = Value(p1["c1_hp"].get<double>());
    auto p1_c1_physical_atk = Value(p1["c1_physical_atk"].get<double>());
    auto p1_c1_physical_def = Value(p1["c1_physical_def"].get<double>());
    auto p1_c1_magical_atk = Value(p1["c1_magical_atk"].get<double>());
    auto p1_c1_magical_def = Value(p1["c1_magical_def"].get<double>());
    auto p1_c1_skill1 = Value(p1["c1_skill1"].get<double>());
    auto p1_c1_skill1_lv = Value(p1["c1_skill1_lv"].get<double>());
    auto p1_c1_skill2 = Value(p1["c1_skill2"].get<double>());
    auto p1_c1_skill2_lv = Value(p1["c1_skill2_lv"].get<double>());
    auto p1_c1_zokusei = Value(p1["c1_zokusei"].get<double>());
    auto p1_c2_type = Value(p1["c2_type"].get<double>());
    auto p1_c2_level = Value(p1["c2_level"].get<double>());
    auto p1_c2_class_level = Value(p1["c2_class_level"].get<double>());
    auto p1_c2_hp = Value(p1["c2_hp"].get<double>());
    auto p1_c2_physical_atk = Value(p1["c2_physical_atk"].get<double>());
    auto p1_c2_physical_def = Value(p1["c2_physical_def"].get<double>());
    auto p1_c2_magical_atk = Value(p1["c2_magical_atk"].get<double>());
    auto p1_c2_magical_def = Value(p1["c2_magical_def"].get<double>());
    auto p1_c2_skill1 = Value(p1["c2_skill1"].get<double>());
    auto p1_c2_skill1_lv = Value(p1["c2_skill1_lv"].get<double>());
    auto p1_c2_skill2 = Value(p1["c2_skill2"].get<double>());
    auto p1_c2_skill2_lv = Value(p1["c2_skill2_lv"].get<double>());
    auto p1_c2_zokusei = Value(p1["c2_zokusei"].get<double>());
    auto p1_c3_type = Value(p1["c3_type"].get<double>());
    auto p1_c3_level = Value(p1["c3_level"].get<double>());
    auto p1_c3_class_level = Value(p1["c3_class_level"].get<double>());
    auto p1_c3_hp = Value(p1["c3_hp"].get<double>());
    auto p1_c3_physical_atk = Value(p1["c3_physical_atk"].get<double>());
    auto p1_c3_physical_def = Value(p1["c3_physical_def"].get<double>());
    auto p1_c3_magical_atk = Value(p1["c3_magical_atk"].get<double>());
    auto p1_c3_magical_def = Value(p1["c3_magical_def"].get<double>());
    auto p1_c3_skill1 = Value(p1["c3_skill1"].get<double>());
    auto p1_c3_skill1_lv = Value(p1["c3_skill1_lv"].get<double>());
    auto p1_c3_skill2 = Value(p1["c3_skill2"].get<double>());
    auto p1_c3_skill2_lv = Value(p1["c3_skill2_lv"].get<double>());
    auto p1_c3_zokusei = Value(p1["c3_zokusei"].get<double>());
    auto p1_c4_type = Value(p1["c4_type"].get<double>());
    auto p1_c4_level = Value(p1["c4_level"].get<double>());
    auto p1_c4_class_level = Value(p1["c4_class_level"].get<double>());
    auto p1_c4_hp = Value(p1["c4_hp"].get<double>());
    auto p1_c4_physical_atk = Value(p1["c4_physical_atk"].get<double>());
    auto p1_c4_physical_def = Value(p1["c4_physical_def"].get<double>());
    auto p1_c4_magical_atk = Value(p1["c4_magical_atk"].get<double>());
    auto p1_c4_magical_def = Value(p1["c4_magical_def"].get<double>());
    auto p1_c4_skill1 = Value(p1["c4_skill1"].get<double>());
    auto p1_c4_skill1_lv = Value(p1["c4_skill1_lv"].get<double>());
    auto p1_c4_skill2 = Value(p1["c4_skill2"].get<double>());
    auto p1_c4_skill2_lv = Value(p1["c4_skill2_lv"].get<double>());
    auto p1_c4_zokusei = Value(p1["c4_zokusei"].get<double>());
    auto p1_c5_type = Value(p1["c5_type"].get<double>());
    auto p1_c5_level = Value(p1["c5_level"].get<double>());
    auto p1_c5_class_level = Value(p1["c5_class_level"].get<double>());
    auto p1_c5_hp = Value(p1["c5_hp"].get<double>());
    auto p1_c5_physical_atk = Value(p1["c5_physical_atk"].get<double>());
    auto p1_c5_physical_def = Value(p1["c5_physical_def"].get<double>());
    auto p1_c5_magical_atk = Value(p1["c5_magical_atk"].get<double>());
    auto p1_c5_magical_def = Value(p1["c5_magical_def"].get<double>());
    auto p1_c5_skill1 = Value(p1["c5_skill1"].get<double>());
    auto p1_c5_skill1_lv = Value(p1["c5_skill1_lv"].get<double>());
    auto p1_c5_skill2 = Value(p1["c5_skill2"].get<double>());
    auto p1_c5_skill2_lv = Value(p1["c5_skill2_lv"].get<double>());
    auto p1_c5_zokusei = Value(p1["c5_zokusei"].get<double>());
    
    auto p2__id = Value(p2["_id"].get<double>());
    auto p2_name = Value(p2["name"].get<std::string>());
    auto p2_rank = Value(p2["rank"].get<double>());
    auto p2_c1_type = Value(p2["c1_type"].get<double>());
    auto p2_c1_level = Value(p2["c1_level"].get<double>());
    auto p2_c1_class_level = Value(p2["c1_class_level"].get<double>());
    auto p2_c1_hp = Value(p2["c1_hp"].get<double>());
    auto p2_c1_physical_atk = Value(p2["c1_physical_atk"].get<double>());
    auto p2_c1_physical_def = Value(p2["c1_physical_def"].get<double>());
    auto p2_c1_magical_atk = Value(p2["c1_magical_atk"].get<double>());
    auto p2_c1_magical_def = Value(p2["c1_magical_def"].get<double>());
    auto p2_c1_skill1 = Value(p2["c1_skill1"].get<double>());
    auto p2_c1_skill1_lv = Value(p2["c1_skill1_lv"].get<double>());
    auto p2_c1_skill2 = Value(p2["c1_skill2"].get<double>());
    auto p2_c1_skill2_lv = Value(p2["c1_skill2_lv"].get<double>());
    auto p2_c1_zokusei = Value(p2["c1_zokusei"].get<double>());
    auto p2_c2_type = Value(p2["c2_type"].get<double>());
    auto p2_c2_level = Value(p2["c2_level"].get<double>());
    auto p2_c2_class_level = Value(p2["c2_class_level"].get<double>());
    auto p2_c2_hp = Value(p2["c2_hp"].get<double>());
    auto p2_c2_physical_atk = Value(p2["c2_physical_atk"].get<double>());
    auto p2_c2_physical_def = Value(p2["c2_physical_def"].get<double>());
    auto p2_c2_magical_atk = Value(p2["c2_magical_atk"].get<double>());
    auto p2_c2_magical_def = Value(p2["c2_magical_def"].get<double>());
    auto p2_c2_skill1 = Value(p2["c2_skill1"].get<double>());
    auto p2_c2_skill1_lv = Value(p2["c2_skill1_lv"].get<double>());
    auto p2_c2_skill2 = Value(p2["c2_skill2"].get<double>());
    auto p2_c2_skill2_lv = Value(p2["c2_skill2_lv"].get<double>());
    auto p2_c2_zokusei = Value(p2["c2_zokusei"].get<double>());
    auto p2_c3_type = Value(p2["c3_type"].get<double>());
    auto p2_c3_level = Value(p2["c3_level"].get<double>());
    auto p2_c3_class_level = Value(p2["c3_class_level"].get<double>());
    auto p2_c3_hp = Value(p2["c3_hp"].get<double>());
    auto p2_c3_physical_atk = Value(p2["c3_physical_atk"].get<double>());
    auto p2_c3_physical_def = Value(p2["c3_physical_def"].get<double>());
    auto p2_c3_magical_atk = Value(p2["c3_magical_atk"].get<double>());
    auto p2_c3_magical_def = Value(p2["c3_magical_def"].get<double>());
    auto p2_c3_skill1 = Value(p2["c3_skill1"].get<double>());
    auto p2_c3_skill1_lv = Value(p2["c3_skill1_lv"].get<double>());
    auto p2_c3_skill2 = Value(p2["c3_skill2"].get<double>());
    auto p2_c3_skill2_lv = Value(p2["c3_skill2_lv"].get<double>());
    auto p2_c3_zokusei = Value(p2["c3_zokusei"].get<double>());
    auto p2_c4_type = Value(p2["c4_type"].get<double>());
    auto p2_c4_level = Value(p2["c4_level"].get<double>());
    auto p2_c4_class_level = Value(p2["c4_class_level"].get<double>());
    auto p2_c4_hp = Value(p2["c4_hp"].get<double>());
    auto p2_c4_physical_atk = Value(p2["c4_physical_atk"].get<double>());
    auto p2_c4_physical_def = Value(p2["c4_physical_def"].get<double>());
    auto p2_c4_magical_atk = Value(p2["c4_magical_atk"].get<double>());
    auto p2_c4_magical_def = Value(p2["c4_magical_def"].get<double>());
    auto p2_c4_skill1 = Value(p2["c4_skill1"].get<double>());
    auto p2_c4_skill1_lv = Value(p2["c4_skill1_lv"].get<double>());
    auto p2_c4_skill2 = Value(p2["c4_skill2"].get<double>());
    auto p2_c4_skill2_lv = Value(p2["c4_skill2_lv"].get<double>());
    auto p2_c4_zokusei = Value(p2["c4_zokusei"].get<double>());
    auto p2_c5_type = Value(p2["c5_type"].get<double>());
    auto p2_c5_level = Value(p2["c5_level"].get<double>());
    auto p2_c5_class_level = Value(p2["c5_class_level"].get<double>());
    auto p2_c5_hp = Value(p2["c5_hp"].get<double>());
    auto p2_c5_physical_atk = Value(p2["c5_physical_atk"].get<double>());
    auto p2_c5_physical_def = Value(p2["c5_physical_def"].get<double>());
    auto p2_c5_magical_atk = Value(p2["c5_magical_atk"].get<double>());
    auto p2_c5_magical_def = Value(p2["c5_magical_def"].get<double>());
    auto p2_c5_skill1 = Value(p2["c5_skill1"].get<double>());
    auto p2_c5_skill1_lv = Value(p2["c5_skill1_lv"].get<double>());
    auto p2_c5_skill2 = Value(p2["c5_skill2"].get<double>());
    auto p2_c5_skill2_lv = Value(p2["c5_skill2_lv"].get<double>());
    auto p2_c5_zokusei = Value(p2["c5_zokusei"].get<double>());
    
    sqlite3* db;
    std::string fullpath;
    
    db = NULL;
    fullpath = FileUtils::getInstance()->getWritablePath();
    fullpath += "userdata.db";
    
    if (sqlite3_open(fullpath.c_str(), &db) == SQLITE_OK) {
        
        // insert か update かチェック
        bool insertOrUpdate = false;
        sqlite3_stmt *stmt = NULL;
        std::string sql = "select * from other_users_party";
        if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, NULL) == SQLITE_OK){
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                insertOrUpdate = true;
            }
        }
        sqlite3_reset(stmt);
        sqlite3_finalize(stmt);

        
        // INSERT
        if(!insertOrUpdate){
        
            char tmpChar[4096];
            
            CCLOG("insert into other_users_party");
            
            sprintf(tmpChar, "INSERT INTO other_users_party (_id, name, rank, c1_type, c1_level, c1_class_level, c1_hp, c1_physical_atk, c1_physical_def, c1_magical_atk, c1_magical_def, c1_skill1, c1_skill1_lv, c1_skill2, c1_skill2_lv, c1_zokusei, c2_type, c2_level, c2_class_level, c2_hp, c2_physical_atk, c2_physical_def, c2_magical_atk, c2_magical_def, c2_skill1, c2_skill1_lv, c2_skill2, c2_skill2_lv, c2_zokusei, c3_type, c3_level, c3_class_level, c3_hp, c3_physical_atk, c3_physical_def, c3_magical_atk, c3_magical_def, c3_skill1, c3_skill1_lv, c3_skill2, c3_skill2_lv, c3_zokusei, c4_type, c4_level, c4_class_level, c4_hp, c4_physical_atk, c4_physical_def, c4_magical_atk, c4_magical_def, c4_skill1, c4_skill1_lv, c4_skill2, c4_skill2_lv, c4_zokusei, c5_type, c5_level, c5_class_level, c5_hp, c5_physical_atk, c5_physical_def, c5_magical_atk, c5_magical_def, c5_skill1, c5_skill1_lv, c5_skill2, c5_skill2_lv, c5_zokusei) VALUES ( NULL, '%s', %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d )", p1_name.asString().c_str(), p1_rank.asInt(), p1_c1_type.asInt(), p1_c1_level.asInt(), p1_c1_class_level.asInt(), p1_c1_hp.asInt(), p1_c1_physical_atk.asInt(), p1_c1_physical_def.asInt(), p1_c1_magical_atk.asInt(), p1_c1_magical_def.asInt(), p1_c1_skill1.asInt(), p1_c1_skill1_lv.asInt(), p1_c1_skill2.asInt(), p1_c1_skill2_lv.asInt(), p1_c1_zokusei.asInt(), p1_c2_type.asInt(), p1_c2_level.asInt(), p1_c2_class_level.asInt(), p1_c2_hp.asInt(), p1_c2_physical_atk.asInt(), p1_c2_physical_def.asInt(), p1_c2_magical_atk.asInt(), p1_c2_magical_def.asInt(), p1_c2_skill1.asInt(), p1_c2_skill1_lv.asInt(), p1_c2_skill2.asInt(), p1_c2_skill2_lv.asInt(), p1_c2_zokusei.asInt(), p1_c3_type.asInt(), p1_c3_level.asInt(), p1_c3_class_level.asInt(), p1_c3_hp.asInt(), p1_c3_physical_atk.asInt(), p1_c3_physical_def.asInt(), p1_c3_magical_atk.asInt(), p1_c3_magical_def.asInt(), p1_c3_skill1.asInt(), p1_c3_skill1_lv.asInt(), p1_c3_skill2.asInt(), p1_c3_skill2_lv.asInt(), p1_c3_zokusei.asInt(), p1_c4_type.asInt(), p1_c4_level.asInt(), p1_c4_class_level.asInt(), p1_c4_hp.asInt(), p1_c4_physical_atk.asInt(), p1_c4_physical_def.asInt(), p1_c4_magical_atk.asInt(), p1_c4_magical_def.asInt(), p1_c4_skill1.asInt(), p1_c4_skill1_lv.asInt(), p1_c4_skill2.asInt(), p1_c4_skill2_lv.asInt(), p1_c4_zokusei.asInt(), p1_c5_type.asInt(), p1_c5_level.asInt(), p1_c5_class_level.asInt(), p1_c5_hp.asInt(), p1_c5_physical_atk.asInt(), p1_c5_physical_def.asInt(), p1_c5_magical_atk.asInt(), p1_c5_magical_def.asInt(), p1_c5_skill1.asInt(), p1_c5_skill1_lv.asInt(), p1_c5_skill2.asInt(), p1_c5_skill2_lv.asInt(), p1_c5_zokusei.asInt() );
            
            CCLOG("%s", tmpChar);
            sqlite3_exec(db, tmpChar, NULL, NULL, NULL);
            
            sprintf(tmpChar, "INSERT INTO other_users_party (_id, name, rank, c1_type, c1_level, c1_class_level, c1_hp, c1_physical_atk, c1_physical_def, c1_magical_atk, c1_magical_def, c1_skill1, c1_skill1_lv, c1_skill2, c1_skill2_lv, c1_zokusei, c2_type, c2_level, c2_class_level, c2_hp, c2_physical_atk, c2_physical_def, c2_magical_atk, c2_magical_def, c2_skill1, c2_skill1_lv, c2_skill2, c2_skill2_lv, c2_zokusei, c3_type, c3_level, c3_class_level, c3_hp, c3_physical_atk, c3_physical_def, c3_magical_atk, c3_magical_def, c3_skill1, c3_skill1_lv, c3_skill2, c3_skill2_lv, c3_zokusei, c4_type, c4_level, c4_class_level, c4_hp, c4_physical_atk, c4_physical_def, c4_magical_atk, c4_magical_def, c4_skill1, c4_skill1_lv, c4_skill2, c4_skill2_lv, c4_zokusei, c5_type, c5_level, c5_class_level, c5_hp, c5_physical_atk, c5_physical_def, c5_magical_atk, c5_magical_def, c5_skill1, c5_skill1_lv, c5_skill2, c5_skill2_lv, c5_zokusei) VALUES ( NULL, '%s', %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d )", p2_name.asString().c_str(), p2_rank.asInt(), p2_c1_type.asInt(), p2_c1_level.asInt(), p2_c1_class_level.asInt(), p2_c1_hp.asInt(), p2_c1_physical_atk.asInt(), p2_c1_physical_def.asInt(), p2_c1_magical_atk.asInt(), p2_c1_magical_def.asInt(), p2_c1_skill1.asInt(), p2_c1_skill1_lv.asInt(), p2_c1_skill2.asInt(), p2_c1_skill2_lv.asInt(), p2_c1_zokusei.asInt(), p2_c2_type.asInt(), p2_c2_level.asInt(), p2_c2_class_level.asInt(), p2_c2_hp.asInt(), p2_c2_physical_atk.asInt(), p2_c2_physical_def.asInt(), p2_c2_magical_atk.asInt(), p2_c2_magical_def.asInt(), p2_c2_skill1.asInt(), p2_c2_skill1_lv.asInt(), p2_c2_skill2.asInt(), p2_c2_skill2_lv.asInt(), p2_c2_zokusei.asInt(), p2_c3_type.asInt(), p2_c3_level.asInt(), p2_c3_class_level.asInt(), p2_c3_hp.asInt(), p2_c3_physical_atk.asInt(), p2_c3_physical_def.asInt(), p2_c3_magical_atk.asInt(), p2_c3_magical_def.asInt(), p2_c3_skill1.asInt(), p2_c3_skill1_lv.asInt(), p2_c3_skill2.asInt(), p2_c3_skill2_lv.asInt(), p2_c3_zokusei.asInt(), p2_c4_type.asInt(), p2_c4_level.asInt(), p2_c4_class_level.asInt(), p2_c4_hp.asInt(), p2_c4_physical_atk.asInt(), p2_c4_physical_def.asInt(), p2_c4_magical_atk.asInt(), p2_c4_magical_def.asInt(), p2_c4_skill1.asInt(), p2_c4_skill1_lv.asInt(), p2_c4_skill2.asInt(), p2_c4_skill2_lv.asInt(), p2_c4_zokusei.asInt(), p2_c5_type.asInt(), p2_c5_level.asInt(), p2_c5_class_level.asInt(), p2_c5_hp.asInt(), p2_c5_physical_atk.asInt(), p2_c5_physical_def.asInt(), p2_c5_magical_atk.asInt(), p2_c5_magical_def.asInt(), p2_c5_skill1.asInt(), p2_c5_skill1_lv.asInt(), p2_c5_skill2.asInt(), p2_c5_skill2_lv.asInt(), p2_c5_zokusei.asInt() );
            
            CCLOG("%s", tmpChar);
            sqlite3_exec(db, tmpChar, NULL, NULL, NULL);
        }
        // UPDATE
        else{
            
            char tmpChar[4096];
            
            CCLOG("update other_users_party");
            
            sprintf(tmpChar, "UPDATE other_users_party set name = '%s', rank = %d, c1_type = %d, c1_level = %d, c1_class_level = %d, c1_hp = %d, c1_physical_atk = %d, c1_physical_def = %d, c1_magical_atk = %d, c1_magical_def = %d, c1_skill1 = %d, c1_skill1_lv = %d, c1_skill2 = %d, c1_skill2_lv = %d, c1_zokusei = %d, c2_type = %d, c2_level = %d, c2_class_level = %d, c2_hp = %d, c2_physical_atk = %d, c2_physical_def = %d, c2_magical_atk = %d, c2_magical_def = %d, c2_skill1 = %d, c2_skill1_lv = %d, c2_skill2 = %d, c2_skill2_lv = %d, c2_zokusei = %d, c3_type = %d, c3_level = %d, c3_class_level = %d, c3_hp = %d, c3_physical_atk = %d, c3_physical_def = %d, c3_magical_atk = %d, c3_magical_def = %d, c3_skill1 = %d, c3_skill1_lv = %d, c3_skill2 = %d, c3_skill2_lv = %d, c3_zokusei = %d, c4_type = %d, c4_level = %d, c4_class_level = %d, c4_hp = %d, c4_physical_atk = %d, c4_physical_def = %d, c4_magical_atk = %d, c4_magical_def = %d, c4_skill1 = %d, c4_skill1_lv = %d, c4_skill2 = %d, c4_skill2_lv = %d, c4_zokusei = %d, c5_type = %d, c5_level = %d, c5_class_level = %d, c5_hp = %d, c5_physical_atk = %d, c5_physical_def = %d, c5_magical_atk = %d, c5_magical_def = %d, c5_skill1 = %d, c5_skill1_lv = %d, c5_skill2 = %d, c5_skill2_lv = %d, c5_zokusei = %d where _id = 1", p1_name.asString().c_str(), p1_rank.asInt(), p1_c1_type.asInt(), p1_c1_level.asInt(), p1_c1_class_level.asInt(), p1_c1_hp.asInt(), p1_c1_physical_atk.asInt(), p1_c1_physical_def.asInt(), p1_c1_magical_atk.asInt(), p1_c1_magical_def.asInt(), p1_c1_skill1.asInt(), p1_c1_skill1_lv.asInt(), p1_c1_skill2.asInt(), p1_c1_skill2_lv.asInt(), p1_c1_zokusei.asInt(), p1_c2_type.asInt(), p1_c2_level.asInt(), p1_c2_class_level.asInt(), p1_c2_hp.asInt(), p1_c2_physical_atk.asInt(), p1_c2_physical_def.asInt(), p1_c2_magical_atk.asInt(), p1_c2_magical_def.asInt(), p1_c2_skill1.asInt(), p1_c2_skill1_lv.asInt(), p1_c2_skill2.asInt(), p1_c2_skill2_lv.asInt(), p1_c2_zokusei.asInt(), p1_c3_type.asInt(), p1_c3_level.asInt(), p1_c3_class_level.asInt(), p1_c3_hp.asInt(), p1_c3_physical_atk.asInt(), p1_c3_physical_def.asInt(), p1_c3_magical_atk.asInt(), p1_c3_magical_def.asInt(), p1_c3_skill1.asInt(), p1_c3_skill1_lv.asInt(), p1_c3_skill2.asInt(), p1_c3_skill2_lv.asInt(), p1_c3_zokusei.asInt(), p1_c4_type.asInt(), p1_c4_level.asInt(), p1_c4_class_level.asInt(), p1_c4_hp.asInt(), p1_c4_physical_atk.asInt(), p1_c4_physical_def.asInt(), p1_c4_magical_atk.asInt(), p1_c4_magical_def.asInt(), p1_c4_skill1.asInt(), p1_c4_skill1_lv.asInt(), p1_c4_skill2.asInt(), p1_c4_skill2_lv.asInt(), p1_c4_zokusei.asInt(), p1_c5_type.asInt(), p1_c5_level.asInt(), p1_c5_class_level.asInt(), p1_c5_hp.asInt(), p1_c5_physical_atk.asInt(), p1_c5_physical_def.asInt(), p1_c5_magical_atk.asInt(), p1_c5_magical_def.asInt(), p1_c5_skill1.asInt(), p1_c5_skill1_lv.asInt(), p1_c5_skill2.asInt(), p1_c5_skill2_lv.asInt(), p1_c5_zokusei.asInt() );
            
            CCLOG("%s", tmpChar);
            sqlite3_exec(db, tmpChar, NULL, NULL, NULL);
            
            sprintf(tmpChar, "UPDATE other_users_party set name = '%s', rank = %d, c1_type = %d, c1_level = %d, c1_class_level = %d, c1_hp = %d, c1_physical_atk = %d, c1_physical_def = %d, c1_magical_atk = %d, c1_magical_def = %d, c1_skill1 = %d, c1_skill1_lv = %d, c1_skill2 = %d, c1_skill2_lv = %d, c1_zokusei = %d, c2_type = %d, c2_level = %d, c2_class_level = %d, c2_hp = %d, c2_physical_atk = %d, c2_physical_def = %d, c2_magical_atk = %d, c2_magical_def = %d, c2_skill1 = %d, c2_skill1_lv = %d, c2_skill2 = %d, c2_skill2_lv = %d, c2_zokusei = %d, c3_type = %d, c3_level = %d, c3_class_level = %d, c3_hp = %d, c3_physical_atk = %d, c3_physical_def = %d, c3_magical_atk = %d, c3_magical_def = %d, c3_skill1 = %d, c3_skill1_lv = %d, c3_skill2 = %d, c3_skill2_lv = %d, c3_zokusei = %d, c4_type = %d, c4_level = %d, c4_class_level = %d, c4_hp = %d, c4_physical_atk = %d, c4_physical_def = %d, c4_magical_atk = %d, c4_magical_def = %d, c4_skill1 = %d, c4_skill1_lv = %d, c4_skill2 = %d, c4_skill2_lv = %d, c4_zokusei = %d, c5_type = %d, c5_level = %d, c5_class_level = %d, c5_hp = %d, c5_physical_atk = %d, c5_physical_def = %d, c5_magical_atk = %d, c5_magical_def = %d, c5_skill1 = %d, c5_skill1_lv = %d, c5_skill2 = %d, c5_skill2_lv = %d, c5_zokusei = %d where _id = 2", p2_name.asString().c_str(), p2_rank.asInt(), p2_c1_type.asInt(), p2_c1_level.asInt(), p2_c1_class_level.asInt(), p2_c1_hp.asInt(), p2_c1_physical_atk.asInt(), p2_c1_physical_def.asInt(), p2_c1_magical_atk.asInt(), p2_c1_magical_def.asInt(), p2_c1_skill1.asInt(), p2_c1_skill1_lv.asInt(), p2_c1_skill2.asInt(), p2_c1_skill2_lv.asInt(), p2_c1_zokusei.asInt(), p2_c2_type.asInt(), p2_c2_level.asInt(), p2_c2_class_level.asInt(), p2_c2_hp.asInt(), p2_c2_physical_atk.asInt(), p2_c2_physical_def.asInt(), p2_c2_magical_atk.asInt(), p2_c2_magical_def.asInt(), p2_c2_skill1.asInt(), p2_c2_skill1_lv.asInt(), p2_c2_skill2.asInt(), p2_c2_skill2_lv.asInt(), p2_c2_zokusei.asInt(), p2_c3_type.asInt(), p2_c3_level.asInt(), p2_c3_class_level.asInt(), p2_c3_hp.asInt(), p2_c3_physical_atk.asInt(), p2_c3_physical_def.asInt(), p2_c3_magical_atk.asInt(), p2_c3_magical_def.asInt(), p2_c3_skill1.asInt(), p2_c3_skill1_lv.asInt(), p2_c3_skill2.asInt(), p2_c3_skill2_lv.asInt(), p2_c3_zokusei.asInt(), p2_c4_type.asInt(), p2_c4_level.asInt(), p2_c4_class_level.asInt(), p2_c4_hp.asInt(), p2_c4_physical_atk.asInt(), p2_c4_physical_def.asInt(), p2_c4_magical_atk.asInt(), p2_c4_magical_def.asInt(), p2_c4_skill1.asInt(), p2_c4_skill1_lv.asInt(), p2_c4_skill2.asInt(), p2_c4_skill2_lv.asInt(), p2_c4_zokusei.asInt(), p2_c5_type.asInt(), p2_c5_level.asInt(), p2_c5_class_level.asInt(), p2_c5_hp.asInt(), p2_c5_physical_atk.asInt(), p2_c5_physical_def.asInt(), p2_c5_magical_atk.asInt(), p2_c5_magical_def.asInt(), p2_c5_skill1.asInt(), p2_c5_skill1_lv.asInt(), p2_c5_skill2.asInt(), p2_c5_skill2_lv.asInt(), p2_c5_zokusei.asInt() );
            
            CCLOG("%s", tmpChar);
            sqlite3_exec(db, tmpChar, NULL, NULL, NULL);
        }
        
    }
    sqlite3_close(db);
    
    // 強制アップデート
    CCLOG("force update check , server : %f, local : %f", version.asDouble(), VERSION);
    if(version.asDouble() > VERSION){
        forceVersionUP();
        return;
    }
    
    // アリーナで別の相手を捜している場合、アリーナに飛ばす
    if(searchingOtherOpponent == true){
        tapProhibit = false;
        searchingOtherOpponent = false;
        globalMenu(500);
    }
    
}


void HelloWorld::playSE(int _id)
{
    
    if(playingSE){ return; }
    
    playingSE = true;

    if(!playingSE1){ playingSE1 = true; playingSE = false; return; } // 立ち上げ直後にEnter音がなるのの防止。
    
    switch (_id) {
        case 0:
            CocosDenshion::SimpleAudioEngine::getInstance()->playEffect("se/se_enter.mp3");
            break;
        case 1:
            CocosDenshion::SimpleAudioEngine::getInstance()->playEffect("se/se_ng.mp3");
            break;
        case 2:
            CocosDenshion::SimpleAudioEngine::getInstance()->playEffect("se/se_levelup.mp3");
            break;
        case 3:
            CocosDenshion::SimpleAudioEngine::getInstance()->playEffect("se/se_continue.mp3");
            break;
        case 4:
            CocosDenshion::SimpleAudioEngine::getInstance()->playEffect("se/se_coin.mp3");
            break;
            
        default:
            CocosDenshion::SimpleAudioEngine::getInstance()->playEffect("se/se_enter.mp3");
            break;
    }
    
    this->schedule(schedule_selector(HelloWorld::playSE_timerReset), 0.5);
    
}


void HelloWorld::playSE_timerReset(float dt){
    playingSE = false;
}


void HelloWorld::gotoBattleScene()
{
    SceneManager* sceneManager = SceneManager::getInstance();
    int tmpDangeonId = sceneManager->dangeonId;

    CCLOG("tmpDangeonId : %d, battleCount: %d", tmpDangeonId, battleCount);
    
    if(tmpDangeonId == 1 ||
       tmpDangeonId == 2 ||
       tmpDangeonId == 3 ||
       tmpDangeonId == 4 ||
       (tmpDangeonId == -1 && battleCount == 1)
        ){
        auto scene = TipsScene::createScene();
        Director::getInstance()->replaceScene(scene);
    }
    else{
        auto scene = BattleScene::createScene();
        Director::getInstance()->replaceScene(scene);
    }
}

void HelloWorld::displayOpening()
{
    Sprite* bg = Sprite::create("worldmap.jpg");
    bg->setScale( h / bg->getContentSize().height );
    bg->setAnchorPoint(Point(0.5, 0.5));
    bg->setPosition(Point(w * 0.5, h * 0.5));
    this->addChild(bg, 1, 1);
    
    // skipボタン
    Scale9Sprite* image = Scale9Sprite::create("window.png", Rect(0, 0, 180, 180), Rect(40, 40, 100, 100));
    image->setContentSize(Size(w * 0.3 , w * 0.15));
    image->setPosition(Point::ZERO);
    Size tmpSize = image->getContentSize();
    
    MenuItemSprite* menuItem;
    menuItem = MenuItemSprite::create(image, image, CC_CALLBACK_0(HelloWorld::confirmRiyoukiyaku, this));
    menuItem->setContentSize(tmpSize);
    menuItem->setPosition(Point::ZERO);
    
    Menu* menu = Menu::create(menuItem, NULL);
    menu->setContentSize(tmpSize);
    menu->setPosition(Point(w * 1.8, h * 0.1));
    
    auto label = LabelTTF::create(LocalizedString::getLocalizedString("skip", "skip"), FONT, LEVEL_FONT_SIZE, Size(tmpSize.width*0.9, tmpSize.height), TextHAlignment::CENTER, TextVAlignment::CENTER);
    label->setFontFillColor(Color3B(255, 255, 255));
    label->setAnchorPoint(Point(0.5, 0.5));
    label->setPosition(Point(tmpSize.width * 0.5, tmpSize.height * 0.5));
    image->addChild(label);
    
    this->addChild(menu, 100, 100);
    
    auto move = MoveBy::create(0.25, Point(-w, 0));
    menu->runAction(move);
    
    // BGM再生
    CocosDenshion::SimpleAudioEngine::getInstance()->setBackgroundMusicVolume(0.5);
    CocosDenshion::SimpleAudioEngine::getInstance()->setEffectsVolume(0.5);
    CocosDenshion::SimpleAudioEngine::getInstance()->stopBackgroundMusic();
    CocosDenshion::SimpleAudioEngine::getInstance()->playBackgroundMusic("bgm/bgm_opening.mp3", true);
    
    char tmpChar[32];
    for(int i = 1; i <= OPENINGLINES ; i++){
        sprintf(tmpChar, "opening%d", i);
        auto label = LabelTTF::create(LocalizedString::getLocalizedString(tmpChar, tmpChar), FONT, LEVEL_FONT_SIZE, Size(w*0.9, NULL), TextHAlignment::CENTER, TextVAlignment::TOP);
        label->setFontFillColor(Color3B(255, 255, 255));
        label->setAnchorPoint(Point(0.5, 0.5));
        label->enableStroke(Color3B(0, 0, 0), 3);
        label->setPosition(Point(w * 0.5, h * 0.25));
        this->addChild(label, 10+i, 10+i);
        
        auto delay0 = DelayTime::create((i-1) * 6.0);
        
        auto hide = FadeOut::create(0);
        auto fadein1 = FadeIn::create(3);
        auto move1 = MoveBy::create(3, Point(0, h*0.05));
        auto move2 = MoveBy::create(24, Point(0, h*0.4));
        auto fadeout3 = FadeOut::create(3);
        auto move3 = MoveBy::create(3, Point(0, h*0.05));
        
        label->runAction(Sequence::create(hide ,delay0, Spawn::create(fadein1, move1, NULL), move2, Spawn::create(fadeout3, move3, NULL), NULL));

    }
    auto delay = DelayTime::create(60);
    auto func = CallFunc::create(CC_CALLBACK_0(HelloWorld::confirmRiyoukiyaku, this));
    this->runAction(Sequence::create(delay, func, NULL));

}

void HelloWorld::blank(){ CCLOG("blank()"); }

