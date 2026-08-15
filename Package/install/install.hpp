#pragma once
#include <curl/curl.h>
#include <string>
#include <thread>
#include <mutex>  
#include <iostream>
#include <vector>

size_t write_file_cb(void* ptr, size_t size, size_t nmemb, void* userdata);

bool download_package_file(const std::string& url, const std::string& dest);

bool extract_zip(const std::string& zip_path, const std::string& dest_dir);

void install_package(const std::string &package_name, const std::string &package_dir);
