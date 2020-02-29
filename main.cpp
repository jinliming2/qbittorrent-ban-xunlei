#include <iostream>
#include <thread>

#include <curl/curl.h>
#include "rapidjson/include/rapidjson/document.h"

#include "main.hpp"

using std::chrono::seconds;
using std::chrono::system_clock;
using std::this_thread::sleep_for;

bool notChangedFlag = true;

int main() {
  while (true) {
    try {
      do_job();
    } catch(std::string e) {
      std::cout << "[E]" << ' ' << e << std::endl;
    } catch(const char *e) {
      std::cout << "[E]" << ' ' << e << std::endl;
    }
    sleep_for(seconds(3));
  }

  return 0;
}

void do_job() {
  clear_expired_ban_list();
  update_torrents();
  for (auto &m : torrent_list) {
    update_peers(m.first, m.second.size);
  }
  if (!notChangedFlag) {
    set_ban_list();
  }
}

// 清除过期封禁列表
void clear_expired_ban_list() {
  std::time_t now = system_clock::to_time_t(system_clock::now());

  std::string cleared;

  for (const auto &m : banned_list) {
    if (m.second <= now) {
      banned_list.erase(m.first);
      cleared += m.first + " ";
      notChangedFlag = false;
    }
  }

  if (!notChangedFlag) {
    std::cout << "[I] Cleared: " << cleared << std::endl;
  }
}

// 更新 Torrent 列表
void update_torrents() {
  static int rid = 0;

  CURL *curl = curl_easy_init();

  if (!curl) {
    throw "update_torrents: CURL init failure!";
  }

  std::string str;

  struct curl_slist *chunk = NULL;
  chunk = curl_slist_append(chunk, "Accept: application/json");

  curl_easy_setopt(curl, CURLOPT_URL, (std::string(HOST) + "/api/v2/sync/maindata?rid=" + std::to_string(rid)).c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
  curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
  curl_easy_setopt(curl, CURLOPT_COOKIEFILE, COOKIEFILE);
  curl_easy_setopt(curl, CURLOPT_COOKIEJAR, COOKIEFILE);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CURL_write_stdString);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &str);
  // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1l);

  CURLcode res = curl_easy_perform(curl);

  curl_easy_cleanup(curl);
  curl_slist_free_all(chunk);

  if (res != CURLE_OK) {
    throw std::string("update_torrents: CURL request failure! ") + curl_easy_strerror(res);
  }

  if (str.length() < 2) {
    throw std::string("update_torrents: Cannot fetch data! ") + str;
  }

  rapidjson::Document document;
  document.Parse(str.c_str());
  rid = document["rid"].GetInt();

  auto torrents = document.FindMember("torrents");
  if (torrents == document.MemberEnd() || !torrents->value.IsObject()) {
    return;
  }

  for (const auto &m : torrents->value.GetObject()) {
    std::string name = m.name.GetString();
    auto torrent = m.value.GetObject();
    auto num_leechs = torrent.FindMember("num_leechs");
    bool has_num_leechs = num_leechs != torrent.MemberEnd() && num_leechs->value.IsNumber();
    auto size = torrent.FindMember("size");
    bool has_size = size != torrent.MemberEnd() && size->value.IsUint64();

    auto info = torrent_list.find(name);
    if (info == torrent_list.end()) {
      struct torrent_info new_info = {
        .num_leechs = has_num_leechs ? num_leechs->value.GetInt() : 0,
        .size = has_size ? size->value.GetUint64() : INT_MAX,
      };
      torrent_list[name] = new_info;
    } else {
      if (has_num_leechs) {
        info->second.num_leechs = num_leechs->value.GetInt();
      }
      if (has_size) {
        info->second.size = size->value.GetUint64();
      }
    }
  }

  auto removed = document.FindMember("torrents_removed");
  if (removed != document.MemberEnd() && removed->value.IsArray()) {
    for (const auto &m : document["torrents_removed"].GetArray()) {
      torrent_list.erase(m.GetString());
    }
  }
}

