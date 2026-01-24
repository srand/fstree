#pragma once

#include <atomic>
#include <cstddef>

namespace fstree {

#ifndef NDEBUG
void increment_objects_in_use();
void decrement_objects_in_use();
void report_leaks();
#endif  // NDEBUG

template<typename T>
class intrusive_ptr;

template<typename T>
class intrusive_ptr_base {
public:
    virtual ~intrusive_ptr_base() {
#ifndef NDEBUG
        decrement_objects_in_use();
#endif
    }

    void add_ref() {
        _refcount.fetch_add(1, std::memory_order_relaxed);
    }

    void release_ref() {
        if (_refcount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            delete this;
        }
    }

protected:
    intrusive_ptr_base() {
#ifndef NDEBUG
        increment_objects_in_use();
#endif
    }

    intrusive_ptr<T> intrusive_from_this() {
        return intrusive_ptr<T>(static_cast<T*>(this));
    }

private:
    std::atomic<unsigned> _refcount{0};
};

template<typename T>
class intrusive_ptr {
 private:
  T* _ptr = nullptr;

  public:
    intrusive_ptr() = default;

    intrusive_ptr(std::nullptr_t) : _ptr(nullptr) {}

    explicit intrusive_ptr(T* ptr) : _ptr(ptr) {
        if (_ptr) {
            _ptr->add_ref();
        }
    }

    intrusive_ptr(const intrusive_ptr& other) : _ptr(other._ptr) {
        if (_ptr) {
            _ptr->add_ref();
        }
    }

    intrusive_ptr(intrusive_ptr&& other) noexcept : _ptr(other._ptr) {
        other._ptr = nullptr;
    }

    ~intrusive_ptr() {
        if (_ptr) {
            _ptr->release_ref();
        }
    }

    intrusive_ptr& operator=(const intrusive_ptr& other) {
        if (this != &other) {
            if (_ptr) {
                _ptr->release_ref();
            }
            _ptr = other._ptr;
            if (_ptr) {
                _ptr->add_ref();
            }
        }
        return *this;
    }

    intrusive_ptr& operator=(intrusive_ptr&& other) noexcept {
        if (this != &other) {
            if (_ptr) {
                _ptr->release_ref();
            }
            _ptr = other._ptr;
            other._ptr = nullptr;
        }
        return *this;
    }

    bool operator==(const intrusive_ptr& other) const {
        return _ptr == other._ptr;
    }

    bool operator!=(const intrusive_ptr& other) const {
        return _ptr != other._ptr;
    }

    void reset() {
        if (_ptr) {
            _ptr->release_ref();
            _ptr = nullptr;
        }
    }

    T* get() const {
        return _ptr;
    }

    T& operator*() const {
        return *_ptr;
    }

    T* operator->() const {
        return _ptr;
    }

    explicit operator bool() const {
        return _ptr != nullptr;
    }
};

template <typename T, typename... Args>
intrusive_ptr<T> make_intrusive(Args&&... args) {
    return intrusive_ptr<T>(new T(std::forward<Args>(args)...));
}

} // namespace fstree
