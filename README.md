# iBeacon Scanner

BLE / iBeacon デバイスを検出し、UUID / Major / Minor / RSSI / 推定距離を表示する Qt/QML 製のスキャナーアプリです。

ESPresense の MQTT トピックや Home Assistant / Bermuda の Area センサーから部屋情報を取り込み、デバイスの部屋移動履歴を保存・検索することもできます。Windows と Android を主な動作対象にしています。

## 主な機能

- BLE デバイスのスキャン
- iBeacon パケットの解析
- UUID、Major、Minor、RSSI、推定距離の表示
- 最終検出からの経過時間表示
- 検出デバイスのソート
- UUID / MAC アドレスのコピー
- TX Power 補正オフセットによる距離推定の調整
- スキャン間隔の調整
- ESPresense MQTT ブローカーへの接続
- `espresense/devices/#` の購読
- Home Assistant / Bermuda の Area センサー読み取り
- Home Assistant からの過去履歴インポート
- 部屋情報の履歴保存と検索
- 動作ログの表示と詳細ログの切り替え

## 画面構成

- `デバイス`: 検出した BLE / iBeacon デバイス一覧を表示します。
- `履歴`: ESPresense / Bermuda から受信した部屋移動履歴を検索します。
- `ログ`: Bluetooth スキャン、MQTT、Home Assistant 連携の動作ログを表示します。
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

Windows では、短い周期で強制的な stop/start を繰り返すと USB Bluetooth アダプターが不安定になることがあります。このため、Qt Bluetooth の discovery timeout による自然終了後、短い休止を挟んで再開します。

Android では、Qt Bluetooth の `QBluetoothDeviceDiscoveryAgent` ではなく Android の `BluetoothLeScanner` を直接使います。Java 側でスキャン結果をキューに蓄積し、C++ 側が定期的に poll して取り出す方式です。JNI ネイティブ callback 登録に依存しないため、Android 実機での接続点を単純にしています。

## ESPresense / MQTT 連携

`設定` タブで MQTT ブローカーの URL を入力して接続できます。

- MQTT URL
- ユーザー名
- パスワード
- 起動時の自動接続

URL例:

```text
mqtt://192.168.1.x:1883
```

接続後、アプリは次のトピックを購読します。

```text
espresense/devices/#
```

受信した JSON から以下を読み取ります。

- `name`
- `room`
- `distance`

`room` が空でない場合、部屋情報を履歴に保存します。同じデバイスが同じ部屋に連続している場合は、重複記録を避けます。

MQTT は必要最小限の MQTT 3.1.1 パケット処理を独自実装しています。高度な MQTT 機能は対象外です。

## Home Assistant / Bermuda 連携

`設定` タブで Home Assistant の URL と Long-Lived Access Token を入力すると、Bermuda が作成する Area センサーを定期的に読み取れます。

- Home Assistant URL
- Long-Lived Access Token
- 更新間隔
- 起動時の自動接続
- 過去履歴インポート

Bermuda の `Area` センサーから現在の部屋を読み取り、部屋が変わったときに履歴へ保存します。現在は Home Assistant REST API の `/api/states` を定期ポーリングする方式です。

過去履歴インポートでは Home Assistant の履歴 API を参照し、指定期間の Area センサー履歴を SQLite に取り込みます。

## 履歴と設定

履歴は SQLite に保存されます。保存先は Qt の `AppDataLocation` 配下です。Windows と Android で実際の保存先は異なります。
検索結果は最大 500 件です。

以下の設定は保存され、次回起動時に復元されます。

- MQTT URL
- MQTT ユーザー名
- MQTT パスワード
- MQTT 自動接続
- TX Power 補正オフセット
- ソート設定
- Home Assistant URL
- Home Assistant Long-Lived Access Token
- Home Assistant 自動接続
- Home Assistant 更新間隔
- 詳細ログ設定

## 対応状況

### Windows

Windows の Bluetooth / WinRT バックエンド経由で BLE スキャンします。

USB Bluetooth アダプターを使う場合、短い周期でスキャンを再起動するとアダプターが不安定になることがあります。問題が出る場合は、スキャン間隔を長めにしてください。

### Android

Android 版は arm64-v8a APK を GitHub Actions でビルドしています。

