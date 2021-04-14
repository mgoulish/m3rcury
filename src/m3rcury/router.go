package M3rcury

import (
         "fmt"
         "os"
         "strings"
       )



type router struct {
  ID    string

  input      Message_Channel
  output     Message_Channel

  log_dir    string
  log_file   string

  start_time float64

}





func new_router ( root_log_dir string, msg Message, start_time float64 ) ( in, out Message_Channel ) {
  in  = make ( Message_Channel, 5 )
  out = make ( Message_Channel, 5 )

  ID := msg.Data["ID"].(string)

  r := & router { output     : out,
                  input      : in,
                  ID         : ID,
                  log_dir    : root_log_dir + "/" + ID,
                  start_time : start_time,
                }
  r.log_file = r.log_dir + "/log"

  r.make_log_dirs ( )
  go r.listen ( )

  return in, out
}




func ( r * router ) listen ( ) {
  for {
    msg := <- r.input
    fp ( os.Stdout, "MDEBUG Router |%s| got msg |%v|\n", r.ID, msg )
  }
}





func ( r * router ) timestamp ( ) ( float64 ) {
  return timestamp() - r.start_time
}





func ( r * router ) make_log_dirs ( ) {

  err := find_or_make_dir ( r.log_dir )
  if err != nil {
    r.output <- Message { Type: "error",
                          Data: map[string]interface{} { "err" : err.Error() } }
  }

  r.log ( "start" )
}




func ( r * router ) log ( format string, args ...interface{}) {
  var file * os.File
  new_format := fmt.Sprintf ( "router %s %.6f : %s\n", r.ID, r.timestamp(), format )

  // Open the log file, if it already exists.
  file, err := os.Open ( r.log_file )
  if err != nil {
    // If it doesn't exist yet, create it.
    if strings.Contains ( err.Error(), "no such file or directory" ) {
      file, err = os.Create ( r.log_file )
      if err != nil {
        fp ( os.Stdout, "%c.%s.log error making |%s| : |%s|\n", glyph, r.ID, r.log_file, err.Error() )
        os.Exit ( 1 )
      }
    }
  }
  defer file.Close()
  fp ( file, new_format, args ... )
}

