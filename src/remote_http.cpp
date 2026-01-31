#ifdef FSTREE_ENABLE_HTTP_REMOTE

#include "remote_http.hpp"

#include "exception.hpp"
#include "filesystem.hpp"
#include "url.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include <curl/curl.h>

namespace fstree {

namespace {

// RAII wrapper for CURL
class CURLHandle {
 public:
  CURLHandle() : handle_(curl_easy_init()) {
    if (!handle_) {
      throw std::runtime_error("Failed to initialize CURL");
    }
  }

  ~CURLHandle() {
    if (handle_) {
      curl_easy_cleanup(handle_);
    }
  }

  CURL* get() const { return handle_; }

 private:
  CURL* handle_;
};

// Callback to write data to a file
size_t FileWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
  FILE* file = static_cast<FILE*>(userp);
  fwrite(contents, size, nmemb, file);
  return size * nmemb;
}

// Callback to read data from a file
size_t FileReadCallback(char* ptr, size_t size, size_t nmemb, void* userp) {
  std::ifstream* file = static_cast<std::ifstream*>(userp);
  file->read(ptr, size * nmemb);
  return file->gcount();
}

// Callback to discard data (suppress output)
size_t WriteToNothing(void* contents, size_t size, size_t nmemb, void* userp) {
  (void)contents;
  (void)userp;
  return size * nmemb;  // Return the number of bytes "written" to discard them
}
}  // namespace

remote_http::remote_http(const url& remote_url) : _remote_url(remote_url) { curl_global_init(CURL_GLOBAL_DEFAULT); }

remote_http::~remote_http() { curl_global_cleanup(); }

// Returns true if the object with the given hash is present in the remote.
bool remote_http::has_object(const std::string& hash) { return head(hash); }

// Returns lists of trees and objects that are missing in the remote.
// The missing_trees and missing_objects vectors are filled with the hashes of the missing trees and objects.
void remote_http::has_tree(const std::string&, std::vector<std::string>&, std::vector<std::string>&) {
  throw fstree::unsupported_operation("remote_http::has_tree is not implemented");
}

// Returns a vector of booleans indicating the presence of objects with the given hashes.
void remote_http::has_objects(const std::vector<std::string>& hashes, std::vector<bool>& presence) {
  presence.clear();
  presence.reserve(hashes.size());

  for (const auto& hash : hashes) {
    presence.push_back(has_object(hash));
  }
}

// Send the object with the given hash and path to the remote.
void remote_http::write_object(const std::string& hash, const std::filesystem::path& path) {
  std::string url = url_path(hash);
  CURLHandle curl;

  // Open input file
  std::ifstream infile(path, std::ios::binary);
  if (!infile.is_open()) {
    throw std::runtime_error("failed to open file for reading: " + path.string());
  }

  // Get file size
  infile.seekg(0, std::ios::end);
  long file_size = infile.tellg();
  infile.seekg(0, std::ios::beg);

  curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl.get(), CURLOPT_UPLOAD, 1L);
  curl_easy_setopt(curl.get(), CURLOPT_READFUNCTION, FileReadCallback);
  curl_easy_setopt(curl.get(), CURLOPT_READDATA, &infile);
  curl_easy_setopt(curl.get(), CURLOPT_INFILESIZE_LARGE, (curl_off_t)file_size);
  curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);

  curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 1L);
  curl_easy_setopt(curl.get(), CURLOPT_VERBOSE, 0L);
  curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, WriteToNothing);
  curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, nullptr);

  CURLcode res = curl_easy_perform(curl.get());
  infile.close();
  if (res != CURLE_OK) {
    throw std::runtime_error("failed to upload object: " + hash + ": CURL error: " + curl_easy_strerror(res));
  }
  long response_code;
  curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &response_code);
  if (response_code != 200 && response_code != 201) {
    throw std::runtime_error("failed to upload object: " + hash + ": HTTP " + std::to_string(response_code));
  }
}

// Read the object with the given hash from the remote and write it to the given path.
// The temp path is used to store the object temporarily before moving it to the final path.
void remote_http::read_object(
    const std::string& hash, const std::filesystem::path& path, const std::filesystem::path& temp) {
  std::string url = url_path(hash);
  CURLHandle curl;

  // Create a temporary file using mkstemp
  std::filesystem::path temp_path = temp;
  FILE* outfile = fstree::mkstemp(temp_path);
  if (!outfile) {
    std::filesystem::remove(temp_path);
    throw std::runtime_error("failed to create temporary file: " + temp_path.string() + ": " + std::strerror(errno));
  }

  curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, FileWriteCallback);
  curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, outfile);
  curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);

  CURLcode res = curl_easy_perform(curl.get());
  fclose(outfile);

  if (res != CURLE_OK) {
    std::filesystem::remove(temp_path);
    throw std::runtime_error("failed to download object: " + hash + ": CURL error: " + curl_easy_strerror(res));
  }

  long response_code;
  curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &response_code);
  if (response_code != 200) {
    std::filesystem::remove(temp_path);
    throw std::runtime_error("failed to download object: " + hash + ": HTTP " + std::to_string(response_code));
  }

  // Move the temporary file to the final path
  std::error_code ec;
  if (!std::filesystem::exists(path.parent_path(), ec)) {
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
      std::filesystem::remove(temp_path);
      throw std::runtime_error("failed to create directory: " + path.parent_path().string() + ": " + ec.message());
    }
  }

  std::filesystem::rename(temp_path, path, ec);
  if (ec) {
    // std::filesystem::remove(temp_path);
    throw std::runtime_error("failed to rename temporary file: " + temp_path.string() + ": " + ec.message());
  }
}

std::string remote_http::url_path(const std::string& hash) {
  return _remote_url.string() + "/" + hash.substr(0, 2) + "/" + hash.substr(2, 6) + "/" + hash.substr(6);
}

bool remote_http::head(const std::string& hash) {
  try {
    std::string url = url_path(hash);
    CURLHandle curl;

    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_NOBODY, 1L);  // HEAD request
    curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);

    // Completely suppress all output
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, WriteToNothing);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, nullptr);
    curl_easy_setopt(curl.get(), CURLOPT_HEADERFUNCTION, WriteToNothing);
    curl_easy_setopt(curl.get(), CURLOPT_HEADERDATA, nullptr);
    curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl.get(), CURLOPT_VERBOSE, 0L);

    CURLcode res = curl_easy_perform(curl.get());
    if (res != CURLE_OK) {
      return false;
    }

    long response_code;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &response_code);
    return response_code == 200;
  }
  catch (...) {
    return false;
  }
}

}  // namespace fstree

#endif // FSTREE_ENABLE_HTTP_REMOTE
