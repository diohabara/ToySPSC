# Primitive SPSC Queue 解説

## SPSC とは何か

**SPSC** は **Single-Producer Single-Consumer** の略で、**1つのスレッド（producer）がデータを書き込み、もう1つのスレッド（consumer）がデータを読み出す**というパターンを指す。

### どこで使われるか

| 分野 | 用途 |
|------|------|
| オーディオ処理 | リアルタイムスレッドとI/Oスレッド間のサンプルバッファ受け渡し |
| ���ットワーク | パケット受信スレッド → 処理スレッドへのメッセージパッシング |
| ログ | アプリケーションスレッド → ログ書き込みスレッドへの非同期転送 |
| ゲーム | 更新スレッド → レンダリングスレッドへのフレームデータ受け渡し |

### なぜ lock-free か

mutex（ロック）を使った同期は以下の問題がある:

1. **レイテンシのスパイク**: ロック競合時に OS がスレッドをスリープさせる → マイクロ秒〜ミリ秒の遅延
2. **優先度逆転**: 低優先度スレッドがロックを保持中に高優先度スレッドがブロックされる
3. **コンテキストスイッチ**: OS のスケジューラ介入によるオーバーヘッド

SPSC では producer と consumer がそれぞれ **自分専用のインデックスだけを書き込む** ため、CAS（compare-and-swap）すら不要。`acquire`/`release` の atomic store/load だけで正しさが保証される。これは **MPMC（Multiple-Producer Multiple-Consumer）では不可能な、SPSC 固有の性質** である。

---

## リングバッファの基礎

リングバッファ（circular buffer）は固定サイズの配列を「環」のように使い回すデータ構造。

```
  buffer_[0]  buffer_[1]  buffer_[2]  buffer_[3]
  ┌───────────┬───────────┬───────────┬───────────┐
  │           │  data A   │  data B   │           │
  └───────────┴───────────┴───────────┴───────────┘
               ↑ head=1                ↑ tail=3
               (次に読む)              (次に書く)
```

### head と tail

- **`tail_`**: producer が次に書き込む位置。producer だけが更新する
- **`head_`**: consumer が次に読み出す位置���consumer だけが更新する

両方とも **単調増加** する（0, 1, 2, 3, ... とインクリメントし続ける）。配列へのインデックスに変換するときだけ Capacity で割る。

### 2のべき乗トリック

Capacity を2のべき乗（2, 4, 8, 16, ...）に制限すると、剰余演算をビットマスクに置き換えられる:

```
index % Capacity  →  index & (Capacity - 1)
```

例えば Capacity = 8 の場合:
- `Capacity - 1 = 7 = 0b0111`
- `index & 0b0111` で下位3ビットだけ取り出す → 0〜7 に丸まる

**なぜこれが重要か**: 整数の除算（`div` 命令）はCPU上で数十サイクルかかるが、AND演算は1サイクル。ホットパスでは無視できない差になる。

コード上では `std::has_single_bit(Capacity)` で2のべき乗を `static_assert` で強制している:

```cpp
static_assert(std::has_single_bit(Capacity), "Capacity must be a power of two");
static constexpr std::size_t kMask = Capacity - 1;
// ...
buffer_[tail & kMask] = value;
```

### 満杯と空の判定

head/tail は単調増加するので:

- **空**: `head == tail`（consumer が producer に追いついた）
- **満杯**: `tail - head == Capacity`（producer が consumer を1周追い越しそう）

`std::size_t`（64ビット符号なし整数）が一周するには 2^64 回の操作が必要。1GHz でインクリメントし続けても約585年かかるので、実用上オーバーフローは起きない。

---

## メモリオーダリング

### CPU はなぜ命令を並べ替えるか

現代の CPU はパフォーマンスのため、メモリへの読み書きの**順序を並べ替える**。例えば:

```
// プログラム上の順序
buffer_[idx] = value;  // (1) データ書き込み
tail_ = tail + 1;      // (2) tail 更新
```

CPU は (2) を (1) より先に実行する可能性がある。単一スレッドでは問題ないが、マルチスレッドでは consumer が **tail の更新を見たのに、データがまだ書かれていない** 状態を観測し得る。

### acquire / release の契約

C++ の `std::memory_order` はコンパイラと CPU に対して「ここまでの操作はこの順序で見えるようにしろ」と指示する仕組み。

#### Producer 側（`try_push`）

```cpp
[[nodiscard]] auto try_push(const T& value) noexcept -> bool {
  const auto tail = tail_.load(std::memory_order_relaxed);   // ① 自分しか書かないので relaxed
  const auto head = head_.load(std::memory_order_acquire);   // ② consumer の release と対
  if (tail - head == Capacity) {
    return false;
  }
  buffer_[tail & kMask] = value;                             // ③ データ書き込み
  tail_.store(tail + 1, std::memory_order_release);          // ④ ③の後に見えることを保証
  return true;
}
```

