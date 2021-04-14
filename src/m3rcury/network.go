package M3rcury

import (
         "fmt"
         "os"
         "strings"
       )



type network struct {
  name       string
  boxes      map [ string ] string   // [ box ] address
  input      Message_Channel
  output     Message_Channel
  log_dir    string
  log_file   string
  start_time float64    
}





func new_network ( root_log_dir string, msg Message, start_time float64 ) (in, out Message_Channel) {

  in  = make ( Message_Channel, 5 )
  out = make ( Message_Channel, 5 )

  my_name := msg.Data["name"].(string)

  n := & network { name       : my_name,
                   boxes      : make ( map [ string ] string ),
                   input      : in,
                   output     : out,
                   log_dir    : root_log_dir,
                   log_file   : root_log_dir + "/" + my_name,
                   start_time : start_time,
                 }

  n.make_log_dirs ( )
  go n.listen ( )

  return in, out
}





func ( n * network ) listen ( ) {
  for {
    msg := <- n.input
    fp ( os.Stdout, "MDEBUG Network |%s| got msg |%v|\n", n.name, msg )
  }
}






func ( n * network ) timestamp ( ) ( float64 ) {
  return timestamp() - n.start_time
}





func ( n * network ) make_log_dirs ( ) {

  err := find_or_make_dir ( n.log_dir )
  if err != nil {
    n.output <- Message { Type: "error",
                          Data: map[string]interface{} { "err" : err.Error() } }
  }

  n.log ( "start" )
}




func ( n * network ) log ( format string, args ...interface{}) {
  var file * os.File
  new_format := fmt.Sprintf ( "network %s %.6f : %s\n", n.name, n.timestamp(), format )

  // Open the log file, if it already exists.
  file, err := os.Open ( n.log_file )
  if err != nil {
    // If it doesn't exist yet, create it.
    if strings.Contains ( err.Error(), "no such file or directory" ) {
      file, err = os.Create ( n.log_file )
      if err != nil {
        fp ( os.Stdout, "%c.%s.log error making |%s| : |%s|\n", glyph, n.name, n.log_file, err.Error() )
        os.Exit ( 1 )
      }
    }
  }
  defer file.Close()
  fp ( file, new_format, args ... )
}





