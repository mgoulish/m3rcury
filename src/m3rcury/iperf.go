package M3rcury

import (
         "fmt"
         "os"
         "strings"
       )



type iperf struct {
  mode       string
  input      Message_Channel
  output     Message_Channel

  log_dir    string
  log_file   string

  start_time float64

}





func new_iperf ( mode string, 
                 root_log_dir string, 
                 start_time float64 ) ( in, out Message_Channel ) {
  in  = make ( Message_Channel, 5 )
  out = make ( Message_Channel, 5 )

  r := & iperf { mode       : mode,
                 output     : out,
                 input      : in,
                 log_dir    : root_log_dir + "/" + mode,
                 start_time : start_time,
               }
  r.log_file = r.log_dir + "/log"

  r.make_log_dirs ( )
  go r.listen ( )

  return in, out
}




func ( r * iperf ) listen ( ) {
  for {
    msg := <- r.input
    fp ( os.Stdout, "MDEBUG Router |%s| got msg |%v|\n", r.mode, msg )
  }
}





func ( r * iperf ) timestamp ( ) ( float64 ) {
  return timestamp() - r.start_time
}





func ( r * iperf ) make_log_dirs ( ) {

  err := find_or_make_dir ( r.log_dir )
  if err != nil {
    r.output <- Message { Type: "error",
                          Data: map[string]interface{} { "err" : err.Error() } }
  }

  r.log ( "start" )
}




func ( r * iperf ) log ( format string, args ...interface{}) {
  var file * os.File
  new_format := fmt.Sprintf ( "iperf %s %.6f : %s\n", r.mode, r.timestamp(), format )

  // Open the log file, if it already exists.
  file, err := os.Open ( r.log_file )
  if err != nil {
    // If it doesn't exist yet, create it.
    if strings.Contains ( err.Error(), "no such file or directory" ) {
      file, err = os.Create ( r.log_file )
      if err != nil {
        fp ( os.Stdout, "%c.%s.log error making |%s| : |%s|\n", glyph, r.mode, r.log_file, err.Error() )
        os.Exit ( 1 )
      }
    }
  }
  defer file.Close()
  fp ( file, new_format, args ... )
}