- **④ の `release`**: 「④より前のすべてのメモリ操作（③のデータ書き込み含む）が、④を `acquire` で読んだスレッドから見えることを保証する」
- **② の `acquire`**: 「consumer が head を `release` で更新した時点までの操作が、ここから見えることを保証する」→ consumer がスロットを消費済みであることを正しく観測できる
- **① の `relaxed`**: tail は producer 自身しか書かない。自スレッドの書き込みは常に自スレッドから見えるので、同期は不要

#### Consumer 側（`try_pop`）

```cpp
[[nodiscard]] auto try_pop() noexcept -> std::optional<T> {
  const auto head = head_.load(std::memory_order_relaxed);   // ① 自分しか書かないので relaxed
  const auto tail = tail_.load(std::memory_order_acquire);   // ② producer の release と対
  if (head == tail) {
    return std::nullopt;
  }
  auto value = buffer_[head & kMask];                        // ③ ���ータ読み出し
  head_.store(head + 1, std::memory_order_release);          // ④ ③の後に見えることを保証
  return value;
}
```

対称的な構造。consumer の `acquire` は producer の `release` と対になり、producer の `acquire` は consumer の `release` と対になる。

### なぜ `seq_cst` は不要か

`memory_order_seq_cst`（逐次一貫性）は「すべてのスレッドが同じ順序でatomic操作を観測する」ことを保証する、最も強い（＝最も遅い）順序。

SPSC では:
- スレッドは **2つだけ**
- 同期変数は **`head_` と `tail_` の2つだけ**
- 各変数は **片方のスレッドだけが書く**

この条件下では `acquire`/`release` ペアで十分。`seq_cst` が必要になるのは、3つ以上のスレッドが2つ以上のatomic変数の更新順序について合意する必要がある場合（例: Dekker のアルゴリズム）。

---

## False Sharing

### キャッシュラインとは

CPU は RAM からデータをバイト単位ではなく、**キャッシュライン**（通常64バイト）という固定サイズのブロック単位で読み込む。同じキャッシュラインに乗っているデータは高速にアクセスできる。

### False Sharing の仕組み

`head_` と `tail_` が同じキャッシュラインに乗っていると何が起きるか:

1. Producer が `tail_` を更新 → Core 0 の L1 キャッシュのそのラインが dirty になる
2. Consumer が `head_` を読もうとする → 同じキャッシュラインなので Core 1 は Core 0 からラインを取得する必要がある
3. Consumer が `head_` を更新 → Core 1 のラインが dirty になる
4. Producer が `tail_` を読もうとする → Core 0 は Core 1 からラインを取得する必要がある

これが **毎回の push/pop で** 発生する。キャッシュライン転送は数十ナノ秒かかり、SPSC の性能を大幅に劣化させる。

**実際にはデータの共有は不要** なのに（`head_` は consumer だけが書き、`tail_` は producer だけが書く）、同じキャッシュラインに乗っているだけでこの転送が発生する。これが **false sharing**（偽共有）と呼ばれる所以。

### 対策: `alignas`

```cpp
alignas(kCacheLineSize) std::atomic<std::size_t> tail_{0};
alignas(kCacheLineSize) std::atomic<std::size_t> head_{0};
alignas(kCacheLineSize) std::array<T, Capacity> buffer_{};
```

`alignas(64)` はコンパイラに「この変数を64バイト境界に配置しろ」と指示する。各変数が別のキャッシュラインの先頭に来るため、false sharing が排除される。

C++17 では `std::hardware_destructive_interference_size` という定数が導入され、「false sharing を避けるために必要な最小オフセット」をポータブルに取得できる。ただし GCC では `-Winterference-size` 警告が出る（ABI 安定性の問題）ため、本実装では固定値 64 を直接使っている:

```cpp
// std::hardware_destructive_interference_size is not ABI-stable and triggers
// -Winterference-size on GCC. Use a fixed constant instead.
inline constexpr std::size_t kCacheLineSize = 64;
```

---

## コードウォークスルー

### テンプレート宣言と制約

```cpp
template <typename T, std::size_t Capacity>
  requires std::is_trivially_copyable_v<T>
class SpscQueue {
```

- `requires std::is_trivially_copyable_v<T>`: T ��� trivially copyable（`memcpy` で安全にコピーできる）ことを要求。`int32_t`, `double`, POD 構造体などが該当。`std::string` や `std::vector` は不可
- なぜこの制約か: バッファへの書き込み/読み出しが単純な代入で済むことを保証。非 trivial な型はコンストラクタ/デストラクタの呼び出しタイミングが複雑になり、lock-free の正しさの保証が難しくなる

### コピー/ムーブの禁止

```cpp
SpscQueue(const SpscQueue&) = delete;
SpscQueue(SpscQueue&&) = delete;
auto operator=(const SpscQueue&) -> SpscQueue& = delete;
auto operator=(SpscQueue&&) -> SpscQueue& = delete;
```

