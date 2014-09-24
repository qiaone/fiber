
//          Copyright Oliver Kowalke 2013.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "boost/fiber/recursive_mutex.hpp"

#include <algorithm>

#include <boost/assert.hpp>

#include "boost/fiber/interruption.hpp"
#include "boost/fiber/operations.hpp"

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_PREFIX
#endif

namespace boost {
namespace fibers {

bool
recursive_mutex::lock_if_unlocked_()
{
    if ( UNLOCKED == state_)
    {
        state_ = LOCKED;
        BOOST_ASSERT( ! owner_);
        owner_ = this_fiber::get_id();
        ++count_;
        return true;
    }
    else if ( this_fiber::get_id() == owner_)
    {
        ++count_;
        return true;
    }

    return false;
}

recursive_mutex::recursive_mutex() :
    splk_(),
	state_( UNLOCKED),
    owner_(),
    count_( 0),
    waiting_()
{}

recursive_mutex::~recursive_mutex()
{
    BOOST_ASSERT( ! owner_);
    BOOST_ASSERT( 0 == count_);
    BOOST_ASSERT( waiting_.empty() );
}

void
recursive_mutex::lock()
{
    detail::fiber_base * f( fm_active() );
    for (;;)
    {
        unique_lock< detail::spinlock > lk( splk_);

        if ( lock_if_unlocked_() ) return;

        // store this fiber in order to be notified later
        BOOST_ASSERT( waiting_.end() == std::find( waiting_.begin(), waiting_.end(), f) );
        waiting_.push_back( f);

        // suspend this fiber
        fm_wait( lk);
    }
}

bool
recursive_mutex::try_lock()
{
    unique_lock< detail::spinlock > lk( splk_);

    if ( lock_if_unlocked_() ) return true;

    lk.unlock();
    // let other fiber release the lock
    this_fiber::yield();
    return false;
}

void
recursive_mutex::unlock()
{
    BOOST_ASSERT( LOCKED == state_);
    BOOST_ASSERT( this_fiber::get_id() == owner_);

    unique_lock< detail::spinlock > lk( splk_);
    detail::fiber_base * f( 0);
    
    if ( 0 == --count_)
    {
        if ( ! waiting_.empty() )
        {
            f = waiting_.front();
            waiting_.pop_front();
        }
        owner_ = detail::fiber_base::id();
        state_ = UNLOCKED;
        lk.unlock();
        if ( f) f->set_ready();
    }
}

}}

#ifdef BOOST_HAS_ABI_HEADERS
#  include BOOST_ABI_SUFFIX
#endif
