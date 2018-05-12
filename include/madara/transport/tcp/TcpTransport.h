#ifndef _MADARA_TCP_TRANSPORT_H_
#define _MADARA_TCP_TRANSPORT_H_

#include <string>

#include "madara/transport/tcp/TcpTransportReadThread.h"
#include "madara/knowledge/ThreadSafeContext.h"
#include "madara/transport/Transport.h"
#include "madara/threads/Threader.h"

namespace madara
{
  namespace transport
  {
    /**
     * @class TcpTransport
     * @brief TCP-based transport (skeleton code)
     **/
    class TcpTransport : public Base
    {
    public:
  
      enum {
        RELIABLE = 0
      };

      enum {
        ERROR_TCP_NOT_STARTED = -1,
      };

      static const int PROFILES = 1;

      /**
       * Constructor
       * @param   id   unique identifer - usually a combination of host:port
       * @param   context  knowledge context
       * @param   config   transport configuration settings
       * @param   launch_transport  whether or not to launch this transport
       **/
      TcpTransport (const std::string & id, 
        madara::knowledge::ThreadSafeContext & context, 
        TransportSettings & config, bool launch_transport);

      /**
       * Destructor
       **/
      virtual ~TcpTransport ();
      
      /**
       * Sends a list of knowledge updates to listeners
       * @param   updates listing of all updates that must be sent
       * @return  result of write operation or -1 if we are shutting down
       **/
      long send_data (const madara::knowledge::VariableReferenceMap & updates) override;
    
      /**
       * Accesses reliability setting. If this returns zero, it doesn't
       * make much sense.
       * @return  whether we are using reliable dissemination or not
       **/
      int reliability (void) const;

      /**
       * Accesses reliability setting. If this returns zero, it doesn't
       * make much sense.
       * @return  whether we are using reliable dissemination or not
       **/
      int reliability (const int & setting);
      long read (void);
      void close (void) override;
      int setup (void) override;
    protected:
    private:
      /// knowledge base for threads to use
      knowledge::KnowledgeBase          knowledge_;

      /// threads for reading knowledge updates
      threads::Threader                         read_threads_;

      std::map <std::string, ACE_INET_Addr>     addresses_;

    };
  }
}

#endif // _MADARA_TCP_TRANSPORT_H_