`std::atomic` はコピーもムーブもできないので、SpscQueue 自体もコピー/ムーブ不可。また、producer と consumer が参照を保持している状態でキューが移動すると、ダングリング参照になる。

### `[[nodiscard]]` と `noexcept`

```cpp
[[nodiscard]] auto try_push(const T& value) noexcept -> bool;
[[nodiscard]] auto try_pop() noexcept -> std::optional<T>;
```

- `[[nodiscard]]`: 戻り値を無視したらコンパイラが警告する。`try_push` の結果を無視すると、データを失ったことに気づかない
- `noexcept`: trivially copyable な型の代入は例外を投げないし、atomic 操作も例外を投げない。`noexcept` を明示することでコンパイラが最適化しやすくなる

---

## 制約と発展

この実装は「primitive」な SPSC であり、以下の制約がある:

| 制約 | 説明 |
|------|------|
| 固定容量 | コンパイル時に決まる。実行時にリサイズ不可 |
| trivially copyable のみ | `std::string` 等は格納できない |
| Non-blocking のみ | 空/満杯時に即座に返る。呼び出し側がスピンやバックオフを管理 |
| バルク操作なし | 1要素ずつの push/pop のみ |
| キャッシュ付きインデックスなし | 毎回 atomic load で相手のインデックスを読む |

### 発展の方向

1. **キャッシュ付きインデックス**: 相手のインデックスのローカルコピーを持ち、atomic load の頻度を減らす → スループット向上
2. **バルク操作**: 複数要素を一度に push/pop → アルゴリズム-atomic 比率の改善
3. **Blocking API**: `try_push`/`try_pop` の上にスピン + バックオフ + futex wait を構築
4. **非 trivially copyable 型対応**: placement new / manual destructor で生存期間を管理

---

## プロファイリング

`just profile` でベンチマークの包括的なプロファイリングが実行される。

### 出力される情報

| ステップ | ツール | 何がわかるか | 備考 |
|---------|--------|------------|------|
| perf stat | `perf` | cycles, instructions, cache-misses, branch-misses などの HW カウンタ | Linux ホストのみ |
| perf report | `perf` | CPU 時間を消費している関数のランキング | Linux ホストのみ |
| perf annotate | `perf` | ホットな関数のアセンブリとソースのマッピング | Linux ホストのみ |
| flamegraph | `perf` + FlameGraph | コールスタックごとの CPU 時間を SVG で可視化 | Linux ホストのみ。`flamegraph.svg` に出力 |
| cachegrind | `valgrind` | L1/LL キャッシュミス、分岐ミスを関数・行レベルでシミュレーション | どの OS でも動作。`cachegrind.out` に出力 |

### flamegraph の読み方

- **横幅** = その関数（とその子関数）が使った CPU 時間の割合。幅が広いほどホット
- **縦方向** = コールスタックの深さ。下が呼び出し元、上が呼び出し先
- **色** は意味を持たない（ランダム）
- クリックでズームイン可能（SVG をブラウザで開く）

SPSC のホットパスでは `try_push` / `try_pop` 内の atomic load/store が支配的になるはず。それ以外の関数が幅を取っていれば、そこに最適化の余地がある。

### cachegrind の読み方

`cg_annotate` の出力で注目すべき列:

- **D1mr** / **DLmr**: L1 データキャッシュ読みミス / LL（Last Level）読みミス
- **D1mw** / **DLmw**: L1 データキャッシュ書きミス / LL 書きミス
- **Bc** / **Bcm**: 条件分岐の総数 / ミス数

false sharing が発生している場合、`head_` と `tail_` 周辺の `D1mr`/`D1mw` が異常に高くなる。`alignas` が正しく効いていれば、これらの値は低く抑えられる。

### macOS での制限

macOS ホスト上の Podman コンテナでは Linux の `perf_event` サブシステムにアクセスできないため、perf 系のステップ（perf stat / record / report / annotate / flamegraph）はスキップされる。cachegrind はソフトウェアシミュレーションなのでどの環境でも動作する。

Linux ホスト上では全ステップが実行される。

### 参考文献

- [Folly `ProducerConsumerQueue`](https://github.com/facebook/folly/blob/main/folly/ProducerConsumerQueue.h) — Facebook の SPSC 実装
- [Rigtorp `SPSCQueue`](https://github.com/rigtorp/SPSCQueue) — キャッシュ付きインデックスを持つ高性能版
- [Boost.Lockfree `spsc_queue`](https://www.boost.org/doc/libs/release/doc/html/lockfree/reference.html#header.boost.lockfree.spsc_queue_hpp) — Boost の SPSC
- Preshing, Jeff. ["An Introduction to Lock-Free Programming"](https://preshing.com/20120612/an-introduction-to-lock-free-programming/) — lock-free プログラミングの入門
- Preshing, Jeff. ["Acquire and Release Semantics"](https://preshing.com/20120913/acquire-and-release-semantics/) — acquire/release の詳細な解説
