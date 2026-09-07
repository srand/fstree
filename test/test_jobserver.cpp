
#include "jobserver.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

#if !defined(_WIN32)
#include <filesystem>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {

void set_env(const char* name, const char* value) {
#if defined(_WIN32)
  _putenv_s(name, value ? value : "");
#else
  if (value) {
    ::setenv(name, value, 1);
  }
  else {
    ::unsetenv(name);
  }
#endif
}

}  // namespace

TEST(Jobserver, AuthFromEnv_Unset) {
  set_env("MAKEFLAGS", nullptr);
  EXPECT_EQ(fstree::jobserver::jobserver_auth_from_env(), "");
}

TEST(Jobserver, AuthFromEnv_NoJobserver) {
  set_env("MAKEFLAGS", "-j8 --output-sync=target");
  EXPECT_EQ(fstree::jobserver::jobserver_auth_from_env(), "");
  set_env("MAKEFLAGS", nullptr);
}

TEST(Jobserver, AuthFromEnv_Fifo) {
  set_env("MAKEFLAGS", "--jobserver-auth=fifo:/tmp/GMfifo123");
  EXPECT_EQ(fstree::jobserver::jobserver_auth_from_env(), "fifo:/tmp/GMfifo123");
  set_env("MAKEFLAGS", nullptr);
}

TEST(Jobserver, AuthFromEnv_Fds) {
  set_env("MAKEFLAGS", "--jobserver-auth=3,4");
  EXPECT_EQ(fstree::jobserver::jobserver_auth_from_env(), "3,4");
  set_env("MAKEFLAGS", nullptr);
}

TEST(Jobserver, AuthFromEnv_LegacyFds) {
  set_env("MAKEFLAGS", "--jobserver-fds=3,4");
  EXPECT_EQ(fstree::jobserver::jobserver_auth_from_env(), "3,4");
  set_env("MAKEFLAGS", nullptr);
}

TEST(Jobserver, AuthFromEnv_EmbeddedAmongFlags) {
  set_env("MAKEFLAGS", "-j --jobserver-auth=3,4 --output-sync=target -Otarget");
  EXPECT_EQ(fstree::jobserver::jobserver_auth_from_env(), "3,4");
  set_env("MAKEFLAGS", nullptr);
}

TEST(Jobserver, Create_NoConfig) {
  set_env("MAKEFLAGS", nullptr);
  EXPECT_FALSE(fstree::jobserver::create());

  set_env("MAKEFLAGS", "-j8 --output-sync");
  EXPECT_FALSE(fstree::jobserver::create());
  set_env("MAKEFLAGS", nullptr);
}

TEST(Jobserver, SetPath_UnusablePath) {
  set_env("MAKEFLAGS", nullptr);
  fstree::jobserver::set_path("/nonexistent/fstree-jobserver-fifo");
  EXPECT_FALSE(fstree::jobserver::create());
  fstree::jobserver::set_path("");
}

#if !defined(_WIN32)

TEST(Jobserver, PipeTokens) {
  int fds[2];
  ASSERT_EQ(::pipe(fds), 0);
  ASSERT_EQ(::write(fds[1], "++", 2), 2);  // two available tokens

  std::string makeflags = "--jobserver-auth=" + std::to_string(fds[0]) + "," + std::to_string(fds[1]);
  set_env("MAKEFLAGS", makeflags.c_str());

  fstree::jobserver::ptr js = fstree::jobserver::create();
  ASSERT_TRUE(js);
  EXPECT_TRUE(js->active());

  char t1 = 0, t2 = 0, t3 = 0;
  EXPECT_TRUE(js->try_acquire(t1));
  EXPECT_TRUE(js->try_acquire(t2));
  EXPECT_FALSE(js->try_acquire(t3));  // exhausted

  js->release(t1);
  char t4 = 0;
  EXPECT_TRUE(js->try_acquire(t4));  // token returned

  js.reset();
  ::close(fds[0]);
  ::close(fds[1]);
  set_env("MAKEFLAGS", nullptr);
}

TEST(Jobserver, FifoTokens) {
  std::filesystem::path path = std::filesystem::temp_directory_path() / ("fstree_js_" + std::to_string(::getpid()));
  ::unlink(path.c_str());
  ASSERT_EQ(::mkfifo(path.c_str(), 0600), 0);

  int wfd = ::open(path.c_str(), O_RDWR);  // keep the fifo open with a writer
  ASSERT_GE(wfd, 0);
  ASSERT_EQ(::write(wfd, "++", 2), 2);

  set_env("MAKEFLAGS", ("--jobserver-auth=fifo:" + path.string()).c_str());

  fstree::jobserver::ptr js = fstree::jobserver::create();
  ASSERT_TRUE(js);
  EXPECT_TRUE(js->active());

  char t1 = 0, t2 = 0, t3 = 0;
  EXPECT_TRUE(js->try_acquire(t1));
  EXPECT_TRUE(js->try_acquire(t2));
  EXPECT_FALSE(js->try_acquire(t3));

  js->release(t1);
  char t4 = 0;
  EXPECT_TRUE(js->try_acquire(t4));

  js.reset();
  ::close(wfd);
  ::unlink(path.c_str());
  set_env("MAKEFLAGS", nullptr);
}

TEST(Jobserver, SetPath_OverridesEnv) {
  std::filesystem::path path =
      std::filesystem::temp_directory_path() / ("fstree_js_path_" + std::to_string(::getpid()));
  ::unlink(path.c_str());
  ASSERT_EQ(::mkfifo(path.c_str(), 0600), 0);

  int wfd = ::open(path.c_str(), O_RDWR);  // keep the fifo open with a writer
  ASSERT_GE(wfd, 0);
  ASSERT_EQ(::write(wfd, "+", 1), 1);

  set_env("MAKEFLAGS", "-j8 --output-sync=target");  // no jobserver advertised
  fstree::jobserver::set_path(path.string());

  fstree::jobserver::ptr js = fstree::jobserver::create();
  ASSERT_TRUE(js);
  EXPECT_TRUE(js->active());

  char t1 = 0, t2 = 0;
  EXPECT_TRUE(js->try_acquire(t1));
  EXPECT_FALSE(js->try_acquire(t2));
  js->release(t1);

  js.reset();

  // An empty path restores the environment lookup.
  fstree::jobserver::set_path("");
  EXPECT_FALSE(fstree::jobserver::create());

  ::close(wfd);
  ::unlink(path.c_str());
  set_env("MAKEFLAGS", nullptr);
}

#endif  // !_WIN32
