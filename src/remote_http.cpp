#ifdef FSTREE_ENABLE_HTTP_REMOTE

#include "remote_http.hpp"

#include "exception.hpp"
#include "filesystem.hpp"
#include "hash.hpp"
#include "url.hpp"

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <stdexcept>

#include <curl/curl.h>

namespace fstree {

namespace {

// File that downloaded data is written to, and the error that stopped it
struct DownloadFile {
  FILE* file;
  int error;
};

// Callback to write data to a file. Returns the number of bytes written,
// so that curl aborts the transfer if they were not all written.
size_t FileWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
  DownloadFile* download = static_cast<DownloadFile*>(userp);
  size_t written = fwrite(contents, 1, size * nmemb, download->file);
  if (written != size * nmemb) {
    download->error = errno;
  }
  return written;
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

// An easy handle on loan from the remote's pool, returned when destroyed.
class remote_http::handle {
 public:
  explicit handle(remote_http& remote) : _remote(remote), _handle(nullptr) {
    {
      std::lock_guard<std::mutex> lock(_remote._handles_mutex);
      if (!_remote._handles.empty()) {
        _handle = static_cast<CURL*>(_remote._handles.back());
        _remote._handles.pop_back();
      }
    }

    if (!_handle) {
      _handle = curl_easy_init();
      if (!_handle) {
        throw std::runtime_error("Failed to initialize CURL");
      }
    }
  }

  ~handle() {
    // Resetting clears the options of the previous request but keeps the
    // handle's connections open for the next one.
    curl_easy_reset(_handle);

    std::lock_guard<std::mutex> lock(_remote._handles_mutex);
    _remote._handles.push_back(_handle);
  }

  handle(const handle&) = delete;
  handle& operator=(const handle&) = delete;

  CURL* get() const { return _handle; }

 private:
  remote_http& _remote;
  CURL* _handle;
};

remote_http::remote_http(const url& remote_url) : _remote_url(remote_url) { curl_global_init(CURL_GLOBAL_DEFAULT); }

remote_http::~remote_http() {
  for (void* handle : _handles) {
    curl_easy_cleanup(static_cast<CURL*>(handle));
  }
  curl_global_cleanup();
}

// Returns true if the object with the given hash is present in the remote.
bool remote_http::has_object(const fstree::digest& hash) { return head(hash); }

// Finds trees and objects that are missing in the remote.
void remote_http::has_tree(const std::vector<fstree::digest>&, const missing_callback&) {
  throw fstree::unsupported_operation("remote_http::has_tree is not implemented");
}

// Returns a vector of booleans indicating the presence of objects with the given hashes.
void remote_http::has_objects(const std::vector<fstree::digest>& hashes, std::vector<bool>& presence) {
  presence.clear();
  presence.reserve(hashes.size());

  for (const auto& hash : hashes) {
    presence.push_back(has_object(hash));
  }
}

// Send the object with the given hash and path to the remote.
void remote_http::write_object(const fstree::digest& hash, const std::filesystem::path& path) {
  std::string url = url_path(hash);
  handle curl(*this);

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
    throw std::runtime_error("failed to upload object: " + hash.string() + ": CURL error: " + curl_easy_strerror(res));
  }
  long response_code;
  curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &response_code);
  if (response_code != 200 && response_code != 201) {
    throw std::runtime_error("failed to upload object: " + hash.string() + ": HTTP " + std::to_string(response_code));
  }
}

// Read the object with the given hash from the remote and write it to the given path.
// The temp path is used to store the object temporarily before moving it to the final path.
void remote_http::read_object(
    const fstree::digest& hash, const std::filesystem::path& path, const std::filesystem::path& temp) {
  std::string url = url_path(hash);
  handle curl(*this);

  // Create a temporary file using mkstemp
  std::filesystem::path temp_path = temp;
  FILE* outfile = fstree::mkstemp(temp_path);
  if (!outfile) {
    std::filesystem::remove(temp_path);
    throw std::runtime_error("failed to create temporary file: " + temp_path.string() + ": " + std::strerror(errno));
  }

  curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, FileWriteCallback);
  DownloadFile download{outfile, 0};
  curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &download);
  curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);

  CURLcode res = curl_easy_perform(curl.get());
  // Closing flushes buffered data, which can fail too
  if (fclose(outfile) != 0 && res == CURLE_OK) {
    download.error = errno;
    res = CURLE_WRITE_ERROR;
  }

  if (res == CURLE_WRITE_ERROR && download.error != 0) {
    std::filesystem::remove(temp_path);
    throw std::runtime_error(
        "failed to write downloaded object: " + temp_path.string() + ": " + std::strerror(download.error));
  }

  if (res != CURLE_OK) {
    std::filesystem::remove(temp_path);
    throw std::runtime_error("failed to download object: " + hash.string() + ": CURL error: " + curl_easy_strerror(res));
  }

  long response_code;
  curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &response_code);
  if (response_code != 200) {
    std::filesystem::remove(temp_path);
    throw std::runtime_error("failed to download object: " + hash.string() + ": HTTP " + std::to_string(response_code));
  }

  // A plain HTTP server stores whatever it is given and a download can end
  // early, so check the content against its digest before it enters the cache.
  // Every later checkout trusts an object found there. Only digests of the
  // algorithm in use can be computed here.
  if (hash.alg() == fstree::hash_function) {
    fstree::digest actual = hashsum_hex_file(temp_path);
    if (!(actual == hash)) {
      std::filesystem::remove(temp_path);
      throw std::runtime_error("downloaded object does not match its digest: " + hash.string() + ": got " + actual.string());
    }
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
    std::error_code remove_ec;
    std::filesystem::remove(temp_path, remove_ec);
    throw std::runtime_error("failed to rename temporary file: " + temp_path.string() + ": " + ec.message());
  }
}

std::string remote_http::url_path(const fstree::digest& hash) const {
  const std::string& hex = hash.hexdigest();
  return _remote_url.string() + "/" + hex.substr(0, 2) + "/" + hex.substr(2, 4) + "/" + hex.substr(6);
}

bool remote_http::head(const fstree::digest& hash) {
  try {
    std::string url = url_path(hash);
    handle curl(*this);

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
