#pragma once

// Linux HTTP GET for update check using libcurl.
// Runs on a worker thread; closing the editor cancels and joins.

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <curl/curl.h>

namespace oxo::linux {

class HttpGet {
public:
    HttpGet() : curl_(curl_easy_init()), stop_(false) {}
    ~HttpGet() {
        cancel();
        if (curl_) curl_easy_cleanup(curl_);
    }

    HttpGet(const HttpGet&) = delete;
    HttpGet& operator=(const HttpGet&) = delete;

    // Start a GET request. `callback` is called on the worker thread when done
    // (or cancelled). The string is empty on failure/cancel.
    void get(const std::string &url, std::function<void(std::string)> callback) {
        cancel();
        stop_ = false;
        callback_ = std::move(callback);
        url_ = url;
        thread_ = std::thread(&HttpGet::run, this);
    }

    void cancel() {
        stop_ = true;
        if (thread_.joinable()) thread_.join();
    }

private:
    static size_t writeCallback(void *contents, size_t size, size_t nmemb, std::string *out) {
        size_t total = size * nmemb;
        out->append(static_cast<const char*>(contents), total);
        return total;
    }

    void run() {
        std::string response;
        if (!curl_) {
            if (callback_) callback_("");
            return;
        }

        curl_easy_setopt(curl_, CURLOPT_URL, url_.c_str());
        curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl_, CURLOPT_TIMEOUT, 10L);
        curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl_, CURLOPT_USERAGENT, "OxoSynth2/1.0");

        CURLcode res = curl_easy_perform(curl_);

        if (!stop_ && callback_) {
            if (res == CURLE_OK) {
                long httpCode = 0;
                curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &httpCode);
                if (httpCode == 200) {
                    callback_(response);
                } else {
                    callback_("");
                }
            } else {
                callback_("");
            }
        }
    }

    CURL *curl_ = nullptr;
    std::thread thread_;
    std::atomic<bool> stop_;
    std::string url_;
    std::function<void(std::string)> callback_;
};

} // namespace oxo::linux