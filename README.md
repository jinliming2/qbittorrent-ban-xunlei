# qBittorrent Ban Xunlei

通过 WebUI API 为 qBittorrent server 屏蔽吸血迅雷。

## Build

```bash
$ git clone ......
$ cd ......
$ git submodule init
$ git submodule update --depth 1
$ # 编辑 main.hpp 文件第 5 行，改为 qbittorrent 本地地址
$ # qbittorrent 需要开启设置 Web UI 中的 Bypass authentication for clients on localhost，以跳过本地接口请求的帐号认证
$ g++ main.cpp -lcurl -o qbittorrent-ban-xl
```

## TODO

* [ ] 支持配置文件，配置 qbittorrent 地址、封禁时长等
* [ ] 支持配置帐号密码
* [ ] 添加 systemd 守护运行配置文件示例
* [ ] 添加 Docker 运行方式
