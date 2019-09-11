#pragma once

#include <new>
#include <atomic>
#include <utility>
#include "Threading/SpinLock.h"

class SequenceAllocator {
public:
    SequenceAllocator() {
        _Current = _Root = new Block();
    }

    void AddBlock() {
        _OomLock.Enter();
        if (_Head > 4u << 20u) {
            auto blk = new Block();
            _Current->Next = blk;
            _Current = blk;
            _Head = sizeof(Block*);
        }
        _OomLock.Leave();
    }

    void* Allocate(int size) {
        if (size <= 1u << 18u) {
            for (;;) {
                if (auto last = _Head.fetch_add(size)>4u << 20u) {
                    AddBlock();
                }
                else {
                    return reinterpret_cast<char*>(_Current) + last - size;
                }
            }
        }
        return nullptr; // too large
    }

    ~SequenceAllocator() {
        for (auto cur = _Root; cur;) {
            auto ths = cur;
            cur = cur->Next;
            delete ths;
        }
    }
private:
    struct Block {
        union {
            Block* Next = nullptr;
            std::aligned_storage_t<4u << 20u, alignof(std::max_align_t)> __UN_MEMORY__;
        };
    };

    std::atomic_int _Head{sizeof(Block*)};
    SpinLock _OomLock;
    Block* _Root, *_Current;
};
