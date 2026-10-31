# Dynamic Wind Demo

C++20 / OpenGL 4.3 で静的 FBX モデルを表示するデモです。

Entity の Transform、モデル描画、カメラ操作と ImGui によるデバッグ操作を提供します。

## 必要環境

実行時:

- Windows 10 / 11 (x64)
- OpenGL 4.3 以上に対応する GPU とメーカー製ドライバー
- [Microsoft Visual C++ 再頒布可能パッケージ（x64）](https://learn.microsoft.com/ja-jp/cpp/windows/latest-supported-vc-redist)

ソースからビルドする場合:

- Visual Studio 2026 または 2022
  - 「C++ によるデスクトップ開発」
  - MSVC x64/x86 ビルドツール、Windows SDK
  - 「Windows 用 C++ CMake ツール」
- CMake 3.25 以上（VS 2026 のソリューション生成には 4.2 以上）
- 初回構成時のインターネット接続（依存ライブラリ取得用）

Git はバージョン管理に、GitHub アカウントは GitHub への公開時に必要です。

## 同梱の実行ファイル

リポジトリのルートから、同梱の Release ビルドを起動できます。

```powershell
.\solutions\exe\dynamic_wind_demo.exe
.\solutions\exe\dynamic_wind_demo.exe --help
```

この実行ファイルにはビルド時のモデル配置先
`D:\dynamic_wind_demo\solutions\model` が組み込まれています。

実行ファイル単体では動作せず、その場所に地形・移動物体のモデルと参照画像が必要です。
別の場所や PC で利用する場合は、モデルを含むリポジトリから
[ビルド](#ビルドと-visual-studio-ソリューション)し直し、生成された実行ファイルを使用してください。

ソースの再ビルドでは `solutions\exe` の同梱ファイルは自動更新されません。

## ビルドと Visual Studio ソリューション

PowerShell でプロジェクトのルートから実行します。

スクリプトは PATH 上の CMake、または Visual Studio 同梱 CMake を使用します。

```powershell
.\scripts\build.ps1 -Preset vs2026 -Configuration Debug
.\scripts\build.ps1 -Preset vs2026 -Configuration Release
```

PowerShell の実行ポリシーによりスクリプトが拒否された場合は、組織のポリシーに従い、
下記の CMake コマンドを Developer PowerShell for VS から直接実行してください。

VS 2022 では `vs2026` を `vs2022` に置き換えます。

```powershell
cmake --preset vs2026
cmake --build --preset vs2026-debug --parallel
```

CMake が PATH にない通常の PowerShell では、Visual Studio 2026 同梱版を
次のように直接指定できます。実行ポリシーの変更は不要です。

```powershell
$tools = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
& "$tools\cmake.exe" --preset vs2026
& "$tools\cmake.exe" --build --preset vs2026-debug --parallel
```

生成される `solutions\vs2026\DynamicWindDemo.slnx` を Visual Studio 2026 で開きます。
VS 2022 では `solutions\vs2022\DynamicWindDemo.sln` になります。

`dynamic_wind_demo` がスタートアッププロジェクトです。Debug / Release と x64 を選択して F5 で実行できます。
生成ファイルは Git 管理せず、CMake から再生成します。

```powershell
.\solutions\vs2026\Debug\dynamic_wind_demo.exe
.\solutions\vs2026\Release\dynamic_wind_demo.exe
.\solutions\vs2026\Debug\dynamic_wind_demo.exe --help
```

`--help` はウィンドウを開かず使用方法を表示します。その他の引数は受け付けません。

## 構成

| 要素 | 実装 |
|---|---|
| 言語 / ビルド | C++20 / CMake Presets |
| ウィンドウ | GLFW 3.4 |
| 描画 | OpenGL 4.3 Core / 静的モデル、方向光、環境光 |
| OpenGL 関数ロード | GLEW 2.2.0（glew-cmake 配布、静的リンク） |
| デバッグ UI | Dear ImGui v1.91.9b / GLFW・OpenGL3 バックエンド |
| モデル読み込み | ufbx 0.23.1 / stb_image |
| バージョン管理 | Git / GitHub 対応 |

依存関係は標準プリセットでは FetchContent で `solutions\<preset>\_deps` に取得し、バージョンと
SHA-256 を固定しています。Python、vcpkg、手動のライブラリ配置は不要です。

依存元のライセンスは取得先の LICENSE / COPYING を参照してください。

- `solutions\src\main.cpp`: コマンドライン引数の処理とアプリケーションの起動
- `solutions\src\render`: カメラ、共有 GPU モデル資源、描画
- `shaders\model.vert` / `shaders\model.frag`: モデルの変換と照明
- `solutions\src\demo\wind\simulation\shader\*.comp`: 風の格子の初期化・分配・仕上げ
- `solutions\src\demo\wind\debug`: 風デバッグ操作と、線分生成・描画・断面プレビュー・操作画面の実装
- `cmake\shader_sources.hpp.in`: GLSL の実行ファイルへの埋め込み

シェーダー変更時は再ビルドしてください。CMake が変更を検出して埋め込みを再生成します。
実行時の作業ディレクトリに依存せず、シェーダーファイルのコピーも不要です。

## 風のデバッグ

標準の VS 2026 / 2022 プリセットでは風デバッグ機能が有効です。

「風のデバッグ」画面で基本風と球・円錐・円柱の発生器を設定し、風管理空間の
格子、風向き、断面を確認できます。発生器の配置、各表示とシミュレーションの詳細は
[風のデバッグとシミュレーション](docs/wind.md) を参照してください。

風によるモデルの変形・移動はまだ実装していません。

プリセットを使わない場合、CMake オプション `DYNAMIC_WIND_ENABLE_DEBUG_MOTORS` の
既定値は `OFF` です。標準プリセットで無効にする場合は、構成時に明示して再ビルドします。

```powershell
cmake --preset vs2026 -D DYNAMIC_WIND_ENABLE_DEBUG_MOTORS=OFF
cmake --build --preset vs2026-debug --parallel
```

プリセットを引数なしで再構成する、または `build.ps1` を再実行すると `ON` に戻ります。

## ステージ表示と操作

起動時に次の両方を読み込みます。どちらかが読み込めない場合はエラーを表示して終了します。

- 地形: `solutions\model\stage\simple\source\Map 3D Models FBX.fbx`
- 移動物体: `solutions\model\actor\cylinder.fbx`

地形モデルの選択機能はありません。

両方のパスは CMake 構成時のソースディレクトリに基づくため、実行時の作業ディレクトリには
依存しません。

実行ファイルだけでなくモデルと参照画像も必要です。
リポジトリを移動した場合は再構成・再ビルドした実行ファイルを使用してください。

- **右ドラッグ**: カメラの視線を回転
- **W / S**: 視線方向に前進 / 後退
- **A / D**: カメラに対して左 / 右へ移動
- **Lighting**: 方向光と環境光の有効 / 無効
- **Light direction**: 光の進行方向。ゼロや非有限値は適用しません。
- **Stage**: 移動物体の周回経路の幅・奥行きと移動速度を変更
- **Esc**: 終了

カメラの初期位置は `(0, 10, 10)`、初期視線は負の Z 方向です。
原点を見たい場合は、右ドラッグで下方向に視線を動かしてください。

フィールド Entity の Transform は原点・回転なし・4 倍で固定です。移動物体は
0.5 倍で、フィールドの中央・床上を幅 10 m、奥行き 10 m、初速 2 m/s の
四角形経路で周回します。

一般のモデル Entity は
`ModelComponent` と `LocalTransform` を組み合わせ、同期済みのワールド行列で描画できます。
複数 Entity が同じ `render::Model` を共有できます。

GPU 資源は OpenGL コンテキストを
所有するスレッドで作成・使用・解放し、コンテキストより先に破棄してください。
モデル読み込みや GPU 登録が失敗した場合はエラーを表示して終了します。

三角形ごとのマテリアル、基本色・テクスチャ、粗さ・金属度を使用します。透過、
法線マップ、投影される影、アニメーションは描画しません。ゼロスケールなどで
法線変換ができない場合は環境光のみで表示します。

ローダーは法線マップも読み込むため、描画に使わない場合でも FBX が参照する画像は
読み込み可能である必要があります。

### 使用したステージ素材

- 作品: [Map 3D Models FBX](https://sketchfab.com/3d-models/map-3d-models-fbx-399a527803604197adfbf2d4bd1ea1f4)
- 作者: [Kasie Stone](https://sketchfab.com/kasiestone)
- ライセンス: [Creative Commons 表示 4.0 国際（CC BY 4.0）](https://creativecommons.org/licenses/by/4.0/)
- 変更: 付属テクスチャのファイル名と配置を変更しています。FBX とテクスチャの
  ファイル内容は同梱の配布アーカイブと同一です。デモでは地形を 4 倍で表示します。

CC BY 4.0 は、適切なクレジット、ライセンスへのリンク、変更の表示などを条件に、
商用を含む共有・改変を認めています。

追加の法的条件や技術的制限により、
ライセンスが許諾する利用を制限することはできません。

詳細は
[ライセンス本文](https://creativecommons.org/licenses/by/4.0/legalcode.ja)を参照してください。
このライセンス表示はステージ素材に適用され、他の素材やプログラムのライセンスを
定めるものではありません。

## 詳細資料

- [風のデバッグとシミュレーション](docs/wind.md): 操作、風発生器、GPU 更新と読み戻し
- [開発者向けガイド](docs/development.md): FBX 読み込み、姿勢、ECS
- [利用ライブラリとライセンス](solutions/src/LIBRAEIES_USED.md): ufbx / stb_image

現在の CMake 設定には自動テストのターゲットはありません。ビルド後は起動して
モデル表示と必要な操作を確認してください。

## Git / GitHub

ローカルは `git init -b main` で初期化します。ビルド成果物・依存の展開先・IDE 個人設定は除外します。
コミットや GitHub リポジトリの作成・送信は自動では行いません。

Git が PATH にない場合、Visual Studio の「Git」メニューまたは同梱の `git.exe` を使用できます。

GitHub 側に空のリポジトリを用意した後、認証済みの環境で実行してください。
公開範囲、リモート URL、ライセンスはプロジェクトの方針に応じて決定してください。

```powershell
git add .
git commit -m "Initial C++20 OpenGL compute demo"
git remote add origin https://github.com/YOUR_ACCOUNT/dynamic_wind_demo.git
git push -u origin main
```

GitHub 認証トークンやローカル専用のパス・資格情報はソースに記録しないでください。
