#pragma once

#include <algorithm>
#include <cassert>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {}

    ~RawMemory() {
        Deallocate(buffer_);
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory(RawMemory&& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    RawMemory& operator=(RawMemory&& other) noexcept {
        if (this != &other) {
            std::swap(buffer_, other.buffer_);
            std::swap(capacity_, other.capacity_);
        }
        return *this;
    }

    T* operator+(size_t offset) noexcept {
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    const T& operator[](size_t index) const noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept { return buffer_; }
    T* GetAddress() noexcept { return buffer_; }
    size_t Capacity() const noexcept { return capacity_; }

private:
    T* buffer_ = nullptr;
    size_t capacity_ = 0;

    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }
};

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;

    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size) {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_) {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept
        : data_(std::move(other.data_))
        , size_(other.size_) {
        other.size_ = 0;
    }

    Vector& operator=(const Vector& rhs) {
        if (this == &rhs) {
            return *this;
        }

        if (rhs.size_ > data_.Capacity()) {
            Vector<T> tmp(rhs);
            Swap(tmp);
            return *this;
        }

        if (rhs.size_ < size_) {
            std::copy_n(rhs.data_.GetAddress(), rhs.size_, data_.GetAddress());
            std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
        }
        else {
            std::copy_n(rhs.data_.GetAddress(), size_, data_.GetAddress());
            std::uninitialized_copy_n(
                rhs.data_.GetAddress() + size_,
                rhs.size_ - size_,
                data_.GetAddress() + size_
            );
        }
        size_ = rhs.size_;
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            Swap(rhs);
        }
        return *this;
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    iterator begin() noexcept {
        return data_.GetAddress();
    }

    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }

    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator end() const noexcept {
        return data_.GetAddress() + size_;
    }

    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator cend() const noexcept {
        return data_.GetAddress() + size_;
    }


private:
    void MoveOrCopyRange(const T* src, T* dst, size_t count) {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(src, count, dst);
        }
        else {
            std::uninitialized_copy_n(src, count, dst);
        }
    }

public:
    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        assert(pos >= begin() && pos <= end());
        const size_t index = pos - begin();

        if (size_ == data_.Capacity()) {
            const size_t new_capacity = (size_ == 0) ? 1 : size_ * 2;
            RawMemory<T> new_data(new_capacity);


            new (new_data + index) T(std::forward<Args>(args)...);

            try {

                MoveOrCopyRange(data_.GetAddress(), new_data.GetAddress(), index);

                MoveOrCopyRange(
                    data_.GetAddress() + index,
                    new_data.GetAddress() + index + 1,
                    size_ - index
                );
            }
            catch (...) {
                std::destroy_at(new_data + index);
                throw;
            }

            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        else {
            if (index == size_) {
                new (data_ + index) T(std::forward<Args>(args)...);
            }
            else {
                T temp(std::forward<Args>(args)...);
                new (data_ + size_) T(std::move(data_[size_ - 1]));
                std::move_backward(
                    data_.GetAddress() + index,
                    data_.GetAddress() + size_ - 1,
                    data_.GetAddress() + size_
                );
                data_[index] = std::move(temp);
            }
        }

        ++size_;
        return begin() + index;
    }

    iterator Erase(const_iterator first, const_iterator last) noexcept(std::is_nothrow_move_assignable_v<T>) {
        assert(first >= begin() && last <= end() && first <= last);
        const size_t first_idx = first - begin();
        const size_t count = last - first;

        if (count == 0) {
            return begin() + first_idx;
        }

        iterator mutable_first = begin() + first_idx;
        iterator mutable_last = mutable_first + count;

        std::move(mutable_last, end(), mutable_first);
        std::destroy_n(end() - count, count);
        size_ -= count;

        return mutable_first;
    }

    iterator Erase(const_iterator pos) noexcept(std::is_nothrow_move_assignable_v<T>) {
        assert(pos >= begin() && pos < end());
        return Erase(pos, pos + 1);
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        return *Emplace(cend(), std::forward<Args>(args)...);
    }

    void PushBack(const T& value) {
        Emplace(cend(), value);
    }

    void PushBack(T&& value) {
        Emplace(cend(), std::move(value));
    }

    void PopBack() noexcept {
        assert(size_ != 0);
        std::destroy_at(data_.GetAddress() + size_ - 1);
        --size_;
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }

        RawMemory<T> new_data(new_capacity);
        MoveOrCopyRange(data_.GetAddress(), new_data.GetAddress(), size_);

        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    void Resize(size_t new_size) {
        if (new_size < size_) {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
            size_ = new_size;
        }
        else if (new_size > size_) {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
            size_ = new_size;
        }
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    const T& Back() const noexcept {
        assert(size_ != 0);
        return data_[size_ - 1];
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        assert(index < size_);
        return data_[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;
};
