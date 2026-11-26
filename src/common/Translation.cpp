#include "Translation.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace catbrowser::translation {
namespace {

// Small, offline EN <-> JA dictionary focused on browser-game UI wording.
// It is intentionally compact: lookup happens only when the user asks for it.
constexpr std::pair<const char*, const char*> kEntries[] = {
    {"start game", "ゲームを始める"},
    {"game over", "ゲームオーバー"},
    {"high score", "ハイスコア"},
    {"how to play", "遊び方"},
    {"full screen", "全画面"},
    {"please wait", "お待ちください"},
    {"new game", "新しいゲーム"},
    {"leaderboard", "ランキング"},
    {"continue", "続ける"},
    {"settings", "設定"},
    {"options", "オプション"},
    {"restart", "リスタート"},
    {"resume", "再開"},
    {"pause", "一時停止"},
    {"score", "スコア"},
    {"level", "レベル"},
    {"stage", "ステージ"},
    {"lives", "ライフ"},
    {"health", "体力"},
    {"speed", "スピード"},
    {"timer", "タイマー"},
    {"time", "時間"},
    {"win", "勝利"},
    {"lose", "敗北"},
    {"menu", "メニュー"},
    {"play", "プレイ"},
    {"loading", "読み込み中"},
    {"sound", "サウンド"},
    {"music", "音楽"},
    {"volume", "音量"},
    {"fullscreen", "全画面"},
    {"exit", "終了"},
    {"quit", "終了"},
    {"back", "戻る"},
    {"next", "次へ"},
    {"previous", "前へ"},
    {"home", "ホーム"},
    {"help", "ヘルプ"},
    {"controls", "操作"},
    {"click", "クリック"},
    {"tap", "タップ"},
    {"drag", "ドラッグ"},
    {"press", "押す"},
    {"start", "スタート"},
    {"retry", "リトライ"},
    {"coins", "コイン"},
    {"coin", "コイン"},
    {"gold", "ゴールド"},
    {"points", "ポイント"},
    {"bonus", "ボーナス"},
    {"reward", "報酬"},
    {"shop", "ショップ"},
    {"store", "ストア"},
    {"buy", "購入"},
    {"upgrade", "アップグレード"},
    {"weapon", "武器"},
    {"armor", "防具"},
    {"character", "キャラクター"},
    {"player", "プレイヤー"},
    {"enemy", "敵"},
    {"boss", "ボス"},
    {"attack", "攻撃"},
    {"defense", "防御"},
    {"magic", "魔法"},
    {"skill", "スキル"},
    {"item", "アイテム"},
    {"inventory", "インベントリ"},
    {"map", "マップ"},
    {"world", "ワールド"},
    {"quest", "クエスト"},
    {"mission", "ミッション"},
    {"task", "タスク"},
    {"rank", "ランク"},
    {"ranking", "ランキング"},
    {"record", "記録"},
    {"best", "ベスト"},
    {"save", "保存"},
    {"load", "読み込み"},
    {"delete", "削除"},
    {"reset", "リセット"},
    {"clear", "クリア"},
    {"complete", "完了"},
    {"failed", "失敗"},
    {"error", "エラー"},
    {"warning", "警告"},
    {"yes", "はい"},
    {"no", "いいえ"},
    {"cancel", "キャンセル"},
    {"submit", "送信"},
    {"close", "閉じる"},
    {"open", "開く"},
    {"language", "言語"},
    {"english", "英語"},
    {"japanese", "日本語"},
    {"translate", "翻訳"},
    {"jump", "ジャンプ"},
    {"move", "移動"},
    {"shoot", "射撃"},
    {"collect", "集める"},
    {"avoid", "避ける"},
    {"target", "ターゲット"},
    {"goal", "ゴール"},
    {"finish", "完了"},
    {"round", "ラウンド"},
    {"wave", "ウェーブ"},
    {"difficulty", "難易度"},
    {"easy", "簡単"},
    {"normal", "普通"},
    {"hard", "難しい"},
    {"auto", "自動"},
    {"manual", "手動"},
    {"login", "ログイン"},
    {"logout", "ログアウト"},
    {"account", "アカウント"},
    {"profile", "プロフィール"},
    {"share", "共有"},
    {"copy", "コピー"},
    {"edit", "編集"},
    {"search", "検索"},
    {"filter", "フィルター"},
    {"sort", "並べ替え"},
    {"all", "すべて"},
    {"none", "なし"},
    {"default", "既定"},
};

std::string jsonEscape(std::string_view value) {
    std::string out;
    out.reserve(value.size() + 4);
    for (unsigned char c : value) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                char buffer[8];
                std::snprintf(buffer, sizeof(buffer), "\\u%04x", c);
                out += buffer;
            } else {
                out.push_back(static_cast<char>(c));
            }
        }
    }
    return out;
}

