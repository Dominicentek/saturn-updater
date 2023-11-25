#include <SDL2/SDL.h>
#include "gui.h"
#include "json_parser.h"
#include "portable_file_dialogs.h"
#include "main.h"
#include <cpr/cpr.h>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>

#ifdef WINDOWS
#include <Windows.h>
#include <Objbase.h>
#include <Shobjidl.h>
#endif

std::filesystem::path saturn_dir;
std::string executable_filename;

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

#define REPO_OWNER "Llennpie"
#define REPO_NAME  "Saturn"

int current_screen = 0;
float download_progress = 0;
std::ofstream download_stream;
std::string latest_download_link;
std::thread download_thread;
DownloadFinishCallback download_finish_callback;
bool init = false;
std::string release_date;

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
    system((saturn_dir / executable_filename).string().c_str());
}

void begin_download(std::string url, std::string file, DownloadFinishCallback finish_callback) {
    download_progress = 0;
    download_finish_callback = finish_callback;
    download_thread = std::thread([](std::string url, std::string file) {
        cpr::Response response = cpr::Get(cpr::Url { url }, cpr::ProgressCallback([&](cpr::cpr_off_t downloadTotal, cpr::cpr_off_t downloadNow, cpr::cpr_off_t uploadTotal, cpr::cpr_off_t uploadNow, intptr_t userdata) -> bool {
            download_progress = (float)downloadNow / (float)downloadTotal;
            return true;
        }));
        std::cout << response.status_code << " : " << url << std::endl;
        std::filesystem::create_directories(std::filesystem::path(file).parent_path());
        std::ofstream stream = std::ofstream(file);
        stream.write(response.text.data(), response.downloaded_bytes);
        stream.close();
        download_finish_callback(response.status_code == 200);
    }, url, file);
}

bool updater_init() {
    executable_filename = "saturn";
#ifdef WINDOWS
    saturn_dir = std::string(std::getenv("HOMEPATH")) + "/AppData/Roaming/v64saturn";
    executable_filename += ".exe";
#else
    saturn_dir = std::string(std::getenv("HOME")) + "/.local/share/v64saturn";
#endif
    std::filesystem::create_directories(saturn_dir);
    if (!std::filesystem::exists(saturn_dir / executable_filename)) {
        cpr::Response response = cpr::Get(cpr::Url { "https://api.github.com/repos/" REPO_OWNER "/" REPO_NAME "/releases/113142596" });
        current_screen = response.status_code == 200 ? SCREEN_INSTALL : SCREEN_NO_INTERNET;
        if (response.status_code != 200) return false;
        Json::Value json;
        json << response.text;
        latest_download_link = json["assets"][0]["browser_download_url"].asString();
        std::ofstream out = std::ofstream(saturn_dir / "latest_update_date.txt");
        std::string publish_date = json["published_at"].asString();
        out.write(publish_date.c_str(), publish_date.length());
        out.close();
    }
    else if (std::filesystem::exists(saturn_dir / "no_updates")) return true; 
    else {
        cpr::Response response = cpr::Get(cpr::Url { "https://api.github.com/repos/" REPO_OWNER "/" REPO_NAME "/releases/latest" });
        if (response.status_code != 200) return true;
        Json::Value json;
        json << response.text;
        release_date = json["published_at"].asString();
        std::time_t release_time = parse_time(release_date);
        bool should_update = false;
        latest_download_link = json["assets"][0]["browser_download_url"].asString();
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
        if (!should_update) return true;
        current_screen = SCREEN_UPDATE;
    }
    return false;
}

bool updater() {
    switch (current_screen) {
        case SCREEN_INSTALL:
            gui_text_centered("Proceed with the installation?", 0, 20, 266, -1);
            if (gui_button("No",  266 / 2 - 3 - 64, 41, 64, 24)) return true;
            if (gui_button("Yes", 266 / 2 + 3     , 41, 64, 24)) current_screen = SCREEN_CHOOSE_ROM;
            break;
        case SCREEN_INSTALLING:
            gui_text_centered("Downloading Saturn...", 0, 20, 266, -1);
            gui_progress(5, 41, 256, 24, download_progress);
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
                std::ofstream stream = std::ofstream(saturn_dir / "latest_update_date.txt");
                stream.write(release_date.c_str(), release_date.length());
                stream.close();
                begin_download(latest_download_link, (saturn_dir / executable_filename).string(), [](bool success) {
                    current_screen = success ? SCREEN_UPDATED : SCREEN_UPDATE_FAILED;
                });
                current_screen = SCREEN_UPDATING;
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
            gui_text_centered("Updating Saturn...", 0, 20, 266, -1);
            gui_progress(5, 41, 256, 24, download_progress);
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
                begin_download(latest_download_link, (saturn_dir / executable_filename).string(), [](bool success) {
                    current_screen = success ? SCREEN_INSTALLED : SCREEN_INSTALL_FAILED;
#ifdef WINDOWS
                    if (!success) return;
                    CoInitialize(nullptr);
                    IShellLink* shell_link;
                    CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&shell_link);
                    shell_link->SetPath((saturn_dir / executable_filename).string().c_str());
                    IPersistFile* persist_file;
                    shell_link->QueryInterface(IID_IPersistFile, (LPVOID*)&persist_file);
                    persist_file->Save((std::to_string(std::getenv("HOMEPATH")) + "/AppData/Roaming/Microsoft/Windows/Start Menu/Programs/Saturn.lnk").c_str());
                    persist_file->Release();
                    shell_link->Release();
                    CoUninitialize();
#endif
                });
            }
            break;
    }
    return false;
}