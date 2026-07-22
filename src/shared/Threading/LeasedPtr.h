#ifndef MANGOS_LEASED_PTR_H
#define MANGOS_LEASED_PTR_H

#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <utility>

// Publishes a non-owning pointer while preventing its pointee from being deleted
// during an active use. A Lease never holds the state mutex while caller code runs.
template <class T>
class LeasedPtr
{
public:
    class Lease
    {
    public:
        Lease() = default;
        ~Lease() { release(); }

        Lease(Lease const&) = delete;
        Lease& operator=(Lease const&) = delete;

        Lease(Lease&& other) noexcept
            : m_owner(other.m_owner), m_value(other.m_value)
        {
            other.m_owner = nullptr;
            other.m_value = nullptr;
        }

        Lease& operator=(Lease&& other) noexcept
        {
            if (this != &other)
            {
                release();
                m_owner = other.m_owner;
                m_value = other.m_value;
                other.m_owner = nullptr;
                other.m_value = nullptr;
            }
            return *this;
        }

        T* get() const { return m_value; }
        T* operator->() const { return m_value; }
        explicit operator bool() const { return m_value != nullptr; }

    private:
        friend class LeasedPtr<T>;

        Lease(LeasedPtr* owner, T* value) : m_owner(owner), m_value(value) {}

        void release()
        {
            if (!m_owner)
                return;
            m_owner->releaseLease();
            m_owner = nullptr;
            m_value = nullptr;
        }

        LeasedPtr* m_owner = nullptr;
        T* m_value = nullptr;
    };

    Lease acquire()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_value)
            return {};
        ++m_active;
        return Lease(this, m_value);
    }

    void publish(T* value)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        assert(!m_value || m_value == value);
        m_value = value;
    }

    void detach()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_value = nullptr;
    }

    void detachAndWait()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_value = nullptr;
        m_drained.wait(lock, [&] { return m_active == 0; });
    }

    void waitForNoLeases()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_drained.wait(lock, [&] { return m_active == 0; });
    }

private:
    void releaseLease()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        assert(m_active != 0);
        --m_active;
        if (m_active == 0)
            m_drained.notify_all();
    }

    std::mutex m_mutex;
    std::condition_variable m_drained;
    T* m_value = nullptr;
    std::size_t m_active = 0;
};

#endif
