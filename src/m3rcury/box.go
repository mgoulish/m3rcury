package M3rcury

import (
         "fmt"
         "os"
         "strings"
       )



type box struct {
  name       string
  input      Message_Channel
  output     Message_Channel

  log_dir    string
  log_file   string

  start_time float64
}





func new_box ( name, root_log_dir string, start_time float64 ) ( in, out Message_Channel ) {
  in  = make ( Message_Channel, 5 )
  out = make ( Message_Channel, 5 )

  b := & box { output     : out,
               input      : in,
               name       : name,
               log_dir    : root_log_dir + "/" + name,
               start_time : start_time,
             }

  b.log_file = b.log_dir + "/log"

  b.make_log_dirs ( )
  go b.listen (  )

  return in, out
}





func ( b * box ) listen ( ) {
  for {
    msg := <- b.input
    fp ( os.Stdout, "MDEBUG Box |%s| got msg |%v|\n", b.name, msg )
  }
}





func ( b * box ) make_log_dirs ( ) {

  err := find_or_make_dir ( b.log_dir )
  if err != nil {
    b.output <- Message { Type: "error",
                          Data: map[string]interface{} { "err" : err.Error() } }
  }

  b.log ( "start" )
}





func ( b * box ) timestamp ( ) ( float64 ) {
  return timestamp() - b.start_time
}





func ( b * box ) log ( format string, args ...interface{}) {
  var file * os.File
  new_format := fmt.Sprintf ( "box %s %.6f : %s\n", b.name, b.timestamp(), format )

  // Open the log file, if it already exists.
  file, err := os.Open ( b.log_file )
  if err != nil {
    // If it doesn't exist yet, create it.
    if strings.Contains ( err.Error(), "no such file or directory" ) {
      file, err = os.Create ( b.log_file )
      if err != nil {
        fp ( os.Stdout, "%c.%s.log error making |%s| : |%s|\n", glyph, b.name, b.log_file, err.Error() )
        os.Exit ( 1 )
      }
    }
  }
  defer file.Close()
  fp ( file, new_format, args ... )
}