Android 12 以降では `BLUETOOTH_SCAN` / `BLUETOOTH_CONNECT` などの Bluetooth 権限が必要です。また、このアプリは iBeacon の検出結果を位置推定に使えるため、`BLUETOOTH_SCAN` に `neverForLocation` を付けていません。`neverForLocation` を付けると Android OS が一部の BLE beacon をスキャン結果から除外するため、iBeacon が見つからない原因になります。

実機で iBeacon が表示されない場合は、次を確認してください。

- 端末の Bluetooth が有効になっていること
- アプリに `付近のデバイス` 権限が許可されていること
- アプリに `位置情報` 権限が許可されていること
- 端末側の位置情報サービスが有効になっていること
- 省電力設定でアプリや Bluetooth スキャンが強く制限されていないこと
- 既存インストールから更新した場合は、必要に応じてアンインストール後に再インストールすること

## 既知の注意点

- Windows の Bluetooth / WinRT バックエンドでは、BLE スキャンの挙動がアダプターやドライバーに依存します。
- USB Bluetooth アダプターが反応しなくなる場合は、スキャン間隔を長めにしてください。
- Windows のプライバシー設定で Bluetooth アクセスが無効だと、BLE スキャンできない場合があります。
- Android では OS バージョンごとに必要な Bluetooth 権限が異なります。
- Android では端末やメーカーの省電力制御により、スキャン頻度やバックグラウンド動作が制限される場合があります。
- Android の詳細ログは検出数が多い環境では量が増えます。通常時は `設定` タブの `詳細ログを表示する` をオフにしてください。通常ログでは同じ iBeacon の検出ログを一定間隔に抑えます。
- RSSI は揺れやすいため、推定距離は目安です。
- MQTT は必要最小限の MQTT 3.1.1 パケット処理を独自実装しています。高度な MQTT 機能は対象外です。

## 開発者向け

### 必要環境

- Qt 6.7 以降
- CMake 3.16 以降
- C++17 対応コンパイラ
- Qt モジュール: `Core`, `Concurrent`, `Gui`, `Quick`, `QuickControls2`, `Bluetooth`, `Network`, `Sql`

Windows で BLE スキャンを行う場合は、Bluetooth アダプターと Windows の Bluetooth / プライバシー設定が有効であることを確認してください。

Android で BLE スキャンを行う場合は、Android SDK / NDK / JDK と Qt Android キットが必要です。Android 固有の権限、Java ブリッジ、アイコン、パッケージ設定は `android/` ディレクトリに置き、`QT_ANDROID_PACKAGE_SOURCE_DIR` から参照しています。

### ビルド

Qt Creator で開く場合は、このフォルダの `CMakeLists.txt` を開いて構成してください。

Windows コマンドライン例:

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:\Qt\6.11.1\msvc2022_64"
cmake --build build --config Release
```

Qt Creator 付属のキットを使う場合は、Qt Creator からビルドするのが一番確実です。MSVC を使う場合は、Visual Studio の開発者環境が有効になっている必要があります。

Android 版は Qt Creator の Android キット、または GitHub Actions の release workflow と同等の環境でビルドしてください。CI では Android arm64-v8a APK、Windows x64 zip、Linux x64 AppImage をタグ push で生成し、GitHub Release に添付します。

### プロジェクト構成

```text
.
├── .github
│   └── workflows
│       └── release.yml
├── android
│   ├── AndroidManifest.xml
│   ├── build.gradle
│   ├── res
│   │   ├── drawable
│   │   │   └── app_icon.png
│   │   ├── values
│   │   │   └── libs.xml
│   │   └── xml
│   │       └── qtprovider_paths.xml
│   └── src
│       └── io/github/nanamitm/ibeaconscanner/QtBeaconScanner.java
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
├── src
│   ├── BeaconInfo.h
│   ├── BeaconScanner.cpp
│   ├── BeaconScanner.h
│   ├── HaReceiver.cpp
│   ├── HaReceiver.h
│   ├── LocationHistory.cpp
│   ├── LocationHistory.h
│   ├── MqttReceiver.cpp
│   ├── MqttReceiver.h
│   └── main.cpp
├── CMakeLists.txt
├── LICENSE
└── README.md
```

## ライセンス

MIT License です。詳細は [LICENSE](LICENSE) を参照してください。
