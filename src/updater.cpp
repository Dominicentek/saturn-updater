#include <SDL2/SDL.h>
#include "gui.h"
#include "picojson.h"
#include "portable_file_dialogs.h"
#include "main.h"
#include "downloader.h"
#include <filesystem>
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <utility>

#ifdef WINDOWS
#include <Windows.h>
#include <Objbase.h>
#include <Shobjidl.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

std::filesystem::path saturn_dir;
std::string executable_filename;
std::string updater_filename;

typedef void(*DownloadFinishCallback)(bool success);

#define SCREEN_INSTALL 0
#define SCREEN_INSTALLING 1
#define SCREEN_UPDATE 2
#define SCREEN_UPDATING 3
#define SCREEN_UPDATE_PREF 4
#define SCREEN_INSTALLED 5
#define SCREEN_INSTALL_FAILED 6
#define SCREEN_UPDATED 7
#define SCREEN_UPDATE_FAILED 8
#define SCREEN_NO_INTERNET 9
#define SCREEN_CHOOSE_ROM 10

#define REPO_OWNER       "Llennpie"
#define REPO_NAME        "Saturn"
#define REPO_BRANCH      "legacy"

int current_screen = 0;
int current_queue_entry = 0;
int queue_entries = 0;
float download_progress = 0;
std::ofstream download_stream;
std::thread download_thread;
DownloadFinishCallback download_finish_callback;
bool init = false;
std::string release_date;
std::vector<std::pair<std::string, std::string>> download_queue = {};

std::time_t parse_time(std::string time) {
    int year, month, day, hour, minute, second;
    sscanf(time.c_str(), "%d-%d-%dT%d:%d:%dZ", &year, &month, &day, &hour, &minute, &second);
    std::tm parsed_time = {};
    parsed_time.tm_year = year - 1900;
    parsed_time.tm_mon = month - 1;
    parsed_time.tm_mday = day;
    parsed_time.tm_hour = hour;
    parsed_time.tm_min = minute;
    parsed_time.tm_sec = second;
    return std::mktime(&parsed_time);
}

void run_saturn() {
#ifdef WINDOWS
    STARTUPINFO si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);
    SetCurrentDirectory(saturn_dir.string().c_str());
    CreateProcessA(nullptr, executable_filename.data(), nullptr, nullptr, false, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
#else
    std::filesystem::current_path(saturn_dir);
    std::system((saturn_dir / executable_filename).c_str());
#endif
}

char* exe_path() {
#ifdef WINDOWS
    char* buf = (char*)malloc(MAX_PATH);
    GetModuleFileName(NULL, buf, MAX_PATH);
    return buf;
#else
    char* buf = (char*)malloc(PATH_MAX);
    ssize_t len = readlink("/proc/self/exe", buf, PATH_MAX - 1);
    buf[len] = 0;
    return buf;
#endif
}

void download_queue_add(std::string url, std::string file) {
    download_queue.push_back({ url, file });
}

void download_queue_begin(DownloadFinishCallback finish_callback) {
    download_progress = 0;
    download_finish_callback = finish_callback;
    download_thread = std::thread([]() {
        queue_entries = download_queue.size();
        for (int i = 0; i < download_queue.size(); i++) {
            auto& entry = download_queue[i];
            current_queue_entry = i;
            Downloader downloader = Downloader(entry.first);
            downloader.progress([](double now, double total) {
                download_progress = now / total;
            });
            downloader.download();
            if (downloader.status != 200) {
                download_finish_callback(false);
                return;
            }
            std::filesystem::create_directories(std::filesystem::path(entry.second).parent_path());
            std::ofstream stream = std::ofstream(entry.second, std::ios::binary);
            stream.write(downloader.data.data(), downloader.data.size());
            stream.close();
        }
        download_queue.clear();
        download_finish_callback(true);
    });
}

