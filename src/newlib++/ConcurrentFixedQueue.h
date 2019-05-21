//
// Created by Carl Timmer on 2019-05-14.
//

#ifndef EVIO_6_0_CONCURRENTFIXEDQUEUE_H
#define EVIO_6_0_CONCURRENTFIXEDQUEUE_H

#include <mutex>
#include <queue>
#include <condition_variable>

using namespace std;

template<typename Data>

/**
 * Class implementing a fixed size queue which is thread safe.
 * @tparam Data Type of data in queue.
 */
class ConcurrentFixedQueue {

private:

    /** Maximum number of elements in queue. */
    size_t maxSize;
    /** Vector containing queue elements. */
    queue<Data> q;
    /** Mutex. */
    mutable mutex mtx;
    /** Condition variable. */
    condition_variable cv;

public:

    /** Constructor. */
    explicit ConcurrentFixedQueue(size_t size) {
        maxSize = size;
        q();
        mtx();
        cv();
    }

    /** Move assignment operator which moves the contents of other to lhs. */
    ConcurrentFixedQueue & operator=(ConcurrentFixedQueue&& other) noexcept {
        if (this == &other) {
            other.mtx.lock();
            q = std::move(other.q);
            maxSize = other.maxSize;
            other.mtx.unlock();
        }
        return *this;
    }

    /** Copy assignment operator. */
    ConcurrentFixedQueue & operator=(const ConcurrentFixedQueue& other) {
        // Note: mutexes and cvs cannot be copied or moved
        if (this == &other) {
            other.mtx.lock();
            q = other.q;
            maxSize = other.maxSize;
            other.mtx.unlock();
        }
        return *this;
    }

    /**
     * Get the maximum number of elements in this queue.
     * @return maximum number of elements in this queue.
     */
    size_t getMaxSize() const {
        return maxSize;
    }

    /**
     * Get the current number of elements in this queue.
     * @return current number of elements in this queue.
     */
    size_t getSize() const {
        mtx.lock();
        size_t size = q.size();
        mtx.unlock();
        return size;
    }

    /**
      * Is the queue empty?
      * @return true if queue empty, else false;
      */
    bool isEmpty() const {
        mtx.lock();
        bool empty = q.empty();
        mtx.unlock();
        return empty;
    }

    /**
     * Is the queue full?
      * @return true if queue full, else false;
     */
    bool isFull() const {
        mtx.lock();
        bool full = (q.size() >= maxSize);
        mtx.unlock();
        return full;
    }

    /**
     * Add element to queue if not full.
     * @param data element to add to queue.
     * @return false if queue full and data not added, else true.
     */
    bool push(Data const& data) {
        mtx.lock();
        if (q.size() >= maxSize) {
            mtx.unlock();
            return false;
        }
        q.push(data);
        mtx.unlock();
        cv.notify_one();
        return true;
    }

    /**
     * Try to pop from this queue. Returns immediately if queue empty.
     * @param popped_value returned queue item.
     * @return true if returned value is valid, false if queue is empty.
     */
    bool tryPop(Data& popped_value) {
        mtx.lock();
        if (q.empty()) {
            mtx.unlock();
            return false;
        }

        popped_value = q.front();
        q.pop();
        mtx.unlock();
        return true;
    }

    /**
      * Pop from this queue. Blocks until queue item available.
      * @param popped_value returned queue item.
      */
    void waitPop(Data& popped_value) {
        mtx.lock();
        while(q.empty()) {
            cv.wait(lock);
        }

        popped_value = q.front();
        q.pop();
        mtx.unlock();
    }

    /**
     * Timed pop from this queue.
     * @param popped_value returned queue item.
     * @param millisec number of milliseconds to wait
     * @return true if returned value is valid, false if timeout.
     */
    bool waitPop(Data& popped_value, uint32_t millisec) {
        mtx.lock();
        while (q.empty()) {
            if (cv.wait_for(lock, millisec * 1ms) == std::cv_status::timeout) {
                mtx.unlock();
                return false;
            }
        }

        popped_value = q.front();
        q.pop();
        mtx.unlock();
        return true;
    }

};


#endif //EVIO_6_0_CONCURRENTFIXEDQUEUE_H