std::string buildPairs(Direction direction) {
    std::vector<std::pair<std::string, std::string>> pairs;
    pairs.reserve(std::size(kEntries));
    for (const auto& entry : kEntries) {
        if (direction == Direction::ToJapanese) {
            pairs.emplace_back(entry.first, entry.second);
        } else {
            pairs.emplace_back(entry.second, entry.first);
        }
    }

    // Longest keys first so "high score" wins over "score".
    std::sort(pairs.begin(), pairs.end(), [](const auto& a, const auto& b) {
        return a.first.size() > b.first.size();
    });

    std::vector<std::string> seen;
    std::string out = "[";
    bool first = true;
    for (const auto& pair : pairs) {
        if (std::find(seen.begin(), seen.end(), pair.first) != seen.end()) {
            continue;
        }
        seen.push_back(pair.first);
        if (!first) {
            out += ',';
        }
        out += "[\"" + jsonEscape(pair.first) + "\",\"" +
               jsonEscape(pair.second) + "\"]";
        first = false;
    }
    out += ']';
    return out;
}

constexpr const char* kTranslateScript = R"JS(
(function(){
try{
if(window.__catbrowserRestore){window.__catbrowserRestore();}
var pairs=PAIRS;
var escapeRegExp=function(s){return s.replace(/[.*+?^${}()|[\]\\]/g,'\\$&');};
var isAscii=/^[A-Za-z0-9 ]+$/;
var changed=[];
var walker=document.createTreeWalker(document.body,NodeFilter.SHOW_TEXT,null,false);
var node;
while(node=walker.nextNode()){
var text=node.nodeValue;
if(!text||!text.trim())continue;
var parent=node.parentElement;
if(parent&&parent.closest&&parent.closest('script,style,noscript,textarea,input,select,option'))continue;
var output=text;
for(var i=0;i<pairs.length;i++){
var key=pairs[i][0],value=pairs[i][1];
var pattern=isAscii.test(key)?'\\b'+escapeRegExp(key)+'\\b':escapeRegExp(key);
var flags=isAscii.test(key)?'gi':'g';
output=output.replace(new RegExp(pattern,flags),value);
}
if(output!==text){changed.push([node,text]);node.nodeValue=output;}
}
window.__catbrowserTranslation=changed;
window.__catbrowserRestore=function(){
var list=window.__catbrowserTranslation||[];
for(var i=0;i<list.length;i++){try{list[i][0].nodeValue=list[i][1];}catch(e){}}
window.__catbrowserTranslation=[];
};
}catch(e){}
})();
)JS";

} // namespace

std::string buildTranslateScript(Direction direction) {
    std::string script(kTranslateScript);
    const size_t pos = script.find("PAIRS");
    if (pos != std::string::npos) {
        script.replace(pos, 5, buildPairs(direction));
    }
    return script;
}

std::string buildRestoreScript() {
    return "(function(){if(window.__catbrowserRestore){window.__catbrowserRestore();}})();";
}

} // namespace catbrowser::translation