// 获取正在传输的 Torrent 列表
void update_peers(const std::string &hash, const uint64_t &size) {
  CURL *curl = curl_easy_init();

  if (!curl) {
    throw "update_peers: CURL init failure!";
  }

  std::string str;

  struct curl_slist *chunk = NULL;
  chunk = curl_slist_append(chunk, "Accept: application/json");

  curl_easy_setopt(curl, CURLOPT_URL, (std::string(HOST) + "/api/v2/sync/torrentPeers?hash=" + hash + "&rid=0").c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
  curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
  curl_easy_setopt(curl, CURLOPT_COOKIEFILE, COOKIEFILE);
  curl_easy_setopt(curl, CURLOPT_COOKIEJAR, COOKIEFILE);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CURL_write_stdString);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &str);
  // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1l);

  CURLcode res = curl_easy_perform(curl);

  curl_easy_cleanup(curl);
  curl_slist_free_all(chunk);

  if (res != CURLE_OK) {
    throw std::string("update_peers: CURL request failure! ") + curl_easy_strerror(res);
  }

  if (str.length() < 2) {
    throw std::string("update_peers: Cannot fetch data! ") + str;
  }

  rapidjson::Document document;
  document.Parse(str.c_str());

  std::time_t expire = system_clock::to_time_t(system_clock::now()) + BANTIME;

  auto peers = document.FindMember("peers");
  if (peers == document.MemberEnd() || !peers->value.IsObject()) {
    return;
  }

  for (const auto &m : peers->value.GetObject()) {
    auto peer = m.value.GetObject();

    auto ip = peer.FindMember("ip");
    if (ip == peer.MemberEnd() || !ip->value.IsString()) {
      continue;
    }

    std::string ip_address = ip->value.GetString();

    auto client = peer.FindMember("client");
    if (client != peer.MemberEnd() && client->value.IsString()) {
      std::string client_str = client->value.GetString();
      if (std::regex_search(client_str, XL0012) || std::regex_search(client_str, XUNLEI0012)) {
        banned_list[ip_address] = expire;
        notChangedFlag = false;
        continue;
      }
    }

    auto uploaded = peer.FindMember("uploaded");
    auto progress = peer.FindMember("progress");
    if (
      uploaded != peer.MemberEnd() && uploaded->value.IsUint64() &&
      progress != peer.MemberEnd() && progress->value.IsNumber()
    ) {
      int should_progress = uploaded->value.GetUint64() * 100 / size;
      int actrue_progress = progress->value.GetDouble() * 100;
      if (should_progress - actrue_progress > 2) {
        banned_list[ip_address] = expire;
        notChangedFlag = false;
        continue;
      }
    }
  }
}

// 设置封禁列表
void set_ban_list() {
  CURL *curl = curl_easy_init();

  if (!curl) {
    throw "update_peers: CURL init failure!";
  }

  std::string banned;

  std::string data = "json=%7B%22banned_IPs%22%3A%22";
  for (const auto &m : banned_list) {
    data.append(std::regex_replace(m.first, regCOLON, "%3A")).append("%5Cn");
    banned += m.first + " ";
  }
  data.append("%22%7D");

  struct curl_slist *chunk = NULL;
  chunk = curl_slist_append(chunk, "Content-Type: application/x-www-form-urlencoded");

  curl_easy_setopt(curl, CURLOPT_URL, (std::string(HOST) + "/api/v2/app/setPreferences").c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
  curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
  curl_easy_setopt(curl, CURLOPT_COOKIEFILE, COOKIEFILE);
  curl_easy_setopt(curl, CURLOPT_COOKIEJAR, COOKIEFILE);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
  // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1l);

  CURLcode res = curl_easy_perform(curl);

  if (res != CURLE_OK) {
    throw std::string("set_ban_list: CURL request failure! ") + curl_easy_strerror(res);
  } else {
    notChangedFlag = true;
    std::cout << "[I] Banned: " << banned << std::endl;
  }

  curl_easy_cleanup(curl);
  curl_slist_free_all(chunk);
}

// CURL 读取数据处理函数
size_t CURL_write_stdString(void *contents, size_t size, size_t nmemb, std::string *str) {
  size_t newLength = size * nmemb;
  try {
    str->append((char*)contents, newLength);
  } catch(std::bad_alloc &e) {
    //handle memory problem
    return 0;
  }
  return newLength;
}
