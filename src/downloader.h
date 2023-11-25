#include <curl/curl.h>
#include <string>
#include <vector>

typedef void (*ProgressCallback)(double now, double total);

class Downloader {
private:
    ProgressCallback progress_callback = nullptr;
    std::string _url;
    static size_t libcurl_write(void* contents, size_t size, size_t nmemb, std::vector<char>* buffer) {
        size_t totalSize = size * nmemb;
        buffer->insert(buffer->end(), (char*)contents, (char*)contents + totalSize);
        return totalSize;
    }
    static int libcurl_progress(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
        Downloader* downloader = (Downloader*)clientp;
        if (downloader->progress_callback != nullptr) downloader->progress_callback(dlnow, dltotal);
        return 0;
    }
public:
    int status = -1;
    std::vector<char> data = {};
    Downloader(std::string url) {
        _url = url;
    }
    void progress(ProgressCallback callback) {
        progress_callback = callback;
    }
    void download() {
        data.clear();
        CURL* curl = curl_easy_init();
        curl_easy_setopt(curl, CURLOPT_URL, _url.c_str());
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, &libcurl_progress);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &libcurl_write);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
        struct curl_slist* header = nullptr;
        header = curl_slist_append(header, "Accept: */*");
        header = curl_slist_append(header, "User-Agent: saturn-updater");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
        curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        curl_easy_cleanup(curl);
        curl_slist_free_all(header);
    }
};