void saturn_repair() {
    std::string url_prefix = "https://raw.githubusercontent.com/" REPO_OWNER "/" REPO_NAME "/" REPO_BRANCH "/";
    Downloader downloader = Downloader(url_prefix + "dependencies.txt");
    downloader.download();
    if (downloader.status != 200) return;
    std::string data = std::string(downloader.data.data());
    int ptr = 0;
    int bufptr = 0;
    char buf[256];
    std::vector<std::pair<std::string, std::string>> redirects = {};
    std::vector<std::string> links = {};
    while (true) {
        char character = data[ptr++];
        if (character == 0) break;
        if (character == 13) continue;
        if (character == 10 || bufptr == 255) {
            if (bufptr == 0) continue;
            buf[bufptr++] = 0;
            bufptr = 0;
            if (buf[0] == '#') continue;
            std::string link = std::string(buf + 2);
#ifdef WINDOWS
            if (buf[0] == '%') continue;
#else
            if (buf[0] == '$') continue;
#endif
            if (buf[0] == '>') {
                std::vector<std::string> split = {};
                std::string segment = "";
                std::stringstream stream = std::stringstream(link);
                while (std::getline(stream, segment, ';')) {
                    split.push_back(segment);
                }
                redirects.push_back({ split[0], split[1] });
            }
            else links.push_back(link);
        }
        else buf[bufptr++] = character;
    }
    for (std::string link : links) {
        std::string file = std::string(link.c_str() + url_prefix.length());
        for (auto redirect : redirects) {
            if (file.find(redirect.first) != 0) continue;
            file = redirect.second + "/" + file.substr(redirect.first.length());
            break;
        }
        if (std::filesystem::exists(saturn_dir / file)) continue;
        download_queue_add(link, (saturn_dir / file).string());
    }
}

void update_begin() {
    std::ofstream stream = std::ofstream(saturn_dir / "latest_update_date.txt");
    stream.write(release_date.c_str(), release_date.length());
    stream.close();
    download_queue_begin([](bool success) {
        system(("chmod +x " + (saturn_dir / executable_filename).string()).c_str());
        current_screen = success ? SCREEN_UPDATED : SCREEN_UPDATE_FAILED;
    });
    current_screen = SCREEN_UPDATING;
}

