# ToySPSC

C++23 で書くおもちゃのロックフリー Single-Producer / Single-Consumer (SPSC) リングバッファ。

> English version: [`README.md`](./README.md)

現時点では `Hello, ToySPSC` を出力する main しか入っていません。このリポジトリの
役割は、SPSC 本体を書き始めたときに即座にビルド・テスト・ベンチ・プロファイル・CI
が回るよう、周辺ツールを揃えておくことです。

## 前提

ホスト側に必要なのは 2 つだけです:

- [`podman`](https://podman.io/) (macOS の場合は `podman machine start` を 1 度実行)
- [`just`](https://github.com/casey/just)

`cmake` / `ninja` / `gcc-14` / `clang-tools` / `lcov` / `doxygen` / `perf` /
`valgrind` などは **すべて Linux コンテナ `toyspsc:dev` の中**に入っています
(`Containerfile`、Nix ベース)。ホストに C++ ツールチェインをインストールする
必要はありません。

## クイックスタート

```sh
just image      # toyspsc:dev イメージをビルド
just test       # configure + build + ctest
just bench      # Google Benchmark マイクロベンチ
just profile    # perf stat + perf record + perf report --stdio
just fmt        # clang-format -i
just fmt-check  # clang-format --dry-run --Werror
just lint       # clang-tidy
just clean      # build-* と perf.data を削除
```

`just profile` は `perf` がハードウェアカウンタを読めるよう
`--cap-add SYS_ADMIN --security-opt seccomp=unconfined` 付きでコンテナを起動します。
このフラグは justfile 内で自動的に渡されます。

## ディレクトリ構成

```
.
├── CMakeLists.txt       # C++23, FetchContent で GoogleTest / Google Benchmark
├── Containerfile        # nixos/nix ベース、nix profile install でツール一式
├── justfile             # ホスト側の Podman タスクランナー
├── Doxyfile             # API ドキュメント設定
├── .clang-format        # Google スタイル、100 桁
├── .clang-tidy          # bugprone/cert/cppcore/modernize/...
├── .github/workflows/   # GitHub Actions CI (test / fmt / lint / bench-smoke)
├── src/
│   ├── main.cc          # hello world プレースホルダ
│   └── spsc/            # 将来 SPSC ヘッダを置く場所
├── test/
│   └── sanity_test.cc   # GoogleTest プレースホルダ
└── bench/
    └── sanity_bench.cc  # Google Benchmark プレースホルダ
```
