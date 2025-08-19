# trans

uptermなど、ttyベースのコネクションしか提供されない状況で、byte-transparentなポートフォワードをする試みです。

```
trans := transparent & transport
```

**モラルに気をつけて使いましょう。port forwardingが提供されない環境には、その理由があるものです。**

## build

```
make trans
```

## usage

```
./trans --help
```

## vnc tunneling example

リモート側:
- ~/transに本リポジトリをcloneしmakeしておきます
- `upterm host --force-command ~/trans/tunnel_server.sh`
- uptermで「このsshを実行しろ」と表示されるので、その `ログイン名:パスワード@ホスト名` の部分をコピー

ローカル側: 3つのターミナル画面が必要です
1. `make tunnel HOST=<upterm_ssh_target>`
    - 先程コピーしたものを `<upterm_ssh_target>` に指定
2. `make ssh ARGS="-l <remote_account>"`
    - transで張ったトンネル上でさらにVNCをポートフォワードするSSHセッションを作成
3. `make vnc`
