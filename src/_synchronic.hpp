/*******************************************************************************
  Copyright (c) 2016, Manuel Freiberger
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice, this
    list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/

#ifndef LOG11_SYNCHRONIC_HPP
#define LOG11_SYNCHRONIC_HPP

#include <condition_variable>
#include <mutex>

namespace log11
{
namespace log11_detail
{

template <typename T>
class synchronic
{
public:
    using atomic_type = std::atomic<T>;

    synchronic() = default;
    ~synchronic() = default;

    synchronic(const synchronic&) = delete;
    synchronic& operator=(const synchronic&) = delete;

    void notify(atomic_type& object, T value) noexcept
    {
        m_mutex.lock();
        object.store(value);
        m_mutex.unlock();
        m_cv.notify_all();
    }

    template <typename F>
    void notify(atomic_type& /*object*/, F&& func)
    {
        func();
        m_cv.notify_all();
    }

    void expect(const atomic_type& object, T desired) const noexcept
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [&] { return object.load() == desired; } );
    }

    template <typename F>
    void expect(const atomic_type& /*object*/, F&& pred) const
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, std::forward<F>(pred));
    }

private:
    mutable std::mutex m_mutex;
    mutable std::condition_variable m_cv;
};

} // log11_detail
} // namespace log11

#endif // LOG11_SYNCHRONIC_HPP
