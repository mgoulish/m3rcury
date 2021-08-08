#include <proton/codec.h>
#include <proton/delivery.h>
#include <proton/engine.h>
#include <proton/event.h>
#include <proton/listener.h>
#include <proton/message.h>
#include <proton/proactor.h>
#include <proton/sasl.h>
#include <proton/types.h>
#include <proton/version.h>

#include <inttypes.h>
#include <memory.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>


#define MAX_NAME   100
#define MAX_ADDRS  1000
#define MAX_MESSAGE 2000000

typedef 
struct context_s 
{
  pn_link_t       * link;
  char            * path;
  char              name [ MAX_NAME ];
  char              id [ MAX_NAME ];
  char              host [ MAX_NAME ];
  size_t            message_length,
                    bytes_received;
  char              incoming_message [ MAX_MESSAGE ];
  char            * port;
  int               received;
  pn_message_t    * message;
  int               expected_messages;
  size_t            credit_window;
  pn_proactor_t   * proactor;
  pn_listener_t   * listener;
  pn_connection_t * connection;
  double            start_time,
                    stop_time;
}
context_t,
* context_p;



context_p context_g = 0;



static
double
get_timestamp ( void )
{
  struct timeval t;
  gettimeofday ( & t, 0 );
  return t.tv_sec + ((double) t.tv_usec) / 1000000.0;
}



void 
halt ( context_p context )
{
  if ( context->connection )
    pn_connection_close(context->connection);
  if ( context->listener )
    pn_listener_close(context->listener);
}



size_t
decode_message ( context_p context, pn_delivery_t * delivery ) 
{
  size_t len = 0;

  pn_message_t * msg  = context->message;
  pn_link_t    * link = pn_delivery_link ( delivery );
  ssize_t        incoming_size = pn_delivery_pending ( delivery );


  pn_link_recv ( link, context->incoming_message, incoming_size);
  pn_message_clear ( msg );

  if ( pn_message_decode ( msg, context->incoming_message, incoming_size ) ) 
  {
    exit ( 2 );
  }
  else
  {
    pn_string_t *s = pn_string ( NULL );
    pn_inspect ( pn_message_body(msg), s );
    const char * message_content = pn_string_get(s);
    len = strlen(message_content);
    const char * src = message_content;
    context->bytes_received += strlen(message_content);
    // Uncomment these lines to see the actual messages.
    //fprintf ( stdout, "received %d bytes: %d\n", len );
    //fprintf ( stdout, "received message: |%s|\n", message_content );
    pn_free ( s );
  }
  return len;
}



bool 
process_event ( context_p context, pn_event_t * event ) 
{
  pn_session_t   * event_session;
  pn_transport_t * event_transport;
  pn_link_t      * event_link;
  pn_delivery_t  * event_delivery;
  char link_name [ 1000 ];


  switch ( pn_event_type( event ) ) 
  {
    case PN_LISTENER_ACCEPT:
      context->connection = pn_connection ( );
      pn_listener_accept ( pn_event_listener ( event ), context->connection );
    break;

    case PN_CONNECTION_INIT:
      snprintf ( context->id, MAX_NAME, "%d", int(getpid()));
      pn_connection_set_container ( pn_event_connection( event ), context->id );
      event_session = pn_session ( pn_event_connection( event ) );
      pn_session_open ( event_session );

      sprintf ( link_name, "%d_recv", getpid() );
      context->link = pn_receiver( event_session, link_name );
      pn_terminus_set_address ( pn_link_source(context->link), context->path );
      pn_link_open ( context->link );
    break;

    case PN_CONNECTION_BOUND: 
      event_transport = pn_event_transport ( event );
      pn_transport_require_auth ( event_transport, false );
      pn_sasl_allowed_mechs ( pn_sasl(event_transport), "ANONYMOUS" );
    break;

    case PN_CONNECTION_REMOTE_OPEN : 
      pn_connection_open ( pn_event_connection( event ) ); 
    break;

    case PN_SESSION_REMOTE_OPEN:
      pn_session_open ( pn_event_session( event ) );
    break;

    case PN_LINK_REMOTE_OPEN: 
      event_link = pn_event_link( event );
      pn_link_open ( event_link );
      if ( pn_link_is_receiver ( event_link ) )
      {
        pn_link_flow ( event_link, context->credit_window );
      }
    break;

    case PN_DELIVERY: 
      if ( context->received == 0 ) 
      {
        context->start_time = get_timestamp();
      }
      event_delivery = pn_event_delivery( event );
      event_link = pn_delivery_link ( event_delivery );

      if ( pn_link_is_receiver ( event_link ) )
      {
        if ( ! pn_delivery_readable  ( event_delivery ) )
          break;

        if ( pn_delivery_partial ( event_delivery ) ) 
          break;

        int len = decode_message ( context, event_delivery );
        pn_delivery_update ( event_delivery, PN_ACCEPTED );
        pn_delivery_settle ( event_delivery );

        context->received ++;

        if ( context->received >= context->expected_messages) 
        {
          context->stop_time = get_timestamp();
          double duration = context->stop_time - context->start_time;
          double bytes_per_second = (double)context->bytes_received / duration;
          fprintf ( stdout, 
                    "%d messages %d bytes received in %.3lf seconds.\n", 
                    duration,
                    context->received,
                    context->bytes_received
                  );
          fprintf ( stdout, "%.0lf bytes per second\n", bytes_per_second );
          halt ( context );
          break;
        }
        pn_link_flow ( event_link, context->credit_window - pn_link_credit(event_link) );
      }
      else
      {
        fprintf ( stderr, 
                  "A delivery came to a link that is not a receiver.\n" 
                );
        exit ( 1 );
      }
    break;

    case PN_CONNECTION_REMOTE_CLOSE :
      pn_connection_close ( pn_event_connection( event ) );
    break;

    case PN_SESSION_REMOTE_CLOSE :
      pn_session_close ( pn_event_session( event ) );
    break;

    case PN_LINK_REMOTE_CLOSE :
      pn_link_close ( pn_event_link( event ) );
    break;

    case PN_PROACTOR_INACTIVE:
      return false;

    default:
      break;
  }

  return true;
}





