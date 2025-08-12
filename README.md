# trans

uptermなど、ttyベースのコネクションしか提供されない状況で、byte-transparentなポートフォワードをする試みです。

```
trans := transparent & transport
```

モラルに気をつけて使いましょう。port forwardingが提供されない環境には、その理由があるものです。

## build

```
make trans
```

## usage

```
./trans --help
```