bool updater_init() {
    executable_filename = "saturn";
    updater_filename = "updater";
#ifdef WINDOWS
    saturn_dir = std::string(std::getenv("HOMEPATH")) + "/AppData/Roaming/v64saturn";
    executable_filename += ".exe";
    updater_filename += ".exe";
#else
    saturn_dir = std::string(std::getenv("HOME")) + "/.local/share/v64saturn";
#endif
    std::filesystem::create_directories(saturn_dir);
    if (!std::filesystem::exists(saturn_dir / executable_filename)) {
        Downloader downloader = Downloader("https://api.github.com/repos/" REPO_OWNER "/" REPO_NAME "/releases/latest");
        downloader.download();
        current_screen = downloader.status == 200 ? SCREEN_INSTALL : SCREEN_NO_INTERNET;
        if (downloader.status != 200) return false;
        picojson::value json;
        picojson::parse(json, std::string(downloader.data.data()));
        std::ofstream out = std::ofstream(saturn_dir / "latest_update_date.txt");
        std::string publish_date = json.get("published_at").get<std::string>();
        out.write(publish_date.c_str(), publish_date.length());
        out.close();
        for (auto& asset : json.get("assets").get<picojson::array>()) {
            std::string name = asset.get("name").get<std::string>();
#ifdef WINDOWS
            if (name == "update-win64.exe") {
#else
            if (name == "update-linux64") {
#endif
                download_queue_add(asset.get("browser_download_url").get<std::string>(), (saturn_dir / executable_filename).string());
                break;
            }
        }
    }
    else {
        saturn_repair();
        bool force_update = download_queue.size() != 0;
        current_screen = SCREEN_UPDATE;
        if (!std::filesystem::exists(saturn_dir / "no_updates")) {
            Downloader downloader = Downloader("https://api.github.com/repos/" REPO_OWNER "/" REPO_NAME "/releases/latest");
            downloader.download();
            if (downloader.status == 200) {
                picojson::value json;
                picojson::parse(json, std::string(downloader.data.data()));
                release_date = json.get("published_at").get<std::string>();
                std::time_t release_time = parse_time(release_date);
                bool should_update = false;
                if (!std::filesystem::exists(saturn_dir / "latest_update_date.txt")) should_update = true;
                else {
                    int length = std::filesystem::file_size(saturn_dir / "latest_update_date.txt");
                    char* data = (char*)malloc(length);
                    std::ifstream in = std::ifstream(saturn_dir / "latest_update_date.txt");
                    in.read(data, length);
                    std::time_t current_time = parse_time(std::string(data));
                    should_update = release_time > current_time;
                    free(data);
                }
                if (should_update) {
                    for (auto& asset : json.get("assets").get<picojson::array>()) {
                        std::string name = asset.get("name").get<std::string>();
#ifdef WINDOWS
                        if (name == "update-win64.exe") {
#else
                        if (name == "update-linux64") {
#endif
                            download_queue_add(asset.get("browser_download_url").get<std::string>(), (saturn_dir / executable_filename).string());
                            break;
                        }
                    }
                }
            }
        }
        if (force_update) update_begin();
    }
    return download_queue.size() == 0;
}

bool updater() {
    switch (current_screen) {
        case SCREEN_INSTALL:
            gui_text_centered("Proceed with the installation?", 0, 20, 266, -1);
            if (gui_button("No",  266 / 2 - 3 - 64, 41, 64, 24)) return true;
            if (gui_button("Yes", 266 / 2 + 3     , 41, 64, 24)) current_screen = SCREEN_CHOOSE_ROM;
            break;
        case SCREEN_INSTALLING:
            gui_text_centered("Downloading Saturn...", 0, 5, 266, -1);
            gui_progress(5, 26, 256, 24, download_progress, PROGRESS_TEXT_PERCENTAGE);
            gui_progress(5, 56, 256, 24, (float)current_queue_entry / queue_entries, PROGRESS_TEXT_STEPS, queue_entries);
            break;
        case SCREEN_INSTALLED:
            gui_text_centered("Successfully installed", 0, 20, 266, -1);
            if (gui_button("Open" , 266 / 2 - 3 - 64, 41, 64, 24)) {
                should_run_saturn = true;
                return true;
            }
            if (gui_button("Close", 266 / 2 + 3     , 41, 64, 24)) return true;
            break;
        case SCREEN_INSTALL_FAILED:
            gui_text_centered("Installation failed", 0, 20, 266, -1);
            if (gui_button("OK", 266 / 2 - 32, 56, 64, 24)) return true;
            break;
        case SCREEN_UPDATE:
            gui_text_centered("There is an update available", 0, 5, 266, -1);
            should_run_saturn = true;
            if (gui_button("Update",           266 / 2 - 3 - 128, 26, 128, 24)) {
                update_begin();
            }
            if (gui_button("Don't update",     266 / 2 + 3      , 26, 128, 24)) return true;
            if (gui_button("Remind me later",  266 / 2 - 3 - 128, 56, 128, 24)) {
                std::ofstream stream = std::ofstream(saturn_dir / "latest_update_date.txt");
                stream.write(release_date.c_str(), release_date.length());
                stream.close();
                return true;
            }
            if (gui_button("Turn off updates", 266 / 2 + 3      , 56, 128, 24)) {
                std::ofstream stream = std::ofstream(saturn_dir / "no_updates");
                stream.close();
                current_screen = SCREEN_UPDATE_PREF;
            }
            break;
        case SCREEN_UPDATING:
            gui_text_centered("Updating Saturn...", 0, 5, 266, -1);
            gui_progress(5, 26, 256, 24, download_progress, PROGRESS_TEXT_PERCENTAGE);
            gui_progress(5, 56, 256, 24, (float)current_queue_entry / queue_entries, PROGRESS_TEXT_STEPS, queue_entries);
            break;
        case SCREEN_UPDATED:
            gui_text_centered("Successfully updated", 0, 20, 266, -1);
            if (gui_button("Open" , 266 / 2 - 3 - 64, 41, 64, 24)) {
                should_run_saturn = true;
                return true;
            }
            if (gui_button("Close", 266 / 2 + 3     , 41, 64, 24)) return true;
            break;
        case SCREEN_UPDATE_FAILED:
            should_run_saturn = false;
            gui_text_centered("Update failed", 0, 20, 266, -1);
            if (gui_button("OK", 266 / 2 - 32, 56, 64, 24)) return true;
            break;
        case SCREEN_UPDATE_PREF:
            gui_text_centered("You can change your update", 0, 5, 266, -1);
            gui_text_centered("preference in Saturn's settings.", 0, 26, 266, -1);
            if (gui_button("OK", 266 / 2 - 32, 56, 64, 24)) return true;
            break;
        case SCREEN_NO_INTERNET:
            gui_text_centered("Cannot connect to the internet", 0, 5, 266, -1);
            gui_text_centered("Please check your connection", 0, 26, 266, -1);
            if (gui_button("OK", 266 / 2 - 32, 56, 64, 24)) return true;
            break;
        case SCREEN_CHOOSE_ROM:
            gui_text_centered("Choose an unmodified US", 0, 5, 266, -1);
            gui_text_centered("version of Super Mario 64", 0, 26, 266, -1);
            if (gui_button("Browse...", 266 / 2 - 48, 56, 96, 24)) {
                auto file = pfd::open_file("Choose SM64 ROM", ".", { "Nintendo 64 ROM", "*.z64" });
                if (file.result().size() == 0) {
                    current_screen = SCREEN_INSTALL_FAILED;
                    return false;
                }
                if (std::filesystem::exists(saturn_dir / "sm64.z64")) std::filesystem::remove(saturn_dir / "sm64.z64");
                std::filesystem::copy(file.result()[0], saturn_dir / "sm64.z64");
                current_screen = SCREEN_INSTALLING;
                saturn_repair();
                char* updater_executable = exe_path();
                std::filesystem::remove(saturn_dir / updater_filename);
                std::filesystem::copy_file(updater_executable, saturn_dir / updater_filename);
                free(updater_executable);
                download_queue_begin([](bool success) {
                    current_screen = success ? SCREEN_INSTALLED : SCREEN_INSTALL_FAILED;
                    if (!success) return;
#ifdef WINDOWS
                    CoInitialize(nullptr);
                    IShellLink* shell_link;
                    CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&shell_link);
                    shell_link->SetPath((saturn_dir / updater_filename).string().c_str());
                    shell_link->SetIconLocation((saturn_dir / executable_filename).string().c_str(), 0);
                    shell_link->SetWorkingDirectory(saturn_dir.string().c_str());
                    IPersistFile* persist_file;
                    shell_link->QueryInterface(IID_IPersistFile, (LPVOID*)&persist_file);
                    const char* path = (std::string(std::getenv("HOMEPATH")) + "/AppData/Roaming/Microsoft/Windows/Start Menu/Programs/Saturn.lnk").c_str();
                    size_t size = strlen(path) + 1;
                    wchar_t* wa = new wchar_t[size];
                    mbstowcs(wa, path, size);
                    persist_file->Save(wa, 0);
                    persist_file->Release();
                    shell_link->Release();
                    CoUninitialize();
#else
                    system(("chmod +x \"" + (saturn_dir / executable_filename).string() + "\"").c_str());
                    std::filesystem::path desktop_path = saturn_dir.parent_path() / "applications" / "saturn.desktop";
                    std::filesystem::create_directories(desktop_path.parent_path());
                    std::ofstream desktop_stream = std::ofstream(desktop_path, std::ios::binary);
                    std::stringstream desktop_content = std::stringstream();
                    desktop_content << "[Desktop Entry]" << "\n";
                    desktop_content << "Name=Saturn" << "\n";
                    desktop_content << "Comment=A cross-platform, all-in-one machinima studio for Super Mario 64." << "\n";
                    desktop_content << "Exec=" << (saturn_dir / updater_filename).string() << "\n";
                    desktop_content << "Icon=" << (saturn_dir / "res" / "saturn-linuxicon.png").string() << "\n";
                    desktop_content << "Path=" << (saturn_dir).string() << "\n";
                    desktop_content << "Terminal=false" << "\n";
                    desktop_content << "Categories=Game" << "\n";
                    std::string str = desktop_content.str();
                    desktop_stream.write(str.data(), str.length());
                    desktop_stream.close();
#endif
                });
            }
            break;
    }
    return false;
}
