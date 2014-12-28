#include "Worker_Thread.h"
#include "ace/High_Res_Timer.h"
#include "ace/OS_NS_sys_time.h"

#ifdef _MADARA_JAVA_
#include <jni.h>
#include "madara_jni.h"
#endif

#ifdef WIN32

#include <process.h>

unsigned __stdcall worker_thread_windows_glue (void * param)
{
  Madara::Threads::Worker_Thread * caller = 
    static_cast < Madara::Threads::Worker_Thread *> (
      param);
  if (caller)
  {
    return (unsigned) caller->svc ();
  }
  else
  {
    return 0;
  }
}

#endif


#include <iostream>
#include <algorithm>

Madara::Threads::Worker_Thread::Worker_Thread ()
  : thread_ (0), control_ (0), data_ (0), hertz_ (-1.0)
{
}


Madara::Threads::Worker_Thread::Worker_Thread (
  const std::string & name,
  Base_Thread * thread,
  Knowledge_Engine::Knowledge_Base * control,
  Knowledge_Engine::Knowledge_Base * data,
  double hertz)
  : name_ (name), thread_ (thread), control_ (control), data_ (data),
    hertz_ (hertz)
{
  if (thread && control)
  {
    std::stringstream base_string;
    base_string << name;
    
    thread->name = name;
    thread->init_control_vars (*control);

    finished_.set_name (
      base_string.str () + ".finished", *control);
    started_.set_name (
      base_string.str () + ".started", *control);

    finished_ = 0;
    started_ = 0;
  }
}

Madara::Threads::Worker_Thread::Worker_Thread (const Worker_Thread & input)
  : name_ (input.name_), thread_ (input.thread_),
    control_ (input.control_), data_ (input.data_),
    finished_ (input.finished_), started_ (input.started_),
    hertz_ (input.hertz_)
{
}

Madara::Threads::Worker_Thread::~Worker_Thread ()
{
}

void
Madara::Threads::Worker_Thread::operator= (const Worker_Thread & input)
{
  if (this != &input)
  {
    this->name_ = input.name_;
    this->thread_ = input.thread_;
    this->control_ = input.control_;
    this->data_ = input.data_;
    this->finished_ = input.started_;
    this->started_ = input.started_;
    this->hertz_ = input.hertz_;
  }
}

void
Madara::Threads::Worker_Thread::run (void)
{
  int result;

#ifndef WIN32
  result = this->activate ();
#else
  result = 0;
  _beginthreadex(NULL, 0, worker_thread_windows_glue, (void*)this, 0, 0);
    
#endif

  if (result != -1)
  {
    MADARA_DEBUG (MADARA_LOG_MAJOR_EVENT, (LM_DEBUG, 
      DLINFO "Worker_Thread::Worker_Thread:" \
      " read thread started (result = %d)\n", result));
  }
  else
  {
    MADARA_DEBUG (MADARA_LOG_MAJOR_EVENT, (LM_DEBUG, 
      DLINFO "Worker_Thread::Worker_Thread:" \
      " failed to create thread. ERRNO = %d\n", ACE_OS::last_error ()));
  }
}

int
Madara::Threads::Worker_Thread::svc (void)
{
  MADARA_DEBUG (MADARA_LOG_MAJOR_EVENT, (LM_DEBUG, 
    DLINFO "Worker_Thread::svc:" \
    " checking thread existence\n"));

  if (thread_)
  {
    started_ = 1;

    thread_->init (*data_);

    {
      ACE_Time_Value current = ACE_High_Res_Timer::gettimeofday ();
      ACE_Time_Value next_epoch, poll_frequency;
      
      bool one_shot = true;
      bool blaster = false;

      std::string terminated (name_ + ".terminated");
      std::string paused (name_ + ".paused");

      if (hertz_ > 0.0)
      {
        MADARA_DEBUG (MADARA_LOG_MAJOR_EVENT, (LM_DEBUG, 
          DLINFO "Worker_Thread::svc:" \
          " %s thread repeating at %f hz\n", name_.c_str (), hertz_));

        one_shot = false;
        poll_frequency.set (1.0 / hertz_);
        next_epoch = current + poll_frequency;
      }
      else if (hertz_ == 0.0)
      {
        // infinite hertz until terminate
        
        MADARA_DEBUG (MADARA_LOG_MAJOR_EVENT, (LM_DEBUG, 
          DLINFO "Worker_Thread::svc:" \
          " %s thread blasting at infinite hz\n", name_.c_str ()));

        one_shot = false;
        blaster = true;
      }
      else
      {
        MADARA_DEBUG (MADARA_LOG_MAJOR_EVENT, (LM_DEBUG, 
          DLINFO "Worker_Thread::svc:" \
          " %s thread running once\n", name_.c_str ()));
      }

      while (control_->evaluate (terminated).is_false ())
      {
        MADARA_DEBUG (MADARA_LOG_MAJOR_EVENT, (LM_DEBUG, 
          DLINFO "Worker_Thread::svc:" \
          " %s thread checking for pause\n", name_.c_str ()));

        if (control_->evaluate (paused).is_false ())
        {
          MADARA_DEBUG (MADARA_LOG_MAJOR_EVENT, (LM_DEBUG, 
            DLINFO "Worker_Thread::svc:" \
            " %s thread calling run function\n", name_.c_str ()));

          thread_->run ();
        }

        if (one_shot)
          break;

        if (!blaster)
        {
          current = ACE_High_Res_Timer::gettimeofday ();
          
          MADARA_DEBUG (MADARA_LOG_MAJOR_EVENT, (LM_DEBUG, 
            DLINFO "Worker_Thread::svc:" \
            " %s thread checking for next hertz epoch\n", name_.c_str ()));

          if (current < next_epoch)
            Madara::Utility::sleep (next_epoch - current);  
          
          MADARA_DEBUG (MADARA_LOG_MAJOR_EVENT, (LM_DEBUG, 
            DLINFO "Worker_Thread::svc:" \
            " %s thread past epoch\n", name_.c_str ()));

          next_epoch += poll_frequency;
        }
      }
    }

    thread_->cleanup ();
    
    MADARA_DEBUG (MADARA_LOG_MAJOR_EVENT, (LM_DEBUG, 
      DLINFO "Worker_Thread::svc:" \
      " deleting thread %s)\n", name_.c_str ()));

    delete thread_;
    
    MADARA_DEBUG (MADARA_LOG_MAJOR_EVENT, (LM_DEBUG, 
      DLINFO "Worker_Thread::svc:" \
      " setting %s to 1)\n", finished_.get_name ().c_str ()));

    finished_ = 1;

#ifdef _MADARA_JAVA_
    // try detaching one more time, just to make sure.
    ::jni_detach ();
#endif
  }
  else
  {
    MADARA_DEBUG (MADARA_LOG_MAJOR_EVENT, (LM_DEBUG, 
      DLINFO "Worker_Thread::svc:" \
      " thread creation failed\n"));
  }

  return 0;
}
