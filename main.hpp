#pragma once
#include <map>
#include <regex>

#define HOST "http://127.0.0.1:8080"
#define COOKIEFILE "./qbittorrent.cookie"

#define BANTIME 86400

std::regex XL0012("-XL0012-", std::regex_constants::icase);
std::regex XUNLEI0012("Xunlei 0.0.1.2", std::regex_constants::icase);

std::regex regCOLON(":");

struct torrent_info {
  int num_leechs;
  uint64_t size;
};

static std::map<std::string, std::time_t> banned_list;
static std::map<std::string, struct torrent_info> torrent_list;

void do_job();

// 清除过期封禁列表
void clear_expired_ban_list();

// 更新 Torrent 列表
void update_torrents();


// 获取指定 Torrent 的 Peer 列表，并识别封禁
void update_peers(const std::string&, const uint64_t&);

// 设置封禁列表
void set_ban_list();

// CURL 读取数据处理函数
size_t CURL_write_stdString(void*, size_t, size_t, std::string*);
