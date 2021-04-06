package M3rcury

import ( 
         "fmt"
         "os"
         "strings"
         "time"
       )

var fp    = fmt.Fprintf
var glyph = '\u263F'




type Message struct {
  Type string
  Data map [ string ] interface{}
}


type Message_Channel chan Message



// Mercury's view of a Box.
type m3_box struct {
  name string
  input, output Message_Channel
}



type m3rcury struct {
  input      Message_Channel
  output     Message_Channel
  log_dir    string
  log_file   string
  start_time float64
  boxes      map [ string ] m3_box 
  local_box  string
}





func Start_M3rcury ( local_box string, log_dir string ) ( in, out Message_Channel ) {
  in  = make ( Message_Channel, 5 )
  out = make ( Message_Channel, 5 )
  m3 := & m3rcury { output     : out,
                    input      : in,
                    log_dir    : log_dir,
                    local_box  : local_box,
                  }
  m3.log_file = m3.log_dir + "/m3rcury"
  now     := time.Now()
  seconds := float64 ( now.UnixNano() ) / 1000000000
  m3.start_time = seconds
  m3.make_log_dirs ( )

  go m3.listen (  )

  return in, out
}





func ( m3 * m3rcury ) listen ( ) {
  for { 
    msg := <- m3.input
    fp ( os.Stdout, "☿: received message: |%v|\n", msg )
    m3.output <- Message { Type: "ping" }
    switch ( msg.Type ) {

      case "pong" :
        time.Sleep ( 2 * time.Second )
        m3.output <- Message { Type: "ping" }
      
       case "box" :
         name := msg.Data["name"]
         new_box ( name.(string), m3.log_dir + "/boxes", m3.start_time ) 

       default:
         fp ( os.Stdout, "%c : unknown command: |%s|\n", glyph, msg.Type )
    }
  }
}





func ( m3 * m3rcury ) make_log_dirs ( ) {

  // My log dir ----------------------------------
  err := find_or_make_dir ( m3.log_dir )
  if err != nil {
    // Already existing is not an error.
    if ! strings.Contains ( err.Error(), "exists" ) {
      m3.output <- Message { Type: "error",
                             Data: map[string]interface{} { "err" : err.Error() } }
    }
  }
  m3.log ( "start on %s", m3.local_box )

  // the Boxes ----------------------------------
  err = find_or_make_dir ( m3.log_dir + "/boxes" )
  if err != nil {
    m3.output <- Message { Type: "error",
                           Data: map[string]interface{} { "err" : err.Error() } }
  }
}





func find_or_make_dir ( dir string ) ( err error ) {
  err = os.Mkdir ( dir, 0744 )
  if err != nil {
    // Already existing is not an error.
    if ! strings.Contains ( err.Error(), "exists" ) {
      return err
    }
  }
  return nil
}





func timestamp ( ) ( float64 ) {
  now     := time.Now()
  return float64 ( now.UnixNano() ) / 1000000000
}





func ( m3 * m3rcury ) timestamp ( ) ( float64 ) {
  return timestamp() - m3.start_time
}





func ( m3 * m3rcury ) log ( format string, args ...interface{}) {
  var file * os.File
  new_format := fmt.Sprintf ( "%c %.6f : %s\n", glyph, m3.timestamp(), format )

  file, err := os.Open ( m3.log_file )
  if err != nil {
    // If it doesn't exist yet, create it.
    if strings.Contains ( err.Error(), "no such file or directory" ) {
      file, err = os.Create ( m3.log_file )
      if err != nil {
        fp ( os.Stdout, "%c.log error making |%s| : |%s|\n", glyph, m3.log_file, err.Error() )
        os.Exit ( 1 )
      }
    }
  }
  defer file.Close()
  fp ( file, new_format, args ... )
}





