package m3rcury

import ( 
         "fmt"
         "os"
         "time"
       )

var fp = fmt.Fprintf
var mercury = '\u263F'




type Message struct {
  Type string
  Data map [ string ] interface{}
}


type Message_Channel chan Message



type M3rcury struct {
  input      Message_Channel
  output     Message_Channel
  log_dir    string
  log_file   string
  start_time float64
}



func Start_M3rcury ( ) ( in, out Message_Channel ) {
  in  = make ( Message_Channel, 5 )
  out = make ( Message_Channel, 5 )
  m3 := & M3rcury { output     : out,
                    input      : in }
  go m3.listen (  )

  now     := time.Now()
  seconds := float64 ( now.UnixNano() ) / 1000000000
  m3.start_time = seconds

  return in, out
}





func ( m3 * M3rcury ) listen ( ) {
  for { 
    msg := <- m3.input
    fp ( os.Stdout, "☿: received message: |%v|\n", msg )
    m3.output <- Message { Type: "ping" }
    switch ( msg.Type ) {

      case "pong" :
        time.Sleep ( 2 * time.Second )
        m3.output <- Message { Type: "ping" }
      
      case "log" :
        dir := msg.Data["dir"]
        m3.log_dir, _ = dir.(string)
        m3.log_file = m3.log_dir + "/m3rcury" 
        m3.make_log_dir ( )
    }
  }
}





func ( m3 * M3rcury ) make_log_dir ( ) {
  err := os.Mkdir ( m3.log_dir, 0744 )
  if err != nil {
    m3.output <- Message { Type: "error",
                           Data: map[string]interface{} { "err" : err.Error() } }
  }
}





func ( m3 * M3rcury ) timestamp ( ) ( float64 ) {
  now     := time.Now()
  seconds := float64 ( now.UnixNano() ) / 1000000000

  return seconds - m3.start_time
}





func ( m3 * M3rcury ) log ( format string, args ...interface{}) {
  new_format := fmt.Sprintf ( "%c %.6f : %s\n", mercury, m3.timestamp(), format )

  // Open the log file, write this, then close it.
  file, err := os.Open ( m3.log_file )
  if err != nil {
    fp ( os.Stdout, "MDEBUG log error:  |%s|\n", err.Error() )
    os.Exit ( 1 )
  }
  defer file.Close()
  fp ( file, new_format, args ... )
}





