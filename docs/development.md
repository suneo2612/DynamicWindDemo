# 開発者向けガイド

[README に戻る](../README.md)

## FBX モデル読み込み

`LoaderFactory` で FBX ローダーを作成して読み込み、共通の `ModelData` を取得できます。
パス引数は UTF-8 として渡してください。

```cpp
const auto loaderInstance = loader::LoaderFactory::CreateLoader(path);
if (loaderInstance == nullptr || !loaderInstance->Load(path))
{
    // loaderInstance が存在する場合は GetLastError() で失敗理由を確認します。
}
else
{
    const loader::ModelData& model = loaderInstance->GetModelData();
}
```

静的メッシュを三角形化して位置・法線・全 UV セット・ノード変換・三角形ごとの
マテリアル番号を保持します。

基本マテリアルにはベースカラー、不透明度、粗さ、
金属度、ベースカラー／法線テクスチャを含みます。

画像は RGBA8 にデコードされ、
埋め込み画像と FBX ファイルからの相対パスで参照される外部画像を読み込みます。
絶対パスの外部テクスチャ、アニメーション、スキニング、モーフは対象外です。

利用バージョンとライセンス条件は
[`solutions/src/LIBRAEIES_USED.md`](../solutions/src/LIBRAEIES_USED.md) に記載しています。

## 3D 姿勢と ECS 基礎

`general/math` は四元数と列優先の 4×4 行列を提供します。右手系で、右は `+X`、
上は `+Y`、初期状態の前は `-Z` です。

オイラー角は固定軸 X、Y、Z の順に適用し、
ラジアンと度の両方を使用できます。オイラー角の特異点では同じ回転を表す別の
角度が返ることがあります。

`demo/ecs` は Entity の世代付き識別子、Component の依存関係とライフサイクル、
System の更新順、および保留要求を提供します。

`ComponentWorld` では Component を
種類ごとに管理し、更新の前に生成・Component変更・親変更、全更新の後に Entity の
削除を処理します。

要求は ID で状態を確認し、`TakeResult()` で完了結果を受け取れます。
親 Entity の削除時は子孫も終了・削除します。

`ComponentWorld` の読み取りコンテキストと内部格納型は、それぞれ
`demo/ecs/component_read_context.h` と `demo/ecs/defs/component_world_types.h` に
分離しています。初期化・終了処理では引き続き `ComponentWorld::ReadContext` を利用できます。

```cpp
#include "demo/ecs/component_world.h"

using demo::component::transform::LocalTransform;
using demo::ecs::ComponentWorld;
using demo::ecs::RequestStatus;

auto world = ComponentWorld<LocalTransform>();
const auto request = world.RequestCreate(LocalTransform{});
world.Update(0.0F);
const auto result = world.TakeResult(request);
if (result.status == RequestStatus::Succeeded && result.entity)
{
    world.SynchronizeTransforms();
    const auto forward = world.Transforms().GetWorldForward(*result.entity);
}
```

`demo/component/transform` はローカル位置・回転・スケールと親子変換を管理します。
ワールド変換は `SynchronizeTransforms()` で明示的に同期し、未同期の値を取得しようと
すると例外が発生します。

System はフェーズごとに優先度の大きい順、同順位では登録順に
実行されます。同期 System を複数回登録することもできます。

### Component の追加とライフサイクル

Component を追加する場合は `ComponentKind` に識別子を追加し、
`ComponentTraits` の特殊化で識別子・必須 Component・優先度・初期化処理を定義した上で、
`ComponentWorld` の型引数に登録します。識別子の重複、未登録の依存先、循環依存は
コンパイル時に拒否されます。

初期化は Entity ごとに前初期化・初期化・後初期化の順で行い、
各段階では依存先を優先します。不要な処理を明示するために
`NoOpComponentInitializer` を利用できます。

初期化中は対象 Component を変更でき、依存先は読み取り専用です。
依存先のポインター等を通じた変更や、失敗時に戻せない
外部への変更は行わず、途中で確保した資源は Component の寿命により解放してください。

終了処理は例外を送出しない契約です。Component への参照は、要求の反映や Entity の
削除をまたいで保持しないでください。

`ComponentWorld` へのアクセスは単一スレッドから
行ってください。

### 方向とスケール

前・右・上方向は姿勢の四元数から計算し、スケールを含めません。負スケールや
親の非一様スケールによって、変形後のモデルの見た目の軸と一致しない場合があります。
ゼロスケールは許容しますが、その変換行列の逆行列は存在しません。
