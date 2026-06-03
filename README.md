# iBeacon Scanner

BLE / iBeacon デバイスを検出し、UUID / Major / Minor / RSSI / 推定距離を表示するスキャナーアプリです。

ESPresense の MQTT トピックを購読して、デバイスの部屋移動履歴を保存・検索することもできます。現在は Windows での利用を主対象にしつつ、Android 版も展開できる構成を想定しています。

## 主な機能

- BLE デバイスのスキャン
- iBeacon パケットの解析
- UUID、Major、Minor、RSSI、推定距離の表示
- 最終検出からの経過時間表示
- TX Power 補正オフセットによる距離推定の調整
- スキャン間隔の調整
- ESPresense MQTT ブローカーへの接続
- spresense/devices/# の購読
- Home Assistant / Bermuda のAreaセンサー読み取り
- 部屋情報の履歴保存と検索
- 動作ログの表示

## 画面構成

- `デバイス`: 検出した BLE / iBeacon デバイス一覧を表示します。
- `履歴`: ESPresense / Bermuda から受信した部屋移動履歴を検索します。
- `ログ`: Bluetooth スキャンと MQTT の動作ログを表示します。
- `設定`: TX Power 補正、MQTT 接続、Home Assistant / Bermuda 連携、スキャン間隔を設定します。

## 使い方

1. Bluetooth アダプター、または Android 端末の Bluetooth を有効にします。
2. アプリを起動します。
3. Android の場合は、必要な Bluetooth / 位置情報権限を許可します。
4. `スキャン開始` を押します。
5. 検出されたデバイスが `デバイス` タブに表示されます。
6. 必要に応じて `設定` タブでスキャン間隔や TX Power 補正を調整します。

## iBeacon の表示

iBeacon と判定されたデバイスは、カード上に以下の情報を表示します。

- デバイス名
- アドレス
- 最終検出からの経過時間
- UUID
- Major / Minor
- RSSI
- 推定距離

推定距離は RSSI と TX Power から計算しているため、環境やビーコン個体差で大きく変動します。距離が実際より遠い / 近い場合は、`設定` タブの `TX Power 補正オフセット` を調整してください。

## スキャン間隔について

設定可能なスキャン間隔は `5秒` から `30秒` です。USB Bluetooth アダプターや Android 端末によっては短すぎる間隔で不安定になることがあるため、問題が出る場合は 10 秒以上にしてください。

## ESPresense / MQTT 連携

`設定` タブで MQTT ブローカー情報を入力して接続できます。

- ホスト
- ポート
- ユーザー名
- パスワード
- 起動時の自動接続

接続後、アプリは次のトピックを購読します。

```text
espresense/devices/#
```

受信した JSON から以下を読み取ります。

- `name`
- `room`
- `distance`

`room` が空でない場合、部屋情報を履歴に保存します。同じデバイスが同じ部屋に連続している場合は、重複記録を避けます。

## Home Assistant / Bermuda 連携

`設定` タブで Home Assistant のURLと Long-Lived Access Token を入力すると、Bermuda が作成するAreaセンサーを定期的に読み取れます。

- Home Assistant URL
- Long-Lived Access Token
- 更新間隔
- 起動時の自動接続

Bermudaの `Area` センサーから現在の部屋を読み取り、部屋が変わったときに履歴へ保存します。現在は Home Assistant REST API の `/api/states` を定期ポーリングする方式です。

## 履歴と設定

履歴は SQLite に保存されます。保存先は Qt の `AppDataLocation` 配下です。Windows と Android で実際の保存先は異なります。
検索結果は最大 500 件です。

以下の設定は保存され、次回起動時に復元されます。

- MQTT ホスト
- MQTT ポート
- MQTT ユーザー名
- MQTT パスワード
- MQTT 自動接続
- TX Power 補正オフセット
- Home Assistant URL
- Home Assistant Long-Lived Access Token
- Home Assistant 自動接続
- Home Assistant 更新間隔

## 対応状況

### Windows

現在の主な動作対象です。Windows の Bluetooth / WinRT バックエンド経由で BLE スキャンします。

USB Bluetooth アダプターを使う場合、短い周期でスキャンを再起動するとアダプターが不安定になることがあります。このため、スキャンは一定時間ごとに自然終了を待ってから再開する方式にしています。

### Android

Android 版も対応予定です。Android では OS バージョンごとに BLE スキャンの権限や動作制限が異なるため、端末側の Bluetooth / 位置情報設定や省電力設定の影響を受ける場合があります。

## 既知の注意点

- Windows の Bluetooth / WinRT バックエンドでは、BLE スキャンの挙動がアダプターやドライバーに依存します。
- USB Bluetooth アダプターが反応しなくなる場合は、スキャン間隔を長めにしてください。
- Windows のプライバシー設定で Bluetooth アクセスが無効だと、BLE スキャンできない場合があります。
- Android では OS バージョンごとに必要な Bluetooth 権限が異なります。
- Android では端末やメーカーの省電力制御により、スキャン頻度やバックグラウンド動作が制限される場合があります。
- RSSI は揺れやすいため、推定距離は目安です。
- MQTT は必要最小限の MQTT 3.1.1 パケット処理を独自実装しています。高度な MQTT 機能は対象外です。

## 開発者向け

### 必要環境

- Qt 6.10 以降
- CMake 3.16 以降
- C++17 対応コンパイラ
- Qt モジュール: `Core`, `Gui`, `Quick`, `QuickControls2`, `Bluetooth`, `Network`, `Sql`

Windows で BLE スキャンを行う場合は、Bluetooth アダプターと Windows の Bluetooth / プライバシー設定が有効であることを確認してください。

Android で BLE スキャンを行う場合は、Android SDK / NDK / JDK と Qt Android キットが必要です。Android 固有の権限やアイコン、パッケージ設定を追加する場合は、プロジェクト直下に `android/` ディレクトリを用意し、`QT_ANDROID_PACKAGE_SOURCE_DIR` から参照されるようにします。

### ビルド

Qt Creator で開く場合は、このフォルダの `CMakeLists.txt` を開いて構成してください。

Windows コマンドライン例:

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:\Qt\6.11.1\msvc2022_64"
cmake --build build --config Release
```

Qt Creator 付属のキットを使う場合は、Qt Creator からビルドするのが一番確実です。MSVC を使う場合は、Visual Studio の開発者環境が有効になっている必要があります。

Android 版は Qt Creator の Android キットで構成してください。

### プロジェクト構成

```text
.
├── .gitattributes
├── .gitignore
├── CMakeLists.txt
├── LICENSE
├── README.md
├── qml
│   ├── AppTheme.qml
│   ├── BeaconListItem.qml
│   ├── HistoryTab.qml
│   └── Main.qml
├── resources
│   ├── app_icon.ico
│   ├── app_icon.png
│   ├── app_icon_*.png
│   └── app_icon.rc
└── src
    ├── BeaconInfo.h
    ├── BeaconScanner.cpp
    ├── BeaconScanner.h
    ├── LocationHistory.cpp
    ├── LocationHistory.h
    ├── MqttReceiver.cpp
    ├── MqttReceiver.h
    └── main.cpp
```

## ライセンス

MIT License です。詳細は [LICENSE](LICENSE) を参照してください。

