package main

import (
         "fmt"
         "os"
         "time"

         m3 "m3rcury"
       )


var fp = fmt.Fprintf



func listen_to_mercury ( m3rcury_output m3.Message_Channel ) {
  for {
    msg := <- m3rcury_output
    fp ( os.Stdout, "Main received msg: |%v|\n", msg )
  }
}



func main ( ) {

  m3rcury_input, m3rcury_output := m3.M3rcury ( "Brontonomicon", "./log" )
  go listen_to_mercury ( m3rcury_output )

  m3rcury_input <- m3.Message { Type: "start" }

  count := 0
  for { 
    time.Sleep ( 10 * time.Millisecond )
    m3rcury_input <- m3.Message { Type: "ping" }
    count ++
    fp ( os.Stdout, "count %d\n", count )
  }
}



