#ifndef IDREESWATCH_ANEMOIA_SD_COMPAT_H
#define IDREESWATCH_ANEMOIA_SD_COMPAT_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define FILE_READ  0
#define FILE_WRITE 1

class File
{
public:
    File() : data_(nullptr), size_(0), position_(0), valid_(false) {}
    File(const uint8_t *data, size_t size)
        : data_(data), size_(size), position_(0), valid_(data && size) {}

    explicit operator bool() const { return valid_; }
    size_t position() const { return position_; }
    size_t size() const { return size_; }
    void close() { valid_ = false; }

    bool seek(size_t position)
    {
        if (!valid_ || position > size_) return false;
        position_ = position;
        return true;
    }

    size_t read(uint8_t *destination, size_t length)
    {
        if (!valid_ || !destination) return 0;
        size_t remaining = size_ - position_;
        if (length > remaining) length = remaining;
        memcpy(destination, data_ + position_, length);
        position_ += length;
        return length;
    }

    size_t write(const uint8_t *, size_t) { return 0; }
    size_t write(uint8_t) { return 0; }
    size_t print(const char *) { return 0; }

private:
    const uint8_t *data_;
    size_t size_;
    size_t position_;
    bool valid_;
};

class SDClass
{
public:
    void setMemory(const void *data, size_t size)
    {
        data_ = static_cast<const uint8_t *>(data);
        size_ = size;
    }

    File open(const char *, int mode = FILE_READ) const
    {
        return mode == FILE_READ ? File(data_, size_) : File();
    }

    bool exists(const char *) const { return false; }
    bool mkdir(const char *) const { return false; }
    bool remove(const char *) const { return false; }
    bool rename(const char *, const char *) const { return false; }

private:
    const uint8_t *data_ = nullptr;
    size_t size_ = 0;
};

extern SDClass SD;

#endif