void
init_context ( context_p context, int argc, char ** argv )
{
  #define NEXT_ARG      argv[i+1]

  strcpy ( context->name, "default_name" );
  strcpy ( context->host, "0.0.0.0" );
  context->listener                = 0;
  context->connection              = 0;
  context->proactor                = 0;
  context->received                = 0;
  context->message                 = 0;
  context->message_length          = 100;     
  context->expected_messages       = 0;
  context->credit_window           = 1000;

  for ( int i = 1; i < argc; ++ i )
  {
    // address ----------------------------------------------
    if ( ! strcmp ( "--address", argv[i] ) )
    {
      context->path = strdup ( NEXT_ARG );
      context->link = 0;
      i ++;
    }
    // name ----------------------------------------------
    else
    if ( ! strcmp ( "--name", argv[i] ) )
    {
      if ( ! strcmp ( NEXT_ARG, "PID" ) )
      {
        sprintf ( context->name, "client_%d", getpid() );
      }
      else
      {
        memset  ( context->name, 0, MAX_NAME );
        strncpy ( context->name, NEXT_ARG, MAX_NAME );
      }

      i ++;
    }
    // message_length ----------------------------------------------
    else
    if ( ! strcmp ( "--message_length", argv[i] ) )
    {
      context->message_length = atoi ( NEXT_ARG );
      i ++;
    }
    // port ----------------------------------------------
    else
    if ( ! strcmp ( "--port", argv[i] ) )
    {
      context->port = strdup ( NEXT_ARG );
      i ++;
    }
    // host ----------------------------------------------
    else
    if ( ! strcmp ( "--host", argv[i] ) )
    {
      sprintf ( context->host, "%s", NEXT_ARG );
      i ++;
    }
    // messages ----------------------------------------------
    else
    if ( ! strcmp ( "--messages", argv[i] ) )
    {
      context->expected_messages = atoi ( NEXT_ARG );
      i ++;
    }
    // unknown ----------------------------------------------
    else
    {
      fprintf ( stderr, "Unknown option: |%s|\n", argv[i] );
      exit ( 1 );
    }
  }
}



int 
main ( int argc, char ** argv ) 
{
  srand ( getpid() );
  context_t context;
  context_g = & context;
  init_context ( & context, argc, argv );

  context.message = pn_message();

  char addr[PN_MAX_ADDR];
  pn_proactor_addr ( addr, sizeof(addr), context.host, context.port );
  context.proactor   = pn_proactor();
  context.connection = pn_connection();
  pn_proactor_connect ( context.proactor, context.connection, addr );

  int batch_done = 0;
  while ( ! batch_done ) 
  {
    pn_event_batch_t *events = pn_proactor_wait ( context.proactor );
    pn_event_t * event;
    for ( event = pn_event_batch_next(events); event; event = pn_event_batch_next(events)) 
    {
      if (! process_event( & context, event ))
      {
        batch_done = 1;
        break;
       }
    }
    pn_proactor_done ( context.proactor, events );
  }
  return 0;
}



