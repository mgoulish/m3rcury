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
  size_t            message_length;
  char            * outgoing_buffer;
  char              incoming_message [ MAX_MESSAGE ];   
  char            * port;
  int               sent,             
                    received,         
                    accepted;
  pn_message_t    * message;
  int               expected_messages;
  size_t            credit_window;
  pn_proactor_t   * proactor;
  pn_connection_t * connection;
  bool              start_signal_received;
}
context_t,
* context_p;

context_p context_g = 0;



void
sig_handler ( int signum )
{
  if ( context_g ) 
    context_g->start_signal_received = true;
}



void 
halt ( context_p context )
{
  if ( context->connection )
    pn_connection_close(context->connection);
}



size_t 
encode_outgoing_message ( context_p context ) 
{
  int err = 0;
  size_t size = context->message_length * 2;

  if ( 0 == (err = pn_message_encode ( context->message, context->outgoing_buffer, & size) ) )
    return size;

  if ( err == PN_OVERFLOW ) 
  {
    fprintf ( stderr, 
              "error : overflowed outgoing_buffer_size == %d\n", 
              context->message_length 
            );
    exit ( 1 );
  } 
  else
  if ( err != 0 ) 
  {
    fprintf ( stderr, 
              "error : encoding message: %s |%s|\n", 
              pn_code ( err ), 
              pn_error_text(pn_message_error ( context->message ) ) 
            );
    exit ( 1 );
  }
  return 0; // unreachable   // I think.
}



void 
send_message ( context_p context ) 
{
  if ( ! context->start_signal_received ) 
  {
    usleep ( 10000 );
    return;
  }

  // Set messages ID from sent count.
  pn_atom_t id_atom;
  char id_string [ 20 ];
  sprintf ( id_string, "%d", context->sent );
  id_atom.type = PN_STRING;
  id_atom.u.as_bytes = pn_bytes ( strlen(id_string), id_string );
  pn_message_set_id ( context->message, id_atom );

  pn_data_t * body = pn_message_body ( context->message );
  pn_data_clear ( body );
  pn_data_enter ( body );

  memset ( context->outgoing_buffer, 'x', context->message_length );
  sprintf ( context->outgoing_buffer, "Message %d", context->sent );
  pn_bytes_t bytes = { context->message_length, context->outgoing_buffer };

  pn_data_put_string ( body, bytes );
  pn_data_exit ( body );
  size_t outgoing_size = encode_outgoing_message ( context );
  pn_delivery ( context->link, 
                pn_dtag ( (const char *) & context->sent, sizeof(context->sent) ) 
              );
  pn_link_send ( context->link, 
                 context->outgoing_buffer, 
                 outgoing_size 
               );
  context->sent ++;
  pn_link_advance ( context->link );
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
    case PN_CONNECTION_INIT:
      snprintf ( context->id, MAX_NAME, "%d", int(getpid()) );
      pn_connection_set_container ( pn_event_connection( event ), context->id );
      event_session = pn_session ( pn_event_connection( event ) );
      pn_session_open ( event_session );

      sprintf ( link_name, "%d_send", getpid());
      context->link = pn_sender (  event_session, link_name );
      pn_terminus_set_address ( pn_link_target(context->link), context->path );
      pn_link_set_snd_settle_mode ( context->link, PN_SND_UNSETTLED );
      pn_link_set_rcv_settle_mode ( context->link, PN_RCV_FIRST );

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

    case PN_LINK_FLOW : 
    {
      event_link = pn_event_link ( event );

      // Send messages as fast as we are allowed to 
      // by the amount of credit available.
      if ( pn_link_is_sender(event_link) )
      {
        while ( pn_link_credit ( event_link ) > 0 && context->sent < context->expected_messages )
          send_message ( context );
      }
    }
    break;

    case PN_DELIVERY: 
    {
      event_delivery = pn_event_delivery( event );
      event_link = pn_delivery_link ( event_delivery );
      if ( ! pn_link_is_sender ( event_link ) ) 
      {
        fprintf ( stderr, "Got non-sender link.\n");
        exit ( 1 );
      }

      int state = pn_delivery_remote_state(event_delivery);
      pn_delivery_settle ( event_delivery );

      switch ( state ) 
      {
        case PN_ACCEPTED:
          // Don't shut down when they're all sent, or our connection dies
          // too fast. SHut down after the receiver has accepted them all.
          context->accepted ++;
          if ( context->accepted >= context->expected_messages )
            halt ( context );
        break;

        case PN_REJECTED:
        case PN_RELEASED:
        case PN_MODIFIED:
          fprintf ( stderr, "error: message bad disposition.\n" );
          exit ( 1 );
        break;

        default:
          fprintf ( stderr, "error : unknown remote state! %d\n", state );
        break;
      }
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

  context->connection              = 0;
  context->proactor                = 0;
  context->sent                    = 0;
  context->received                = 0;
  context->accepted                = 0;
  context->message                 = 0;
  context->message_length          = 100;
  context->expected_messages       = 0;
  context->credit_window           = 1000;
  context->start_signal_received   = false;

  for ( int i = 1; i < argc; ++ i )
  {

    // address ----------------------------------------------
    if ( ! strcmp ( "--address", argv[i] ) )
    {
      context->path     = strdup ( NEXT_ARG );
      context->link     = 0;
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
  signal ( SIGUSR1, sig_handler );

  context_t context;
  context_g = & context;
  init_context ( & context, argc, argv );
  context.outgoing_buffer = (char *) malloc ( context.message_length * 2 );
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



