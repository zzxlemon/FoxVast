#include "install.hpp"
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
#include <atomic>
#include "../tools.hpp"

#ifdef _WIN32
#include <direct.h>
#define FOX_MKDIR(p) _mkdir(p)
#else
#include <unistd.h>
#define FOX_MKDIR(p) mkdir(p, 0755)
#endif

static bool dirExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && (st.st_mode & S_IFDIR) != 0;
}

// Create every missing directory in the chain (handles both separators).
static void ensureDirChain(const std::string& dir) {
    if (dir.empty() || dirExists(dir)) return;
    std::string cur;
    for (size_t i = 0; i <= dir.size(); i++) {
        if (i == dir.size() || dir[i] == '/' || dir[i] == '\\') {
            if (!cur.empty() && !dirExists(cur)) FOX_MKDIR(cur.c_str());
            if (i < dir.size()) cur += dir[i];
        } else {
            cur += dir[i];
        }
    }
}

static std::mutex mtx;   
static int sharedCounter = 0;

static int isInstalling = 0;
static int install_result = 0; // 0=unknown, 1=ok, 2=fail
static std::string download_error_msg = "";

static std::atomic<long long> dl_total = 0;
static std::atomic<long long> dl_now = 0;

bool extract_zip(const std::string& zip_path, const std::string& dest_dir) {
    ensureDirChain(dest_dir);
    std::string cmd = "tar -xf \"" + zip_path + "\" -C \"" + dest_dir + "\"";
    int rc = system(cmd.c_str());
    if (rc != 0) {
        err_color(12);
        std::fprintf(stderr, "Error: failed to extract %s (tar exit %d)\n", zip_path.c_str(), rc);
        err_color(7);
        return false;
    }
    return true;
}

static std::string url = ""; 
static std::string package_dest = "";

size_t write_file_cb(void *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t written = fwrite(ptr, size, nmemb, static_cast<FILE*>(userdata));
    dl_now.store(dl_now.load() + (long long)written);
    return written;
}

int progress_cb(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    dl_total.store((long long)dltotal);
    dl_now.store((long long)dlnow);
    return 0;
}

bool download_package_file(const std::string &url, const std::string &dest) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        download_error_msg = "failed to init curl";
        return false;
    }

    // Create parent directory chain before opening the file
    size_t slash = dest.find_last_of("/\\");
    if (slash != std::string::npos) {
        ensureDirChain(dest.substr(0, slash));
    }

    FILE* fp = fopen(dest.c_str(), "wb");
    if (!fp) {
        download_error_msg = "cannot open " + dest;
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);      
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);         
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_cb);

    dl_total.store(0);
    dl_now.store(0);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    fclose(fp);

    if (res != CURLE_OK) {
        download_error_msg = std::string(curl_easy_strerror(res));
        remove(dest.c_str());  
        return false;
    }
    return true;
}

void show_install_progress(int id) {
    std::cout << "Installing package with thread ID: " << id << std::endl;

    const char* spinner = "|/-\\";
    int frame = 0;
    const int bar_max = 20;
    while (isInstalling) {
        long long total = dl_total.load();
        long long now = dl_now.load();

        std::cout << "\r" << std::string(60, ' ') << "\r" << std::flush;
        out_color(11);
        std::cout << "-- Installing " << spinner[frame % 4] << " [";
        if (total > 0) {
            int pct = (int)(now * 100 / total);
            if (pct > 100) pct = 100;
            int filled = (int)((long long)pct * bar_max / 100);
            for (int i = 0; i < bar_max; i++) {
                std::cout << (i < filled ? '#' : ' ');
            }
            std::cout << "] " << pct << "%";
        } else {
            int filled = frame % (bar_max + 1);
            for (int i = 0; i < bar_max; i++) {
                std::cout << (i < filled ? '#' : ' ');
            }
            std::cout << "] " << (now / 1024) << " KB";
        }
        std::cout << std::flush;
        out_color(7);
        frame++;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (install_result == 1) {
        std::cout << "\r" << std::string(60, ' ') << "\r" << std::flush;
        out_color(11);
        std::cout << "-- Installing - [" << std::string(bar_max, '#') << "] 100%" << std::flush;
        out_color(7);
    }
    std::cout << std::endl;
}

void do_install_package(int id) {
    if (url == "" || package_dest == "") {
        download_error_msg = "URL or destination path is not set";
        install_result = 2;
        isInstalling = 0;
        return;
    }

    if (!download_package_file(url, package_dest)) {
        install_result = 2;
    } else {
        install_result = 1;
    }

    isInstalling = 0; 
}

void install_package(const std::string &package_name, const std::string &package_dir) {
    out_color(14);
    std::cout << "Starting installation of package: " << package_name << std::endl;
    out_color(7);
    isInstalling = 1;

    std::string config_url = readLine("./config.txt", 2); // Ensure config.txt exists and is readable

    url = config_url + "/api/plugins/" + package_name + "/download"; 
    package_dest = package_dir + "/" + package_name + ".zip";   

    if (aaa != "none") {
        url = aaa;
    }

    out_color(11);
    std::cout << "- Downloading from: " << url << std::endl;
    std::cout << "- Saving to: " << package_dest << std::endl;
    out_color(7);

    std::thread t1(show_install_progress, 1);
    std::thread t2(do_install_package, 2);

    t1.join();
    t2.join();
    if (install_result == 1) {
        std::string extract_dir = package_dir + "/" + package_name;
        if (extract_zip(package_dest, extract_dir)) {
            out_color(10);
            std::cout << "Package " << package_name << " installed successfully." << std::endl;
            out_color(7);
        } else {
            err_color(12);
            std::cerr << "Failed to extract package: " << package_name << std::endl;
            err_color(7);
        }
    }
    else {
        err_color(12);
        if (!download_error_msg.empty()) {
            std::cerr << "Download error: " << download_error_msg << std::endl;
        }
        std::cerr << "Failed to download package: " << package_name << std::endl;
        err_color(7);
    }
    std::string zip_file = package_dest;
    remove(zip_file.c_str());
}

void record_install_package_in_file(const std::string &package_name, const std::string &package_dir) {
    ensureDirChain("./packages/");
    std::ofstream namesFile("./packages/installed_packages_name_list.txt", std::ios::app);
    if (namesFile.is_open()) {
        namesFile << package_name << "\n";
        namesFile.close();
    } else {
        std::cerr << "Warning: cannot open packages/installed_packages_name_list.txt" << std::endl;
    }
    std::ofstream dirsFile("./packages/installed_packages_dir_list.txt", std::ios::app);
    if (dirsFile.is_open()) {
        dirsFile << package_dir << "\n";
        dirsFile.close();
    } else {
        std::cerr << "Warning: cannot open packages/installed_packages_dir_list.txt" << std::endl;
    }
}
