#ifndef BOOST_ENABLE_WEAK_FROM_THIS_HPP
#define BOOST_ENABLE_WEAK_FROM_THIS_HPP

#include <boost/weak_ptr.hpp>
#include <boost/shared_ptr.hpp>

namespace boost
{

template<class T>
struct enable_weak_from_this
{
    enable_weak_from_this() : ref_(static_cast<T *>(this), nopDeleter) {}

    weak_ptr<T> weak_from_this() const
    {
        return ref_;
    }

private:
    static void nopDeleter(void *) {}

    shared_ptr<T> ref_;
};

} // namespace boost

#endif // BOOST_ENABLE_WEAK_FROM_THIS_HPP