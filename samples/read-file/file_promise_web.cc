#include <emscripten/fetch.h>

#include "file_promise.h"

namespace {

struct Request {
  Request(
      std::shared_ptr<igasync::Promise<igasync::sample::FilePromise::result_t>>
          l)
      : fpp(l) {}

  std::shared_ptr<igasync::Promise<igasync::sample::FilePromise::result_t>> fpp;
};

struct RaiiShield {
  ::Request* ptr;

  RaiiShield(::Request* p) : ptr(p) {}
  ~RaiiShield() {
    if (ptr) delete ptr;
  }
};

struct RaiiGuardFetch {
  emscripten_fetch_t* f;

  RaiiGuardFetch(emscripten_fetch_t* ff) : f(ff) {}
  ~RaiiGuardFetch() {
    if (f) {
      emscripten_fetch_close(f);
    }
  }
};

void em_fetch_success(emscripten_fetch_t* fetch) {
  if (!fetch->userData) {
    return;
  }

  ::Request* r = reinterpret_cast<::Request*>(fetch->userData);

  ::RaiiShield rm(r);
  ::RaiiGuardFetch fg(fetch);

  std::string data(fetch->numBytes, '\0');
  memcpy(&data[0], fetch->data, fetch->numBytes);
  r->fpp->resolve(std::move(data));
}

void em_fetch_error(emscripten_fetch_t* fetch) {
  if (!fetch->userData) {
    return;
  }

  ::Request* r = reinterpret_cast<::Request*>(fetch->userData);

  ::RaiiShield rm(r);
  ::RaiiGuardFetch fg(fetch);

  if (fetch->status == 404) {
    r->fpp->resolve(igasync::sample::FileReadError::FileNotFound);
  } else {
    r->fpp->resolve(igasync::sample::FileReadError::FileNotRead);
  }
}

}  // namespace

namespace igasync {
namespace sample {
std::shared_ptr<Promise<igasync::sample::FilePromise::result_t>>
FilePromise::Create(const std::string& file_name) {
  auto rsl = Promise<igasync::sample::FilePromise::result_t>::Create();

  emscripten_fetch_attr_t attr;
  emscripten_fetch_attr_init(&attr);
  strcpy(attr.requestMethod, "GET");
  attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;

  Request* r = new Request(rsl);
  attr.userData = r;
  attr.onsuccess = em_fetch_success;
  attr.onerror = em_fetch_error;

  emscripten_fetch(&attr, file_name.c_str());

  return rsl;
}
}  // namespace sample
}  // namespace igasync
