#ifndef OPENVPN_RELIABLE_RELSEND_H
#define OPENVPN_RELIABLE_RELSEND_H

#include <openvpn/common/types.hpp>
#include <openvpn/common/exception.hpp>
#include <openvpn/common/msgwin.hpp>
#include <openvpn/time/time.hpp>
#include <openvpn/buffer/buffer.hpp>
#include <openvpn/reliable/relcommon.hpp>

namespace openvpn {

  class ReliableSend
  {
  public:
    typedef ReliableMessageBase::id_t id_t;

    enum {
      RETRANSMIT = 2 // retransmit in N seconds if ACK not received
    };

    class Message : public ReliableMessageBase
    {
      friend class ReliableSend;

    public:
      bool ready_retransmit(const Time now) const
      {
	return defined() && now >= retransmit_at_;
      }

      Time::Duration until_retransmit(const Time now) const
      {
	Time::Duration ret;
	if (now < retransmit_at_)
	  ret = retransmit_at_ - now;
	return ret;
      }

      void reset_retransmit(const Time now)
      {
	retransmit_at_ = now + Time::Duration::seconds(RETRANSMIT);
      }

    private:
      Time retransmit_at_;
    };

    ReliableSend() : next(0) {}
    ReliableSend(const id_t span) { init(span); }

    void init(const id_t span)
    {
      next = 1;
      window_.init(next, span);
    }

    // Return the id that the object at the head of the queue
    // would have (even if it isn't defined yet).
    id_t head_id() const { return window_.head_id(); }

    // Return the ID of one past the end of the window
    id_t tail_id() const { return window_.tail_id(); }

    // Return the window size
    id_t span() const { return window_.span(); }

    // Return a reference to M object at id, throw exception
    // if id is not in current window
    Message& ref_by_id(const id_t id)
    {
      return window_.ref_by_id(id);
    }

    // Return the shortest duration for any pending retransmissions
    Time::Duration until_retransmit(const Time now)
    {
      Time::Duration ret = Time::Duration::infinite();
      for (id_t i = head_id(); i < tail_id(); ++i)
	{
	  const Message& msg = ref_by_id(i);
	  if (msg.defined())
	    {
	      Time::Duration ut = msg.until_retransmit(now);
	      if (ut < ret)
		ret = ut;
	    }
	}
      return ret;
    }

    // Return a fresh Message object that can be used to
    // construct the next packet in the sequence.  Don't call
    // unless ready() returns true.
    Message& send(const Time now)
    {
      Message& msg = window_.ref_by_id(next);
      msg.id_ = next++;
      msg.reset_retransmit(now);
      return msg;
    }

    // Return true if output buffer is ready to receive another packet
    bool ready() const { return window_.in_window(next); }

    // Remove a packet from send buffer that has been acknowledged
    void ack(const id_t id) { window_.rm_by_id(id); }

  private:
    id_t next;
    MessageWindow<Message, id_t> window_;
  };

} // namespace openvpn

#endif // OPENVPN_RELIABLE_RELSEND_H